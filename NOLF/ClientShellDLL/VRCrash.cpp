// VRCrash.cpp: see VRCrash.h.

#include "stdafx.h"
#include "VRCrash.h"
#include "VRLog.h"

#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <string.h>

namespace VRCrash
{
namespace
{
	char  s_szReport[MAX_PATH] = { 0 };
	LONG  s_nFirstChance = 0;
	LONG  s_bWriting = 0;

	// "lithtech.exe+0x46E93", or the bare address when no module holds it.
	void Where(DWORD_PTR addr, char* pOut, size_t n)
	{
		HMODULE hMod = NULL;
		char szPath[MAX_PATH] = { 0 };
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
							   (LPCSTR)addr, &hMod) && hMod
			&& GetModuleFileNameA(hMod, szPath, MAX_PATH))
		{
			const char* pName = strrchr(szPath, '\\');
			_snprintf(pOut, n, "%s+0x%X", pName ? pName + 1 : szPath, (unsigned)(addr - (DWORD_PTR)hMod));
		}
		else
			_snprintf(pOut, n, "0x%08X", (unsigned)addr);
		pOut[n - 1] = 0;
	}

	const char* CodeName(DWORD c)
	{
		switch (c)
		{
		case EXCEPTION_ACCESS_VIOLATION:      return "ACCESS_VIOLATION";
		case EXCEPTION_ILLEGAL_INSTRUCTION:   return "ILLEGAL_INSTRUCTION";
		case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "INT_DIVIDE_BY_ZERO";
		case EXCEPTION_PRIV_INSTRUCTION:      return "PRIV_INSTRUCTION";
		case EXCEPTION_STACK_OVERFLOW:        return "STACK_OVERFLOW";
		case EXCEPTION_IN_PAGE_ERROR:         return "IN_PAGE_ERROR";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
		case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
		}
		return "exception";
	}

	bool Fatal(DWORD c)
	{
		return c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION
			|| c == EXCEPTION_INT_DIVIDE_BY_ZERO || c == EXCEPTION_PRIV_INSTRUCTION
			|| c == EXCEPTION_STACK_OVERFLOW || c == EXCEPTION_IN_PAGE_ERROR;
	}

	// FIRST CHANCE: say where, once per fault, for the first five of a run. Some
	// of these are caught on purpose (the renderer's calibration probes), and
	// the line says only what happened, not that the game died.
	LONG CALLBACK FirstChance(PEXCEPTION_POINTERS p)
	{
		if (!p || !p->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;
		const DWORD c = p->ExceptionRecord->ExceptionCode;
		if (!Fatal(c)) return EXCEPTION_CONTINUE_SEARCH;
		if (InterlockedIncrement(&s_nFirstChance) > 5) return EXCEPTION_CONTINUE_SEARCH;
		char szWhere[160];
		Where((DWORD_PTR)p->ExceptionRecord->ExceptionAddress, szWhere, sizeof(szWhere));
		VRLog::Msg("FAULT (first chance, may be caught): %s 0x%08X at %s",
			CodeName(c), (unsigned)c, szWhere);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	typedef BOOL (WINAPI *PFN_StackWalk64)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID,
		PREAD_PROCESS_MEMORY_ROUTINE64, PFUNCTION_TABLE_ACCESS_ROUTINE64,
		PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
	typedef BOOL (WINAPI *PFN_SymInitialize)(HANDLE, PCSTR, BOOL);

	LONG WINAPI Unhandled(PEXCEPTION_POINTERS p)
	{
		if (InterlockedExchange(&s_bWriting, 1)) return EXCEPTION_CONTINUE_SEARCH;
		FILE* f = s_szReport[0] ? fopen(s_szReport, "w") : NULL;
		if (!f || !p || !p->ExceptionRecord || !p->ContextRecord)
		{
			if (f) fclose(f);
			return EXCEPTION_CONTINUE_SEARCH;
		}
		const EXCEPTION_RECORD& r = *p->ExceptionRecord;
		const CONTEXT& c = *p->ContextRecord;
		char szWhere[160];
		Where((DWORD_PTR)r.ExceptionAddress, szWhere, sizeof(szWhere));
		SYSTEMTIME st; GetLocalTime(&st);
		fprintf(f, "No One Lives Forever VR - crash report\n");
		fprintf(f, "time     : %04d-%02d-%02d %02d:%02d:%02d\n", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		fprintf(f, "fault    : %s 0x%08X at %s\n", CodeName(r.ExceptionCode), (unsigned)r.ExceptionCode, szWhere);
		if (r.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r.NumberParameters >= 2)
			fprintf(f, "access   : %s 0x%08X\n", r.ExceptionInformation[0] == 0 ? "reading" : r.ExceptionInformation[0] == 1 ? "writing" : "executing", (unsigned)r.ExceptionInformation[1]);
#ifndef _WIN64
		fprintf(f, "registers: eax %08X ebx %08X ecx %08X edx %08X esi %08X edi %08X ebp %08X esp %08X eip %08X\n",
			c.Eax, c.Ebx, c.Ecx, c.Edx, c.Esi, c.Edi, c.Ebp, c.Esp, c.Eip);
#endif
		fprintf(f, "\nstack (module+offset, newest first):\n");
		HMODULE hDbg = LoadLibraryA("dbghelp.dll");
		PFN_StackWalk64 pWalk = hDbg ? (PFN_StackWalk64)GetProcAddress(hDbg, "StackWalk64") : NULL;
		PFN_SymInitialize pInit = hDbg ? (PFN_SymInitialize)GetProcAddress(hDbg, "SymInitialize") : NULL;
		PFUNCTION_TABLE_ACCESS_ROUTINE64 pFta = hDbg ? (PFUNCTION_TABLE_ACCESS_ROUTINE64)GetProcAddress(hDbg, "SymFunctionTableAccess64") : NULL;
		PGET_MODULE_BASE_ROUTINE64 pBase = hDbg ? (PGET_MODULE_BASE_ROUTINE64)GetProcAddress(hDbg, "SymGetModuleBase64") : NULL;
		int nFrames = 0;
#ifndef _WIN64
		if (pWalk && pInit && pFta && pBase)
		{
			pInit(GetCurrentProcess(), NULL, TRUE);
			CONTEXT ctx = c;
			STACKFRAME64 sf = {};
			sf.AddrPC.Offset = ctx.Eip;    sf.AddrPC.Mode = AddrModeFlat;
			sf.AddrFrame.Offset = ctx.Ebp; sf.AddrFrame.Mode = AddrModeFlat;
			sf.AddrStack.Offset = ctx.Esp; sf.AddrStack.Mode = AddrModeFlat;
			while (nFrames < 48 && pWalk(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(), GetCurrentThread(),
										 &sf, &ctx, NULL, pFta, pBase, NULL) && sf.AddrPC.Offset)
			{
				Where((DWORD_PTR)sf.AddrPC.Offset, szWhere, sizeof(szWhere));
				fprintf(f, "  %2d  %s\n", nFrames, szWhere);
				++nFrames;
			}
		}
#endif
		if (!nFrames) fprintf(f, "  (no stack walk: dbghelp.dll unavailable)\n");
		fprintf(f, "\nmodules:\n");
		HMODULE aMods[] = { GetModuleHandleA(NULL), GetModuleHandleA("d3dstub.ren"), GetModuleHandleA("SDL2.dll"),
							GetModuleHandleA("DINPUT.dll") };
		for (HMODULE h : aMods)
		{
			char szPath[MAX_PATH] = { 0 };
			if (h && GetModuleFileNameA(h, szPath, MAX_PATH)) fprintf(f, "  %08X  %s\n", (unsigned)(DWORD_PTR)h, szPath);
		}
		HMODULE hSelf = NULL;
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
							   (LPCSTR)&Unhandled, &hSelf) && hSelf)
		{
			char szPath[MAX_PATH] = { 0 };
			if (GetModuleFileNameA(hSelf, szPath, MAX_PATH)) fprintf(f, "  %08X  %s (the client)\n", (unsigned)(DWORD_PTR)hSelf, szPath);
		}
		fclose(f);
		VRLog::Msg("CRASH: %s - the report is %s", CodeName(r.ExceptionCode), s_szReport);
		return EXCEPTION_CONTINUE_SEARCH;	// Windows' own handling (and its event log record) still runs
	}
}

void Install(const char* pszRunDir)
{
	static bool s_bDone = false;
	if (s_bDone) return;
	s_bDone = true;
	if (pszRunDir && *pszRunDir)
	{
		_snprintf(s_szReport, sizeof(s_szReport), "%s\\crash.txt", pszRunDir);
		s_szReport[sizeof(s_szReport) - 1] = 0;
	}
	AddVectoredExceptionHandler(0, FirstChance);
	SetUnhandledExceptionFilter(Unhandled);
	VRLog::Msg("crash report: armed (%s)", s_szReport[0] ? s_szReport : "no run folder");
}

}	// namespace VRCrash

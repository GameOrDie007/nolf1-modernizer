// ----------------------------------------------------------------------- //
//
// MODULE  : VRInputGuard.cpp
//
// PURPOSE : The DirectInput start-up fix, from inside the client
//
// Windows' dinput.dll corrupts its own heap when the engine asks it for every
// device class at once (EnumDevices with type 0) on some machines - a Logitech
// receiver did it on one, and the engine then dies before the renderer loads,
// with nothing in any log. The fix has always been the DINPUT.dll proxy that
// ships beside lithtech.exe (host\dinputproxy\dinput_proxy.cpp): it limits that
// sweep to keyboard and mouse, and keeps the keyboard working with the game's
// window behind the headset view.
//
// 27 October: on a player's PC the proxy was sitting beside lithtech.exe, byte
// for byte the shipped file, and Windows loaded its own dinput.dll anyway (the
// crash dump's module list). The game died at a wild address made of two
// UTF-16 '?' characters, the shape of a device name run off the end of a
// buffer. So the fix must not depend on which dinput.dll Windows picks.
//
// lithtech.exe imports exactly one function from it, DirectInputCreateA, and
// calls it after this client shell is constructed. So the constructor patches
// that one import: the engine's call comes here, goes on to whichever
// dinput.dll is loaded, and the interface it gets back is patched the same way
// the proxy patches it. When the proxy IS the loaded dinput.dll, nothing is
// touched - it already does all of this.
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "VRInputGuard.h"
#include "VRLog.h"
#include <windows.h>

// DirectInput 7 device types, as passed to EnumDevices.
#define G_DEVTYPE_MOUSE		0x00000002
#define G_DEVTYPE_KEYBOARD	0x00000003
#define G_DEVTYPE_JOYSTICK	0x00000004

#define G_DISCL_EXCLUSIVE		0x00000001
#define G_DISCL_NONEXCLUSIVE	0x00000002
#define G_DISCL_FOREGROUND		0x00000004
#define G_DISCL_BACKGROUND		0x00000008
#define G_DISCL_NOWINKEY		0x00000010

// {6F1D2B61-D5A0-11CF-BFC7-444553540000}
static const GUID G_GUID_SysKeyboard =
	{ 0x6F1D2B61, 0xD5A0, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };

typedef HRESULT (WINAPI *PFN_CreateA)(HINSTANCE, DWORD, void**, void*);
typedef HRESULT (STDMETHODCALLTYPE *PFN_EnumDevices)(void*, DWORD, void*, void*, DWORD);
typedef HRESULT (STDMETHODCALLTYPE *PFN_CreateDevice)(void*, const GUID*, void**, void*);
typedef HRESULT (STDMETHODCALLTYPE *PFN_SetCoop)(void*, HWND, DWORD);

static PFN_CreateA		s_pRealCreateA	= NULL;
static PFN_EnumDevices	s_pRealEnum		= NULL;
static PFN_CreateDevice	s_pRealCreateDev = NULL;
static PFN_SetCoop		s_pRealSetCoop	= NULL;
static void*			s_apKeyboard[4]	= { 0 };
static int				s_nKeyboards	= 0;
static bool				s_bBackgroundKeyboard = true;

// Writes one pointer in memory that may be read-only (an import table, a vtable).
static bool PatchPointer(void** ppSlot, void* pNew)
{
	DWORD dwOld = 0;
	if (!VirtualProtect(ppSlot, sizeof(void*), PAGE_READWRITE, &dwOld)) return false;
	*ppSlot = pNew;
	DWORD dwDummy = 0;
	VirtualProtect(ppSlot, sizeof(void*), dwOld, &dwDummy);
	return true;
}

// Slot 4 of IDirectInputA (0-2 IUnknown, 3 CreateDevice, 4 EnumDevices).
static HRESULT STDMETHODCALLTYPE GuardEnumDevices(void* self, DWORD dwType, void* cb, void* ref, DWORD dwFlags)
{
	if (dwType == G_DEVTYPE_JOYSTICK)
	{
		VRLog::Msg("VRInputGuard: EnumDevices(JOYSTICK) skipped");
		return S_OK;
	}
	if (dwType == 0)
	{
		VRLog::Msg("VRInputGuard: EnumDevices(ALL) -> keyboard + mouse only");
		HRESULT hr = s_pRealEnum(self, G_DEVTYPE_KEYBOARD, cb, ref, dwFlags);
		HRESULT hr2 = s_pRealEnum(self, G_DEVTYPE_MOUSE, cb, ref, dwFlags);
		return SUCCEEDED(hr) ? hr2 : hr;
	}
	return s_pRealEnum(self, dwType, cb, ref, dwFlags);
}

// Slot 13 of IDirectInputDeviceA: the keyboard keeps working with the game's
// window behind (the headset view is in front by design). BACKGROUND needs
// NONEXCLUSIVE, and NOWINKEY needs FOREGROUND, so those go.
static HRESULT STDMETHODCALLTYPE GuardSetCoop(void* self, HWND hWnd, DWORD dwFlags)
{
	bool bKeyboard = false;
	for (int i = 0; i < s_nKeyboards; i++) if (s_apKeyboard[i] == self) bKeyboard = true;
	if (!bKeyboard || !s_bBackgroundKeyboard) return s_pRealSetCoop(self, hWnd, dwFlags);

	DWORD dwWant = (dwFlags & ~(G_DISCL_FOREGROUND | G_DISCL_EXCLUSIVE | G_DISCL_NOWINKEY)) | G_DISCL_BACKGROUND | G_DISCL_NONEXCLUSIVE;
	HRESULT hr = s_pRealSetCoop(self, hWnd, dwWant);
	VRLog::Msg("VRInputGuard: keyboard SetCooperativeLevel 0x%lx -> 0x%lx: %s", dwFlags, dwWant,
		SUCCEEDED(hr) ? "background" : "FAILED, using the game's own");
	if (FAILED(hr)) hr = s_pRealSetCoop(self, hWnd, dwFlags);
	return hr;
}

static HRESULT STDMETHODCALLTYPE GuardCreateDevice(void* self, const GUID* pGuid, void** ppOut, void* pUnk)
{
	HRESULT hr = s_pRealCreateDev(self, pGuid, ppOut, pUnk);
	if (SUCCEEDED(hr) && ppOut && *ppOut && pGuid && memcmp(pGuid, &G_GUID_SysKeyboard, sizeof(GUID)) == 0)
	{
		if (s_nKeyboards < 4) s_apKeyboard[s_nKeyboards++] = *ppOut;
		void** vt = *(void***)*ppOut;
		if (vt && vt[13] != (void*)GuardSetCoop)
		{
			s_pRealSetCoop = (PFN_SetCoop)vt[13];
			if (!PatchPointer(&vt[13], (void*)GuardSetCoop))
				VRLog::Msg("VRInputGuard: could not patch the keyboard's SetCooperativeLevel");
		}
	}
	return hr;
}

static HRESULT WINAPI GuardDirectInputCreateA(HINSTANCE hInst, DWORD dwVersion, void** ppOut, void* pUnk)
{
	HRESULT hr = s_pRealCreateA(hInst, dwVersion, ppOut, pUnk);
	VRLog::Msg("VRInputGuard: DirectInputCreateA(0x%lx) -> 0x%08lx", dwVersion, hr);
	if (FAILED(hr) || !ppOut || !*ppOut) return hr;

	void** vt = *(void***)*ppOut;
	if (!vt) return hr;
	if (vt[4] != (void*)GuardEnumDevices)
	{
		s_pRealEnum = (PFN_EnumDevices)vt[4];
		if (!PatchPointer(&vt[4], (void*)GuardEnumDevices))
			VRLog::Msg("VRInputGuard: could not patch EnumDevices  <- THE START-UP FIX IS NOT IN FORCE");
	}
	if (vt[3] != (void*)GuardCreateDevice)
	{
		s_pRealCreateDev = (PFN_CreateDevice)vt[3];
		if (!PatchPointer(&vt[3], (void*)GuardCreateDevice))
			VRLog::Msg("VRInputGuard: could not patch CreateDevice (keyboard with the window behind)");
	}
	return hr;
}

static void DirOf(const char* szPath, char* szDir, int nLen)
{
	strncpy(szDir, szPath, nLen - 1);
	szDir[nLen - 1] = 0;
	char* pSlash = strrchr(szDir, '\\');
	if (pSlash) *pSlash = 0;
}

void VRInputGuard_Install()
{
	char szExe[MAX_PATH] = { 0 }, szDi[MAX_PATH] = { 0 };
	HMODULE hExe = GetModuleHandleA(NULL);
	HMODULE hDi = GetModuleHandleA("dinput.dll");
	GetModuleFileNameA(hExe, szExe, MAX_PATH);
	if (hDi) GetModuleFileNameA(hDi, szDi, MAX_PATH);

	char szExeDir[MAX_PATH], szDiDir[MAX_PATH];
	DirOf(szExe, szExeDir, MAX_PATH);
	DirOf(szDi, szDiDir, MAX_PATH);

	if (hDi && _stricmp(szExeDir, szDiDir) == 0)
	{
		VRLog::Msg("VRInputGuard: the game's own DINPUT.dll is loaded (%s) - it does the fix, nothing patched", szDi);
		return;
	}
	VRLog::Msg("VRInputGuard: DINPUT.dll in use is %s, NOT the game's own - patching lithtech.exe's import instead",
		hDi ? szDi : "(none loaded yet)");

	char szBg[8] = { 0 };
	GetEnvironmentVariableA("NOLFVR_DINPUT_BG", szBg, sizeof(szBg) - 1);
	s_bBackgroundKeyboard = !(szBg[0] == '0' && szBg[1] == 0);

	// lithtech.exe's import table: DINPUT.dll!DirectInputCreateA.
	BYTE* pBase = (BYTE*)hExe;
	IMAGE_DOS_HEADER* pDos = (IMAGE_DOS_HEADER*)pBase;
	IMAGE_NT_HEADERS* pNt = (IMAGE_NT_HEADERS*)(pBase + pDos->e_lfanew);
	IMAGE_DATA_DIRECTORY& dirImp = pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (!dirImp.VirtualAddress) { VRLog::Msg("VRInputGuard: lithtech.exe has no import table"); return; }

	for (IMAGE_IMPORT_DESCRIPTOR* pImp = (IMAGE_IMPORT_DESCRIPTOR*)(pBase + dirImp.VirtualAddress); pImp->Name; pImp++)
	{
		if (_stricmp((const char*)(pBase + pImp->Name), "DINPUT.dll") != 0) continue;

		IMAGE_THUNK_DATA* pNames = (IMAGE_THUNK_DATA*)(pBase + (pImp->OriginalFirstThunk ? pImp->OriginalFirstThunk : pImp->FirstThunk));
		IMAGE_THUNK_DATA* pAddrs = (IMAGE_THUNK_DATA*)(pBase + pImp->FirstThunk);
		for (; pNames->u1.AddressOfData; pNames++, pAddrs++)
		{
			if (IMAGE_SNAP_BY_ORDINAL(pNames->u1.Ordinal)) continue;
			IMAGE_IMPORT_BY_NAME* pName = (IMAGE_IMPORT_BY_NAME*)(pBase + pNames->u1.AddressOfData);
			if (strcmp((const char*)pName->Name, "DirectInputCreateA") != 0) continue;

			s_pRealCreateA = (PFN_CreateA)pAddrs->u1.Function;
			if (PatchPointer((void**)&pAddrs->u1.Function, (void*)GuardDirectInputCreateA))
				VRLog::Msg("VRInputGuard: DirectInputCreateA import patched (real %p)", s_pRealCreateA);
			else
				VRLog::Msg("VRInputGuard: could not patch the import  <- THE START-UP FIX IS NOT IN FORCE");
			return;
		}
	}
	VRLog::Msg("VRInputGuard: DirectInputCreateA import not found  <- THE START-UP FIX IS NOT IN FORCE");
}

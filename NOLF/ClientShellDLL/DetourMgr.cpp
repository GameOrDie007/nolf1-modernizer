#include "stdafx.h"
#include "ltbasedefs.h"
#include <detours.h>
#include "DetourMgr.h"
#include "ConsoleMgr.h"

DetourMgr* g_pDetourMgr = NULL;
extern ConsoleMgr* g_pConsoleMgr;

DetourMgr::DetourMgr()
{
	g_pDetourMgr = this;
	m_pConsoleFunc = NULL;
	m_pConsoleFuncVa = NULL;
	m_pConsoleFuncSimple = NULL;
}

DetourMgr::~DetourMgr()
{
	Term();

	// Until August 2026 this destructor had never run on any build, so nothing
	// below it had ever been exercised. Clear the global before anything else can
	// reach a destroyed object through it.
	if (g_pDetourMgr == this) {
		g_pDetourMgr = NULL;
	}
}

void DetourMgr::Init()
{
	m_pConsoleFunc = (int*)CONSOLE_FUNCTION_PTR;
	m_pConsoleFuncVa = (int*)CONSOLE_FUNCTION_PTR_VA;
	m_pConsoleFuncSimple = (int*)CONSOLE_FUNCTION_PTR_SIMPLE;

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	// Attach some functions!
	DetourAttach(&(PVOID&)m_pConsoleFunc, df_Console);
	DetourAttach(&(PVOID&)m_pConsoleFuncSimple, df_ConsoleSimple);

	DetourTransactionCommit();

}

void DetourMgr::Term()
{
	// Init() may not have run. A teardown path that has never executed is not
	// entitled to assume the setup path did.
	if (!m_pConsoleFunc) {
		return;
	}

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)m_pConsoleFunc, df_Console);
	DetourDetach(&(PVOID&)m_pConsoleFuncSimple, df_ConsoleSimple);
	DetourTransactionCommit();

	// DetourDetach restores these to the original engine addresses. Null them so
	// a second Term() is a no-op rather than a second detach.
	m_pConsoleFunc = NULL;
	m_pConsoleFuncSimple = NULL;
	m_pConsoleFuncVa = NULL;
}

// Main console print method
// We can filter with iLevel, although I've only seen 0 and 1 be used, and I'm not sure either's context.
// Colour works, so that's neat.
void DetourMgr::ConsolePrint(HLTCOLOR iColour, int iLevel, const char* pMsg)
{
	if (!g_pConsoleMgr) {
		return;
	}

	std::string sString = pMsg;

	g_pConsoleMgr->Read(sString.c_str(), iColour, iLevel);

	// Run the original console print
	((ConsoleFn)m_pConsoleFunc)(iColour, iLevel, pMsg);
}


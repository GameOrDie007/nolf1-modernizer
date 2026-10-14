// VRBook.cpp: see VRBook.h.

#include "stdafx.h"
#include "VRBook.h"
#include "VRShared.h"
#include "VRLog.h"

#include <windows.h>
#include <stdio.h>
#include <stddef.h>

namespace
{
	// The layer's block. Its layout is the contract: the same 540 bytes in a
	// 32-bit game and a 64-bit host.
#pragma pack(push, 4)
	struct BookSignal
	{
		unsigned long	magic;			// "GTB1", written by the layer
		unsigned long	size;			// sizeof(BookSignal)
		volatile long	ready;			// 1 while the session can show the book
		volatile long	open;			// the game sets 1; the layer takes it back to 0
		volatile long	showing;		// 1 while the book is open
		long			main_hand;		// 0 left, 1 right: the page-turning hand
		long			floating;		// 0 = held in the other hand
		char			tome_dir[512];	// the folder holding tome.txt
	};
#pragma pack(pop)
	typedef char CheckSize[sizeof(BookSignal) == 540 ? 1 : -1];
	typedef char CheckDir[offsetof(BookSignal, tome_dir) == 28 ? 1 : -1];

	const unsigned long kMagic = 0x31425447ul;

	HANDLE			g_hMap = NULL;
	BookSignal*		g_pSig = NULL;
	unsigned long	g_nPid = 0;			// the host the block belongs to
	DWORD			g_nTried = 0;
	char			g_szDir[MAX_PATH] = "";
	int				g_nInstalled = -1;	// not looked yet

	// The folder beside the game's: lithtech.exe runs from game\.
	const char* TomeDir()
	{
		if (!g_szDir[0])
		{
			char szExe[MAX_PATH] = "";
			GetModuleFileNameA(NULL, szExe, MAX_PATH);
			char* p = strrchr(szExe, '\\');
			if (p) *p = 0;
			char szRel[MAX_PATH];
			_snprintf(szRel, sizeof(szRel), "%s\\..\\GameTome", szExe);
			szRel[sizeof(szRel) - 1] = 0;
			if (!GetFullPathNameA(szRel, MAX_PATH, g_szDir, NULL))
				strcpy(g_szDir, szRel);
		}
		return g_szDir;
	}

	BookSignal* Signal()
	{
		const unsigned long nPid = VRShared::State().nHostPid;
		if (!nPid) return NULL;
		if (g_pSig && g_nPid == nPid) return g_pSig;
		if (g_pSig) { UnmapViewOfFile(g_pSig); g_pSig = NULL; }
		if (g_hMap) { CloseHandle(g_hMap); g_hMap = NULL; }

		// The menu asks every time it opens; once a second is plenty.
		const DWORD now = GetTickCount();
		if (g_nTried && now - g_nTried < 1000) return NULL;
		g_nTried = now;

		char szName[96];
		_snprintf(szName, sizeof(szName), "Local\\GameOrDie.GameTome.%lu", nPid);
		szName[sizeof(szName) - 1] = 0;
		HANDLE h = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, szName);
		if (!h) return NULL;
		BookSignal* p = (BookSignal*)MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(BookSignal));
		if (!p || p->magic != kMagic || p->size != sizeof(BookSignal))
		{
			VRLog::Msg("VRBook: %s is not the book's block - no book", szName);
			if (p) UnmapViewOfFile(p);
			CloseHandle(h);
			return NULL;
		}
		g_hMap = h;
		g_pSig = p;
		g_nPid = nPid;
		VRLog::Msg("VRBook: the layer's signal is open (ready %ld)", p->ready);
		return p;
	}
}

bool VRBook::Installed()
{
	if (g_nInstalled < 0)
	{
		char szTxt[MAX_PATH + 16];
		_snprintf(szTxt, sizeof(szTxt), "%s\\tome.txt", TomeDir());
		szTxt[sizeof(szTxt) - 1] = 0;
		g_nInstalled = GetFileAttributesA(szTxt) != INVALID_FILE_ATTRIBUTES ? 1 : 0;
		VRLog::Msg("VRBook: %s %s", szTxt, g_nInstalled ? "is here" : "is not here - no Game Manual row");
	}
	return g_nInstalled == 1;
}

bool VRBook::Ready()
{
	if (!Installed()) return false;
	BookSignal* p = Signal();
	return p && p->ready;
}

bool VRBook::Open()
{
	BookSignal* p = Installed() ? Signal() : NULL;
	if (!p || !p->ready)
	{
		VRLog::Msg("VRBook: asked for the book, but the layer is not ready");
		return false;
	}
	_snprintf(p->tome_dir, sizeof(p->tome_dir), "%s", TomeDir());
	p->tome_dir[sizeof(p->tome_dir) - 1] = 0;
	p->main_hand = VRShared::SwapHands() ? 0 : 1;
	p->floating = 0;
	InterlockedExchange(&p->open, 1);
	VRLog::Msg("VRBook: opened %s (pages in the %s hand)", p->tome_dir, p->main_hand ? "right" : "left");
	return true;
}

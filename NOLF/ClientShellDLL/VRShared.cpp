// ----------------------------------------------------------------------- //
//
// MODULE  : VRShared.cpp
//
// PURPOSE : Client-side reader for the host's shared pose block. See
//           VRShared.h for the contract.
//
//           Fails open in every direction: if the host is absent, stopped, or
//           writing a torn frame, the client keeps its last good pose and
//           reports not-live, so the game runs flat rather than freezing on
//           stale tracking (a project rule).
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "VRShared.h"
#include <intrin.h>
#include "VRLog.h"

namespace
{
	HANDLE			g_hMap		= NULL;
	VRSharedState*	g_pShared	= NULL;

	VRSharedState	g_Snapshot	= { 0 };
	bool			g_bHaveData	= false;

	DWORD			g_dwNextTry	= 0;
	uint32_t		g_nLastSeq	= 0xFFFFFFFF;
	DWORD			g_dwLastNew	= 0;		// local tick of last fresh frame

	// The host process, opened when the block is attached (v19). Opened THEN,
	// while the host is certainly alive, so a process id reused after it exits
	// cannot be mistaken for it.
	HANDLE			g_hHostProc	= NULL;
	bool			g_bSwapHands = false;	// the Leftorium (VRLeftorium), set each frame
	bool			g_bSwapSticks = false;	// VRSwapSticks: move on the right, turn on the left
	DWORD			g_dwNextHostCheck = 0;
	bool			g_bHostGone	= false;

	bool Attach()
	{
		if (g_pShared) return true;

		// Don't hammer OpenFileMapping every frame when no host is running.
		const DWORD now = GetTickCount();
		if (now < g_dwNextTry) return false;
		g_dwNextTry = now + 1000;

		// Read/write: the client publishes its render surface size back to the
		// host, which cannot work it out from the window.
		g_hMap = OpenFileMappingA(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, VRSHARED_NAME);
		if (!g_hMap) return false;

		g_pShared = (VRSharedState*)MapViewOfFile(g_hMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0,
			sizeof(VRSharedState));
		if (!g_pShared)
		{
			CloseHandle(g_hMap);
			g_hMap = NULL;
			return false;
		}

		if (g_pShared->nMagic != VRSHARED_MAGIC)
		{
			VRLog::Msg("VRShared: bad magic 0x%08X, ignoring", g_pShared->nMagic);
			VRShared::Close();
			g_dwNextTry = GetTickCount() + 5000;
			return false;
		}
		if (g_pShared->nVersion != VRSHARED_VERSION)
		{
			VRLog::Msg("VRShared: version %u, expected %u - host and client are out of step",
				g_pShared->nVersion, VRSHARED_VERSION);
			VRShared::Close();
			g_dwNextTry = GetTickCount() + 5000;
			return false;
		}

		// IN A MENU FROM THE FIRST FRAME. The game starts on its splash and
		// its menus, and until PublishInMenu first ran (six seconds in) the
		// host read 0 here and showed the eye split - the splash as two
		// copies side by side for a moment (headset testing, 13 September evening).
		g_pShared->nInMenu = 1;
		VRLog::Msg("VRShared: attached to host block, in menu until the world says otherwise");

		if (g_pShared->nHostPid && !g_hHostProc)
		{
			g_hHostProc = OpenProcess(SYNCHRONIZE, FALSE, g_pShared->nHostPid);
			VRLog::Msg("VRShared: watching the host, pid %u (%s)", g_pShared->nHostPid,
				g_hHostProc ? "opened" : "COULD NOT OPEN - the game cannot tell if it exits");
		}
		return true;
	}
}

namespace VRShared
{

bool Poll()
{
	if (!Attach()) return g_bHaveData;

	// Seqlock read. An odd sequence means the host is mid-write; a changed
	// sequence across the copy means it wrote while we were reading. Either
	// way, keep the previous snapshot rather than using a torn one.
	for (int attempt = 0; attempt < 4; ++attempt)
	{
		const uint32_t before = g_pShared->nSequence;
		if (before & 1) continue;

		VRSharedState tmp = *g_pShared;

		const uint32_t after = g_pShared->nSequence;
		if (before != after) continue;

		if (tmp.nSequence != g_nLastSeq)
		{
			g_nLastSeq  = tmp.nSequence;
			g_dwLastNew = GetTickCount();
		}
		g_Snapshot  = tmp;
		// THE LEFTORIUM: the two controllers swap HERE, where the host's hand
		// state enters the game, so every reader - the gun, the trigger, the
		// sticks, the torch, the fire ray - takes the other hand with nothing
		// else changed. The menu button is on the LEFT Touch controller only,
		// and the game reads it from Hands[0]; it stays there.
		if (g_bSwapHands)
		{
			VRHandState h = g_Snapshot.Hands[0];
			g_Snapshot.Hands[0] = g_Snapshot.Hands[1];
			g_Snapshot.Hands[1] = h;
			g_Snapshot.Hands[0].nButtons |=  (g_Snapshot.Hands[1].nButtons & VRBTN_MENU);
			g_Snapshot.Hands[1].nButtons &= ~(uint32_t)VRBTN_MENU;
		}
		// THE STICKS ARE THEIR OWN CHOICE. A left-handed player holds the gun in
		// the left hand and still moves with the left stick - asked by the
		// player who asked for the Leftorium; Into the Radius splits it the
		// same way. So the sticks (axes and their click) follow VRSwapSticks,
		// not the hand swap: after the swap above, put them back unless the two
		// settings disagree. Hands[0]'s stick is the MOVE stick, Hands[1]'s turns.
		if (g_bSwapHands != g_bSwapSticks)
		{
			VRHandState& a = g_Snapshot.Hands[0];
			VRHandState& b = g_Snapshot.Hands[1];
			const float fX = a.fStickX, fY = a.fStickY;
			a.fStickX = b.fStickX; a.fStickY = b.fStickY;
			b.fStickX = fX;        b.fStickY = fY;
			const uint32_t nA = a.nButtons & VRBTN_THUMBCLICK, nB = b.nButtons & VRBTN_THUMBCLICK;
			a.nButtons = (a.nButtons & ~(uint32_t)VRBTN_THUMBCLICK) | nB;
			b.nButtons = (b.nButtons & ~(uint32_t)VRBTN_THUMBCLICK) | nA;
		}
		g_bHaveData = true;
		return true;
	}

	return g_bHaveData;
}

const VRSharedState& State()
{
	return g_Snapshot;
}

bool IsLive()
{
	if (!g_bHaveData) return false;

	// A freshly created block reads as sequence 0 with everything zeroed. That
	// is "attached", not "tracking" - requiring a non-zero sequence stops the
	// first read of an empty block being reported as live.
	if (g_Snapshot.nSequence == 0) return false;

	// The host stamps its own GetTickCount, so this measures the host's
	// liveness directly rather than our bookkeeping about it. Same machine,
	// same clock, no skew.
	const DWORD now = GetTickCount();
	if ((now - g_Snapshot.nHostAliveTick) > 1000) return false;

	return (now - g_dwLastNew) < 1000;
}

void SetSwapHands(bool bSwap) { g_bSwapHands = bSwap; }
void SetSwapSticks(bool bSwap) { g_bSwapSticks = bSwap; }
bool SwapSticks() { return g_bSwapSticks; }
bool SwapHands() { return g_bSwapHands; }

bool HostGone()
{
	if (g_bHostGone) return true;
	if (!g_hHostProc) return false;
	const DWORD now = GetTickCount();
	if (now < g_dwNextHostCheck) return false;
	g_dwNextHostCheck = now + 500;
	if (WaitForSingleObject(g_hHostProc, 0) != WAIT_OBJECT_0) return false;
	DWORD nCode = 0;
	GetExitCodeProcess(g_hHostProc, &nCode);
	VRLog::Msg("VRShared: THE HOST HAS EXITED (pid %u, exit code %lu)",
		g_Snapshot.nHostPid ? g_Snapshot.nHostPid : 0u, (unsigned long)nCode);
	g_bHostGone = true;
	return true;
}

void PublishScreenSize(uint32_t nWidth, uint32_t nHeight)
{
	if (!g_pShared) return;
	if (g_pShared->nGameScreenW == nWidth && g_pShared->nGameScreenH == nHeight) return;

	// Written outside the host's seqlock. These are two independent uint32s
	// that the host only reads for sizing, so a torn read is harmless and
	// self-corrects on the next frame.
	g_pShared->nGameScreenW = nWidth;
	g_pShared->nGameScreenH = nHeight;
	VRLog::Msg("VRShared: published game screen size %ux%u", nWidth, nHeight);
}

void PublishFov(float fFovXRad, float fFovYRad)
{
	if (!g_pShared) return;
	if (g_pShared->fGameFovXRad == fFovXRad && g_pShared->fGameFovYRad == fFovYRad) return;

	g_pShared->fGameFovXRad = fFovXRad;
	g_pShared->fGameFovYRad = fFovYRad;
	VRLog::Msg("VRShared: published rendered fov %.2f x %.2f deg",
		fFovXRad * 57.2957795f, fFovYRad * 57.2957795f);
}

void PublishMarker(uint32_t nHostFrame)
{
	if (!g_pShared) return;
	if (g_pShared->nVersion < 16) return;
	g_pShared->nMarkerPainted = nHostFrame;
}

void Haptic(int nHand, float fAmp, float fMs)
{
	if (!g_pShared) return;
	if (g_pShared->nVersion < 17) return;
	// The game names the LOGICAL hand; the host pulses a PHYSICAL controller.
	g_pShared->nHapticHand = (uint32_t)(((nHand ? 1 : 0) ^ (g_bSwapHands ? 1 : 0)) & 1);
	g_pShared->fHapticAmp  = (fAmp < 0.0f) ? 0.0f : (fAmp > 1.0f ? 1.0f : fAmp);
	g_pShared->fHapticMs   = (fMs < 1.0f) ? 1.0f : fMs;
	_ReadWriteBarrier();
	++g_pShared->nHapticSerial;
}

void PublishPoseLag(float fMs)
{
	if (!g_pShared) return;
	if (g_pShared->nPoseLagValid && g_pShared->fPoseLagMs == fMs) return;

	g_pShared->fPoseLagMs   = fMs;
	g_pShared->nPoseLagValid = 1;
	VRLog::Msg("VRShared: published pose lag %.1f ms", fMs);
}

void PublishAsymActive(bool bActive)
{
	if (!g_pShared) return;
	const uint32_t n = bActive ? 1u : 0u;
	if (g_pShared->nAsymActive == n) return;

	g_pShared->nAsymActive = n;
	VRLog::Msg("VRShared: per-eye optical centres %s", bActive ? "ACTIVE" : "off");
}

void PublishAppliedCentre(int nEye, float fYawRad, float fPitchRad)
{
	if (!g_pShared || nEye < 0 || nEye > 1) return;
	g_pShared->fAppliedYawRad[nEye]   = fYawRad;
	g_pShared->fAppliedPitchRad[nEye] = fPitchRad;
}

void PublishExactPose(bool bOn)
{
	if (!g_pShared) return;
	const uint32_t n = bOn ? 1u : 0u;
	if (g_pShared->nExactPose == n) return;

	g_pShared->nExactPose = n;
	VRLog::Msg("VRShared: exact per-frame pose %s", bOn ? "ON" : "OFF");
}

void PublishCalib(bool bActive, float fYawRad)
{
	if (!g_pShared) return;
	g_pShared->nCalibActive = bActive ? 1u : 0u;
	g_pShared->fCalibYawRad = fYawRad;
}

void PublishHeadLocked(bool bLocked)
{
	if (!g_pShared) return;
	const uint32_t n = bLocked ? 1u : 0u;
	if (g_pShared->nHeadLocked == n) return;

	g_pShared->nHeadLocked = n;
	VRLog::Msg("VRShared: head-locked submission %s (head rotation %s)",
		bLocked ? "ON" : "off",
		bLocked ? "goes through the mouse path" : "is declared to the runtime");
}

void PublishBodyYaw(float fYawRad, int nMode)
{
	if (!g_pShared) return;

	const uint32_t n = (uint32_t)((nMode < 0) ? 0 : ((nMode > 2) ? 2 : nMode));

	// Written every frame - the yaw changes continuously - so only the MODE
	// change is worth a log line.
	g_pShared->fBodyYawRad = fYawRad;

	if (g_pShared->nYawSpaceMode != n)
	{
		g_pShared->nYawSpaceMode = n;
		VRLog::Msg("VRShared: yaw-carrying reference space %s",
			(n == 0) ? "OFF - host declares in LOCAL, frames disagree by the body yaw"
					 : ((n == 1) ? "ON (sign +)" : "ON (sign -)"));
	}
}

void RequestRecenter()
{
	if (!g_pShared) return;
	++g_pShared->nRecenterReq;
	VRLog::Msg("VRRecenter: requested (%u)", (unsigned)g_pShared->nRecenterReq);
}

uint32_t RecenterGeneration()
{
	return g_pShared ? g_pShared->nRecenterGen : 0u;
}

static uint32_t g_nHeldCommands = 0;

void SetCommandHeld(int nCmd, bool bHeld)
{
	if (nCmd < 0 || nCmd > 31) return;
	const uint32_t bit = 1u << nCmd;
	if (bHeld) g_nHeldCommands |=  bit;
	else       g_nHeldCommands &= ~bit;
}

bool CommandOn(int nCmd)
{
	if (nCmd < 0 || nCmd > 31) return false;
	return (g_nHeldCommands & (1u << nCmd)) != 0;
}

void ClearCommands()
{
	g_nHeldCommands = 0;
}

void PublishInMenu(bool bInMenu)
{
	if (!g_pShared) return;
	const uint32_t n = bInMenu ? 1u : 0u;
	if (g_pShared->nInMenu == n) return;
	g_pShared->nInMenu = n;
	VRLog::Msg("VRShared: %s", bInMenu ? "entered menu/loading" : "entered world");
}

void Close()
{
	if (g_pShared) { UnmapViewOfFile(g_pShared); g_pShared = NULL; }
	if (g_hMap)    { CloseHandle(g_hMap);        g_hMap = NULL; }
	if (g_hHostProc) { CloseHandle(g_hHostProc); g_hHostProc = NULL; }
	g_bHaveData = false;
}

} // namespace VRShared

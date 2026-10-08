// VRBinds.cpp: see VRBinds.h.

#include "stdafx.h"
#include "VRBinds.h"
#include "VRShared.h"
#include "ClientUtilities.h"
#include "VRLog.h"

#include <stdio.h>
#include <math.h>

namespace VRBinds
{
namespace
{
	// Console-variable suffixes: VRBindQuestUse, VRBindFrameReload...
	// Never rename one, it is the player's saved setting.
	const char* const kKey[ACT_COUNT] = {
		"Use", "Reload", "Jump", "Duck",
		"Flashlight", "Wheel", "Holster", "QuickSave",
		"PrevWeapon", "NextWeapon", "NextAmmo",
	};

	const char* const kLabel[ACT_COUNT] = {
		"Use / activate", "Reload", "Jump", "Duck",
		"Flashlight", "Weapon wheel", "Holster", "Quick save",
		"Previous weapon", "Next weapon", "Next ammo type",
	};

	// THE DEFAULTS ARE THE LAYOUTS THAT SHIPPED. Quest: v1.1's, so nothing
	// moves for anyone who never opens the page. Frame: the v1.2 layout.
	const int kQuest[ACT_COUNT] = {
		PAD_X, PAD_Y, PAD_A, PAD_B,
		PAD_LTRIGGER, PAD_RSTICK, PAD_NONE, PAD_NONE,
		PAD_NONE, PAD_NONE, PAD_NONE,
	};
	const int kFrame[ACT_COUNT] = {
		PAD_Y, PAD_X, PAD_A, PAD_B,
		PAD_LTRIGGER, PAD_RSTICK, PAD_DDOWN, PAD_DUP,
		PAD_DLEFT, PAD_DRIGHT, PAD_RSHOULDER,
	};

	uint32 s_nNow = 0, s_nWas = 0;		// bit per Pad
	bool   s_bFrame = false;

	void VarName(char* pOut, size_t n, int nAct, bool bFrame)
	{
		_snprintf(pOut, n, "VRBind%s%s", bFrame ? "Frame" : "Quest", kKey[nAct]);
		pOut[n - 1] = 0;
	}
}

const char* ActLabel(int nAct)
{
	return (nAct >= 0 && nAct < ACT_COUNT) ? kLabel[nAct] : "";
}

const char* PadLabel(int nPad, bool bFrame)
{
	switch (nPad)
	{
	case PAD_NONE:      return "(none)";
	case PAD_A:         return bFrame ? "A" : "A (right)";
	case PAD_B:         return bFrame ? "B" : "B (right)";
	case PAD_X:         return bFrame ? "X" : "X (left)";
	case PAD_Y:         return bFrame ? "Y" : "Y (left)";
	case PAD_LSTICK:    return "Left stick click";
	case PAD_RSTICK:    return "Right stick click";
	case PAD_LTRIGGER:  return "Left trigger";
	case PAD_RUP:       return "Right stick up";
	case PAD_RDOWN:     return "Right stick down";
	case PAD_DUP:       return "D-pad up";
	case PAD_DDOWN:     return "D-pad down";
	case PAD_DLEFT:     return "D-pad left";
	case PAD_DRIGHT:    return "D-pad right";
	case PAD_LSHOULDER: return "Left shoulder";
	case PAD_RSHOULDER: return "Right shoulder";
	}
	return "?";
}

int PadCount(bool bFrame)
{
	return bFrame ? PAD_COUNT : PAD_DUP;
}

int Default(int nAct, bool bFrame)
{
	if (nAct < 0 || nAct >= ACT_COUNT) return PAD_NONE;
	return bFrame ? kFrame[nAct] : kQuest[nAct];
}

int Get(int nAct, bool bFrame)
{
	if (nAct < 0 || nAct >= ACT_COUNT) return PAD_NONE;
	char sz[48];
	VarName(sz, sizeof(sz), nAct, bFrame);
	int n = (int)(GetConsoleFloat(sz, (LTFLOAT)Default(nAct, bFrame)) + 0.5f);
	if (n < 0 || n >= PadCount(bFrame)) n = Default(nAct, bFrame);
	return n;
}

void Set(int nAct, bool bFrame, int nPad)
{
	if (nAct < 0 || nAct >= ACT_COUNT) return;
	if (nPad < 0 || nPad >= PadCount(bFrame)) nPad = PAD_NONE;
	char sz[48];
	VarName(sz, sizeof(sz), nAct, bFrame);
	WriteConsoleInt(sz, nPad);
}

void ResetDefaults(bool bFrame)
{
	for (int a = 0; a < ACT_COUNT; ++a) Set(a, bFrame, Default(a, bFrame));
	VRLog::Msg("VRBinds: %s controls reset to the defaults", bFrame ? "Steam Frame" : "Quest");
}

bool Frame()
{
	return VRShared::FrameLayout();
}

void Update(bool bStickBusy)
{
	s_nWas = s_nNow;
	s_nNow = 0;
	s_bFrame = VRShared::FrameLayout();

	const VRSharedState& s = VRShared::State();
	const VRHandState& L = s.Hands[0];		// logical: the off hand
	const VRHandState& R = s.Hands[1];		// logical: the gun hand
	#define PADBIT(p) (1u << (p))

	// The stick clicks and the left trigger by LOGICAL hand on both
	// controllers: the sticks follow Swap sticks and the trigger follows the
	// Leftorium (the flashlight goes with the off hand), as they always have.
	if (L.nButtons & VRBTN_THUMBCLICK) s_nNow |= PADBIT(PAD_LSTICK);
	if (R.nButtons & VRBTN_THUMBCLICK) s_nNow |= PADBIT(PAD_RSTICK);
	if (L.nButtons & VRBTN_TRIGGER)    s_nNow |= PADBIT(PAD_LTRIGGER);

	// THE TURNING STICK UP AND DOWN. On past 0.7 with up/down the stronger
	// axis, off again below 0.4: a turn that drifts a little vertically never
	// jumps, and a held push does not flicker at the edge. After the stick
	// belonged to something else it must come back to the middle first, so
	// closing the weapon wheel with the stick still up is not a jump.
	{
		static bool s_bUp = false, s_bDown = false, s_bWaitCentre = false;
		const float y = R.fStickY;
		const float ax = fabsf(R.fStickX);
		if (bStickBusy || !R.nActive)
		{
			s_bUp = s_bDown = false;
			s_bWaitCentre = true;
		}
		else
		{
			if (s_bWaitCentre && fabsf(y) < 0.4f) s_bWaitCentre = false;
			if (s_bWaitCentre)
			{
				s_bUp = s_bDown = false;
			}
			else
			{
				s_bUp   = s_bUp   ? (y >  0.4f) : (y >  0.7f &&  y > ax);
				s_bDown = s_bDown ? (y < -0.4f) : (y < -0.7f && -y > ax);
			}
		}
		if (s_bUp)   s_nNow |= PADBIT(PAD_RUP);
		if (s_bDown) s_nNow |= PADBIT(PAD_RDOWN);
	}

	if (s_bFrame)
	{
		// Physical buttons, the Frame's face buttons do not mirror.
		const uint32 nL = VRShared::FramePhysical(0);
		const uint32 nR = VRShared::FramePhysical(1);
		if (nR & VRBTN_PRIMARY)    s_nNow |= PADBIT(PAD_A);
		if (nR & VRBTN_SECONDARY)  s_nNow |= PADBIT(PAD_B);
		if (nR & VRBTN_PADX)       s_nNow |= PADBIT(PAD_X);
		if (nR & VRBTN_PADY)       s_nNow |= PADBIT(PAD_Y);
		if (nL & VRBTN_DPAD_UP)    s_nNow |= PADBIT(PAD_DUP);
		if (nL & VRBTN_DPAD_DOWN)  s_nNow |= PADBIT(PAD_DDOWN);
		if (nL & VRBTN_DPAD_LEFT)  s_nNow |= PADBIT(PAD_DLEFT);
		if (nL & VRBTN_DPAD_RIGHT) s_nNow |= PADBIT(PAD_DRIGHT);
		if (nL & VRBTN_SHOULDER)   s_nNow |= PADBIT(PAD_LSHOULDER);
		if (nR & VRBTN_SHOULDER)   s_nNow |= PADBIT(PAD_RSHOULDER);
	}
	else
	{
		// Touch, by logical hand: A and B on the gun hand, X and Y on the off
		// hand, so the Leftorium mirrors them exactly as before.
		if (R.nButtons & VRBTN_PRIMARY)   s_nNow |= PADBIT(PAD_A);
		if (R.nButtons & VRBTN_SECONDARY) s_nNow |= PADBIT(PAD_B);
		if (L.nButtons & VRBTN_PRIMARY)   s_nNow |= PADBIT(PAD_X);
		if (L.nButtons & VRBTN_SECONDARY) s_nNow |= PADBIT(PAD_Y);
	}
	#undef PADBIT

	// SAY WHAT EACH PRESS DID, the first 40 in a run: which button, and the
	// action the map gives it. A button that "does nothing" in the headset is
	// then either absent from this line (the controller never sent it) or
	// present with "(nothing)" (the map has no action on it).
	static int s_nSaid = 0;
	if (s_nSaid < 40)
	{
		const uint32 nWent = s_nNow & ~s_nWas;
		for (int p = 1; p < PAD_COUNT && s_nSaid < 40; ++p)
		{
			if (!(nWent & (1u << p))) continue;
			const char* pszAct = "(nothing)";
			for (int a = 0; a < ACT_COUNT; ++a)
				if (Get(a, s_bFrame) == p) { pszAct = kLabel[a]; break; }
			++s_nSaid;
			VRLog::Msg("VRBinds: %s pressed %s -> %s", s_bFrame ? "Frame" : "Quest", PadLabel(p, s_bFrame), pszAct);
		}
	}
}

int PadOf(int nAct)
{
	return Get(nAct, s_bFrame);
}

bool Held(int nAct)
{
	const int p = PadOf(nAct);
	return p != PAD_NONE && (s_nNow & (1u << p)) != 0;
}

bool Pressed(int nAct)
{
	const int p = PadOf(nAct);
	return p != PAD_NONE && (s_nNow & (1u << p)) && !(s_nWas & (1u << p));
}

bool Released(int nAct)
{
	const int p = PadOf(nAct);
	return p != PAD_NONE && !(s_nNow & (1u << p)) && (s_nWas & (1u << p));
}

}	// namespace VRBinds

// VRWalk.cpp: see VRWalk.h.

#include "stdafx.h"
#include "VRWalk.h"
#include "VRShared.h"
#include "VRLog.h"
#include "ClientUtilities.h"

#include <math.h>

namespace VRWalk
{
namespace
{
	bool  s_bLive   = false;
	float s_fYawOff = 0.0f;
	bool  s_bAnalog = false;
	float s_fDirX = 0.0f, s_fDirY = 0.0f, s_fSpeed = 0.0f;
	int   s_nSaidMode = -1;
	int   s_nSaidAnalog = -1;
	int   s_nMode = 0;
	bool  s_bMoving = false;
}

bool Update(bool bVehicle)
{
	s_bLive = false;
	s_fYawOff = 0.0f;
	s_bAnalog = false;
	s_fDirX = s_fDirY = s_fSpeed = 0.0f;
	s_bMoving = false;

	if (bVehicle) return false;
	if (!VRShared::IsLive()) return false;
	if (GetConsoleFloat("VRControls", 1.0f) <= 0.0f) return false;

	const VRSharedState& s = VRShared::State();
	int nMode = (int)(GetConsoleFloat("VRWalkDir", 0.0f) + 0.5f);
	if (nMode < 0 || nMode > 2) nMode = 0;
	s_nMode = nMode;
	{
		float dzM = GetConsoleFloat("VRStickDeadzone", 0.25f);
		if (dzM < 0.05f) dzM = 0.05f;
		const float mx = s.Hands[0].fStickX, my = s.Hands[0].fStickY;
		s_bMoving = (mx * mx + my * my) > dzM * dzM;
	}

	// THE SAME SIGN THE VIEW USES. The head's yaw reaches the camera as
	// EulerRotateY(yaw * VRYawScale) on top of the body (VRYawScale -1 as
	// shipped), and the hand's as EulerRotateY(-yaw) in the aim code: in both,
	// LithTech's yaw is the negative of the degrees the host reports.
	const float fScale = GetConsoleFloat("VRYawScale", -1.0f);
	const float fD2R = 0.01745329f;
	if (nMode == 1)
		s_fYawOff = s.fHeadYawDeg * fScale * fD2R;
	else if (nMode == 2 && s.Hands[0].nActive)
		// Hands[0] is the off hand whichever side the Leftorium put it on.
		s_fYawOff = s.Hands[0].fYawDeg * fScale * fD2R;

	if (nMode != s_nSaidMode)
	{
		s_nSaidMode = nMode;
		VRLog::Msg("VRWalk: walk direction = %s", nMode == 1 ? "HEAD" : nMode == 2 ? "OFF HAND" : "body");
	}

	// ANALOG WALKING. Hands[0]'s stick is the MOVE stick (VRShared::Poll puts
	// it there for Swap sticks too). Past the dead zone, the push is rescaled
	// so the edge of the dead zone is a crawl and the rim is a run.
	const bool bAnalogOn = GetConsoleFloat("VRAnalogWalk", 0.0f) > 0.0f;
	if (bAnalogOn != (s_nSaidAnalog == 1))
	{
		s_nSaidAnalog = bAnalogOn ? 1 : 0;
		VRLog::Msg("VRWalk: analog walking %s", bAnalogOn ? "ON" : "off");
	}
	if (bAnalogOn)
	{
		float dz = GetConsoleFloat("VRStickDeadzone", 0.25f);
		if (dz < 0.05f) dz = 0.05f;
		if (dz > 0.90f) dz = 0.90f;
		const float x = s.Hands[0].fStickX, y = s.Hands[0].fStickY;
		const float m = (float)sqrt(x * x + y * y);
		if (m > dz)
		{
			float t = (m - dz) / (1.0f - dz);
			if (t > 1.0f) t = 1.0f;
			s_bAnalog = true;
			s_fDirX = x / m;
			s_fDirY = y / m;
			// A floor so the slowest push still visibly moves: a treadmill at a
			// slow pace should walk slowly, not stand still.
			s_fSpeed = 0.15f + 0.85f * t;
		}
	}

	s_bLive = true;
	return true;
}

float YawOffset() { return s_bLive ? s_fYawOff : 0.0f; }
bool  Analog()    { return s_bLive && s_bAnalog; }
float DirX()      { return s_fDirX; }
float DirY()      { return s_fDirY; }
float Speed()     { return s_fSpeed; }
bool  WantsRun()  { return Analog() && s_fSpeed > 0.75f; }
bool  Live()      { return s_bLive; }
int   Mode()      { return s_nMode; }
bool  Moving()    { return s_bLive && s_bMoving; }

}	// namespace VRWalk

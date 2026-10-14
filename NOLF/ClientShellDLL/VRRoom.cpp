// VRRoom.cpp: see VRRoom.h.

#include "stdafx.h"
#include "VRRoom.h"
#include "VRShared.h"
#include "VRLog.h"
#include "VRPrims.h"
#include "ClientUtilities.h"

#include <math.h>

namespace VRRoom
{
namespace
{
	bool  s_bRef = false;
	float s_fRefX = 0.0f, s_fRefZ = 0.0f;
	float s_fCarryX = 0.0f, s_fCarryZ = 0.0f;	// tracking metres the body has carried
	DWORD s_dwStuckSince = 0;					// when the body last could not move
	int   s_nSaidOn = -1;
	int   s_nSaidBlocked = 0;

	float UnitsPerMetre()
	{
		const float mm = GetConsoleFloat("VRUnitMM", 17.02f);
		return (mm > 0.01f) ? 1000.0f / mm : 58.75f;
	}

	// One collision move; returns where the object ended up.
	LTVector MoveTo(HOBJECT hObj, const LTVector& vTo)
	{
		LTVector v = vTo;
		g_pPhysicsLT->MoveObject(hObj, &v, 0);
		LTVector vNow;
		g_pLTClient->GetObjectPos(hObj, &vNow);
		return vNow;
	}

	LTBOOL WorldOnly(HOBJECT hObj, void*)
	{
		// The level and its doors; never a character or a pickup.
		return g_pLTClient->GetObjectType(hObj) == OT_WORLDMODEL;
	}
}

void NoteHeadRef(float x, float /*y*/, float z)
{
	s_bRef = true;
	s_fRefX = x; s_fRefZ = z;
	if (s_fCarryX != 0.0f || s_fCarryZ != 0.0f)
		VRLog::Msg("VRRoom: new head reference - %.2f %.2f m carried is forgotten", s_fCarryX, s_fCarryZ);
	s_fCarryX = s_fCarryZ = 0.0f;
	s_dwStuckSince = 0;
}

void Carried(float* pX, float* pZ)
{
	*pX = s_fCarryX;
	*pZ = s_fCarryZ;
}

void Step(HOBJECT hObj, float fYaw, bool bAllowed)
{
	const bool bOn = bAllowed && s_bRef && hObj && VRShared::IsLive()
		&& GetConsoleFloat("VRRoomScale", 1.0f) > 0.0f;
	if ((bOn ? 1 : 0) != s_nSaidOn)
	{
		s_nSaidOn = bOn ? 1 : 0;
		VRLog::Msg("VRRoom: room-scale %s", bOn ? "ON - the body walks with the head" : "off here");
	}
	if (!bOn) return;

	const VRSharedState& s = VRShared::State();
	// Where the head has walked, less what the body has already carried.
	const float hx = s.fHeadPosX - s_fRefX, hz = s.fHeadPosZ - s_fRefZ;
	{
		static DWORD s_dwSaidHead = 0;
		if (GetTickCount() - s_dwSaidHead > 10000)
		{
			s_dwSaidHead = GetTickCount();
			VRLog::Msg("VRRoom: head at %.3f %.3f %.3f m, reference %.3f %.3f, carried %.3f %.3f m", s.fHeadPosX, s.fHeadPosY, s.fHeadPosZ, s_fRefX, s_fRefZ, s_fCarryX, s_fCarryZ);
		}
	}
	float dx = hx - s_fCarryX, dz = hz - s_fCarryZ;
	const float fLeft = (float)sqrt(dx * dx + dz * dz);
	if (fLeft < 0.01f) { s_dwStuckSince = 0; return; }

	// Tracking to world by the body's yaw: x is the body's right, -z its forward.
	LTRotation rYaw;
	g_pLTClient->SetupEuler(&rYaw, 0.0f, fYaw, 0.0f);
	LTVector vU, vR, vF;
	g_pLTClient->GetRotationVectors(&rYaw, &vU, &vR, &vF);
	const float u = UnitsPerMetre();
	LTVector vWant = vR * (dx * u) + vF * (-dz * u);

	// At most 64 units a frame, in steps of 8: a move looks only at where it
	// ends, so one long move would pass through a thin wall.
	float fWant = vWant.Mag();
	if (fWant > 64.0f) { vWant = vWant * (64.0f / fWant); fWant = 64.0f; }
	LTVector vStart;
	g_pLTClient->GetObjectPos(hObj, &vStart);
	LTVector vPos = vStart;
	const int nSteps = (int)(fWant / 8.0f) + 1;
	const LTVector vStep = vWant * (1.0f / (float)nSteps);
	for (int i = 0; i < nSteps; ++i)
	{
		const LTVector vGoal = vPos + vStep;
		LTVector vGot = MoveTo(hObj, vGoal);
		const LTVector vGotH(vGot.x - vPos.x, 0.0f, vGot.z - vPos.z);
		const float fStepLen = (float)sqrt(vStep.x * vStep.x + vStep.z * vStep.z);
		if (vGotH.Mag() < fStepLen * 0.5f)
		{
			// Blocked: each axis alone, so the body slides along the wall.
			const LTVector vX(vPos.x + vStep.x, vGot.y, vPos.z);
			vGot = MoveTo(hObj, vX);
			const LTVector vZ(vGot.x, vGot.y, vGot.z + vStep.z);
			vGot = MoveTo(hObj, vZ);
		}
		vPos = vGot;
	}

	// What the body really moved, back into tracking metres.
	const LTVector vMoved(vPos.x - vStart.x, 0.0f, vPos.z - vStart.z);
	float mx = (vMoved.x * vR.x + vMoved.z * vR.z) / u;
	float mz = -(vMoved.x * vF.x + vMoved.z * vF.z) / u;

	// ONLY A STEP THAT BRINGS THE BODY NEARER THE HEAD IS KEPT. Against a wall the
	// blocked axis slides the body along it, and that slide can carry it past the
	// head; the next frame stepped it back, and with the head held still the body
	// went back and forth a couple of centimetres every few frames. Everything
	// placed from the body's camera (her hands, the gun, her body) jumped with it
	// while the world, drawn from the eyes, held still. A step that does not close
	// at least two millimetres of the gap is undone, so the body settles and stays.
	{
		const float ax = dx - mx, az = dz - mz;
		const float fAfter = (float)sqrt(ax * ax + az * az);
		if (fAfter > fLeft - 0.002f && (mx != 0.0f || mz != 0.0f)
			&& GetConsoleFloat("VRRoomSettle", 1.0f) > 0.0f)
		{
			g_pLTClient->SetObjectPos(hObj, &vStart);
			mx = mz = 0.0f;
			static int s_nSaidUndone = 0;
			if (s_nSaidUndone < 10)
			{
				++s_nSaidUndone;
				VRLog::Msg("VRRoom: a step that left the body %.3f m from the head (was %.3f) is undone", fAfter, fLeft);
			}
		}
	}
	s_fCarryX += mx;
	s_fCarryZ += mz;

	// RE-ANCHOR when the body cannot follow: the head far out (into a wall, over
	// a ledge) and the body standing still for half a second. What is left is
	// taken as carried, so the view comes back to the body instead of staying
	// away from it for good.
	const float fMoved = (float)sqrt(mx * mx + mz * mz);
	dx = hx - s_fCarryX; dz = hz - s_fCarryZ;
	const float fAway = (float)sqrt(dx * dx + dz * dz);
	if (fMoved < 0.002f && fAway > 0.05f)
	{
		if (!s_dwStuckSince) s_dwStuckSince = GetTickCount();
		if (s_nSaidBlocked < 20 && GetTickCount() - s_dwStuckSince > 300 && fAway > 0.15f)
		{
			++s_nSaidBlocked;
			VRLog::Msg("VRRoom: the body cannot follow - the head is %.2f m from it", fAway);
		}
		if (fAway > GetConsoleFloat("VRRoomReanchor", 0.75f) && GetTickCount() - s_dwStuckSince > 500)
		{
			VRLog::Msg("VRRoom: re-anchored - %.2f m the body could not walk is taken as carried", fAway);
			s_fCarryX = hx; s_fCarryZ = hz;
			s_dwStuckSince = 0;
		}
	}
	else
		s_dwStuckSince = 0;

	static int s_nSaid = 0;
	if (fMoved > 0.001f && (s_nSaid++ % 300) == 0)
		VRLog::Msg("VRRoom: body walked %.3f m with the head (carried %.2f %.2f m)", fMoved, s_fCarryX, s_fCarryZ);
}

float WallFade(const LTVector& vBodyEye, const LTVector& vEye, HOBJECT hIgnore)
{
	if (GetConsoleFloat("VRWallFade", 1.0f) <= 0.0f) return 0.0f;
	const LTVector d = vEye - vBodyEye;
	const float fLen = d.Mag();
	if (fLen < 0.5f) return 0.0f;
	ClientIntersectQuery q;
	ClientIntersectInfo  info;
	memset(&q, 0, sizeof(q));
	q.m_From = vBodyEye;
	q.m_To = vEye;
	q.m_Flags = INTERSECT_OBJECTS | IGNORE_NONSOLID;
	q.m_FilterFn = WorldOnly;
	q.m_pUserData = (void*)hIgnore;
	float fAlpha = 0.0f;
	if (g_pLTClient->IntersectSegment(&q, &info))
	{
		// Black over the first 15 cm past the wall's surface.
		const float fIn = (vEye - info.m_Point).Mag() / UnitsPerMetre();
		fAlpha = fIn / 0.15f;
		if (fAlpha > 1.0f) fAlpha = 1.0f;
	}
	static int s_nSaidFade = -1;
	const int nState = (fAlpha > 0.0f) ? 1 : 0;
	if (nState != s_nSaidFade)
	{
		s_nSaidFade = nState;
		VRLog::Msg("VRRoom: wall fade %s", nState ? "ON - the eyes are in a wall" : "off");
	}
	if (fAlpha > 0.0f)
	{
		// A small closed box round the eyes, black, over everything.
		const float h = 12.0f;
		LTVector c[8];
		for (int i = 0; i < 8; ++i)
			c[i] = vEye + LTVector((i & 1) ? h : -h, (i & 2) ? h : -h, (i & 4) ? h : -h);
		VRPrims_SetFadeBox(c, fAlpha);
	}
	return fAlpha;
}

}	// namespace VRRoom

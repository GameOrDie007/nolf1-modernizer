// ----------------------------------------------------------------------- //
//
// MODULE  : MarkSFX.cpp
//
// PURPOSE : Mark special FX - Implementation
//
// CREATED : 10/13/97
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "MarkSFX.h"
#include "iltclient.h"
#include "ltlink.h"
#include "GameClientShell.h"
#include "SurfaceFunctions.h"
#include "VarTrack.h"
#include "VRLog.h"

extern CGameClientShell* g_pGameClientShell;

#define REGION_DIAMETER			100.0f  // Squared distance actually
// RAISED WITH THE REST. The game recycles the oldest mark once this many sit
// within REGION_DIAMETER of each other, so persistent holes without this would
// still vanish anywhere you actually aimed twice - which is exactly where
// somebody would look for them.
#define MAX_MARKS_IN_REGION		64

VarTrack	g_cvarClipMarks;
VarTrack	g_cvarLightMarks;
VarTrack	g_cvarShowMarks;
VarTrack	g_cvarMarkLift;
VarTrack	g_cvarMarkFadeTime;
VarTrack	g_cvarMarkSolidTime;


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMarkSFX::Init
//
//	PURPOSE:	Create the mark
//
// ----------------------------------------------------------------------- //

LTBOOL CMarkSFX::Init(SFXCREATESTRUCT* psfxCreateStruct)
{
    if (!psfxCreateStruct) return LTFALSE;

	CSpecialFX::Init(psfxCreateStruct);

	MARKCREATESTRUCT* pMark = (MARKCREATESTRUCT*)psfxCreateStruct;

    m_Rotation = pMark->m_Rotation;
	VEC_COPY(m_vPos, pMark->m_vPos);
	m_fScale		= pMark->m_fScale;
	m_nAmmoId		= pMark->nAmmoId;
	m_nSurfaceType	= pMark->nSurfaceType;

	if (!g_cvarClipMarks.IsInitted())
	{
        g_cvarClipMarks.Init(g_pLTClient, "MarksClip", NULL, 0.0f);
	}

	if (!g_cvarLightMarks.IsInitted())
	{
        g_cvarLightMarks.Init(g_pLTClient, "MarkLight", NULL, 0.0f);
	}

	if (!g_cvarShowMarks.IsInitted())
	{
        g_cvarShowMarks.Init(g_pLTClient, "MarkShow", NULL, 1.0f);
	}

	if (!g_cvarMarkLift.IsInitted())
	{
		// How far a bullet hole sits off its surface, in world units. The
		// engine's own placement is 0.65 - about 11 mm - which shows in
		// stereo. VRMarkLift 0.65 restores it exactly.
		g_cvarMarkLift.Init(g_pLTClient, "VRMarkLift", NULL, 0.1f);
	}

	if (!g_cvarMarkFadeTime.IsInitted())
	{
        g_cvarMarkFadeTime.Init(g_pLTClient, "MarkFadeTime", NULL, 3.0f);
	}

	if (!g_cvarMarkSolidTime.IsInitted())
	{
        g_cvarMarkSolidTime.Init(g_pLTClient, "MarkSolidTime", NULL, 3.0f);
	}

    return LTTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMarkSFX::CreateObject
//
//	PURPOSE:	Create object associated with the mark
//
// ----------------------------------------------------------------------- //

LTBOOL CMarkSFX::CreateObject(ILTClient *pClientDE)
{
    if (!CSpecialFX::CreateObject(pClientDE) || !g_pGameClientShell) return LTFALSE;

	CSFXMgr* psfxMgr = g_pGameClientShell->GetSFXMgr();
    if (!psfxMgr) return LTFALSE;

	// If we're not showing marks, don't bother...

	if (!g_cvarShowMarks.GetFloat()) return LTFALSE;


	// Before we create a new buillet hole see if there is already another
	// bullet hole close by that we could use instead...

	CSpecialFXList* pList = psfxMgr->GetFXList(SFX_MARK_ID);
    if (!pList) return LTFALSE;

	int nNumBulletHoles = pList->GetSize();

    HOBJECT hMoveObj         = LTNULL;
    HOBJECT hObj             = LTNULL;
    LTFLOAT  fClosestMarkDist = REGION_DIAMETER;
    uint8   nNumInRegion     = 0;

    LTVector vPos;

	for (int i=0; i < nNumBulletHoles; i++)
	{
		if ((*pList)[i])
		{
			hObj = (*pList)[i]->GetObject();
			if (hObj)
			{
				pClientDE->GetObjectPos(hObj, &vPos);

                LTFLOAT fDist = VEC_DISTSQR(vPos, m_vPos);
				if (fDist < REGION_DIAMETER)
				{
					if (fDist < fClosestMarkDist)
					{
						fClosestMarkDist = fDist;
						hMoveObj = hObj;
					}

					if (++nNumInRegion > MAX_MARKS_IN_REGION)
					{
						// Just move this bullet-hole to the correct pos, and
						// remove thyself...

						pClientDE->SetObjectPos(hMoveObj, &m_vPos);
                        return LTFALSE;
					}
				}
			}
		}
	}


	// Setup the mark...

	ObjectCreateStruct createStruct;
	INIT_OBJECTCREATESTRUCT(createStruct);

    LTFLOAT fScaleAdjust = 1.0f;
	if (!GetImpactSprite((SurfaceType)m_nSurfaceType, fScaleAdjust, m_nAmmoId,
		createStruct.m_Filename, ARRAY_LEN(createStruct.m_Filename)))
	{
        return LTFALSE;
	}

	createStruct.m_ObjectType = OT_SPRITE;
	createStruct.m_Flags = FLAG_VISIBLE | FLAG_ROTATEABLESPRITE;

	// Should probably force this in low detail modes...

	if (g_cvarLightMarks.GetFloat() == 0.0f)
	{
		createStruct.m_Flags |= FLAG_NOLIGHT;
	}

	VEC_COPY(createStruct.m_Pos, m_vPos);
    createStruct.m_Rotation = m_Rotation;

	m_hObject = pClientDE->CreateObject(&createStruct);

	m_fScale *= fScaleAdjust;

    LTVector vScale;
	VEC_SET(vScale, m_fScale, m_fScale, m_fScale);
	m_pClientDE->SetObjectScale(m_hObject, &vScale);

	// HOW FAR OFF THE SURFACE IS IT? In the headset the holes sit off the
	// surface by a tiny amount, roughly half an inch. A decal exactly coplanar with
	// a wall would z-fight, so something already offsets these - the impact
	// position arrives from the server and this code adds nothing - and the
	// question is how much. Half an inch is about 0.75 units at this game's
	// 58.75 to the metre, which is too small to have been noticed on a monitor
	// and is a real gap in stereo. Measured rather than guessed at, capped.
	{
		IntersectQuery qL;
		IntersectInfo  iL;
		LTVector vLU, vLR, vLF;
		g_pLTClient->GetRotationVectors(&m_Rotation, &vLU, &vLR, &vLF);
		// Backwards along the mark's own forward, which is the surface normal,
		// from a little in front of it.
		qL.m_From  = m_vPos + (vLF * 8.0f);
		qL.m_To    = m_vPos - (vLF * 8.0f);
		qL.m_Flags = IGNORE_NONSOLID | INTERSECT_OBJECTS;
		if (g_pLTClient->IntersectSegment(&qL, &iL))
		{
			const LTVector vD = m_vPos - iL.m_Point;
			const LTFLOAT fGap = vD.Dot(vLF);
			static long s_nSaidLift = 0;
			if (s_nSaidLift < 8)
			{
				++s_nSaidLift;
				VRLog::Msg("VRMarkLift: mark sat %.2f units off the surface"
						   " (%.1f mm); seating it at %.2f",
						   fGap, fGap * 1000.0f / 58.75f,
						   g_cvarMarkLift.GetFloat());
			}
			// SEAT IT. Measured at 0.65 units - 11 mm - which is the roughly
			// half an inch seen in the headset. Invisible on a monitor and a real
			// gap in stereo, now that the decal lies flat rather than facing
			// the viewer.
			//
			// Not seated flush: a quad exactly coplanar with a wall z-fights,
			// and this game's depth range is near 1.03 to far 100000. A tenth
			// of a unit is under two millimetres and enough to stay in front.
			if (fGap > 0.0f)
				m_vPos = iL.m_Point + vLF * g_cvarMarkLift.GetFloat();
		}
	}

	// WHAT DID THIS MARK LAND ON? A short ray through the impact point, the
	// same trick the clip option below uses. Done regardless of that option
	// because riding a door is not the same feature as clipping to a polygon.
	{
		IntersectQuery qRide;
		IntersectInfo  iRide;
		LTVector vRU, vRR, vRF;
		g_pLTClient->GetRotationVectors(&m_Rotation, &vRU, &vRR, &vRF);
		qRide.m_From  = m_vPos + (vRF * 2.0f);
		qRide.m_To    = m_vPos - (vRF * 2.0f);
		qRide.m_Flags = IGNORE_NONSOLID | INTERSECT_OBJECTS;
		if (g_pLTClient->IntersectSegment(&qRide, &iRide) && iRide.m_hObject
			&& !IsMainWorld(iRide.m_hObject))
		{
			RideOn(iRide.m_hObject);
		}
	}

	if (g_cvarClipMarks.GetFloat() > 0)
	{
		// Clip the mark to th poly...

		IntersectQuery qInfo;
		IntersectInfo iInfo;

        LTVector vU, vR, vF;
        g_pLTClient->GetRotationVectors(&m_Rotation, &vU, &vR, &vF);

		qInfo.m_From = m_vPos + (vF * 2.0);
		qInfo.m_To   = m_vPos - (vF * 2.0);

		qInfo.m_Flags = IGNORE_NONSOLID | INTERSECT_HPOLY;

        if (g_pLTClient->IntersectSegment(&qInfo, &iInfo))
		{
            g_pLTClient->ClipSprite(m_hObject, iInfo.m_hPoly);
		}
	}

    LTFLOAT r, g, b, a;
	r = g = b = 0.5f;
	a = 1.0f;
	m_pClientDE->SetObjectColor(m_hObject, r, g, b, a);

	m_fStartTime = m_pClientDE->GetTime();

    return LTTRUE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMarkSFX::WantRemove
//
//	PURPOSE:	If this gets called, remove the mark (this should only get
//				called if we have a server object associated with us, and
//				that server object gets removed).
//
// ----------------------------------------------------------------------- //

void CMarkSFX::WantRemove(LTBOOL bRemove)
{
	CSpecialFX::WantRemove(bRemove);

	if (!g_pGameClientShell) return;

	CSFXMgr* psfxMgr = g_pGameClientShell->GetSFXMgr();
	if (!psfxMgr) return;

	// Tell the special fx mgr to go ahead and remove us...

	if (m_hObject)
	{
		psfxMgr->RemoveSpecialFX(m_hObject);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMarkSFX::Update
//
//	PURPOSE:	Always return TRUE, however check to see if we should
//				hide/show the mark.
//
// ----------------------------------------------------------------------- //

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMarkSFX::RideOn()
//
//	PURPOSE:	Remember this mark's place in a moving brush's own frame.
//
//				Stored as an offset and two axes in the PARENT's basis rather
//				than as a parent-space matrix, because rebuilding a rotation
//				from a forward and an up is one call the engine already has,
//				and a matrix would have to be inverted here and composed every
//				frame for no gain.
//
// ----------------------------------------------------------------------- //

void CMarkSFX::RideOn(HLOCALOBJ hObj)
{
	if (!hObj) return;

	LTVector vPPos;
	LTRotation rPRot;
	g_pLTClient->GetObjectPos(hObj, &vPPos);
	g_pLTClient->GetObjectRotation(hObj, &rPRot);

	LTVector vPU, vPR, vPF;
	g_pLTClient->GetRotationVectors(&rPRot, &vPU, &vPR, &vPF);

	const LTVector vD = m_vPos - vPPos;
	m_vRideOfs.x = vD.Dot(vPR);
	m_vRideOfs.y = vD.Dot(vPU);
	m_vRideOfs.z = vD.Dot(vPF);

	LTVector vMU, vMR, vMF;
	g_pLTClient->GetRotationVectors(&m_Rotation, &vMU, &vMR, &vMF);
	m_vRideFwd.x = vMF.Dot(vPR);
	m_vRideFwd.y = vMF.Dot(vPU);
	m_vRideFwd.z = vMF.Dot(vPF);
	m_vRideUp.x  = vMU.Dot(vPR);
	m_vRideUp.y  = vMU.Dot(vPU);
	m_vRideUp.z  = vMU.Dot(vPF);

	m_hRideObj = hObj;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMarkSFX::UpdateRide()
//
//	PURPOSE:	Put the mark back where its surface has taken it.
//
// ----------------------------------------------------------------------- //

void CMarkSFX::UpdateRide()
{
	if (!m_hRideObj || !m_hObject) return;

	LTVector vPPos;
	LTRotation rPRot;
	g_pLTClient->GetObjectPos(m_hRideObj, &vPPos);
	g_pLTClient->GetObjectRotation(m_hRideObj, &rPRot);

	LTVector vPU, vPR, vPF;
	g_pLTClient->GetRotationVectors(&rPRot, &vPU, &vPR, &vPF);

	LTVector vPos = vPPos + vPR * m_vRideOfs.x
						  + vPU * m_vRideOfs.y
						  + vPF * m_vRideOfs.z;
	LTVector vFwd = vPR * m_vRideFwd.x + vPU * m_vRideFwd.y + vPF * m_vRideFwd.z;
	LTVector vUp  = vPR * m_vRideUp.x  + vPU * m_vRideUp.y  + vPF * m_vRideUp.z;

	LTRotation rNew;
	g_pLTClient->AlignRotation(&rNew, &vFwd, &vUp);

	g_pLTClient->SetObjectPos(m_hObject, &vPos);
	g_pLTClient->SetObjectRotation(m_hObject, &rNew);
}


LTBOOL CMarkSFX::Update()
{
	if (!g_cvarShowMarks.GetFloat())
	{
		// Remove the object...
		return LTFALSE;
	}

	// BEFORE ANY EARLY RETURN. A persistent mark returns below without ever
	// reaching the fade, and a mark that rides a door has to be moved on
	// exactly those frames.
	UpdateRide();

	LTFLOAT fTime = g_pLTClient->GetTime();

	// VRPersistentFX: the hole stays. Returning early rather than setting a
	// huge time keeps the arithmetic below honest - a fade that divides by
	// MarkFadeTime should not be handed a number chosen to make it a no-op.
	if (g_pGameClientShell && g_pGameClientShell->VRPersistentFX())
		return LTTRUE;

	LTFLOAT fFadeStartTime = m_fStartTime + g_cvarMarkSolidTime.GetFloat();
    LTFLOAT fFadeEndTime = fFadeStartTime + g_cvarMarkFadeTime.GetFloat();
	if (fTime > fFadeEndTime)
	{
		// Remove the object...
		return LTFALSE;
	}
	else if (fTime > fFadeStartTime)
	{
		LTFLOAT fScale = ((fFadeEndTime - fTime) / g_cvarMarkFadeTime.GetFloat());
		LTFLOAT r, g, b, a;
	
		g_pLTClient->GetObjectColor(m_hObject, &r, &g, &b, &a);
		a = a < fScale ? a : fScale;
		g_pLTClient->SetObjectColor(m_hObject, r, g, b, a);
	}

	return LTTRUE;
}
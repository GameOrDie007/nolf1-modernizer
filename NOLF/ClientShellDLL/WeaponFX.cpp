// ----------------------------------------------------------------------- //
//
// MODULE  : WeaponFX.cpp
//
// PURPOSE : Weapon special FX - Implementation
//
// CREATED : 2/22/98
//
// (c) 1997-2000 Monolith Productions, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "WeaponFX.h"
#include "iltclient.h"
#include "ClientUtilities.h"
#include "WeaponFXTypes.h"
#include "GameClientShell.h"
#include "MarkSFX.h"
#include "VRLog.h"
#include "VRPrims.h"
#include "ParticleShowerFX.h"
#include "DynamicLightFX.h"
#include "BulletTrailFX.h"
#include "MsgIDs.h"
#include "ShellCasingFX.h"
#include "ParticleExplosionFX.h"
#include "BaseScaleFX.h"
#include "DebrisFX.h"
#include "CMoveMgr.h"
#include "RandomSparksFX.h"
#include "iltphysics.h"
#include "iltcustomdraw.h"
#include "MuzzleFlashFX.h"
#include "SurfaceFunctions.h"
#include "VarTrack.h"
#include "PolyDebrisFX.h"
#include "CharacterFX.h"
#include "VRShared.h"

extern CGameClientShell* g_pGameClientShell;

static uint32 s_nNumShells = 0;

VarTrack	g_cvarShowFirePath;
VarTrack	g_cvarLightBeamColorDelta;
VarTrack	g_cvarImpactPitchShift;
VarTrack	g_cvarFlyByRadius;
VarTrack	g_cvarFlyBySoundRadius;
VarTrack	g_vtBloodSplatsMinNum;
VarTrack	g_vtBloodSplatsMaxNum;
VarTrack	g_vtVRDecalLift;
VarTrack	g_vtBloodSplatsMinLifetime;
VarTrack	g_vtBloodSplatsMaxLifetime;
VarTrack	g_vtBloodSplatsMinScale;
VarTrack	g_vtBloodSplatsMaxScale;
VarTrack	g_vtBloodSplatsRange;
VarTrack	g_vtBloodSplatsPerturb;
VarTrack	g_vtBigBloodSizeScale;
VarTrack	g_vtBigBloodLifeScale;
VarTrack	g_vtCreatePolyDebris;
VarTrack	g_vtWeaponFXMinImpactDot;
VarTrack	g_vtWeaponFXMinFireDot;
VarTrack	g_vtWeaponFXUseFOVPerformance;
// Units up the barrel from the DRAWN gun's centre that a tracer starts.
// See CWeaponFX::VRMuzzleOrFirePos.
VarTrack	g_vtVRTracerFwd;
VarTrack	g_vtVRNodeProbe;
// Defined beside VRMuzzleOrFirePos; used by the tracer and the casings above it.
static LTVector VRPerEffectOffset(const LTVector& vOff, const LTVector& vDir);
// Where a shell leaves, relative to the bore: right and up, in units.
VarTrack	g_vtVRShellRight;
VarTrack	g_vtVRShellUp;
VarTrack	g_vtWeaponFXMaxFireDist;
VarTrack	g_vtWeaponFXMaxImpactDist;
VarTrack	g_vtWeaponFXMaxMultiImpactDist;
VarTrack	g_vtMultiDing;

LTBOOL		g_bCanSeeImpactPos	= LTTRUE;
LTBOOL		g_bCanSeeFirePos	= LTTRUE;
LTBOOL		g_bDistantFirePos	= LTFALSE;
LTBOOL		g_bDistantImpactPos	= LTFALSE;

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::Init
//
//	PURPOSE:	Init the weapon fx
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponFX::Init(HLOCALOBJ hServObj, HMESSAGEREAD hMessage)
{
    if (!CSpecialFX::Init(hServObj, hMessage)) return LTFALSE;
    if (!hMessage) return LTFALSE;

	WCREATESTRUCT w;

	w.hServerObj	= hServObj;
    w.hFiredFrom    = g_pLTClient->ReadFromMessageObject(hMessage);
    w.nWeaponId     = g_pLTClient->ReadFromMessageByte(hMessage);
    w.nAmmoId       = g_pLTClient->ReadFromMessageByte(hMessage);
    w.nSurfaceType  = g_pLTClient->ReadFromMessageByte(hMessage);
    w.wIgnoreFX     = g_pLTClient->ReadFromMessageWord(hMessage);
    w.nShooterId    = g_pLTClient->ReadFromMessageByte(hMessage);
    g_pLTClient->ReadFromMessageVector(hMessage, &(w.vFirePos));
    g_pLTClient->ReadFromMessageVector(hMessage, &(w.vPos));
    g_pLTClient->ReadFromMessageVector(hMessage, &(w.vSurfaceNormal));
	// This doesn't always give the correct values...
    //g_pLTClient->ReadFromMessageCompPosition(hMessage, &(w.vFirePos));
    //g_pLTClient->ReadFromMessageCompPosition(hMessage, &(w.vPos));
    //g_pLTClient->ReadFromMessageCompPosition(hMessage, &(w.vSurfaceNormal));

	return Init(&w);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::Init
//
//	PURPOSE:	Init the weapon fx
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponFX::Init(SFXCREATESTRUCT* psfxCreateStruct)
{
    if (!CSpecialFX::Init(psfxCreateStruct)) return LTFALSE;

	WCREATESTRUCT* pCS = (WCREATESTRUCT*)psfxCreateStruct;

	m_nWeaponId		= pCS->nWeaponId;
	m_nAmmoId		= pCS->nAmmoId;
	m_eSurfaceType	= (SurfaceType)pCS->nSurfaceType;
	m_wIgnoreFX		= pCS->wIgnoreFX;

    m_hFiredFrom     = pCS->hFiredFrom; // LTNULL
	m_vFirePos		 = pCS->vFirePos;
	m_vPos			 = pCS->vPos;
	m_vSurfaceNormal = pCS->vSurfaceNormal;
	m_vSurfaceNormal.Norm();

	m_eCode			= CC_NO_CONTAINER;
	m_eFirePosCode	= CC_NO_CONTAINER;

	m_pAmmo = g_pWeaponMgr->GetAmmo(m_nAmmoId);
    if (!m_pAmmo) return LTFALSE;

	m_pWeapon = g_pWeaponMgr->GetWeapon(m_nWeaponId);
    if (!m_pWeapon) return LTFALSE;

    m_fInstDamage   = (LTFLOAT) m_pAmmo->nInstDamage;
    m_fAreaDamage   = (LTFLOAT) m_pAmmo->nAreaDamage;

	m_nShooterId	= pCS->nShooterId;
	m_bLocal		= pCS->bLocal;

	if (!g_cvarShowFirePath.IsInitted())
	{
		g_cvarShowFirePath.Init(g_pLTClient, "ShowFirePath", NULL, -1.0f);
    }

	if (!g_cvarLightBeamColorDelta.IsInitted())
	{
		g_cvarLightBeamColorDelta.Init(g_pLTClient, "LightBeamColorDelta", NULL, 50.0f);
	}

	if (!g_cvarImpactPitchShift.IsInitted())
	{
		g_cvarImpactPitchShift.Init(g_pLTClient, "PitchShiftImpact", NULL, -1.0f);
	}

	if (!g_cvarFlyByRadius.IsInitted())
	{
		g_cvarFlyByRadius.Init(g_pLTClient, "FlyByRadius", NULL, 300.0f);
	}

	if (!g_cvarFlyBySoundRadius.IsInitted())
	{
		g_cvarFlyBySoundRadius.Init(g_pLTClient, "FlyBySoundRadius", NULL, 500.0f);
	}

	if (!g_vtBloodSplatsMinNum.IsInitted())
	{
		g_vtBloodSplatsMinNum.Init(g_pLTClient, "BloodSplatsMinNum", NULL, 3.0f);
	}

	if (!g_vtBloodSplatsMaxNum.IsInitted())
	{
		g_vtBloodSplatsMaxNum.Init(g_pLTClient, "BloodSplatsMaxNum", NULL, 10.0f);
	}

	if (!g_vtBloodSplatsMinLifetime.IsInitted())
	{
		// How far a decal sits off the surface, in world units. See the note
		// at the splat. 0.5 is about 8 mm; the original was 2, along the
		// shot direction rather than the normal.
		g_vtVRDecalLift.Init(g_pLTClient, "VRDecalLift", NULL, 0.5f);
		g_vtBloodSplatsMinLifetime.Init(g_pLTClient, "BloodSplatsMinLifetime", NULL, 5.0f);
	}

	if (!g_vtBloodSplatsMaxLifetime.IsInitted())
	{
		g_vtBloodSplatsMaxLifetime.Init(g_pLTClient, "BloodSplatsMaxLifetime", NULL, 10.0f);
	}

	if (!g_vtBloodSplatsMinScale.IsInitted())
	{
		g_vtBloodSplatsMinScale.Init(g_pLTClient, "BloodSplatsMinScale", NULL, 0.01f);
	}

	if (!g_vtBloodSplatsMaxScale.IsInitted())
	{
		g_vtBloodSplatsMaxScale.Init(g_pLTClient, "BloodSplatsMaxScale", NULL, 0.05f);
	}

	if (!g_vtBloodSplatsRange.IsInitted())
	{
		g_vtBloodSplatsRange.Init(g_pLTClient, "BloodSplatsRange", NULL, 500.0f);
	}

	if (!g_vtBloodSplatsPerturb.IsInitted())
	{
		g_vtBloodSplatsPerturb.Init(g_pLTClient, "BloodSplatsPerturb", NULL, 100.0f);
	}

	if (!g_vtBigBloodSizeScale.IsInitted())
	{
		g_vtBigBloodSizeScale.Init(g_pLTClient, "BigBloodSizeScale", NULL, 5.0f);
	}

	if (!g_vtBigBloodLifeScale.IsInitted())
	{
		g_vtBigBloodLifeScale.Init(g_pLTClient, "BigBloodLifeScale", NULL, 3.0f);
	}

	if (!g_vtCreatePolyDebris.IsInitted())
	{
		g_vtCreatePolyDebris.Init(g_pLTClient, "CreatePolyDebris", NULL, 1.0f);
	}

	if (!g_vtWeaponFXMinFireDot.IsInitted())
	{
		g_vtWeaponFXMinFireDot.Init(g_pLTClient, "WeaponFXMinFireDot", NULL, 0.6f);
	}

	if (!g_vtWeaponFXMinImpactDot.IsInitted())
	{
		g_vtWeaponFXMinImpactDot.Init(g_pLTClient, "WeaponFXMinImpactDot", NULL, 0.6f);
	}

	if (!g_vtWeaponFXUseFOVPerformance.IsInitted())
	{
		g_vtWeaponFXUseFOVPerformance.Init(g_pLTClient, "WeaponFXUseFOVPerformance", NULL, 1.0f);
	}
	if (!g_vtVRTracerFwd.IsInitted())
	{
		g_vtVRTracerFwd.Init(g_pLTClient, "VRTracerFwd", NULL, 0.0f);
	}
	if (!g_vtVRNodeProbe.IsInitted())
	{
		g_vtVRNodeProbe.Init(g_pLTClient, "VRNodeProbe", NULL, 0.0f);
	}
	if (!g_vtVRShellRight.IsInitted())
	{
		g_vtVRShellRight.Init(g_pLTClient, "VRShellRight", NULL, 3.0f);
		g_vtVRShellUp.Init(g_pLTClient, "VRShellUp", NULL, 1.5f);
	}

	if (!g_vtWeaponFXMaxFireDist.IsInitted())
	{
		g_vtWeaponFXMaxFireDist.Init(g_pLTClient, "WeaponFXMaxFireDist", NULL, 1000.0f);
	}

	if (!g_vtWeaponFXMaxImpactDist.IsInitted())
	{
		g_vtWeaponFXMaxImpactDist.Init(g_pLTClient, "WeaponFXMaxImpactDist", NULL, 1000.0f);
	}

	if (!g_vtWeaponFXMaxMultiImpactDist.IsInitted())
	{
		g_vtWeaponFXMaxMultiImpactDist.Init(g_pLTClient, "WeaponFXMaxMultiImpactDist", NULL, 300.0f);
	}

	if (!g_vtMultiDing.IsInitted())
	{
		g_vtMultiDing.Init(g_pLTClient, "WeaponFXMultiImpactDing", NULL, 1.0f);
	}

    return LTTRUE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::CreateObject
//
//	PURPOSE:	Create the various fx
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponFX::CreateObject(ILTClient* pClientDE)
{
    if (!CSpecialFX::CreateObject(pClientDE) || !g_pWeaponMgr) return LTFALSE;

	CGameSettings* pSettings = g_pInterfaceMgr->GetSettings();
    if (!pSettings) return LTFALSE;

	// Set up our data members...

	// Set the local client id...

    uint32 dwId;
    g_pLTClient->GetLocalClientID(&dwId);
    m_nLocalId = (uint8)dwId;


	m_nDetailLevel = pSettings->SpecialFXSetting();

	// Fire pos may get tweaked a little...

	m_vFirePos = CalcFirePos(m_vFirePos);

	m_vDir = m_vPos - m_vFirePos;
	m_fFireDistance = m_vDir.Mag();
	m_vDir.Norm();

    g_pLTClient->AlignRotation(&m_rSurfaceRot, &m_vSurfaceNormal, LTNULL);
    g_pLTClient->AlignRotation(&m_rDirRot, &m_vDir, LTNULL);

	SetupExitInfo();



	// Calculate if the camera can see the fire position and the impact
	// position...

	g_bCanSeeImpactPos	= LTTRUE;
	g_bCanSeeFirePos	= LTTRUE;
	g_bDistantImpactPos	= LTFALSE;
	g_bDistantFirePos	= LTFALSE;

	// NOT IN VR. Both of these cull the muzzle flash, the shell casing and the
	// muzzle light by measuring the MUZZLE against the CAMERA - a 2000-era
	// performance saving that retail could make safely because the gun was
	// welded a few units in front of the eye, so it was always within
	// MinFireDot's 53 degrees and always inside MaxFireDist.
	//
	// With the gun in a tracked hand neither holds. Measured at the desk on
	// 19 September: one weapon's muzzle came out at dot 0.56 against the 0.60
	// minimum and the engine silently skipped every flash and every casing -
	// and holding the gun out to the side, or looking away from it, does the
	// same thing on purpose. The saving is worth nothing on the machine and the
	// failure is invisible, which is the worst trade in the file.
	//
	// The cvar still works for anyone who wants the retail behaviour back.
	if (g_vtWeaponFXUseFOVPerformance.GetFloat() && !VRPrims_RebaseEverKnown())
	{
		HOBJECT hCamera = g_pGameClientShell->GetCamera();
		LTVector vCameraPos, vU, vR, vF, vDir;
		LTRotation rCameraRot;
		g_pLTClient->GetObjectPos(hCamera, &vCameraPos);
		g_pLTClient->GetObjectRotation(hCamera, &rCameraRot);
		g_pLTClient->GetRotationVectors(&rCameraRot, &vU, &vR, &vF);

		vDir = m_vPos - vCameraPos;
		LTFLOAT fImpactDist = vDir.Mag();

		if (fImpactDist > g_vtWeaponFXMaxImpactDist.GetFloat())
		{
			g_bDistantImpactPos = LTTRUE;
		}

		vDir.Norm();

		LTFLOAT fMul = VEC_DOT(vDir, vF);
		g_bCanSeeImpactPos = (fMul < g_vtWeaponFXMinImpactDot.GetFloat() ? LTFALSE : LTTRUE);

		// In multiplayer we need to account for impacts that occur around
		// our camera that we didn't cause (this is also an issue in single
		// player, but due to the singler player gameplay dynamics it isn't
		// as noticeable)...

		if (!g_bCanSeeImpactPos && IsMultiplayerGame())
		{
			// Somebody else shot this...if the impact is close enough, we 
			// "saw" it...
			if (m_nLocalId != m_nShooterId && fImpactDist <= g_vtWeaponFXMaxMultiImpactDist.GetFloat())
			{
				g_bCanSeeImpactPos = LTTRUE;
			}
		}

		vDir = m_vFirePos - vCameraPos;

		if (vDir.Mag() > g_vtWeaponFXMaxFireDist.GetFloat())
		{
			g_bDistantFirePos = LTTRUE;
		}

		vDir.Norm();

		fMul = VEC_DOT(vDir, vF);
		g_bCanSeeFirePos = (fMul < g_vtWeaponFXMinFireDot.GetFloat() ? LTFALSE : LTTRUE);

		// WHY A SHOT CAN PRODUCE NO FLASH, NO CASING AND NO LIGHT, and why that is
		// a VR problem rather than a broken effect.
		//
		// Both gates measure the MUZZLE against the CAMERA, and retail could take
		// that for granted: in flat NOLF the gun is welded a few units in front of
		// the eye, so the muzzle is always within 53 degrees of where you are
		// looking (MinFireDot 0.6) and always within 1000 units (MaxFireDist).
		// With the gun in a tracked HAND neither is guaranteed - look away from
		// your own weapon and CanSeeFirePos goes false, which silently skips
		// CreateMuzzleFX, CreateShell and CreateMuzzleLight further down.
		//
		// casings from about 8 feet away and no trail.
		// This line is what says whether the gate is the cause; it is printed for
		// EVERY shot, inside no branch, because the interesting case is the one
		// where nothing happens and nothing is logged.
		{
			static int s_nSaidGate = 0;
			if (s_nSaidGate < 12)
			{
				++s_nSaidGate;
				const LTVector vToMuzzle = m_vFirePos - vCameraPos;
				VRLog::Msg("VRFireGate: muzzle %.0f %.0f %.0f  camera %.0f %.0f %.0f"
					"  dist %.0f (max %.0f)  dot %.2f (min %.2f)"
					"  -> canSeeFire %s, distantFire %s%s",
					m_vFirePos.x, m_vFirePos.y, m_vFirePos.z,
					vCameraPos.x, vCameraPos.y, vCameraPos.z,
					vToMuzzle.Mag(), g_vtWeaponFXMaxFireDist.GetFloat(),
					fMul, g_vtWeaponFXMinFireDot.GetFloat(),
					g_bCanSeeFirePos ? "YES" : "NO",
					g_bDistantFirePos ? "YES" : "no",
					(!g_bCanSeeFirePos || g_bDistantFirePos)
						? "   <- THIS SHOT DREW NO FLASH AND NO CASING" : "");
			}
		}
	}



	// Determine what container the sfx is in...

	HLOCALOBJ objList[1];
    LTVector vTestPos = m_vPos + m_vSurfaceNormal;  // Test a little closer...
    uint32 dwNum = g_pLTClient->GetPointContainers(&vTestPos, objList, 1);

	if (dwNum > 0 && objList[0])
	{
        uint32 dwUserFlags;
        g_pLTClient->GetObjectUserFlags(objList[0], &dwUserFlags);

		if (dwUserFlags & USRFLG_VISIBLE)
		{
            uint16 dwCode;
            if (g_pLTClient->GetContainerCode(objList[0], &dwCode))
			{
				m_eCode = (ContainerCode)dwCode;
			}
		}
	}

	// Determine if the fire point is in liquid

	vTestPos = m_vFirePos + m_vDir;  // Test a little further in...
    dwNum = g_pLTClient->GetPointContainers(&vTestPos, objList, 1);

	if (dwNum > 0 && objList[0])
	{
        uint32 dwUserFlags;
        g_pLTClient->GetObjectUserFlags(objList[0], &dwUserFlags);

		if (dwUserFlags & USRFLG_VISIBLE)
		{
            uint16 dwCode;
            if (g_pLTClient->GetContainerCode(objList[0], &dwCode))
			{
				m_eFirePosCode = (ContainerCode)dwCode;
			}
		}
	}


	if (IsLiquid(m_eCode))
	{
		m_wImpactFX	= m_pAmmo->pUWImpactFX ? m_pAmmo->pUWImpactFX->nFlags : 0;
	}
	else
	{
		m_wImpactFX	= m_pAmmo->pImpactFX ? m_pAmmo->pImpactFX->nFlags : 0;
	}

	m_wFireFX = m_pAmmo->pFireFX ? m_pAmmo->pFireFX->nFlags : 0;

	// Assume alt-fire, silenced, and tracer...these will be cleared by
	// IgnoreFX if not used...

	m_wFireFX |= WFX_ALTFIRESND | WFX_SILENCED | WFX_TRACER;

	// Assume impact ding, it will be cleared if not used...

	m_wImpactFX |= WFX_IMPACTDING;

	// Clear all the fire fx we want to ignore...

	m_wFireFX &= ~m_wIgnoreFX;
	m_wImpactFX &= ~m_wIgnoreFX;


	// See if this is a redundant weapon fx (i.e., this client shot the
	// weapon so they've already seen this fx)...

	if (g_pGameClientShell->IsMultiplayerGame())
	{
		if (m_pAmmo->eType != PROJECTILE)
		{
			if (!m_bLocal && m_nLocalId >= 0 && m_nLocalId == m_nShooterId)
			{
				if (m_wImpactFX & WFX_IMPACTDING)
				{
					if (g_vtMultiDing.GetFloat())
					{
						PlayImpactDing();
					}
				}

                return LTFALSE;
			}
		}
	}


	// Show the fire path...(debugging...)

	if (g_cvarShowFirePath.GetFloat() > 0)
	{
		PLFXCREATESTRUCT pls;

		pls.vStartPos			= m_vFirePos;
		pls.vEndPos				= m_vPos;
        pls.vInnerColorStart    = LTVector(GetRandom(127.0f, 255.0f), GetRandom(127.0f, 255.0f), GetRandom(127.0f, 255.0f));
		pls.vInnerColorEnd		= pls.vInnerColorStart;
        pls.vOuterColorStart    = LTVector(0, 0, 0);
        pls.vOuterColorEnd      = LTVector(0, 0, 0);
		pls.fAlphaStart			= 1.0f;
		pls.fAlphaEnd			= 1.0f;
		pls.fMinWidth			= 0;
		pls.fMaxWidth			= 10;
		pls.fMinDistMult		= 1.0f;
		pls.fMaxDistMult		= 1.0f;
		pls.fLifeTime			= 10.0f;
		pls.fAlphaLifeTime		= 10.0f;
		pls.fPerturb			= 0.0f;
        pls.bAdditive           = LTFALSE;
		pls.nWidthStyle			= PLWS_CONSTANT;
		pls.nNumSegments		= 2;

		CSpecialFX* pFX = g_pGameClientShell->GetSFXMgr()->CreateSFX(SFX_POLYLINE_ID, &pls);
		if (pFX) pFX->Update();
	}


	// If the surface is the sky, don't create any impact related fx...

	if (m_eSurfaceType != ST_SKY || (m_wImpactFX & WFX_IMPACTONSKY))
	{
		CreateWeaponSpecificFX();

		if (g_bCanSeeImpactPos)
		{
			// WHICH OF THE FOUR CONDITIONS A MARK FAILED.
			//
			// The renderer can only see the outcome - an impact texture that
			// never draws is equally consistent with the mark not being
			// created, being created invisible, or never reaching the publish.
			//
			// KEPT BECAUSE IT ALREADY CORRECTED ONE WRONG ANSWER. The first
			// reading was "impactFX 0000, surface 2, showsMark 0", which looked
			// like a broken mark - until SurfaceDefs.h said surface 2 is
			// ST_FLESH. The test had been shooting PEOPLE, and flesh takes no
			// bullet hole. Fired at the ground the same probe reads "impactFX
			// 0001, surface 11, showsMark 1" and BHOLSTN1.DTX draws. Marks
			// were never broken; the surface under the crosshair was.
			{
				static int s_nSaidMark = 0;
				if (s_nSaidMark < 6)
				{
					++s_nSaidMark;
					VRLog::Msg("VRMark: impactFX %04X (WFX_MARK %d),"
						" surface %d showsMark %d, MarkShow %d, detail %d",
						m_wImpactFX, (m_wImpactFX & WFX_MARK) ? 1 : 0,
						(int)m_eSurfaceType, ShowsMark(m_eSurfaceType) ? 1 : 0,
						(int)GetConsoleInt("MarkShow", 1), (int)m_nDetailLevel);
				}
			}
			if ((m_wImpactFX & WFX_MARK) && ShowsMark(m_eSurfaceType) && (LTBOOL)GetConsoleInt("MarkShow", 1))
			{
				LTBOOL bCreateMark = LTTRUE;
				if (g_bDistantImpactPos && m_nLocalId == m_nShooterId)
				{
					// Assume we'll see the mark if we're zoomed in ;)
					bCreateMark = g_pGameClientShell->IsZoomed();
				}

				if (bCreateMark)
				{
					CreateMark(m_vPos, m_vSurfaceNormal, m_rSurfaceRot, m_eSurfaceType);
				}
			}

			CreateSurfaceSpecificFX();
		}

		PlayImpactSound();
	}


	if (IsBulletTrailWeapon())
	{
		if (IsLiquid(m_eFirePosCode))
		{
			if (m_nDetailLevel != RS_LOW)
			{
				// From the barrel too - see VRMuzzleOrFirePos.
				LTVector vTrailFrom = VRMuzzleOrFirePos();
				CreateBulletTrail(&vTrailFrom);
			}
		}
	}


	// No tracers under water...

	if ((LTBOOL)GetConsoleInt("Tracers", 1) && (m_wFireFX & WFX_TRACER) && !IsLiquid(m_eCode))
	{
		CreateTracer();
	}

	if (g_bCanSeeFirePos)
	{
		// Only do muzzle fx for the client (not for AIs)...

		if ((m_wFireFX & WFX_MUZZLE) && (m_nLocalId == m_nShooterId))
		{
			CreateMuzzleFX();
		}

		if (!g_bDistantFirePos &&
			(LTBOOL)GetConsoleInt("ShellCasings", 1) &&
			(m_wFireFX & WFX_SHELL))
		{
			CreateShell();
		}

		if ((m_wFireFX & WFX_LIGHT))
		{
			CreateMuzzleLight();
		}
	}

	// EVERY EFFECT THIS SHOT CREATES, AND WHERE. No more guessing.
	//
	// Three diagnostics in a row printed NOTHING in a headset log -
	// VRFlashPos (the player-view flash objects), then VRWorldFlash (the world
	// muzzle flash). The player can plainly see a muzzle flash way off to the right
	// about twenty feet away, so it is a fourth thing, and each round of
	// reasoning about which has cost a headset test.
	//
	// So stop reasoning. This prints the flags that decide every branch and the
	// position of every effect the shot produces, against the camera, so the
	// one that is twenty feet out can be identified by its distance instead of
	// by argument.
	{
		static int s_nSaidShot = 0;
		if (s_nSaidShot < 8 && m_nLocalId == m_nShooterId)
		{
			++s_nSaidShot;
			LTVector vCam(0.0f, 0.0f, 0.0f);
			HOBJECT hCam = g_pGameClientShell->GetCamera();
			if (hCam) g_pLTClient->GetObjectPos(hCam, &vCam);
			LTVector vMuz = VRMuzzleOrFirePos();
			const LTVector vHandFromCam = m_vFirePos - vCam;
			const LTVector vMuzFromCam  = vMuz - vCam;

			VRLog::Msg("VRShotFX: fx flags%s%s%s%s%s | canSeeFire %s distantFire %s"
				" | first person %s | hand %.0f %.0f %.0f (%.0f from the camera,"
				" %.1f ft) | muzzle %.0f %.0f %.0f (%.0f from the camera, %.1f ft)",
				(m_wFireFX & WFX_MUZZLE)   ? " MUZZLE"   : "",
				(m_wFireFX & WFX_SHELL)    ? " SHELL"    : "",
				(m_wFireFX & WFX_LIGHT)    ? " LIGHT"    : "",
				(m_wFireFX & WFX_TRACER)   ? " TRACER"   : "",
				(m_wFireFX & WFX_SILENCED) ? " SILENCED" : "",
				g_bCanSeeFirePos ? "yes" : "NO",
				g_bDistantFirePos ? "YES" : "no",
				g_pGameClientShell->IsFirstPerson() ? "yes" : "NO",
				m_vFirePos.x, m_vFirePos.y, m_vFirePos.z,
				vHandFromCam.Mag(), vHandFromCam.Mag() / 18.0f,
				vMuz.x, vMuz.y, vMuz.z,
				vMuzFromCam.Mag(), vMuzFromCam.Mag() / 18.0f);
		}
	}

	if ((m_wFireFX & WFX_FIRESOUND) || (m_wFireFX & WFX_ALTFIRESND) || (m_wFireFX & WFX_SILENCED))
	{
		PlayFireSound();
	}

	// Only do fly-by sounds for weapons that leave bullet trails...

	if (IsBulletTrailWeapon())
	{
		PlayBulletFlyBySound();
	}


    return LTFALSE;  // Just delete me, I'm done :)
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::SetupExitInfo
//
//	PURPOSE:	Setup our exit info
//
// ----------------------------------------------------------------------- //

void CWeaponFX::SetupExitInfo()
{
	m_eExitSurface	= ST_UNKNOWN;
	m_vExitPos		= m_vFirePos;
	m_vExitNormal	= m_vDir;
	m_eExitCode		= CC_NO_CONTAINER;

	if (m_nDetailLevel == RS_LOW) return;

	// Determine if there is an "exit" surface...

	IntersectQuery qInfo;
	IntersectInfo iInfo;

	qInfo.m_From = m_vFirePos + m_vDir;
	qInfo.m_To   = m_vFirePos - m_vDir;

	qInfo.m_Flags = INTERSECT_OBJECTS | IGNORE_NONSOLID | INTERSECT_HPOLY;

    if (g_pLTClient->IntersectSegment(&qInfo, &iInfo))
	{
		m_eExitSurface	= GetSurfaceType(iInfo);
		m_vExitNormal	= iInfo.m_Plane.m_Normal;
		m_vExitPos		= iInfo.m_Point + m_vDir;

		// Determine what container the sfx is in...

		HLOCALOBJ objList[1];
        LTVector vTestPos = m_vExitPos + m_vExitNormal;  // Test a little closer...
        uint32 dwNum = g_pLTClient->GetPointContainers(&vTestPos, objList, 1);

		if (dwNum > 0 && objList[0])
		{
            uint32 dwUserFlags;
            g_pLTClient->GetObjectUserFlags(objList[0], &dwUserFlags);

			if (dwUserFlags & USRFLG_VISIBLE)
			{
                uint16 dwCode;
                if (g_pLTClient->GetContainerCode(objList[0], &dwCode))
				{
					m_eExitCode = (ContainerCode)dwCode;
				}
			}
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::CreateExitDebris
//
//	PURPOSE:	Create any exit debris
//
// ----------------------------------------------------------------------- //

void CWeaponFX::CreateExitDebris()
{
	int i;

	// Create the surface specific exit fx...

	SURFACE* pSurf = g_pSurfaceMgr->GetSurface(m_eExitSurface);
	if (!pSurf || !pSurf->bCanShootThrough) return;

	if (IsLiquid(m_eExitCode))
	{
		// Create underwater fx...

		// Create any exit particle shower fx associated with this surface...

		for (i=0; i < pSurf->nNumUWExitPShowerFX; i++)
		{
			CPShowerFX* pPShowerFX = g_pSurfaceMgr->GetPShowerFX(pSurf->aUWExitPShowerFXIds[i]);
			g_pFXButeMgr->CreatePShowerFX(pPShowerFX, m_vExitPos, m_vExitNormal, m_vSurfaceNormal);
		}
	}
	else
	{
		// Create normal fx...

		// Create any exit scale fx associated with this surface...

		for (i=0; i < pSurf->nNumExitScaleFX; i++)
		{
			CScaleFX* pScaleFX = g_pSurfaceMgr->GetScaleFX(pSurf->aExitScaleFXIds[i]);
			g_pFXButeMgr->CreateScaleFX(pScaleFX, m_vExitPos, m_vExitNormal, &m_vExitNormal, &m_rSurfaceRot);
		}

		// Create any exit particle shower fx associated with this surface...

		for (i=0; i < pSurf->nNumExitPShowerFX; i++)
		{
			CPShowerFX* pPShowerFX = g_pSurfaceMgr->GetPShowerFX(pSurf->aExitPShowerFXIds[i]);
			g_pFXButeMgr->CreatePShowerFX(pPShowerFX, m_vExitPos, m_vExitNormal, m_vSurfaceNormal);
		}

		// Create any exit poly debris fx associated with this surface...

		for (i=0; i < pSurf->nNumExitPolyDebrisFX; i++)
		{
			CPolyDebrisFX* pPolyDebrisFX = g_pSurfaceMgr->GetPolyDebrisFX(pSurf->aExitPolyDebrisFXIds[i]);
			g_pFXButeMgr->CreatePolyDebrisFX(pPolyDebrisFX, m_vExitPos, m_vExitNormal, m_vExitNormal);
		}
	}


	// Determine if we should create a beam of light through the surface...

	CreateLightBeamFX(pSurf);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::CreateLightBeamFX
//
//	PURPOSE:	Create a light beam (if appropriate)
//
// ----------------------------------------------------------------------- //

void CWeaponFX::CreateLightBeamFX(SURFACE* pSurf)
{
	if (!pSurf) return;

    LTVector vEnterColor, vExitColor;
    if (g_pLTClient->GetPointShade(&m_vExitPos, &vExitColor) == LT_OK)
	{
		// Get the EnterColor value...

        LTVector vEnterPos = m_vExitPos - (m_vExitNormal * float(pSurf->nMaxShootThroughThickness + 1));

        if (g_pLTClient->GetPointShade(&vEnterPos, &vEnterColor) == LT_OK)
		{
			// Calculate the difference in light value...

            LTFLOAT fMaxEnter = Max(vEnterColor.x, vEnterColor.y);
			fMaxEnter = Max(fMaxEnter, vEnterColor.z);

            LTFLOAT fMaxExit = Max(vExitColor.x, vExitColor.y);
			fMaxExit = Max(fMaxExit, vExitColor.z);

			if (fabs((double)(fMaxExit - fMaxEnter)) >= g_cvarLightBeamColorDelta.GetFloat())
			{
                LTVector vStartPoint, vDir;
				if (fMaxEnter > fMaxExit)
				{
					vStartPoint = m_vExitPos;
					vDir = m_vDir;
				}
				else
				{
					vStartPoint = vEnterPos;
					vDir = -m_vDir;
				}

				PLFXCREATESTRUCT pls;

				pls.vStartPos			= vStartPoint;
				pls.vEndPos				= vStartPoint + (vDir * GetRandom(100.0, 150.0f));
                pls.vInnerColorStart    = LTVector(230, 230, 230);
				pls.vInnerColorEnd		= pls.vInnerColorStart;
                pls.vOuterColorStart    = LTVector(0, 0, 0);
                pls.vOuterColorEnd      = LTVector(0, 0, 0);
				pls.fAlphaStart			= 0.5f;
				pls.fAlphaEnd			= 0.0f;
				pls.fMinWidth			= 0;
				pls.fMaxWidth			= 10;
				pls.fMinDistMult		= 1.0f;
				pls.fMaxDistMult		= 1.0f;
				pls.fLifeTime			= 10.0f;
				pls.fAlphaLifeTime		= 10.0f;
				pls.fPerturb			= 0.0f;
                pls.bAdditive           = LTFALSE;
				pls.nWidthStyle			= PLWS_CONSTANT;
				pls.nNumSegments		= 1;

				CSpecialFX* pFX = g_pGameClientShell->GetSFXMgr()->CreateSFX(SFX_POLYLINE_ID, &pls);
				if (pFX) pFX->Update();
			}
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::CreateExitMark
//
//	PURPOSE:	Create any exit surface marks
//
// ----------------------------------------------------------------------- //

void CWeaponFX::CreateExitMark()
{
	if (m_eExitSurface != ST_UNKNOWN && ShowsMark(m_eExitSurface))
	{
        LTRotation rNormRot;
        g_pLTClient->AlignRotation(&rNormRot, &m_vExitNormal, LTNULL);

		CreateMark(m_vExitPos, m_vExitNormal, rNormRot, m_eExitSurface);
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::CreateMark
//
//	PURPOSE:	Create a mark fx
//
// ----------------------------------------------------------------------- //

void CWeaponFX::CreateMark(LTVector vPos, LTVector vNorm, LTRotation rRot,
						   SurfaceType eType)
{
	IMPACTFX* pImpactFX = m_pAmmo->pImpactFX;

	if (IsLiquid(m_eCode))
	{
		pImpactFX = m_pAmmo->pUWImpactFX;
	}

	if (!pImpactFX) return;

	CSFXMgr* psfxMgr = g_pGameClientShell->GetSFXMgr();
	if (!psfxMgr) return;

	MARKCREATESTRUCT mark;

	mark.m_vPos = vPos;
	mark.m_Rotation = rRot;

	// Randomly rotate the bullet hole...

    g_pLTClient->RotateAroundAxis(&mark.m_Rotation, &vNorm, GetRandom(0.0f, MATH_CIRCLE));

	mark.m_fScale		= pImpactFX->fMarkScale;
	mark.nAmmoId		= m_nAmmoId;
	mark.nSurfaceType   = eType;

	psfxMgr->CreateSFX(SFX_MARK_ID, &mark);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::CreateBulletTrail
//
//	PURPOSE:	Create a bullet trail fx
//
// ----------------------------------------------------------------------- //

void CWeaponFX::CreateBulletTrail(LTVector *pvStartPos)
{
	CSFXMgr* psfxMgr = g_pGameClientShell->GetSFXMgr();
	if (!psfxMgr || !pvStartPos) return;

    LTVector vColor1, vColor2;
	vColor1.Init(200.0f, 200.0f, 200.0f);
	vColor2.Init(255.0f, 255.0f, 255.0f);

	BTCREATESTRUCT bt;

	bt.vStartPos		= *pvStartPos;
	bt.vDir				= m_vDir;
	bt.vColor1			= vColor1;
	bt.vColor2			= vColor2;
	bt.fLifeTime		= 0.5f;
	bt.fFadeTime		= 0.3f;
	bt.fRadius			= 400.0f;
	bt.fGravity			= 0.0f;
	bt.fNumParticles	= (m_nDetailLevel == RS_MED) ? 15.0f : 30.0f;

	CSpecialFX* pFX = psfxMgr->CreateSFX(SFX_BULLETTRAIL_ID, &bt);

	// Let each bullet trail do its initial update...

	if (pFX) pFX->Update();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::CreateTracer
//
//	PURPOSE:	Create a tracer fx
//
// ----------------------------------------------------------------------- //

void CWeaponFX::CreateTracer()
{
	if (!m_pAmmo || !m_pAmmo->pTracerFX) return;

	CSFXMgr* psfxMgr = g_pGameClientShell->GetSFXMgr();
	if (!psfxMgr) return;

	if (m_nDetailLevel != RS_HIGH && GetRandom(1, 2) == 1) return;

	// Create tracer...

	if (m_fFireDistance > 100.0f)
	{
		TRCREATESTRUCT tracer;

		// Make tracer start in front of gun a little ways...

		// FROM THE BARREL, NOT THE HAND. See VRMuzzleOrFirePos - m_vFirePos is
		// the grip, and a tracer drawn from it starts behind and below the gun,
		// which is what headset testing has now reported twice.
		{
			CWeaponModel* pWMt = g_pGameClientShell->GetWeaponModel();
			const LTVector vT = pWMt ? pWMt->VRTracerOffset() : LTVector(0.0f,0.0f,0.0f);
			tracer.vStartPos = VRMuzzleOrFirePos() + VRPerEffectOffset(vT, m_vDir);
		}
		VRNodeProbe();

		// AND SAY HOW FAR IT MOVED, so the desk can tell a working fix from a
		// silent fallback. If this prints 0 units the muzzle was refused and
		// the tracer is still leaving the hand.
		{
			static int s_nSaidTracer = 0;
			if (s_nSaidTracer < 6)
			{
				++s_nSaidTracer;
				const LTVector vMoved = tracer.vStartPos - m_vFirePos;
				VRLog::Msg("VRTracer: starts %.0f %.0f %.0f, the hand was"
					" %.0f %.0f %.0f - moved %.0f units forward (%.2f m)",
					tracer.vStartPos.x, tracer.vStartPos.y, tracer.vStartPos.z,
					m_vFirePos.x, m_vFirePos.y, m_vFirePos.z,
					vMoved.Mag(), vMoved.Mag() * 0.01692f);

				// AND AGAINST THE GUN YOU CAN SEE, decomposed so the answer is
				// a direction and not just a distance.
				//
				// the tracers start
				// around 6-12 inches from the gun, on the right hand side.
				// Six to twelve inches is 9 to 18 units, and "to the right" is
				// the part that matters - the start is anchored to m_vFirePos,
				// the computed hand, and if that sits right of the DRAWN gun
				// then so does every tracer, at every range.
				//
				// VRPrims_DrawnGunCentre is a point on the drawn weapon - it is
				// what the shell casings leave from, and the player has not reported
				// those as displaced since they were moved onto it. So it is the
				// reference the tracer should be measured against, and probably
				// the one it should be anchored to.
				//
				// RIGHT and UP are built from the shot's own direction rather
				// than the camera's, so "right" means right of the barrel.
				LTVector vGunCentre;
				if (VRPrims_DrawnGunCentre(vGunCentre))
				{
					// DECOMPOSED IN THE GUN'S FRAME, not the world's.
					//
					// A world-up basis cannot tell "welded to the gun" from
					// "rotating against it" under ROLL, because it does not roll
					// either - both look identical. That is exactly why the
					// yaw-only sweep reported the tracer healthy while its
					// muzzle offset was in the wrong frame. In the gun's own
					// frame a welded offset reads the SAME numbers at every
					// roll, which is a test that can fail.
					LTVector vRight, vUp;
					CWeaponModel* pWMd = g_pGameClientShell->GetWeaponModel();
					bool bGunFrame = false;
					if (pWMd)
					{
						LTRotation rG = pWMd->VRGunRot();
						LTVector vGU, vGR, vGF;
						g_pLTClient->GetRotationVectors(&rG, &vGU, &vGR, &vGF);
						if (vGF.Mag() > 0.001f) { vRight = vGR; vUp = vGU; bGunFrame = true; }
					}
					LTVector vWorldUp(0.0f, 1.0f, 0.0f);
					if (!bGunFrame) vRight = vWorldUp.Cross(m_vDir);
					if (vRight.Mag() > 0.001f)
					{
						vRight.Norm();
						if (!bGunFrame) vUp = m_vDir.Cross(vRight);
						vUp.Norm();

						const LTVector vD = tracer.vStartPos - vGunCentre;
						const float fR = vD.Dot(vRight);
						const float fU = vD.Dot(vUp);
						const float fF = vD.Dot(m_vDir);
						VRLog::Msg("VRTracerVsGun: the drawn gun is at %.0f %.0f %.0f;"
							" the tracer starts right %+.1f, up %+.1f, forward %+.1f"
							" units of it (%.1f, %.1f, %.1f inches)",
							vGunCentre.x, vGunCentre.y, vGunCentre.z,
							fR, fU, fF,
							fR * 0.666f, fU * 0.666f, fF * 0.666f);
					}
				}
				else
				{
					VRLog::Msg("VRTracerVsGun: no drawn gun centre this frame");
				}
			}
		}

		tracer.vEndPos		= m_vPos;
		tracer.pTracerFX	= m_pAmmo->pTracerFX;

		CSpecialFX* pFX = psfxMgr->CreateSFX(SFX_TRACER_ID, &tracer);
		if (pFX) pFX->Update();
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::CreateWeaponSpecificFX()
//
//	PURPOSE:	Create weapon specific fx
//
// ----------------------------------------------------------------------- //

void CWeaponFX::CreateWeaponSpecificFX()
{
	// Do fire fx beam fx...

	if (m_pAmmo->pFireFX && m_pAmmo->pFireFX->nNumBeamFX > 0)
	{
		for (int i=0; i < m_pAmmo->pFireFX->nNumBeamFX; i++)
		{
			g_pFXButeMgr->CreateBeamFX(m_pAmmo->pFireFX->pBeamFX[i],
				m_vFirePos, m_vPos);
		}
	}

	// Only do impact fx if the client can see the impact position
	// or the impact fx may last a little while...

	if (g_bCanSeeImpactPos || m_pAmmo->fProgDamage > 0.0f || m_pAmmo->nAreaDamage > 0)
	{
		if (IsLiquid(m_eCode))
		{
			// Create underwater weapon fx...

			IFXCS cs;
			cs.eCode		= m_eCode;
			cs.eSurfType	= m_eSurfaceType;
			cs.rSurfRot		= m_rSurfaceRot;
			cs.vDir			= m_vDir;
			cs.vPos			= m_vPos;
			cs.vSurfNormal	= m_vSurfaceNormal;
			cs.fBlastRadius = (LTFLOAT) m_pAmmo->nAreaDamageRadius;
			cs.fTintRange   = (LTFLOAT) (m_pAmmo->nAreaDamageRadius * 5);

			g_pFXButeMgr->CreateImpactFX(m_pAmmo->pUWImpactFX, cs);
		}
		else
		{
			IFXCS cs;
			cs.eCode		= m_eCode;
			cs.eSurfType	= m_eSurfaceType;
			cs.rSurfRot		= m_rSurfaceRot;
			cs.vDir			= m_vDir;
			cs.vPos			= m_vPos;
			cs.vSurfNormal	= m_vSurfaceNormal;
			cs.fBlastRadius = (LTFLOAT) m_pAmmo->nAreaDamageRadius;
			cs.fTintRange   = (LTFLOAT) (m_pAmmo->nAreaDamageRadius * 5);

			g_pFXButeMgr->CreateImpactFX(m_pAmmo->pImpactFX, cs);
		}
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::CreateSurfaceSpecificFX()
//
//	PURPOSE:	Create surface specific fx
//
// ----------------------------------------------------------------------- //

void CWeaponFX::CreateSurfaceSpecificFX()
{
	CGameSettings* pSettings = g_pInterfaceMgr->GetSettings();
	if (!pSettings) return;

	// Don't do gore fx...

	if (m_eSurfaceType == ST_FLESH)
	{
		if (!pSettings->Gore())
		{
			return;
		}

		if (m_pAmmo->eType == VECTOR && m_pAmmo->eInstDamageType == DT_BULLET)
		{
			CreateBloodSplatFX();
		}
	}

	if ((m_wFireFX & WFX_EXITMARK) && ShowsMark(m_eExitSurface))
	{
		CreateExitMark();
	}

	if (m_wFireFX & WFX_EXITDEBRIS)
	{
		CreateExitDebris();
	}

	if (!m_pAmmo || !m_pAmmo->pImpactFX) return;
	if (!m_pAmmo->pImpactFX->bDoSurfaceFX) return;


	// Create the surface specific fx...

	int i;
	SURFACE* pSurf = g_pSurfaceMgr->GetSurface(m_eSurfaceType);
	if (pSurf)
	{
		if (IsLiquid(m_eCode))
		{
			// Create underwater fx...

			// Create any impact particle shower fx associated with this surface...

			for (i=0; i < pSurf->nNumUWImpactPShowerFX; i++)
			{
				CPShowerFX* pPShowerFX = g_pSurfaceMgr->GetPShowerFX(pSurf->aUWImpactPShowerFXIds[i]);
				g_pFXButeMgr->CreatePShowerFX(pPShowerFX, m_vPos, m_vDir, m_vSurfaceNormal);
			}
		}
		else
		{
			// Create normal fx...

			// Create any impact scale fx associated with this surface...

			for (i=0; i < pSurf->nNumImpactScaleFX; i++)
			{
				CScaleFX* pScaleFX = g_pSurfaceMgr->GetScaleFX(pSurf->aImpactScaleFXIds[i]);
				g_pFXButeMgr->CreateScaleFX(pScaleFX, m_vPos, m_vDir, &m_vSurfaceNormal, &m_rSurfaceRot);
			}

			// Create any impact particle shower fx associated with this surface...

			for (i=0; i < pSurf->nNumImpactPShowerFX; i++)
			{
				CPShowerFX* pPShowerFX = g_pSurfaceMgr->GetPShowerFX(pSurf->aImpactPShowerFXIds[i]);
				g_pFXButeMgr->CreatePShowerFX(pPShowerFX, m_vPos, m_vDir, m_vSurfaceNormal);
			}

			// Create any impact poly debris fx associated with this surface...

			if (g_vtCreatePolyDebris.GetFloat())
			{
				for (i=0; i < pSurf->nNumImpactPolyDebrisFX; i++)
				{
					CPolyDebrisFX* pPolyDebrisFX = g_pSurfaceMgr->GetPolyDebrisFX(pSurf->aImpactPolyDebrisFXIds[i]);
					g_pFXButeMgr->CreatePolyDebrisFX(pPolyDebrisFX, m_vPos, m_vDir, m_vSurfaceNormal);
				}
			}
		}
	}
}




// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::CreateMuzzleLight()
//
//	PURPOSE:	Create a muzzle light associated with this fx
//
// ----------------------------------------------------------------------- //

void CWeaponFX::CreateMuzzleLight()
{
	// Check to see if we have the silencer...

	if (m_wFireFX & WFX_SILENCED) return;

	if (m_nLocalId != m_nShooterId || !g_pGameClientShell->IsFirstPerson())
	{
		MUZZLEFLASHCREATESTRUCT mf;

		mf.pWeapon	= m_pWeapon;
		mf.hParent	= m_hFiredFrom;
		// THE MUZZLE, like the tracer. This is the WORLD muzzle flash, and on
		// these weapons it is the only one there is: the headset log reports
		// "bute NO" - VRHasPVMuzzleFX false - so the player-view flash does not
		// exist at all, and "PV flash shown 0 times". Every hour spent on the
		// player-view flash object was spent on something that is never drawn.
		mf.vPos		= VRMuzzleOrFirePos();
		mf.rRot		= m_rDirRot;

		{
			static int s_nSaidWorldFlash = 0;
			if (s_nSaidWorldFlash < 6)
			{
				++s_nSaidWorldFlash;
				const LTVector vOff = mf.vPos - m_vFirePos;
				VRLog::Msg("VRWorldFlash: created at %.0f %.0f %.0f, %.0f units"
					" from the hand - first person %s",
					mf.vPos.x, mf.vPos.y, mf.vPos.z, vOff.Mag(),
					g_pGameClientShell->IsFirstPerson() ? "yes" : "NO");
			}
		}

		CSFXMgr* psfxMgr = g_pGameClientShell->GetSFXMgr();
		if (!psfxMgr) return;

		psfxMgr->CreateSFX(SFX_MUZZLEFLASH_ID, &mf);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::IsBulletTrailWeapon()
//
//	PURPOSE:	See if this weapon creates bullet trails in liquid
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponFX::IsBulletTrailWeapon()
{
	return (m_pAmmo->eInstDamageType == DT_BULLET);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::PlayImpactSound()
//
//	PURPOSE:	Play a surface impact sound if appropriate
//
// ----------------------------------------------------------------------- //

void CWeaponFX::PlayImpactSound()
{
	IMPACTFX* pImpactFX = m_pAmmo->pImpactFX;

	if (IsLiquid(m_eCode))
	{
		pImpactFX = m_pAmmo->pUWImpactFX;
	}

	if (!pImpactFX) return;


	if (m_pAmmo->eType == VECTOR)
	{
		if ((m_nDetailLevel == RS_LOW) && GetRandom(1, 2) != 1) return;
		else if ((m_nDetailLevel == RS_MED) && GetRandom(1, 3) == 1) return;
	}

	char* pSnd = GetImpactSound(m_eSurfaceType, m_nAmmoId);
    LTFLOAT fSndRadius = (LTFLOAT) pImpactFX->nSoundRadius;

	if (pSnd)
	{
		uint32 dwFlags = 0;
		float fPitchShift = 1.0f;
		if (g_cvarImpactPitchShift.GetFloat() > 0.0f)
		{
			dwFlags |= PLAYSOUND_CTRL_PITCH;
		}

        uint8 nVolume = IsLiquid(m_eCode) ? 50 : 100;
		g_pClientSoundMgr->PlaySoundFromPos(m_vPos, pSnd, fSndRadius,
			SOUNDPRIORITY_MISC_LOW, dwFlags, nVolume, fPitchShift);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::CreateMuzzleFX()
//
//	PURPOSE:	Create muzzle specific fx
//
// ----------------------------------------------------------------------- //

void CWeaponFX::CreateMuzzleFX()
{
	CSFXMgr* psfxMgr = g_pGameClientShell->GetSFXMgr();
	if (!psfxMgr) return;


	char* pTexture = "SFX\\Impact\\Spr\\Smoke.spr";

	if (IsLiquid(m_eFirePosCode))
	{
		pTexture = DEFAULT_BUBBLE_TEXTURE;
	}

	PARTICLESHOWERCREATESTRUCT sp;

	// At the muzzle, not the grip - same reason as the tracer.
	sp.vPos				= VRMuzzleOrFirePos();
	sp.vDir				= m_vSurfaceNormal * 10.0f;
	sp.pTexture			= pTexture;
	sp.nParticles		= 1;
	sp.fRadius			= 400.0f;
	sp.fDuration		= 1.0f;
	sp.fEmissionRadius	= 0.05f;
	sp.fRadius			= 800.0f;
	sp.fGravity			= 0.0f;

	sp.vColor1.Init(100.0f, 100.0f, 100.0f);
	sp.vColor2.Init(125.0f, 125.0f, 125.0f);

	if (IsLiquid(m_eFirePosCode))
	{
		GetLiquidColorRange(m_eFirePosCode, &sp.vColor1, &sp.vColor2);
		sp.fRadius		= 600.0f;
		sp.fGravity		= 50.0f;
	}

	psfxMgr->CreateSFX(SFX_PARTICLESHOWER_ID, &sp);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::CreateShell()
//
//	PURPOSE:	Create shell casing
//
// ----------------------------------------------------------------------- //

void CWeaponFX::CreateShell()
{
	CSFXMgr* psfxMgr = g_pGameClientShell->GetSFXMgr();
	if (!psfxMgr) return;

	SHELLCREATESTRUCT sc;
	sc.rRot		 = m_rDirRot;
	sc.vStartPos = m_vFirePos;
	sc.nWeaponId = m_nWeaponId;
	sc.nAmmoId	 = m_nAmmoId;

    sc.b3rdPerson = LTFALSE;

	// See if this is our local client who fired, and if so are we in 3rd person...

	if (m_nLocalId == m_nShooterId)
	{
		sc.b3rdPerson = !g_pGameClientShell->IsFirstPerson();
	}
	else
	{
        sc.b3rdPerson = LTTRUE;
	}


	// Adjust the shell position based on the hand-held breach offset...

	if (sc.b3rdPerson)
	{
		sc.vStartPos += (m_vDir * m_pWeapon->fHHBreachOffset);
	}
	else  // Get the shell eject pos...
	{
		// NOT THROUGH THE BREACH SOCKET IN VR.
		//
		// GetShellEjectPos asks the engine for the breach socket "in world
		// space" - but the view weapon carries FLAG_REALLYCLOSE, and this
		// project established on 11 September that such an object's world space
		// IS CAMERA SPACE. It hands back a point near the map origin wherever
		// the player is standing, which is the same defect the muzzle flash had
		// and the eject socket was never offered the fix.
		//
		// CORRECTED 19 September: vStartPos arriving here is m_vFirePos, and
		// m_vFirePos is the HAND, not the drawn barrel end. This comment said
		// the barrel for days and sent the tracer hunt into the wrong file.
		// See CWeaponFX::VRMuzzleOrFirePos for the assignment trail.
		// The casing should leave the ejection port; a point ON THE GUN is what
		// is wanted, and the breach socket cannot give one until it is re-based.
		// AND A CASING LEAVES THE BODY, NOT THE BARREL. vStartPos arriving here
		// is the HAND (corrected 19 September - it is not the barrel end),
		// and a shell should leave the body rather than either: headset testing,
		// 19 September, saw the shells mainly leaving the barrel, and noted that
		// the guns are different lengths. The report is right that a
		// fixed step back from the muzzle only suits one weapon. The centroid
		// of the gun's own drawn nodes is about where the ejection port is and
		// scales with whatever model is in the player's hand.
		// THE BREACH, IN TRUE WORLD SPACE, FROM THE AUTHORED OFFSET.
		//
		// This used VRPrims_DrawnGunCentre, and that is the wrong coordinate
		// space - the same fault that put the tracer metres to the right while
		// the log swore it was beside the gun. Those drawn-gun points are
		// published to OUR renderer and are not where a world-space effect is
		// drawn. In the frames from a clip the casings are scattered to
		// the right exactly as the tracer was, which is the same bug wearing a
		// different hat - and it is why the shells-from-the-barrel report never
		// quite got fixed by moving them
		// between points that were all in that space.
		//
		// The ejection point is authored per weapon, like the muzzle was:
		// HHBreachOffset in ATTRIBUTES/WEAPONS.TXT, "the offset of the breach
		// from the tip of the hand-held weapon model" - a distance back along
		// the barrel, -10 on most, -20 and -50.3 on others. The third-person
		// branch a few lines above has always used it. First person never did,
		// because it had a socket to read instead, and that socket is on a
		// FLAG_REALLYCLOSE model whose world space is camera space.
		//
		// So: the muzzle, which is now built from m_vFirePos and stays in true
		// world, then back along the shot's own direction by that offset.
		{
			// CLAMPED, BECAUSE THE OFFSET IS MEASURED FROM A DIFFERENT MODEL.
			//
			// HHBreachOffset is "from the tip of the HAND-HELD weapon model" -
			// the third-person one - and we are applying it to the player-view
			// muzzle, which is a different reference and a different length.
			// Measured: the ak47's -50.3 against a muzzle 17 units ahead of the
			// grip puts the casing 33 units BEHIND the player's hand, half a metre back
			// past the player's wrist. The Sterling's -10 lands correctly at 11 units.
			// So the figure is right for the model it was authored against and
			// cannot be trusted wholesale against this one.
			//
			// A casing leaves somewhere between the grip and the muzzle; it
			// never appears behind the hand holding the gun. That is the clamp,
			// and it costs the Sterling nothing.
			// IN THE SAME UNITS AS THE MUZZLE IT CLAMPS AGAINST. This limits the
			// breach to "not behind the grip", and the muzzle is now scaled, so
			// an unscaled limit here would clamp 2.5x too tight and drag every
			// casing forward onto the barrel.
			CWeaponModel* pWMb = g_pGameClientShell->GetWeaponModel();
			const float fBreachK = (pWMb && pWMb->VRWeaponScale() > 0.01f)
								 ? (1.0f / pWMb->VRWeaponScale()) : 1.0f;
			const float fMuzFwd = (m_pWeapon ? m_pWeapon->vMuzzlePos.z : 0.0f) * fBreachK;
			float fBreach = m_pWeapon ? m_pWeapon->fHHBreachOffset : 0.0f;
			if (fBreach < -fMuzFwd) fBreach = -fMuzFwd;
			if (fBreach > 0.0f)     fBreach = 0.0f;

			// AND OUT OF THE PORT, NOT THE BORE. Without this the casing leaves
			// along the barrel's own centre line, which is what the report of
			// shells leaving the barrel describes.
			// An ejection port sits to the right of the
			// bore and slightly above it on nearly every weapon in this game.
			// Small, and cvar-tunable rather than argued about.
			LTVector vEjR(0.0f, 0.0f, 0.0f), vEjU(0.0f, 0.0f, 0.0f);
			{
				// THE PORT ROLLS WITH THE GUN, so this basis must too.
				//
				// The first version built right/up from the WORLD's up crossed
				// with the shot direction. That is stable and it does not roll:
				// turn the controller on its side and the casing still leaves
				// towards world-right instead of out of the port, which has
				// moved. It is the same fault that put the muzzle flash's offset
				// in the camera's frame (see CWeaponModel::VRGunRot) - an offset
				// is meaningless without saying which frame it is in, and the
				// answer for anything attached to the gun is always the gun.
				//
				// The world-up basis stays as the fallback, for the shots that
				// are not ours and for a null weapon model, where there is no
				// gun rotation to ask for.
				// Our own shot only: the weapon model is OUR gun, so handing its
				// rotation to an AI's casing would roll every shell in the level
				// with the player's wrist. Same test VRMuzzleOrFirePos uses; it
				// cannot borrow that one's local, which lives in another
				// function.
				const bool bMineToRoll = (m_nLocalId == m_nShooterId)
					&& !g_pGameClientShell->IsUsingExternalCamera()
					&& (g_pGameClientShell->IsFirstPerson() || VRPrims_RebaseEverKnown());

				bool bHaveGunFrame = false;
				CWeaponModel* pWMr = g_pGameClientShell->GetWeaponModel();
				if (bMineToRoll && pWMr)
				{
					LTRotation rG = pWMr->VRGunRot();
					LTVector vGU, vGR, vGF;
					g_pLTClient->GetRotationVectors(&rG, &vGU, &vGR, &vGF);
					if (vGR.Mag() > 0.001f)
					{
						// The gun's right and up in WORLD axes - through the camera
						// basis, see VRPrims_GunFrameToWorld.
						// The Leftorium's mirrored gun ejects to its other side.
						const float fEjSide = VRShared::SwapHands() ? -1.0f : 1.0f;
						vEjR = VRPrims_GunFrameToWorldLast(rG, LTVector(fEjSide * g_vtVRShellRight.GetFloat(), 0.0f, 0.0f));
						vEjU = VRPrims_GunFrameToWorldLast(rG, LTVector(0.0f, g_vtVRShellUp.GetFloat(), 0.0f));
						bHaveGunFrame = true;
					}
				}

				LTVector vWorldUp(0.0f, 1.0f, 0.0f);
				LTVector vR = vWorldUp.Cross(m_vDir);
				if (!bHaveGunFrame && vR.Mag() > 0.001f)
				{
					vR.Norm();
					LTVector vU = m_vDir.Cross(vR);
					vU.Norm();
					vEjR = vR * g_vtVRShellRight.GetFloat();
					vEjU = vU * g_vtVRShellUp.GetFloat();
				}
			}

			{
				CWeaponModel* pWMs = g_pGameClientShell->GetWeaponModel();
				const LTVector vS = pWMs ? pWMs->VRShellOffset() : LTVector(0.0f,0.0f,0.0f);
				sc.vStartPos = VRMuzzleOrFirePos() + (m_vDir * fBreach) + vEjR + vEjU
							 + VRPerEffectOffset(vS, m_vDir);
			}

			static int s_nSaidBreach = 0;
			if (s_nSaidBreach < 6)
			{
				++s_nSaidBreach;
				const LTVector vFromHand = sc.vStartPos - m_vFirePos;
				const LTVector vMuzB = VRMuzzleOrFirePos();
				CWeaponModel* pWMs2 = g_pGameClientShell->GetWeaponModel();
				const LTVector vS2 = pWMs2 ? pWMs2->VRShellOffset() : LTVector(0.0f,0.0f,0.0f);
				VRLog::Msg("VRBreach: casing leaves %.0f %.0f %.0f - breach offset"
					" %.1f back from the muzzle, %.0f units from the hand (%.2f m)"
					" | muzzle %.0f %.0f %.0f dir %.2f %.2f %.2f ejR %.1f %.1f %.1f ejU %.1f %.1f %.1f tune %.1f %.1f %.1f",
					sc.vStartPos.x, sc.vStartPos.y, sc.vStartPos.z, fBreach,
					vFromHand.Mag(), vFromHand.Mag() * 0.01692f,
					vMuzB.x, vMuzB.y, vMuzB.z, m_vDir.x, m_vDir.y, m_vDir.z,
					vEjR.x, vEjR.y, vEjR.z, vEjU.x, vEjU.y, vEjU.z, vS2.x, vS2.y, vS2.z);
			}
		}

		// WHERE THE CASING ACTUALLY STARTS, against the muzzle the same shot used.
		//
		// casings come from about 8 feet away and
		// there is no bullet trail at all. m_vFirePos arrives here as the HAND - NOT, as
		// this comment claimed, re-based onto WeaponModel's m_vFlashPos, which
		// nothing ever assigns to it. GetShellEjectPos then
		// OVERWRITES it with the breach socket read off the view weapon - and the
		// view weapon carries FLAG_REALLYCLOSE, whose "world space" is CAMERA
		// space. That is the same defect the muzzle flash had on 11 September and
		// the same one VRPrims_RebasePointLast exists to undo; the eject socket was
		// never offered to it.
		//
		// This line does not fix it. It prints the separation so the desk can say
		// whether it is a few units (fine), ~100 (the reported 8 feet) or ~2000 (the map
		// origin), because those three want three different answers and reading the
		// code cannot tell them apart.
		{
			static int s_nSaidShell = 0;
			if (s_nSaidShell < 8)
			{
				++s_nSaidShell;
				const LTVector vSep = sc.vStartPos - m_vFirePos;
				VRLog::Msg("VRShell: casing from %.0f %.0f %.0f; the shot's HAND was"
					// 1 unit = 16.92 mm, from this repo's own bullet-hole
					// measurement (0.65 units = 11.0 mm), so 18.0 units per
					// foot. The first version of this line said 12 and
					// reported 22 units as 1.8 ft when it is 1.2.
					" %.0f %.0f %.0f - %.0f units apart (%.2f m, %.1f ft)",
					sc.vStartPos.x, sc.vStartPos.y, sc.vStartPos.z,
					m_vFirePos.x, m_vFirePos.y, m_vFirePos.z,
					vSep.Mag(), vSep.Mag() * 0.01692f, vSep.Mag() / 18.0f);
			}
		}

		// Add on the player's velocity...

		HOBJECT hObj = g_pGameClientShell->GetMoveMgr()->GetObject();
		if (hObj)
		{
            g_pLTClient->Physics()->GetVelocity(hObj, &sc.vStartVel);
		}
		//sc.dwFlags = FLAG_REALLYCLOSE;
	}


	// Only create every other shell if medium detail set...
	// if (m_nDetailLevel == RS_MED && (++s_nNumShells % 2 == 0)) return;

	psfxMgr->CreateSFX(SFX_SHELLCASING_ID, &sc);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::CreateBloodSplatFX
//
//	PURPOSE:	Create the blood splats, etc.
//
// ----------------------------------------------------------------------- //

void CWeaponFX::CreateBloodSplatFX()
{
	CGameSettings* pSettings = g_pInterfaceMgr->GetSettings();
	if (!pSettings || !pSettings->Gore()) return;

	CSFXMgr* psfxMgr = g_pGameClientShell->GetSFXMgr();
	if (!psfxMgr) return;

	CSpecialFX* pFX = LTNULL;

	LTFLOAT fRange = g_vtBloodSplatsRange.GetFloat();

	// See if we should make some blood splats...

	ClientIntersectQuery iQuery;
	IntersectInfo iInfo;

	iQuery.m_From = m_vPos;

	LTVector vDir = m_vDir;

	// Create some blood splats...

	int nNumSplats = GetRandom((int)g_vtBloodSplatsMinNum.GetFloat(), (int)g_vtBloodSplatsMaxNum.GetFloat());

	LTVector vU, vR, vF;
    g_pLTClient->GetRotationVectors(&m_rDirRot, &vU, &vR, &vF);

	for (int i=0; i < nNumSplats; i++)
	{
		LTVector vDir = m_vDir;

		// Perturb direction after first splat...

		if (i > 0)
		{
			float fPerturb = g_vtBloodSplatsPerturb.GetFloat();

			float fRPerturb = (GetRandom(-fPerturb, fPerturb))/1000.0f;
			float fUPerturb = (GetRandom(-fPerturb, fPerturb))/1000.0f;

			vDir += (vR * fRPerturb);
			vDir += (vU * fUPerturb);
		}

		iQuery.m_To = vDir * fRange;
		iQuery.m_To += m_vPos;
		iQuery.m_Flags = IGNORE_NONSOLID | INTERSECT_HPOLY;

		if (g_pLTClient->IntersectSegment(&iQuery, &iInfo) && IsMainWorld(iInfo.m_hObject))
		{
			SurfaceType eType = GetSurfaceType(iInfo);
			if (eType == ST_SKY || eType == ST_INVISIBLE)
			{
				return; // Don't leave blood on the sky
			}


			LTBOOL bBigBlood = (LTBOOL)GetConsoleInt("BigBlood", 0);

			// Create a blood splat...

			BSCREATESTRUCT sc;

			g_pLTClient->AlignRotation(&(sc.rRot), &(iInfo.m_Plane.m_Normal), LTNULL);

			// Randomly rotate the blood splat

			g_pLTClient->RotateAroundAxis(&(sc.rRot), &(iInfo.m_Plane.m_Normal), GetRandom(0.0f, MATH_CIRCLE));

			// OFF THE WALL A BIT - BUT NOT TWO UNITS, AND NOT ALONG THE SHOT.
			//
			// The original pushes the splat 2 units back along the FIRE
			// direction. On a monitor that is invisible; in stereo it is about
			// 3.4 cm of parallax at this game's 58.75 units to the metre, and
			// in the headset the blood read as slightly three-dimensional, standing
			// off the wall by a couple of inches. Worse, pushing along the shot rather than the
			// surface means a glancing hit slides the splat sideways as well as
			// out.
			//
			// Along the surface NORMAL, by VRDecalLift (0.5 units, about 8 mm),
			// which is enough to stay off the wall and too little to read as a
			// gap. VRDecalLift 2 restores the old behaviour exactly.
			LTVector vLift = iInfo.m_Plane.m_Normal * g_vtVRDecalLift.GetFloat();
			sc.vPos = iInfo.m_Point + vLift;
			sc.vVel.Init(0.0f, 0.0f, 0.0f);

			sc.vInitialScale.Init(1.0f, 1.0f, 1.0f);
			sc.vInitialScale.x	= GetRandom(g_vtBloodSplatsMinScale.GetFloat(), g_vtBloodSplatsMaxScale.GetFloat());

			if (bBigBlood) sc.vInitialScale.x *= g_vtBigBloodSizeScale.GetFloat();

			sc.vInitialScale.y	= sc.vInitialScale.x;
			sc.vFinalScale		= sc.vInitialScale;

			sc.dwFlags			= FLAG_VISIBLE | FLAG_ROTATEABLESPRITE | FLAG_NOLIGHT;
			sc.fLifeTime		= GetRandom(g_vtBloodSplatsMinLifetime.GetFloat(), g_vtBloodSplatsMaxLifetime.GetFloat());

			if (bBigBlood) sc.fLifeTime *= g_vtBigBloodLifeScale.GetFloat();

			sc.fInitialAlpha	= 1.0f;
			sc.fFinalAlpha		= 0.0f;

			// VRPersistentFX: the splat stays. Both halves are needed - a long
			// life alone still fades to nothing over it, because the scale FX
			// interpolates alpha across the lifetime. Equal alphas hold it.
			if (g_pGameClientShell && g_pGameClientShell->VRPersistentFX())
			{
				sc.fLifeTime	= 100000.0f;
				sc.fFinalAlpha	= sc.fInitialAlpha;
			}
			sc.nType			= OT_SPRITE;
			sc.bMultiply		= LTTRUE;

			char* pBloodFiles[] =
			{
				"Sfx\\Test\\Spr\\BloodL1.spr",
				"Sfx\\Test\\Spr\\BloodL2.spr",
				"Sfx\\Test\\Spr\\BloodL3.spr",
				"Sfx\\Test\\Spr\\BloodL4.spr"
			};

			sc.pFilename = pBloodFiles[GetRandom(0,3)];

			pFX = psfxMgr->CreateSFX(SFX_SCALE_ID, &sc);
			if (pFX) pFX->Update();
		}
		else if (i==0)
		{
			// Didn't hit anything straight back, do don't bother to
			// do anymore...

			return;
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::PlayFireSound
//
//	PURPOSE:	Play the fire sound
//
// ----------------------------------------------------------------------- //

void CWeaponFX::PlayFireSound()
{
	if (m_nLocalId >= 0 && m_nLocalId == m_nShooterId)
	{
		return;  // This client already heard the sound ;)
	}

	PlayerSoundId eSoundId = PSI_FIRE;

	if (m_wFireFX & WFX_SILENCED)
	{
		eSoundId = PSI_SILENCED_FIRE;
	}
	else if (m_wFireFX & WFX_ALTFIRESND)
	{
		eSoundId = PSI_ALT_FIRE;
	}

	::PlayWeaponSound(m_pWeapon, m_vFirePos, eSoundId);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::CalcFirePos
//
//	PURPOSE:	Calculate the fire position based on the FireFrom object
//
// ----------------------------------------------------------------------- //

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::VRMuzzleOrFirePos()
//
//	PURPOSE:	The point the shot should LOOK like it left, in VR
//
// ----------------------------------------------------------------------- //

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::VRNodeProbe()
//
//	PURPOSE:	Ask the MODEL where its muzzle is, and print it next to what
//				the attributes claim
//
// ----------------------------------------------------------------------- //
//
// WHY THIS IS A PROBE AND NOT A FIX. The authored MuzzlePos cannot be right for
// every weapon: 32 of the 45 share their value with another weapon and 19 share
// one default, so most of the arsenal has no muzzle of its own in that table.
// Reading the ART instead was the obvious answer and it ran straight into the
// question nobody can reason their way past - what UNIT is a node transform in,
// and does it already carry the 0.4 draw scale.
//
// Every attempt to settle that on paper today produced a ratio that looked
// convincing and then disagreed with the next weapon. So do not settle it on
// paper. Print the authored offset and the model's own node side by side, fire
// the gun once, and read the factor off the log.
//
// The Luger is the weapon to fire. It is the one that overshoots, and it is the
// only one that carries a node literally called 'muzzlenode' - so it has an
// answer the attributes cannot argue with.

void CWeaponFX::VRNodeProbe() const
{
	if (g_vtVRNodeProbe.GetFloat() < 0.5f) return;

	// Eight shots is enough to see it and few enough not to cost a frame.
	static int s_nProbed = 0;
	if (s_nProbed >= 8) return;
	++s_nProbed;

	CWeaponModel* pWM = g_pGameClientShell->GetWeaponModel();
	if (!pWM || !g_pModelLT) return;
	HOBJECT hGun = pWM->GetHandle();
	if (!hGun) return;

	const LTVector vAuth = pWM->GetMuzzleOffset();
	const float fS = (pWM->VRWeaponScale() > 0.01f) ? (1.0f / pWM->VRWeaponScale()) : 1.0f;
	VRLog::Msg("VRNodeProbe: authored MuzzlePos <%.2f %.2f %.2f>, scaled x%.2f -> <%.2f %.2f %.2f>",
		vAuth.x, vAuth.y, vAuth.z, fS, vAuth.x * fS, vAuth.y * fS, vAuth.z * fS);

	// The names the 44 player-view models actually use, read out of the .abc
	// node tables rather than guessed: only 9 weapons carry one, and the Luger
	// is the only one with both 'barrel' and 'muzzlenode'.
	static const char* kNames[] = {
		"muzzlenode", "barrel", "frontbarrel2", "barrelback",
		"Delisle_barrel2", "nozzle_1", "backtip", NULL };

	int nFound = 0;
	for (int i = 0; kNames[i]; ++i)
	{
		HMODELNODE hN = INVALID_MODEL_NODE;
		if (g_pModelLT->GetNode(hGun, (char*)kNames[i], hN) != LT_OK) continue;
		if (hN == INVALID_MODEL_NODE) continue;
		++nFound;

		// ZEROED FIRST. LTVector's default constructor is `_CVector() {}` - it
		// initialises nothing - and the log below reads tL.m_Pos whether or not
		// the call succeeded, because the arguments are evaluated before the
		// "(no local)" text explains that they are meaningless. That is
		// uninitialised stack read into a printf, and it would print convincing
		// garbage on exactly the failure it is meant to report.
		LTransform tL, tW;
		tL.m_Pos.Init(); tW.m_Pos.Init();
		bool bL = (g_pModelLT->GetNodeTransform(hGun, hN, tL, LTFALSE) == LT_OK);
		bool bW = (g_pModelLT->GetNodeTransform(hGun, hN, tW, LTTRUE)  == LT_OK);
		VRLog::Msg("VRNodeProbe:   node '%s'%s local <%.3f %.3f %.3f>%s world <%.1f %.1f %.1f>",
			kNames[i],
			bL ? "" : " (no local)",  tL.m_Pos.x, tL.m_Pos.y, tL.m_Pos.z,
			bW ? "" : " (no world)",  tW.m_Pos.x, tW.m_Pos.y, tW.m_Pos.z);
	}
	if (!nFound)
		VRLog::Msg("VRNodeProbe:   this weapon carries no named barrel node");

	// AND HOW BIG THE GUN ACTUALLY IS, which is the number every attempt to
	// settle the muzzle has lacked.
	//
	// The offsets are known exactly - the ak47's muzzle sits 41.34 units out,
	// the Luger's 56.95 - and there has been no way to say whether that is the
	// barrel tip or a metre past it, because the gun's own SIZE was never
	// measured. Three offline attempts to get it from the art all died on the
	// player-view models having no shared coordinate convention, and a
	// photograph cannot settle it either: a tracer travels between the shot and
	// the shutter, and the brightest thing added to a firing frame is the
	// IMPACT, not the muzzle.
	//
	// GetModelAnimUserDims returns the drawn model's bounding half-dimensions
	// for its current animation, so a muzzle beyond the gun's own forward
	// extent would be provably out in the room, with no eye needed.
	//
	// IT DOES NOT WORK, AND THIS IS THE RECORD OF THAT. Measured 20 September on
	// six weapons - ak47, Sterling, Luger, Walther SMG, Revolver, Dragunov -
	// every one returns <1.50 2.00 1.50>. That is the gameplay COLLISION box, a
	// fixed placeholder for a player-view weapon, not the visual extent: it
	// cannot distinguish a Luger from a Sterling, so it cannot judge a muzzle.
	//
	// So the engine does not know the drawn gun's size either. That is FOUR
	// ways of getting it without a human: mesh extents, a millimetre ruler
	// against the real firearms, relative mesh size against a verified weapon,
	// and now the engine's own dims. The line below stays because the number it
	// prints is the evidence, and a fifth attempt should start by reading it.
	{
		LTAnimTracker* pTracker = LTNULL;
		HMODELANIM hAnim = (HMODELANIM)-1;
		if (g_pModelLT->GetMainTracker(hGun, pTracker) == LT_OK && pTracker &&
			g_pModelLT->GetCurAnim(pTracker, hAnim) == LT_OK)
		{
			LTVector vDims(0.0f, 0.0f, 0.0f);
			if (g_pLTClient->Common()->GetModelAnimUserDims(hGun, &vDims, hAnim) == LT_OK)
			{
				VRLog::Msg("VRNodeProbe:   drawn gun half-dims <%.2f %.2f %.2f>,"
						   " scaled x%.2f -> forward extent %.2f  (muzzle is at %.2f)",
						   vDims.x, vDims.y, vDims.z, fS, vDims.z * fS,
						   vAuth.z * fS);
			}
			else VRLog::Msg("VRNodeProbe:   GetModelAnimUserDims refused");
		}
		else VRLog::Msg("VRNodeProbe:   no anim tracker for the drawn gun");
	}

	// The grip, in the space everything here is built from, so the log shows a
	// DISTANCE rather than two unrelated coordinates.
	VRLog::Msg("VRNodeProbe:   m_vFirePos (the grip, true world) <%.1f %.1f %.1f>",
		m_vFirePos.x, m_vFirePos.y, m_vFirePos.z);
}


// THE PER-EFFECT NUDGE, IN THE GUN'S FRAME.
//
// The tracer and the casings start from the same muzzle the flash does - that is
// e71f170 and it stays - but each can now be nudged off it on its own, because
// a tracer that leaves the barrel correctly and a casing that leaves the breach
// correctly are not the same point.
//
// Returns zero unless the player has tuned that effect, so nothing moves until asked.
static LTVector VRPerEffectOffset(const LTVector& vOff, const LTVector& vDir)
{
	if (vOff.MagSqr() < 0.0001f) return LTVector(0.0f, 0.0f, 0.0f);
	CWeaponModel* pWM = g_pGameClientShell->GetWeaponModel();
	if (!pWM) return LTVector(0.0f, 0.0f, 0.0f);
	LTRotation rG = pWM->VRGunRot();
	LTVector vGU, vGR, vGF;
	g_pLTClient->GetRotationVectors(&rG, &vGU, &vGR, &vGF);
	if (vGF.Mag() <= 0.001f)
	{
		// Same world-up fallback the muzzle itself uses when there is no gun
		// rotation to ask for.
		LTVector vWorldUp(0.0f, 1.0f, 0.0f);
		LTVector vRight = vWorldUp.Cross(vDir);
		if (vRight.Mag() <= 0.001f) return LTVector(0.0f, 0.0f, 0.0f);
		vRight.Norm();
		LTVector vUp = vDir.Cross(vRight);
		vUp.Norm();
		return vRight * vOff.x + vUp * vOff.y + vDir * vOff.z;
	}
	// Through the camera basis - see VRPrims_GunFrameToWorld.
	return VRPrims_GunFrameToWorldLast(rG, vOff);
}

LTVector CWeaponFX::VRMuzzleOrFirePos() const
{
	// m_vFirePos IS THE HAND, NOT THE MUZZLE, AND THE COMMENTS IN THIS FILE
	// SAID OTHERWISE FOR DAYS.
	//
	// Two of them claim m_vFirePos "is now the drawn barrel end" and "arrives
	// here already re-based onto the gun in the hand (WeaponModel's
	// m_vFlashPos)". Neither is true. m_vFirePos is assigned exactly twice -
	// from the fire message at construction, and through CalcFirePos, which
	// returns its argument untouched in first person. Nothing ever puts a
	// muzzle in it.
	//
	// What the fire message carries is CWeaponModel::GetFireInfo's vFirePos:
	// the camera position plus the hand's offset from the head. That is the
	// GRIP. It is the right origin for the ray - it is why shots go where the
	// barrel points instead of 43 cm high - and the wrong origin for anything
	// you can SEE, because a tracer drawn from the grip starts behind and
	// below the barrel it should be leaving.
	//
	// the tracers came out below and
	// behind the gun, and again on the reverted build. The report was right both times, and both
	// times the muzzle work was in the wrong file - the muzzle FLASH was fine,
	// because that is placed from VRPrims_DrawnMuzzle. The tracer never asked.
	//
	// ONLY FOR OUR OWN FIRST-PERSON SHOT. VRPrims_DrawnMuzzle holds the muzzle
	// of the model in OUR hand, so handing it to an AI's shot, another
	// player's, or our own third-person body would move every tracer in the
	// level onto our barrel.
	// IN VR, OR IN FIRST PERSON. Not "in first person" alone, because the one
	// case that needs this most is the case where IsFirstPerson() has gone
	// false: that is what creates the world muzzle flash at all. Gating on it
	// here would have handed that flash the hand instead of the muzzle, which
	// is a quieter version of the same bug.
	const bool bOurs = (m_nLocalId == m_nShooterId)
		&& !g_pGameClientShell->IsUsingExternalCamera()
		&& (g_pGameClientShell->IsFirstPerson() || VRPrims_RebaseEverKnown());
	if (bOurs)
	{
		// A STORED POSITION GOES STALE; A DISTANCE DOES NOT.
		//
		// The first version of this asked VRPrims_DrawnMuzzle for the muzzle's
		// world POINT. It worked about half the time. From the 19 September
		// headset log, every one of these shots the same p38:
		//
		//   moved 20 units forward        good
		//   moved  9 units forward        good
		//   the muzzle is 286 units from the hand - too far      rejected
		//   the muzzle is 371 units from the hand - too far      rejected
		//
		// The stored point is written by the weapon model's update. On a frame
		// where that has not run, or has not run since the player moved, the
		// point is left behind in the world - 286 units is 4.8 metres, which is
		// simply where the player was standing a moment ago. The guard then correctly
		// refused it and the tracer went back to leaving the player's hand, which is the
		// bug this function exists to fix.
		//
		// So do not carry a point across frames at all. The shot already knows
		// its own direction - m_vDir is the hand-to-impact line, which IS the
		// barrel's line - and the weapon knows how far its muzzle sits from the
		// grip. A distance is a property of the WEAPON, not of where anybody was
		// standing, so it cannot be stale in a way that matters: the worst a
		// frame-old value can be is the right answer for the gun you are holding.
		// ANCHOR ON THE GUN YOU CAN SEE, NOT ON THE HAND THE CODE COMPUTES.
		//
		// Measured 19 September, tracer start against VRPrims_DrawnGunCentre,
		// decomposed along the shot's own axes:
		//
		//   sterling   right -5.2  up -8.7  forward +41.5 units (27.7 inches)
		//   revolver   right -2.1  up -1.1  forward +29.5 units (19.6 inches)
		//
		// The error is FORWARD, up to two feet of it, and it is a double count:
		// m_vFirePos is already about 21 units ahead of the drawn gun, and the
		// muzzle distance was added on top of that. In the headset it read as
		// around 6-12 inches from the gun on the right hand side, which is
		// the same thing seen from the eye - a point two feet down an angled
		// barrel is off to one side of it.
		//
		// So anchor where the SHELL CASINGS leave from. That point is on the
		// drawn weapon, it is published by VRPublishModels rather than by the
		// weapon model's update - which is the one that went stale and threw
		// out 286-unit muzzles - and the player has not called the casings displaced
		// since they were moved onto it. VRTracerFwd nudges it up the barrel if
		// leaving from the gun's middle reads wrong; 0 is the conservative
		// default, because every overshoot today has been in that direction.
		// TRUE WORLD SPACE, AND NOTHING FROM THE DRAWN GUN.
		//
		// Frames pulled out of a clip (ffmpeg, 19 September)
		// settle what four rounds of logging could not. The bright thing the player has
		// been reporting as muzzle flashes way off to the right about twenty feet
		// away is not a flash at all: it is THE TRACER, floating detached to
		// the right of the gun. The flash at the wall is the impact, ringed by
		// its own bullet holes, and it is correct.
		//
		// The streak's line passes exactly through the impact point and about a
		// hundred pixels ABOVE the muzzle. Its end is right and its start is
		// wrong - while the log insisted the start was 7.8 units from the drawn
		// gun. Both are true, and that is the finding: VRPrims_DrawnGunCentre
		// and VRPrims_DrawnMuzzle are positions published to OUR renderer, and
		// they are not the world coordinates a world-space effect is drawn in.
		// Anchoring a tracer to them puts it near the gun in the log and metres
		// away on screen. The shell casings use the same anchor and are scattered
		// to the right in those same frames.
		//
		// m_vFirePos IS true world: the bullets land where the player aims, and the
		// tracer's own vEndPos is the impact and renders correctly. So build the
		// muzzle from it and never leave that space.
		//
		// The offset is the authored per-weapon MuzzlePos from
		// ATTRIBUTES/WEAPONS.TXT - P38 <2.04,-2.25,12.72>, Sterling
		// <3.06,-5.64,19.30> - applied in a basis built from the shot's own
		// direction. Barrel roll is not recoverable here and is not worth
		// recovering: it would swing a two-unit sideways term, which is under an
		// inch.
		CWeaponModel* pWMm = g_pGameClientShell->GetWeaponModel();
		if (pWMm)
		{
			// SCALED LIKE THE FLASH'S, or the two disagree by 2.5x.
			//
			// The authored MuzzlePos is in the MODEL's units and the view weapon
			// draws at VRWeaponScale 0.400, so an unscaled offset lands short by
			// 1/0.4. The flash was corrected for that; this path feeds the
			// TRACER and, through it, the shell casings. Leaving it unscaled
			// would have put the flash on the barrel tip and the tracer back at
			// the magazine - the exact flash-right-trail-wrong
			// split that started this whole thread.
			//
			// Caught by re-reading the change rather than by a tester finding it.
			const float fMzScale = (pWMm->VRWeaponScale() > 0.01f)
								 ? (1.0f / pWMm->VRWeaponScale()) : 1.0f;
			const LTVector vOff = pWMm->GetMuzzleOffset() * fMzScale;
			if (vOff.MagSqr() > 0.25f && vOff.MagSqr() < 40000.0f)
			{
				// THE GUN'S FRAME FIRST, because the authored MuzzlePos has
				// RIGHT and UP components - the ak47's is <1.85, -3.12, 16.54> -
				// and those are meaningless without a frame that rolls with the
				// weapon. A basis built from the world's up does not roll, so
				// tipping the controller on its side leaves the muzzle where the
				// world thinks right is rather than where the barrel's right has
				// moved to.
				//
				// A yaw-only sweep cannot see this: yaw keeps world-up aligned
				// with the gun's up, so the measurement came back healthy (9%
				// against the flash's 44%) while a roll would have shown the
				// error. It is the same fault as the flash's camera-frame offset
				// and the casing's ejection basis, found by looking for the
				// pattern rather than by it going wrong again.
				CWeaponModel* pWMg = g_pGameClientShell->GetWeaponModel();
				if (bOurs && pWMg)
				{
					LTRotation rG = pWMg->VRGunRot();
					LTVector vGU, vGR, vGF;
					g_pLTClient->GetRotationVectors(&rG, &vGU, &vGR, &vGF);
					if (vGF.Mag() > 0.001f)
					{
						// THE SAME PER-WEAPON TRIM THE FLASH USES.
						//
						// The flash reads VRFlashOffR/U/F@<weapon> and the
						// tracer read only the global VRTracerFwd, so tuning a
						// weapon's flash onto its barrel would have left the
						// tracer where it was - the flash and the trail
						// disagreeing, which is the fault this entire thread
						// started from, rebuilt out of the tuning controls.
						//
						// There is one muzzle. Both effects take it, and one
						// number moves both.
						const LTVector vTrim = pWMg->VRFlashOffset();
						// ...AND THROUGH THE CAMERA BASIS. See VRPrims_GunFrameToWorld.
						// ...plus the GRIP offset, so the trail and the casings start
						// from the gun as drawn, not from where the hand alone put it.
						return m_vFirePos
							 + VRPrims_GunFrameToWorldLast(rG, vOff + vTrim + pWMg->VRGripOffset())
							 + m_vDir * g_vtVRTracerFwd.GetFloat();
					}
				}

				// The world-up fallback, for anyone else's shot and for a null
				// weapon model, where there is no gun rotation to ask for.
				LTVector vWorldUp(0.0f, 1.0f, 0.0f);
				LTVector vRight = vWorldUp.Cross(m_vDir);
				if (vRight.Mag() > 0.001f)
				{
					vRight.Norm();
					LTVector vUp = m_vDir.Cross(vRight);
					vUp.Norm();
					return m_vFirePos + vRight * vOff.x + vUp * vOff.y
						 + m_vDir * vOff.z + m_vDir * g_vtVRTracerFwd.GetFloat();
				}
			}
		}

		CWeaponModel* pWM = g_pGameClientShell->GetWeaponModel();
		if (pWM)
		{
			// The authored per-weapon offset, which is what the muzzle flash has
			// used all along. Its forward component dominates; taking the
			// magnitude drops the small down-and-across part, and that is
			// deliberate - headset testing twice found the full offset a little lower
			// than the gun, and the tracer wants the barrel's line, not a point
			// slung under it.
			// Scaled for the same reason as above.
			const float fMuzKs = (pWM->VRWeaponScale() > 0.01f)
							   ? (1.0f / pWM->VRWeaponScale()) : 1.0f;
			const float fMuzzleDist = pWM->GetMuzzleOffset().Mag() * fMuzKs;
			if (fMuzzleDist > 0.5f && fMuzzleDist < 200.0f)
				return m_vFirePos + (m_vDir * fMuzzleDist);
		}
	}

	return m_vFirePos;
}


LTVector CWeaponFX::CalcFirePos(LTVector vFirePos)
{
	if (!m_hFiredFrom) return vFirePos;

	// See if this is our local client who fired, and if so
	// only calculate fire position if we are in 3rd person...

	if (m_nLocalId == m_nShooterId)
	{
		if (g_pGameClientShell->IsFirstPerson()) return vFirePos;

		// AND IN VR, EVEN WHEN THE CAMERA SAYS OTHERWISE.
		//
		// Below, this asks the player's THIRD-PERSON body for its "Flash"
		// attachment socket. That body stands where the player object is, which
		// in VR is nowhere near where the player is looking from - so every muzzle
		// effect built on the result appears somewhere across the room.
		//
		// It is reached because IsFirstPerson() is not reliably true while
		// firing. The 19 September headset log, the same session, seconds apart:
		//     camera other          (14.236)
		//     camera first person   (15.413)
		// One of those frames takes the socket and one does not, which is also
		// why the flash looked like it teleported rather than sat still in the
		// wrong place.
		//
		// In VR the player IS first person whatever the camera enum reports, so
		// the socket is never the right answer for our own shot.
		if (VRPrims_RebaseEverKnown()) return vFirePos;
	}

    LTVector vPos;
    LTRotation rRot;
	if (!GetAttachmentSocketTransform(m_hFiredFrom, "Flash", vPos, rRot))
	{
		vPos = vFirePos;
	}

	return vPos;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::PlayBulletFlyBySound()
//
//	PURPOSE:	Play bullet fly by sound (if appropriate)
//
// ----------------------------------------------------------------------- //

void CWeaponFX::PlayBulletFlyBySound()
{
	if (!m_pWeapon || !m_pAmmo) return;

	if (m_pAmmo->eType != VECTOR) return;

	// Camera pos

	HOBJECT hCamera = g_pGameClientShell->GetCamera();
    LTVector vPos;
    g_pLTClient->GetObjectPos(hCamera, &vPos);

	// We only play the flyby sound if we won't hear an impact or
	// fire sound...

	LTVector vDist = m_vFirePos - vPos;
	if (vDist.Mag() < m_pWeapon->nFireSoundRadius) return;

	if (m_pAmmo->pImpactFX)
	{
		vDist = m_vPos - vPos;
		if (vDist.Mag() < m_pAmmo->pImpactFX->nSoundRadius) return;
	}


	// See if the camera is close enough to the bullet path to hear the
	// bullet...

	LTFLOAT fRadius = g_cvarFlyByRadius.GetFloat();

	LTVector vDir = m_vDir;

	const LTVector vRelativePos = vPos - m_vFirePos;
    const LTFLOAT fRayDist = vDir.Dot(vRelativePos);
	LTVector vBulletDir = (vDir*fRayDist - vRelativePos);

    const LTFLOAT fDistSqr = vBulletDir.MagSqr();

	if (fDistSqr < fRadius*fRadius)
	{
		vPos += vBulletDir;
		g_pClientSoundMgr->PlaySoundFromPos(vPos, "Guns\\Snd\\flyby.wav",
			g_cvarFlyBySoundRadius.GetFloat(), SOUNDPRIORITY_MISC_LOW);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponFX::PlayImpactDing()
//
//	PURPOSE:	Play a impact ding sound if appropriate
//
// ----------------------------------------------------------------------- //

void CWeaponFX::PlayImpactDing()
{
	if (!IsMultiplayerGame()) return;

	CCharacterFX* pCharFX = g_pGameClientShell->GetMoveMgr()->GetCharacterFX();
	if (pCharFX)
	{
		pCharFX->PlayDingSound();
	}
}
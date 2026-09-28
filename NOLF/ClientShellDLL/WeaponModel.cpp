// ----------------------------------------------------------------------- //
//
// MODULE  : WeaponModel.cpp
//
// PURPOSE : Generic client-side WeaponModel wrapper class - Implementation
//
// CREATED : 9/27/97
//
// (c) 1997-2000 Monolith Productions, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "WeaponModel.h"
#include "ClientUtilities.h"
#include "GameClientShell.h"
#include "ShellCasingFX.h"
#include "SFXMsgIds.h"
#include "GameSettings.h"
#include "MsgIds.h"
#include "WeaponFX.h"
#include "ProjectileFX.h"
#include "ClientServerShared.h"
#include "ClientWeaponUtils.h"
#include "iltphysics.h"
#include "PlayerStats.h"
#include "VRShared.h"
#include "VRWeaponVar.h"
#include "VRLog.h"
#include "VRPrims.h"
extern VarTrack g_vtVRGunAutoTrim;

// VR: aim the weapon with the right controller instead of the head.
extern VarTrack g_vtVRHandAim;
extern VarTrack g_vtVRFlashTune;
extern VarTrack g_vtVRFlashOffR;
extern VarTrack g_vtVRFlashOffU;
extern VarTrack g_vtVRFlashOffF;
extern VarTrack g_vtVRFlashHold;
extern VarTrack g_vtVRHandPosScale;
extern VarTrack g_vtVRHandFire;
// TEST ONLY: pretend no silencer is fitted, so the flash path can be
// exercised whatever the player happens to be carrying. Every weapon in
// Cate's early save is silenced - the P38 by design, and the mods she owns
// fit the rest - so a desk run cannot otherwise produce a flash at all,
// and three runs on 11 September measured the gate rather than the flash.
static VarTrack g_vtVRDebugNoSilencer;

extern VarTrack g_vtVRGunTrimPitch;
extern VarTrack g_vtVRGunAtHand;
extern VarTrack g_vtVRHandRoll;
extern VarTrack g_vtVRHeadAsMouse;
extern VarTrack g_vtVRHaptics;
extern VarTrack g_vtVRViewModelScale;
extern VarTrack g_vtVRWeaponDist;
extern VarTrack g_vtVRWeaponScale;

// Per-weapon placement. Each falls back to the global VarTrack above when no
// "@<weapon>" override exists, so behaviour is unchanged until an override is
// actually set. See VRWeaponVar.h for the naming convention.
// AND THE SAME FOR THE OTHER TWO EFFECTS, so each can be tuned on its own.
//
// The tracer and the casings are ADDITIVE ON TOP of the flash's offset, not
// replacements for it. e71f170 deliberately made one number move the flash and
// the tracer together, because tuning one and leaving the other behind is the
// fault that started this whole thread. That still holds: move the flash and
// all three follow. These are the per-effect difference after that.
static VRWeaponVar s_vrTracerOffR;
static VRWeaponVar s_vrTracerOffU;
static VRWeaponVar s_vrTracerOffF;
static VRWeaponVar s_vrShellOffR;
static VRWeaponVar s_vrShellOffU;
static VRWeaponVar s_vrShellOffF;
static VRWeaponVar s_vrGripOffR;
static VRWeaponVar s_vrGripOffU;
static VRWeaponVar s_vrGripOffF;
// THE SCOPE LENS, for a scope that is part of the gun mesh: where the eyepiece
// sits in the gun's frame (world units, R/U/F), on top of whatever the mesh
// says, and a disc radius that replaces the mesh's when set. The tuner's
// LENS mode writes the three offsets.
static VRWeaponVar s_vrLensOffR;
static VRWeaponVar s_vrLensOffU;
static VRWeaponVar s_vrLensOffF;
static VRWeaponVar s_vrLensRad;
static VRWeaponVar s_vrWeaponDist;
static VRWeaponVar s_vrWeaponScale;
// THE ANGLE THE TUNER SETS BY EYE, per weapon, degrees, always on. Headset
// testing, 20 September: the Contender pointed down at roughly 30 degrees -
// the player-view models share no axis convention, so each gets its own.
static VRWeaponVar s_vrAngleYaw;
static VRWeaponVar s_vrAnglePitch;
static VRWeaponVar s_vrAngleRoll;
// AIM: where the shot line sits relative to the DRAWN gun, degrees. ANGLE
// places the gun in the hand and the shot follows it; AIM is the residual,
// because a model's +z is not always its barrel. The P38 sat right in the
// hand at ANGLE pitch 4 while its reticle, flash and burst ran 4 degrees
// above the barrel. Positive pitch = up, as ANGLE.
static VRWeaponVar s_vrAimYaw;
static VRWeaponVar s_vrAimPitch;
// ...plus the automatic barrel alignment's own contribution, see the table in
// GameClientShell.cpp: the aligned gun's barrel sits at the mirror of the
// pivot line, so the shot goes there too. Degrees, positive pitch up.
extern void VRAlignTrimForWeapon(int nWeaponId, float& fPitch, float& fYaw);
static void VRWeaponVarsInit();
static VarTrack s_vtAimFollowsAlign;
static void VRAimTotal(int nWeaponId, float& fYawDeg, float& fPitchDeg)
{
	VRWeaponVarsInit();
	if (!s_vtAimFollowsAlign.IsInitted()) s_vtAimFollowsAlign.Init(g_pLTClient, "VRAimFollowsAlignment", LTNULL, 1.0f);
	fYawDeg   = s_vrAimYaw  .Get(nWeaponId);
	fPitchDeg = s_vrAimPitch.Get(nWeaponId);
	float fP = 0.0f, fY = 0.0f;
	VRAlignTrimForWeapon(nWeaponId, fP, fY);
	const bool bFollow = s_vtAimFollowsAlign.GetFloat() > 0.0f;
	if (bFollow) { fYawDeg -= fY; fPitchDeg -= fP; }
	static int s_nSaidFor = -1; static float s_fSaidP = -999.0f;
	if (s_nSaidFor != nWeaponId || s_fSaidP != fP)
	{
		s_nSaidFor = nWeaponId; s_fSaidP = fP;
		VRLog::Msg("VRAim: weapon %d shot line yaw %+.1f pitch %+.1f = AIM trim (%+.1f %+.1f) minus alignment (%+.1f %+.1f)%s",
				   nWeaponId, fYawDeg, fPitchDeg, s_vrAimYaw.Get(nWeaponId), s_vrAimPitch.Get(nWeaponId), fY, fP,
				   bFollow ? "" : " (alignment NOT applied, VRAimFollowsAlignment 0)");
	}
}
// THE MUZZLE FLASH'S OFFSET IS PER-WEAPON, and measured rather than assumed.
// With the flash finally in the right coordinate space, what is left is a
// residual along the barrel, and it is neither constant nor proportional to the
// authored MuzzlePos - the AK47 wants about +10 units and the Sterling wants 0,
// while their authored forward figures are 16.54 and 19.30. So it belongs here,
// beside the angle trims, keyed by weapon, additive over a global.
static VarTrack    g_vtVRFlashWorld;
// VRFlashTrace: one line a frame, flash against gun. See UpdateFlash.
static VarTrack    g_vtVRFlashTrace;
static VRWeaponVar s_vrFlashOffR;
static VRWeaponVar s_vrFlashOffU;
static VRWeaponVar s_vrFlashOffF;
static LTBOOL      s_bVRWeaponVarsReady = LTFALSE;

// THE CACHE HAS TO BE DROPPED WHEN THE TUNER CREATES AN OVERRIDE.
//
// VRWeaponVar::Get resolves ONCE per weapon id and remembers which variable
// answered. At level load VRFlashOffF@ak47 does not exist yet, so it resolves to
// the global and caches that. The tuner then creates the @ak47 variable - and
// the reader carries on reading the global, so the numbers climb on screen and
// the flash does not move. Headset testing confirmed it: the tuning buttons
// did not move the muzzle flash at all.
//
// Re-Init sets m_nCachedId back to -2, which forces the next Get to resolve
// afresh and find the override.
void VRFlashOffsetsInvalidate()
{
	s_bVRWeaponVarsReady = LTFALSE;
}

// THE LEFTORIUM: a gun-frame offset on the MIRRORED gun is the same offset
// with its right-hand component the other way. The drawn gun is mirrored in
// VRPublishModels; everything placed on it goes through here.
static inline LTVector VRMirR(LTVector v)
{
	if (VRShared::SwapHands()) v.x = -v.x;
	return v;
}

static void VRWeaponVarsInit()
{
	if (s_bVRWeaponVarsReady) return;
	s_bVRWeaponVarsReady = LTTRUE;

	// These defaults are only reached if the global console variable is missing
	// too, which should not happen. They exist so a lookup failure degrades to
	// the current behaviour rather than to zero.
	s_vrWeaponDist .Init("VRWeaponDist",  2.5f);
	s_vrWeaponScale.Init("VRWeaponScale", 0.4f);
	// Angles additive: the per-weapon value is a difference, the global is the
	// common correction for all of them together. See VRWeaponVar.h.
	s_vrAngleYaw   .Init("VRAngleYaw",    0.0f, true);
	s_vrAnglePitch .Init("VRAnglePitch",  0.0f, true);
	s_vrAngleRoll  .Init("VRAngleRoll",   0.0f, true);
	s_vrAimYaw     .Init("VRAimYaw",      0.0f, true);
	s_vrAimPitch   .Init("VRAimPitch",    0.0f, true);
	// Additive, like the angles: the per-weapon value is that gun's difference
	// and the global is the common correction for all of them at once.
	s_vrFlashOffR  .Init("VRFlashOffR",   0.0f, true);
	s_vrFlashOffU  .Init("VRFlashOffU",   0.0f, true);
	s_vrFlashOffF  .Init("VRFlashOffF",   0.0f, true);
	s_vrTracerOffR .Init("VRTracerOffR",  0.0f, true);
	s_vrTracerOffU .Init("VRTracerOffU",  0.0f, true);
	s_vrTracerOffF .Init("VRTracerOffF",  0.0f, true);
	s_vrShellOffR  .Init("VRShellOffR",   0.0f, true);
	s_vrShellOffU  .Init("VRShellOffU",   0.0f, true);
	s_vrShellOffF  .Init("VRShellOffF",   0.0f, true);
	s_vrGripOffR   .Init("VRGripOffR",    0.0f, true);
	s_vrGripOffU   .Init("VRGripOffU",    0.0f, true);
	s_vrGripOffF   .Init("VRGripOffF",    0.0f, true);
	s_vrLensOffR   .Init("VRLensOffR",    0.0f, true);
	s_vrLensOffU   .Init("VRLensOffU",    0.0f, true);
	s_vrLensOffF   .Init("VRLensOffF",    0.0f, true);
	s_vrLensRad    .Init("VRLensRad",     0.0f, false);
}
#include "WeaponFXTypes.h"
#include "SurfaceFunctions.h"
#include "iltcustomdraw.h"
#include "VarTrack.h"
#include "CharacterFX.h"

extern CGameClientShell* g_pGameClientShell;
extern LTBOOL g_bInfiniteAmmo;

#define INFINITE_AMMO_AMOUNT	1000
#define INVALID_ANI				((HMODELANIM)-1)
#define DEFAULT_ANI				((HMODELANIM)0)

#define	WEAPON_KEY_SUNGLASS		"SUNGLASSES_KEY"

static uint8 g_nRandomWeaponSeed;

// Used with camera disabler gadget...

static char* s_pCamDisPieces[] =
{
	"dis1", 0
};

// Used with code decipher gadget...

static char* s_pCodeDecPieces[] =
{
	"Dec1", 0
};

namespace
{
	VarTrack	g_vtFastTurnRate;
	VarTrack	g_vtPerturbRotationEffect;
	VarTrack	g_vtPerturbIncreaseSpeed;
	VarTrack	g_vtPerturbDecreaseSpeed;
	VarTrack	g_vtPerturbWalkPercent;
	VarTrack	g_vtCameraShutterSpeed;

    LTFLOAT		m_fLastPitch = 0.0f;
    LTFLOAT		m_fLastYaw = 0.0f;

	int			m_nCurTracer = 0;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::CWeaponModel()
//
//	PURPOSE:	Initialize
//
// ----------------------------------------------------------------------- //

CWeaponModel::CWeaponModel()
{
	m_nWeaponId			= WMGR_INVALID_ID;
	m_nHolsterWeaponId	= WMGR_INVALID_ID;
	m_nAmmoId			= WMGR_INVALID_ID;
    m_hObject           = LTNULL;

    m_hSilencerModel    = LTNULL;
    m_hLaserModel       = LTNULL;
    m_hScopeModel       = LTNULL;

	m_hBreachSocket		= INVALID_MODEL_SOCKET;
	m_hLaserSocket		= INVALID_MODEL_SOCKET;
	m_hSilencerSocket	= INVALID_MODEL_SOCKET;
	m_hScopeSocket		= INVALID_MODEL_SOCKET;

    m_bHaveSilencer     = LTFALSE;
    m_bHaveLaser        = LTFALSE;
    m_bHaveScope        = LTFALSE;

	m_fBobHeight		= 0.0f;
	m_fBobWidth			= 0.0f;
	m_fFlashStartTime	= 0.0f;

	m_vFlashPos.Init();
	m_vFlashOffset.Init();

	m_fNextIdleTime			= 0.0f;
	m_nAmmoInClip			= 0;
	m_nNewAmmoInClip		= 0;
    m_bFire                 = LTFALSE;
	m_eState				= W_IDLE;
	m_eLastWeaponState		= W_IDLE;
	m_eLastFireType			= FT_NORMAL_FIRE;
    m_bCanSetLastFire       = LTTRUE;

    m_bUsingAltFireAnis         = LTFALSE;
    m_bFireKeyDownLastUpdate    = LTFALSE;

	m_nSelectAni			= INVALID_ANI;
	m_nDeselectAni			= INVALID_ANI;
	m_nReloadAni			= INVALID_ANI;

	m_nAltSelectAni			= INVALID_ANI;
	m_nAltDeselectAni		= INVALID_ANI;
	m_nAltDeselect2Ani		= INVALID_ANI;
	m_nAltReloadAni			= INVALID_ANI;

    int i;
    for (i=0; i < WM_MAX_FIRE_ANIS; i++)
	{
		m_nFireAnis[i] = INVALID_ANI;
	}

	for (i=0; i < WM_MAX_IDLE_ANIS; i++)
	{
		m_nIdleAnis[i] = INVALID_ANI;
	}

	for (i=0; i < WM_MAX_ALTFIRE_ANIS; i++)
	{
		m_nAltFireAnis[i] = INVALID_ANI;
	}

	for (i=0; i < WM_MAX_ALTIDLE_ANIS; i++)
	{
		m_nAltIdleAnis[i] = INVALID_ANI;
	}

	m_vPath.Init();
	m_vFirePos.Init();
	m_vEndPos.Init();

	m_wIgnoreFX				= 0;
	m_nRequestedWeaponId	= WMGR_INVALID_ID;
    m_bWeaponDeselected     = LTFALSE;

    m_pWeapon   = LTNULL;
    m_pAmmo     = LTNULL;

	m_fMovementPerturb		= 0.0f;

    m_bDisabled             = LTFALSE;
    m_bVisible              = LTTRUE;

	m_rCamRot.Init();
	m_vCamPos.Init();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::CWeaponModel()
//
//	PURPOSE:	Destructor
//
// ----------------------------------------------------------------------- //

CWeaponModel::~CWeaponModel()
{
	if (m_hObject)
	{
        g_pLTClient->DeleteObject(m_hObject);
        m_hObject = LTNULL;
	}

	RemoveMods();

	m_PVFXMgr.Term();
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::Init()
//
//	PURPOSE:	Initialize perturb variables
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::Init()
{
    g_vtFastTurnRate.Init(g_pLTClient, "FastTurnRate", NULL, 2.3f);

    LTFLOAT fTemp = g_pLayoutMgr->GetPerturbRotationEffect();
    g_vtPerturbRotationEffect.Init(g_pLTClient, "PerturbRotationEffect", NULL, fTemp);

	fTemp = g_pLayoutMgr->GetPerturbIncreaseSpeed();
    g_vtPerturbIncreaseSpeed.Init(g_pLTClient, "PerturbIncreaseSpeed", NULL, fTemp);

	fTemp = g_pLayoutMgr->GetPerturbDecreaseSpeed();
    g_vtPerturbDecreaseSpeed.Init(g_pLTClient, "PerturbDecreaseSpeed", NULL, fTemp);

	fTemp = g_pLayoutMgr->GetPerturbWalkPercent();
    g_vtPerturbWalkPercent.Init(g_pLTClient, "PerturbWalkPercent", NULL, fTemp);

   g_vtCameraShutterSpeed.Init(g_pLTClient, "CameraShutterSpeed", NULL, 0.3f);

    return LTTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::Create()
//
//	PURPOSE:	Create the WeaponModel model
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::Create(ILTClient* pClientDE, uint8 nWeaponId, uint8 nAmmoId,
                           uint32 dwAmmo)
{
    if (!pClientDE) return LTFALSE;

	m_nWeaponId	= nWeaponId;

	// Report which per-weapon variables answer for this weapon, every time it
	// changes, whether or not VR is running.
	//
	// Deliberately NOT inside the hand-aim block: that only runs with a tracked
	// controller, so the mapping would only ever be observable in a headset -
	// and a mapping that can only be checked in a headset is one nobody checks.
	// Flat, switching weapons, this proves the slug derivation and the override
	// lookup for every gun in the game.
	{
		VRWeaponVarsInit();
		char szSlug[64];
		VRWeaponSlugForId(m_nWeaponId, szSlug, sizeof(szSlug));

		if (szSlug[0])
		{
			const float fS = s_vrWeaponScale.Get(m_nWeaponId);
			const float fY = s_vrAngleYaw  .Get(m_nWeaponId);
			const float fP = s_vrAnglePitch.Get(m_nWeaponId);
			const float fR = s_vrAngleRoll .Get(m_nWeaponId);
			const bool  bOv = s_vrWeaponScale.UsingOverride() || s_vrAngleYaw.UsingOverride() ||
							  s_vrAnglePitch.UsingOverride() || s_vrAngleRoll.UsingOverride();
			VRLog::Msg("weapon '%s': scale %.3f, angle yaw %+.2f pitch %+.2f roll %+.2f%s",
				szSlug, fS, fY, fP, fR,
				bOv ? "   <- a per-weapon value answers for this gun" : "");
		}
	}
	m_pWeapon = g_pWeaponMgr->GetWeapon(nWeaponId);
    if (!m_pWeapon) return LTFALSE;

	m_nAmmoId = nAmmoId;
	m_pAmmo	= g_pWeaponMgr->GetAmmo(nAmmoId);
    if (!m_pAmmo) return LTFALSE;

	// Important to update this before we access PlayerStats...

	g_pInterfaceMgr->UpdateWeaponStats(nWeaponId, nAmmoId, dwAmmo);

	ResetWeaponData();
	CreateModel();
	CreateFlash();
	CreateMods();

	m_PVFXMgr.Init(m_hObject, m_pWeapon);

	InitAnimations();

	Select();	// Select the weapon

    return LTTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::ResetWeaponData
//
//	PURPOSE:	Reset weapon specific data
//
// ----------------------------------------------------------------------- //

void CWeaponModel::ResetWeaponData()
{
	m_hBreachSocket		= INVALID_MODEL_SOCKET;
	m_hLaserSocket		= INVALID_MODEL_SOCKET;
	m_hSilencerSocket	= INVALID_MODEL_SOCKET;
	m_hScopeSocket		= INVALID_MODEL_SOCKET;

    m_bHaveSilencer     = LTFALSE;
    m_bHaveLaser        = LTFALSE;
    m_bHaveScope        = LTFALSE;

	m_fBobHeight		= 0.0f;
	m_fBobWidth			= 0.0f;
	m_fFlashStartTime	= 0.0f;

	m_vFlashPos.Init();
	m_vFlashOffset.Init();

	m_nAmmoInClip			= 0;
	m_nNewAmmoInClip		= 0;
    m_bFire                 = LTFALSE;
	m_eState				= W_IDLE;
	m_eLastWeaponState		= W_IDLE;
	m_eLastFireType			= FT_NORMAL_FIRE;
    m_bCanSetLastFire       = LTTRUE;

	m_nSelectAni			= INVALID_ANI;
	m_nDeselectAni			= INVALID_ANI;
	m_nReloadAni			= INVALID_ANI;

	m_nAltSelectAni			= INVALID_ANI;
	m_nAltDeselectAni		= INVALID_ANI;
	m_nAltDeselect2Ani		= INVALID_ANI;
	m_nAltReloadAni			= INVALID_ANI;

    int i;
    for (i=0; i < WM_MAX_FIRE_ANIS; i++)
	{
		m_nFireAnis[i] = INVALID_ANI;
	}

	for (i=0; i < WM_MAX_IDLE_ANIS; i++)
	{
		m_nIdleAnis[i] = INVALID_ANI;
	}

	for (i=0; i < WM_MAX_ALTFIRE_ANIS; i++)
	{
		m_nAltFireAnis[i] = INVALID_ANI;
	}

	for (i=0; i < WM_MAX_ALTIDLE_ANIS; i++)
	{
		m_nAltIdleAnis[i] = INVALID_ANI;
	}

	m_vPath.Init();
	m_vFirePos.Init();
	m_vEndPos.Init();

	m_wIgnoreFX				= 0;
	m_nRequestedWeaponId	= WMGR_INVALID_ID;
    m_bWeaponDeselected     = LTFALSE;

	m_nAmmoInClip		= 0;
	m_nNewAmmoInClip	= 0;

    m_bHaveSilencer     = LTFALSE;
    m_bHaveLaser        = LTFALSE;
    m_bHaveScope        = LTFALSE;

    m_bUsingAltFireAnis         = LTFALSE;
    m_bFireKeyDownLastUpdate    = LTFALSE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::InitAnimations
//
//	PURPOSE:	Set the animations
//
// ----------------------------------------------------------------------- //

void CWeaponModel::InitAnimations(LTBOOL bAllowSelectOverride)
{
	if (!m_hObject) return;

    m_nSelectAni    = g_pLTClient->GetAnimIndex(m_hObject, "Select");
    m_nDeselectAni  = g_pLTClient->GetAnimIndex(m_hObject, "Deselect");
    m_nReloadAni    = g_pLTClient->GetAnimIndex(m_hObject, "Reload");

    m_nAltSelectAni     = g_pLTClient->GetAnimIndex(m_hObject, "AltSelect");
    m_nAltDeselectAni   = g_pLTClient->GetAnimIndex(m_hObject, "AltDeselect");
    m_nAltDeselect2Ani  = g_pLTClient->GetAnimIndex(m_hObject, "AltDeselect2");
    m_nAltReloadAni     = g_pLTClient->GetAnimIndex(m_hObject, "AltReload");

	char buf[30];

    int i;
    for (i=0; i < WM_MAX_IDLE_ANIS; i++)
	{
		sprintf(buf, "Idle_%d", i);
        m_nIdleAnis[i] = g_pLTClient->GetAnimIndex(m_hObject, buf);
    }


	for (i=0; i < WM_MAX_FIRE_ANIS; i++)
	{
		if (i > 0)
		{
			sprintf(buf, "Fire%d", i);
		}
		else
		{
			sprintf(buf, "Fire");
		}

        m_nFireAnis[i] = g_pLTClient->GetAnimIndex(m_hObject, buf);
	}

	for (i=0; i < WM_MAX_ALTIDLE_ANIS; i++)
	{
		sprintf(buf, "AltIdle_%d", i);
        m_nAltIdleAnis[i] = g_pLTClient->GetAnimIndex(m_hObject, buf);
	}

	for (i=0; i < WM_MAX_ALTFIRE_ANIS; i++)
	{
		if (i > 0)
		{
			sprintf(buf, "AltFire%d", i);
		}
		else
		{
			sprintf(buf, "AltFire");
		}

        m_nAltFireAnis[i] = g_pLTClient->GetAnimIndex(m_hObject, buf);
	}


	// See if there are Ammo-override animations...
	if (m_pAmmo->pAniOverrides)
	{
		// Set new animations...

		SetAmmoOverrideAnis(bAllowSelectOverride);
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::SetAmmoOverrideAnis
//
//	PURPOSE:	Set the ammo specific override animations...
//
// ----------------------------------------------------------------------- //

void CWeaponModel::SetAmmoOverrideAnis(LTBOOL bAllowSelectOverride)
{
	if (!m_hObject || !m_pAmmo || ! m_pAmmo->pAniOverrides) return;

	if (bAllowSelectOverride && m_pAmmo->pAniOverrides->szSelectAni[0])
	{
        m_nSelectAni = g_pLTClient->GetAnimIndex(m_hObject, m_pAmmo->pAniOverrides->szSelectAni);
	}

	if (m_pAmmo->pAniOverrides->szDeselectAni[0])
	{
        m_nDeselectAni = g_pLTClient->GetAnimIndex(m_hObject, m_pAmmo->pAniOverrides->szDeselectAni);
	}

	if (m_pAmmo->pAniOverrides->szReloadAni[0])
	{
        m_nReloadAni = g_pLTClient->GetAnimIndex(m_hObject, m_pAmmo->pAniOverrides->szReloadAni);
	}

    int i;
    for (i=0; i < WM_MAX_IDLE_ANIS; i++)
	{
		if (i < m_pAmmo->pAniOverrides->nNumIdleAnis)
		{
			if (m_pAmmo->pAniOverrides->szIdleAnis[i][0])
			{
                m_nIdleAnis[i] = g_pLTClient->GetAnimIndex(m_hObject, m_pAmmo->pAniOverrides->szIdleAnis[i]);
			}
		}
		else
		{
			m_nIdleAnis[i] = INVALID_ANI;
		}
	}


	for (i=0; i < WM_MAX_FIRE_ANIS; i++)
	{
		if (i < m_pAmmo->pAniOverrides->nNumFireAnis)
		{
			if (m_pAmmo->pAniOverrides->szFireAnis[i][0])
			{
                m_nFireAnis[i] = g_pLTClient->GetAnimIndex(m_hObject, m_pAmmo->pAniOverrides->szFireAnis[i]);
			}
		}
		else
		{
			m_nFireAnis[i] = INVALID_ANI;
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::Reset()
//
//	PURPOSE:	Reset the model
//
// ----------------------------------------------------------------------- //

void CWeaponModel::Reset()
{
	if (!m_hObject) return;

	RemoveModel();

	m_nWeaponId				= WMGR_INVALID_ID;
	m_nHolsterWeaponId		= WMGR_INVALID_ID;
	m_nAmmoInClip			= 0;
    m_bFire                 = LTFALSE;
	m_nRequestedWeaponId	= WMGR_INVALID_ID;
    m_bWeaponDeselected     = LTFALSE;

    m_bUsingAltFireAnis         = LTFALSE;
    m_bFireKeyDownLastUpdate    = LTFALSE;

	m_PVFXMgr.Term();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::RemoveModel()
//
//	PURPOSE:	Remove our model data member
//
// ----------------------------------------------------------------------- //

void CWeaponModel::RemoveModel()
{
	if (!m_hObject) return;

	RemoveMods();

    g_pLTClient->DeleteObject(m_hObject);
    m_hObject   = LTNULL;

	m_MuzzleFlash.Term();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::RemoveMods()
//
//	PURPOSE:	Remove our mod data members
//
// ----------------------------------------------------------------------- //

void CWeaponModel::RemoveMods()
{
	// NO MODS MEANS NO SILENCER, whether or not there was a model to delete.
	//
	// m_bHaveSilencer was cleared INSIDE the block below - only when a
	// silencer model existed - while CreateSilencer sets it to LTTRUE
	// unconditionally, even if CreateModelObject returned nothing. Once the
	// two disagree the flag is stuck true for the rest of the session, and a
	// stuck "silenced" is not cosmetic: CWeaponModel::UpdateWeaponModel does
	// not even CALL UpdateFlash for a silenced weapon, so every gun in the
	// game stops producing a muzzle flash. Measured 11 September: after the
	// P38, the revolver reported "silencer FITTED" too.
	m_bHaveSilencer = LTFALSE;

	// Remove the silencer model...

	if (m_hSilencerModel)
	{
        g_pLTClient->DeleteObject(m_hSilencerModel);
        m_hSilencerModel  = LTNULL;
		m_hSilencerSocket = INVALID_MODEL_SOCKET;
	}

	// Remove the laser model...

	if (m_hLaserModel)
	{
        g_pLTClient->DeleteObject(m_hLaserModel);
        m_hLaserModel  = LTNULL;
		m_hLaserSocket = INVALID_MODEL_SOCKET;
        m_bHaveLaser   = LTFALSE;

		m_LaserBeam.TurnOff();
	}

	// Remove the scope model...

	if (m_hScopeModel)
	{
        g_pLTClient->DeleteObject(m_hScopeModel);
        m_hScopeModel  = LTNULL;
		m_hScopeSocket = INVALID_MODEL_SOCKET;
        m_bHaveScope   = LTFALSE;
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::UpdateWeaponModel()
//
//	PURPOSE:	Update the WeaponModel state
//
// ----------------------------------------------------------------------- //

WeaponState CWeaponModel::UpdateWeaponModel(LTRotation rCamRot, LTVector vCamPos,
                                            LTBOOL bFire, FireType eFireType)
{
	if (!m_hObject) return W_IDLE;

	// Store current camera pos/rot...

	m_rCamRot = rCamRot;
	m_vCamPos = vCamPos;

	// See if we are disabled...If so don't allow any weapon stuff...

	if (m_bDisabled)
	{
		// Make sure we all the pvfxmgr to hide its stuff...
		m_PVFXMgr.Update();

		return W_IDLE;
	}


	if (bFire && m_bCanSetLastFire)
	{
		m_eLastFireType	= eFireType;
	}

	// See if we just started/stopped firing...

	if (!m_bFireKeyDownLastUpdate && bFire)
	{
		HandleFireKeyDown();
	}
	else if (m_bFireKeyDownLastUpdate && !bFire)
	{
		HandleFireKeyUp();
	}

	// Check for special zip cord case...

	LTBOOL bFireOverride = bFire;
	if (bFire && m_pAmmo->eInstDamageType == DT_GADGET_ZIPCORD)
	{
		bFireOverride = HandleZipCordFire();
	}

	// Need to set these after HandleZipCordFire...It uses the
	// m_bFireKeyDownLastUpdate...

	m_bFireKeyDownLastUpdate = bFire;
	m_eLastWeaponState		 = GetState();

	bFire = bFireOverride;

	// Selecting Alt-fire does not fire the weapon if we are using
	// alt fire animations...

	if (m_bUsingAltFireAnis && m_eLastFireType == FT_ALT_FIRE)
	{
        bFire = LTFALSE;
	}


	// VR DIAGNOSTIC: while the trigger is held, what the weapon thinks, twice a
	// second. A trigger pull that produces nothing is answered by which
	// of these refuses: the state machine, the disabled flag, the edge flags,
	// or the ammo.
	if (bFire)
	{
		static double s_fHeldSaid = -1e9;
		const double fNowH = VRLog::NowMs() / 1000.0;
		if (fNowH - s_fHeldSaid > 0.5)
		{
			s_fHeldSaid = fNowH;
			CPlayerStats* pStatsH = g_pGameClientShell->GetPlayerStats();
			VRLog::Msg("VRTrigger: held - weapon %d state %d disabled %d keyDownLast %d canSetLastFire %d m_bFire %d ammo %d",
					   (int)m_nWeaponId, (int)m_eState, (int)m_bDisabled, (int)m_bFireKeyDownLastUpdate,
					   (int)m_bCanSetLastFire, (int)m_bFire,
					   pStatsH ? pStatsH->GetAmmoCount(m_nAmmoId) : -1);
		}
	}

	// Update the state of the model...

	WeaponState eState = UpdateModelState(bFire);


	// Compute offset for WeaponModel and move the model to the
	// correct position (this is all now camera-relative)

    LTVector vOffset         = GetWeaponOffset();
    LTVector vMuzzleOffset   = GetMuzzleOffset();
    LTVector vRecoil         = m_pWeapon->vRecoil;

    LTVector vNewPos(0, 0, 0);
    LTRotation rNewRot;
	rNewRot.Init();
    g_pLTClient->SetObjectRotation(m_hObject, &rNewRot);

    LTVector vU, vR, vF;
    g_pLTClient->GetRotationVectors(&rNewRot, &vU, &vR, &vF);

    LTVector vCamU, vCamR, vCamF;
    g_pLTClient->GetRotationVectors(&m_rCamRot, &vCamU, &vCamR, &vCamF);

    vNewPos += vR * (vOffset.x + m_fBobWidth);
    vNewPos += vU * (vOffset.y + m_fBobHeight);
    vNewPos += vF * vOffset.z;

	LTVector vTemp(0, 0, 0);
	m_vFlashOffset.Init();

	m_vFlashOffset.x = (vOffset.x + vMuzzleOffset.x + m_fBobWidth);
	m_vFlashOffset.y = (vOffset.y + vMuzzleOffset.y + m_fBobHeight);
	m_vFlashOffset.z = (vOffset.z + vMuzzleOffset.z);

	vTemp = vCamR  * m_vFlashOffset.x;
	vTemp += vCamU * m_vFlashOffset.y;
	vTemp += vCamF * m_vFlashOffset.z;

	if (FiredWeapon(eState))
	{
        LTFLOAT xRand = GetRandom(-vRecoil.x, vRecoil.x);
        LTFLOAT yRand = GetRandom(-vRecoil.y, vRecoil.y);
        LTFLOAT zRand = GetRandom(-vRecoil.z, vRecoil.z);

		vNewPos += vU * yRand;
		vNewPos += vR * xRand;
		vNewPos += vF * zRand;

		m_vFlashOffset.x += yRand;
		m_vFlashOffset.y += xRand;
		m_vFlashOffset.z += zRand;

		vTemp = vCamR  * m_vFlashOffset.x;
		vTemp += vCamU * m_vFlashOffset.y;
		vTemp += vCamF * m_vFlashOffset.z;

        g_pLTClient->SetObjectPos(m_hObject, &vNewPos, LTTRUE);

		if (!m_bHaveSilencer || g_vtVRDebugNoSilencer.GetFloat() > 0.0f)
		{
			StartFlash();
		}

		// Send message to server telling player to fire...

		SendFireMsg();
	}
	else
	{
        g_pLTClient->SetObjectPos(m_hObject, &vNewPos, LTTRUE);
	}


	// --- VR: aim the weapon with the hand rather than the head ------------- //
	//
	// Applied HERE, after both branches above have set the position and before
	// the attachments update, because silencer, laser and scope derive their
	// transforms from this model's sockets and must see the final one. The
	// three places the weapon transform is set (the identity rotation earlier,
	// and a position in each branch) are all upstream of this point, so one
	// override covers them - converting them individually would have left
	// whichever was missed fighting this.
	//
	// The weapon is camera-relative, which makes this much simpler than it
	// looks: the camera already carries body*head, so expressing the hand
	// relative to the camera cancels the body rotation entirely. Only the
	// difference between hand and head is needed.
	//
	// Direction only - yaw and pitch, not roll. A barrel needs a direction,
	// and roll about the barrel is cosmetic. It also avoids reconstructing a
	// rotation from three published angles, which is exactly what went wrong
	// with head tracking before it was moved to a quaternion.
	// Say why nothing happened, ONCE per change of reason.
	//
	// This block has three silent early-outs, and every instrument upstream of
	// it - the config loading, the weapon slug resolving, the per-weapon values
	// resolving to a name - reports success whichever way they go. Three
	// correct measurements in a row are not a diagnosis when none of them
	// watches the last hop, and a report of no hands and no gun cost a test
	// round that this line would have answered from the desk.
	//
	// Per REASON, not per frame: a per-frame failure here writes gigabytes and
	// buries everything else in the log.
	{
		const bool bOn   = (g_vtVRHandAim.GetFloat() > 0.0f);
		const bool bLive = VRShared::IsLive();
		const bool bHand = bLive && (VRShared::State().Hands[1].nActive != 0);

		const int nReason = !bOn ? 1 : (!bLive ? 2 : (!bHand ? 3 : 0));

		static int s_nLastReason = -1;
		if (nReason != s_nLastReason)
		{
			s_nLastReason = nReason;
			switch (nReason)
			{
			case 1: VRLog::Msg("VR weapon: NOT placing it - VRHandAim is 0"); break;
			case 2: VRLog::Msg("VR weapon: NOT placing it - no host attached (flat run)"); break;
			case 3: VRLog::Msg("VR weapon: NOT placing it - the RIGHT controller is not tracking"); break;
			default: VRLog::Msg("VR weapon: placing it (VRHandAim %d, right controller tracking)",
						(int)g_vtVRHandAim.GetFloat()); break;
			}
		}
	}

	if (g_vtVRHandAim.GetFloat() > 0.0f && VRShared::IsLive())
	{
		const VRSharedState& s = VRShared::State();
		const VRHandState&   h = s.Hands[1];			// right hand

		if (h.nActive)
		{
			const float fD2R = 0.01745329f;

			// Hand direction relative to the view. The Z-flip that converts
			// OpenXR to LithTech inverts yaw and pitch, matching the scales
			// measured for the head.
			const float fYaw   = -(h.fYawDeg   - s.fHeadYawDeg)   * fD2R;
			const float fPitch = -(h.fPitchDeg - s.fHeadPitchDeg) * fD2R;

			// 1 = move only, 2 = rotate only, 3 = both. Split because the two
			// halves fail differently and the first attempt could not tell
			// them apart: the model rotates about its own origin, so if that
			// origin is not where the geometry sits, rotating swings the arms
			// out of frame - which matches the symptom better than any
			// translation error does.
			const int nMode = (int)g_vtVRHandAim.GetFloat();

			if (nMode == 2 || nMode == 3)
			{
				// Per-weapon angle, in degrees, on top of where the hand points.
				//
				// This is the axis on which weapons differ most: the models are
				// authored for a flat over-the-shoulder view and each sits in
				// the hand at its own angle, so one hand pose cannot hold every
				// weapon correctly. The angle is set by eye, per weapon, in the
				// tuner's ANGLE mode and saved to vrtune.cfg. Zero until tuned.
				VRWeaponVarsInit();
				// The tuner's ANGLE mode, degrees, no gate. Pitch is negated so
				// that a positive number (the Up key) tilts the barrel UP;
				// positive X rotation in this engine tilts the muzzle down.
				// THE LEFTORIUM mirrors the gun into the left hand, and the
				// mirror image of a turn or a roll is the opposite one.
				const float fMir      = VRShared::SwapHands() ? -1.0f : 1.0f;
				const float fAngYaw   =  s_vrAngleYaw  .Get(m_nWeaponId) * fD2R * fMir;
				const float fAngPitch = -s_vrAnglePitch.Get(m_nWeaponId) * fD2R;
				const float fAngRoll  =  s_vrAngleRoll .Get(m_nWeaponId) * fD2R * fMir;

				// TWO rotations, and the difference matters.
				//
				// rHand is where the hand points, and it is what swings the
				// weapon's POSITION about the eye further down. rModel is that
				// plus the per-weapon trim, and it only ever sets the model's
				// orientation.
				//
				// Folding the trim into rHand - which the first version of this
				// did - swings the position too: the p38's -48 degree yaw trim
				// carried the gun 48 degrees round the eye and out of the view
				// entirely. Reported in headset testing as no hands and no gun. It is
				// the same trap the note below already warns about, walked into
				// from the other direction.
				LTRotation rHand;
				rHand.Init();
				g_pLTClient->EulerRotateY(&rHand, fYaw);
				g_pLTClient->EulerRotateX(&rHand, fPitch);

				// THE WRIST. Turning the controller over did nothing, because
				// only yaw and pitch were ever taken from the hand - the third
				// axis was dropped and the only roll applied was the per-weapon
				// trim, a constant. A hand turns about three axes and the gun
				// in it should follow all three.
				//
				// Same Z-flip sign as the other two. Roll is a rotation about
				// the barrel, so it changes how the gun sits in the hand and
				// nothing about where it points - which is why the fire
				// direction does not want it and this does.
				// SIGN FLIPPED 8 SEPTEMBER. Headset testing: rolling the
				// controller left and right rolled the gun the opposite way -
				// the same fault the Raze port had. The comment
				// below argued this took the Z-flip sign of yaw and pitch; the
				// headset says roll does not. Yaw and pitch reverse under a
				// Z-flip because they are rotations that involve Z; roll is a
				// rotation ABOUT Z and keeps its sense.
				const float fRoll = +(h.fRollDeg - s.fHeadRollDeg) * fD2R;

				// THE MODEL'S ORIENTATION IS ABSOLUTE, NOT RELATIVE TO THE HEAD.
				//
				// Headset testing: the gun rolled correctly but did not pitch
				// up and down fully - it lifted, but short of the controller's
				// full movement. fYaw and fPitch above subtract the HEAD's angles,
				// on the theory that the renderer adds the head back on top.
				// It does not: the publish rotates these nodes by the camera
				// OBJECT's rotation, and that object never carries the head
				// pose - the fire-direction code says so in as many words and
				// uses absolute angles for exactly that reason. So the drawn
				// gun was hand MINUS head: correct with the head level, and
				// short by the head's pitch whenever the player looked where they
				// pointed, which is always. The dot was right; the gun lagged.
				//
				// Same Z-flip sign as the fire path. rHand above keeps the
				// relative form because it only swings the authored POSITION,
				// which VRGunAtHand does not use.
				const float fAbsYaw   = -h.fYawDeg   * fD2R;
				const float fAbsPitch = -h.fPitchDeg * fD2R;
				LTRotation rModel;
				rModel.Init();
				g_pLTClient->EulerRotateY(&rModel, fAbsYaw   + fAngYaw);
				// VRGunTrimPitch: degrees, one number for the whole hand rig.
				// The barrel sits pitched up relative to the aim pose - in the
				// headset it sat in the hand tilted higher than it should -
				// because the model's barrel is not along the wrist bone's
				// axis. Positive tilts the muzzle down.
				// ...UNLESS THE BARREL IS ALIGNED PER WEAPON (VRGunAutoTrim,
				// GameClientShell's rest capture): one number fitted the
				// Walther and put the sub-machine gun's barrel below its aim.
				g_pLTClient->EulerRotateX(&rModel, fAbsPitch + fAngPitch
										  + ((g_vtVRGunAutoTrim.GetFloat() > 0.0f) ? 0.0f
											 : g_vtVRGunTrimPitch.GetFloat() * fD2R));

				// Roll is applied here and not to the head. Head roll is
				// dropped elsewhere because it is cosmetic and rebuilding it
				// from Euler angles has warped the world before; this is a
				// fixed placement offset for one model, which is a different
				// thing entirely.
				const float fRollTotal = fAngRoll
					+ ((g_vtVRHandRoll.GetFloat() > 0.0f) ? fRoll : 0.0f);
				if (fRollTotal != 0.0f)
					g_pLTClient->EulerRotateZ(&rModel, fRollTotal);

				g_pLTClient->SetObjectRotation(m_hObject, &rModel);

				// Swing the weapon about the EYE, not about its own origin.
				//
				// Rotating in place turns the model around whatever point the
				// artist happened to use as its origin, which is not the grip -
				// so the arms swung out of frame and could only be found by
				// putting the controller on a desk and looking away.
				//
				// Carrying the authored offset through the same rotation keeps
				// the gun at exactly the distance from the face the game
				// intended, so it can never leave the view, while still
				// pointing where the hand points. That is also why free
				// positional tracking was abandoned here: an inch of hand
				// movement threw the model off screen, and no scale factor
				// fixes something that can leave the frame at all.
				LTVector vHU, vHR, vHF;
				g_pLTClient->GetRotationVectors(&rHand, &vHU, &vHR, &vHF);

				// Push the weapon further from the eye. The player reports the
				// silencer alone filling the screen top to bottom, which is
				// backwards from what the field of view predicts: the world is
				// drawn at 120 degrees vertical where the flat game uses 78, so
				// at an unchanged distance the gun should look SMALLER, not
				// larger.
				//
				// That mismatch suggests the weapon is drawn with a different
				// field from the world - which would make it about twice its
				// correct size relative to everything else, and is the thing to
				// establish before treating distance as the real fix. Until
				// then this is an honest workaround, not an explanation.
				VRWeaponVarsInit();
				const float fDistRaw = s_vrWeaponDist.Get(m_nWeaponId);
				const float fDist = (fDistRaw > 0.1f) ? fDistRaw : 1.0f;

				LTVector vSwung = (vHR * vNewPos.x + vHU * vNewPos.y + vHF * vNewPos.z) * fDist;
				g_pLTClient->SetObjectPos(m_hObject, &vSwung, LTTRUE);

				// Where the weapon actually ended up, once per weapon.
				//
				// A report of no hands and no gun is not one anyone can act
				// on without knowing whether the model is off to the side,
				// behind the eye, or simply too small to notice. The offset is
				// relative to the camera, so a large sideways or backwards
				// component means it left the view rather than failed to draw.
				{
					// Per WEAPON *and* per change of hand angle. Keyed on the
					// weapon alone, an A/B that holds the controller at two
					// different angles logs the first arm and stays silent for
					// the second - so the one comparison the line exists to
					// support is the one it cannot make.
					// AND NOT MORE THAN FOUR TIMES A SECOND. "Only when the
					// angle CHANGES" reads like a cap and is not one: in VR the
					// hand is always moving, and a hand turning at even 45 deg/s
					// crosses half a degree inside one frame. Measured over the
					// 15-minute soak, this line and its partner in GetFireInfo
					// wrote 46,509 records each - 52 a second, 93,000 of the
					// run's 108,000 lines - which is a per-frame file write in
					// the middle of a headset session.
					//
					// A quarter-second throttle keeps every trend these lines
					// exist to show and drops them by thirteen times.
					static int    s_nPosLoggedFor = -2;
					static float  s_fLoggedYaw = -9999.0f;
					static double s_fPosLoggedAt = -1.0;
					const double fNowPos = g_pLTClient->GetTime();
					if ((m_nWeaponId != s_nPosLoggedFor
							|| fabsf(h.fYawDeg - s_fLoggedYaw) > 0.5f)
						&& (fNowPos - s_fPosLoggedAt) > 0.25)
					{
						s_nPosLoggedFor = m_nWeaponId;
						s_fLoggedYaw = h.fYawDeg;
						s_fPosLoggedAt = fNowPos;
						// vSwung IS ALREADY CAMERA-RELATIVE - the line above
						// this block says so in the retail path's own words,
						// "this is all now camera-relative" - so subtracting
						// the world camera position from it, which is what
						// this used to do, measures the camera's distance from
						// the map origin and calls it the weapon's offset from
						// the eye. It reported the gun 7000 units away and
						// "BEHIND THE EYE" on a frame where it was drawn
						// perfectly well, which is a diagnosis nobody could act
						// on and one I acted on for twenty minutes.
						const float fFwd = vSwung.z;
						VRLog::Msg("  weapon at x %+.1f y %+.1f z %+.1f units"
							" camera-relative (dist x%.2f, scale x%.3f)"
							"  | hand y%+.1f p%+.1f vs head y%+.1f p%+.1f%s",
							vSwung.x, vSwung.y, vSwung.z, fDist,
							s_vrWeaponScale.Get(m_nWeaponId),
							h.fYawDeg, h.fPitchDeg,
							s.fHeadYawDeg, s.fHeadPitchDeg,
							(fFwd <= 0.0f) ? "   <- behind the eye" : "");
					}
				}

				// Shrink the model.
				//
				// Distance turned out not to be the lever: pushing the weapon
				// 2.5x further away changed its apparent size not at all, which
				// cannot happen under a perspective projection. Taken with the
				// rotation working correctly - so it IS this object - and mode
				// 1 having thrown it off screen - so position IS applied - the
				// only explanation left is that the model is simply enormous.
				//
				// That is normal for a player-view weapon of this era: authored
				// oversized and placed very close, because the flat game draws
				// it through a narrow field where that reads correctly. A
				// headset's 120 degree field does not, so it has to come down
				// in size rather than move away.
				const float fScaleRaw = s_vrWeaponScale.Get(m_nWeaponId);
				const float fScale = (fScaleRaw > 0.01f) ? fScaleRaw : 1.0f;
				LTVector vScale(fScale, fScale, fScale);
				g_pLTClient->SetObjectScale(m_hObject, &vScale);

				// The attachments are SEPARATE objects, not part of the weapon
				// mesh. Scaling only m_hObject shrank the arms and left the
				// silencer at full size, filling the screen on its own - which
				// is what the player saw and what finally identified this.
				if (m_hSilencerModel) g_pLTClient->SetObjectScale(m_hSilencerModel, &vScale);
				if (m_hLaserModel)    g_pLTClient->SetObjectScale(m_hLaserModel,    &vScale);
				if (m_hScopeModel)    g_pLTClient->SetObjectScale(m_hScopeModel,    &vScale);
			}

			// OFFSET from where the game already puts the weapon, not a
			// replacement for it.
			//
			// Replacing it outright put the model's origin at the hand, which
			// sits well below the view axis - the gun vanished off the bottom
			// of the screen unless the hand was raised above the head, and what
			// remained was badly stretched, because the edge of a 120 degree
			// rectilinear projection distorts severely. The game's own offset
			// is authored to sit the weapon correctly in frame, and the model's
			// origin is not its grip, so that offset has to be kept.
			//
			// A neutral resting pose is subtracted so the delta is zero when
			// the hand is where the flat game assumes it is: roughly 35 cm
			// below and 35 cm in front of the head, a little to the right.
			// Moving the hand from there moves the gun from its normal place.
			// 58.75 IS THE WORLD'S SCALE AND THIS IS NOT WORLD SPACE.
			//
			// 1 unit = 17.02 mm is correct for the level - it comes from a
			// 1600 mm eye height over 94 units - and it is wrong here by more
			// than an order of magnitude, because the weapon is positioned
			// CAMERA-RELATIVE in a space the retail path scales for itself.
			// Measured at the desk with a fake controller:
			//
			//   authored weapon offset   (+0.4, -0.5, +1.3)  ~1.5 units total
			//   10 cm of hand movement   +5.9 units          4x the whole offset
			//
			// So an inch of hand travel threw the model off screen, which is
			// exactly what the note below says happened and why positional
			// tracking was abandoned. It was never the idea that was wrong.
			//
			// WHAT THE RIGHT NUMBER IS, derived rather than fitted: the
			// authored offset is the game's own statement of where a
			// hand-held weapon sits relative to the eye. Taking that as an arm
			// at roughly 45 cm forward, 15 cm right and 17 cm down gives
			// 1.3/0.45 = 2.9, 0.4/0.15 = 2.7 and 0.5/0.17 = 2.9 units per
			// metre - three axes agreeing on about 2.8, against 58.75 in use.
			//
			// A CVAR, because that derivation assumes an arm's length and only
			// a headset can say whether it feels 1:1. VRHandPosScale 58.75
			// restores the old behaviour exactly.
			const float fUnitsPerMetre = (g_vtVRHandPosScale.GetFloat() > 0.0f)
				? g_vtVRHandPosScale.GetFloat() : 3.0f;
			// THE REST POSE IS ONLY FOR THE OFFSET MODEL.
			//
			// Two placements. AT the controller and nowhere else: a held gun
			// has no authored screen offset, the controller is where the hand
			// is, and that is where the gun goes.
			//
			// The OFFSET placement keeps the flat game's authored camera-relative
			// offset and swings the weapon around the EYE, because an early
			// attempt at putting the origin at the hand sent the gun off the
			// bottom of the screen. That attempt used 58.75 units per metre -
			// the WORLD's scale - in a space that needs about 3, so an inch of
			// hand movement moved the gun by more than its whole offset. The
			// idea was not what failed.
			//
			// VRGunAtHand 1 is the first: the hand's displacement from the
			// head IS the position, with no authored offset and no rest pose to
			// subtract, because there is nothing to be a delta from.
			const bool bAtHand = (g_vtVRGunAtHand.GetFloat() > 0.0f);
			const float fRestX = bAtHand ? 0.0f :  0.15f;
			const float fRestY = bAtHand ? 0.0f : -0.35f;
			const float fRestZ = bAtHand ? 0.0f : -0.35f;

			LTVector vHand;
			vHand.x = ((h.fPosX - s.fHeadPosX) - fRestX) * fUnitsPerMetre;
			vHand.y = ((h.fPosY - s.fHeadPosY) - fRestY) * fUnitsPerMetre;
			vHand.z = -((h.fPosZ - s.fHeadPosZ) - fRestZ) * fUnitsPerMetre;	// Z flip

			// NO HEAD IN THIS BASIS. The camera object carries the body's
			// rotation here (the head is applied inside RenderWorldEyes and
			// put back), so the hand's tracking-space offset maps straight
			// onto it - the same way the fire direction maps the hand's
			// absolute angles. The head-yaw undo this used to do was wrong by
			// the head yaw. Kept only for VRHeadAsMouse, where the body IS
			// driven by the head. See the publish path in GameClientShell.
			const float fHeadYaw = (g_vtVRHeadAsMouse.GetFloat() > 0.0f)
				? s.fHeadYawDeg * fD2R : 0.0f;
			const float fC = (float)cos(fHeadYaw), fS = (float)sin(fHeadYaw);

			if (nMode == 1 || nMode == 3)
			{
				// AT the hand, or offset FROM where the flat game put it.
				LTVector vLocal = bAtHand ? LTVector(0.0f, 0.0f, 0.0f) : vNewPos;
				vLocal.x += vHand.x * fC - vHand.z * fS;
				vLocal.y += vHand.y;
				vLocal.z += vHand.x * fS + vHand.z * fC;

				g_pLTClient->SetObjectPos(m_hObject, &vLocal, LTTRUE);

				// CAN THE NODES BE READ BACK HERE, right after placement?
				//
				// At PUBLISH time they come back (0,0,0) with the object at
				// (0,0,0) too, so the view weapon draws at the map origin. The
				// publish runs from Update() and the placement from
				// UpdatePlaying, which is later - so the question is whether
				// stashing the pose HERE and publishing it next frame would
				// carry a real transform, or whether the engine has not posed
				// the skeleton at this point either.
				{
					static float s_fSaidPose = -99999.0f;
					HMODELNODE hN = INVALID_MODEL_NODE, hNext = INVALID_MODEL_NODE;
					if (g_pLTClient->GetNextModelNode(m_hObject, hN, &hNext) == LT_OK)
					{
						LTransform tfw;
						if (g_pLTClient->GetModelLT()->GetNodeTransform(
								m_hObject, hNext, tfw, LTTRUE) == LT_OK
							&& fabsf(tfw.m_Pos.x - s_fSaidPose) > 0.01f)
						{
							s_fSaidPose = tfw.m_Pos.x;
							VRLog::Msg("  view weapon node0 AT PLACEMENT"
								" %+.2f %+.2f %+.2f", tfw.m_Pos.x,
								tfw.m_Pos.y, tfw.m_Pos.z);
						}
					}
				}

				// THE POSITION THAT IS ACTUALLY WRITTEN, which the line in the
				// rotation block above is not: that one prints vSwung, and
				// this block overwrites it. An A/B that moved the hand 10 cm
				// showed no change at all in the log for exactly that reason,
				// while the thing being measured was never printed.
				//
				// vHand is the hand's displacement from its resting pose in
				// game units, so the two together say how far a centimetre of
				// real movement travels on screen.
				{
					static float s_fSaidX = -99999.0f;
					if (fabsf(vHand.x - s_fSaidX) > 0.05f)
					{
						s_fSaidX = vHand.x;
						// WHICH OBJECT, because writing a position is not the
						// same as moving the thing on screen. A hand offset of
						// 167 units - the gun thrown right out of the frame -
						// left the picture byte-identical, so this path has
						// never reached what is drawn. The handle is what lets
						// that be compared against the renderer's own list.
						VRLog::Msg("  weapon POSITION written on object %08X"
							" x %+.1f y %+.1f"
							" z %+.1f  (authored %+.1f %+.1f %+.1f + hand"
							" %+.1f %+.1f %+.1f units)",
							(unsigned)(uintptr_t)m_hObject,
							vLocal.x, vLocal.y, vLocal.z,
							vNewPos.x, vNewPos.y, vNewPos.z,
							vHand.x, vHand.y, vHand.z);
					}
				}
			}
		}
	}

	m_vFlashPos = m_vCamPos + vTemp;
	// THE MUZZLE IS ON THE GUN IN THE HAND, NOT ON THE FACE. m_vFlashPos is
	// the camera plus the authored camera-relative muzzle offset, which is
	// where the retail gun's muzzle sits - in front of the eye. Every effect
	// that starts at the muzzle takes it: the flash, the shell casings, and
	// the tracer and impact FX, because it is written into the fire message
	// for the server. With the gun placed at the hand they all came from the
	// face (the bullet trail, shell casing and
	// muzzle flash all came from roughly the middle of the face).
	// The same re-basing the camera-relative effects use, on the same
	// camera-relative point, against the last published gun pose.
	if (g_vtVRGunAtHand.GetFloat() > 0.0f && VRShared::IsLive() && VRPrims_RebaseEverKnown())
	{
		const LTVector vWasFlash = m_vFlashPos;
		// THE MUZZLE'S OFFSET FROM THE GUN IS vMuzzleOffset, IN GAME UNITS.
		//
		// m_vFlashOffset is vOffset + vMuzzleOffset + bob - the muzzle measured
		// from the CAMERA - and it used to be handed to VRPrims_RebasePointLast,
		// which is for the engine's compressed camera space and multiplies by
		// fK (VRViewModelScale, 17). A ~14-unit authored offset came out ~240
		// units away, laid along the gun's own axes: in the headset, 19 September,
		// about 13 feet away to the right and near the ground.
		//
		// The gun is placed at vOffset and the muzzle at vOffset + vMuzzleOffset,
		// so the difference is vMuzzleOffset alone; the bob applies to both and
		// cancels. No scaling: it is already in the units the world is in.
		// THE DRAWN BARREL END FIRST, and an authored offset only if the gun
		// has not been through the node walk yet.
		//
		// The offset route has to agree with the gun's rotation, its published
		// origin AND the K*S its nodes are scaled by, all at once. Getting the
		// scale wrong put this thirteen feet out; getting the origin wrong left
		// it a few inches low with the trail starting at the gun's body instead
		// of its barrel. The drawn node has
		// already been through all three.
		// THE AUTHORED OFFSET, NOT THE DRAWN NODES. Reverted 19 September.
		//
		// Taking the muzzle from the gun's furthest drawn node looked better and
		// is not general: it depends on the mesh having nodes SPREAD along the
		// barrel, and they often do not. Measured at the desk with the arm nodes
		// excluded - every one of the ak47's gun nodes sits at the SAME point,
		// 0.0 units of spread, so the "barrel end" came back as the gun's own
		// origin. The Sterling has 18.9 units of spread and worked, which is
		// exactly why it passed here and failed on the tester's weapons.
		//
		// The tester judged the two builds: the authored offset sat a little lower
		// than the gun; the drawn node sat below and behind the gun. The
		// authored figure is per-weapon, always present, and was the better of
		// the two, so it is what ships.
		const LTVector vMuzzleFromGun = VRMirR(GetMuzzleOffset());	// Leftorium: the mirrored gun's muzzle
		LTVector vDrawn;
		bool bDrawn = false;
		(void)vDrawn;
		// AND SANITY-CHECK IT AGAINST THE GUN. The node walk is a frame behind,
		// so the very first sample of a session - and the first after a level
		// change - can still hold the previous set, which sits near the map
		// origin: 64 units from the gun where every settled sample reads 18-24
		// (measured at the desk, 19 September). A muzzle further from the gun's
		// own origin than any gun is long is not a muzzle, and firing on that
		// frame would throw the flash, the tracer and the casing across the map
		// exactly the way the x17 bug did.
		if (bDrawn)
		{
			// AGAINST THE CAMERA, not against the gun. Checking it against the
			// gun's own pose looks right and catches nothing on the frame that
			// matters: on the first frame of a session BOTH are stale and near
			// the map origin, so they agree with each other while being
			// thousands of units from the player (measured, 19 September - the
			// gun-relative check passed a muzzle at 7 -22 26). The eye is the
			// one position that is never stale. Nothing held in a hand is two
			// hundred units from the face.
			const LTVector vChk = vDrawn - m_vCamPos;
			if (vChk.Mag() > 200.0f) bDrawn = false;
		}
		// THE SAME SPACE THE BULLETS ARE IN. This is the flash on the carpet by
		// the fountain, in several copies - the tester watched it, photographed it,
		// and was right about it.
		//
		// VRPrims_GunPointFromOffsetLast builds the muzzle in the DRAWN gun's
		// frame, and that frame is published to our renderer rather than being
		// world coordinates. A point from it lands somewhere fixed in the level
		// and stays there, which is precisely what a flash lying on the floor in
		// three or four copies looks like. The tracer had the identical fault
		// and was fixed by leaving that space entirely.
		//
		// GetFireInfo's vFirePos is TRUE world - the bullets land where the player aims,
		// so it has to be - and the authored per-weapon MuzzlePos goes in the
		// basis it hands back. That is the same construction the tracer uses, so
		// the flash and the trail now come out of one point by construction,
		// which is the disagreement that started all of this.
		{
			// THE POSITION FROM THE SHOT, THE ROTATION FROM THE DRAWN GUN.
			//
			// The first version of this used GetFireInfo's basis for both, and
			// The tester caught it at once: moving the gun at all left the flash
			// wandering instead of travelling with it. The tester's clip shows the
			// flash on the muzzle while the gun is level and off to one side as
			// soon as it turns.
			//
			// Two different rotations were in play. The fire basis is the HAND's
			// aim. The drawn gun uses GetModelRot(), which is the hand plus the
			// per-weapon yaw/pitch/roll trims - autoexec.cfg carries
			// VRWeaponPitch@ak47 -16.57 and VRWeaponRoll@ak47 among others. The
			// two agree at one angle and separate at every other, so the flash
			// swung away from the barrel exactly when the player moved.
			//
			// The flash has to sit on the gun you can SEE, so it takes the gun's
			// own rotation. The position still comes from vFirePos, which is
			// true world and is where the gun is drawn.
			// VRFlashWorld picks which construction is used, so the two can be
			// compared in one session instead of one build apart.
			//   1  the drawn gun's own frame (VRPrims_GunPointFromOffsetLast)
			//   0  the fire position plus the drawn gun's rotation
			// The second tracks the gun while it is level and drifts as it
			// turns, because the publish path anchors the gun's MESH onto the
			// hand rather than its authored origin, so vFirePos is not the
			// drawn gun's origin and the gap is per weapon.
			if (!g_vtVRFlashWorld.IsInitted())
				g_vtVRFlashWorld.Init(g_pLTClient, "VRFlashWorld", LTNULL, 1.0f);
			if (!g_vtVRFlashTrace.IsInitted())
				g_vtVRFlashTrace.Init(g_pLTClient, "VRFlashTrace", LTNULL, 0.0f);

			LTVector vFU, vFR, vFF, vFirePos;
			// THE PUBLISH'S OWN MUZZLE FIRST. VRPublishModels computes it from
			// the gun pose it is establishing THIS frame, so it carries no
			// one-frame lag; GunPointFromOffsetLast reads last frame's pose and
			// lags by r * dTheta while the gun turns, which is the drift.
			LTVector vMuzNow;
			if (g_vtVRFlashWorld.GetFloat() > 0.0f && VRPrims_DrawnMuzzle(vMuzNow))
			{
				m_vFlashPos = vMuzNow;
			}
			else if (g_vtVRFlashWorld.GetFloat() > 0.0f)
			{
				m_vFlashPos = VRPrims_GunPointFromOffsetLast(vMuzzleFromGun);
			}
			else if (GetFireInfo(vFU, vFR, vFF, vFirePos))
			{
				// VRGunRot, not GetModelRot - the same fault as the tuner
				// offsets had. This is the VRFlashWorld 0 arm and is not the
				// default, but leaving a known-wrong frame in a fallback is how
				// it gets found again the hard way.
				m_vFlashPos = vFirePos
							+ VRPrims_GunFrameToWorldLast(VRGunRot(), vMuzzleFromGun);
			}
			else
			{
				m_vFlashPos = VRPrims_GunPointFromOffsetLast(vMuzzleFromGun);
			}
		}

		// ONE MUZZLE POINT FOR EVERYTHING. The muzzle FLASH object is placed in
		// VRPublishModels from VRPrims_DrawnMuzzle, so it has to be told the
		// same point the tracer uses or the two disagree - which is how the
		// trail came out of the barrel while the muzzle flash did not.
		VRPrims_NoteDrawnMuzzle(m_vFlashPos);
		static int s_nSaidFlash = 0;
		if (s_nSaidFlash < 4)
		{
			++s_nSaidFlash;
			const LTVector vFromGun = m_vFlashPos - VRPrims_RebaseLast().vGunWorld;
			VRLog::Msg("VRFlash: muzzle from the face (%.0f %.0f %.0f) to %s"
				" (%.0f %.0f %.0f); %.1f units from the gun's origin"
				" (authored offset would have been %.1f)",
				vWasFlash.x, vWasFlash.y, vWasFlash.z,
				bDrawn ? "THE DRAWN BARREL END" : "an authored offset (no node walk yet)",
				m_vFlashPos.x, m_vFlashPos.y, m_vFlashPos.z,
				vFromGun.Mag(), vMuzzleFromGun.Mag());
		}
	}


	// Update the muzzle flash...

	if (!g_vtVRDebugNoSilencer.IsInitted())
		g_vtVRDebugNoSilencer.Init(g_pLTClient, "VRDebugNoSilencer", NULL, 0.0f);
	if (!m_bHaveSilencer || g_vtVRDebugNoSilencer.GetFloat() > 0.0f)
	{
		UpdateFlash(eState);
	}


	// Update the mods...

	UpdateMods();


	// Update any player-view fx...

	m_PVFXMgr.Update();


	UpdateMovementPerturb();


	// AutoSelectWeapon

	if (GetState() == W_FIRING_NOAMMO)
	{
		AutoSelectWeapon();
	}

	return eState;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::UpdateBob()
//
//	PURPOSE:	Update WeaponModel bob
//
// ----------------------------------------------------------------------- //

void CWeaponModel::UpdateBob(LTFLOAT fWidth, LTFLOAT fHeight)
{
	m_fBobWidth  = fWidth;
	m_fBobHeight = fHeight;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::UpdateMods()
//
//	PURPOSE:	Update WeaponModel bob
//
// ----------------------------------------------------------------------- //

void CWeaponModel::UpdateMods()
{
	if (!m_hObject) return;

    LTVector vPos = GetModelPos();

    LTRotation rRot;
    uint32 dwFlags;

	// Update the silencer...

	if (m_hSilencerModel)
	{
		if (m_bHaveSilencer && m_hSilencerSocket != INVALID_MODEL_SOCKET)
		{
			LTransform transform;
            if (g_pModelLT->GetSocketTransform(m_hObject, m_hSilencerSocket, transform, LTTRUE) == LT_OK)
			{
				g_pTransLT->Get(transform, vPos, rRot);
                g_pLTClient->SetObjectPos(m_hSilencerModel, &vPos, LTTRUE);
                g_pLTClient->SetObjectRotation(m_hSilencerModel, &rRot);
			}
		}
		else
		{
			// Keep the model close to us...

            g_pLTClient->SetObjectPos(m_hSilencerModel, &vPos, LTTRUE);

			// Hide model...

            dwFlags = g_pLTClient->GetObjectFlags(m_hSilencerModel);
            g_pLTClient->SetObjectFlags(m_hSilencerModel, dwFlags & ~FLAG_VISIBLE);
		}
	}


	// Update the laser...

	if (m_hLaserModel)
	{
		if (m_bHaveLaser && m_hLaserSocket != INVALID_MODEL_SOCKET)
		{
			LTransform transform;
            if (g_pModelLT->GetSocketTransform(m_hObject, m_hLaserSocket, transform, LTTRUE) == LT_OK)
			{
				g_pTransLT->Get(transform, vPos, rRot);
                g_pLTClient->SetObjectPos(m_hLaserModel, &vPos, LTTRUE);
                g_pLTClient->SetObjectRotation(m_hLaserModel, &rRot);

				// Update the laser beam...

                m_LaserBeam.Update(vPos, &rRot, LTFALSE);
			}
		}
		else
		{
			// Keep the model close to us...

            g_pLTClient->SetObjectPos(m_hLaserModel, &vPos, LTTRUE);

			// Hide model...

            dwFlags = g_pLTClient->GetObjectFlags(m_hLaserModel);
            g_pLTClient->SetObjectFlags(m_hLaserModel, dwFlags & ~FLAG_VISIBLE);

			m_LaserBeam.TurnOff();
		}
	}


	// Update the scope...

	if (m_hScopeModel)
	{
		if (m_bHaveScope && m_hScopeSocket != INVALID_MODEL_SOCKET)
		{
			LTransform transform;
            if (g_pModelLT->GetSocketTransform(m_hObject, m_hScopeSocket, transform, LTTRUE) == LT_OK)
			{
				g_pTransLT->Get(transform, vPos, rRot);
                g_pLTClient->SetObjectPos(m_hScopeModel, &vPos, LTTRUE);
                g_pLTClient->SetObjectRotation(m_hScopeModel, &rRot);
			}
		}
		else
		{
			// Keep the model close to us...

            g_pLTClient->SetObjectPos(m_hScopeModel, &vPos, LTTRUE);

			// Hide model...

            dwFlags = g_pLTClient->GetObjectFlags(m_hScopeModel);
            g_pLTClient->SetObjectFlags(m_hScopeModel, dwFlags & ~FLAG_VISIBLE);
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::Disable()
//
//	PURPOSE:	Disable/Enable the weapon
//
// ----------------------------------------------------------------------- //

void CWeaponModel::Disable(LTBOOL bDisable)
{
    LTBOOL bOldVisibility = m_bVisible;

	// Let the client shell handle the weapon being disabled...

	g_pGameClientShell->HandleWeaponDisable(bDisable);

	// Handle Gadget Disable...

	GadgetDisable(bDisable);

	if (bDisable)
	{
		// Force weapon invisible...

        SetVisible(LTFALSE);

		// Reset our data member for when the weapon is re-enabled...

		m_bVisible = bOldVisibility;

		// Must set this AFTER call to SetVisible()

        m_bDisabled = LTTRUE;
	}
	else
	{
		// Must set this BEFORE the call to SetVisible()

        m_bDisabled = LTFALSE;

		// Set the visibility back to whatever it was...

		SetVisible(m_bVisible);
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GadgetDisable()
//
//	PURPOSE:	Disable/Enable the gadget
//
// ----------------------------------------------------------------------- //

void CWeaponModel::GadgetDisable(LTBOOL bDisable)
{
	if (!m_pAmmo) return;

	// Handle sunglasses gadget...

	if (m_pAmmo->eType == GADGET)
	{
		if (bDisable)
		{
			if (m_pAmmo->eInstDamageType == DT_GADGET_CAMERA)
			{
				g_pInterfaceMgr->SetSunglassMode(SUN_NONE);
			}
			else if (m_pAmmo->eInstDamageType == DT_GADGET_MINE_DETECTOR)
			{
				g_pInterfaceMgr->SetSunglassMode(SUN_NONE);
			}
			else if (m_pAmmo->eInstDamageType == DT_GADGET_INFRA_RED)
			{
				g_pInterfaceMgr->SetSunglassMode(SUN_NONE);
			}
			else
			{
				g_pInterfaceMgr->SetSunglassMode(SUN_NONE);
			}
		}
		else  // Enable...
		{
			if (m_pAmmo->eInstDamageType == DT_GADGET_CAMERA)
			{
				g_pInterfaceMgr->SetSunglassMode(SUN_CAMERA);
			}
			else if (m_pAmmo->eInstDamageType == DT_GADGET_MINE_DETECTOR)
			{
				g_pInterfaceMgr->SetSunglassMode(SUN_MINES);
			}
			else if (m_pAmmo->eInstDamageType == DT_GADGET_INFRA_RED)
			{
				g_pInterfaceMgr->SetSunglassMode(SUN_IR);
			}
			else
			{
				g_pInterfaceMgr->SetSunglassMode(SUN_NONE);
			}
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::SetVisible()
//
//	PURPOSE:	Hide/Show the weapon model
//
// ----------------------------------------------------------------------- //

void CWeaponModel::SetVisible(LTBOOL bVis)
{
	if (!m_hObject) return;

	// Set the visible/invisible data member even if we are disabled.
	// The Disabled() function will make sure the weapon is visible/invisible
	// if SetVisible() was called while the weapon was disabled...

	m_bVisible = bVis;

	if (m_bDisabled) return;


	// Hide/Show weapon model...

    uint32 dwFlags = g_pLTClient->GetObjectFlags(m_hObject);
	if (bVis)
	{
		dwFlags |= FLAG_VISIBLE;
	}
	else
	{
		dwFlags &= ~FLAG_VISIBLE;
	}

    g_pLTClient->SetObjectFlags(m_hObject, dwFlags);


	// Always hide the flash (it will be shown when needed)...

	m_MuzzleFlash.Hide();


	// Hide/Show silencer...

	if (m_hSilencerModel)
	{
        dwFlags = g_pLTClient->GetObjectFlags(m_hSilencerModel);
		if (bVis && m_bHaveSilencer)
		{
			dwFlags |= FLAG_VISIBLE;
		}
		else
		{
			dwFlags &= ~FLAG_VISIBLE;
		}

        g_pLTClient->SetObjectFlags(m_hSilencerModel, dwFlags);
	}


	// Hide/Show laser...

	if (m_hLaserModel)
	{
        dwFlags = g_pLTClient->GetObjectFlags(m_hLaserModel);
		if (bVis && m_bHaveLaser)
		{
			dwFlags |= FLAG_VISIBLE;
		}
		else
		{
			dwFlags &= ~FLAG_VISIBLE;
		}

        g_pLTClient->SetObjectFlags(m_hLaserModel, dwFlags);
	}

	if (bVis && m_bHaveLaser)
	{
		m_LaserBeam.TurnOn();
	}
	else
	{
		m_LaserBeam.TurnOff();
	}


	// Hide/Show scope...

	if (m_hScopeModel)
	{
        dwFlags = g_pLTClient->GetObjectFlags(m_hScopeModel);
		if (bVis && m_bHaveScope)
		{
			dwFlags |= FLAG_VISIBLE;
		}
		else
		{
			dwFlags &= ~FLAG_VISIBLE;
		}

        g_pLTClient->SetObjectFlags(m_hScopeModel, dwFlags);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::CreateFlash
//
//	PURPOSE:	Create the muzzle flash
//
// ----------------------------------------------------------------------- //

void CWeaponModel::CreateFlash()
{
	if (!m_pWeapon) return;

	// Setup Breach socket (if it exists)...

	m_hBreachSocket = INVALID_MODEL_SOCKET;
	if (m_hObject)
	{
		if (g_pModelLT->GetSocket(m_hObject, "Breach", m_hBreachSocket) != LT_OK)
		{
			m_hBreachSocket = INVALID_MODEL_SOCKET;
		}
	}

	MUZZLEFLASHCREATESTRUCT mf;

    mf.bPlayerView  = LTTRUE;
	mf.hParent		= m_hObject;
	mf.pWeapon		= m_pWeapon;
	mf.vPos			= GetModelPos();
	mf.rRot			= GetModelRot();

	m_MuzzleFlash.Setup(mf);
	m_MuzzleFlash.Hide();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::CreateModel
//
//	PURPOSE:	Create the weapon model
//
// ----------------------------------------------------------------------- //

void CWeaponModel::CreateModel()
{
	if (!m_pWeapon) return;

	ObjectCreateStruct createStruct;
	INIT_OBJECTCREATESTRUCT(createStruct);

	SAFE_STRCPY(createStruct.m_Filename, m_pWeapon->szPVModel);
	SAFE_STRCPY(createStruct.m_SkinNames[0], m_pWeapon->szPVSkin);

    // Figure out what hand skin to use...

	ModelStyle eModelStyle = eModelStyleDefault;
	CCharacterFX* pCharFX = g_pGameClientShell->GetMoveMgr()->GetCharacterFX();
	if (pCharFX)
	{
		eModelStyle = pCharFX->GetModelStyle();
	}

	if (g_pGameClientShell->GetGameType() == SINGLE || g_pGameClientShell->GetMoveMgr()->IsPlayerModel())
	{
		SAFE_STRCPY(createStruct.m_SkinNames[1], g_pModelButeMgr->GetHandsSkinFilename(eModelStyle));
	}
	else
	{
		SAFE_STRCPY(createStruct.m_SkinNames[1], "guns\\skins_pv\\MultiHands_pv.dtx");
	}

	// Check for special case of hands for the main weapon skin...

	if (strcmp(m_pWeapon->szPVSkin, "Hands") == 0)
	{
		if ( g_pGameClientShell->GetGameType() == SINGLE )
		{
			SAFE_STRCPY(createStruct.m_SkinNames[0], g_pModelButeMgr->GetHandsSkinFilename(eModelStyle));
		}
		else
		{
			SAFE_STRCPY(createStruct.m_SkinNames[0], "guns\\skins_pv\\MultiHands_pv.dtx");
		}

		// Okay, here is a nice 11th hour hack for you...We want to make sure the
		// player is using the space hands model for the space station mission...so,
		// check to see if we're on the space station...

		if (g_pGameClientShell->GetCurrentMission() == 19)
		{
			SAFE_STRCPY(createStruct.m_Filename, "guns\\models_pv\\SpaceChop_pv.abc");
		}
	}


	m_hObject = CreateModelObject(m_hObject, createStruct);
	if (!m_hObject) return;

	DoSpecialCreateModel();
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::DoSpecialCreateModel()
//
//	PURPOSE:	Do special case create model processing...
//
// ----------------------------------------------------------------------- //

void CWeaponModel::DoSpecialCreateModel()
{
	// Currently we need to check for gadget special cases...

	LTFLOAT r, g, b, a;
	g_pLTClient->GetObjectColor(m_hObject, &r, &g, &b, &a);
	a = 1.0f;

	if (m_pAmmo->eType == GADGET)
	{
		// If we're out of ammo, hide the necessary pieces...

		if (IsOutOfAmmo(m_nWeaponId))
		{
			// Hide the necessary pieces...

            SpecialShowPieces(LTFALSE);
		}

		// See if we should set the alpha (sunglasses)...

		if ((m_pAmmo->eInstDamageType == DT_GADGET_CAMERA) ||
		   (m_pAmmo->eInstDamageType == DT_GADGET_MINE_DETECTOR) ||
		   (m_pAmmo->eInstDamageType == DT_GADGET_INFRA_RED))
		{
			a = 0.99f;
		}
	}

	g_pLTClient->SetObjectColor(m_hObject, r, g, b, a);


	// Just do a SetupModel()...This will handle other special cases...

	SetupModel();
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::CreateModelObject
//
//	PURPOSE:	Create a weaponmodel model object
//
// ----------------------------------------------------------------------- //

HOBJECT CWeaponModel::CreateModelObject(HOBJECT hOldObj, ObjectCreateStruct & createStruct)
{
    if (!m_pWeapon) return LTNULL;

	HOBJECT hObj = hOldObj;

	if (!hObj)
	{
		createStruct.m_ObjectType = OT_MODEL;
        createStruct.m_Flags     |= FLAG_VISIBLE | FLAG_REALLYCLOSE;
		createStruct.m_Flags2	 |= FLAG2_PORTALINVISIBLE | FLAG2_DYNAMICDIRLIGHT;

        hObj = g_pLTClient->CreateObject(&createStruct);
        if (!hObj) return LTNULL;

        //g_pLTClient->SetObjectColor(hObj, 1.0f, 1.0f, 1.0f, 1.0f);
	}

    g_pLTClient->Common()->SetObjectFilenames(hObj, &createStruct);

    uint32 dwFlags = g_pLTClient->GetObjectFlags(hObj);

	if (m_pWeapon->bEnvironmentMap)
	{
        g_pLTClient->SetObjectFlags(hObj, dwFlags | FLAG_ENVIRONMENTMAP);
	}
	else
	{
        g_pLTClient->SetObjectFlags(hObj, dwFlags & ~FLAG_ENVIRONMENTMAP);
	}

    uint32 dwCFlags = g_pLTClient->GetObjectClientFlags(hObj);
    g_pLTClient->SetObjectClientFlags(hObj, dwCFlags | CF_NOTIFYMODELKEYS);

    g_pLTClient->SetModelLooping(hObj, LTFALSE);
    g_pLTClient->SetModelAnimation(hObj, 0);

	return hObj;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::CreateMods
//
//	PURPOSE:	Create any necessary mods
//
// ----------------------------------------------------------------------- //

void CWeaponModel::CreateMods()
{
	if (!m_pWeapon) return;

	// Create the available mods...

	CreateSilencer();
	CreateLaser();
	CreateScope();

	// Put the mods in their starting pos/rot...

	UpdateMods();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::CreateSilencer
//
//	PURPOSE:	Create the silencer model
//
// ----------------------------------------------------------------------- //

void CWeaponModel::CreateSilencer()
{
	if (!m_pWeapon) return;

	m_hSilencerSocket = INVALID_MODEL_SOCKET;

	// Make sure we have the silencer...

	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();

	MOD* pMod = g_pWeaponMgr->GetMod((ModType)pStats->GetSilencer());

	if (!pMod || !pMod->szSocket[0] || !pStats->HaveMod(pMod->nId))
	{
		if (m_hSilencerModel)
		{
            uint32 dwFlags = g_pLTClient->GetObjectFlags(m_hSilencerModel);
            g_pLTClient->SetObjectFlags(m_hSilencerModel, dwFlags & ~FLAG_VISIBLE);
		}

		return;
	}



	// Make sure we have a socket for the silencer...

	if (m_hObject)
	{
		if (g_pModelLT->GetSocket(m_hObject, pMod->szSocket, m_hSilencerSocket) != LT_OK)
		{
			if (m_hSilencerModel)
			{
                uint32 dwFlags = g_pLTClient->GetObjectFlags(m_hSilencerModel);
                g_pLTClient->SetObjectFlags(m_hSilencerModel, dwFlags & ~FLAG_VISIBLE);
			}

			return;
		}
	}


	// Okay create/setup the model...

	ObjectCreateStruct createStruct;
	INIT_OBJECTCREATESTRUCT(createStruct);

	SAFE_STRCPY(createStruct.m_Filename, pMod->szAttachModel);
	SAFE_STRCPY(createStruct.m_SkinNames[0], pMod->szAttachSkin);

	m_hSilencerModel = CreateModelObject(m_hSilencerModel, createStruct);

	if (m_hSilencerModel)
	{
        uint32 dwFlags = g_pLTClient->GetObjectFlags(m_hSilencerModel);
        g_pLTClient->SetObjectFlags(m_hSilencerModel, dwFlags | FLAG_VISIBLE);
        g_pLTClient->SetObjectScale(m_hSilencerModel, &(pMod->vAttachScale));
	}

	// AND THE FLAG FOLLOWS THE MODEL. Setting it true when the model failed to
	// create is what leaves it stuck: RemoveMods used to clear it only when
	// there was a model to delete.
	m_bHaveSilencer = (m_hSilencerModel != LTNULL);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::CreateLaser
//
//	PURPOSE:	Create the laser model
//
// ----------------------------------------------------------------------- //

void CWeaponModel::CreateLaser()
{
	if (!m_pWeapon) return;

    uint32 dwFlags;

	m_hLaserSocket = INVALID_MODEL_SOCKET;

	// Make sure we have the laser...

	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();

	MOD* pMod = g_pWeaponMgr->GetMod((ModType)pStats->GetLaser());

	if (!pMod || !pMod->szSocket[0] || !pStats->HaveMod(pMod->nId))
	{
        uint32 dwFlags;
		if (m_hLaserModel)
		{
            dwFlags = g_pLTClient->GetObjectFlags(m_hLaserModel);
            g_pLTClient->SetObjectFlags(m_hLaserModel, dwFlags & ~FLAG_VISIBLE);
		}

		m_LaserBeam.TurnOff();
		return;
	}


	// Make sure we have a socket for the laser...

	if (m_hObject)
	{
		if (g_pModelLT->GetSocket(m_hObject, pMod->szSocket, m_hLaserSocket) != LT_OK)
		{
			if (m_hLaserModel)
			{
                dwFlags = g_pLTClient->GetObjectFlags(m_hLaserModel);
                g_pLTClient->SetObjectFlags(m_hLaserModel, dwFlags & ~FLAG_VISIBLE);
			}

			m_LaserBeam.TurnOff();
			return;
		}
	}


	// Okay create/setup the model...

	ObjectCreateStruct createStruct;
	INIT_OBJECTCREATESTRUCT(createStruct);

	SAFE_STRCPY(createStruct.m_Filename, pMod->szAttachModel);
	SAFE_STRCPY(createStruct.m_SkinNames[0], pMod->szAttachSkin);

	m_hLaserModel = CreateModelObject(m_hLaserModel, createStruct);

	if (m_hLaserModel)
	{
        dwFlags = g_pLTClient->GetObjectFlags(m_hLaserModel);
        g_pLTClient->SetObjectFlags(m_hLaserModel, dwFlags | FLAG_VISIBLE);
        g_pLTClient->SetObjectScale(m_hLaserModel, &(pMod->vAttachScale));
	}

	m_LaserBeam.TurnOn();

    m_bHaveLaser = LTTRUE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::CreateScope
//
//	PURPOSE:	Create the scope model
//
// ----------------------------------------------------------------------- //

void CWeaponModel::CreateScope()
{
	if (!m_pWeapon) return;

	m_hScopeSocket = INVALID_MODEL_SOCKET;

	// Make sure we have the scope...

	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();

	MOD* pMod = g_pWeaponMgr->GetMod((ModType)pStats->GetScope());
	if (!pMod || !pMod->szSocket[0] || !pStats->HaveMod(pMod->nId))
	{
		if (m_hScopeModel)
		{
            uint32 dwFlags = g_pLTClient->GetObjectFlags(m_hScopeModel);
            g_pLTClient->SetObjectFlags(m_hScopeModel, dwFlags & ~FLAG_VISIBLE);
		}

		return;
	}



	// Make sure we have a socket for the scope...

	if (m_hObject)
	{
		if (g_pModelLT->GetSocket(m_hObject, pMod->szSocket, m_hScopeSocket) != LT_OK)
		{
			if (m_hScopeModel)
			{
                uint32 dwFlags = g_pLTClient->GetObjectFlags(m_hScopeModel);
                g_pLTClient->SetObjectFlags(m_hScopeModel, dwFlags & ~FLAG_VISIBLE);
			}

			return;
		}
	}


	// Okay create/setup the model...

	ObjectCreateStruct createStruct;
	INIT_OBJECTCREATESTRUCT(createStruct);

	SAFE_STRCPY(createStruct.m_Filename, pMod->szAttachModel);
	SAFE_STRCPY(createStruct.m_SkinNames[0], pMod->szAttachSkin);

	m_hScopeModel = CreateModelObject(m_hScopeModel, createStruct);

	if (m_hScopeModel)
	{
        uint32 dwFlags = g_pLTClient->GetObjectFlags(m_hScopeModel);
        g_pLTClient->SetObjectFlags(m_hScopeModel, dwFlags | FLAG_VISIBLE);
        g_pLTClient->SetObjectScale(m_hScopeModel, &(pMod->vAttachScale));
	}

    m_bHaveScope = LTTRUE;
}



// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::UpdateFlash()
//
//	PURPOSE:	Update muzzle flash state
//
// ----------------------------------------------------------------------- //

// How long a first-person muzzle flash is held, at least. See UpdateFlash.
static VarTrack g_vtVRFlashMinSec;

void CWeaponModel::UpdateFlash(WeaponState eState)
{
    uint32 dwFlags = g_pLTClient->GetObjectFlags(m_hObject);

	if (!(dwFlags & FLAG_VISIBLE) || !m_pWeapon->pPVMuzzleFX)
	{
		m_MuzzleFlash.Hide();
		return;
	}

    LTFLOAT fCurTime = g_pLTClient->GetTime();
    LTFLOAT fFlashDuration = m_pWeapon->pPVMuzzleFX->fDuration;

	// A FLASH SHORTER THAN THE PIPELINE CAN SEE IS A FLASH NOBODY SEES.
	//
	// NOLF's first-person muzzle effects last 5 to 7.5 milliseconds
	// (ATTRIBUTES/FX.TXT: PV_P38MuzzFX 0.0075, PV_AK47MuzzFX 0.005). The
	// retail renderer drew the frame it was switched on, so that was enough.
	// This port publishes what it draws from a pass of its own, and a window
	// that short falls between two publishes every time: measured on 11
	// September, the flash was switched on 13 times in one run and the
	// publish pass found its objects HIDDEN on all 900 frames it looked.
	//
	// So the flash is held for at least VRFlashMinSec. It still expires on
	// its own, one frame later than it used to. 0 restores retail timing.
	if (!g_vtVRFlashMinSec.IsInitted())
		g_vtVRFlashMinSec.Init(g_pLTClient, "VRFlashMinSec", NULL, 0.033f);
	const LTFLOAT fVRMin = g_vtVRFlashMinSec.GetFloat();
	if (fVRMin > 0.0f && fFlashDuration < fVRMin) fFlashDuration = fVRMin;

	// HELD LIT WHILE TUNING. The AK47's flash lasts five MILLISECONDS
	// (PV_AK47MuzzFX 0.005 in ATTRIBUTES/FX.TXT) - VRFlashMinSec already
	// stretches it to 33 ms just so the publish pass can see it at all. You
	// cannot aim at something that brief, and tuning a thing you only glimpse
	// is why this effect has been slow to tune. With VRFlashHold it stays lit
	// continuously so the numpad can be watched against it.
	// ONE SWITCH, NOT TWO. VRFlashTune 1 means the flash stays lit, full stop.
	//
	// This used to require VRFlashTune AND VRFlashHold together. In headset
	// testing the persistent muzzle flash disappeared and only showed while
	// actively shooting - which is precisely what this reads like when either
	// one is not set, and there is no way to tell from inside the headset which
	// of the two it was. Two switches for one behaviour is one switch too many
	// on a tool whose whole job is to be watched while you aim at it.
	//
	// VRFlashHold survives as an override so the held flash can be turned OFF
	// while still tuning (the Delete key), which is the only reason to want
	// them separate: a few weapons draw a particle burst that the hold cannot
	// keep on screen anyway.
	const bool bHoldOff = g_vtVRFlashHold.IsInitted()
					   && (g_vtVRFlashHold.GetFloat() <= 0.0f);
	const bool bFlashHold = (g_vtVRFlashTune.GetFloat() > 0.0f)
						 && !bHoldOff
						 && (g_pGameClientShell->GetPlayerState() == PS_ALIVE);

	if (!bFlashHold &&
		( fCurTime >= m_fFlashStartTime + fFlashDuration ||
		 g_pGameClientShell->GetPlayerState() != PS_ALIVE ||
		 IsLiquid(g_pGameClientShell->GetCurContainerCode()) ))
	{
		m_MuzzleFlash.Hide();
	}
	else
	{
		// Align the flash object to the direction the model is facing...

  		m_MuzzleFlash.Show();

		// THE TUNER'S OFFSET, in the gun's own frame. Zero unless the player is tuning,
		// so this is inert in a normal run. See CGameClientShell::VRFlashTuneUpdate.
		// THE PER-WEAPON OFFSET, ALWAYS - not only while tuning. This is the
		// residual after the coordinate-space fix, it differs per gun, and a
		// correction that only applies with a debug cvar on is not a fix.
		VRWeaponVarsInit();
		LTVector vFlashPos = m_vFlashPos;
		{
			const int nWep = (int)GetWeaponId();
			const float fR = s_vrFlashOffR.Get(nWep);
			const float fU = s_vrFlashOffU.Get(nWep);
			const float fF = s_vrFlashOffF.Get(nWep);
			if (fR != 0.0f || fU != 0.0f || fF != 0.0f)
			{
				// VRGunRot, NOT GetModelRot - see the note on VRGunRot. This
				// offset has to point down the BARREL, and GetModelRot points
				// where the head is looking.
				// ...AND THROUGH THE CAMERA BASIS, because VRGunRot is the
				// hand's rotation in the camera object's space. See
				// VRPrims_GunFrameToWorld: a trim rotated by VRGunRot alone
				// stayed fixed in the world through a stick turn.
				vFlashPos += VRPrims_GunFrameToWorldLast(VRGunRot(), LTVector(fR, fU, fF));
			}
		}
		// VRFlashTrace 1: EVERY FRAME, so a drift that only happens while the gun
		// is MOVING leaves a record.
		//
		// Two candidate causes have now been measured and ruled out - the
		// one-frame pose lag is six millimetres at the muzzle at 94 deg/s, and
		// the flash's stereo disparity is within 2-3% of the gun's. Neither is
		// what the tester is describing, and the desk cannot reproduce it: at
		// VRFlashOffF 0 the flash sits on the gun still AND swinging.
		//
		// So stop guessing at it from here. This logs the separation between
		// the flash and the gun DECOMPOSED IN THE GUN'S OWN FRAME, next to the
		// hand's yaw, once per frame. If the separation is constant while the
		// yaw changes, the flash is welded to the gun and the fault is
		// elsewhere; if it grows with the yaw RATE it is a lag; if it grows
		// with the yaw ANGLE it is a pivot. Five seconds of the player swinging is
		// four hundred samples and answers which.
		if (g_vtVRFlashTrace.GetFloat() > 0.0f)
		{
			LTRotation rG = VRGunRot();
			LTVector vGU, vGR, vGF;
			g_pLTClient->GetRotationVectors(&rG, &vGU, &vGR, &vGF);
			// THE GUN'S WORLD POSITION, NOT THE OBJECT'S.
			//
			// The first version of this read m_hObject's position, and the
			// separation came out at TWO THOUSAND units. That is not a drift,
			// it is the whole problem in one number: the view weapon carries
			// FLAG_REALLYCLOSE, so its object position is CAMERA-RELATIVE
			// (vSwung, a handful of units), while m_vFlashPos is a WORLD point.
			// Subtracting one from the other measures the distance from the map
			// origin and calls it an offset.
			//
			// The re-base knows where the gun really is - it is the value the
			// renderer draws the gun at - so that is the only honest comparison.
			const LTVector vGunPos = VRPrims_RebaseLast().vGunWorld;
			const LTVector d = vFlashPos - vGunPos;
			float fYawNow = 0.0f;
			if (VRShared::IsLive())
			{
				const VRSharedState& s = VRShared::State();
				fYawNow = s.Hands[1].fYawDeg;
			}
			static float s_fPrevYaw = 0.0f;
			const float fRate = (fYawNow - s_fPrevYaw) * 90.0f;	// deg/s at 90 fps
			s_fPrevYaw = fYawNow;
			VRLog::Msg("VRFlashTrace: yaw %+7.2f rate %+8.1f deg/s | flash-gun"
					   " r %+7.2f u %+7.2f f %+7.2f | len %6.2f",
					   fYawNow, fRate,
					   d.Dot(vGR), d.Dot(vGU), d.Dot(vGF), d.Mag());
		}

		{
			static int s_nSaidFP = 0;
			if (s_nSaidFP < 6)
			{
				++s_nSaidFP;
				LTVector vC(0,0,0);
				HOBJECT hC = g_pGameClientShell->GetCamera();
				if (hC) g_pLTClient->GetObjectPos(hC, &vC);
				const LTVector d = vFlashPos - vC;
				VRLog::Msg("VRFlashWhere: flash at %.0f %.0f %.0f, camera at"
						   " %.0f %.0f %.0f - %.0f units apart (%.1f ft)",
						   vFlashPos.x, vFlashPos.y, vFlashPos.z,
						   vC.x, vC.y, vC.z, d.Mag(), d.Mag()/18.0f);
			}
		}
		m_MuzzleFlash.SetPos(vFlashPos, m_vFlashOffset);
		// The flash points down the BARREL, not down the player's gaze.
		m_MuzzleFlash.SetRot(VRAimRot());
		m_MuzzleFlash.Update();
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::StartFlash()
//
//	PURPOSE:	Start the muzzle flash
//
// ----------------------------------------------------------------------- //

void CWeaponModel::StartFlash()
{

    m_fFlashStartTime = g_pLTClient->GetTime();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetModelPos()
//
//	PURPOSE:	Get the position of the weapon model
//
// ----------------------------------------------------------------------- //

LTVector CWeaponModel::GetModelPos() const
{
    LTVector vPos;
	vPos.Init();

	if (m_hObject)
	{
        g_pLTClient->GetObjectPos(m_hObject, &vPos);
		vPos += m_vCamPos;
	}

	return vPos;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetModelRot()
//
//	PURPOSE:	Get the rotation of the weapon model
//
// ----------------------------------------------------------------------- //

// THE GUN'S ACTUAL ROTATION, WHICH GetModelRot IS NOT.
//
// GetModelRot returns m_rCamRot - the CAMERA's rotation - and the code below it
// that reads the object's own rotation is unreachable, sitting after an early
// return. That is fine for what retail used it for and wrong for anything that
// has to sit on the gun in VR, because in VR the gun carries the HAND's
// rotation and the camera does not.
//
// Measured with VRFlashTrace through a 45-degree sweep: the flash-to-gun
// distance stayed EXACTLY constant at 16.93 units - the ak47's authored muzzle
// distance, so the muzzle itself is welded on correctly - while the direction
// rotated with the yaw, from (r -0.96, f +16.61) at yaw +3 to (r -8.49, f
// +14.31) at yaw +39. A constant length with a rotating direction is an offset
// expressed in the wrong frame.
//
// That is a report that the flash does not travel with the gun,
// and that the further out it is pushed the more it gets away in a swing:
// the player's tuned VRFlashOffF is applied along the CAMERA's forward, so it points
// where the player's head looks rather than down the barrel, and the further out the player
// pushes it the further it slides. An angular error at radius r displaces by
// r * theta, which is exactly the further-out-is-worse behaviour.
LTVector CWeaponModel::VRFlashOffset() const
{
	VRWeaponVarsInit();
	const int nW = (int)m_nWeaponId;
	return VRMirR(LTVector(s_vrFlashOffR.Get(nW), s_vrFlashOffU.Get(nW), s_vrFlashOffF.Get(nW)));
}

// ON TOP OF the flash's offset, never instead of it - see the note by the
// declarations. Zero until one is tuned, so nothing changes until asked.
LTVector CWeaponModel::VRGripOffset() const
{
	VRWeaponVarsInit();
	const int nW = (int)m_nWeaponId;
	return VRMirR(LTVector(s_vrGripOffR.Get(nW), s_vrGripOffU.Get(nW), s_vrGripOffF.Get(nW)));
}

LTVector CWeaponModel::VRLensOffset() const
{
	VRWeaponVarsInit();
	const int nW = (int)m_nWeaponId;
	return VRMirR(LTVector(s_vrLensOffR.Get(nW), s_vrLensOffU.Get(nW), s_vrLensOffF.Get(nW)));
}

float CWeaponModel::VRLensRadius() const
{
	VRWeaponVarsInit();
	return s_vrLensRad.Get((int)m_nWeaponId);
}

LTVector CWeaponModel::VRTracerOffset() const
{
	VRWeaponVarsInit();
	const int nW = (int)m_nWeaponId;
	return VRMirR(LTVector(s_vrTracerOffR.Get(nW), s_vrTracerOffU.Get(nW), s_vrTracerOffF.Get(nW)));
}

LTVector CWeaponModel::VRShellOffset() const
{
	VRWeaponVarsInit();
	const int nW = (int)m_nWeaponId;
	return VRMirR(LTVector(s_vrShellOffR.Get(nW), s_vrShellOffU.Get(nW), s_vrShellOffF.Get(nW)));
}

float CWeaponModel::VRWeaponScale() const
{
	VRWeaponVarsInit();
	return s_vrWeaponScale.Get(m_nWeaponId);
}

LTRotation CWeaponModel::VRGunRot() const
{
	LTRotation rRot;
	rRot.Init();
	if (m_hObject) g_pLTClient->GetObjectRotation(m_hObject, &rRot);
	return rRot;
}

LTRotation CWeaponModel::VRAimRot() const
{
	LTRotation rRot = VRGunRot();
	float fYawDeg = 0.0f, fPitchDeg = 0.0f;
	VRAimTotal(m_nWeaponId, fYawDeg, fPitchDeg);
	const float fY =  fYawDeg   * 0.01745329f * (VRShared::SwapHands() ? -1.0f : 1.0f);	// Leftorium: mirrored
	const float fP = -fPitchDeg * 0.01745329f;
	if (fY == 0.0f && fP == 0.0f) return rRot;
	// About the gun's OWN axes: same order and signs as the model's ANGLE.
	LTRotation rTrim;
	rTrim.Init();
	g_pLTClient->EulerRotateY(&rTrim, fY);
	g_pLTClient->EulerRotateX(&rTrim, fP);
	return rRot * rTrim;
}

LTRotation CWeaponModel::GetModelRot() const
{
	return m_rCamRot;

    LTRotation rRot;
	rRot.Init();

	if (m_hObject)
	{
        g_pLTClient->GetObjectRotation(m_hObject, &rRot);
		//rRot += m_rCamRot;
	}

	return rRot;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPVWeaponModel::StringKey()
//
//	PURPOSE:	Handle animation command
//
// ----------------------------------------------------------------------- //

void CWeaponModel::OnModelKey(HLOCALOBJ hObj, ArgList* pArgList)
{
	if (!hObj || (hObj != m_hObject) || !pArgList || !pArgList->argv || pArgList->argc == 0) return;
	if (!m_pWeapon) return;

	char* pKey = pArgList->argv[0];
	if (!pKey) return;

	if (stricmp(pKey, WEAPON_KEY_FIRE) == 0)
	{
		// Only allow fire keys if it is a fire animation...

		if (m_hObject)
		{
			uint32 dwAni = g_pLTClient->GetModelAnimation(m_hObject);
			if (IsFireAni(dwAni))
			{
				m_bFire = LTTRUE;
			}
		}
	}
	else if (stricmp(pKey, WEAPON_KEY_SOUND) == 0)
	{
		if (pArgList->argc > 1 && pArgList->argv[1])
		{
            char* pBuf = LTNULL;

			PlayerSoundId nId = (PlayerSoundId)atoi(pArgList->argv[1]);
			switch (nId)
			{
				case PSI_RELOAD:
				case PSI_RELOAD2:
				case PSI_RELOAD3:
				{
					pBuf = m_pWeapon->szReloadSounds[nId - PSI_RELOAD];
				}
				break;
				case PSI_SELECT:
					pBuf = m_pWeapon->szSelectSound;
				break;
				case PSI_DESELECT:
					pBuf = m_pWeapon->szDeselectSound;
				break;

				case PSI_INVALID:
				default : break;
			}

			if (pBuf && pBuf[0])
			{
				g_pClientSoundMgr->PlaySoundLocal(pBuf, SOUNDPRIORITY_PLAYER_HIGH);

				// Send message to Server so that other client's can hear this sound...

                uint32 dwId;
                g_pLTClient->GetLocalClientID(&dwId);

                HMESSAGEWRITE hWrite = g_pLTClient->StartMessage(MID_WEAPON_SOUND);
                g_pLTClient->WriteToMessageByte(hWrite, nId);
                g_pLTClient->WriteToMessageByte(hWrite, m_nWeaponId);
                g_pLTClient->WriteToMessageByte(hWrite, (uint8)dwId);
                g_pLTClient->WriteToMessageVector(hWrite, &m_vFlashPos);
                g_pLTClient->EndMessage2(hWrite, MESSAGE_NAGGLEFAST);
			}
		}
	}
	else if (stricmp(pKey, WEAPON_KEY_FX) == 0)
	{
		m_PVFXMgr.HandleFXKey(pArgList);
	}
	else if (stricmp(pKey, WEAPON_KEY_FIREFX) == 0)
	{
		// Only allow fire keys if it is a fire animation...

		if (m_hObject)
		{
			uint32 dwAni = g_pLTClient->GetModelAnimation(m_hObject);
			if (IsFireAni(dwAni))
			{
		        m_bFire = LTTRUE;
			}
		}

		m_PVFXMgr.HandleFXKey(pArgList);
	}
	else if (stricmp(pKey, WEAPON_KEY_SUNGLASS) == 0)
	{
		HandleSunglassMode();
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::HandleSunglassWeaponKey()
//
//	PURPOSE:	Handle changing sunglass modes...
//
// ----------------------------------------------------------------------- //

void CWeaponModel::HandleSunglassMode()
{
	// Handle sunglasses gadget...

	if (m_pAmmo->eType == GADGET)
	{
		if (GetState() == W_DESELECT)
		{
			g_pInterfaceMgr->SetSunglassMode(SUN_NONE);
		}
		else if (m_pAmmo->eInstDamageType == DT_GADGET_CAMERA)
		{
			g_pInterfaceMgr->SetSunglassMode(SUN_CAMERA);
		}
		else if (m_pAmmo->eInstDamageType == DT_GADGET_MINE_DETECTOR)
		{
			g_pInterfaceMgr->SetSunglassMode(SUN_MINES);
		}
		else if (m_pAmmo->eInstDamageType == DT_GADGET_INFRA_RED)
		{
			g_pInterfaceMgr->SetSunglassMode(SUN_IR);
		}
		else
		{
			g_pInterfaceMgr->SetSunglassMode(SUN_NONE);
		}
	}
	else
	{
		g_pInterfaceMgr->SetSunglassMode(SUN_NONE);
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::UpdateModelState
//
//  PURPOSE:    Update the model's state (fire if bFire == LTTRUE)
//
// ----------------------------------------------------------------------- //

WeaponState CWeaponModel::UpdateModelState(LTBOOL bFire)
{
	WeaponState eRet = W_IDLE;

	// Determine what we should be doing...

	if (bFire)
	{
		UpdateFiring();
	}
	else
	{
		UpdateNonFiring();
	}


	if (m_bFire)
	{
		eRet = Fire((m_pAmmo->eType != GADGET));
	}


	// See if we just finished deselecting the weapon...

	if (m_bWeaponDeselected)
	{
        m_bWeaponDeselected = LTFALSE;

		// Change weapons if we're not chaing between normal and
		// alt-fire modes...

		if (m_nRequestedWeaponId != m_nWeaponId)
		{
			HandleInternalWeaponChange(m_nRequestedWeaponId);
		}
	}

	return eRet;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::Fire
//
//	PURPOSE:	Handle firing the weapon
//
// ----------------------------------------------------------------------- //

WeaponState CWeaponModel::Fire(LTBOOL bUpdateAmmo)
{

	WeaponState eRet = W_IDLE;

	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
	if (!pStats) return W_IDLE;

    LTBOOL bInfiniteAmmo = (g_bInfiniteAmmo || m_pWeapon->bInfiniteAmmo);
	int nAmmo = bInfiniteAmmo ? INFINITE_AMMO_AMOUNT : pStats->GetAmmoCount(m_nAmmoId);
	{
		static int s_nFireSaid = 0;
		if (s_nFireSaid++ < 200)
			VRLog::Msg("VRTrigger: FIRE - weapon %d ammo %d%s", (int)m_nWeaponId, nAmmo,
					   nAmmo > 0 ? "" : "  <- NO AMMO, dry fire");
	}

	// If this weapon uses ammo, make sure we have ammo...

	if (nAmmo > 0)
	{
		eRet = W_FIRED;

		if (bUpdateAmmo)
		{
			DecrementAmmo();
		}
	}
	else  // NO AMMO
	{
		SetState(W_FIRING_NOAMMO);

		// Play dry-fire sound...

		if (m_pWeapon->szDryFireSound[0])
		{
			g_pClientSoundMgr->PlaySoundLocal(m_pWeapon->szDryFireSound, SOUNDPRIORITY_PLAYER_HIGH);
		}


		// Send message to Server so that other client's can hear this sound...

        uint32 dwId;
        g_pLTClient->GetLocalClientID(&dwId);

        HMESSAGEWRITE hWrite = g_pLTClient->StartMessage(MID_WEAPON_SOUND);
        g_pLTClient->WriteToMessageByte(hWrite, PSI_DRY_FIRE);
        g_pLTClient->WriteToMessageByte(hWrite, m_nWeaponId);
        g_pLTClient->WriteToMessageByte(hWrite, (uint8)dwId);
        g_pLTClient->WriteToMessageVector(hWrite, &m_vFlashPos);
        g_pLTClient->EndMessage2(hWrite, MESSAGE_NAGGLEFAST);
	}

    m_bFire = LTFALSE;

	return eRet;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::DecrementAmmo
//
//	PURPOSE:	Decrement the weapon's ammo count
//
// ----------------------------------------------------------------------- //

void CWeaponModel::DecrementAmmo()
{
	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
	if (!pStats) return;

    LTBOOL bInfiniteAmmo = (g_bInfiniteAmmo || m_pWeapon->bInfiniteAmmo);
	int nAmmo = bInfiniteAmmo ? INFINITE_AMMO_AMOUNT : pStats->GetAmmoCount(m_nAmmoId);

	int nShotsPerClip = m_pWeapon->nShotsPerClip;

	if (m_nAmmoInClip > 0)
	{
		if (nShotsPerClip > 0)
		{
			m_nAmmoInClip--;
		}

		if (!bInfiniteAmmo)
		{
			nAmmo--;

			// Update our stats.  This will ensure that our stats are always
			// accurate (even in multiplayer)...

			pStats->UpdateAmmo(m_nWeaponId, m_nAmmoId, nAmmo, LTFALSE, LTFALSE);
		}
	}

	// Check to see if we need to reload...

	if (nShotsPerClip > 0)
	{
		if (m_nAmmoInClip <= 0)
		{
            ReloadClip(LTTRUE, nAmmo);
		}
	}


// TESTING
	// If we're out of ammo, set the appropriate state...
	if (pStats->GetAmmoCount(m_nAmmoId) <= 0)
	{
		SetState(W_FIRING_NOAMMO);
	}
// TESTING

}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeapon::ReloadClip
//
//	PURPOSE:	Fill the clip
//
// ----------------------------------------------------------------------- //

void CWeaponModel::ReloadClip(LTBOOL bPlayReload, int nNewAmmo, LTBOOL bForce)
{
	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
	if (!pStats) return;


	// Handle gadget reload...

	GadgetReload();


	int nAmmoCount = pStats->GetAmmoCount(m_nAmmoId);
	int nAmmo = nNewAmmo >= 0 ? nNewAmmo : nAmmoCount;
	int nShotsPerClip = m_pWeapon->nShotsPerClip;


	// Update the player's stats...

	g_pInterfaceMgr->UpdateWeaponStats(m_nWeaponId, m_nAmmoId, nAmmo);


	// Make sure we can reload the clip...

	if (!bForce)
	{
		// Already reloading...

		if (m_hObject && (GetState() == W_RELOADING))
		{
			return;
		}

		// Clip is full...

		if (m_nAmmoInClip == nShotsPerClip || m_nAmmoInClip == nAmmoCount)
		{
			return;
		}
	}

	if (nAmmo > 0 && nShotsPerClip > 0)
	{
		m_nNewAmmoInClip = nAmmo < nShotsPerClip ? nAmmo : nShotsPerClip;

		if (bPlayReload && GetReloadAni() != INVALID_ANI)
		{
			SetState(W_RELOADING);
			return;
		}
		else
		{
			// This will get set after the reloading animation is done if
			// we are playing a reload animation...

			m_nAmmoInClip = m_nNewAmmoInClip;
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::UpdateFiring
//
//	PURPOSE:	Update the animation state of the model
//
// ----------------------------------------------------------------------- //

void CWeaponModel::UpdateFiring()
{
    m_bCanSetLastFire = LTTRUE;

	if (GetState() == W_RELOADING)
	{
		if (!PlayReloadAnimation())
		{
			SetState(W_FIRING);
		}
	}
	if (GetState() == W_IDLE)
	{
		SetState(W_FIRING);
	}
	if (GetState() == W_SELECT)
	{
		if (!PlaySelectAnimation())
		{
			SetState(W_FIRING);
		}
	}
	if (GetState() == W_DESELECT)
	{
		if (!PlayDeselectAnimation())
		{
			SetState(W_FIRING);
		}
	}
	if (GetState() == W_FIRING || GetState() == W_FIRING_NOAMMO)
	{
		if (PlayFireAnimation())
		{
            m_bCanSetLastFire = LTFALSE;
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::UpdateNonFiring
//
//	PURPOSE:	Update the non-firing animation state of the model
//
// ----------------------------------------------------------------------- //

void CWeaponModel::UpdateNonFiring()
{
    m_bCanSetLastFire = LTTRUE;

	if (GetState() == W_FIRING)
	{
        if (!PlayFireAnimation(LTFALSE))
		{
			SetState(W_IDLE);
		}
		else
		{
            m_bCanSetLastFire = LTFALSE;
		}
	}
	if (GetState() == W_FIRING_NOAMMO)
	{
		SetState(W_IDLE);
	}
	if (GetState() == W_RELOADING)
	{
		if (!PlayReloadAnimation())
		{
			SetState(W_IDLE);
		}
	}
	if (GetState() == W_SELECT)
	{
		if (!PlaySelectAnimation())
		{
			SetState(W_IDLE);
		}
	}
	if (GetState() == W_DESELECT)
	{
		if (!PlayDeselectAnimation())
		{
            m_bWeaponDeselected = LTTRUE;
			SetState(W_IDLE);
		}
	}
	if (GetState() == W_IDLE)
	{
		PlayIdleAnimation();
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::PlaySelectAnimation()
//
//	PURPOSE:	Set model to select animation
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::PlaySelectAnimation()
{
    uint32 dwSelectAni = GetSelectAni();

    if (!m_hObject || dwSelectAni == INVALID_ANI) return LTFALSE;

    uint32 dwAni    = g_pLTClient->GetModelAnimation(m_hObject);
    uint32 dwState  = g_pLTClient->GetModelPlaybackState(m_hObject);

    LTBOOL bIsSelectAni = IsSelectAni(dwAni);
	if (bIsSelectAni && (dwState & MS_PLAYDONE))
	{
        return LTFALSE;
	}
	else if (!bIsSelectAni)
	{
        g_pLTClient->SetModelLooping(m_hObject, LTFALSE);
        g_pLTClient->SetModelAnimation(m_hObject, dwSelectAni);
/*
		// Tell the server we're playing the select animation...

        HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_PLAYER_CLIENTMSG);
        g_pLTClient->WriteToMessageByte(hMessage, CP_WEAPON_STATUS);
        g_pLTClient->WriteToMessageByte(hMessage, WS_SELECT);
        g_pLTClient->EndMessage(hMessage);
*/
	}

    return LTTRUE;  // Animation playing
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::PlayDeselectAnimation()
//
//	PURPOSE:	Set model to deselect animation
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::PlayDeselectAnimation()
{
    uint32 dwDeselectAni = GetDeselectAni();

	if (!m_hObject || dwDeselectAni == INVALID_ANI)
	{
        m_bWeaponDeselected = LTTRUE;
        return LTFALSE;
	}

    uint32 dwAni    = g_pLTClient->GetModelAnimation(m_hObject);
    uint32 dwState  = g_pLTClient->GetModelPlaybackState(m_hObject);

    LTBOOL bIsDeselectAni = IsDeselectAni(dwAni);

	if (bIsDeselectAni && (dwState & MS_PLAYDONE))
	{
        m_bWeaponDeselected = LTTRUE;
        return LTFALSE;
	}

    return LTTRUE;  // Animation playing
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::PlayFireAnimation()
//
//	PURPOSE:	Set model to firing animation
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::PlayFireAnimation(LTBOOL bResetAni)
{
    if (!m_hObject) return LTFALSE;

	// Can only set the last fire type if a fire animation isn't playing
	// (i.e., we'll assume this function will return false)...

    uint32 dwAni    = g_pLTClient->GetModelAnimation(m_hObject);
    uint32 dwState  = g_pLTClient->GetModelPlaybackState(m_hObject);

    LTBOOL bIsFireAni = IsFireAni(dwAni);

	if (!bIsFireAni || (dwState & MS_PLAYDONE))
	{
		if (bResetAni)
		{
            uint32 dwFireAni = GetFireAni(m_eLastFireType);
            if (dwFireAni == INVALID_ANI) return LTFALSE;

            g_pLTClient->SetModelLooping(m_hObject, LTFALSE);
            g_pLTClient->SetModelAnimation(m_hObject, dwFireAni);
            g_pLTClient->ResetModelAnimation(m_hObject);  // Start from beginning
		}

		if (bIsFireAni && (dwState & MS_PLAYDONE))
		{
			DoSpecialEndFire();
            return LTFALSE;
		}
	}

    return LTTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::DoSpecialFire()
//
//	PURPOSE:	Do special case fire processing
//
// ----------------------------------------------------------------------- //

void CWeaponModel::DoSpecialFire()
{
	// Currently we need to check for gadget special cases...

	if (m_pAmmo->eType == GADGET)
	{
		// Hide the necessary pieces...

        SpecialShowPieces(LTFALSE);

		// Decrement ammo count here...

		DecrementAmmo();
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::DoSpecialEndFire()
//
//	PURPOSE:	Do special case end of fire animation processing
//
// ----------------------------------------------------------------------- //

void CWeaponModel::DoSpecialEndFire()
{
	// Currently we need to check for gadget special cases...

	if (m_pAmmo->eType == GADGET)
	{
		// Unhide any hidden pieces...

        SpecialShowPieces(LTTRUE);

		// If we're out of ammo, switch weapons...

		if (IsOutOfAmmo(m_nWeaponId))
		{
			AutoSelectWeapon();
		}
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::SpecialShowPieces()
//
//	PURPOSE:	Special case showing nodes
//
// ----------------------------------------------------------------------- //

void CWeaponModel::SpecialShowPieces(LTBOOL bShow, LTBOOL bForce)
{
	// Currently we need to check for gadget special cases...

	if (m_pAmmo->eType == GADGET)
	{
		// If we're out of ammo, keep hidden...

		if (bShow && !bForce)
		{
			CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
			if (pStats && pStats->GetAmmoCount(m_nAmmoId) < 1)
			{
                bShow = LTFALSE; // Hide the necessary pieces...
			}
		}

        char** s_pPieceArray = LTNULL;

		DamageType eType = m_pAmmo->eInstDamageType;
		if (eType == DT_GADGET_CAMERA_DISABLER)
		{
			s_pPieceArray = s_pCamDisPieces;
		}
		else if (eType == DT_GADGET_CODE_DECIPHERER)
		{
			s_pPieceArray = s_pCodeDecPieces;
		}

        ILTModel* pModelLT = g_pLTClient->GetModelLT();
        HMODELPIECE hPiece = LTNULL;

		for (int i=0; s_pPieceArray && s_pPieceArray[i]; i++)
		{
			if (pModelLT->GetPiece(m_hObject, s_pPieceArray[i], hPiece) == LT_OK)
			{
				pModelLT->SetPieceHideStatus(m_hObject, hPiece, !bShow);
			}
		}
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::PlayReloadAnimation()
//
//	PURPOSE:	Set model to reloading animation
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::PlayReloadAnimation()
{
    uint32 dwReloadAni = GetReloadAni();

    if (!m_hObject || dwReloadAni == INVALID_ANI) return LTFALSE;

    uint32 dwAni    = g_pLTClient->GetModelAnimation(m_hObject);
    uint32 dwState  = g_pLTClient->GetModelPlaybackState(m_hObject);

    LTBOOL bCanPlay  = (!IsFireAni(dwAni) || g_pLTClient->GetModelLooping(m_hObject) || (dwState & MS_PLAYDONE));

    LTBOOL bIsReloadAni = IsReloadAni(dwAni);

	if (bIsReloadAni && (dwState & MS_PLAYDONE))
	{
		// Set ammo in clip amount...

		m_nAmmoInClip = m_nNewAmmoInClip;

		// Update the player's stats...

		CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
		if (pStats)
		{
			int nAmmo = pStats->GetAmmoCount(m_nAmmoId);
			g_pInterfaceMgr->UpdateWeaponStats(m_nWeaponId, m_nAmmoId, nAmmo);
		}

        return LTFALSE;
	}
	else if (!bIsReloadAni && bCanPlay)
	{
        g_pLTClient->SetModelLooping(m_hObject, LTFALSE);
        g_pLTClient->SetModelAnimation(m_hObject, dwReloadAni);

		// Tell the server we're playing the reload ani...

        HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_PLAYER_CLIENTMSG);
        g_pLTClient->WriteToMessageByte(hMessage, CP_WEAPON_STATUS);
        g_pLTClient->WriteToMessageByte(hMessage, WS_RELOADING);
        g_pLTClient->EndMessage(hMessage);
	}

    return LTTRUE;  // Animation playing
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::PlayIdleAnimation()
//
//	PURPOSE:	Set model to Idle animation
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::PlayIdleAnimation()
{
    if (!m_hObject || g_pGameClientShell->IsZoomed()) return LTFALSE;

    LTBOOL bCurAniDone = !!(g_pLTClient->GetModelPlaybackState(m_hObject) & MS_PLAYDONE);

	// Make sure idle animation is done if one is currently playing...

    uint32 dwAni = g_pLTClient->GetModelAnimation(m_hObject);
	if (IsIdleAni(dwAni))
	{
		if (!bCurAniDone)
		{
            return LTTRUE;
		}
	}


	// See if the player is moving...Don't do normal idles when player is
	// moving...

    LTBOOL bMoving = LTFALSE;
	if (g_pGameClientShell->GetMoveMgr()->GetVelMagnitude() > 0.1f)
	{
		bMoving = !!(g_pGameClientShell->GetPlayerFlags() & BC_CFLG_MOVING);
	}


	// Play idle if it is time...(and not moving)...

    LTFLOAT fTime = g_pLTClient->GetTime();

    LTBOOL bPlayIdle = LTFALSE;

	if (fTime > m_fNextIdleTime && bCurAniDone)
	{
		bPlayIdle = !bMoving;
		m_fNextIdleTime	= GetNextIdleTime();
	}

    uint32 nSubleIdleAni = GetSubtleIdleAni();

	if (bPlayIdle)
	{
        uint32 nAni = GetIdleAni();

		if (nAni == INVALID_ANI)
		{
			nAni = DEFAULT_ANI;
		}

        g_pLTClient->SetModelLooping(m_hObject, LTFALSE);
        g_pLTClient->SetModelAnimation(m_hObject, nAni);

        return LTTRUE;
	}
	else if (nSubleIdleAni != INVALID_ANI)
	{
		// Play subtle idle...

		if (dwAni != nSubleIdleAni || bCurAniDone)
		{
            g_pLTClient->SetModelLooping(m_hObject, LTFALSE /*LTTRUE*/);
            g_pLTClient->SetModelAnimation(m_hObject, nSubleIdleAni);
            g_pLTClient->ResetModelAnimation(m_hObject);
		}

        return LTTRUE;
	}

    return LTFALSE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::Select()
//
//	PURPOSE:	Select the weapon
//
// ----------------------------------------------------------------------- //

void CWeaponModel::Select()
{
	SetState(W_SELECT);

    ReloadClip(LTFALSE);

    uint32 dwSelectAni = GetSelectAni();

	if (m_hObject && dwSelectAni != INVALID_ANI)
	{
        uint32 dwAni = g_pLTClient->GetModelAnimation(m_hObject);

		if (!IsSelectAni(dwAni))
		{
            g_pLTClient->SetModelLooping(m_hObject, LTFALSE);
            g_pLTClient->SetModelAnimation(m_hObject, dwSelectAni);
            g_pLTClient->ResetModelAnimation(m_hObject);
		}

		// Tell the server we're playing the select animation...

        HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_PLAYER_CLIENTMSG);
        g_pLTClient->WriteToMessageByte(hMessage, CP_WEAPON_STATUS);
        g_pLTClient->WriteToMessageByte(hMessage, WS_SELECT);
        g_pLTClient->EndMessage(hMessage);
	}

	// Make sure the zipcord is off (incase it was on)...

	g_pGameClientShell->GetMoveMgr()->TurnOffZipCord();
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GadgetReload()
//
//	PURPOSE:	Handle gadget reload
//
// ----------------------------------------------------------------------- //

void CWeaponModel::GadgetReload()
{
	// Handle sunglasses gadget...

	if (m_pAmmo->eType == GADGET)
	{
		if (GetState() == W_SELECT)
		{
			// This will be handled by HandleSunglassMode() if we're selecting
			// the gadget...
			g_pInterfaceMgr->SetSunglassMode(SUN_NONE);
			return;
		}

		if (m_pAmmo->eInstDamageType == DT_GADGET_CAMERA)
		{
			g_pInterfaceMgr->SetSunglassMode(SUN_CAMERA);
		}
		else if (m_pAmmo->eInstDamageType == DT_GADGET_MINE_DETECTOR)
		{
			g_pInterfaceMgr->SetSunglassMode(SUN_MINES);
		}
		else if (m_pAmmo->eInstDamageType == DT_GADGET_INFRA_RED)
		{
			g_pInterfaceMgr->SetSunglassMode(SUN_IR);
		}
		else
		{
			g_pInterfaceMgr->SetSunglassMode(SUN_NONE);
		}
	}
	else
	{
		g_pInterfaceMgr->SetSunglassMode(SUN_NONE);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::Deselect()
//
//	PURPOSE:	Deselect the weapon
//
// ----------------------------------------------------------------------- //

void CWeaponModel::Deselect()
{
    uint32 dwDeselectAni = GetDeselectAni();
    LTBOOL bPlayDeselectAni = (m_hObject && dwDeselectAni != INVALID_ANI);

	// Special case for gadgets...

	if (m_pAmmo->eType == GADGET && IsOutOfAmmo(m_nWeaponId))
	{
        bPlayDeselectAni = LTFALSE;
	}

	if (bPlayDeselectAni)
	{
        uint32 dwAni = g_pLTClient->GetModelAnimation(m_hObject);

		SetState(W_DESELECT);

		if (!IsDeselectAni(dwAni))
		{
            g_pLTClient->SetModelLooping(m_hObject, LTFALSE);
            g_pLTClient->SetModelAnimation(m_hObject, dwDeselectAni);
            g_pLTClient->ResetModelAnimation(m_hObject);
		}

		// Tell the server we're playing the deselect animation...

        HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_PLAYER_CLIENTMSG);
        g_pLTClient->WriteToMessageByte(hMessage, CP_WEAPON_STATUS);
        g_pLTClient->WriteToMessageByte(hMessage, WS_DESELECT);
        g_pLTClient->EndMessage(hMessage);
	}
	else
	{
        m_bWeaponDeselected = LTTRUE;
	}

}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::SendFireMsg
//
//	PURPOSE:	Send fire message to server
//
// ----------------------------------------------------------------------- //

void CWeaponModel::SendFireMsg()
{
	if (!m_hObject) return;


	// Special case, check for gadget modes that don't actually support
	// firing...

	if ((m_pAmmo->eInstDamageType == DT_GADGET_MINE_DETECTOR) ||
		(m_pAmmo->eInstDamageType == DT_GADGET_INFRA_RED))
	{
		return;
	}


	LTVector vU, vR, vF, vFirePos;
    LTFLOAT fPerturb = m_fMovementPerturb;

	if (!g_pGameClientShell->IsMultiplayerGame())
	{
		CPlayerSummaryMgr *pPSummary = g_pGameClientShell->GetPlayerSummary();
        LTFLOAT  fPerturbX = pPSummary->m_PlayerRank.fPerturbMultiplier;
		fPerturb *= fPerturbX;
	}

	if (!GetFireInfo(vU, vR, vF, vFirePos)) return;


	// Make sure we always ignore the fire sounds...

	m_wIgnoreFX = WFX_FIRESOUND | WFX_ALTFIRESND;

	if (!m_bHaveSilencer)
	{
		m_wIgnoreFX |= WFX_SILENCED;
	}

	// Calculate a random seed...(srand uses this value so it can't be 1, since
	// that has a special meaning for srand)

    uint8 nRandomSeed = GetRandom(2, 255);

	g_nRandomWeaponSeed = nRandomSeed;


	// Create a client-side projectile for every vector...

	WeaponPath wp;
	wp.nWeaponId = m_nWeaponId;
	wp.vU		 = vU;
	wp.vR		 = vR;
	wp.fPerturbR = fPerturb;
	wp.fPerturbU = wp.fPerturbR;

	for (int i=0; i < m_pWeapon->nVectorsPerRound; i++)
	{
		wp.vPath = vF;

		//srand(g_nRandomWeaponSeed);
		//g_nRandomWeaponSeed = GetRandom(2, 255);

		g_pWeaponMgr->CalculateWeaponPath(wp);

        // g_pLTClient->CPrint("Client Fire Path (%d): %.2f, %.2f, %.2f",
		// g_nRandomWeaponSeed, wp.vPath.x, wp.vPath.y, wp.vPath.z);

		// Do client-side firing...

		ClientFire(wp.vPath, vFirePos);

		// A SHOT YOU FEEL. One pulse in the firing hand per shot: a gun kicks
		// harder and longer than a fist or a knife. Headset testing found the
		// shots felt as if they had no force. VRHaptics 0 turns it off.
		if (g_vtVRHaptics.GetFloat() > 0.0f)
		{
			WEAPON* pHW = g_pWeaponMgr->GetWeapon(m_nWeaponId);
			const bool bMelee = (pHW && pHW->nRange < 300);
			VRShared::Haptic(1, bMelee ? 0.5f : 1.0f, bMelee ? 35.0f : 70.0f);
		}
	}


	// Play Fire sound...

    uint8 nFireType = GetLastSndFireType();

	PlayerSoundId eSoundId = PSI_FIRE;
	if (nFireType == PSI_SILENCED_FIRE)
	{
		eSoundId = PSI_SILENCED_FIRE;
	}
	else if (nFireType == PSI_ALT_FIRE)
	{
		eSoundId = PSI_ALT_FIRE;
	}

	LTVector vPos(0, 0, 0);
	PlayWeaponSound(m_pWeapon, vPos, eSoundId, LTTRUE);


	// Send Fire message to server...

	if (m_pAmmo->eType != GADGET)
	{
        HMESSAGEWRITE hWrite = g_pLTClient->StartMessage(MID_WEAPON_FIRE);
        g_pLTClient->WriteToMessageVector(hWrite, &m_vFlashPos);
        g_pLTClient->WriteToMessageVector(hWrite, &vFirePos);
        g_pLTClient->WriteToMessageVector(hWrite, &vF);
        g_pLTClient->WriteToMessageByte(hWrite, nRandomSeed);
        g_pLTClient->WriteToMessageByte(hWrite, m_nWeaponId);
        g_pLTClient->WriteToMessageByte(hWrite, m_nAmmoId);
        g_pLTClient->WriteToMessageByte(hWrite, (LTBOOL) (m_eLastFireType == FT_ALT_FIRE));
        g_pLTClient->WriteToMessageByte(hWrite, (uint8) (fPerturb * 255.0f));
		g_pLTClient->WriteToMessageDWord(hWrite, (int) (g_pLTClient->GetTime() * 1000.0f));
        g_pLTClient->EndMessage2(hWrite, MESSAGE_NAGGLEFAST);
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetFireInfo
//
//	PURPOSE:	Get the fire pos/rot
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::GetFireInfo(LTVector & vU, LTVector & vR, LTVector & vF,
                                LTVector & vFirePos)
{
	// Get the fire position / direction from the camera (so it lines
	// up correctly with the crosshairs...

    LTRotation rRot;
	if (g_pGameClientShell->IsFirstPerson() &&
		!g_pGameClientShell->IsUsingExternalCamera())
	{
		HOBJECT hCamera = g_pGameClientShell->GetCamera();
        if (!hCamera) return LTFALSE;

		g_pLTClient->GetObjectPos(hCamera, &vFirePos);
		g_pLTClient->GetObjectRotation(hCamera, &rRot);
	    g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);

		// SHOOT WHERE THE HAND POINTS, NOT WHERE THE HEAD LOOKS.
		//
		// This function is the single choke point for the aim: everything
		// downstream - the projectile, the impact FX, the bullet holes, what
		// the server is told - is built from the vU/vR/vF that leave here. So
		// this is the one place the hand has to reach, and moving the weapon
		// MODEL was never going to do it.
		//
		// Composed the same way the weapon's position is: take the hand's
		// direction RELATIVE TO THE HEAD, then express it in the camera's
		// basis. That is explicit vector arithmetic rather than LTRotation
		// composition, deliberately - the two conventions have cost this
		// project a reconstruction before, and this way there is nothing to be
		// wrong about.
		//
		// Falls through to the camera untouched when there is no host, when
		// the controller is not tracking, or when the switch is off, so a flat
		// run is unaffected.
		if (g_vtVRHandFire.GetFloat() > 0.0f && VRShared::IsLive())
		{
			const VRSharedState& s = VRShared::State();
			const VRHandState&   h = s.Hands[1];		// right hand
			if (h.nActive)
			{
				const float fD2R = 0.01745329f;
				// ABSOLUTE, NOT RELATIVE TO THE HEAD, and this is the whole
				// difference between shooting where you point and shooting
				// where the mouse last was.
				//
				// The weapon MODEL subtracts the head, correctly: it is placed
				// in camera space and the renderer applies the head pose on top
				// of that, so subtracting avoids counting the head twice.
				//
				// The FIRE direction is not in camera space. It is built from
				// the camera OBJECT's rotation, and that object does not carry
				// the head pose at all - measured: with the headset pitched 25
				// degrees down the camera forward came back +0.000 in Y, dead
				// horizontal. The head is applied downstream, in the renderer,
				// to the VIEW only.
				//
				// So there is no head in the basis to double-count, and
				// subtracting one removes the thing that makes looking and
				// pointing agree: turn your head 30 degrees, point your hand
				// down your own gaze, and the relative angle is zero - it would
				// shoot straight ahead while you look sideways.
				//
				// It also means AIM HAS NEVER FOLLOWED THE HEAD in this port,
				// with or without a controller. Shots have always gone where
				// the flat game's aim pointed.
				const float fYaw   = -h.fYawDeg   * fD2R;
				const float fPitch = -h.fPitchDeg * fD2R;

				// THE SHOT FOLLOWS THE DRAWN BARREL. The per-weapon ANGLE rotates
				// the model in the hand; without it here the bullet, the reticle,
				// the beam and the burst kept the HAND's line while the barrel sat
				// at the tuned angle - the Contender, 22 degrees apart, in headset
				// testing on 21 September. Same terms, same signs as the model's
				// rotation; a gun with no angle is unchanged. VRAimFollowsBarrel 0
				// aims the hand alone.
				VRWeaponVarsInit();
				static VarTrack s_vtAimBarrel;
				if (!s_vtAimBarrel.IsInitted()) s_vtAimBarrel.Init(g_pLTClient, "VRAimFollowsBarrel", LTNULL, 1.0f);
				const float fAimOn    = (s_vtAimBarrel.GetFloat() > 0.0f) ? 1.0f : 0.0f;
				float fTrimYawDeg = 0.0f, fTrimPitchDeg = 0.0f;
				VRAimTotal(m_nWeaponId, fTrimYawDeg, fTrimPitchDeg);
				const float fAimYaw   =  s_vrAngleYaw  .Get(m_nWeaponId) * fD2R * fAimOn
				                      +  fTrimYawDeg * fD2R;
				const float fAimPitch = -s_vrAnglePitch.Get(m_nWeaponId) * fD2R * fAimOn
				                      -  fTrimPitchDeg * fD2R;

				LTRotation rHand;
				rHand.Init();
				g_pLTClient->EulerRotateY(&rHand, fYaw + fAimYaw);
				g_pLTClient->EulerRotateX(&rHand, fPitch + fAimPitch);

				LTVector vHU, vHR, vHF;
				g_pLTClient->GetRotationVectors(&rHand, &vHU, &vHR, &vHF);

				const LTVector vCU = vU, vCR = vR, vCF = vF;
				vF = vCR * vHF.x + vCU * vHF.y + vCF * vHF.z;
				vU = vCR * vHU.x + vCU * vHU.y + vCF * vHU.z;
				vR = vCR * vHR.x + vCU * vHR.y + vCF * vHR.z;

				// THE RAY STARTS AT THE HAND, NOT THE EYE.
				//
				// The direction was the hand's and the ORIGIN was still the
				// camera's, so the ray ran parallel to the barrel from a point
				// 40-odd centimetres above it (the log: the gun's centre sits
				// -25 units, 43 cm, below the eye). A parallel line 43 cm up
				// lands 43 cm above where the barrel points, at every range,
				// and the dot went with it. In the headset the aim dot sat too
				// high and not where the barrel pointed - while the
				// drawn barrel's pitch tracked the controller's within a
				// degree (GUN DIR lines: hand +24.3 barrel +23.6, -11.6 and
				// -11.8, +18.0 and +17.9). Not an angle error; a parallax.
				//
				// The origin is where the gun is drawn: the hand's offset from
				// the head, in the same basis and through the same two scales
				// the placement uses (VRHandPosScale units per metre in the
				// weapon's space, times VRViewModelScale into the world), so
				// the ray and the picture cannot drift apart. Only with the
				// gun at the hand; the authored-offset model keeps the eye.
				const LTVector vEyeOrigin = vFirePos;
				LTVector vOrgShift(0.0f, 0.0f, 0.0f);
				if (g_vtVRGunAtHand.GetFloat() > 0.0f)
				{
					const float U = (g_vtVRHandPosScale.GetFloat() > 0.0f)
						? g_vtVRHandPosScale.GetFloat() : 3.0f;
					const float K = (g_vtVRViewModelScale.GetFloat() > 0.1f)
						? g_vtVRViewModelScale.GetFloat() : 17.0f;
					const float hx =  (h.fPosX - s.fHeadPosX) * U * K;
					const float hy =  (h.fPosY - s.fHeadPosY) * U * K;
					const float hz = -(h.fPosZ - s.fHeadPosZ) * U * K;	// Z flip
					vOrgShift = vCR * hx + vCU * hy + vCF * hz;

					// ...BUT NEVER FROM INSIDE A WALL. At a window the hand
					// and the rifle reach past the sill into the wall, and a
					// ray that starts inside the wall hits it at once: the dot
					// sat on the open window and nothing across the street
					// could be aimed at. Headset testing, Morocco's sniping
					// section: the red dot stuck on the window. The segment from
					// the eye to the hand is tested first; if it meets
					// anything solid the origin stops just short of it, so
					// the ray still runs down the barrel's line but begins in
					// open air.
					{
						ClientIntersectQuery iq;
						ClientIntersectInfo  ii;
						memset(&iq, 0, sizeof(iq));
						HLOCALOBJ hPl = g_pLTClient->GetClientObject();
						HOBJECT hFilt[] = { hPl, m_hObject, LTNULL };
						VEC_COPY(iq.m_From, vFirePos);
						LTVector vTo = vFirePos + vOrgShift;
						VEC_COPY(iq.m_To, vTo);
						iq.m_Flags     = INTERSECT_OBJECTS | IGNORE_NONSOLID;
						iq.m_FilterFn  = ObjListFilterFn;
						iq.m_pUserData = hFilt;
						if (g_pLTClient->IntersectSegment(&iq, &ii))
						{
							// FROM THE EYE, THEN. Stopping short of the wall was not
							// enough: at a window the hand is BELOW the sill, so an
							// origin two units in front of the wall is two units in
							// front of the wall under the window, and the ray along
							// the barrel goes straight into it - the dot sat on the
							// glass of an open window (run 12). The eye is above the
							// sill by construction (you are looking out), so when the
							// hand is buried the ray runs from the eye along the
							// barrel's direction, the way it did before the parallax
							// fix. A little high at the muzzle, and never in a wall.
							static int s_nSaidWall = 0;
							if (s_nSaidWall++ % 90 == 0)
								VRLog::Msg("VR fire: the hand is past a wall (%.1f of %.1f units) - firing from the eye",
									(ii.m_Point - vFirePos).Mag(), vOrgShift.Mag());
							vOrgShift.Init();
						}
					}
					vFirePos += vOrgShift;
				}

				// A SCOPED SHOT STARTS WHERE THE SCOPE LOOKS FROM. The lens's
				// picture is rendered from the scope's objective, placed a
				// little ahead of the tube so it starts in open air - and the
				// bullet started back at the hand. At a window the objective
				// clears the frame while the hand does not: the scope showed a
				// clear line to the target and the shot met the frame (a
				// tester, Morocco's sniping section: the assassin on the far
				// right could not be hit through the left window). With a lens
				// drawn, the shot leaves from the objective along the barrel,
				// so anything the scope shows can be hit. Only when the eye can
				// reach the objective in the open - it is never fired from
				// inside a wall. VRScopeFireOrigin 0 turns this off.
				{
					static VarTrack s_vtScopeOrg;
					if (!s_vtScopeOrg.IsInitted()) s_vtScopeOrg.Init(g_pLTClient, "VRScopeFireOrigin", LTNULL, 1.0f);
					LTVector vObj;
					if (s_vtScopeOrg.GetFloat() > 0.0f && VRPrims_GetScopeObjective(vObj))
					{
						ClientIntersectQuery iq;
						ClientIntersectInfo  ii;
						memset(&iq, 0, sizeof(iq));
						HLOCALOBJ hPl = g_pLTClient->GetClientObject();
						HOBJECT hFilt[] = { hPl, m_hObject, LTNULL };
						VEC_COPY(iq.m_From, vEyeOrigin);
						VEC_COPY(iq.m_To, vObj);
						iq.m_Flags     = INTERSECT_OBJECTS | IGNORE_NONSOLID;
						iq.m_FilterFn  = ObjListFilterFn;
						iq.m_pUserData = hFilt;
						const bool bBlocked = g_pLTClient->IntersectSegment(&iq, &ii) ? true : false;
						static int s_nSaidScopeOrg = 0;
						if (s_nSaidScopeOrg++ % 90 == 0)
							VRLog::Msg("VR fire: scope lens drawn - origin %s the objective (%.1f units from the hand's origin)",
								bBlocked ? "NOT moved to (a wall between the eye and)" : "moved to",
								(vObj - vFirePos).Mag());
						if (!bBlocked) vFirePos = vObj;
					}
				}

				// WHERE IT ENDED UP, on every change of hand angle. A shot
				// going somewhere unexpected is the hardest thing in this
				// project to see from a screenshot - the bullet is gone before
				// the frame is captured - so the direction is written down
				// instead of photographed.
				// Throttled to four a second: see the note in UpdateWeaponPosition.
				// The half-degree test is not a cap when the hand never stops.
				static float  s_fSaidFire = -9999.0f;
				static double s_fSaidFireAt = -1.0;
				const double fNowFire = g_pLTClient->GetTime();
				if (fabsf(h.fYawDeg - s_fSaidFire) > 0.5f
					&& (fNowFire - s_fSaidFireAt) > 0.25)
				{
					s_fSaidFire = h.fYawDeg;
					s_fSaidFireAt = fNowFire;
					VRLog::Msg("VR fire: hand y%+.1f p%+.1f (head y%+.1f p%+.1f)"
						" -> dir %+.3f %+.3f %+.3f, camera would have been"
						" %+.3f %+.3f %+.3f | origin moved off the eye by"
						" %+.1f %+.1f %+.1f units",
						h.fYawDeg, h.fPitchDeg, s.fHeadYawDeg, s.fHeadPitchDeg,
						vF.x, vF.y, vF.z, vCF.x, vCF.y, vCF.z,
						vOrgShift.x, vOrgShift.y, vOrgShift.z);
				}
			}
		}
	}
	else
	{
		vFirePos = GetModelPos();
		rRot	 = GetModelRot();
		g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);
	}

    return LTTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetLastSndFireType
//
//	PURPOSE:	Get the last fire snd type
//
// ----------------------------------------------------------------------- //

uint8 CWeaponModel::GetLastSndFireType()
{
	// Determine the fire snd type...

    uint8 nFireType = PSI_FIRE;

	if (m_bHaveSilencer)
	{
		nFireType = PSI_SILENCED_FIRE;
	}
	else if (m_eLastFireType == FT_ALT_FIRE)
	{
		nFireType = PSI_ALT_FIRE;
	}

	return nFireType;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::ClientFire
//
//	PURPOSE:	Do client-side weapon firing
//
// ----------------------------------------------------------------------- //

void CWeaponModel::ClientFire(LTVector & vPath, LTVector & vFirePos)
{
	m_vPath		= vPath;
	m_vFirePos	= vFirePos;

	// Always process gadget firing...

	if (m_pAmmo->eType == GADGET)
	{
		DoGadget();
		return;
	}

	// Only process the rest of these if this is a multiplayer game...

	if (!g_pGameClientShell->IsMultiplayerGame()) return;

	switch (m_pAmmo->eType)
	{
		case PROJECTILE :
		{
			DoProjectile();
		}
		break;

		case VECTOR :
		{
			DoVector();
		}
		break;

		default :
		{
            g_pLTClient->CPrint("ERROR in CWeaponModel::ClientFire().  Invalid Ammo Type!");
		}
		break;
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::DoProjectile
//
//	PURPOSE:	Do client-side projectile
//
// ----------------------------------------------------------------------- //

void CWeaponModel::DoProjectile()
{
	// All projectiles are server-side (for now)...
	return;

	if (!m_hObject) return;

	CSFXMgr* psfxMgr = g_pGameClientShell->GetSFXMgr();
	if (!psfxMgr) return;

	PROJECTILECREATESTRUCT projectile;

    uint32 dwId;
    g_pLTClient->GetLocalClientID(&dwId);

	projectile.hServerObj = CreateServerObj();
	projectile.nWeaponId  = m_nWeaponId;
	projectile.nAmmoId	  = m_nAmmoId;
    projectile.nShooterId = (uint8)dwId;
    projectile.bLocal     = LTTRUE;
	projectile.bAltFire	  = !!(m_eLastFireType == FT_ALT_FIRE);


	psfxMgr->CreateSFX(SFX_PROJECTILE_ID, &projectile);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::CreateServerObj
//
//	PURPOSE:	Create a "server" object used by the projectile sfx
//
// ----------------------------------------------------------------------- //

HLOCALOBJ CWeaponModel::CreateServerObj()
{
    if (!m_hObject) return LTNULL;

    LTRotation rRot;
    g_pLTClient->AlignRotation(&rRot, &m_vPath, LTNULL);

	ObjectCreateStruct createStruct;
	INIT_OBJECTCREATESTRUCT(createStruct);

    uint32 dwFlags = FLAG_POINTCOLLIDE | FLAG_NOSLIDING | FLAG_TOUCH_NOTIFY;
	dwFlags |= m_pAmmo->pProjectileFX ? m_pAmmo->pProjectileFX->dwObjectFlags : 0;

	createStruct.m_ObjectType = OT_NORMAL;
	createStruct.m_Flags = dwFlags;
	createStruct.m_Pos = m_vFirePos;
	createStruct.m_Rotation = rRot;

    HLOCALOBJ hObj = g_pLTClient->CreateObject(&createStruct);

    g_pLTClient->Physics()->SetForceIgnoreLimit(hObj, 0.0f);

	return hObj;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::DoGadget
//
//	PURPOSE:	Do client-side gadget
//
// ----------------------------------------------------------------------- //

void CWeaponModel::DoGadget()
{
	if (m_pAmmo->eInstDamageType == DT_GADGET_POODLE)
	{
        LTVector vImpactPoint(0, 0, 0);
        HandleGadgetImpact(LTNULL, vImpactPoint);
	}
	else
	{
		// Do Camera shutter fx...

		if (m_pAmmo->eInstDamageType == DT_GADGET_CAMERA)
		{
			g_pInterfaceMgr->StartScreenFadeIn(g_vtCameraShutterSpeed.GetFloat());
		}

		DoVector();
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::DoVector
//
//	PURPOSE:	Do client-side vector
//
// ----------------------------------------------------------------------- //

void CWeaponModel::DoVector()
{
	if (!m_hObject) return;

	IntersectInfo iInfo;
	IntersectQuery qInfo;
	qInfo.m_Flags = INTERSECT_OBJECTS | IGNORE_NONSOLID | INTERSECT_HPOLY;

    LTVector vTemp;
	VEC_MULSCALAR(vTemp, m_vPath, m_pWeapon->nRange);
	VEC_ADD(m_vEndPos, m_vFirePos, vTemp);

    HOBJECT hFilterList[] = {g_pLTClient->GetClientObject(),
        g_pGameClientShell->GetMoveMgr()->GetObject(), LTNULL};

	qInfo.m_FilterFn  = ObjListFilterFn;
	qInfo.m_pUserData = hFilterList;

	qInfo.m_From = m_vFirePos;
	qInfo.m_To = m_vEndPos;

    if (g_pLTClient->IntersectSegment(&qInfo, &iInfo))
	{
		HandleVectorImpact(qInfo, iInfo);
	}
	else
	{
        LTVector vUp;
		vUp.Init(0.0f, 1.0f, 0.0f);
        AddImpact(LTNULL, m_vEndPos, vUp, ST_SKY);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::AddImpact
//
//	PURPOSE:	Add the weapon impact
//
// ----------------------------------------------------------------------- //

void CWeaponModel::AddImpact(HLOCALOBJ hObj, LTVector & vImpactPoint,
                             LTVector & vNormal, SurfaceType eType)
{
	// Handle gadget special case...

	if (m_pAmmo->eType == GADGET)
	{
		HandleGadgetImpact(hObj, vImpactPoint);
		return;  // No impact fx for gadgets...
	}

	// See if we should do tracers or not...

	if (m_pAmmo->pTracerFX)
	{
		++m_nCurTracer;
		if ((m_nCurTracer % m_pAmmo->pTracerFX->nFrequency) != 0)
		{
			m_wIgnoreFX |= WFX_TRACER;
		}
	}
	else
	{
		m_wIgnoreFX |= WFX_TRACER;
	}

	::AddLocalImpactFX(hObj, m_vFlashPos, vImpactPoint, vNormal, eType,
					   m_vPath, m_nWeaponId, m_nAmmoId, m_wIgnoreFX);

	// If we do multiple calls to AddLocalImpact, make sure we only do some
	// effects once :)

	m_wIgnoreFX |= WFX_SILENCED | WFX_SHELL | WFX_LIGHT | WFX_MUZZLE | WFX_TRACER;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CProjectile::HandleVectorImpact
//
//	PURPOSE:	Handle a vector hitting something
//
// ----------------------------------------------------------------------- //

void CWeaponModel::HandleVectorImpact(IntersectQuery & qInfo, IntersectInfo & iInfo)
{
	// Get the surface type (check the poly first)...

	SurfaceType eType = GetSurfaceType(iInfo.m_hPoly);

	if (eType == ST_UNKNOWN)
	{
		eType = GetSurfaceType(iInfo.m_hObject);
	}

	AddImpact(iInfo.m_hObject, iInfo.m_Point, iInfo.m_Plane.m_Normal, eType);

	// If we hit liquid, cast another ray that will go through the water...

	if (eType == ST_LIQUID)
	{
		qInfo.m_FilterFn = AttackerLiquidFilterFn;

        if (g_pLTClient->IntersectSegment(&qInfo, &iInfo))
		{
			// Get the surface type (check the poly first)...

			SurfaceType eType = GetSurfaceType(iInfo.m_hPoly);

			if (eType == ST_UNKNOWN)
			{
				eType = GetSurfaceType(iInfo.m_hObject);
			}

			AddImpact(iInfo.m_hObject, iInfo.m_Point, iInfo.m_Plane.m_Normal, eType);
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CProjectile::HandleGadgetImpact
//
//	PURPOSE:	Handle a gadget vector hitting an object
//
// ----------------------------------------------------------------------- //

void CWeaponModel::HandleGadgetImpact(HOBJECT hObj, LTVector vImpactPoint)
{
	// If the gadget can activate this type of object, Tell the server
	// that the gadget was activated on this object...

    LTVector vU, vR, vF, vFirePos;
	if (!GetFireInfo(vU, vR, vF, vFirePos)) return;

    uint32 dwUserFlags = 0;
	if (hObj)
	{
        g_pLTClient->GetObjectUserFlags(hObj, &dwUserFlags);
	}

	DamageType eType = m_pAmmo->eInstDamageType;

    LTBOOL bTestDamageType = LTTRUE;

	// Make sure the object isn't a character object (gadget and character
	// user flags overlap) unless this is a character specific gadget...

	if (dwUserFlags & USRFLG_CHARACTER)
	{
		// Only the lighter is used with character models currently...

		if (eType == DT_GADGET_LIGHTER)
		{
			// See if we're oriented correctly with the character
			// to use the lighter...

            LTVector vPos, vObjU, vObjR, vObjF;
			vPos = GetModelPos();

            LTVector vDir;
			vDir = vPos - vImpactPoint;
			vDir.Norm();

            LTRotation rRot;// = GetModelRot();
			g_pLTClient->GetObjectRotation(hObj, &rRot);
            g_pLTClient->GetRotationVectors(&rRot, &vObjU, &vObjR, &vObjF);

            LTFLOAT fMul = VEC_DOT(vDir, vObjF);
			if (fMul <= 0.5f) return;

			// Everything's cool...Light that baby...

            bTestDamageType = LTFALSE;
		}
		else
		{
			return;
		}
	}


	// Test the damage type if necessary...

	if (bTestDamageType)
	{
		if (eType == DT_GADGET_CAMERA_DISABLER)
		{
			// Make sure the object can be disabled...

			if (!(dwUserFlags & USRFLG_GADGET_CAMERA_DISABLER)) return;
		}
		else if (eType == DT_GADGET_CODE_DECIPHERER)
		{
			// Make sure the object can be deciphered...

			if (!(dwUserFlags & USRFLG_GADGET_CODE_DECIPHERER)) return;
		}
		else if (eType == DT_GADGET_LOCK_PICK)
		{
			// Make sure the object is "pickable"...

			if (!(dwUserFlags & USRFLG_GADGET_LOCK_PICK)) return;
		}
		else if (eType == DT_GADGET_LIGHTER)
		{
			// Make sure the object is "lightable"...

			if (!(dwUserFlags & USRFLG_GADGET_LIGHTER)) return;
		}
		else if (eType == DT_GADGET_WELDER)
		{
			// Make sure the object is "weldable"...

			if (!(dwUserFlags & USRFLG_GADGET_WELDER)) return;
		}
		else if (eType == DT_GADGET_CAMERA)
		{
			// Make sure the object is something we can photograph...

			if (dwUserFlags & USRFLG_GADGET_INTELLIGENCE)
			{
				// Make sure we're in camera range...

				if (!g_pGameClientShell->InCameraGadgetRange(hObj)) return;
			}
			else
			{
				return;
			}
		}
		else if (eType == DT_GADGET_ZIPCORD)
		{
			// Make sure the object is something we can zipcord to...

			if (dwUserFlags & USRFLG_GADGET_ZIPCORD)
			{
				// Tell the MoveMgr we're zipcording...

				if (!g_pGameClientShell->GetMoveMgr()->IsZipCordOn())
				{
					g_pGameClientShell->GetMoveMgr()->TurnOnZipCord(hObj);
				}
			}
			else
			{
				return;
			}
		}
		else if (eType != DT_GADGET_POODLE)
		{
			return;
		}
	}


    HMESSAGEWRITE hWrite = g_pLTClient->StartMessage(MID_WEAPON_FIRE);
    g_pLTClient->WriteToMessageVector(hWrite, &m_vFlashPos);
    g_pLTClient->WriteToMessageVector(hWrite, &vFirePos);
    g_pLTClient->WriteToMessageVector(hWrite, &vF);
    g_pLTClient->WriteToMessageByte(hWrite, 0);
    g_pLTClient->WriteToMessageByte(hWrite, m_nWeaponId);
    g_pLTClient->WriteToMessageByte(hWrite, m_nAmmoId);
    g_pLTClient->WriteToMessageByte(hWrite, (LTBOOL) (m_eLastFireType == FT_ALT_FIRE));
    g_pLTClient->WriteToMessageByte(hWrite, (uint8) (m_fMovementPerturb * 255.0f));
	g_pLTClient->WriteToMessageDWord(hWrite, (int) (g_pLTClient->GetTime() * 1000.0f));
    g_pLTClient->WriteToMessageObject(hWrite, hObj);
    g_pLTClient->EndMessage2(hWrite, MESSAGE_NAGGLEFAST);


	// Do any special processing...

	DoSpecialFire();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::CanChangeToWeapon()
//
//	PURPOSE:	See if we can change to this weapon
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::CanChangeToWeapon(uint8 nCommandId)
{
	if (g_pGameClientShell->IsPlayerDead() ||
        g_pGameClientShell->IsSpectatorMode()) return LTFALSE;

	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
    if (!pStats) return LTFALSE;

    uint8 nWeaponId = g_pWeaponMgr->GetWeaponId(nCommandId);


	// Make sure this is a valid weapon for us to switch to...

    if (!pStats->HaveWeapon(nWeaponId)) return LTFALSE;



	// If this weapon has no ammo, let user know...

	if (IsOutOfAmmo(nWeaponId))
	{
		g_pInterfaceMgr->UpdatePlayerStats(IC_OUTOFAMMO_ID, nWeaponId, 0, 0.0f);
        return LTFALSE;
	}

    return LTTRUE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::IsOutOfAmmo()
//
//	PURPOSE:	Do we have any ammo for this weapon
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::IsOutOfAmmo(uint8 nWeaponId)
{
	WEAPON* pWeapon = g_pWeaponMgr->GetWeapon(nWeaponId);
    if (!pWeapon) return LTTRUE;

	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
    if (!pStats) return LTTRUE;

	if (pWeapon->bInfiniteAmmo)
	{
        return LTFALSE;
	}
	else
	{
		for (int i=0; i < pWeapon->nNumAmmoTypes; i++)
		{
			if (pStats->GetAmmoCount(pWeapon->aAmmoTypes[i]) > 0)
			{
                return LTFALSE;
			}
		}
	}

    return LTTRUE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetFirstAvailableAmmoType()
//
//	PURPOSE:	Get the fire available ammo type for this weapon.
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::GetFirstAvailableAmmoType(uint8 nWeaponId, int & nAmmoType)
{
	nAmmoType = WMGR_INVALID_ID;

	WEAPON* pWeapon = g_pWeaponMgr->GetWeapon(nWeaponId);
    if (!pWeapon) return LTFALSE;

	// If we don't always have ammo, return an ammo type that we have (if
	// possible)...

	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
    if (!pStats) return LTFALSE;

	for (int i=0; i < pWeapon->nNumAmmoTypes; i++)
	{
		if (pStats->GetAmmoCount(pWeapon->aAmmoTypes[i]) > 0)
		{
			nAmmoType = pWeapon->aAmmoTypes[i];
            return LTTRUE;
		}
	}

	// If we get to here (which we shouldn't), just use the default ammo
	// type if this weapon uses infinite ammo...

	if (pWeapon->bInfiniteAmmo)
	{
		nAmmoType = pWeapon->nDefaultAmmoType;
        return LTTRUE;
	}

    return LTFALSE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetBestAvailableAmmoType()
//
//	PURPOSE:	Get the best available ammo type for this weapon.
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::GetBestAvailableAmmoType(uint8 nWeaponId, int & nAmmoType)
{
	nAmmoType = WMGR_INVALID_ID;

	WEAPON* pWeapon = g_pWeaponMgr->GetWeapon(nWeaponId);
    if (!pWeapon) return LTFALSE;

	// If this is a gadget, return the first ammo type...

	if (pWeapon->IsAGadget())
	{
		return GetFirstAvailableAmmoType(nWeaponId, nAmmoType);
	}
	

	// If we don't always have ammo, return an ammo type that we have (if
	// possible)...

	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
    if (!pStats) return LTFALSE;

	int nAmmoBest = WMGR_INVALID_ID;
	LTFLOAT fMaxPriority = -1.0f;

	for (int i=0; i < pWeapon->nNumAmmoTypes; i++)
	{
		if (pStats->GetAmmoCount(pWeapon->aAmmoTypes[i]) > 0)
		{
			int nAmmo = pWeapon->aAmmoTypes[i];
			AMMO* pAmmo = g_pWeaponMgr->GetAmmo(nAmmo);
			if ( pAmmo->fPriority > fMaxPriority )
			{
				nAmmoBest = nAmmo;
				fMaxPriority = pAmmo->fPriority;
			}
		}
	}

	if ( nAmmoBest != WMGR_INVALID_ID )
	{
		nAmmoType = nAmmoBest;
		return LTTRUE;
	}

	// If we get to here (which we shouldn't), just use the default ammo
	// type if this weapon uses infinite ammo...

	if (pWeapon->bInfiniteAmmo)
	{
		nAmmoType = pWeapon->nDefaultAmmoType;
        return LTTRUE;
	}

	nAmmoType = WMGR_INVALID_ID;
    return LTFALSE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::ChangeWeapon()
//
//	PURPOSE:	Change to a different weapon
//
// ----------------------------------------------------------------------- //

void CWeaponModel::ChangeWeapon(uint8 nCommandId, LTBOOL bCanDeselect, LTBOOL bDontCloseChooser)
{
	if (!CanChangeToWeapon(nCommandId)) return;

	// Don't do anything if we are trying to change to the same weapon...

    LTBOOL bDeselectWeapon = (m_nWeaponId != WMGR_INVALID_ID);
    uint8 nWeaponId = g_pWeaponMgr->GetWeaponId(nCommandId);

	if (nWeaponId == m_nWeaponId)
	{
		return;
	}

	if (!bDontCloseChooser && (g_pInterfaceMgr->IsChoosingWeapon() || g_pInterfaceMgr->IsChoosingAmmo()))
	{
		g_pInterfaceMgr->CloseChoosers();
	}

	// Handle deselection of current weapon...

	if (bDeselectWeapon && bCanDeselect)
	{
		// Need to wait for deselection animation to finish...Save the
		// new weapon id...

		m_nRequestedWeaponId = nWeaponId;

		Deselect();
	}
	else
	{
		HandleInternalWeaponChange(nWeaponId);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::HandleInternalWeaponChange()
//
//	PURPOSE:	Change to a different weapon
//
// ----------------------------------------------------------------------- //

void CWeaponModel::HandleInternalWeaponChange(uint8 nWeaponId)
{
	if (g_pGameClientShell->IsPlayerDead() ||
		g_pGameClientShell->IsSpectatorMode()) return;


	// Change to the weapon...

	DoWeaponChange(nWeaponId);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::DoWeaponChange()
//
//	PURPOSE:	Do the actual weapon change.  This isn't part of
//				HandleInternalWeaponChange() so that it can be called when
//				loading the player.
//
// ----------------------------------------------------------------------- //

void CWeaponModel::DoWeaponChange(uint8 nWeaponId)
{
	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
	if (!pStats) return;

	DoSpecialWeaponChange();

	if (pStats->HaveWeapon(nWeaponId))
	{
		int nAmmoId = WMGR_INVALID_ID;
		if (GetBestAvailableAmmoType(nWeaponId, nAmmoId) && (nAmmoId != WMGR_INVALID_ID))
		{
			g_pGameClientShell->ChangeWeapon(nWeaponId, nAmmoId, pStats->GetAmmoCount(nAmmoId));
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::DoSpecialWeaponChange()
//
//	PURPOSE:	Do special case weapon change processing...
//
// ----------------------------------------------------------------------- //

void CWeaponModel::DoSpecialWeaponChange()
{
	// Currently we need to check for gadget special cases...

	if (m_pAmmo->eType == GADGET)
	{
		// Show the necessary pieces (so they aren't hidden on the new
		// weapon model)...

        SpecialShowPieces(LTTRUE, LTTRUE);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::AutoSelectWeapon()
//
//	PURPOSE:	Determine what weapon to switch to, and switch
//
// ----------------------------------------------------------------------- //

void CWeaponModel::AutoSelectWeapon()
{
	// Try and auto-select a new ammo type before auto selecting a new weapon...

	int nAmmoId = WMGR_INVALID_ID;
	if (GetBestAvailableAmmoType(m_nWeaponId, nAmmoId) && (nAmmoId != WMGR_INVALID_ID))
	{
		// Set our ammo type...

		m_nAmmoId	= nAmmoId;
		m_pAmmo		= g_pWeaponMgr->GetAmmo(m_nAmmoId);

		// Reload our clip...

        ReloadClip(LTTRUE);
		return;
	}

	// Okay, need to change, find the next weapon/gadget that will
	// do damage...

	ChangeToNextRealWeapon();
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::ChangeToNextRealWeapon()
//
//	PURPOSE:	Change to the next weapon/gadget that does damage.
//				(used by the auto weapon switching)
//
// ----------------------------------------------------------------------- //

void CWeaponModel::ChangeToNextRealWeapon()
{
	if (!m_pWeapon) return;

	// If we're supposed to hide the weapon when it is empty (i.e.,
	// it doesn't make sense to see it, like for the poodle, or lipsticks)
	// then we don't want to play the deselect animation...

	LTBOOL bCanDeselect = !m_pWeapon->bHideWhenEmpty;

	// Find the next weapon that does damage...

	uint8 nCurrWeaponId = m_nWeaponId;

	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
	if (!pStats) return;

	int nMinWeapon = g_pWeaponMgr->GetFirstWeaponCommandId();
	int nMaxWeapon = g_pWeaponMgr->GetLastWeaponCommandId();
	int nOriginalWeapon = g_pWeaponMgr->GetCommandId(nCurrWeaponId);

	int nWeaponCommand = nOriginalWeapon + 1;
	if (nWeaponCommand > nMaxWeapon) nWeaponCommand = nMinWeapon;
	int nWeaponIndex = g_pWeaponMgr->GetWeaponId(nWeaponCommand);

	uint8 nMeleeId = WMGR_INVALID_ID;

	while (1)
	{
		if (pStats->HaveWeapon(nWeaponIndex) && !IsOutOfAmmo(nWeaponIndex))
		{
			WEAPON* pWeapon = g_pWeaponMgr->GetWeapon(nWeaponIndex);
			if (pWeapon)
			{
				AMMO* pAmmo = g_pWeaponMgr->GetAmmo(pWeapon->nDefaultAmmoType);
				if (pAmmo)
				{
					if (pAmmo->eInstDamageType == DT_MELEE)
					{
						nMeleeId = nWeaponIndex;
					}
					else if (!IsGadgetAmmo(pAmmo))
					{
						ChangeWeapon(g_pWeaponMgr->GetCommandId(nWeaponIndex), bCanDeselect);
						return;
					}
				}
			}
		}

		nWeaponCommand++;

		if (nWeaponCommand > nMaxWeapon) nWeaponCommand = nMinWeapon;

		if (nWeaponCommand == nOriginalWeapon) break;

		nWeaponIndex = g_pWeaponMgr->GetWeaponId(nWeaponCommand);
	}

	// Check to see if we should change to the melee weapon...

	if (nMeleeId != WMGR_INVALID_ID)
	{
		ChangeWeapon(g_pWeaponMgr->GetCommandId(nMeleeId), bCanDeselect);
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::IsGadgetAmmo()
//
//	PURPOSE:	Is the ammo used by a gadget
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::IsGadgetAmmo(AMMO* pAmmo)
{
	if (!pAmmo) return LTFALSE;

	if (IsGadgetType(pAmmo->eInstDamageType)) return LTTRUE;

	// Special case

	if (strcmpi(pAmmo->szName, "Coin") == 0) return LTTRUE;

	return LTFALSE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::PrevWeapon()
//
//	PURPOSE:	Determine what the previous weapon is
//
// ----------------------------------------------------------------------- //

uint8 CWeaponModel::PrevWeapon(uint8 nCurrWeaponId)
{
	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
	if (!pStats) return -1;

	if (!g_pWeaponMgr->IsValidWeapon(nCurrWeaponId))
	{
		nCurrWeaponId = m_nWeaponId;
	}

	if (!pStats->HaveWeapon(nCurrWeaponId)) return -1;

	int nMinWeapon = g_pWeaponMgr->GetFirstWeaponCommandId();
	int nMaxWeapon = g_pWeaponMgr->GetLastWeaponCommandId();
	int nOriginalWeapon = g_pWeaponMgr->GetCommandId(nCurrWeaponId);

	int nWeapon = nOriginalWeapon - 1;
	if (nWeapon < nMinWeapon) nWeapon = nMaxWeapon;
	int nWeaponIndex = g_pWeaponMgr->GetWeaponId(nWeapon);

	while (!pStats->HaveWeapon(nWeaponIndex) || IsOutOfAmmo(nWeaponIndex))
	{
		nWeapon--;

		if (nWeapon < nMinWeapon) nWeapon = nMaxWeapon;

		if (nWeapon == nOriginalWeapon) break;

		nWeaponIndex = g_pWeaponMgr->GetWeaponId(nWeapon);
	}

    return (uint8)g_pWeaponMgr->GetWeaponId(nWeapon);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::NextWeapon()
//
//	PURPOSE:	Determine what the next weapon is
//
// ----------------------------------------------------------------------- //

uint8 CWeaponModel::NextWeapon(uint8 nCurrWeaponId)
{
	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
	if (!pStats) return -1;

	if (!g_pWeaponMgr->IsValidWeapon(nCurrWeaponId))
	{
		nCurrWeaponId = m_nWeaponId;
	}

	if (!pStats->HaveWeapon(nCurrWeaponId)) return -1;

	int nMinWeapon = g_pWeaponMgr->GetFirstWeaponCommandId();
	int nMaxWeapon = g_pWeaponMgr->GetLastWeaponCommandId();
	int nOriginalWeapon = g_pWeaponMgr->GetCommandId(nCurrWeaponId);

	int nWeapon = nOriginalWeapon + 1;
	if (nWeapon > nMaxWeapon) nWeapon = nMinWeapon;
	int nWeaponIndex = g_pWeaponMgr->GetWeaponId(nWeapon);

	while (!pStats->HaveWeapon(nWeaponIndex) || IsOutOfAmmo(nWeaponIndex))
	{
		nWeapon++;

		if (nWeapon > nMaxWeapon) nWeapon = nMinWeapon;

		if (nWeapon == nOriginalWeapon) break;

		nWeaponIndex = g_pWeaponMgr->GetWeaponId(nWeapon);
	}

    return (uint8)g_pWeaponMgr->GetWeaponId(nWeapon);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::NextAmmo()
//
//	PURPOSE:	Determine the next ammo type
//
// ----------------------------------------------------------------------- //

uint8 CWeaponModel::NextAmmo(uint8 nCurrAmmoId)
{
	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
	if (!pStats || !m_pWeapon) return -1;

	if (!g_pWeaponMgr->IsValidAmmoType(nCurrAmmoId))
	{
		nCurrAmmoId = m_nAmmoId;
	}

	int nNewAmmoId = nCurrAmmoId;
	int nOriginalAmmoIndex = 0;
	int nCurAmmoIndex = 0;
	int nAmmoCount = 0;

	for (int i=0; i < m_pWeapon->nNumAmmoTypes; i++)
	{
		if (nCurrAmmoId == m_pWeapon->aAmmoTypes[i])
		{
			nOriginalAmmoIndex = i;
			nCurAmmoIndex = i;
			break;
		}
	}

	while (1)
	{
		nCurAmmoIndex++;

		if (nCurAmmoIndex >= m_pWeapon->nNumAmmoTypes) nCurAmmoIndex = 0;
		if (nCurAmmoIndex == nOriginalAmmoIndex) break;

		nAmmoCount = pStats->GetAmmoCount(m_pWeapon->aAmmoTypes[nCurAmmoIndex]);
		if (nAmmoCount > 0)
		{
			nNewAmmoId = m_pWeapon->aAmmoTypes[nCurAmmoIndex];
			break;
		}
	}

	return nNewAmmoId;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::ChangeAmmo()
//
//	PURPOSE:	Change to the specified ammo type
//
// ----------------------------------------------------------------------- //

void CWeaponModel::ChangeAmmo(uint8 nNewAmmoId, LTBOOL bForce)
{
	// Update the player's stats...

	if (GetState() == W_RELOADING && !bForce) return;

	
	// Make sure this is an ammo type our current weapon can use...

	if (!m_pWeapon) return;
	for (int i=0; i < m_pWeapon->nNumAmmoTypes; i++)
	{
		if (nNewAmmoId == m_pWeapon->aAmmoTypes[i])
		{
			break;
		}
	}
	if (i == m_pWeapon->nNumAmmoTypes) return;  // Not a valid ammo type


	if (g_pWeaponMgr->IsValidAmmoType(nNewAmmoId) && nNewAmmoId != m_nAmmoId)
	{
		m_nAmmoId	= nNewAmmoId;
		m_pAmmo		= g_pWeaponMgr->GetAmmo(m_nAmmoId);

		// Make sure we reset the anis...

		InitAnimations(LTTRUE);

		if (m_pAmmo->pAniOverrides)
		{
			// If we're not using the defaults play the new select ani...

			Select();
		}
		else
		{
			// Do normal reload...

            ReloadClip(LTTRUE, -1, LTTRUE);
		}
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetWeaponOffset()
//
//	PURPOSE:	Set the weapon offset
//
// ----------------------------------------------------------------------- //

LTVector CWeaponModel::GetWeaponOffset()
{
    LTVector vRet;
	vRet.Init();

	if (!m_pWeapon) return vRet;
	return m_pWeapon->vPos;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::SetWeaponOffset()
//
//	PURPOSE:	Set the weapon offset
//
// ----------------------------------------------------------------------- //

void CWeaponModel::SetWeaponOffset(LTVector vPos)
{
	WEAPON* pWeaponData = g_pWeaponMgr->GetWeapon(m_nWeaponId);

	if (!m_pWeapon) return;
	m_pWeapon->vPos = vPos;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetMuzzleOffset()
//
//	PURPOSE:	Set the weapon muzzle offset
//
// ----------------------------------------------------------------------- //

LTVector CWeaponModel::GetMuzzleOffset()
{
    LTVector vRet;
	vRet.Init();

	if (!m_pWeapon) return vRet;
	return m_pWeapon->vMuzzlePos;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::SetMuzzleOffset()
//
//	PURPOSE:	Set the muzzle offset
//
// ----------------------------------------------------------------------- //

void CWeaponModel::SetMuzzleOffset(LTVector vPos)
{
	if (!m_pWeapon) return;
	m_pWeapon->vMuzzlePos = vPos;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetShellEjectPos()
//
//	PURPOSE:	Get the shell eject pos
//
// ----------------------------------------------------------------------- //

LTVector CWeaponModel::GetShellEjectPos(LTVector & vOriginalPos)
{
    LTVector vPos = vOriginalPos;

	if (m_hObject && m_hBreachSocket != INVALID_MODEL_SOCKET)
	{
		LTransform transform;
        if (g_pModelLT->GetSocketTransform(m_hObject, m_hBreachSocket, transform, LTTRUE) == LT_OK)
		{
			g_pTransLT->GetPos(transform, vPos);
		}
	}

	return vPos;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetNextIdleTime()
//
//	PURPOSE:	Determine the next time we should play an idle animation
//
// ----------------------------------------------------------------------- //

LTFLOAT CWeaponModel::GetNextIdleTime()
{
    return g_pLTClient->GetTime() + GetRandom(WEAPON_MIN_IDLE_TIME, WEAPON_MAX_IDLE_TIME);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetFireAni()
//
//	PURPOSE:	Get the fire animation based on the fire type
//
// ----------------------------------------------------------------------- //

uint32 CWeaponModel::GetFireAni(FireType eFireType)
{
	int nNumValid = 0;

	if ((eFireType == FT_ALT_FIRE && !CanUseAltFireAnis()) ||
		(m_bUsingAltFireAnis && eFireType == FT_NORMAL_FIRE))
	{
        uint32 dwValidAltFireAnis[WM_MAX_ALTFIRE_ANIS];

		for (int i=0; i < WM_MAX_ALTFIRE_ANIS; i++)
		{
			if (m_nAltFireAnis[i] != INVALID_ANI)
			{
				dwValidAltFireAnis[nNumValid] = m_nAltFireAnis[i];
				nNumValid++;
			}
		}

		if (nNumValid > 0)
		{
			return dwValidAltFireAnis[GetRandom(0, nNumValid-1)];
		}
	}
	else if (eFireType == FT_NORMAL_FIRE)
	{
        uint32 dwValidFireAnis[WM_MAX_FIRE_ANIS];

		for (int i=0; i < WM_MAX_FIRE_ANIS; i++)
		{
			if (m_nFireAnis[i] != INVALID_ANI)
			{
				dwValidFireAnis[nNumValid] = m_nFireAnis[i];
				nNumValid++;
			}
		}

		if (nNumValid > 0)
		{
			return dwValidFireAnis[GetRandom(0, nNumValid-1)];
		}
	}

	return INVALID_ANI;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetIdleAni()
//
//	PURPOSE:	Get an idle animation
//
// ----------------------------------------------------------------------- //

uint32 CWeaponModel::GetIdleAni()
{
	int nNumValid = 0;

	if (m_bUsingAltFireAnis)
	{
        uint32 dwValidAltIdleAnis[WM_MAX_ALTIDLE_ANIS];

		// Note that we skip the first ani, this is reserved for
		// the subtle idle ani...

		for (int i=1; i < WM_MAX_ALTIDLE_ANIS; i++)
		{
			if (m_nAltIdleAnis[i] != INVALID_ANI)
			{
				dwValidAltIdleAnis[nNumValid] = m_nAltIdleAnis[i];
				nNumValid++;
			}
		}

		if (nNumValid > 0)
		{
			return dwValidAltIdleAnis[GetRandom(0, nNumValid-1)];
		}
	}
	else  // Normal idle anis
	{
        uint32 dwValidIdleAnis[WM_MAX_IDLE_ANIS];

		// Note that we skip the first ani, this is reserved for
		// the subtle idle ani...

		for (int i=1; i < WM_MAX_IDLE_ANIS; i++)
		{
			if (m_nIdleAnis[i] != INVALID_ANI)
			{
				dwValidIdleAnis[nNumValid] = m_nIdleAnis[i];
				nNumValid++;
			}
		}

		if (nNumValid > 0)
		{
			return dwValidIdleAnis[GetRandom(0, nNumValid-1)];
		}
	}


	return INVALID_ANI;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetSubtleIdleAni()
//
//	PURPOSE:	Get a sutble idle animation
//
// ----------------------------------------------------------------------- //

uint32 CWeaponModel::GetSubtleIdleAni()
{
	return m_bUsingAltFireAnis ? m_nAltIdleAnis[0] : m_nIdleAnis[0];
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetSelectAni()
//
//	PURPOSE:	Get a select animation
//
// ----------------------------------------------------------------------- //

uint32 CWeaponModel::GetSelectAni()
{
	return m_bUsingAltFireAnis ? m_nAltSelectAni : m_nSelectAni;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetReloadAni()
//
//	PURPOSE:	Get a reload animation
//
// ----------------------------------------------------------------------- //

uint32 CWeaponModel::GetReloadAni()
{
	return m_bUsingAltFireAnis ? m_nAltReloadAni : m_nReloadAni;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::GetDeselectAni()
//
//	PURPOSE:	Get a deselect animation
//
// ----------------------------------------------------------------------- //

uint32 CWeaponModel::GetDeselectAni()
{
    uint32 dwAni = INVALID_ANI;


	if (m_bUsingAltFireAnis)
	{
		// If we're actually changing weapons make sure we use the
		// currect AltDeselect animation...

		if (m_nRequestedWeaponId != WMGR_INVALID_ID &&
			m_nRequestedWeaponId != m_nWeaponId)
		{
			dwAni = m_nAltDeselect2Ani;
		}
		else
		{
			dwAni = m_nAltDeselectAni;
		}
	}
	else
	{
		dwAni = m_nDeselectAni;
	}

	return dwAni;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::IsFireAni()
//
//	PURPOSE:	Is the passed in animation a fire animation
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::IsFireAni(uint32 dwAni)
{
    if (dwAni == INVALID_ANI) return LTFALSE;

    int i;
    for (i=0; i < WM_MAX_FIRE_ANIS; i++)
	{
		if (m_nFireAnis[i] == dwAni)
		{
            return LTTRUE;
		}
	}

	for (i=0; i < WM_MAX_ALTFIRE_ANIS; i++)
	{
		if (m_nAltFireAnis[i] == dwAni)
		{
            return LTTRUE;
		}
	}

    return LTFALSE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::IsIdleAni()
//
//	PURPOSE:	Is the passed in animation an idle animation (NOTE this
//              will return LTFALSE if the passed in animation is a subtle
//				idle animation).
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::IsIdleAni(uint32 dwAni)
{
    if (dwAni == INVALID_ANI) return LTFALSE;

    int i;
    for (i=1; i < WM_MAX_IDLE_ANIS; i++)
	{
		if (m_nIdleAnis[i] == dwAni)
		{
            return LTTRUE;
		}
	}

	for (i=1; i < WM_MAX_ALTIDLE_ANIS; i++)
	{
		if (m_nAltIdleAnis[i] == dwAni)
		{
            return LTTRUE;
		}
	}

    return LTFALSE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::IsDeselectAni()
//
//	PURPOSE:	Is this a valid deselect ani
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::IsDeselectAni(uint32 dwAni)
{
    if (dwAni == INVALID_ANI) return LTFALSE;

	if (dwAni == m_nDeselectAni ||
		dwAni == m_nAltDeselectAni ||
		dwAni == m_nAltDeselect2Ani)
	{
        return LTTRUE;
	}

    return LTFALSE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::IsSelectAni()
//
//	PURPOSE:	Is this a valid Select ani
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::IsSelectAni(uint32 dwAni)
{
    if (dwAni == INVALID_ANI) return LTFALSE;

	if (dwAni == m_nSelectAni || dwAni == m_nAltSelectAni)
	{
        return LTTRUE;
	}

    return LTFALSE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::IsReloadAni()
//
//	PURPOSE:	Is this a valid Reload ani
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::IsReloadAni(uint32 dwAni)
{
    if (dwAni == INVALID_ANI) return LTFALSE;

	if (dwAni == m_nReloadAni || dwAni == m_nAltReloadAni)
	{
        return LTTRUE;
	}

    return LTFALSE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::CanUseAltFireAnis()
//
//	PURPOSE:	Can we use alt-fire anis?
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::CanUseAltFireAnis()
{
	return (m_nAltSelectAni != INVALID_ANI);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::HandleFireKeyDown()
//
//	PURPOSE:	Handle fire key down
//
// ----------------------------------------------------------------------- //

void CWeaponModel::HandleFireKeyDown()
{
	// Only handle alt-fire case on weapons that have special
	// Alt-fire animations...

	if (m_eLastFireType != FT_ALT_FIRE || !CanUseAltFireAnis()) return;

	// If we aren't playing the select, deselect, or fire ani, it is
	// okay to toggle using Alt-Fire Anis on/off...

    uint32 dwAni = g_pLTClient->GetModelAnimation(m_hObject);

	if (IsSelectAni(dwAni) || IsDeselectAni(dwAni) || IsFireAni(dwAni))
	{
		return;
	}


	// Toggle use of Alt-Fire Anis on/off...

	// Alright we need to either select or deselect the alt-fire
	// aspect of the weapon.  This is a bit tricky since the
	// select/deselect code depends on the current value of
	// m_bUsingAltFireAnis, and we want to change that value here.
	//
	// So, for the select case (i.e., m_bUsingAltFireAni == TRUE AFTER
	// it is toggled), we'll go ahead and toggle it first...).
	//
	// However, for the deselect case (i.e., m_bUsingAltFireAni
	// == TRUE BEFORE it is toggled), we'll toggle it after we
	// call Deslect...


	// See if we need to call Select...

	if (!m_bUsingAltFireAnis)
	{
		// Toggle so Select knows the right ani to play...

		m_bUsingAltFireAnis = !m_bUsingAltFireAnis;

		Select();
	}
	else
	{
		// Call deselect, then toggle...

		Deselect();

		m_bUsingAltFireAnis = !m_bUsingAltFireAnis;
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::HandleFireKeyUp()
//
//	PURPOSE:	Handle fire key up
//
// ----------------------------------------------------------------------- //

void CWeaponModel::HandleFireKeyUp()
{
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::SetState()
//
//	PURPOSE:	Set our m_eState data member
//
// ----------------------------------------------------------------------- //

WeaponState CWeaponModel::SetState(WeaponState eNewState)
{
	WeaponState eOldState = m_eState;

	m_eState = eNewState;

	if (GetState() == W_IDLE)
	{
		// Earliest we can play a non-subtle idle ani...

		m_fNextIdleTime	= GetNextIdleTime();
	}

	return eOldState;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::UpdateMovementPerturb()
//
//	PURPOSE:	Update our weapon perturb value
//
// ----------------------------------------------------------------------- //

void CWeaponModel::UpdateMovementPerturb()
{
	// Make sure the weapon has perturb...

	if (m_pWeapon && m_pWeapon->nMaxPerturb == 0)
	{
		m_fMovementPerturb = 0.0f;
		return;
	}

    LTFLOAT fDelta = g_pGameClientShell->GetFrameTime();
    LTFLOAT fMove = g_pGameClientShell->GetMoveMgr()->GetMovementPercent();
	if (fMove > 1.0f)
	{
		fMove = 1.0f;
	}

	//if walking
	if (!(g_pGameClientShell->GetMoveMgr()->GetControlFlags() & BC_CFLG_RUN) || g_pGameClientShell->IsZoomed())
	{
		fMove *= g_vtPerturbWalkPercent.GetFloat();
	}

	if (g_pGameClientShell->GetDamageFXMgr()->IsPoisoned() || g_pGameClientShell->GetDamageFXMgr()->IsStunned())
	{
		fMove = 1.0f;
	}


    LTVector vPlayerRot;
	g_pGameClientShell->GetPlayerPitchYawRoll(vPlayerRot);
    LTFLOAT fPitchDiff = (LTFLOAT)fabs(vPlayerRot.x - m_fLastPitch);
    LTFLOAT fYawDiff = (LTFLOAT)fabs(vPlayerRot.y - m_fLastYaw);
	m_fLastPitch = vPlayerRot.x;
	m_fLastYaw = vPlayerRot.y;
    LTFLOAT fRot = g_vtPerturbRotationEffect.GetFloat() * (fPitchDiff + fYawDiff) / (2.0f + g_vtFastTurnRate.GetFloat() * fDelta);
	if (fRot > 1.0f)
		fRot = 1.0f;

    LTFLOAT fAdj = Max(fRot,fMove);
    LTFLOAT fDiff = (LTFLOAT)fabs(fAdj - m_fMovementPerturb);
	if (fAdj >  m_fMovementPerturb)
	{
		fDelta *= g_vtPerturbIncreaseSpeed.GetFloat();
		m_fMovementPerturb += Min(fDelta,fDiff);
	}
	else if (fAdj <  m_fMovementPerturb)
	{
		fDelta *= g_vtPerturbDecreaseSpeed.GetFloat();
		m_fMovementPerturb -= Min(fDelta,fDiff);
	}
}



// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::ToggleHolster()
//
//	PURPOSE:	Holster or unholster our weapon
//
// ----------------------------------------------------------------------- //

void CWeaponModel::ToggleHolster(LTBOOL bPlayDeselect)
{
	// if weapon isn't hand
	if (!IsMeleeWeapon())
	{
		m_nHolsterWeaponId = m_nWeaponId;
		ChangeWeapon(g_pWeaponMgr->GetCommandId(MeleeWeapon()), bPlayDeselect);
	}
	else
	{
		ChangeWeapon(g_pWeaponMgr->GetCommandId(m_nHolsterWeaponId), bPlayDeselect);
		m_nHolsterWeaponId = m_nWeaponId;
	}
}



// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::SetHolster()
//
//	PURPOSE:	Set what weapon is in our holster
//
// ----------------------------------------------------------------------- //

void CWeaponModel::SetHolster(uint8 nWeaponId)
{
	if (!g_pWeaponMgr->IsValidWeapon(nWeaponId)) return;
	m_nHolsterWeaponId = nWeaponId;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::MeleeWeapon()
//
//	PURPOSE:	Determine what the current melee weapon is
//
// ----------------------------------------------------------------------- //

uint8 CWeaponModel::MeleeWeapon()
{
	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
	if (!pStats) return -1;

	int nWeapon = g_pWeaponMgr->GetFirstWeaponCommandId();
	int nMaxWeapon = g_pWeaponMgr->GetLastWeaponCommandId();
	int nMelee = -1;

	while (nWeapon <= nMaxWeapon && nMelee < 0)
	{
		int nWeaponIndex = g_pWeaponMgr->GetWeaponId(nWeapon);
		if (pStats->HaveWeapon(nWeaponIndex))
		{
			WEAPON *pWeapon = g_pWeaponMgr->GetWeapon(nWeaponIndex);
			int nAmmoId = pWeapon->nDefaultAmmoType;
			AMMO *pAmmo = g_pWeaponMgr->GetAmmo(nAmmoId);
			if (pAmmo && pAmmo->eInstDamageType == DT_MELEE)
				nMelee = nWeapon;
		}
		nWeapon++;
	}

    return (uint8)g_pWeaponMgr->GetWeaponId(nMelee);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::HandleZipCordFire()
//
//	PURPOSE:	Handle the zip-cord firing...
//
// ----------------------------------------------------------------------- //

LTBOOL CWeaponModel::HandleZipCordFire()
{
	CPlayerStats* pStats = g_pGameClientShell->GetPlayerStats();
	if (!pStats) return LTFALSE;

	// If we're currently zip-cording, turn it off..

	if (g_pGameClientShell->GetMoveMgr()->IsZipCordOn())
	{
		if (!m_bFireKeyDownLastUpdate)
		{
			g_pGameClientShell->GetMoveMgr()->TurnOffZipCord();
		}

		return LTFALSE;
	}
	else if (!pStats->DrawingActivateGadget())
	{
		// We're not targeting a zip hook so this fire message
		// really didn't count...

		if (!m_bFireKeyDownLastUpdate)
		{
			// Play "sorry try again" sound...

			char* pSound = "Guns\\snd\\zipcord\\notarget.wav";
			g_pClientSoundMgr->PlaySoundLocal(pSound,
				SOUNDPRIORITY_PLAYER_HIGH, PLAYSOUND_CLIENT);
		}

		return LTFALSE;
	}

	return LTTRUE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CWeaponModel::SetupModel
//
//	PURPOSE:	Make sure the player-view model is using the correct
//				textures (based on the player's style).
//
// ----------------------------------------------------------------------- //

void CWeaponModel::SetupModel()
{
	if (!m_hObject || !m_pWeapon) return;

	ObjectCreateStruct createStruct;
	INIT_OBJECTCREATESTRUCT(createStruct);

	SAFE_STRCPY(createStruct.m_Filename, m_pWeapon->szPVModel);
	SAFE_STRCPY(createStruct.m_SkinNames[0], m_pWeapon->szPVSkin);

    // Figure out what hand skin to use...

	ModelStyle eModelStyle = eModelStyleDefault;
	CCharacterFX* pCharFX = g_pGameClientShell->GetMoveMgr()->GetCharacterFX();
	if (pCharFX)
	{
		eModelStyle = pCharFX->GetModelStyle();
	}

	if (g_pGameClientShell->GetGameType() == SINGLE || g_pGameClientShell->GetMoveMgr()->IsPlayerModel())
	{
		SAFE_STRCPY(createStruct.m_SkinNames[1], g_pModelButeMgr->GetHandsSkinFilename(eModelStyle));
	}
	else
	{
		SAFE_STRCPY(createStruct.m_SkinNames[1], "guns\\skins_pv\\MultiHands_pv.dtx");
	}

	// Check for special case of hands for the main weapon skin...

	if (strcmp(m_pWeapon->szPVSkin, "Hands") == 0)
	{
		if ( g_pGameClientShell->GetGameType() == SINGLE )
		{
			SAFE_STRCPY(createStruct.m_SkinNames[0], g_pModelButeMgr->GetHandsSkinFilename(eModelStyle));
		}
		else
		{
			SAFE_STRCPY(createStruct.m_SkinNames[0], "guns\\skins_pv\\MultiHands_pv.dtx");
		}

		// Okay, here is a nice 11th hour hack for you...We want to make sure the
		// player is using the space hands model for the space station mission...so,
		// check to see if we're on the space station...

		if (g_pGameClientShell->GetCurrentMission() == 19)
		{
			SAFE_STRCPY(createStruct.m_Filename, "guns\\models_pv\\SpaceChop_pv.abc");
		}
	}


	// Set the filenames...

    g_pLTClient->Common()->SetObjectFilenames(m_hObject, &createStruct);
}
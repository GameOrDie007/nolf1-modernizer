 // ----------------------------------------------------------------------- //
//
// MODULE  : FlashLight.cpp
//
// PURPOSE : FlashLight class - Implementation
//
// CREATED : 07/21/99
//
// (c) 1999 Monolith Productions, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "FlashLight.h"
#include "GameClientShell.h"
#include "ClientUtilities.h"
#include "VarTrack.h"
#include "BaseScaleFX.h"
#include "MsgIDs.h"
#include "VehicleMgr.h"
#include "VRShared.h"
#include "VRLog.h"

extern CGameClientShell* g_pGameClientShell;

VarTrack	g_cvarFLMinLightRadius;
VarTrack	g_cvarFLMaxLightRadius;
VarTrack	g_cvarFLMaxLightDist;
VarTrack	g_cvarFLBeamMinRadius;
VarTrack	g_cvarFLBeamRadius;
VarTrack	g_cvarFLBeamAlpha;
VarTrack	g_cvarFLBeamUOffset;
VarTrack	g_cvarFLBeamROffset;
VarTrack	g_cvarFLPolyBeam;
VarTrack	g_cvarFLNumSegments;
VarTrack	g_cvarFLMinBeamLen;
VarTrack	g_cvarFLServerUpdateTime;
// VRFlashlightHand: put the torch on the LEFT controller. 0 restores the old
// camera-mounted beam. See the note in CFlashLightPlayer::GetLightPositions.
VarTrack	g_cvarVRFlashlightHand;
// The drawn torch. See CFlashLight::UpdateModel.
VarTrack	g_cvarVRTorchModel;
VarTrack	g_cvarVRTorchScale;
VarTrack	g_cvarVRTorchTrimPitch;
VarTrack	g_cvarVRTorchTrimYaw;
VarTrack	g_cvarVRTorchTrimRoll;
VarTrack	g_cvarVRTorchOffF;
VarTrack	g_cvarVRTorchOffR;
VarTrack	g_cvarVRTorchOffU;
VarTrack	g_cvarVRTorchDrop;
// The two scales the hand's offset goes through, owned by GameClientShell.
extern VarTrack g_vtVRHandPosScale;
extern VarTrack g_vtVRViewModelScale;

// WHERE THE LAMP IS INSIDE THE MODEL, measured out of the file rather than
// guessed at: tools/abc.py on GUNS/MODELS_HH/FLASHLIGHT_HH.ABC reports one
// socket, 'Flash', at (0.00, +1.59, +8.84), and a body 11.81 units long lying
// along local +Z with the wide end - the lamp head - at +Z. So the model's
// forward already IS the barrel and it needs no trim to point down the beam.
//
// These numbers exist so the MODEL can be moved to fit the BEAM, which is the
// way round that matters: the beam's origin is headset-confirmed and the light
// is not being touched. Hanging the torch back by its own lamp offset puts the
// 'Flash' socket exactly on the point the beam leaves from.
static const float kTorchFlashF = 8.84f;
static const float kTorchFlashU = 1.59f;

static LTBOOL NonSolidFilterFn(HOBJECT hTest, void *pUserData)
{
	if (ObjListFilterFn(hTest, pUserData))
	{
		// Ignore non-solid objects (even if ray-hit is true)...

		uint32 dwFlags = g_pLTClient->GetObjectFlags(hTest);

		if (!(dwFlags & FLAG_SOLID))
		{
			return LTFALSE;
		}
	}
    return LTTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLight::CFlashLight()
//
//	PURPOSE:	Constructor
//
// ----------------------------------------------------------------------- //

CFlashLight::CFlashLight()
{
    m_bOn               = LTFALSE;

    m_hLight            = LTNULL;
    m_hModel            = LTNULL;

	m_fMinLightRadius	= 75.0f;
	m_fMaxLightRadius	= 500.0f;
	m_fMaxLightDist		= 10000.0f;

    m_fServerUpdateTimer = (LTFLOAT)INT_MAX;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLight::~CFlashLight()
//
//	PURPOSE:	Destructor
//
// ----------------------------------------------------------------------- //

CFlashLight::~CFlashLight()
{
	if (m_hLight)
	{
        g_pLTClient->DeleteObject(m_hLight);
        m_hLight = LTNULL;
	}

	if (m_hModel)
	{
        g_pLTClient->DeleteObject(m_hModel);
        m_hModel = LTNULL;
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLight::TurnOn()
//
//	PURPOSE:	Turn light on
//
// ----------------------------------------------------------------------- //

void CFlashLight::TurnOn()
{
	CreateLight();

	if (m_hLight)
	{
        m_bOn = LTTRUE;
        uint32 dwFlags = g_pLTClient->GetObjectFlags(m_hLight);
        g_pLTClient->SetObjectFlags(m_hLight, dwFlags | FLAG_VISIBLE);

		dwFlags = m_LightBeam.GetFlags();
		m_LightBeam.SetFlags(dwFlags | FLAG_VISIBLE);

		if ( UpdateServer() )
		{
			HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_PLAYER_CLIENTMSG);
			g_pLTClient->WriteToMessageByte(hMessage, CP_FLASHLIGHT);
			g_pLTClient->WriteToMessageByte(hMessage, FL_ON);
			g_pLTClient->EndMessage(hMessage);
		}
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLight::TurnOff()
//
//	PURPOSE:	Turn light off
//
// ----------------------------------------------------------------------- //

void CFlashLight::TurnOff()
{
	if (m_hLight)
	{
        m_bOn = LTFALSE;
        uint32 dwFlags = g_pLTClient->GetObjectFlags(m_hLight);
        g_pLTClient->SetObjectFlags(m_hLight, dwFlags & ~FLAG_VISIBLE);

		dwFlags = m_LightBeam.GetFlags();
		m_LightBeam.SetFlags(dwFlags & ~FLAG_VISIBLE);

		HideModel();

		if ( UpdateServer() )
		{
			HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_PLAYER_CLIENTMSG);
			g_pLTClient->WriteToMessageByte(hMessage, CP_FLASHLIGHT);
			g_pLTClient->WriteToMessageByte(hMessage, FL_OFF);
			g_pLTClient->EndMessage(hMessage);
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLight::CreateLight()
//
//	PURPOSE:	Create the dynamic light
//
// ----------------------------------------------------------------------- //

void CFlashLight::CreateLight()
{
	if (m_hLight) return;

    g_cvarFLMinLightRadius.Init(g_pLTClient, "FLMinRadius", NULL, m_fMinLightRadius);
    g_cvarFLMaxLightRadius.Init(g_pLTClient, "FLMaxRadius", NULL, m_fMaxLightRadius);
    g_cvarFLMaxLightDist.Init(g_pLTClient, "FLMaxDist", NULL, m_fMaxLightDist);
    g_cvarFLBeamMinRadius.Init(g_pLTClient, "FLBeamMinRadius", NULL, 10.0f);
    g_cvarFLBeamRadius.Init(g_pLTClient, "FLBeamRadius", NULL, 150.0f);
    g_cvarFLBeamAlpha.Init(g_pLTClient, "FLBeamAlpha", NULL, 0.2f);
    g_cvarFLBeamUOffset.Init(g_pLTClient, "FLBeamUOffset", NULL, -10.0f);
    g_cvarFLBeamROffset.Init(g_pLTClient, "FLBeamROffset", NULL, 0.0f);
    g_cvarFLNumSegments.Init(g_pLTClient, "FLNumSegments", NULL, 1.0f);
    g_cvarFLPolyBeam.Init(g_pLTClient, "FLPolyBeam", NULL, 1.0f);
    g_cvarFLMinBeamLen.Init(g_pLTClient, "FLMinBeamLen", NULL, 0.0f);
    g_cvarFLServerUpdateTime.Init(g_pLTClient, "FLServerUpdateTime", LTNULL, 0.20f);
    g_cvarVRFlashlightHand.Init(g_pLTClient, "VRFlashlightHand", LTNULL, 1.0f);

	// THE TRIMS EXIST SO THIS DOES NOT COST A HEADSET TRIP EACH TIME.
	// Every one defaults to the measured-from-the-file placement, so with all
	// of them at zero the torch sits where the model says it should. If it
	// reads wrong in the headset they can be turned at the console and the
	// right numbers baked in afterwards, rather than guess-rebuild-relaunch.
	// VRTorchModel 0 turns the drawn torch off and leaves the beam alone.
    g_cvarVRTorchModel.Init(g_pLTClient, "VRTorchModel", LTNULL, 1.0f);
	// 0.80. Started at 1.0, went to 0.65 when the headset found it too big, and
	// 0.65 then read as slightly too small - so the answer is between them and this
	// is the midpoint. A taste number either way; it is a cvar so disagreeing
	// with it costs the player one console line, not a rebuild.
	//
	// Why it was ever 1.0: the asset is
	// authored for an AI's fist at world scale - 11.81 units long and 6.65
	// across, which at this project's measured 16.92 mm per unit is a 200 mm
	// torch 113 mm THICK. That is a lantern, and it is 40 cm from the player's eye.
	// 0.65 makes it 130 mm by 73 mm, which is a torch. Still the player's call: this is
	// a taste number, and it is a cvar so it costs the player nothing to disagree.
    g_cvarVRTorchScale.Init(g_pLTClient, "VRTorchScale", LTNULL, 0.80f);
	// Units to lower the drawn torch from the hand position this code computes,
	// along the PLAYER's up. See the note where it is applied.
    g_cvarVRTorchDrop.Init(g_pLTClient, "VRTorchDrop", LTNULL, 4.5f);
    g_cvarVRTorchTrimPitch.Init(g_pLTClient, "VRTorchTrimPitch", LTNULL, 0.0f);
    g_cvarVRTorchTrimYaw.Init(g_pLTClient, "VRTorchTrimYaw", LTNULL, 0.0f);
    g_cvarVRTorchTrimRoll.Init(g_pLTClient, "VRTorchTrimRoll", LTNULL, 0.0f);
    g_cvarVRTorchOffF.Init(g_pLTClient, "VRTorchOffF", LTNULL, 0.0f);
    g_cvarVRTorchOffR.Init(g_pLTClient, "VRTorchOffR", LTNULL, 0.0f);
    g_cvarVRTorchOffU.Init(g_pLTClient, "VRTorchOffU", LTNULL, 0.0f);


	HOBJECT hCamera = g_pGameClientShell->GetCamera();
	if (!hCamera) return;

	ObjectCreateStruct createStruct;
	INIT_OBJECTCREATESTRUCT(createStruct);

	createStruct.m_ObjectType = OT_LIGHT;
	createStruct.m_Flags = FLAG_VISIBLE;
    g_pLTClient->GetObjectPos(hCamera, &(createStruct.m_Pos));

    m_hLight = g_pLTClient->CreateObject(&createStruct);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLight::CreateModel()
//
//	PURPOSE:	Create the torch you can actually see
//
// ----------------------------------------------------------------------- //

void CFlashLight::CreateModel()
{
	if (m_hModel) return;

	ObjectCreateStruct createStruct;
	INIT_OBJECTCREATESTRUCT(createStruct);

	createStruct.m_ObjectType = OT_MODEL;

	// NOT FLAG_REALLYCLOSE, AND THAT IS THE WHOLE DESIGN DECISION.
	//
	// The view weapon is REALLYCLOSE, which means LithTech stores its position
	// in CAMERA space and the VR path has to rebase and rescale every point
	// that touches it. That machinery is the source of most of this project's
	// worst bugs - the muzzle flash, the tracer origin and the shell casings
	// have each been lost inside it.
	//
	// This model does not need any of it. GetLightPositions already produces a
	// WORLD position for the hand, and headset testing has confirmed that
	// the beam follows the player's left controller from it. So the torch is an
	// ordinary world object placed at an already-verified world point, and
	// there is no rebase, no fK, and nothing for the camera-space rules to get
	// wrong. It also means the torch occludes against the world correctly,
	// which a REALLYCLOSE object does not.
	//
	// The cost is that it can clip into a wall the player pushes a hand through.
	// That is the right trade in VR, where a gun that passes through geometry
	// reads as more wrong than one that does not.
	createStruct.m_Flags = FLAG_VISIBLE | FLAG_SHADOW;

	SAFE_STRCPY(createStruct.m_Filename, "Guns\\Models_HH\\Flashlight_hh.abc");
	SAFE_STRCPY(createStruct.m_SkinName, "Guns\\Skins_HH\\Flashlight_hh.dtx");

	m_hModel = g_pLTClient->CreateObject(&createStruct);

	if (!m_hModel)
	{
		// Say so once. A silent failure here looks exactly like the feature
		// not being built, which is a bad half hour for whoever tests it.
		static LTBOOL s_bSaid = LTFALSE;
		if (!s_bSaid)
		{
			s_bSaid = LTTRUE;
			VRLog::Msg("VRTorch: could not create the torch model"
					   " 'Guns\\Models_HH\\Flashlight_hh.abc' - beam only");
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLight::HideModel()
//
//	PURPOSE:	Stop drawing the torch without destroying it
//
// ----------------------------------------------------------------------- //

void CFlashLight::HideModel()
{
	if (!m_hModel) return;
	uint32 dwFlags = g_pLTClient->GetObjectFlags(m_hModel);
	g_pLTClient->SetObjectFlags(m_hModel, dwFlags & ~FLAG_VISIBLE);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLight::UpdateModel()
//
//	PURPOSE:	Put the torch in the hand the beam comes out of
//
// ----------------------------------------------------------------------- //

void CFlashLight::UpdateModel()
{
	LTVector vPos;
	LTRotation rRot;

	if (!m_bOn || g_cvarVRTorchModel.GetFloat() <= 0.0f
		|| !GetModelTransform(vPos, rRot))
	{
		HideModel();
		return;
	}

	CreateModel();
	if (!m_hModel) return;

	LTVector vU, vR, vF;
	g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);

	// Where the hand itself is, before any trim, so the log can say whether
	// the torch is actually being held or is floating off it.
	const LTVector vHand = vPos;

	// THE GRIP GOES IN THE HAND. The first version hung the model BACKWARDS by
	// its own lamp offset, so the 'Flash' socket landed exactly on the beam's
	// origin - which is tidy, and wrong, because the beam's origin IS the player's hand.
	// It put the lamp in the player's palm and the whole 200 mm body out behind the player's
	// wrist. the torch was not connected 1:1
	// with the hand. It was not being held at all; it was hanging off the back of it.
	//
	// The model's own origin sits in the grip (the mesh runs -3.14 to +8.67
	// along local Z), so putting the origin AT the hand is what a hand holding
	// a torch looks like.
	//
	// The lamp then ends up kTorchFlashF ahead of where the beam starts. That
	// costs nothing: the offset is along the beam's OWN forward axis, so the
	// beam's line through space is identical - it simply starts 15 cm further
	// down a line it was already travelling. The light is still not touched.
	vPos += vF * g_cvarVRTorchOffF.GetFloat();
	// The Leftorium puts the torch in the RIGHT hand: its sideways offset mirrors.
	vPos += vR * (g_cvarVRTorchOffR.GetFloat() * (VRShared::SwapHands() ? -1.0f : 1.0f));
	vPos += vU * g_cvarVRTorchOffU.GetFloat();

	g_pLTClient->SetObjectPos(m_hModel, &vPos);
	g_pLTClient->SetObjectRotation(m_hModel, &rRot);

	const LTFLOAT fScale = g_cvarVRTorchScale.GetFloat();
	if (fScale > 0.01f)
	{
		LTVector vScale(fScale, fScale, fScale);
		g_pLTClient->SetObjectScale(m_hModel, &vScale);
	}

	uint32 dwFlags = g_pLTClient->GetObjectFlags(m_hModel);
	g_pLTClient->SetObjectFlags(m_hModel, dwFlags | FLAG_VISIBLE);

	static int s_nSaidModel = 0;
	if (s_nSaidModel < 4)
	{
		++s_nSaidModel;
		// NOT "off the hand" ANY MORE. That wording was accurate for one build
		// and became a lie the moment VRTorchDrop moved the point upstream, in
		// GetLightPositions - vHand here is already the LOWERED point, so the
		// difference was always going to print 0.0 and say nothing. Mislabelled
		// diagnostics are what sent the tracer hunt into the wrong file twice
		// today, so this one says what it actually measures.
		const LTVector vOffHand = vPos - vHand;
		VRLog::Msg("VRTorchModel: grip at %.0f %.0f %.0f (dropped %.1f from the"
				   " computed hand, trims moved it %.1f); lamp %.0f %.0f %.0f;"
				   " forward %+.2f %+.2f %+.2f; scale %.2f, so %.0f mm long",
				   vPos.x, vPos.y, vPos.z,
				   g_cvarVRTorchDrop.GetFloat(), vOffHand.Mag(),
				   vPos.x + vF.x * kTorchFlashF + vU.x * kTorchFlashU,
				   vPos.y + vF.y * kTorchFlashF + vU.y * kTorchFlashU,
				   vPos.z + vF.z * kTorchFlashF + vU.z * kTorchFlashU,
				   vF.x, vF.y, vF.z,
				   fScale, 11.81f * fScale * 16.92f);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLight::Update()
//
//	PURPOSE:	Update the flash light
//
// ----------------------------------------------------------------------- //

void CFlashLight::Update()
{
	if (!m_bOn || !m_hLight) return;

	// Calculate light position...

    HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();
	if (!hPlayerObj) return;

    HOBJECT hFilterList[] = {hPlayerObj, g_pGameClientShell->GetMoveMgr()->GetObject(), LTNULL};

	IntersectQuery qInfo;
	IntersectInfo iInfo;

    LTRotation rRot;
	LTVector vPos, vEndPos, vUOffset, vROffset;

	GetLightPositions(vPos, vEndPos, vUOffset, vROffset);

	// Straight after GetLightPositions, because that is what fills in the hand
	// pose the torch is hung from - and before the intersect below shortens
	// vEndPos, which has nothing to do with where the torch is held.
	UpdateModel();

	qInfo.m_From = vPos;
	qInfo.m_To   = vEndPos;

	qInfo.m_Flags = INTERSECT_OBJECTS | IGNORE_NONSOLID;
	qInfo.m_FilterFn = NonSolidFilterFn;
	qInfo.m_pUserData = hFilterList;

    if (g_pLTClient->IntersectSegment(&qInfo, &iInfo))
	{
		vEndPos = iInfo.m_Point;
	}

    m_fServerUpdateTimer += g_pGameClientShell->GetFrameTime();
	if (m_fServerUpdateTimer > g_cvarFLServerUpdateTime.GetFloat())
	{
		m_fServerUpdateTimer = 0.0f;

		if ( UpdateServer() )
		{
			HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_PLAYER_CLIENTMSG);
			g_pLTClient->WriteToMessageByte(hMessage, CP_FLASHLIGHT);
			g_pLTClient->WriteToMessageByte(hMessage, FL_UPDATE);
			g_pLTClient->WriteToMessageCompVector(hMessage, &vEndPos);
			g_pLTClient->EndMessage(hMessage);
		}
	}

    g_pLTClient->SetObjectPos(m_hLight, &vEndPos);

    LTVector vDir = vEndPos - vPos;
    LTFLOAT fDist = vDir.Mag();
	vDir.Norm();

    LTFLOAT fLightRadius = g_cvarFLMinLightRadius.GetFloat() +
		((g_cvarFLMaxLightRadius.GetFloat() - g_cvarFLMinLightRadius.GetFloat()) * fDist / g_cvarFLMaxLightDist.GetFloat());

    g_pLTClient->SetLightRadius(m_hLight, fLightRadius);

    LTVector vColor = LTVector(GetRandom(235.0f, 255.0f), GetRandom(235.0f, 255.0f), GetRandom(200.0f, 235.0f));;
    LTVector vLightColor = vColor / 255.0f;

    g_pLTClient->SetLightColor(m_hLight, vLightColor.x, vLightColor.y, vLightColor.z);


	// Show the light beam...

	if (g_cvarFLPolyBeam.GetFloat() > 0)
	{
		PLFXCREATESTRUCT pls;

		vPos += vUOffset;
		vPos += vROffset;

		pls.pTexture			= "sfx\\test\\fxtest42.dtx";
		pls.dwTexAddr			= LTTEXADDR_CLAMP;
		pls.vStartPos			= vPos;
		pls.vEndPos				= vEndPos;
		pls.vInnerColorStart	= vColor;
		pls.vInnerColorEnd		= vColor;
        pls.vOuterColorStart    = vColor;
        pls.vOuterColorEnd      = vColor;
		pls.fAlphaStart			= g_cvarFLBeamAlpha.GetFloat();
		pls.fAlphaEnd			= g_cvarFLBeamAlpha.GetFloat();
		pls.fMinWidth			= g_cvarFLBeamMinRadius.GetFloat();
		pls.fMaxWidth			= g_cvarFLBeamRadius.GetFloat();
		pls.fMinDistMult		= 1.0f;
		pls.fMaxDistMult		= 1.0f;
		pls.fLifeTime			= 1.0f;
		pls.fAlphaLifeTime		= 1.0f;
		pls.fPerturb			= 0.0f;
        pls.bAdditive           = LTTRUE; // LTFALSE;
        pls.bNoZ                = LTTRUE;
		pls.bAlignFlat			= LTTRUE;
		pls.nWidthStyle			= PLWS_CONSTANT;
		pls.nNumSegments		= (int)g_cvarFLNumSegments.GetFloat();

        LTBOOL bUpdateBeam = LTTRUE;

		if (m_LightBeam.HasBeenDrawn())
		{
			// Keep the light beam in the vis list...

			m_LightBeam.SetPos(vPos);

			// Hide the beam if it is too short...

            uint32 dwFlags = m_LightBeam.GetFlags();

			if (fDist < g_cvarFLMinBeamLen.GetFloat())
			{
				// Fade alpha out as beam gets shorter to help hide
				// the poly line...

				pls.fAlphaStart *= fDist / g_cvarFLMinBeamLen.GetFloat();

				if (pls.fAlphaStart < 0.01f)
				{
					dwFlags &= ~FLAG_VISIBLE;
					bUpdateBeam = LTFALSE;
				}
			}
			else
			{
				dwFlags |= FLAG_VISIBLE;
			}

			m_LightBeam.SetFlags(dwFlags);
			m_LightBeam.ReInit(&pls);
		}
		else
		{
			m_LightBeam.Init(&pls);
            m_LightBeam.CreateObject(g_pLTClient);
		}

		if (bUpdateBeam)
		{
			m_LightBeam.Update();
		}
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLightPlayer::GetLightPositions()
//
//	PURPOSE:	Get the flash light position and rotation...
//
// ----------------------------------------------------------------------- //

CFlashLightPlayer::CFlashLightPlayer()
{
	m_bVRTorch = LTFALSE;
	m_vVRTorchPos.Init();
	m_vVRTorchF.Init();
	m_vVRTorchU.Init();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLightPlayer::GetModelTransform()
//
//	PURPOSE:	Where to draw the torch: the pose the beam was built from
//
// ----------------------------------------------------------------------- //

LTBOOL CFlashLightPlayer::GetModelTransform(LTVector& vPos, LTRotation& rRot)
{
	// Only when the beam is actually on the hand. With VRFlashlightHand 0, or
	// flat, or in a vehicle, the beam comes off the camera - and a torch drawn
	// at the player's own eye is a torch filling the screen.
	if (!m_bVRTorch) return LTFALSE;

	vPos = m_vVRTorchPos;

	LTVector vF = m_vVRTorchF, vU = m_vVRTorchU;
	g_pLTClient->AlignRotation(&rRot, &vF, &vU);

	// The trims are applied to the finished rotation, in the model's own frame,
	// so "pitch" means pitch of the torch and not of anything else.
	const float fD2R = 0.01745329f;
	if (g_cvarVRTorchTrimYaw.GetFloat()   != 0.0f)
		g_pLTClient->EulerRotateY(&rRot, g_cvarVRTorchTrimYaw.GetFloat()   * fD2R);
	if (g_cvarVRTorchTrimPitch.GetFloat() != 0.0f)
		g_pLTClient->EulerRotateX(&rRot, g_cvarVRTorchTrimPitch.GetFloat() * fD2R);
	if (g_cvarVRTorchTrimRoll.GetFloat()  != 0.0f)
		g_pLTClient->EulerRotateZ(&rRot, g_cvarVRTorchTrimRoll.GetFloat()  * fD2R);

	return LTTRUE;
}


void CFlashLightPlayer::GetLightPositions(LTVector& vStartPos, LTVector& vEndPos, LTVector& vUOffset, LTVector& vROffset)
{
	// Cleared every frame and only set by the hand branch below. A stale
	// LTTRUE here would draw the torch at the last place the hand was, which
	// on a level change is somewhere across the map - the same failure the
	// muzzle node's sanity check exists to catch.
	m_bVRTorch = LTFALSE;

	vStartPos.Init();
	vEndPos.Init();
	vUOffset.Init();
	vROffset.Init();

	CMoveMgr* pMoveMgr = g_pGameClientShell->GetMoveMgr();
	if (!pMoveMgr) return;

	LTRotation rRot;

	if (pMoveMgr->GetVehicleMgr()->IsVehiclePhysics())
	{
		if (g_pGameClientShell->IsFirstPerson())
		{
			pMoveMgr->GetVehicleMgr()->GetVehicleLightPosRot(vStartPos, rRot);
		}
		else // 3rd person vehicle
		{
			// Get light pos on 3rd-person vehicle...

			HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();
			if (hPlayerObj)
			{
				g_pLTClient->GetObjectRotation(hPlayerObj, &rRot);
				g_pLTClient->GetObjectPos(hPlayerObj, &vStartPos);
			}
		}
	}
	else if (g_pGameClientShell->IsFirstPerson())
	{
		HOBJECT hCamera = g_pGameClientShell->GetCamera();
		if (!hCamera) return;

		g_pLTClient->GetObjectRotation(hCamera, &rRot);
		g_pLTClient->GetObjectPos(hCamera, &vStartPos);
	}
	else // 3rd person
	{
		// Get light pos from 3rd-person model...

		HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();
		if (hPlayerObj)
		{
			g_pLTClient->GetObjectRotation(hPlayerObj, &rRot);
			g_pLTClient->GetObjectPos(hPlayerObj, &vStartPos);
		}
	}

	LTVector vU, vR, vF;
    g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);

	// ---- THE TORCH GOES ON THE LEFT HAND ----------------------------------
	//
	// the flashlight was a beam from the chest
	// of the character, not following the left controller. It never did: above, the
	// first-person branch takes the beam's position AND rotation straight off
	// the camera, then FLBeamUOffset (-10) drops it ten units below the eye.
	// That is the beam from below the player's face. There is no hand anywhere in this
	// file and there never was, so this is new work rather than a repair -
	// which is worth saying, because it was reported as a regression.
	//
	// Composed exactly the way CWeaponModel::GetFireInfo composes the aim ray,
	// deliberately: that path is settled, headset-confirmed, and its comments
	// record two reconstructions this project already paid for. The only change
	// is Hands[0] instead of Hands[1].
	//
	//   * the DIRECTION is the hand's ABSOLUTE yaw/pitch expressed in the
	//     camera OBJECT's basis. That object carries the body's rotation and
	//     not the head's - measured there: with the headset pitched 25 degrees
	//     down its forward came back +0.000 in Y - so there is no head in the
	//     basis to double-count and none is subtracted.
	//   * the ORIGIN is the hand's offset from the head through the same two
	//     scales the placement uses, VRHandPosScale * VRViewModelScale, which
	//     multiply to the world's 58.75 units per metre. Z is flipped, as it
	//     is there.
	//
	// VRFlashlightHand 0 restores the old camera-mounted beam.
	if (g_pGameClientShell->IsFirstPerson() && VRShared::IsLive()
		&& g_cvarVRFlashlightHand.GetFloat() > 0.0f)
	{
		const VRSharedState& s = VRShared::State();
		const VRHandState&   h = s.Hands[0];			// LEFT hand
		if (h.nActive)
		{
			const float fD2R = 0.01745329f;
			LTRotation rHand;
			rHand.Init();
			g_pLTClient->EulerRotateY(&rHand, -h.fYawDeg   * fD2R);
			g_pLTClient->EulerRotateX(&rHand, -h.fPitchDeg * fD2R);

			LTVector vHU, vHR, vHF;
			g_pLTClient->GetRotationVectors(&rHand, &vHU, &vHR, &vHF);

			const LTVector vCU = vU, vCR = vR, vCF = vF;
			vF = vCR * vHF.x + vCU * vHF.y + vCF * vHF.z;
			vU = vCR * vHU.x + vCU * vHU.y + vCF * vHU.z;
			vR = vCR * vHR.x + vCU * vHR.y + vCF * vHR.z;

			const float U = (g_vtVRHandPosScale.GetFloat() > 0.0f)
				? g_vtVRHandPosScale.GetFloat() : 3.0f;
			const float K = (g_vtVRViewModelScale.GetFloat() > 0.1f)
				? g_vtVRViewModelScale.GetFloat() : 17.0f;
			const float hx =  (h.fPosX - s.fHeadPosX) * U * K;
			const float hy =  (h.fPosY - s.fHeadPosY) * U * K;
			const float hz = -(h.fPosZ - s.fHeadPosZ) * U * K;	// Z flip
			vStartPos += vCR * hx + vCU * hy + vCF * hz;

			static int s_nSaidFL = 0;
			if (s_nSaidFL < 6)
			{
				++s_nSaidFL;
				VRLog::Msg("VRTorch: left hand %+.2f %+.2f %+.2f m from the head"
					" -> beam starts %.0f %.0f %.0f, points %+.2f %+.2f %+.2f"
					" (yaw %+.1f pitch %+.1f)",
					h.fPosX - s.fHeadPosX, h.fPosY - s.fHeadPosY, h.fPosZ - s.fHeadPosZ,
					vStartPos.x, vStartPos.y, vStartPos.z, vF.x, vF.y, vF.z,
					h.fYawDeg, h.fPitchDeg);
			}

			// ---- AND THE POSE THE DRAWN TORCH HANGS FROM ------------------
			//
			// Built here, from the same camera basis and the same hand, so the
			// model cannot drift away from the beam. The one thing it adds is
			// ROLL, which the beam has no use for - a cone is the same shape
			// whichever way up it is - but a torch in the hand is not, and
			// headset testing caught the gun rolling the wrong way on 8 September, so it
			// is worth getting the sense right first time.
			//
			// Roll takes the sign WeaponModel settled on and its reasoning:
			// yaw and pitch reverse under the Z-flip because they are
			// rotations that involve Z, roll is a rotation ABOUT Z and keeps
			// its sense. Head roll comes off it for the same reason it does
			// there - the camera object carries the body, not the head.
			{
				LTRotation rTorch;
				rTorch.Init();
				g_pLTClient->EulerRotateY(&rTorch, -h.fYawDeg   * fD2R);
				g_pLTClient->EulerRotateX(&rTorch, -h.fPitchDeg * fD2R);
				g_pLTClient->EulerRotateZ(&rTorch,
					+(h.fRollDeg - s.fHeadRollDeg) * fD2R);

				LTVector vTU, vTR, vTF;
				g_pLTClient->GetRotationVectors(&rTorch, &vTU, &vTR, &vTF);

				m_vVRTorchF = vCR * vTF.x + vCU * vTF.y + vCF * vTF.z;
				m_vVRTorchU = vCR * vTU.x + vCU * vTU.y + vCF * vTU.z;

				// LOWERED IN THE PLAYER'S OWN UP, NOT THE TORCH'S.
				//
				// the torch sat about 3 inches
				// above where the real hand was - while the log said the
				// grip was 0.0 units off the hand. Both are true: it is exactly
				// on the hand this code COMPUTES, and that is about 3 inches
				// above the player's real one. The drawn torch is the first thing this
				// project has had that makes the hand position visible, so it
				// is the first time the gap could be seen at all.
				//
				// It is not the model: the mesh runs -4.48 to +1.28 in local Y,
				// so its body hangs BELOW its origin. Putting the origin on a
				// point would show the torch low, not high.
				//
				// Applied along the CAMERA's up rather than the torch's,
				// deliberately. This corrects an error in a world position, so
				// it must not turn with the player's wrist - VRTorchOffU does turn with
				// it, because that one is for seating the model in the grip.
				// 4.5 units is the requested three inches at 16.92 mm per unit.
				m_vVRTorchPos = vStartPos - vCU * g_cvarVRTorchDrop.GetFloat();
				m_bVRTorch = LTTRUE;
			}

			// NO BEAM OFFSETS ON THE HAND. FLBeamUOffset's -10 exists to drop
			// the beam clear of the eye it was mounted on; on a torch held in
			// the hand it just tilts the cone off the hand's own line.
			vEndPos = vStartPos + (vF * g_cvarFLMaxLightDist.GetFloat());
			return;
		}
	}

	vEndPos = vStartPos + (vF * g_cvarFLMaxLightDist.GetFloat());

	if (g_pGameClientShell->IsFirstPerson())
	{
		vROffset = (vR * g_cvarFLBeamROffset.GetFloat());
		vUOffset = (vU * g_cvarFLBeamUOffset.GetFloat());
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLightAI::CFlashLightAI()
//
//	PURPOSE:	Constructor
//
// ----------------------------------------------------------------------- //

CFlashLightAI::CFlashLightAI()
{
	m_hAI = LTNULL;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLightAI::~CFlashLightAI()
//
//	PURPOSE:	Destructor
//
// ----------------------------------------------------------------------- //

CFlashLightAI::~CFlashLightAI()
{
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLightAI::Init()
//
//	PURPOSE:	Initializes the flashlight
//
// ----------------------------------------------------------------------- //

void CFlashLightAI::Init(HOBJECT hAI)
{
	m_hAI = hAI;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLightAI::Update()
//
//	PURPOSE:	Update the flash light
//
// ----------------------------------------------------------------------- //

void CFlashLightAI::Update()
{
	if ( !m_hAI ) return;

	uint32 dwUsrFlags = 0;
	if ( LT_OK == g_pLTClient->GetObjectUserFlags(m_hAI, &dwUsrFlags) && (dwUsrFlags & USRFLG_AI_FLASHLIGHT) )
	{
		TurnOn();
	}
	else
	{
		TurnOff();
	}

	CFlashLight::Update();
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CFlashLightAI::GetLightPositions()
//
//	PURPOSE:	Get the flash light position and rotation...
//
// ----------------------------------------------------------------------- //

void CFlashLightAI::GetLightPositions(LTVector& vStartPos, LTVector& vEndPos, LTVector& vUOffset, LTVector& vROffset)
{
	if ( !m_hAI ) return;

	HMODELSOCKET hSocket;
	if ( LT_OK == g_pModelLT->GetSocket(m_hAI, "LeftHand", hSocket) )
	{
		LTransform tf;

		if ( LT_OK == g_pModelLT->GetSocketTransform(m_hAI, hSocket, tf, LTTRUE) )
		{
			LTVector vPos;
			LTRotation rRot;

			if ( LT_OK == g_pTransLT->Get(tf, vPos, rRot) )
			{
				LTVector vRight, vUp, vForward;
				if ( LT_OK == g_pMathLT->GetRotationVectors(rRot, vRight, vUp, vForward) )
				{
					vStartPos = vPos - vUp*4.0f + vForward*8.0f;
					vEndPos = vPos + vForward*200.0f;
					vUOffset = vUp;
					vROffset = vRight;
				}
			}
		}
	}
}
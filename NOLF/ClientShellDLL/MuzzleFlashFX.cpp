// ----------------------------------------------------------------------- //
//
// MODULE  : MuzzleFlashFX.cpp
//
// PURPOSE : MuzzleFlash special FX - Implementation
//
// CREATED : 12/17/99
//
// (c) 1999-2000 Monolith Productions, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "GameClientShell.h"
#include "MuzzleFlashFX.h"
#include "VRPrims.h"
#include "VRLog.h"
#include "iltclient.h"
#include "ClientUtilities.h"
#include "iltmodel.h"
#include "ilttransform.h"
#include "iltcustomdraw.h"
#include "VarTrack.h"
#include "CommonUtilities.h"		// g_pMathLT

VarTrack	g_vtReallyClose;
VarTrack	g_vtVRFlashRotBody;		// VRFlashRotBody <0-1>: the flash model turns with the drawn gun
VarTrack	g_vtVRFlashParticleWorld;	// VRFlashParticleWorld <0-1>: the particle flash is a world object like the model

// THE PARTICLE FLASH GETS THE SAME TREATMENT AS THE FLASH MODEL.
//
// Guns whose player-view flash is a particle burst and a light with no model -
// walther_smg, p38, revolver, contender, luger - never showed a flash in VR.
// the muzzle flash for these guns was not
// showing up. The log said why: the particle object was handed to the publisher
// "visible yes, reallyclose yes at 922 74 -1751" - a WORLD position, put there
// by SetPos, still wearing FLAG_REALLYCLOSE, which Update re-applied every
// frame. The publisher's camera-relative branch then re-based that world
// position as if it were a few units from the map origin and threw the burst
// thousands of units away. The flash MODEL had its flag stripped in SetPos and
// never re-applied; the particle object had it re-applied every frame.
//
// So, in VR: the particle object is a plain world object at the muzzle with
// the drawn gun's rotation, exactly like the model. 0 is the old behaviour.
static bool VRFlashParticleWorld()
{
	if (!g_vtVRFlashParticleWorld.IsInitted())
		g_vtVRFlashParticleWorld.Init(g_pLTClient, "VRFlashParticleWorld", LTNULL, 1.0f);
	return g_vtVRFlashParticleWorld.GetFloat() > 0.0f && VRPrims_RebaseEverKnown();
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMuzzleFlashFX::Init
//
//	PURPOSE:	Init the MuzzleFlash
//
// ----------------------------------------------------------------------- //

LTBOOL CMuzzleFlashFX::Init(HLOCALOBJ hServObj, HMESSAGEREAD hMessage)
{
    if (!CSpecialFX::Init(hServObj, hMessage)) return LTFALSE;
    if (!hMessage) return LTFALSE;

	// Don't support server-side versions of this fx...

    return LTFALSE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMuzzleFlashFX::Init
//
//	PURPOSE:	Init the MuzzleFlash
//
// ----------------------------------------------------------------------- //

LTBOOL CMuzzleFlashFX::Init(SFXCREATESTRUCT* psfxCreateStruct)
{
    if (!CSpecialFX::Init(psfxCreateStruct)) return LTFALSE;

	MUZZLEFLASHCREATESTRUCT* pMF = (MUZZLEFLASHCREATESTRUCT*)psfxCreateStruct;

	m_cs = *((MUZZLEFLASHCREATESTRUCT*)pMF);

    if (!m_cs.pWeapon) return LTFALSE;

	// Set our server object to our hParent so we get notified when
	// the hParent goes away...

	if (!m_hServerObject)
	{
		m_hServerObject = m_cs.hParent;
	}

    return LTTRUE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMuzzleFlashFX::CreateObject
//
//	PURPOSE:	Create fx objects
//
// ----------------------------------------------------------------------- //

LTBOOL CMuzzleFlashFX::CreateObject(ILTClient *pClientDE)
{
    if (!pClientDE || !CSpecialFX::CreateObject(pClientDE)) return LTFALSE;

	if (!g_vtReallyClose.IsInitted())
	{
	    g_vtReallyClose.Init(pClientDE, "MFReallyClose", NULL, 1.0f);
	}

	return ResetFX();
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMuzzleFlashFX::Setup
//
//	PURPOSE:	Create fx objects
//
// ----------------------------------------------------------------------- //

LTBOOL CMuzzleFlashFX::Setup(MUZZLEFLASHCREATESTRUCT & cs)
{
    LTBOOL bRet = LTFALSE;

	if (!m_pClientDE)
	{
		if (Init(&cs))
		{
            bRet = CreateObject(g_pLTClient);
		}
	}
	else
	{
		bRet = Reset(cs);
	}

	return bRet;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMuzzleFlashFX::Reset
//
//	PURPOSE:	Reset the muzzle flash
//
// ----------------------------------------------------------------------- //

LTBOOL CMuzzleFlashFX::Reset(MUZZLEFLASHCREATESTRUCT & cs)
{
    if (!m_pClientDE) return LTFALSE;

	m_cs = cs;

	ReallyHide();
	return ResetFX();
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMuzzleFlashFX::ResetFX
//
//	PURPOSE:	Reset all the fx...
//
// ----------------------------------------------------------------------- //

LTBOOL CMuzzleFlashFX::ResetFX()
{
	if (!m_pClientDE) return LTFALSE;

    m_bUsingParticles = LTFALSE;
    m_bUsingLight     = LTFALSE;
    m_bUsingScale     = LTFALSE;

	// Create/Reset the fx specified by the weapon...

    if (!m_cs.pWeapon) return LTFALSE;

    CMuzzleFX* pMuzzleFX = LTNULL;
	if (m_cs.bPlayerView)
	{
		pMuzzleFX = m_cs.pWeapon->pPVMuzzleFX;
	}
	else
	{
		pMuzzleFX = m_cs.pWeapon->pHHMuzzleFX;
	}

    if (!pMuzzleFX) return LTFALSE;


	// Create/Update the dynamic light...

	if (pMuzzleFX->pDLightFX)
	{
		if (m_cs.bPlayerView || GetConsoleInt("MuzzleLight", 1))
		{
			m_bUsingLight = (LTBOOL) !!(g_pFXButeMgr->CreateDLightFX(
				pMuzzleFX->pDLightFX, m_cs.vPos, &m_Light));
		}
	}


	// Create/Update the scale fx...

	if (pMuzzleFX->pScaleFX)
	{
        LTVector vU, vR, vF;
        m_pClientDE->GetRotationVectors(&(m_cs.rRot), &vU, &vR, &vF);

        m_bUsingScale = (LTBOOL) !!(g_pFXButeMgr->CreateScaleFX(pMuzzleFX->pScaleFX,
			m_cs.vPos, vF, LTNULL, &(m_cs.rRot), &m_Scale));

		// Make camera-relative if player view...

		if (m_bUsingScale && m_cs.bPlayerView)
		{
			if (m_Scale.GetObject())
			{
				uint32 dwFlags = m_pClientDE->GetObjectFlags(m_Scale.GetObject());
				m_pClientDE->SetObjectFlags(m_Scale.GetObject(), dwFlags | FLAG_REALLYCLOSE);
			}
		}
	}

	// Create/Update the particle muzzle fx...

	if (pMuzzleFX->pPMuzzleFX)
	{
		MFPCREATESTRUCT mfpcs;
		mfpcs.pPMuzzleFX		= pMuzzleFX->pPMuzzleFX;
		mfpcs.bPlayerView		= m_cs.bPlayerView;
		mfpcs.vPos				= m_cs.vPos;
		mfpcs.rRot				= m_cs.rRot;
        mfpcs.hFiredFrom        = m_cs.bPlayerView ? LTNULL : m_cs.hParent;

		if (!m_Particle.GetObject())
		{
			if (m_Particle.Init(&mfpcs))
			{
                m_Particle.CreateObject(m_pClientDE);

				// Make camera-relative if player view...

				if (m_cs.bPlayerView)
				{
					if (m_Particle.GetObject())
					{
						uint32 dwFlags = m_pClientDE->GetObjectFlags(m_Particle.GetObject());

						if (g_vtReallyClose.GetFloat())
						{
							m_pClientDE->SetObjectFlags(m_Particle.GetObject(), dwFlags | FLAG_REALLYCLOSE);
						}
						else
						{
							m_pClientDE->SetObjectFlags(m_Particle.GetObject(), dwFlags & ~FLAG_REALLYCLOSE);
						}
					}
				}
			}
		}
		else
		{
			m_Particle.Reset(mfpcs);
		}

        m_bUsingParticles = LTTRUE;
	}

    return LTTRUE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMuzzleFlashFX::Update
//
//	PURPOSE:	Update the fx
//
// ----------------------------------------------------------------------- //

LTBOOL CMuzzleFlashFX::Update()
{
    if (!m_pClientDE || m_bHidden) return LTFALSE;

	// If we we're not a player-view muzzle fx,  If our
	// server object has been removed, we should go away...

    if (!m_cs.bPlayerView && !m_hServerObject) return LTFALSE;

	// Set/Clear the FLAG_REALLYCLOSE

	if (m_bUsingParticles && m_Particle.GetObject())
	{
		uint32 dwFlags = m_pClientDE->GetObjectFlags(m_Particle.GetObject());

		if (m_cs.bPlayerView)
		{
			// See VRFlashParticleWorld: in VR this object holds a WORLD position.
			if (g_vtReallyClose.GetFloat() && !VRFlashParticleWorld())
			{
				m_pClientDE->SetObjectFlags(m_Particle.GetObject(), dwFlags | FLAG_REALLYCLOSE);
			}
			else
			{
				m_pClientDE->SetObjectFlags(m_Particle.GetObject(), dwFlags & ~FLAG_REALLYCLOSE);
			}
		}
		else
		{
			m_pClientDE->SetObjectFlags(m_Particle.GetObject(), dwFlags & ~FLAG_REALLYCLOSE);
		}
	}

	if (m_bUsingScale && m_Scale.GetObject())
	{
		uint32 dwFlags = m_pClientDE->GetObjectFlags(m_Scale.GetObject());

		if (m_cs.bPlayerView)
		{
			m_pClientDE->SetObjectFlags(m_Scale.GetObject(), dwFlags | FLAG_REALLYCLOSE);
		}
		else
		{
			m_pClientDE->SetObjectFlags(m_Scale.GetObject(), dwFlags & ~FLAG_REALLYCLOSE);
		}
	}


	// Update all the fx, and see if we're done...

    LTBOOL bParticleDone = m_bUsingParticles ? !m_Particle.Update()  : LTTRUE;
    LTBOOL bLightDone    = m_bUsingLight     ? !m_Light.Update()     : LTTRUE;
    LTBOOL bScaleDone    = m_bUsingScale     ? !m_Scale.Update()     : LTTRUE;


	// Keep the objects in the correct place...

	if (!m_cs.bPlayerView && m_cs.hParent)
	{
		HOBJECT hObj;
        LTRotation rRot;
        LTVector vPos;

		GetAttachmentSocketTransform(m_cs.hParent, "Flash", vPos, rRot);

		if (m_bUsingScale)
		{
			hObj = m_Scale.GetObject();
			if (hObj)
			{
                m_pClientDE->SetObjectPos(hObj, &vPos);
                m_pClientDE->SetObjectRotation(hObj, &rRot);
			}
		}

		if (m_bUsingParticles)
		{
			hObj = m_Particle.GetObject();
			if (hObj)
			{
                m_pClientDE->SetObjectPos(hObj, &vPos);
                m_pClientDE->SetObjectRotation(hObj, &rRot);
			}
		}

		if (m_bUsingLight)
		{
			hObj = m_Light.GetObject();
			if (hObj)
			{
                m_pClientDE->SetObjectPos(hObj, &vPos);
			}
		}
	}

	return !(bParticleDone && bLightDone && bScaleDone);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMuzzleFlashFX::Hide
//
//	PURPOSE:	Hide the fx
//
// ----------------------------------------------------------------------- //

static unsigned long s_nVRFlashHidden = 0;
unsigned long VRMuzzleFlashHiddenCount() { return s_nVRFlashHidden; }

void CMuzzleFlashFX::Hide()
{
	++s_nVRFlashHidden;
    m_bHidden = LTTRUE;
	ReallyHide();
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMuzzleFlashFX::ReallyHide
//
//	PURPOSE:	Hide the fx
//
// ----------------------------------------------------------------------- //

void CMuzzleFlashFX::ReallyHide()
{
	if (!m_pClientDE) return;

	HOBJECT hObj;
    uint32 dwFlags;

	// Hide particles...

	hObj = m_Particle.GetObject();
	if (hObj)
	{
        dwFlags = m_pClientDE->GetObjectFlags(hObj);
        m_pClientDE->SetObjectFlags(hObj, dwFlags & ~FLAG_VISIBLE);
	}

	// Hide light...

	hObj = m_Light.GetObject();
	if (hObj)
	{
        dwFlags = m_pClientDE->GetObjectFlags(hObj);
        m_pClientDE->SetObjectFlags(hObj, dwFlags & ~FLAG_VISIBLE);
	}

	// Hide scale fx...

	hObj = m_Scale.GetObject();
	if (hObj)
	{
        dwFlags = m_pClientDE->GetObjectFlags(hObj);
        m_pClientDE->SetObjectFlags(hObj, dwFlags & ~FLAG_VISIBLE);
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMuzzleFlashFX::Show
//
//	PURPOSE:	Show the fx
//
// ----------------------------------------------------------------------- //

// HOW MANY TIMES THE FLASH WAS ACTUALLY SHOWN.
//
// NOLF's first-person flash lives for 7.5 ms (ATTRIBUTES/FX.TXT, Duration on
// PV_P38MuzzFX), which at 90 fps is less than one frame. Sampling the effect's
// state once a second therefore always finds it hidden, whether it is working
// perfectly or not working at all - so the only usable measurement is a count
// of how often it was switched on.
static unsigned long s_nVRFlashShown = 0;
unsigned long VRMuzzleFlashShownCount() { return s_nVRFlashShown; }
// WHEN it was last switched on, in client seconds.
//
// The publish pass cannot use FLAG_VISIBLE for this effect: measured on
// 11 September, the flash was switched on 14 times in a thirty-second run
// and its objects were flagged invisible on all 900 frames the publish
// pass looked at them - so Show and Hide both happen between two
// publishes, and no amount of extending the duration changes that.
// A time is something both sides can agree on.
static double s_fVRFlashLastShow = -1.0;
static HLOCALOBJ s_hVRFlashLastObj = LTNULL;
HLOCALOBJ VRMuzzleFlashLastObject() { return s_hVRFlashLastObj; }
double VRMuzzleFlashLastShowTime() { return s_fVRFlashLastShow; }

void CMuzzleFlashFX::Show()
{
	if (!m_pClientDE) return;

	HOBJECT hObj;
    uint32 dwFlags;

	// PLAYER-VIEW ONLY. The character model carries its own hand-held
	// flash (HH_*MuzzFX in the butes) and it shows on every shot; counting
	// both together said 'the flash fired 14 times' while the first-person
	// one had never fired at all.
	if (m_cs.bPlayerView)
	{
		// ONLY A FLASH THAT HAS SOMETHING TO SHOW OPENS THE FRESH WINDOW.
		// The component objects outlive the weapon they were built for:
		// Reset hides them and ResetFX leaves them be when the next gun's
		// PV flash has no particles, light or scale (the Hampton Carbine's
		// PV_DelisleMuzzFX has all three commented out). Show() still ran on
		// every Hampton shot and stamped the time, and the publish pass
		// treats any handed-over object as visible for 50 ms after that
		// stamp - so the AK47's particle burst, camera-relative and sitting
		// at the camera's own origin, was drawn AT THE EYE on every shot,
		// and continuously under the tuner's hold. Headset, HQ quick save,
		// 21 September: a large strobing flash at the eye. Morocco
		// starts on the Hampton, so nothing stale existed and it never
		// showed there.
		const bool bHasFX = m_bUsingParticles || m_bUsingLight || m_bUsingScale;
		if (bHasFX)
		{
			++s_nVRFlashShown;
			s_fVRFlashLastShow = m_pClientDE->GetTime();
			// WHICH object was switched on. The publish pass is handed
			// m_Particle.GetObject() and reports it invisible on every frame
			// while this counter says the flash fired - so the next question
			// is whether the two are even the same object.
			s_hVRFlashLastObj = m_bUsingParticles ? m_Particle.GetObject() : LTNULL;
		}
		else
		{
			static const WEAPON* s_pSaidNoFX = LTNULL;
			if (m_cs.pWeapon != s_pSaidNoFX)
			{
				s_pSaidNoFX = m_cs.pWeapon;
				VRLog::Msg("VRFlash: '%s' has no player-view flash components -"
						   " nothing is shown on a shot, and the previous gun's"
						   " objects stay hidden (particles %s light %s scale %s"
						   " still exist)",
						   m_cs.pWeapon ? m_cs.pWeapon->szName : "?",
						   m_Particle.GetObject() ? "yes" : "no",
						   m_Light.GetObject() ? "yes" : "no",
						   m_Scale.GetObject() ? "yes" : "no");
			}
		}
	}
    m_bHidden = LTFALSE;

	// Show particles...

	if (m_bUsingParticles)
	{
		hObj = m_Particle.GetObject();
		if (hObj)
		{
            dwFlags = m_pClientDE->GetObjectFlags(hObj);
            m_pClientDE->SetObjectFlags(hObj, dwFlags | FLAG_VISIBLE);
		}
	}

	// Show light...

	if (m_bUsingLight && GetConsoleInt("MuzzleLight", 1))
	{
		hObj = m_Light.GetObject();
		if (hObj)
		{
            dwFlags = m_pClientDE->GetObjectFlags(hObj);
            m_pClientDE->SetObjectFlags(hObj, dwFlags | FLAG_VISIBLE);
		}
	}

	// Show scale fx...

	if (m_bUsingScale)
	{
		hObj = m_Scale.GetObject();
		if (hObj)
		{
            dwFlags = m_pClientDE->GetObjectFlags(hObj);
            m_pClientDE->SetObjectFlags(hObj, dwFlags | FLAG_VISIBLE);
		}
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMuzzleFlashFX::SetPos
//
//	PURPOSE:	Set the fx positions
//
// ----------------------------------------------------------------------- //

void CMuzzleFlashFX::SetPos(LTVector vWorldPos, LTVector vCamRelPos)
{
	if (!m_pClientDE) return;

	HOBJECT hObj;

	//m_pClientDE->CPrint("WorldPos: %.2f, %.2f, %.2f", VEC_EXPAND(vWorldPos));
	//m_pClientDE->CPrint("CamRelPos: %.2f, %.2f, %.2f", VEC_EXPAND(vCamRelPos));

	if (m_bUsingParticles)
	{
		hObj = m_Particle.GetObject();
		if (hObj)
		{
			// THE PARTICLES HAVE THE SAME FAULT AS THE SCALE OBJECT, and they
			// are the smoke and sparks left glowing on the floor by the
			// fountain after the flash itself was put back on the barrel.
			// Camera-relative position, nothing re-basing it, drawn as world.
			// See the note below.
			if (VRPrims_RebaseEverKnown())
			{
				uint32 dwF = m_pClientDE->GetObjectFlags(hObj);
				if (dwF & FLAG_REALLYCLOSE)
					m_pClientDE->SetObjectFlags(hObj, dwF & ~FLAG_REALLYCLOSE);
				m_pClientDE->SetObjectPos(hObj, &vWorldPos, LTTRUE);

				// SET IT, THEN ASK THE ENGINE WHERE IT PUT IT.
				//
				// Every instrument so far has been blind to the fault the tester can see.
				// VRFlashTrace measures the offset in the SAME frame that places it,
				// so it is self-consistent by construction, and its rate column is the
				// HAND's yaw, which barely moves when the body turns. VRFlashPos caps
				// at twelve lines. Both reported the flash welded to the gun while the player's
				// own clip shows it swinging off the barrel through a turn.
				//
				// A READBACK CANNOT BE SELF-CONSISTENT. If the engine stores this
				// anywhere other than where we meant - a camera-relative
				// reinterpretation being the classic one in this port - what comes
				// back is not what went in. Logged beside the DRAWN gun so the gap
				// between them is readable directly, and uncapped by any cvar so it
				// cannot be switched off by accident.
				{
					static int s_nReadback = 0;
					// 1500 lines, not 400: four seconds did not cover a stick turn
					// that starts twelve seconds into a desk run.
					if (s_nReadback < 1500)
					{
						++s_nReadback;
						LTVector vBack; vBack.Init();
						m_pClientDE->GetObjectPos(hObj, &vBack);
						const VRViewRebase& rbL = VRPrims_RebaseLast();
						const LTVector vGun = rbL.vGunWorld;
						LTVector vOff = vBack - vWorldPos;
						LTVector vRel = vBack - vGun;
						// IN THE BODY'S FRAME, which is the one test a stick turn can
						// fail. The distance stayed at 13.5 through a whole turn
						// while the flash swung off the gun, because a world-fixed
						// offset keeps its length. Decomposed on the camera object's
						// basis - the body's, no head - a rigidly attached flash reads
						// the SAME three numbers before, during and after a turn; a
						// world-fixed one rotates through minus the body yaw.
						const float fBodyYawDeg = g_pGameClientShell
							? g_pGameClientShell->GetYaw() * 57.29578f : 0.0f;
						// The flash's ORIENTATION is not read here on purpose. A column
						// that did read it from this spot said identity every frame
						// while SetRot's own readback and the picture both said the
						// composed rotation - something between SetRot and the next
						// SetPos rewrites it, and the renderer never sees that state.
						// The honest reading is at PUBLISH time: VRFlashDrawnRot in
						// VRPublishModels.
						VRLog::Msg("VRFlashBack: set %.0f %.0f %.0f -> engine %.0f %.0f %.0f"
							" (off %.2f) | gun %.0f %.0f %.0f | flash-to-gun %.1f"
							" | body yaw %.1f | in body frame r %+.1f u %+.1f f %+.1f",
							vWorldPos.x, vWorldPos.y, vWorldPos.z,
							vBack.x, vBack.y, vBack.z, vOff.Mag(),
							vGun.x, vGun.y, vGun.z, vRel.Mag(),
							fBodyYawDeg,
							vRel.Dot(rbL.vCamR), vRel.Dot(rbL.vCamU), vRel.Dot(rbL.vCamF));
					}
				}
			}
			else if (g_vtReallyClose.GetFloat())
			{
				m_pClientDE->SetObjectPos(hObj, &vCamRelPos, LTTRUE);
			}
			else
			{
				m_pClientDE->SetObjectPos(hObj, &vWorldPos, LTTRUE);
			}
		}
	}

	if (m_bUsingLight)
	{
		hObj = m_Light.GetObject();
		if (hObj)
		{
            m_pClientDE->SetObjectPos(hObj, &vWorldPos, LTTRUE);
		}
	}

	if (m_bUsingScale)
	{
		hObj = m_Scale.GetObject();
		if (hObj)
		{
			// THIS IS THE FLASH ON THE CARPET BY THE FOUNTAIN.
			//
			// The SCALE object is the muzzle flash you can actually SEE, and it
			// is positioned from vCamRelPos - a camera-relative offset, a dozen
			// units from the eye. Retail draws it as camera-relative and it
			// lands on the barrel. Here nothing re-bases it: it is a MODEL, so
			// the sprite sweep skips it on type, and it is handed over outside
			// the model sweep, so that misses it too. The camera-relative value
			// is therefore drawn as a WORLD position - a fixed point near the
			// level origin, which in HQ is the floor by the fountain. It never
			// moves with the player, and a new one is left behind each time the
			// effect is rebuilt, which is the three or four copies the tester saw.
			//
			// The tester watched this happen and described it exactly while the desk was
			// still calling it the impact. It is not the impact, it is not the
			// tracer, and it is not the light - the light follows vWorldPos and
			// was already right.
			//
			// In VR, use the world point. m_vFlashPos is now built from
			// GetFireInfo's fire position and the authored per-weapon MuzzlePos,
			// so it is in the same space the bullets are in. FLAG_REALLYCLOSE
			// comes off with it: that flag is what tells everything downstream
			// to read the position as camera space, and the position is no
			// longer camera space.
			if (VRPrims_RebaseEverKnown())
			{
				uint32 dwF = m_pClientDE->GetObjectFlags(hObj);
				if (dwF & FLAG_REALLYCLOSE)
					m_pClientDE->SetObjectFlags(hObj, dwF & ~FLAG_REALLYCLOSE);
				m_pClientDE->SetObjectPos(hObj, &vWorldPos, LTTRUE);
			}
			else
			{
				m_pClientDE->SetObjectPos(hObj, &vCamRelPos, LTTRUE);
			}
		}
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMuzzleFlashFX::SetRot
//
//	PURPOSE:	Set the fx rotations
//
// ----------------------------------------------------------------------- //

void CMuzzleFlashFX::SetRot(LTRotation rRot)
{
	if (!m_pClientDE) return;

	HOBJECT hObj;

	if (m_bUsingParticles)
	{
		hObj = m_Particle.GetObject();
		if (hObj)
		{
			if (VRFlashParticleWorld() && m_cs.bPlayerView)
			{
				// The burst runs along the system's +Z; give it the drawn gun's
				// rotation, the same camera x gun product the flash model takes.
				const LTRotation rCam = VRPrims_RebaseLast().rCam;
				LTRotation rWorld = rCam * rRot;
				m_pClientDE->SetObjectRotation(hObj, &rWorld);
			}
			else if (g_vtReallyClose.GetFloat())
			{
				LTRotation rInitRot;
				rInitRot.Init();
				m_pClientDE->SetObjectRotation(hObj, &rInitRot);
			}
			else
			{
				m_pClientDE->SetObjectRotation(hObj, &rRot);
			}
		}
	}

	if (m_bUsingLight)
	{
		hObj = m_Light.GetObject();
		if (hObj)
		{
            m_pClientDE->SetObjectRotation(hObj, &rRot);
		}
	}

	if (m_bUsingScale)
	{
		hObj = m_Scale.GetObject();
		if (hObj)
		{
			LTRotation rTempRot = rRot;

			// Camera relative rotation...
			if (m_cs.bPlayerView)
			{
				rTempRot.Init();
				// ...WHICH IN THIS PORT IS THE WORLD'S FORWARD, NOT THE BARREL'S.
				//
				// Retail draws this object FLAG_REALLYCLOSE, so identity means
				// "down the camera's forward" - the barrel, in a flat game where
				// the gun is welded to the view. SetPos strips that flag and gives
				// the object a WORLD position, so identity now means world +Z,
				// whatever the gun is doing. Headset testing, 20 September, with the
				// position confirmed attached: the flash did not tilt or rotate with
				// the gun tip, so with the gun turned right or left the muzzle still
				// pointed forward.
				//
				// The gun itself is drawn as camBasis x R_obj (VRPublishModels'
				// node loop). rRot is R_obj - the hand's rotation in the camera
				// object's space - so the flash takes the same product, composed
				// through matrices in the same order that loop uses. The renderer
				// skins this model from its engine-posed nodes, and for a plain
				// world object those carry the object's rotation.
				// VRFlashRotBody 0 is the old arm.
				if (!g_vtVRFlashRotBody.IsInitted())
					g_vtVRFlashRotBody.Init(m_pClientDE, "VRFlashRotBody", LTNULL, 1.0f);
				const bool bCompose = (g_vtVRFlashRotBody.GetFloat() > 0.0f && VRPrims_RebaseEverKnown());
				if (bCompose)
				{
					// The SDK's quaternion product: "exactly the same as converting
					// both to matrices, multiplying (a*b) and converting back" -
					// ltrotation.h. Same order as the node loop's mc * mRelRot.
					const LTRotation rCam = VRPrims_RebaseLast().rCam;
					rTempRot = rCam * rRot;
				}
				// SAY WHAT WAS SET AND WHAT THE ENGINE KEPT. The first build of
				// this composed through ILTMath::SetupRotationFromMatrix and the
				// readback stayed at identity in both arms - a set that did not
				// take, with nothing printed to say so.
				{
					static int s_nSaidRot = 0;
					if (s_nSaidRot < 8)
					{
						++s_nSaidRot;
						LTVector vU, vR, vF, vGU, vGR, vGF;
						LTRotation rIn = rRot;
						m_pClientDE->GetRotationVectors(&rTempRot, &vU, &vR, &vF);
						m_pClientDE->GetRotationVectors(&rIn, &vGU, &vGR, &vGF);
						VRLog::Msg("VRFlashRot: %s | gun rot fwd %+.2f %+.2f %+.2f | set quat %.3f %.3f %.3f %.3f"
							" fwd %+.2f %+.2f %+.2f | cvar %.0f rebase %s",
							bCompose ? "COMPOSED cam*gun" : "identity (old)",
							vGF.x, vGF.y, vGF.z,
							rTempRot.m_Quat[0], rTempRot.m_Quat[1], rTempRot.m_Quat[2], rTempRot.m_Quat[3],
							vF.x, vF.y, vF.z,
							g_vtVRFlashRotBody.GetFloat(), VRPrims_RebaseEverKnown() ? "known" : "NOT known");
					}
				}
			}
			m_rVRGunRot = rRot;

            m_pClientDE->SetObjectRotation(hObj, &rTempRot);
			{
				static int s_nSaidBack = 0;
				if (s_nSaidBack < 8)
				{
					++s_nSaidBack;
					LTRotation rB; rB.Init();
					m_pClientDE->GetObjectRotation(hObj, &rB);
					VRLog::Msg("VRFlashRot: engine kept quat %.3f %.3f %.3f %.3f (flags %08X)",
						rB.m_Quat[0], rB.m_Quat[1], rB.m_Quat[2], rB.m_Quat[3],
						(unsigned)m_pClientDE->GetObjectFlags(hObj));
				}
			}
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMuzzleFlashFX::Term
//
//	PURPOSE:	If Term() is called on the CMuzzleFlashFX, Setup must be
//				called on the object to re-create any engine objects
//				associated with the fx.
//
// ----------------------------------------------------------------------- //

void CMuzzleFlashFX::Term()
{
	// Set our clientde pointer to NULL, this is how Setup knows to
	// create our sub-objects...
	m_pClientDE = LTNULL;

	// Term our sub-objects...

	m_Particle.Term();
	m_Scale.Term();
	m_Light.Term();
}
// ----------------------------------------------------------------------- //
//
// MODULE  : FlashLight.h
//
// PURPOSE : FlashLight class - Definition
//
// CREATED : 07/21/99
//
// (c) 1999 Monolith Productions, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //

#ifndef __FLASH_LIGHT_H__
#define __FLASH_LIGHT_H__

#include "ltbasedefs.h"
#include "PolyLineFX.h"

class CFlashLight
{
	public :

		CFlashLight();
		virtual ~CFlashLight();

		virtual void Update();

		virtual void Toggle()	{ (m_bOn ? TurnOff() : TurnOn());}
		virtual void TurnOn();
		virtual void TurnOff();

		virtual LTBOOL IsOn() const { return m_bOn; }

	protected :

		virtual void CreateLight();

		virtual void GetLightPositions(LTVector & vStartPos, LTVector & vEndPos, LTVector & vUOffset, LTVector & vROffset) = 0;

		virtual LTBOOL UpdateServer() { return LTTRUE; }

		// ---- THE TORCH YOU CAN SEE ----------------------------------------
		//
		// Requested in a visible flashlight
		// model when it is switched on, just like the guns - in a headset an unlit
		// empty hand throwing a beam reads as nothing being held.
		//
		// The model exists and always did: GUNS/MODELS_HH/FLASHLIGHT_HH.ABC in
		// NOLF.REZ, the hand-held torch the AI guards carry (AIHumanStrategy
		// attaches that same file). It is only the PLAYER who never had one,
		// because in a flat first-person game nobody can see their own hand.
		//
		// GetModelTransform returns LTFALSE by default so nothing changes for
		// the AI, whose torch is already attached to its wrist by the server -
		// giving them a second one is how you end up with two.
		virtual LTBOOL   GetModelTransform(LTVector & vPos, LTRotation & rRot) { return LTFALSE; }

		void             UpdateModel();
		void             CreateModel();
		void             HideModel();

	private :

        LTBOOL           m_bOn;
		HOBJECT			 m_hLight;
		HOBJECT			 m_hModel;			// the torch itself, VR only
		CPolyLineFX		 m_LightBeam;

        LTFLOAT          m_fMinLightRadius;
        LTFLOAT          m_fMaxLightRadius;
        LTFLOAT          m_fMaxLightDist;

        LTFLOAT          m_fServerUpdateTimer;
};

class CFlashLightPlayer : public CFlashLight
{
	public :

		CFlashLightPlayer();

	protected :

		void GetLightPositions(LTVector & vStartPos, LTVector & vEndPos, LTVector & vUOffset, LTVector & vROffset);

		// The hand pose the beam was last built from. Stashed rather than
		// recomputed: GetLightPositions runs first every frame and already did
		// all of this work, and two copies of the same trigonometry is how the
		// drawn gun and its muzzle flash came to disagree about where the
		// barrel was.
		LTBOOL GetModelTransform(LTVector & vPos, LTRotation & rRot);

		LTBOOL		m_bVRTorch;			// was the hand branch taken this frame
		LTVector	m_vVRTorchPos;		// where the beam starts, world
		LTVector	m_vVRTorchF;		// the hand's forward / up, world
		LTVector	m_vVRTorchU;
};

class CFlashLightAI : public CFlashLight
{
	public :

		CFlashLightAI();
		~CFlashLightAI();

		void Init(HOBJECT hAI);

		void Update();

	protected :

		void GetLightPositions(LTVector & vStartPos, LTVector & vEndPos, LTVector & vUOffset, LTVector & vROffset);

		LTBOOL UpdateServer() { return LTFALSE; }

	protected :

		HOBJECT			m_hAI;
};

#endif // __FLASH_LIGHT_H__
// ----------------------------------------------------------------------- //
//
// MODULE  : MuzzleFlashFX.h
//
// PURPOSE : MuzzleFlash special fx class - Definition
//
// CREATED : 12/17/99
//
// (c) 1999-2000 Monolith Productions, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //

#ifndef __MUZZLE_FLASH_FX_H__
#define __MUZZLE_FLASH_FX_H__

#include "SpecialFX.h"
#include "WeaponMgr.h"
#include "MuzzleFlashParticleFX.h"
#include "DynamicLightFX.h"
#include "BaseScaleFX.h"

struct MUZZLEFLASHCREATESTRUCT : public SFXCREATESTRUCT
{
    MUZZLEFLASHCREATESTRUCT();

	WEAPON*		pWeapon;		// Weapon data
	HOBJECT		hParent;		// Character or Player model if bPlayerView = TRUE
    LTVector     vPos;           // Initial pos
    LTRotation   rRot;           // Initial rotation
    LTBOOL       bPlayerView;    // Is this fx tied to the client-side player view
};

inline MUZZLEFLASHCREATESTRUCT::MUZZLEFLASHCREATESTRUCT()
{
    rRot.Init();
	vPos.Init();
    hParent     = LTNULL;
    pWeapon     = LTNULL;
    bPlayerView = LTFALSE;
}


class CMuzzleFlashFX : public CSpecialFX
{
	public :

		CMuzzleFlashFX() : CSpecialFX()
		{
            m_bUsingParticles   = LTFALSE;
            m_bUsingScale       = LTFALSE;
            m_bUsingLight       = LTFALSE;
            m_bHidden           = LTFALSE;
			m_rVRGunRot.Init();
		}

		void Term();

        LTBOOL Setup(MUZZLEFLASHCREATESTRUCT & cs);

        virtual LTBOOL Init(HLOCALOBJ hServObj, HMESSAGEREAD hRead);
        virtual LTBOOL Init(SFXCREATESTRUCT* psfxCreateStruct);
        virtual LTBOOL CreateObject(ILTClient* pClientDE);
        virtual LTBOOL Update();

		void Hide();
		void Show();
        void SetPos(LTVector vWorldPos, LTVector vCamRelPos);
        void SetRot(LTRotation rRot);

		virtual uint32 GetSFXID() { return SFX_MUZZLEFLASH_ID; }

		// VR: THE FLASH'S OWN OBJECTS, so the publish path can be HANDED them.
		//
		// A FLAG_REALLYCLOSE object holds no world position - LithTech keeps
		// it in camera space - so it sits a few units from the world ORIGIN
		// wherever the player is standing, and the eye-centred sphere that
		// gathers sprites can never reach it. The view weapon is already
		// added explicitly for exactly this reason (see nViewWeapon); its
		// flash needs the same. the muzzle
		// flash was not attached to the gun.
		HLOCALOBJ VRGetScaleObject() const { return m_Scale.GetObject(); }
		LTBOOL    VRUsingScale()     const { return m_bUsingScale; }
		// The other two thirds of a NOLF muzzle flash. The P38's PV flash
		// has NO scale fx at all - its ScaleFXName is commented out in
		// ATTRIBUTES/FX.TXT - so asking only about the scale object says
		// 'no flash' for a weapon whose flash is particles and a light.
		LTBOOL    VRUsingParticles() const { return m_bUsingParticles; }
		LTBOOL    VRUsingLight()     const { return m_bUsingLight; }
		LTBOOL    VRHidden()         const { return m_bHidden; }
		HLOCALOBJ VRParticleObject() const { return m_Particle.GetObject(); }
		HLOCALOBJ VRLightObject()    const { return m_Light.GetObject(); }
	private :

        LTBOOL   Reset(MUZZLEFLASHCREATESTRUCT & cs);
        LTBOOL   ResetFX();
		void	 ReallyHide();


		MUZZLEFLASHCREATESTRUCT		m_cs;		// Our data

		CMuzzleFlashParticleFX		m_Particle; // Particle system
		CBaseScaleFX				m_Scale;	// Model/Sprite
		CDynamicLightFX				m_Light;	// Dynamic light

        LTBOOL                       m_bUsingParticles;
        LTBOOL                       m_bUsingScale;
        LTBOOL                       m_bUsingLight;
        LTBOOL                       m_bHidden;
		// The gun rotation SetRot was last given (CWeaponModel::VRGunRot), so
		// the readback in SetPos can compare the flash's drawn forward with
		// the gun's. One frame stale, which a still hand cannot tell.
		LTRotation					m_rVRGunRot;
};

#endif // __MUZZLE_FLASH_FX_H__
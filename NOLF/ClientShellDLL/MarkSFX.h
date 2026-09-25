 // ----------------------------------------------------------------------- //
//
// MODULE  : MarkSFX.h
//
// PURPOSE : Mark special fx class - Definition
//
// CREATED : 11/6/97
//
// ----------------------------------------------------------------------- //

#ifndef __MARKSFX_H__
#define __MARKSFX_H__

#include "SpecialFX.h"
#include "ltlink.h"

struct MARKCREATESTRUCT : public SFXCREATESTRUCT
{
    MARKCREATESTRUCT();

    LTRotation   m_Rotation;
    LTVector     m_vPos;
    LTFLOAT      m_fScale;
    uint8       nAmmoId;
    uint8       nSurfaceType;
};

inline MARKCREATESTRUCT::MARKCREATESTRUCT()
{
    m_Rotation.Init();
	m_vPos.Init();
	m_fScale		= 0.0f;
	nAmmoId			= 0;
	nSurfaceType	= 0;
}


class CMarkSFX : public CSpecialFX
{
	public :

		CMarkSFX()
		{
            m_Rotation.Init();
			VEC_INIT(m_vPos);
			m_fScale = 1.0f;
			m_nAmmoId = 0;
			m_nSurfaceType = 0;
			m_fStartTime = 0.0f;
			m_hRideObj = LTNULL;
			m_vRideOfs.Init();
			m_vRideFwd.Init();
			m_vRideUp.Init();
		}

        virtual LTBOOL Init(SFXCREATESTRUCT* psfxCreateStruct);
        virtual LTBOOL Update();
        virtual LTBOOL CreateObject(ILTClient* pClientDE);

        virtual void WantRemove(LTBOOL bRemove=LTTRUE);

		virtual uint32 GetSFXID() { return SFX_MARK_ID; }

		// THE SURFACE THIS MARK IS STUCK TO, when that surface can move.
		//
		// A mark is created at a world position and never moves again, so a
		// bullet hole in a door stays in the air when the door opens - which
		// is what it does in retail too, on a monitor, where nobody notices.
		// In a headset you walk up to it.
		//
		// Only set when the thing hit is NOT the main world: static geometry
		// needs none of this and would pay for it on every mark.
		void            RideOn(HLOCALOBJ hObj);

	private :

		HLOCALOBJ       m_hRideObj;     // the brush this mark is stuck to
		LTVector        m_vRideOfs;     // its position in that brush's frame
		LTVector        m_vRideFwd;     // and its facing, likewise
		LTVector        m_vRideUp;

		void            UpdateRide();

	public :

	private :

        LTRotation	m_Rotation;
        LTVector	m_vPos;
        LTFLOAT		m_fScale;
		LTFLOAT		m_fStartTime;
        uint8		m_nAmmoId;
        uint8		m_nSurfaceType;
};

#endif // __MARKSFX_H__
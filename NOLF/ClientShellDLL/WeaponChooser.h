// WeaponChooser.h: interface for the CWeaponChooser class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_WEAPONCHOOSER_H__1762B140_8553_11D3_B2DB_006097097C7B__INCLUDED_)
#define AFX_WEAPONCHOOSER_H__1762B140_8553_11D3_B2DB_006097097C7B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "stdlith.h"

#define NUM_WEAPON_ICONS	3
#define NUM_AMMO_ICONS		2

class CWeaponChooser
{
public:
	CWeaponChooser();
	virtual ~CWeaponChooser();

	void	Term();

    LTBOOL   Open();
	void	Close();
    LTBOOL   IsOpen()    {return m_bIsOpen;}

	void	NextWeapon();
	void	PrevWeapon();

	void	Draw();


    uint8   GetCurrentSelection() {return m_nWeapons[1];}

private:
	void		SetCommandStr(int nWeaponId);

	HSURFACE	m_hWeaponSurf[NUM_WEAPON_ICONS];
    uint8       m_nWeapons[NUM_WEAPON_ICONS];
    LTBOOL       m_bIsOpen;
	HSTRING		m_hWeaponStr;
	float		m_fStartTime;

	char		m_szWeaponCommand[8];
};

class CAmmoChooser
{
public:
	CAmmoChooser();
	virtual ~CAmmoChooser();

	void	Term();

    LTBOOL   Open();
	void	Close();
    LTBOOL   IsOpen()    {return m_bIsOpen;}

	void	NextAmmo();

	void	Draw();


    uint8   GetCurrentSelection() {return m_nAmmo[0];}

private:
	HSURFACE	m_hAmmoSurf[NUM_AMMO_ICONS];
    uint8       m_nAmmo[NUM_AMMO_ICONS];
    LTBOOL       m_bIsOpen;
	HSTRING		m_hAmmoStr;
	float		m_fStartTime;
};

// THE VR WEAPON WHEEL. Asked for in headset testing: a way to change
// weapons. A radial menu of every weapon the player carries,
// drawn on the HUD around the centre of the view. Opened with the RIGHT
// stick click; the right stick then points at a slot (turning is off while
// it is open); letting the stick spring back to centre selects the slot it
// pointed at and closes the wheel, as does the trigger or a second click.
// A click with nothing pointed at just closes it.
#define VRWHEEL_MAX_SLOTS	16

class CVRWeaponWheel
{
public:
	CVRWeaponWheel();
	~CVRWeaponWheel();

	void	Init();
	void	Term();

	LTBOOL	Open();
	// bSelect: change to the highlighted weapon (if any) on the way out.
	void	Close(LTBOOL bSelect);
	LTBOOL	IsOpen() const { return m_bIsOpen; }

	// The right stick, every frame while open. Deflection highlights the
	// nearest slot; a return to centre after a highlight selects it.
	void	Update(float fStickX, float fStickY);
	void	Draw();

private:
	void	FreeSurfaces();

	LTBOOL		m_bIsOpen;
	int			m_nSlots;
	uint8		m_nWeapon[VRWHEEL_MAX_SLOTS];
	HSURFACE	m_hSurf[VRWHEEL_MAX_SLOTS];
	uint32		m_nSurfW[VRWHEEL_MAX_SLOTS], m_nSurfH[VRWHEEL_MAX_SLOTS];
	int			m_nHighlight;			// -1 none
	LTBOOL		m_bDeflected;			// the stick has been out since opening
	HSTRING		m_hName;				// the highlighted weapon's name
	float		m_fOpenTime;
};

#endif // !defined(AFX_WEAPONCHOOSER_H__1762B140_8553_11D3_B2DB_006097097C7B__INCLUDED_)
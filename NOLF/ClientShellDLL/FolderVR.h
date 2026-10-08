// FolderVR.h: the VR options page.
//
// Everything on it is a cvar this port already had. They were reachable only
// by typing them on a command line, which meant that in practice nobody
// changed any of them - requested in a VR
// options page on the pause menu, with a head bob toggle.
//
// Labels are literal strings rather than IDS_ ids on purpose: the menu text
// lives in CRes.dll and adding a string there is a resource rebuild, while
// every control in CBaseFolder already has a char* overload.

#ifndef _FOLDER_VR_H_
#define _FOLDER_VR_H_

#include "BaseFolder.h"
#include "CycleCtrl.h"
#include "VRBinds.h"

// A row on the Controls page: a cycle control that skips buttons another row
// already uses (FolderVR.cpp).
class CVRBindCtrl : public CCycleCtrl
{
public:
	CVRBindCtrl() : m_ppRows(LTNULL), m_nRow(0) {}
	virtual LTBOOL OnLeft();
	virtual LTBOOL OnRight();
	CVRBindCtrl**	m_ppRows;		// every row on the page, VRBinds::ACT_COUNT of them
	int				m_nRow;			// which one this is
private:
	LTBOOL			Step(int nDir);
};

class CFolderVR;

// PHYSICAL PLAY's first row: Buttons (simple) or Physical sets every row below
// it at once; Custom is what it says when the rows differ, and is only shown.
class CVRPhysPresetCtrl : public CCycleCtrl
{
public:
	CVRPhysPresetCtrl() : m_pFolder(LTNULL) {}
	virtual LTBOOL OnLeft();
	virtual LTBOOL OnRight();
	CFolderVR*		m_pFolder;
};

class CFolderVR : public CBaseFolder
{
public:
	CFolderVR();
	virtual ~CFolderVR();

	LTBOOL	Build();
	void	OnFocus(LTBOOL bFocus);
	uint32	OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2);

protected:
	void	AddOnOff(char* pLabel, LTBOOL* pb);
	void	AddRange(char* pLabel, int* pn, int nMin, int nMax, int nStep);
	void	FillBindRows();
public:
	// PHYSICAL PLAY (FOLDER_ID_VR_PHYSICAL): every part on (1) or off (0).
	void	ApplyPhysicalPreset(int nPreset);
protected:
	int		PhysicalPresetNow() const;
	LTBOOL	m_bHolsters;		// VRHolsters
	int		m_nHolsterHeight;	// VRHolsterHeight + 15, cm
	LTBOOL	m_bManualReload;	// VRManualReload
	LTBOOL	m_bSwingMelee;		// VRSwingMelee
	LTBOOL	m_bWristHud;		// VRWristHud
	LTBOOL	m_bGlassesToFace;	// VRGlassesToFace
	LTBOOL	m_bThrowByHand;		// VRThrowByHand
	LTBOOL	m_bTwoHanded;		// VRTwoHanded
	int		m_nPhysPreset;		// 0 buttons, 1 physical, 2 custom (shown only)

	// CONTROLS (FOLDER_ID_VR_CONTROLS only)
	CLTGUITextItemCtrl*	m_pBindHeader;
	CVRBindCtrl*		m_pBindRows[VRBinds::ACT_COUNT];
	int					m_nBind[VRBinds::ACT_COUNT];	// Pad per action, as shown
	bool				m_bBindFrame;					// the map shown is the Frame's

	// COMFORT
	int		m_nHeadBob;
	int		m_nWeaponSway;
	LTBOOL	m_bSnapTurn;
	int		m_nTurnRate;
	LTBOOL	m_bSteerHands;		// VRVehicleSteer: hold both grips to steer a vehicle
	LTBOOL	m_bLeftorium;		// VRLeftorium: left-handed play (the hands swap)
	LTBOOL	m_bSwapSticks;		// VRSwapSticks: move on the right, turn on the left
	int		m_nWalkDir;			// VRWalkDir: 0 body, 1 head, 2 off hand (VRWalk.h)
	LTBOOL	m_bAnalogWalk;		// VRAnalogWalk: the stick's push sets the speed

	// THE WEAPON
	LTBOOL	m_bGunAtHand;
	int		m_nGunScale;		// tenths
	int		m_nHandTravel;		// units per metre of real hand movement
	LTBOOL	m_bRumble;			// VRHaptics: the controllers buzz on firing and on being hit
	LTBOOL	m_bCateHands;		// VRCateHands: Cate's own articulated hands (VRHands.cpp)
	int		m_nHandsOutfit;		// VRHandsOutfit: 0 painted, 1 Action .. 5 Winter (VRHands.cpp)

	// AIM
	LTBOOL	m_bAimDot;
	// VR_DEBUG_TOOLS - remove these six with the rest
	LTBOOL	m_bDebugSkip;
	LTBOOL	m_bDebugGod;
	LTBOOL	m_bDebugClip;
	LTBOOL	m_bDebugNoAI;
	LTBOOL	m_bDebugMissions;
	LTBOOL	m_bDebugArsenal;
	LTBOOL	m_bDebugPos;
	LTBOOL	m_bVRCaptions;
	int		m_nAimDotSize;		// hundredths

	// THE PICTURE
	LTBOOL	m_bBigMenuText;
	LTBOOL	m_bShowBody;
	LTBOOL	m_bMirrorBody;		// the player's body in mirrors, next launch
	int		m_nResolution;		// percent of the headset's size, next launch
	LTBOOL	m_bSpectator;		// the steadied desktop view, next launch
};

#endif

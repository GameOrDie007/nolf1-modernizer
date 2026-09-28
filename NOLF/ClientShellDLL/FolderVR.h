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

class CFolderVR : public CBaseFolder
{
public:
	CFolderVR();
	virtual ~CFolderVR();

	LTBOOL	Build();
	void	OnFocus(LTBOOL bFocus);
	uint32	OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2);

protected:
	// COMFORT
	int		m_nHeadBob;
	int		m_nWeaponSway;
	LTBOOL	m_bSnapTurn;
	int		m_nTurnRate;
	LTBOOL	m_bSteerHands;		// VRVehicleSteer: hold both grips to steer a vehicle
	LTBOOL	m_bLeftorium;		// VRLeftorium: left-handed play (the hands swap)
	LTBOOL	m_bSwapSticks;		// VRSwapSticks: move on the right, turn on the left

	// THE WEAPON
	LTBOOL	m_bGunAtHand;
	int		m_nGunScale;		// tenths
	int		m_nHandTravel;		// units per metre of real hand movement

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

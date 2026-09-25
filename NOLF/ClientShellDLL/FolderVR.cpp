// FolderVR.cpp: the VR options page.
//
// Modelled on CFolderGame, which is the closest thing in the game to what this
// is: a column of sliders and toggles that read and write console variables.
//
// WHY IT EXISTS. This port had accumulated a lot of VR settings and no way to
// reach any of them without a command line. The tester had been playing with head
// bob on for weeks because turning it off meant knowing the cvar was there.
//
// helpID is 0 throughout. The help line at the bottom of a folder is looked up
// in CRes.dll by id and there is no id for any of this text; 0 leaves the line
// blank rather than printing something misleading.

#include "stdafx.h"
#include "FolderVR.h"
#include "FolderMgr.h"
#include "FolderCommands.h"
#include "ClientRes.h"

#include "GameClientShell.h"
#include "GameSettings.h"
#include "VRShared.h"

bool VRDebugToolsBuilt();		// GameClientShell.cpp: is VR_DEBUG_TOOLS compiled in?

namespace
{
	int kGap = 0;
	int kWidth = 0;

	// Read a console float with a default, the way CFolderGame does.
	inline LTFLOAT CVarF(char* p, LTFLOAT f) { return GetConsoleFloat(p, f); }

	const uint32 CMD_VR_RECENTER = FOLDER_CMD_CUSTOM + 1;
}

CFolderVR::CFolderVR()
{
	m_nHeadBob     = 0;
	m_nWeaponSway  = 0;
	m_bSnapTurn    = LTFALSE;
	m_nTurnRate    = 3;
	m_bSteerHands  = LTTRUE;
	m_bLeftorium   = LTFALSE;
	m_bSwapSticks  = LTFALSE;
	m_bGunAtHand   = LTTRUE;
	m_nGunScale    = 4;
	m_nHandTravel  = 35;		// tenths: 3.5 units per metre
	m_bAimDot      = LTTRUE;
	m_bDebugSkip   = LTFALSE;		// VR_DEBUG_TOOLS
	m_bDebugGod    = LTFALSE;
	m_bDebugClip   = LTFALSE;
	m_bDebugNoAI   = LTFALSE;
	m_bDebugMissions = LTFALSE;
	m_bDebugArsenal  = LTFALSE;
	m_bDebugPos      = LTFALSE;
	m_bVRCaptions  = LTTRUE;
	m_nAimDotSize  = 20;
	m_bBigMenuText = LTTRUE;
	m_bShowBody    = LTFALSE;
}

CFolderVR::~CFolderVR()
{
}

LTBOOL CFolderVR::Build()
{
	CreateTitle("VR");

	if (g_pLayoutMgr->HasCustomValue(FOLDER_ID_VR, "ColumnWidth"))
		kGap = g_pLayoutMgr->GetFolderCustomInt(FOLDER_ID_VR, "ColumnWidth");
	if (g_pLayoutMgr->HasCustomValue(FOLDER_ID_VR, "SliderWidth"))
		kWidth = g_pLayoutMgr->GetFolderCustomInt(FOLDER_ID_VR, "SliderWidth");

	// A folder with no layout section would put every control at zero. These
	// are CFolderGame's numbers, which is the page this one is shaped like.
	if (kGap <= 0)   kGap = 300;
	if (kWidth <= 0) kWidth = 150;

	LTFLOAT yr = g_pInterfaceResMgr->GetYRatio();
	kGap *= yr;
	kWidth *= yr;

	// RECENTER, AT THE TOP. It was both thumbsticks clicked together, which is
	// also Virtual Desktop's own overlay, and the VR host treated the same
	// chord as a debug trigger that saved three screenshots a time. Now: this
	// row, or hold the headset's Meta button (the runtime's own recenter).
	AddTextItem("Recenter", CMD_VR_RECENTER, 0);

	// ---- COMFORT ------------------------------------------------------
	//
	// Head bob first, and at the top, because it is the one that was asked
	// for by name. In VR the camera is the head: bobbing it moves the world
	// under someone who is not moving, which is the textbook way to make a
	// person ill.
	CSliderCtrl* pSlider = AddSlider("Head bob", 0, kGap, kWidth, &m_nHeadBob);
	pSlider->SetSliderRange(0, 10);
	pSlider->SetSliderIncrement(1);

	pSlider = AddSlider("Weapon sway", 0, kGap, kWidth, &m_nWeaponSway);
	pSlider->SetSliderRange(0, 10);
	pSlider->SetSliderIncrement(1);

	CToggleCtrl* pToggle = AddToggle("Snap turning", 0, kGap, &m_bSnapTurn);
	pToggle->SetOnString(IDS_ON);
	pToggle->SetOffString(IDS_OFF);

	pSlider = AddSlider("Turn speed", 0, kGap, kWidth, &m_nTurnRate);
	pSlider->SetSliderRange(1, 10);
	pSlider->SetSliderIncrement(1);

	// Snowmobile and motorcycle: hold both grips and the line between the
	// controllers is the handlebar. Off, the right stick steers as before.
	pToggle = AddToggle("Steer with both grips", 0, kGap, &m_bSteerHands);
	pToggle->SetOnString(IDS_ON);
	pToggle->SetOffString(IDS_OFF);

	// ---- THE WEAPON ---------------------------------------------------
	// THE LEFTORIUM (left-handed play) - named for Ned Flanders' shop. The
	// hands swap: the gun and its trigger in the left hand, the torch in the
	// right. The STICKS are their own row below: a left-handed player still
	// moves with the left stick (asked by the player the Leftorium is for).
	pToggle = AddToggle("Leftorium (left-handed)", 0, kGap, &m_bLeftorium);
	pToggle->SetOnString(IDS_ON);
	pToggle->SetOffString(IDS_OFF);

	pToggle = AddToggle("Swap sticks", 0, kGap, &m_bSwapSticks);
	pToggle->SetOnString(IDS_ON);
	pToggle->SetOffString(IDS_OFF);

	pToggle = AddToggle("Gun follows hand", 0, kGap, &m_bGunAtHand);
	pToggle->SetOnString(IDS_ON);
	pToggle->SetOffString(IDS_OFF);

	pSlider = AddSlider("Gun size", 0, kGap, kWidth, &m_nGunScale);
	pSlider->SetSliderRange(1, 20);
	pSlider->SetSliderIncrement(1);

	// TENTHS, like the aim dot. This slider stored WHOLE units and wrote them
	// back, so opening the page turned the 3.46 default into 3 - and the
	// hand's travel came up short by 13% every time the page was opened.
	pSlider = AddSlider("Hand movement", 0, kGap, kWidth, &m_nHandTravel);
	pSlider->SetSliderRange(10, 120);
	pSlider->SetSliderIncrement(2);

	// ---- AIM ----------------------------------------------------------
	pToggle = AddToggle("Aim dot", 0, kGap, &m_bAimDot);
	pToggle = AddToggle("VR captions", 0, kGap, &m_bVRCaptions);
	pToggle->SetOnString(IDS_ON);
	pToggle->SetOffString(IDS_OFF);

	// Tenths of a world unit: 10 is a real laser dot, about 17 mm across.
	pSlider = AddSlider("Aim dot size", 0, kGap, kWidth, &m_nAimDotSize);
	pSlider->SetSliderRange(3, 30);
	pSlider->SetSliderIncrement(1);

	// ---- THE PICTURE --------------------------------------------------
	// The body is a renderer switch (StubBody) and takes effect on the next
	// launch; the label says so.
	pToggle = AddToggle("Show body (restart)", 0, kGap, &m_bShowBody);
	pToggle->SetOnString(IDS_ON);
	pToggle->SetOffString(IDS_OFF);

	pToggle = AddToggle("Large menu text", 0, kGap, &m_bBigMenuText);
	// VR_DEBUG_TOOLS - testing aids, not features. All off by default, all
	// removable together: delete this block, the members in FolderVR.h, the
	// two lines each in the read and write below, and the marked blocks in
	// GameClientShell.cpp and MessageMgr.h.
	// ONLY IN A BUILD THAT CAN ACT ON THEM. The code that applies these is
	// compiled out of a release (VR_DEBUG_TOOLS 0), and a menu row that does
	// nothing is a bug report waiting to be written - so a release does not
	// show them.
	if (VRDebugToolsBuilt())
	{
	pToggle = AddToggle("Debug: god mode",          0, kGap, &m_bDebugGod);
	pToggle = AddToggle("Debug: walk through walls",0, kGap, &m_bDebugClip);
	pToggle = AddToggle("Debug: all weapons+gear",  0, kGap, &m_bDebugArsenal);
	// KILLS, and cannot be undone within a level: the server's remove-AI
	// handler ignores its flag and detonates every AI it finds. Named for
	// what it does.
	pToggle = AddToggle("Debug: kill all enemies",  0, kGap, &m_bDebugNoAI);
	pToggle = AddToggle("Debug: all missions",      0, kGap, &m_bDebugMissions);
	// Where am I? So a bug report can carry coordinates and the desk harness
	// can stand in the same spot: look-shot.ps1 -At "x,y,z".
	pToggle = AddToggle("Debug: show position",     0, kGap, &m_bDebugPos);
	pToggle = AddToggle("Debug: skip level (R grip + B)", 0, kGap, &m_bDebugSkip);
	}
	pToggle->SetOnString(IDS_ON);
	pToggle->SetOffString(IDS_OFF);

	// NO ANTI-ALIASING CONTROL HERE ON PURPOSE. The renderer's multisampling
	// is built and free but draws sky-coloured shards through alpha-tested
	// geometry, so it ships off; a menu is the last place to offer that. The
	// aliasing was addressed by raising the render resolution instead, which
	// needs no setting. See docs/PERFORMANCE.md.

	if (!CBaseFolder::Build()) return LTFALSE;

	UseBack(LTTRUE, LTTRUE);
	return LTTRUE;
}

void CFolderVR::OnFocus(LTBOOL bFocus)
{
	if (bFocus)
	{
		m_nHeadBob     = (int)(10.0f * CVarF("HeadBob", 1.0f));
		m_nWeaponSway  = (int)(10.0f * CVarF("WeaponSway", 1.0f));
		m_bSnapTurn    = (CVarF("VRSnapTurn", 0.0f) > 0.0f);
		m_nTurnRate    = (int)CVarF("NormalTurnRate", 3.0f);
		m_bSteerHands  = (CVarF("VRVehicleSteer", 1.0f) > 0.0f);
		m_bLeftorium   = (CVarF("VRLeftorium", 0.0f) > 0.0f);
		m_bSwapSticks  = (CVarF("VRSwapSticks", 0.0f) > 0.0f);
		m_bGunAtHand   = (CVarF("VRGunAtHand", 1.0f) > 0.0f);
		m_nGunScale    = (int)(10.0f * CVarF("VRViewModelSize", 1.0f));
		m_nHandTravel  = (int)(10.0f * CVarF("VRHandPosScale", 3.46f) + 0.5f);
		m_bAimDot      = (CVarF("VRAimMarker", 1.0f) > 0.0f);
		m_bDebugSkip   = (CVarF("VRDebugSkip", 0.0f) > 0.0f);	// VR_DEBUG_TOOLS
		m_bDebugGod    = (CVarF("VRDebugGod", 0.0f) > 0.0f);
		m_bDebugClip   = (CVarF("VRDebugClip", 0.0f) > 0.0f);
		m_bDebugNoAI   = (CVarF("VRDebugNoAI", 0.0f) > 0.0f);
		m_bDebugMissions = (CVarF("VRDebugMissions", 0.0f) > 0.0f);
		m_bDebugArsenal  = (CVarF("VRDebugArsenal", 0.0f) > 0.0f);
		m_bDebugPos      = (CVarF("VRDebugPos", 0.0f) > 0.0f);
		m_bVRCaptions  = (CVarF("VRCaptions", 1.0f) > 0.0f);
		m_nAimDotSize  = (int)(10.0f * CVarF("VRAimMarkerWorldSize", 2.0f));
		m_bBigMenuText = (CVarF("VRMenuBigSubs", 1.0f) > 0.0f);
		m_bShowBody    = (CVarF("StubBody", 0.0f) > 0.0f);

		// Clamp what came out of the config into the ranges the controls
		// offer. A slider handed a value outside its range draws its thumb
		// off the end of the track, which reads as a broken menu.
		if (m_nGunScale < 1)   m_nGunScale = 1;
		if (m_nGunScale > 20)  m_nGunScale = 20;
		if (m_nHandTravel < 10) m_nHandTravel = 10;
		if (m_nHandTravel > 120) m_nHandTravel = 120;
		if (m_nAimDotSize < 3) m_nAimDotSize = 3;
		if (m_nAimDotSize > 30) m_nAimDotSize = 30;
		if (m_nTurnRate < 1)   m_nTurnRate = 1;
		if (m_nTurnRate > 10)  m_nTurnRate = 10;

		UpdateData(LTFALSE);
	}
	else
	{
		UpdateData();

		WriteConsoleFloat("HeadBob",         (LTFLOAT)m_nHeadBob / 10.0f);
		WriteConsoleFloat("WeaponSway",      (LTFLOAT)m_nWeaponSway / 10.0f);
		WriteConsoleInt  ("VRSnapTurn",      (int)m_bSnapTurn);
		WriteConsoleFloat("NormalTurnRate",  (LTFLOAT)m_nTurnRate);
		WriteConsoleFloat("FastTurnRate",    (LTFLOAT)m_nTurnRate * 1.1f);
		WriteConsoleInt  ("VRVehicleSteer",  (int)m_bSteerHands);
		WriteConsoleInt  ("VRLeftorium",     (int)m_bLeftorium);
		WriteConsoleInt  ("VRSwapSticks",    (int)m_bSwapSticks);
		WriteConsoleInt  ("VRGunAtHand",     (int)m_bGunAtHand);
		WriteConsoleFloat("VRViewModelSize", (LTFLOAT)m_nGunScale / 10.0f);
		WriteConsoleFloat("VRHandPosScale",  (LTFLOAT)m_nHandTravel / 10.0f);
		WriteConsoleInt  ("VRAimMarker",     (int)m_bAimDot);
		WriteConsoleInt  ("VRDebugSkip",     (int)m_bDebugSkip);	// VR_DEBUG_TOOLS
		WriteConsoleInt  ("VRDebugGod",      (int)m_bDebugGod);
		WriteConsoleInt  ("VRDebugClip",     (int)m_bDebugClip);
		WriteConsoleInt  ("VRDebugNoAI",     (int)m_bDebugNoAI);
		WriteConsoleInt  ("VRDebugMissions", (int)m_bDebugMissions);
		WriteConsoleInt  ("VRDebugArsenal",  (int)m_bDebugArsenal);
		WriteConsoleInt  ("VRDebugPos",      (int)m_bDebugPos);
		WriteConsoleInt  ("VRCaptions",      (int)m_bVRCaptions);
		WriteConsoleFloat("VRAimMarkerWorldSize", (LTFLOAT)m_nAimDotSize / 10.0f);
		WriteConsoleInt  ("VRMenuBigSubs",   (int)m_bBigMenuText);
		WriteConsoleInt  ("StubBody",        (int)m_bShowBody);

		// The engine rewrites this file on exit anyway, but not until then -
		// and a player who sets head bob to zero and is killed by a crash
		// should not have to set it again.
		g_pLTClient->WriteConfigFile("autoexec.cfg");
	}
	CBaseFolder::OnFocus(bFocus);
}

uint32 CFolderVR::OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2)
{
	if (dwCommand == CMD_VR_RECENTER)
	{
		// The same request the chord made; the host folds the head's yaw and
		// position into its reference space.
		VRShared::RequestRecenter();
		g_pInterfaceMgr->RequestInterfaceSound(IS_SELECT);
		return 1;
	}
	return CBaseFolder::OnCommand(dwCommand, dwParam1, dwParam2);
}

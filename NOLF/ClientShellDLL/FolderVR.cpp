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
#include "VRBinds.h"
#include "VRLog.h"

bool VRDebugToolsBuilt();		// GameClientShell.cpp: is VR_DEBUG_TOOLS compiled in?

namespace
{
	int kGap = 0;
	int kWidth = 0;

	// Read a console float with a default, the way CFolderGame does.
	inline LTFLOAT CVarF(char* p, LTFLOAT f) { return GetConsoleFloat(p, f); }

	const uint32 CMD_VR_RECENTER   = FOLDER_CMD_CUSTOM + 1;
	const uint32 CMD_VR_BIND_RESET = FOLDER_CMD_CUSTOM + 2;
	const uint32 CMD_VR_STYLE_PHYS = FOLDER_CMD_CUSTOM + 3;
	const uint32 CMD_VR_STYLE_BTNS = FOLDER_CMD_CUSTOM + 4;
	const uint32 CMD_VR_PAGE       = FOLDER_CMD_CUSTOM + 10;	// + page index
}

CFolderVR::CFolderVR()
{
	m_nHeadBob     = 0;
	m_nWeaponSway  = 0;
	m_bSnapTurn    = LTFALSE;
	m_nTurnRate    = 120;
	m_bSteerHands  = LTTRUE;
	m_bLeftorium   = LTFALSE;
	m_bSwapSticks  = LTFALSE;
	m_nWalkDir     = 0;
	m_bAnalogWalk  = LTFALSE;
	m_bGunAtHand   = LTTRUE;
	m_bRumble      = LTTRUE;
	m_nRumbleStrength = 100;
	m_bMatchRefresh = LTFALSE;
	m_nGunPos[0] = m_nGunPos[1] = m_nGunPos[2] = 10;
	m_bCateHands   = LTFALSE;
	m_nHandsOutfit = 6;
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
	m_bShowBody    = LTTRUE;
	m_bRealCrouch  = LTFALSE;
	m_bMirrorBody  = LTTRUE;
	m_nResolution  = 100;
	m_nResIdx      = 8;
	m_nMenuSize    = 100;
	m_nMenuDist    = 170;
	m_bSpectator   = LTTRUE;
	m_pBindHeader  = LTNULL;
	m_bBindFrame   = false;
	for (int a = 0; a < VRBinds::ACT_COUNT; ++a) { m_pBindRows[a] = LTNULL; m_nBind[a] = 0; }
	m_bHolsters      = LTFALSE;
	m_nHolsterHeight = 15;
	m_bManualReload  = LTFALSE;
	m_bSwingMelee    = LTFALSE;
	m_bWristHud      = LTFALSE;
	m_bGlassesToFace = LTFALSE;
	m_bThrowByHand   = LTFALSE;
	m_bTwoHanded     = LTFALSE;
	m_bRackSlide     = LTFALSE;
	m_bRoomScale     = LTTRUE;
	m_bWallFade      = LTTRUE;
	m_bAmmoOnGun     = LTFALSE;
	m_nPhysPreset    = 0;
}

// PHYSICAL PLAY'S PRESET ROW. Left or right flips between the two presets and
// sets every part's row at once; "Custom" is only ever shown, when the parts
// were set one by one.
LTBOOL CVRPhysPresetCtrl::OnRight()
{
	const int n = (GetSelIndex() == 1) ? 0 : 1;
	SetSelIndex(n);
	if (m_pFolder) m_pFolder->ApplyPhysicalPreset(n);
	return LTTRUE;
}

LTBOOL CVRPhysPresetCtrl::OnLeft()
{
	return OnRight();
}

int CFolderVR::PhysicalPresetNow() const
{
	const int nOn = (m_bHolsters ? 1 : 0) + (m_bManualReload ? 1 : 0) + (m_bSwingMelee ? 1 : 0)
				  + (m_bWristHud ? 1 : 0) + (m_bGlassesToFace ? 1 : 0) + (m_bThrowByHand ? 1 : 0)
				  + (m_bTwoHanded ? 1 : 0) + (m_bRackSlide ? 1 : 0) + (m_bAmmoOnGun ? 1 : 0);
	return (nOn == 0) ? 0 : (nOn == 9) ? 1 : 2;
}

void CFolderVR::ApplyPhysicalPreset(int nPreset)
{
	UpdateData(LTTRUE);
	const LTBOOL b = (nPreset == 1) ? LTTRUE : LTFALSE;
	m_bHolsters = m_bManualReload = m_bSwingMelee = m_bWristHud = m_bGlassesToFace = m_bThrowByHand = m_bTwoHanded = b;
	m_bRackSlide = b;
	// Physical also crouches for real and shows the ammo on the gun; Buttons
	// leaves both off.
	m_bAmmoOnGun = b;
	m_bRealCrouch = b;
	m_nPhysPreset = (nPreset == 1) ? 1 : 0;
	UpdateData(LTFALSE);
	g_pInterfaceMgr->RequestInterfaceSound(IS_SELECT);
}

CFolderVR::~CFolderVR()
{
}

// ONE CLASS, SIX PAGES. Options > VR was a single page of twenty settings,
// and a long list of settings is hard to find anything in. So now there is a
// short first page with Recenter and a row per topic, then one
// page per topic. Each page is its own folder id, so the game's Back and its
// folder history work as on every other page; Build picks the rows by id.
// Every page reads and writes ALL the VR settings on focus (OnFocus below),
// so it does not matter which page a value was changed on.
namespace
{
	struct VRPageLink { const char* pLabel; eFolderID eId; };
	const VRPageLink kVRPages[] = {
		{ "Turning and Moving", FOLDER_ID_VR_MOVE     },
		{ "Your Body",          FOLDER_ID_VR_BODY     },
		{ "Hands and Weapons",  FOLDER_ID_VR_HANDS    },
		{ "Physical Play",      FOLDER_ID_VR_PHYSICAL },
		{ "Screen and Aim",     FOLDER_ID_VR_SCREEN   },
		{ "Controls",           FOLDER_ID_VR_CONTROLS },
	};
	const int kVRPageCount = sizeof(kVRPages) / sizeof(kVRPages[0]);
}

// A CONTROLS ROW: cycles through the buttons, skipping any another row
// already uses. One button, one job, so there is never a button that does two
// things without the player having asked for it. To swap two actions, set one
// to (none) first. (none) is never taken.
LTBOOL CVRBindCtrl::OnRight()
{
	return Step(+1);
}

LTBOOL CVRBindCtrl::OnLeft()
{
	return Step(-1);
}

LTBOOL CVRBindCtrl::Step(int nDir)
{
	const int nCount = GetNumStrings();
	if (nCount <= 1 || !m_ppRows) return LTFALSE;
	int n = GetSelIndex();
	for (int guard = 0; guard < nCount; ++guard)
	{
		n = (n + nDir + nCount) % nCount;
		bool bTaken = false;
		if (n != VRBinds::PAD_NONE)
			for (int r = 0; r < VRBinds::ACT_COUNT; ++r)
				if (r != m_nRow && m_ppRows[r] && m_ppRows[r]->GetSelIndex() == n) { bTaken = true; break; }
		if (!bTaken)
		{
			SetSelIndex(n);
			return LTTRUE;
		}
	}
	return LTFALSE;
}

void CFolderVR::AddOnOff(char* pLabel, LTBOOL* pb)
{
	CToggleCtrl* pToggle = AddToggle(pLabel, 0, kGap, pb);
	pToggle->SetOnString(IDS_ON);
	pToggle->SetOffString(IDS_OFF);
}

void CFolderVR::AddRange(char* pLabel, int* pn, int nMin, int nMax, int nStep)
{
	CSliderCtrl* pSlider = AddSlider(pLabel, 0, kGap, kWidth, pn);
	pSlider->SetSliderRange(nMin, nMax);
	pSlider->SetSliderIncrement(nStep);
}

LTBOOL CFolderVR::Build()
{
	const int nId = GetFolderID();
	const char* pTitle = "VR Options";
	for (int p = 0; p < kVRPageCount; ++p)
		if (kVRPages[p].eId == nId) pTitle = kVRPages[p].pLabel;
	if (nId == FOLDER_ID_VR_STYLE) pTitle = "How do you want to play?";
	CreateTitle((char*)pTitle);

	kGap = 0;
	kWidth = 0;
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

	// The value column clears the widest label actually drawn (as on every
	// other options page): "Steady observer view (restart)" ran into its ON.
	// Measured over every page's labels, so the column is the same on all of
	// them and a page does not jump sideways from the one before it.
	{
		static const char* const kLabels[] = {
			"Head bob", "Weapon sway", "Snap turning", "Turn speed (deg/s)", "Steer with both grips",
			"Leftorium (left-handed)", "Swap sticks", "Gun follows hand", "Gun size",
			"Hand movement", "Controller rumble", "Rumble strength %", "Gun forward / back", "Gun up / down", "Gun right / left", "Aim dot", "VR captions", "Aim dot size",
			"Show body (restart)", "See yourself in mirrors (restart)", "Crouch for real",
			"Resolution (restart)", "Menu size % (restart)", "Menu distance cm (restart)", "Steady observer view (restart)", "Match headset refresh rate (restart)", "Large menu text",
			"Use / activate", "Previous weapon", "Next ammo type",
			"Walk direction", "Analog walking",
			"Physical play", "Holsters on the body", "Holster height", "Manual reload",
			"Swing to hit", "Wrist display", "Glasses to your eyes", "Two hands on long guns", "Rack the slide", "Ammo on the gun", "Room-scale", "Wall fade",
			"Debug: god mode", "Debug: walk through walls", "Debug: all weapons+gear",
			"Debug: kill all enemies", "Debug: all missions", "Debug: show position",
			"Debug: skip level (R grip + B)" };
		kGap = LabelColumnText(kGap, kLabels, sizeof(kLabels) / sizeof(kLabels[0]));
	}

	switch (nId)
	{
	case FOLDER_ID_VR:
	{
		// RECENTER, AT THE TOP. It was both thumbsticks clicked together, which is
		// also Virtual Desktop's own overlay, and the VR host treated the same
		// chord as a debug trigger that saved three screenshots a time. Now: this
		// row, or hold the headset's Meta button (the runtime's own recenter).
		AddTextItem("Recenter", CMD_VR_RECENTER, 0);
		for (int p = 0; p < kVRPageCount; ++p)
			AddTextItem((char*)kVRPages[p].pLabel, CMD_VR_PAGE + p, 0);

		// VR_DEBUG_TOOLS, testing aids, not features. All off by default, all
		// removable together: delete this block, the members in FolderVR.h, the
		// two lines each in the read and write below, and the marked blocks in
		// GameClientShell.cpp and MessageMgr.h.
		// ONLY IN A BUILD THAT CAN ACT ON THEM. The code that applies these is
		// compiled out of a release (VR_DEBUG_TOOLS 0), and a menu row that does
		// nothing is a bug report waiting to be written, so a release does not
		// show them.
		if (VRDebugToolsBuilt())
		{
			AddOnOff("Debug: god mode",           &m_bDebugGod);
			AddOnOff("Debug: walk through walls", &m_bDebugClip);
			AddOnOff("Debug: all weapons+gear",   &m_bDebugArsenal);
			// KILLS, and cannot be undone within a level: the server's remove-AI
			// handler ignores its flag and detonates every AI it finds. Named for
			// what it does.
			AddOnOff("Debug: kill all enemies",   &m_bDebugNoAI);
			AddOnOff("Debug: all missions",       &m_bDebugMissions);
			// Where am I? So a bug report can carry coordinates and the desk harness
			// can stand in the same spot: look-shot.ps1 -At "x,y,z".
			AddOnOff("Debug: show position",      &m_bDebugPos);
			AddOnOff("Debug: skip level (R grip + B)", &m_bDebugSkip);
		}
		break;
	}

	case FOLDER_ID_VR_MOVE:
		// Smooth turning is the default in every port; snap is the
		// option for anyone it does not suit.
		AddOnOff("Snap turning", &m_bSnapTurn);
		// Degrees a second; 120 is the house default.
		AddRange("Turn speed (deg/s)", &m_nTurnRate, 30, 240, 15);
		// A left-handed player still moves with the left stick; this is for
		// anyone who wants move and turn the other way round.
		AddOnOff("Swap sticks", &m_bSwapSticks);
		// WHICH WAY IS FORWARD (VRWalk.h). Head is what an omnidirectional
		// treadmill needs, the player turns with their whole body and the
		// treadmill only ever says "forward", and what many standing players
		// prefer. Body is the game as it always was.
		{
			CCycleCtrl* pCycle = AddCycleItem("Walk direction", 0, kGap, 0, &m_nWalkDir);
			if (pCycle)
			{
				static const char* const kDir[] = { "Body", "Head", "Off hand" };
				for (int d = 0; d < 3; ++d)
				{
					HSTRING h = g_pLTClient->CreateString((char*)kDir[d]);
					pCycle->AddString(h);
					g_pLTClient->FreeString(h);
				}
			}
		}
		// How far the stick is pushed is the speed, and its angle the exact
		// direction, how a treadmill reports the player's pace.
		AddOnOff("Analog walking", &m_bAnalogWalk);
		// Snowmobile and motorcycle: hold both grips and the line between the
		// controllers is the handlebar. Off, the right stick steers as before.
		AddOnOff("Steer with both grips", &m_bSteerHands);
		break;

	case FOLDER_ID_VR_BODY:
		// The body is a renderer switch (StubBody) and takes effect on the next
		// launch; the label says so. The mirror too (StubMirrorBody).
		AddOnOff("Show body (restart)", &m_bShowBody);
		// Crouch down for real (about 30 cm below where your head was at the
		// recenter) and Cate crouches, so she fits where a crouch fits.
		AddOnOff("Crouch for real", &m_bRealCrouch);
		// Walk around the room and Cate walks with you; walls stop her.
		AddOnOff("Room-scale", &m_bRoomScale);
		// Your head inside a wall she could not walk through goes dark.
		AddOnOff("Wall fade", &m_bWallFade);
		AddOnOff("See yourself in mirrors (restart)", &m_bMirrorBody);
		// In VR the camera is the head: bobbing it moves the world under
		// someone who is not moving, which is the textbook way to make a
		// person ill. Asked for by name.
		AddRange("Head bob", &m_nHeadBob, 0, 10, 1);
		break;

	case FOLDER_ID_VR_HANDS:
		// THE LEFTORIUM (left-handed play), named for Ned Flanders' shop. The
		// hands swap: the gun and its trigger in the left hand, the torch in the
		// right. The sticks are their own row (Turning and Moving).
		AddOnOff("Leftorium (left-handed)", &m_bLeftorium);
		AddOnOff("Gun follows hand", &m_bGunAtHand);
		AddRange("Gun size", &m_nGunScale, 1, 20, 1);
		// TENTHS, like the aim dot. This slider stored WHOLE units and wrote them
		// back, so opening the page turned the 3.46 default into 3, and the
		// hand's travel came up short by 13% every time the page was opened.
		AddRange("Hand movement", &m_nHandTravel, 10, 120, 2);
		AddRange("Weapon sway", &m_nWeaponSway, 0, 10, 1);
		// One switch for both kinds of buzz, firing and being hit: VRHaptics
		// already gates both. VRHapticsDamage stays a console-only refinement.
		AddOnOff("Controller rumble", &m_bRumble);
		AddRange("Rumble strength %", &m_nRumbleStrength, 25, 300, 25);
		// Every gun moved in the hand, on top of its own tuned grip:
		// centimetres, -10 to +10 (the middle is 0).
		AddRange("Gun forward / back", &m_nGunPos[0], 0, 20, 1);
		AddRange("Gun up / down", &m_nGunPos[1], 0, 20, 1);
		AddRange("Gun right / left", &m_nGunPos[2], 0, 20, 1);
		// CATE'S OWN HANDS: fingers that follow the trigger, grip and touch, at
		// both controllers (VRHands.cpp). The gloves: ours, painted, or her
		// outfit's own when setup has made them from the game's files.
		AddOnOff("Cate's hands", &m_bCateHands);
		{
			CCycleCtrl* pCycle = AddCycleItem("Gloves", 0, kGap, 0, &m_nHandsOutfit);
			if (pCycle)
			{
				static const char* const kGlove[] = { "Painted", "Action", "Casual", "Scuba", "Undercover", "Winter", "Her outfit" };
				for (int d = 0; d < 7; ++d)
				{
					HSTRING h = g_pLTClient->CreateString((char*)kGlove[d]);
					pCycle->AddString(h);
					g_pLTClient->FreeString(h);
				}
			}
		}
		break;

	case FOLDER_ID_VR_SCREEN:
		AddOnOff("Aim dot", &m_bAimDot);
		// Tenths of a world unit: 10 is a real laser dot, about 17 mm across.
		AddRange("Aim dot size", &m_nAimDotSize, 3, 30, 1);
		AddOnOff("VR captions", &m_bVRCaptions);
		AddOnOff("Large menu text", &m_bBigMenuText);
		// PERCENT OF THE HEADSET'S OWN RESOLUTION. The game's screen size is fixed
		// when it starts, so the launcher applies this on the next launch
		// (play-vr.ps1 reads it from autoexec.cfg with the headset's recommended
		// size); the label says so. 100 is the headset's size; lower is lighter on
		// a weaker GPU.
		// A row that names its value (60% ... 125%): as a slider it showed no
		// number, and players asked what they had set.
		{
			CCycleCtrl* pRes = AddCycleItem("Resolution (restart)", 0, kGap, 0, &m_nResIdx);
			if (pRes)
			{
				for (int r = 60; r <= 125; r += 5)
				{
					char sz[8];
					sprintf(sz, "%d%%", r);
					HSTRING h = g_pLTClient->CreateString(sz);
					pRes->AddString(h);
					g_pLTClient->FreeString(h);
				}
			}
		}
		// THE MENU SCREEN'S SIZE AND DISTANCE are the player's. The host places
		// the panel at launch, so play-vr.ps1 reads these from autoexec.cfg.
		AddRange("Menu size % (restart)", &m_nMenuSize, 50, 150, 10);
		AddRange("Menu distance cm (restart)", &m_nMenuDist, 100, 300, 10);
		// THE DESKTOP VIEW for a stream or a recording: on, the monitor shows a
		// steadied, level cut-out of the right eye that follows the head's turns
		// but not its wobble; off, the eye as it is. The host sets it up at
		// launch, so the label says restart.
		AddOnOff("Steady observer view (restart)", &m_bSpectator);
		// The game at the headset's own rate (120, 144) instead of 90. Off:
		// the headset is asked down to 90 Hz.
		AddOnOff("Match headset refresh rate (restart)", &m_bMatchRefresh);
		// NO ANTI-ALIASING CONTROL HERE ON PURPOSE. The renderer's multisampling
		// is built and free but draws sky-coloured shards through alpha-tested
		// geometry, so it ships off; a menu is the last place to offer that. The
		// aliasing was addressed by raising the render resolution instead, which
		// needs no setting. See docs/PERFORMANCE.md.
		break;

	case FOLDER_ID_VR_PHYSICAL:
	{
		// THE GAME IN YOUR HANDS (VRPhysical.h). Every part is its own row and
		// every button still does what it did: the wheel and the D-pad still
		// change weapons, the reload button still reloads (with manual reload
		// it ejects), the trigger with the fists still chops.
		CVRPhysPresetCtrl* pP = debug_new(CVRPhysPresetCtrl);
		HSTRING hStr = g_pLTClient->CreateString("Physical play");
		const LTBOOL bOk = pP->Create(g_pLTClient, hStr, GetDefaultFont(), kGap, 0, &m_nPhysPreset, m_nAlignment);
		g_pLTClient->FreeString(hStr);
		if (!bOk) debug_delete(pP);
		else
		{
			static const char* const kPreset[] = { "Buttons (simple)", "Physical", "Custom" };
			for (int s = 0; s < 3; ++s)
			{
				HSTRING h = g_pLTClient->CreateString((char*)kPreset[s]);
				pP->AddString(h);
				g_pLTClient->FreeString(h);
			}
			pP->SetColor(m_hSelectedColor, m_hNonSelectedColor, m_hDisabledColor);
			pP->SetHelpID(0);
			pP->SetTransparentColor(m_hTransparentColor);
			pP->m_pFolder = this;
			AddFreeControl(pP);
		}
		// Draw by gripping where it is worn: gun-side hip, over the gun-side
		// shoulder, the chest, the belt. Grip at the slot of the gun in the
		// hand to put it away.
		AddOnOff("Holsters on the body", &m_bHolsters);
		// Centimetres, -15 to +15 (the middle is 0): all the holsters up or down.
		AddRange("Holster height", &m_nHolsterHeight, 0, 30, 1);
		// The reload button ejects; the off hand brings a clip from the pouch
		// on the off-side hip to the gun.
		AddOnOff("Manual reload", &m_bManualReload);
		// With manual reload: a gun fired empty needs its slide pulled by
		// the off hand after the new clip goes in.
		AddOnOff("Rack the slide", &m_bRackSlide);
		// A hand swung fast enough hits who it passes through.
		AddOnOff("Swing to hit", &m_bSwingMelee);
		// Health, armor and the clip on the off-hand wrist.
		AddOnOff("Wrist display", &m_bWristHud);
		// The clip and the rounds left, beside the gun. Not part of the
		// Physical/Buttons choice: it suits either.
		AddOnOff("Ammo on the gun", &m_bAmmoOnGun);
		// The sunglasses go on only while they are held to the eyes.
		AddOnOff("Glasses to your eyes", &m_bGlassesToFace);
		// The coin and the lipsticks: hold the trigger, swing, let go.
		AddOnOff("Throw by hand", &m_bThrowByHand);
		// The off hand on a long gun's front: the gun aims from hand to hand.
		AddOnOff("Two hands on long guns", &m_bTwoHanded);
		break;
	}

	case FOLDER_ID_VR_STYLE:
	{
		// PLAY STYLE, asked once before the first new game in VR (FolderSingle):
		// the Physical Play preset, chosen before it matters. Physical also
		// puts Cate's hands on, which the holsters and the hand reload are
		// built around. Short lines in the small font: the menu panel is narrow.
		AddTextItem("Physical", CMD_VR_STYLE_PHYS, 0);
		static const char* const kPhys[] = { "Draw from holsters on your body,",
			"reload by hand, swing to punch,", "a wrist display, glasses to your eyes." };
		for (int i = 0; i < 3; ++i)
		{
			CLTGUITextItemCtrl* pT = AddTextItem((char*)kPhys[i], 0, 0, LTFALSE, GetSmallFont());
			if (pT) pT->Enable(LTFALSE);
		}
		CLTGUITextItemCtrl* pGap = AddTextItem(" ", 0, 0, LTFALSE, GetSmallFont());
		if (pGap) pGap->Enable(LTFALSE);
		AddTextItem("Buttons", CMD_VR_STYLE_BTNS, 0);
		static const char* const kBtns[] = { "Everything on the controller buttons,", "the way the game was made." };
		for (int i = 0; i < 2; ++i)
		{
			CLTGUITextItemCtrl* pT = AddTextItem((char*)kBtns[i], 0, 0, LTFALSE, GetSmallFont());
			if (pT) pT->Enable(LTFALSE);
		}
		pGap = AddTextItem(" ", 0, 0, LTFALSE, GetSmallFont());
		if (pGap) pGap->Enable(LTFALSE);
		CLTGUITextItemCtrl* pNote = AddTextItem("Change it any time: VR Options > Physical Play.", 0, 0, LTFALSE, GetSmallFont());
		if (pNote) pNote->Enable(LTFALSE);
		break;
	}

	case FOLDER_ID_VR_CONTROLS:
	{
		// Which controllers this map is for. Not selectable; its text is set
		// in OnFocus, because the controller in the player's hands can change
		// between two visits to the page.
		m_pBindHeader = AddTextItem("Quest controllers", 0, 0);
		if (m_pBindHeader) m_pBindHeader->Enable(LTFALSE);
		for (int a = 0; a < VRBinds::ACT_COUNT; ++a)
		{
			CVRBindCtrl* pCtrl = debug_new(CVRBindCtrl);
			HSTRING hStr = g_pLTClient->CreateString((char*)VRBinds::ActLabel(a));
			const LTBOOL bOk = pCtrl->Create(g_pLTClient, hStr, GetDefaultFont(), kGap, 0, &m_nBind[a], m_nAlignment);
			g_pLTClient->FreeString(hStr);
			if (!bOk) { debug_delete(pCtrl); m_pBindRows[a] = LTNULL; continue; }
			pCtrl->SetColor(m_hSelectedColor, m_hNonSelectedColor, m_hDisabledColor);
			pCtrl->SetHelpID(0);
			pCtrl->SetTransparentColor(m_hTransparentColor);
			pCtrl->m_ppRows = m_pBindRows;
			pCtrl->m_nRow = a;
			m_pBindRows[a] = pCtrl;
			AddFreeControl(pCtrl);
		}
		AddTextItem("Reset to defaults", CMD_VR_BIND_RESET, 0);
		break;
	}
	}

	if (!CBaseFolder::Build()) return LTFALSE;

	UseBack(LTTRUE, LTTRUE);
	return LTTRUE;
}

// The strings a Controls row cycles through depend on the controller (the
// Frame has a D-pad and shoulders), so they are filled in on every visit.
void CFolderVR::FillBindRows()
{
	m_bBindFrame = VRBinds::Frame();
	if (m_pBindHeader)
	{
		HSTRING h = g_pLTClient->CreateString(m_bBindFrame
			? "Steam Frame controllers"
			: (m_bLeftorium ? "Quest controllers - left-handed, so X/Y and A/B swap sides"
							: "Quest controllers"));
		m_pBindHeader->RemoveAll();
		m_pBindHeader->AddString(h);
		g_pLTClient->FreeString(h);
	}
	const int nPads = VRBinds::PadCount(m_bBindFrame);
	for (int a = 0; a < VRBinds::ACT_COUNT; ++a)
	{
		CVRBindCtrl* pCtrl = m_pBindRows[a];
		if (!pCtrl) continue;
		pCtrl->RemoveAll();
		for (int p = 0; p < nPads; ++p)
		{
			HSTRING h = g_pLTClient->CreateString((char*)VRBinds::PadLabel(p, m_bBindFrame));
			pCtrl->AddString(h);
			g_pLTClient->FreeString(h);
		}
		m_nBind[a] = VRBinds::Get(a, m_bBindFrame);
		pCtrl->SetSelIndex(m_nBind[a]);
	}
}

void CFolderVR::OnFocus(LTBOOL bFocus)
{
	if (bFocus)
	{
		m_nHeadBob     = (int)(10.0f * CVarF("HeadBob", 1.0f));
		m_nWeaponSway  = (int)(10.0f * CVarF("WeaponSway", 1.0f));
		m_bSnapTurn    = (CVarF("VRSnapTurn", 0.0f) > 0.0f);
		m_nTurnRate    = (int)(CVarF("VRTurnSpeed", 120.0f) + 0.5f);
		m_bSteerHands  = (CVarF("VRVehicleSteer", 1.0f) > 0.0f);
		m_bLeftorium   = (CVarF("VRLeftorium", 0.0f) > 0.0f);
		m_bSwapSticks  = (CVarF("VRSwapSticks", 0.0f) > 0.0f);
		m_nWalkDir     = (int)(CVarF("VRWalkDir", 0.0f) + 0.5f);
		if (m_nWalkDir < 0 || m_nWalkDir > 2) m_nWalkDir = 0;
		m_bAnalogWalk  = (CVarF("VRAnalogWalk", 0.0f) > 0.0f);
		m_bGunAtHand   = (CVarF("VRGunAtHand", 1.0f) > 0.0f);
		m_bRumble      = (CVarF("VRHaptics", 1.0f) > 0.0f);
		m_bMatchRefresh = (CVarF("VRMatchRefresh", 0.0f) > 0.0f);
		m_nRumbleStrength = (int)(CVarF("VRHapticsStrength", 100.0f) + 0.5f);
		if (m_nRumbleStrength < 25)  m_nRumbleStrength = 25;
		if (m_nRumbleStrength > 300) m_nRumbleStrength = 300;
		{
			static const char* const kPos[3] = { "VRGunPosForward", "VRGunPosUp", "VRGunPosRight" };
			for (int a = 0; a < 3; ++a)
			{
				const float f = CVarF((char*)kPos[a], 0.0f);
				m_nGunPos[a] = (int)(f + (f < 0.0f ? -0.5f : 0.5f)) + 10;
				if (m_nGunPos[a] < 0)  m_nGunPos[a] = 0;
				if (m_nGunPos[a] > 20) m_nGunPos[a] = 20;
			}
		}
		m_bCateHands   = (CVarF("VRCateHands", 1.0f) > 0.0f);
		m_nHandsOutfit = (int)(CVarF("VRHandsOutfit", 6.0f) + 0.5f);
		if (m_nHandsOutfit < 0 || m_nHandsOutfit > 6) m_nHandsOutfit = 6;
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
		m_bShowBody    = (CVarF("VRShowBody", 1.0f) > 0.0f);
		m_bRealCrouch  = (CVarF("VRRealCrouch", 0.0f) > 0.0f);
		m_bMirrorBody  = (CVarF("StubMirrorBody", 1.0f) > 0.0f);
		m_nResolution  = (int)(CVarF("VRResolution", 100.0f) + 0.5f);
		m_nResolution  = ((m_nResolution + 2) / 5) * 5;
		if (m_nResolution < 60)  m_nResolution = 60;
		if (m_nResolution > 125) m_nResolution = 125;
		m_nResIdx      = (m_nResolution - 60) / 5;
		m_nMenuSize    = (int)(CVarF("VRMenuSize", 100.0f) + 0.5f);
		m_nMenuSize    = ((m_nMenuSize + 5) / 10) * 10;
		if (m_nMenuSize < 50)  m_nMenuSize = 50;
		if (m_nMenuSize > 150) m_nMenuSize = 150;
		m_nMenuDist    = (int)(CVarF("VRMenuDist", 170.0f) + 0.5f);
		m_nMenuDist    = ((m_nMenuDist + 5) / 10) * 10;
		if (m_nMenuDist < 100) m_nMenuDist = 100;
		if (m_nMenuDist > 300) m_nMenuDist = 300;
		m_bSpectator   = (CVarF("VRSpectator", 1.0f) > 0.0f);
		// PHYSICAL PLAY: every part is off by default.
		m_bHolsters      = (CVarF("VRHolsters", 0.0f) > 0.0f);
		m_nHolsterHeight = (int)(CVarF("VRHolsterHeight", 0.0f) + (CVarF("VRHolsterHeight", 0.0f) < 0.0f ? -0.5f : 0.5f)) + 15;
		if (m_nHolsterHeight < 0)  m_nHolsterHeight = 0;
		if (m_nHolsterHeight > 30) m_nHolsterHeight = 30;
		m_bManualReload  = (CVarF("VRManualReload", 0.0f) > 0.0f);
		m_bSwingMelee    = (CVarF("VRSwingMelee", 0.0f) > 0.0f);
		m_bWristHud      = (CVarF("VRWristHud", 0.0f) > 0.0f);
		m_bGlassesToFace = (CVarF("VRGlassesToFace", 0.0f) > 0.0f);
		m_bThrowByHand   = (CVarF("VRThrowByHand", 0.0f) > 0.0f);
		m_bTwoHanded     = (CVarF("VRTwoHanded", 0.0f) > 0.0f);
		m_bRackSlide     = (CVarF("VRRackSlide", 0.0f) > 0.0f);
		m_bRoomScale     = (CVarF("VRRoomScale", 1.0f) > 0.0f);
		m_bWallFade      = (CVarF("VRWallFade", 1.0f) > 0.0f);
		m_bAmmoOnGun     = (CVarF("VRAmmoOnGun", 0.0f) > 0.0f);
		m_nPhysPreset    = PhysicalPresetNow();

		// Clamp what came out of the config into the ranges the controls
		// offer. A slider handed a value outside its range draws its thumb
		// off the end of the track, which reads as a broken menu.
		if (m_nGunScale < 1)   m_nGunScale = 1;
		if (m_nGunScale > 20)  m_nGunScale = 20;
		if (m_nHandTravel < 10) m_nHandTravel = 10;
		if (m_nHandTravel > 120) m_nHandTravel = 120;
		if (m_nAimDotSize < 3) m_nAimDotSize = 3;
		if (m_nAimDotSize > 30) m_nAimDotSize = 30;
		if (m_nTurnRate < 30)  m_nTurnRate = 30;
		if (m_nTurnRate > 240) m_nTurnRate = 240;

		if (GetFolderID() == FOLDER_ID_VR_CONTROLS) FillBindRows();

		UpdateData(LTFALSE);
	}
	else
	{
		UpdateData();

		WriteConsoleFloat("HeadBob",         (LTFLOAT)m_nHeadBob / 10.0f);
		WriteConsoleFloat("WeaponSway",      (LTFLOAT)m_nWeaponSway / 10.0f);
		WriteConsoleInt  ("VRSnapTurn",      (int)m_bSnapTurn);
		// The keyboard's NormalTurnRate/FastTurnRate are left alone (flat play).
		WriteConsoleFloat("VRTurnSpeed",     (LTFLOAT)m_nTurnRate);
		WriteConsoleInt  ("VRVehicleSteer",  (int)m_bSteerHands);
		WriteConsoleInt  ("VRLeftorium",     (int)m_bLeftorium);
		WriteConsoleInt  ("VRSwapSticks",    (int)m_bSwapSticks);
		WriteConsoleInt  ("VRWalkDir",       m_nWalkDir);
		WriteConsoleInt  ("VRAnalogWalk",    (int)m_bAnalogWalk);
		WriteConsoleInt  ("VRGunAtHand",     (int)m_bGunAtHand);
		WriteConsoleInt  ("VRHaptics",       (int)m_bRumble);
		WriteConsoleInt  ("VRHapticsStrength", m_nRumbleStrength);
		WriteConsoleInt  ("VRMatchRefresh",  (int)m_bMatchRefresh);
		WriteConsoleFloat("VRGunPosForward", (LTFLOAT)(m_nGunPos[0] - 10));
		WriteConsoleFloat("VRGunPosUp",      (LTFLOAT)(m_nGunPos[1] - 10));
		WriteConsoleFloat("VRGunPosRight",   (LTFLOAT)(m_nGunPos[2] - 10));
		WriteConsoleInt  ("VRCateHands",     (int)m_bCateHands);
		WriteConsoleInt  ("VRHandsOutfit",   m_nHandsOutfit);
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
		WriteConsoleInt  ("VRShowBody",      (int)m_bShowBody);
		WriteConsoleInt  ("VRRealCrouch",    (int)m_bRealCrouch);
		WriteConsoleInt  ("StubMirrorBody",  (int)m_bMirrorBody);
		if (m_nResIdx < 0)  m_nResIdx = 0;
		if (m_nResIdx > 13) m_nResIdx = 13;
		m_nResolution = 60 + 5 * m_nResIdx;
		WriteConsoleInt  ("VRResolution",    m_nResolution);
		WriteConsoleInt  ("VRMenuSize",      m_nMenuSize);
		WriteConsoleInt  ("VRMenuDist",      m_nMenuDist);
		WriteConsoleInt  ("VRSpectator",     (int)m_bSpectator);
		WriteConsoleInt  ("VRHolsters",      (int)m_bHolsters);
		WriteConsoleFloat("VRHolsterHeight", (LTFLOAT)(m_nHolsterHeight - 15));
		WriteConsoleInt  ("VRManualReload",  (int)m_bManualReload);
		WriteConsoleInt  ("VRSwingMelee",    (int)m_bSwingMelee);
		WriteConsoleInt  ("VRWristHud",      (int)m_bWristHud);
		WriteConsoleInt  ("VRGlassesToFace", (int)m_bGlassesToFace);
		WriteConsoleInt  ("VRThrowByHand",   (int)m_bThrowByHand);
		WriteConsoleInt  ("VRTwoHanded",     (int)m_bTwoHanded);
		WriteConsoleInt  ("VRRackSlide",     (int)m_bRackSlide);
		WriteConsoleInt  ("VRRoomScale",     (int)m_bRoomScale);
		WriteConsoleInt  ("VRWallFade",      (int)m_bWallFade);
		WriteConsoleInt  ("VRAmmoOnGun",     (int)m_bAmmoOnGun);
		// The Controls page writes the map of the controller it showed.
		if (GetFolderID() == FOLDER_ID_VR_CONTROLS)
			for (int a = 0; a < VRBinds::ACT_COUNT; ++a)
				if (m_pBindRows[a]) VRBinds::Set(a, m_bBindFrame, m_nBind[a]);

		// The engine rewrites this file on exit anyway, but not until then -
		// and a player who sets head bob to zero and is killed by a crash
		// should not have to set it again.
		g_pLTClient->WriteConfigFile("autoexec.cfg");
	}
	CBaseFolder::OnFocus(bFocus);
}

uint32 CFolderVR::OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2)
{
	if (dwCommand == CMD_VR_STYLE_PHYS || dwCommand == CMD_VR_STYLE_BTNS)
	{
		// Set on the members: leaving this page (OnFocus) writes every VR
		// setting from them, so a cvar written here directly would be undone.
		const bool bPhys = (dwCommand == CMD_VR_STYLE_PHYS);
		ApplyPhysicalPreset(bPhys ? 1 : 0);
		if (bPhys) m_bCateHands = LTTRUE;
		WriteConsoleInt("VRPlayStyleAsked", 1);
		VRLog::Msg("VRPlayStyle: %s chosen before the first new game", bPhys ? "Physical" : "Buttons");
		g_pInterfaceMgr->SwitchToFolder(FOLDER_ID_DIFFICULTY);
		return 1;
	}
	if (dwCommand == CMD_VR_RECENTER)
	{
		// The same request the chord made; the host folds the head's yaw and
		// position into its reference space.
		VRShared::RequestRecenter();
		g_pInterfaceMgr->RequestInterfaceSound(IS_SELECT);
		return 1;
	}
	if (dwCommand >= CMD_VR_PAGE && dwCommand < CMD_VR_PAGE + (uint32)kVRPageCount)
	{
		m_pFolderMgr->SetCurrentFolder(kVRPages[dwCommand - CMD_VR_PAGE].eId);
		return 1;
	}
	if (dwCommand == CMD_VR_BIND_RESET)
	{
		VRBinds::ResetDefaults(m_bBindFrame);
		for (int a = 0; a < VRBinds::ACT_COUNT; ++a)
		{
			m_nBind[a] = VRBinds::Get(a, m_bBindFrame);
			if (m_pBindRows[a]) m_pBindRows[a]->SetSelIndex(m_nBind[a]);
		}
		g_pInterfaceMgr->RequestInterfaceSound(IS_SELECT);
		return 1;
	}
	return CBaseFolder::OnCommand(dwCommand, dwParam1, dwParam2);
}

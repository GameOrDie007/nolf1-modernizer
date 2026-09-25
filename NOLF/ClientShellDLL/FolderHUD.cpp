#include "stdafx.h"
#include "FolderHUD.h"
#include "FolderMgr.h"
#include "FolderCommands.h"
#include "ClientRes.h"

#include "GameClientShell.h"
#include "GameSettings.h"

extern VarTrack g_vtUIScale;
extern VarTrack g_vtUseGOTYMenu;
extern VarTrack g_vtShowFPS;
extern VarTrack g_vtLockFPS;
extern VarTrack g_vtLockCinematicAspectRatio;

namespace
{
	int kGap = 0;
	int kWidth = 0;
}

// CFolderInterface
// For those looking for it, HUD was renamed to Interface :)
CFolderHUD::CFolderHUD()
{
	m_fUIScale = 1.0f;
	m_nUIScale = 100;
	m_bUseGOTYMenu = LTFALSE;
	m_bLockFramerate = LTTRUE;
	m_bShowFramerate = LTFALSE;
	m_bRestrictAspectRatio = LTFALSE;
}

CFolderHUD::~CFolderHUD()
{
}

LTBOOL CFolderHUD::Build()
{
	CreateTitle(IDS_TITLE_HUD_OPTIONS);

	if (g_pLayoutMgr->HasCustomValue(FOLDER_ID_HUD, "ColumnWidth"))
		kGap = g_pLayoutMgr->GetFolderCustomInt(FOLDER_ID_HUD, "ColumnWidth");
	if (g_pLayoutMgr->HasCustomValue(FOLDER_ID_HUD, "SliderWidth"))
		kWidth = g_pLayoutMgr->GetFolderCustomInt(FOLDER_ID_HUD, "SliderWidth");

	LTFLOAT yr = g_pInterfaceResMgr->GetYRatio();
	kGap *= yr;
	// THE TOGGLES' LABEL COLUMN, SCALED. They passed a bare 225 while the
	// slider above passes 200*yr and the last row kGap*yr, so with the
	// screen-ratio font sheets (VRMenuFontScale, 4x at 2076 lines) every label
	// ran into its value - "Lock 60 FPS Framerate".
	// 300, not 225: at the desk (3x sheets) "Cinematic Aspect Ratio" alone
	// needs about 245 of the authored units, so every row shares 300 (330 pushed "Game of the Year Edition" to the view's edge).
	// The layout's ColumnWidth (kGap) is authored for the 640x480 font and is
	// too narrow for the scaled sheets, so the wider of the two wins.
	const int nToggleCol = (kGap > (int)(300 * yr)) ? kGap : (int)(300 * yr);


	CSliderCtrl* pSlider = AddSlider(IDS_UI_SCALE, IDS_HELP_UI_SCALE, nToggleCol, 200, &m_nUIScale);
	pSlider->SetSliderRange(50, 100);
	pSlider->SetSliderIncrement(10);
	pSlider->SetNumericDisplay(LTTRUE);

	CToggleCtrl* pToggle = AddToggle(IDS_LOCK_FRAMERATE, IDS_HELP_LOCK_FRAMERATE, nToggleCol, &m_bLockFramerate);
	pToggle->SetOnString(IDS_60_FPS);
	pToggle->SetOffString(IDS_UNLOCKED_FPS);

	pToggle = AddToggle(IDS_SHOW_FRAMERATE, IDS_HELP_SHOW_FRAMERATE, nToggleCol, &m_bShowFramerate);
	pToggle->SetOnString(IDS_ON);
	pToggle->SetOffString(IDS_OFF);

	pToggle = AddToggle(IDS_4X3_CINEMATICS, IDS_HELP_4X3_CINEMATICS, nToggleCol, &m_bRestrictAspectRatio);
	pToggle->SetOnString(IDS_LOCKED_ASPECT);
	pToggle->SetOffString(IDS_UNLOCKED_ASPECT);

	if (g_pVersionMgr->IsGOTY())
	{
		pToggle = AddToggle(IDS_USE_GOTY_MENU, IDS_HELP_USE_GOTY_MENU, nToggleCol, &m_bUseGOTYMenu);
		pToggle->SetOnString(IDS_GOTY_MENU);
		pToggle->SetOffString(IDS_NORMAL_MENU);
	}
	
	// Make sure to call the base class
	if (!CBaseFolder::Build()) return LTFALSE;

	UseBack(LTTRUE, LTTRUE);

	return LTTRUE;
}

void CFolderHUD::OnFocus(LTBOOL bFocus)
{
	// Enter
	if (bFocus) {

		m_fUIScale = g_vtUIScale.GetFloat(0.5f);
		m_nUIScale = m_fUIScale * 100;

		m_bUseGOTYMenu = g_vtUseGOTYMenu.GetFloat();

		m_bLockFramerate = g_vtLockFPS.GetFloat(1.0f);
		m_bShowFramerate = g_vtShowFPS.GetFloat();
		m_bRestrictAspectRatio = g_vtLockCinematicAspectRatio.GetFloat();

		UpdateData(LTFALSE);
		CBaseFolder::OnFocus(bFocus);
		return;
	}

	// Leave
	UpdateData(LTTRUE);

	m_fUIScale = (float)m_nUIScale / 100.0f;

	WriteConsoleFloat("UIScale", m_fUIScale);
	g_pInterfaceResMgr->SetUserScale(m_fUIScale);

	WriteConsoleInt("UseGotyMenu", m_bUseGOTYMenu);

	WriteConsoleInt("FramerateLock", m_bLockFramerate);
	WriteConsoleInt("ShowFramerate", m_bShowFramerate);
	WriteConsoleInt("RestrictCinematicsTo4x3", m_bRestrictAspectRatio);

	g_pLTClient->WriteConfigFile("autoexec.cfg");

	CBaseFolder::OnFocus(bFocus);
}

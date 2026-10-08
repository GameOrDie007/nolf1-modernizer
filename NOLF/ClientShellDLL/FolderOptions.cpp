// FolderOptions.cpp: implementation of the CFolderOptions class.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "FolderOptions.h"
#include "FolderMgr.h"
#include "FolderCommands.h"
#include "ClientRes.h"

#include "GameClientShell.h"
#include "VRShared.h"
extern CGameClientShell* g_pGameClientShell;

namespace
{
	const uint32 CMD_OPT_ADVANCED = FOLDER_CMD_CUSTOM + 1;
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFolderOptions::CFolderOptions()
{
}

CFolderOptions::~CFolderOptions()
{

}

// Build the folder
LTBOOL CFolderOptions::Build()
{

	CreateTitle(IDS_TITLE_OPTIONS);

	// The launcher pins VRStereo on the command line, so this is known the
	// first time the folder is built (as on the Display page).
	m_bVR = (VRShared::IsLive() || GetConsoleInt("VRStereo", 0) > 0);
	if (m_bVR)
		AddVRRows();
	else
	{
		AddTextItem(IDS_SCREEN,			FOLDER_CMD_DISPLAY,		IDS_HELP_DISPLAY);
		AddTextItem(IDS_SOUND,			FOLDER_CMD_AUDIO,		IDS_HELP_SOUND);
		AddTextItem(IDS_CONTROLS,		FOLDER_CMD_CONTROLS,	IDS_HELP_CONTROLS);
		AddTextItem(IDS_GAME_OPTIONS,	FOLDER_CMD_GAME,		IDS_HELP_GAME_OPTIONS);
		AddTextItem(IDS_PERFORMANCE,	FOLDER_CMD_PERFORMANCE,	IDS_HELP_PERFORMANCE);
		AddTextItem(IDS_HUD,			FOLDER_CMD_HUD,			IDS_HELP_HUD);
		AddTextItem(IDS_JUKEBOX,			FOLDER_CMD_JUKEBOX,		IDS_HELP_JUKEBOX);
	}

	// Make sure to call the base class
	if (! CBaseFolder::Build()) return LTFALSE;

	UseBack(LTTRUE,LTTRUE);
	return LTTRUE;
}

uint32 CFolderOptions::OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2)
{
	switch(dwCommand)
	{
	case FOLDER_CMD_DISPLAY:
		{
			m_pFolderMgr->SetCurrentFolder(FOLDER_ID_DISPLAY);
			break;
		}
	case FOLDER_CMD_AUDIO:
		{
			m_pFolderMgr->SetCurrentFolder(FOLDER_ID_AUDIO);
			break;
		}
	case FOLDER_CMD_GAME:
		{
			m_pFolderMgr->SetCurrentFolder(FOLDER_ID_GAME);
			break;
		}
	case FOLDER_CMD_PERFORMANCE:
		{
			m_pFolderMgr->SetCurrentFolder(FOLDER_ID_PERFORMANCE);
			break;
		}
	case FOLDER_CMD_CONTROLS:
		{
			m_pFolderMgr->SetCurrentFolder(FOLDER_ID_CONTROLS);
			break;
		}
	case FOLDER_CMD_HUD:
		{
			m_pFolderMgr->SetCurrentFolder(FOLDER_ID_HUD);
			break;
		}
	case FOLDER_CMD_JUKEBOX:
		{
			m_pFolderMgr->SetCurrentFolder(FOLDER_ID_JUKEBOX);
			break;
		}
	case FOLDER_CMD_VR:
		{
			m_pFolderMgr->SetCurrentFolder(FOLDER_ID_VR);
			break;
		}
	case CMD_OPT_ADVANCED:
		{
			// The rows change on the next frame (Render), not here: this
			// call comes from inside the row that is about to be replaced.
			WriteConsoleInt("VRShowAdvanced", GetConsoleInt("VRShowAdvanced", 0) > 0 ? 0 : 1);
			m_bRebuild = true;
			break;
		}
	default:
		return CBaseFolder::OnCommand(dwCommand,dwParam1,dwParam2);
	}
	return 1;
};


// Change in focus
void    CFolderOptions::OnFocus(LTBOOL bFocus)
{
	if (bFocus)
	{
        UpdateData(LTFALSE);
	}
	else
	{
		UpdateData();
	}
	CBaseFolder::OnFocus(bFocus);
}

// THE OPTIONS PAGE IN THE HEADSET. VR Options first, then the pages a player
// in a headset needs, and everything else behind "Show advanced options".
// The Display page only sets a flat screen (the headset sets the size), the
// Controls page is keyboard, mouse and joystick, and Interface and Jukebox
// are rarely wanted, so those four are the advanced ones. Flat play keeps
// the stock list. A literal string for VR Options: there is no IDS_ for it
// and adding one is a CRes.dll rebuild; helpID 0 leaves the help line blank.
void CFolderOptions::AddVRRows()
{
	const bool bAdvanced = GetConsoleInt("VRShowAdvanced", 0) > 0;
	AddTextItem("VR Options",		FOLDER_CMD_VR,			0);
	CLTGUITextItemCtrl* pGap = AddTextItem(" ", 0, 0);
	if (pGap) pGap->Enable(LTFALSE);
	AddTextItem(IDS_GAME_OPTIONS,	FOLDER_CMD_GAME,		IDS_HELP_GAME_OPTIONS);
	AddTextItem(IDS_PERFORMANCE,	FOLDER_CMD_PERFORMANCE,	IDS_HELP_PERFORMANCE);
	AddTextItem(IDS_SOUND,			FOLDER_CMD_AUDIO,		IDS_HELP_SOUND);
	pGap = AddTextItem(" ", 0, 0);
	if (pGap) pGap->Enable(LTFALSE);
	m_pAdvanced = AddTextItem(bAdvanced ? "Show advanced options: On" : "Show advanced options: Off",
		CMD_OPT_ADVANCED, 0);
	if (bAdvanced)
	{
		AddTextItem(IDS_SCREEN,			FOLDER_CMD_DISPLAY,		IDS_HELP_DISPLAY);
		AddTextItem(IDS_CONTROLS,		FOLDER_CMD_CONTROLS,	IDS_HELP_CONTROLS);
		AddTextItem(IDS_HUD,			FOLDER_CMD_HUD,			IDS_HELP_HUD);
		AddTextItem(IDS_JUKEBOX,		FOLDER_CMD_JUKEBOX,		IDS_HELP_JUKEBOX);
	}
}

// The same steps as leaving and entering the page, with the selection kept
// on the switch.
void CFolderOptions::RebuildVRRows()
{
	SetSelection(kNoSelection);
	m_pAdvanced = LTNULL;
	RemoveFree();
	m_nFirstDrawn = 0;
	m_nLastDrawn = -1;
	AddVRRows();
	CalculateLastDrawn();
	CheckArrows();
	if (m_pAdvanced) SetSelection(GetIndex(m_pAdvanced));
	ForceMouseUpdate();
	UpdateHelpText();
}

LTBOOL CFolderOptions::Render(HSURFACE hDestSurf)
{
	if (m_bRebuild)
	{
		m_bRebuild = false;
		if (m_bVR) RebuildVRRows();
	}
	return CBaseFolder::Render(hDestSurf);
}

// FolderOptions.h: interface for the CFolderOptions class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _FOLDER_OPTIONS_H_
#define _FOLDER_OPTIONS_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BaseFolder.h"

class CFolderOptions : public CBaseFolder
{
public:
	CFolderOptions();
	virtual ~CFolderOptions();

	// Build the folder
    LTBOOL   Build();

    void    OnFocus(LTBOOL bFocus);
    LTBOOL  Render(HSURFACE hDestSurf);

protected:
    uint32  OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2);

	// In VR: the short list, and the rest behind "Show advanced options".
	void    AddVRRows();
	void    RebuildVRRows();
	bool    m_bVR = false;
	bool    m_bRebuild = false;
	CLTGUITextItemCtrl* m_pAdvanced = LTNULL;

};

#endif // _FOLDER_OPTIONS_H_
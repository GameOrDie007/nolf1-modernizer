// InterfaceResMgr.cpp: implementation of the CInterfaceResMgr class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "VarTrack.h"
#include "VRShared.h"
#include "ClientRes.h"
#include "gameclientshell.h"
#include "InterfaceResMgr.h"
#include "VRLog.h"
#include "ClientButeMgr.h"
#include "SDL.h"
#include "ConsoleMgr.h"

CInterfaceResMgr*   g_pInterfaceResMgr = LTNULL;
extern SDL_Window* g_SDLWindow;
extern ConsoleMgr* g_pConsoleMgr;
extern VarTrack g_vtUIScale;
extern VarTrack g_vtVRMenuHDFont;

namespace
{
	char g_szFontName[128];
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CInterfaceResMgr::CInterfaceResMgr()
{
	g_pInterfaceResMgr = this;

	m_hSurfLoading	 = NULL;
	m_hSurfCursor = NULL;
	m_hTransColor = SETRGB(255,0,255);

    m_pTitleFont = LTNULL;
    m_pLargeFont = LTNULL;
	m_pLargeHDFont = LTNULL;
	m_bScaledFonts = LTFALSE;
	m_nFontScale   = 1;
    m_pMediumFont = LTNULL;
	m_pDlgFont[0] = m_pDlgFont[1] = LTNULL;
	m_nDlgFontScale = 0;
    m_pSmallFont = LTNULL;
    m_pHelpFont = LTNULL;

    m_pMsgForeFont = LTNULL;
    m_pHUDForeFont = LTNULL;
    m_pAirFont = LTNULL;
    m_pChooserFont = LTNULL;

	m_Offset.x =  -1;
	m_Offset.y =  -1;

	m_dwScreenWidth = -1;
	m_dwScreenHeight = -1;

	m_fXRatio = 1.0f;
	m_fYRatio = 1.0f;

}

CInterfaceResMgr::~CInterfaceResMgr()
{
    g_pInterfaceResMgr = LTNULL;
}

//////////////////////////////////////////////////////////////////////
// Function name	: CInterfaceResMgr::Init
// Description	    :
// Return type      : LTBOOL
// Argument         : ILTClient* pClientDE
// Argument         : CGameClientShell* pClientShell
//////////////////////////////////////////////////////////////////////
LTBOOL CInterfaceResMgr::Init(ILTClient* pClientDE, CGameClientShell* pClientShell)
{
	if (!pClientDE)
	{
        return LTFALSE;
	}

    HSTRING hString;

	// Set the English flag
    hString = g_pLTClient->FormatString(IDS_GAME_LANGUAGE);
    if (hString && _mbsicmp((const unsigned char*)"english", (const unsigned char*)g_pLTClient->GetStringData(hString)) != 0)
    {
        m_bEnglish=LTFALSE;
	}
	else
    {
        m_bEnglish=LTTRUE;
	}
    g_pLTClient->FreeString(hString);
    hString=LTNULL;

	// Load the virtual key codes for yes responses
    hString=g_pLTClient->FormatString(IDS_MENU_VKEY_YES);
	if (hString)
	{
        m_nYesVKeyCode=atoi(g_pLTClient->GetStringData(hString));
        g_pLTClient->FreeString(hString);
        hString=LTNULL;
	}

	// Load the virtual key codes for no responses
    hString=g_pLTClient->FormatString(IDS_MENU_VKEY_NO);
	if (hString)
	{
        m_nNoVKeyCode=atoi(g_pLTClient->GetStringData(hString));
        g_pLTClient->FreeString(hString);
        hString=LTNULL;
	}

	// Init the InterfaceSurfMgr class
    m_InterfaceSurfMgr.Init(g_pLTClient);

	// set resolution dependant variables
	ScreenDimsChanged();


	// Initialize the fonts
    if (!InitFonts())
	{
        return LTFALSE;
	}

	g_pConsoleMgr->Init();

    return LTTRUE;
}

//////////////////////////////////////////////////////////////////////
// Function name	: CInterfaceResMgr::Term
// Description	    :
// Return type		: void
//////////////////////////////////////////////////////////////////////

void CInterfaceResMgr::Term()
{
	Clean();

	// Terminate the InterfaceSurfMgr class
	m_InterfaceSurfMgr.Term();

	if (m_hSurfCursor)
	{
        g_pLTClient->DeleteSurface(m_hSurfCursor);
		m_hSurfCursor = NULL;
	}

	if ( m_pHelpFont )
	{
		m_pHelpFont->Term();
		debug_delete(m_pHelpFont);
        m_pHelpFont=LTNULL;
	}
	if ( m_pSmallFont )
	{
		m_pSmallFont->Term();
		debug_delete(m_pSmallFont);
        m_pSmallFont=LTNULL;
	}
	if ( m_pMediumFont )
	{
		m_pMediumFont->Term();
		debug_delete(m_pMediumFont);
        m_pMediumFont=LTNULL;
	}
	for (int d = 0; d < 2; ++d)
	{
		if (m_pDlgFont[d])
		{
			m_pDlgFont[d]->Term();
			debug_delete(m_pDlgFont[d]);
			m_pDlgFont[d] = LTNULL;
		}
	}
	m_nDlgFontScale = 0;
	if ( m_pLargeFont )
	{
		m_pLargeFont->Term();
		debug_delete(m_pLargeFont);
        m_pLargeFont=LTNULL;
	}
	if ( m_pLargeHDFont )
	{
		m_pLargeHDFont->Term();
		debug_delete(m_pLargeHDFont);
        m_pLargeHDFont=LTNULL;
	}
	if ( m_pTitleFont )
	{
		m_pTitleFont->Term();
		debug_delete(m_pTitleFont);
        m_pTitleFont=LTNULL;
	}
	if ( m_pMsgForeFont )
	{
		m_pMsgForeFont->Term();
		debug_delete(m_pMsgForeFont);
        m_pMsgForeFont=LTNULL;
	}
	if ( m_pHUDForeFont )
	{
		m_pHUDForeFont->Term();
		debug_delete(m_pHUDForeFont);
        m_pHUDForeFont=LTNULL;
	}
	if ( m_pAirFont )
	{
		m_pAirFont->Term();
		debug_delete(m_pAirFont);
        m_pAirFont=LTNULL;
	}
	if ( m_pChooserFont )
	{
		m_pChooserFont->Term();
		debug_delete(m_pChooserFont);
        m_pChooserFont=LTNULL;
	}

}

//////////////////////////////////////////////////////////////////////
// Function name	: CInterfaceResMgr::Setup
// Description	    :
// Return type      : LTBOOL
//////////////////////////////////////////////////////////////////////

LTBOOL CInterfaceResMgr::Setup()
{
	//preload common surfaces

    return LTTRUE;
}

//////////////////////////////////////////////////////////////////////
// Function name	: CInterfaceResMgr::Clean
// Description	    :
// Return type		: void
//////////////////////////////////////////////////////////////////////

void CInterfaceResMgr::Clean()
{
    if (g_pLTClient)
	{
		if (m_hSurfLoading)
		{
            g_pLTClient->DeleteSurface(m_hSurfLoading);
			m_hSurfLoading = NULL;
		}

		// free shared surfaces
		m_InterfaceSurfMgr.FreeAllSurfaces();


	}
}

//////////////////////////////////////////////////////////////////////
// Function name	: CInterfaceResMgr::DrawFolder
// Description	    :
// Return type		: void
//////////////////////////////////////////////////////////////////////
void CInterfaceResMgr::DrawFolder()
{
    _ASSERT(g_pLTClient);
    if (!g_pLTClient) return;

	if (m_Offset.x < 0)
		ScreenDimsChanged();

	// The screen surface
    HSURFACE hScreen = g_pLTClient->GetScreenSurface();

	// Render the current folder
	g_pInterfaceMgr->GetFolderMgr()->Render(hScreen);

	return;
}

void CInterfaceResMgr::DrawLoadScreen()
{
    _ASSERT(g_pLTClient);
    if (!g_pLTClient) return;

	// The screen surface
    HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	HSURFACE hLoad   = GetSurfaceLoading();
    uint32 dwScreenWidth=0;
    uint32 dwScreenHeight=0;
    uint32 dwLoadWidth=0;
    uint32 dwLoadHeight=0;

	_ASSERT(hLoad && hScreen);
	if (!hLoad || !hScreen) return;

	// Get the dims of the screen and the working surface
    g_pLTClient->GetSurfaceDims(hScreen, &dwScreenWidth, &dwScreenHeight);
    g_pLTClient->GetSurfaceDims(hLoad, &dwLoadWidth, &dwLoadHeight);

	// Center the image and blit it to the screen
	int nLeft=(dwScreenWidth/2)-(dwLoadWidth/2);
	int nTop=(dwScreenHeight/2)-(dwLoadHeight/2);
    g_pLTClient->DrawSurfaceToSurfaceTransparent(hScreen, hLoad, NULL, nLeft, nTop,m_hTransColor);


	return;
}

void CInterfaceResMgr::DrawFolderBars()
{
	HLTCOLOR hShadeColor = g_pLayoutMgr->GetShadeColor();
	HSURFACE hScreen = g_pLTClient->GetScreenSurface();

	int nXOffset = g_pInterfaceResMgr->GetXOffset();

	LTRect rect(0, 0, 0, 0);

	if (nXOffset > 0)
	{
		rect.left = 0;
		rect.right = nXOffset;
		rect.top = 0;
		rect.bottom = g_pInterfaceResMgr->GetScreenHeight();

		g_pOptimizedRenderer->FillRect(hScreen, &rect, hShadeColor);

		rect.left = g_pInterfaceResMgr->GetScreenWidth() - nXOffset;
		rect.right = g_pInterfaceResMgr->GetScreenWidth();
		rect.top = 0;
		rect.bottom = g_pInterfaceResMgr->GetScreenHeight();

		g_pOptimizedRenderer->FillRect(hScreen, &rect, hShadeColor);
	}
}

int CInterfaceResMgr::Get4x3Offset()
{
	/* 
		Simple formula to calculate the edges of a 4x3 resolution.
		We calculate the equivalent 4x3 resolution from our whatever resolution,
		and then subtract it from our current width. 
		That leaves us over with both sides of the edges, 
		so we then divide by 2 to get only one side. 
	*/
	return (GetScreenWidth() - (GetScreenHeight() * Get4x3Ratio())) / 2;
}

// THE HUD IN A HEADSET. The HUD's positions scale with the screen and its
// art scales by UIScale, which the tester keeps at 0.5; the 2D layer then fits the
// whole 2560-wide layout into one eye at about 0.42. The tester's 12 September
// screenshots and a desk capture showed the ammo counter about 12 px tall in
// a 1384-tall eye - a degree of arc - and pushed to the eye's far edge. So in
// VR the HUD's art is scaled up by VRHudScale on top of the player's setting (default
// 2), and PlayerStats pulls each element in from the corners by VRHudInset.
VarTrack g_vtVRHudScale;
LTFLOAT CInterfaceResMgr::GetUIScale()
{
	if (!g_vtVRHudScale.IsInitted())
		g_vtVRHudScale.Init(g_pLTClient, "VRHudScale", LTNULL, 1.5f);	// 2, then 3 on 24 Sep: at 3, pulled in by VRHudInsetX, it read as twice too big
	const float fVR = VRShared::IsLive() ? g_vtVRHudScale.GetFloat() : 1.0f;
	return m_fYRatio * g_vtUIScale.GetFloat() * (fVR > 0.0f ? fVR : 1.0f);
}

void CInterfaceResMgr::DrawMessage(CLTGUIFont* pFont, int nMessageId)
{
    _ASSERT(g_pLTClient && pFont);
    if (!g_pLTClient || !pFont) return;

	// The screen surface
    HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	HSURFACE hBlank  = CreateSurfaceBlank();
	uint32 dwScreenWidth=0;
    uint32 dwScreenHeight=0;
    uint32 dwWidth=0;
    uint32 dwHeight=0;

	_ASSERT(hBlank);
	if (!hBlank) return;

	// Get the string...
    HSTRING hStr = g_pLTClient->FormatString(nMessageId);

	_ASSERT(hStr);
	if (!hStr)
	{
        g_pLTClient->DeleteSurface(hBlank);
		return;
	}

	// Get the dims of the screen and the working surface
	g_pLTClient->GetSurfaceDims(hScreen, &dwScreenWidth, &dwScreenHeight);
    g_pLTClient->GetSurfaceDims(hBlank, &dwWidth, &dwHeight);


	// Center the image and blit it to the screen
	int nLeft=(dwScreenWidth/2)-(dwWidth/2);
	int nTop=(dwScreenHeight/2)-(dwHeight/2);
    g_pLTClient->DrawSurfaceToSurfaceTransparent(hScreen, hBlank, NULL, nLeft, nTop, m_hTransColor);

    g_pLTClient->DeleteSurface(hBlank);

	// Center the string on the surface...
    LTIntPt size = pFont->GetTextExtents(hStr);
	pFont->Draw(hStr, hScreen, dwScreenWidth/2, (dwScreenHeight - size.y)/2, LTF_JUSTIFY_CENTER,kWhite);
    g_pLTClient->FreeString(hStr);

	return;
}

//
// If we're in windowed mode, and the resolution is the same as our desktop,
// then borderless windowed mode it up!
//
void CInterfaceResMgr::HandleBorderlessWindowed()
{
	// Only do this in windowed mode!
	if (GetConsoleInt("windowed", 0) == 0) {
		SDL_SetWindowFullscreen(g_SDLWindow, SDL_WINDOW_FULLSCREEN);

		return;
	}

	uint32 dwScreenWidth = 0;
	uint32 dwScreenHeight = 0;
	SDL_DisplayMode dm;
	if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
		SDL_Log("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
	}

	g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &dwScreenWidth, &dwScreenHeight);

	// If res matches our desktop, borderless it!
	if (dm.w == dwScreenWidth && dm.h == dwScreenHeight)
	{
		SDL_SetWindowFullscreen(g_SDLWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
		return;
	}

	// Otherwise reset to windowed.
	SDL_SetWindowFullscreen(g_SDLWindow, 0);
}

LTBOOL CInterfaceResMgr::InitFonts()
{


	m_pSmallFont = debug_new(CLTGUIFont);
	m_pMediumFont = debug_new(CLTGUIFont);
	m_pLargeFont = debug_new(CLTGUIFont);
	m_pHelpFont = debug_new(CLTGUIFont);
	m_pTitleFont = debug_new(CLTGUIFont);
	m_pMsgForeFont = debug_new(CLTGUIFont);
	m_pHUDForeFont = debug_new(CLTGUIFont);
	m_pAirFont = debug_new(CLTGUIFont);
	m_pChooserFont = debug_new(CLTGUIFont);

	// HD Fonts
	m_pLargeHDFont = debug_new(CLTGUIFont);


	// THE MENU TEXT SCALES WITH THE SCREEN, when asked to.
	//
	// The menu's POSITIONS scale - every folder lays out through GetYRatio(),
	// screen height over 480 - but the English fonts are BITMAPS blitted 1:1,
	// so the glyphs do not. At 2160 tall the layout is 4.5x the authored size
	// and the largest glyph on offer is 52 px (the Modernizer's HD large
	// font), so a 4K menu is art that fills the frame and text that does not.
	// In a headset it is the same defect in degrees: the font is too small.
	//
	// The engine has always had a second way to make a font: a SYSTEM font
	// rendered at a requested size (LITHFONTCREATESTRUCT::szFontName/nHeight),
	// which is how every non-English NOLF draws its menus. Those sizes are
	// resolution-independent by construction, and control rects come from
	// the font's own metrics (CLTGUITextItemCtrl::CalculateSize), so hit
	// testing follows the glyphs with no further work.
	//
	// VRMenuScaleText 1 sends English down that path with the localised
	// sizes multiplied by the layout ratio. It is a switch and not the
	// default until it has been seen in a headset: it changes the typeface
	// from the authored bitmap to Arial, and the bitmap menu is something
	// headset testing has validated.
	const bool bScaleText = (GetConsoleInt("VRMenuScaleText", 0) != 0);

	// WHAT THE RETAIL RENDERER SAYS ABOUT ITS REFLECTION MAPS. d3d.ren
	// registers EnvMapEnable, EnvMapWorld, EnvMapPolyGrids, EnvPanSpeed and
	// EnvScale as console variables; ours does not. Under -Renderer d3d.ren
	// this line is the authored numbers, and the guesses stop.
	{
		static const char* const kEnv[] =
			{ "EnvMapEnable", "EnvMapWorld", "EnvMapPolyGrids", "EnvPanSpeed", "EnvScale" };
		char szLine[256] = "";
		for (int i = 0; i < 5; ++i)
		{
			HCONSOLEVAR hV = g_pLTClient->GetConsoleVar((char*)kEnv[i]);
			char szOne[64];
			if (hV) sprintf(szOne, "%s=%.4f ", kEnv[i], g_pLTClient->GetVarValueFloat(hV));
			else    sprintf(szOne, "%s=(none) ", kEnv[i]);
			strcat(szLine, szOne);
		}
		VRLog::Msg("RENDERER ENV VARS: %s", szLine);
	}

	// WHICH SHEET. tools/hdfont.py makes every menu font at 2x, 3x and 4x -
	// the strip upscaled with a real filter, the separator columns forced back
	// to black so the width calculation sees exactly what it saw in the
	// original (verified: 94 glyph runs before and after, all fifteen files).
	// The layout scales by GetYRatio(), screen height over 480, so the sheet
	// nearest that ratio keeps the authored proportion: at 1384 tall (a
	// headset eye) that is 3x, at 2160 it is 4x. The Modernizer's one HD sheet
	// was 1.86x, which is why the menu read as small in both.
	//
	// VRMenuFontScale: -1 (default) picks by the ratio; 0 forces the
	// originals; 2, 3 or 4 force that sheet. Anything the rez does not carry
	// falls back to the original, so a wrong number cannot lose the menu.
	m_bScaledFonts = LTFALSE;
	m_nFontScale   = GetConsoleInt("VRMenuFontScale", -1);
	if (m_nFontScale < 0) m_nFontScale = (int)(m_fYRatio + 0.5f);
	if (m_nFontScale < 1) m_nFontScale = 1;
	if (m_nFontScale > 4) m_nFontScale = 4;

	// Initialize the bitmap fonts if we are in english
	if (IsEnglish() && !bScaleText)
	{
        // ************* help font
		g_pLayoutMgr->GetHelpFont(g_szFontName,sizeof(g_szFontName));
		if (!SetupFontScaled(m_pHelpFont))
		{
			debug_delete(m_pHelpFont);
            m_pHelpFont=LTNULL;
            return LTFALSE;
		}
        // *********** small font
		g_pLayoutMgr->GetSmallFontBase(g_szFontName,sizeof(g_szFontName));
        if (!SetupFontScaled(m_pSmallFont))
		{
			debug_delete(m_pSmallFont);
            m_pSmallFont=LTNULL;
            return LTFALSE;
		}

        // *********** medium font
		g_pLayoutMgr->GetMediumFontBase(g_szFontName,sizeof(g_szFontName));
        if (!SetupFontScaled(m_pMediumFont))
		{
			debug_delete(m_pMediumFont);
            m_pMediumFont=LTNULL;
            return LTFALSE;
		}

        // *********** Large font
		g_pLayoutMgr->GetLargeFontBase(g_szFontName,sizeof(g_szFontName));
        if (!SetupFontScaled(m_pLargeFont))
		{
			debug_delete(m_pLargeFont);
            m_pLargeFont=LTNULL;
            return LTFALSE;
		}

		//g_pLayoutMgr->GetLargeFontBase(g_szFontName,sizeof(g_szFontName));
		LTStrCpy(g_szFontName, "interface\\fonts\\font_large_0_hd.pcx", sizeof(g_szFontName));
		// With a scaled large sheet loaded the 1.86x HD one is both redundant
		// and SMALLER, and GetLargeFont() would still prefer it. Leave it
		// null so GetLargeFont falls through to the scaled font.
		if (m_bScaledFonts)
		{
			debug_delete(m_pLargeHDFont);
			m_pLargeHDFont = LTNULL;
		}
		else if (!SetupFont(m_pLargeHDFont))
		{
			debug_delete(m_pLargeHDFont);
            m_pLargeHDFont=LTNULL;
            return LTFALSE;
		}

        // ************* Title font
		g_pLayoutMgr->GetTitleFont(g_szFontName,sizeof(g_szFontName));
		if (!SetupFontScaled(m_pTitleFont))
		{
			if (!SetupFontScaled(m_pTitleFont,LTFALSE))
			{
				debug_delete(m_pTitleFont);
				m_pTitleFont=LTNULL;
				return LTFALSE;
			}
		}


        // ************* Foreground HUD (i.e. white) HUD font
		g_pLayoutMgr->GetMsgForeFont(g_szFontName,sizeof(g_szFontName));
        if (!SetupFont(m_pMsgForeFont))
		{
			debug_delete(m_pMsgForeFont);
            m_pMsgForeFont=LTNULL;
            return LTFALSE;
		}


        uint32 dwFlags = LTF_INCLUDE_SYMBOLS_1 | LTF_INCLUDE_NUMBERS | LTF_INCLUDE_SYMBOLS_2;
        // ************* Foreground HUD (i.e. white) HUD font
		g_pLayoutMgr->GetHUDForeFont(g_szFontName,sizeof(g_szFontName));
        if (!SetupFont(m_pHUDForeFont,LTTRUE,dwFlags))
		{
			debug_delete(m_pHUDForeFont);
            m_pHUDForeFont=LTNULL;
            return LTFALSE;
		}

        // ************* Air font
		g_pLayoutMgr->GetAirFont(g_szFontName,sizeof(g_szFontName));
        if (!SetupFont(m_pAirFont,LTTRUE,dwFlags)) // This is actually not used?
		{
			debug_delete(m_pAirFont);
            m_pAirFont=LTNULL;
            return LTFALSE;
		}

        // ************* Weapon Chooser font
		g_pLayoutMgr->GetChooserFont(g_szFontName,sizeof(g_szFontName));
		if (!SetupFont(m_pChooserFont))
		{
			debug_delete(m_pChooserFont);
            m_pChooserFont=LTNULL;
            return LTFALSE;
		}
	}
	else
	{
		// Localised builds: the sizes as authored. Scaled English: the same
		// sizes times the layout ratio, using HEIGHT for both axes so a
		// widescreen mode does not stretch the glyphs (at 3840x2160 the X
		// ratio is 6.0 and the Y ratio 4.5).
		const float fScale = bScaleText ? m_fYRatio : 1.0f;
		if (bScaleText)
			VRLog::Msg("VRMenuScaleText: engine fonts at %.2fx (screen %ux%u)",
								fScale, m_dwScreenWidth, m_dwScreenHeight);

        // TODO: put these into string table for localization
		// Initialize the engine fonts for non-english resource files
		if (!InitEngineFontScaled(m_pSmallFont, IDS_SMALL_FONT_NAME, IDS_SMALL_FONT_WIDTH, IDS_SMALL_FONT_HEIGHT, LTFALSE, fScale))
            return LTFALSE;
		if (!InitEngineFontScaled(m_pHelpFont, IDS_SMALL_FONT_NAME, IDS_SMALL_FONT_WIDTH, IDS_SMALL_FONT_HEIGHT, LTFALSE, fScale))
            return LTFALSE;
		if (!InitEngineFontScaled(m_pMediumFont, IDS_MEDIUM_FONT_NAME, IDS_MEDIUM_FONT_WIDTH, IDS_MEDIUM_FONT_HEIGHT, LTTRUE, fScale))
            return LTFALSE;
		if (!InitEngineFontScaled(m_pLargeFont, IDS_LARGE_FONT_NAME, IDS_LARGE_FONT_WIDTH, IDS_LARGE_FONT_HEIGHT, LTTRUE, fScale))
            return LTFALSE;
		if (!InitEngineFontScaled(m_pTitleFont, IDS_TITLE_FONT_NAME, IDS_TITLE_FONT_WIDTH, IDS_TITLE_FONT_HEIGHT, LTTRUE, fScale))
            return LTFALSE;
		if (!InitEngineFontScaled(m_pMsgForeFont, IDS_MEDIUM_FONT_NAME, IDS_MEDIUM_FONT_WIDTH, IDS_MEDIUM_FONT_HEIGHT, LTTRUE, fScale))
            return LTFALSE;
		if (!InitEngineFontScaled(m_pHUDForeFont, IDS_LARGE_FONT_NAME, IDS_LARGE_FONT_WIDTH, IDS_LARGE_FONT_HEIGHT, LTTRUE, fScale))
            return LTFALSE;
		if (!InitEngineFontScaled(m_pAirFont, IDS_LARGE_FONT_NAME, IDS_LARGE_FONT_WIDTH, IDS_LARGE_FONT_HEIGHT, LTTRUE, fScale))
            return LTFALSE;
		if (!InitEngineFontScaled(m_pChooserFont, IDS_SMALL_FONT_NAME, IDS_SMALL_FONT_WIDTH, IDS_SMALL_FONT_HEIGHT, LTFALSE, fScale))
            return LTFALSE;
		// The HD large font only ever existed on the bitmap path, and
		// GetLargeFont() hands it out under VRMenuHDFont. It has to be a
		// valid font here too, and "large at scale" is what it means now.
		if (!InitEngineFontScaled(m_pLargeHDFont, IDS_LARGE_FONT_NAME, IDS_LARGE_FONT_WIDTH, IDS_LARGE_FONT_HEIGHT, LTTRUE, fScale))
            return LTFALSE;

	}

    // TODO: reimplement this stuff
	// Set the wrapping method
//  HSTRING hString=g_pLTClient->FormatString(IDS_FONT_WRAP_USE_SPACES);
//  if (_mbsicmp((const unsigned char*)g_pLTClient->GetStringData(hString), (const unsigned char*)"1") == 0)
//	{
        CLTGUIFont::SetWrapMethod(LTTRUE);
//	}
//	else
//	{
//      CLTGUIFont::SetWrapMethod(LTFALSE);
//	}

    return LTTRUE;

}

// *******************************************************************

// The resource sizes times fScale, never below one pixel. Everything else is
// InitEngineFont's. See the note in InitFonts for why this exists.
LTBOOL CInterfaceResMgr::InitEngineFontScaled(CLTGUIFont *pFont, int nNameID, int nWidthID, int nHeightID, LTBOOL bBold, float fScale)
{
	if (!pFont) return LTFALSE;
    HSTRING hName   = g_pLTClient->FormatString(nNameID);
    HSTRING hWidth  = g_pLTClient->FormatString(nWidthID);
    HSTRING hHeight = g_pLTClient->FormatString(nHeightID);
	char szFontName[256] = "";
    strncpy(szFontName, g_pLTClient->GetStringData(hName), sizeof(szFontName) - 1);
	int nW = (int)(atoi(g_pLTClient->GetStringData(hWidth))  * fScale + 0.5f);
	int nH = (int)(atoi(g_pLTClient->GetStringData(hHeight)) * fScale + 0.5f);
	if (nW < 1) nW = 1;
	if (nH < 1) nH = 1;
    g_pLTClient->FreeString(hName);
    g_pLTClient->FreeString(hWidth);
    g_pLTClient->FreeString(hHeight);
	return InitEngineFont(pFont, szFontName, nW, nH, bBold);
}

// Initialize an engine font from string IDs that represent the name, width, and height
LTBOOL CInterfaceResMgr::InitEngineFont(CLTGUIFont *pFont, int nNameID, int nWidthID, int nHeightID, LTBOOL bBold)
{
	if (!pFont)
	{
        return LTFALSE;
	}

	// Get the font name, width, and height
    HSTRING hName=g_pLTClient->FormatString(nNameID);
    HSTRING hWidth=g_pLTClient->FormatString(nWidthID);
    HSTRING hHeight=g_pLTClient->FormatString(nHeightID);

	LITHFONTCREATESTRUCT lfCS;
	char szFontName[256] = "";
    strncpy(szFontName,g_pLTClient->GetStringData(hName),sizeof(szFontName));

	lfCS.szFontName	= szFontName;
    lfCS.nWidth     = atoi(g_pLTClient->GetStringData(hWidth));
    lfCS.nHeight    = atoi(g_pLTClient->GetStringData(hHeight));
    lfCS.bItalic    = LTFALSE;
    lfCS.bBold      = bBold;
    lfCS.bUnderline = LTFALSE;

	// Initialize the font
    LTBOOL bResult;

    bResult=pFont->Init(g_pLTClient, &lfCS);
	

	if (!bResult)
	{
		char szString[1024];
        sprintf(szString, "Cannot initialize font: %s", g_pLTClient->GetStringData(hName));
        g_pLTClient->CPrint(szString);
		SDL_Log(szString);
	} else {
		SDL_Log("Initialized Font %s",lfCS.szFontName);
	}


	// Free the strings
    g_pLTClient->FreeString(hName);
    g_pLTClient->FreeString(hWidth);
    g_pLTClient->FreeString(hHeight);

	return bResult;
}

LTBOOL CInterfaceResMgr::InitEngineFont(CLTGUIFont *pFont, char *lpszName, int nWidth, int nHeight, LTBOOL bBold)
{
	if (!pFont)
	{
        return LTFALSE;
	}

	LITHFONTCREATESTRUCT lfCS;
	char szFontName[256] = "";
	strncpy(szFontName,lpszName,sizeof(szFontName));

	lfCS.szFontName	= szFontName;
	lfCS.nWidth		= nWidth;
	lfCS.nHeight	= nHeight;
    lfCS.bItalic    = LTFALSE;
    lfCS.bBold      = bBold;
    lfCS.bUnderline = LTFALSE;

	// Initialize the font
    LTBOOL bResult;
    bResult=pFont->Init(g_pLTClient, &lfCS);

	if (!bResult)
	{
        g_pLTClient->CPrint("Cannot initialize font: %s", lpszName);
	}


	return bResult;
}

#define USABLE_HEIGHT_I 480
#define USABLE_HEIGHT_F 480.0f

void CInterfaceResMgr::ScreenDimsChanged()
{
    if (!g_pLTClient) return;

	RMode currentMode;
    g_pLTClient->GetRenderMode(&currentMode);

	m_Offset.x = (int)(currentMode.m_Width - 640) / 2;
	m_Offset.y = (int)(currentMode.m_Height - USABLE_HEIGHT_I) / 2;

	m_fXRatio = (float)currentMode.m_Width / 640.0f;
	m_fYRatio = (float)currentMode.m_Height / USABLE_HEIGHT_F;

	m_dwScreenWidth = currentMode.m_Width;
	m_dwScreenHeight = currentMode.m_Height;

	HandleBorderlessWindowed();

	// Re-init the console
	g_pConsoleMgr->Init();
}


HSURFACE CInterfaceResMgr::GetSurfaceLoading()
{
	// The loading image
	if (!m_hSurfLoading)
        m_hSurfLoading = g_pLTClient->CreateSurfaceFromBitmap("menu\\art\\loading.pcx");
	_ASSERT(m_hSurfLoading);
	return m_hSurfLoading;
};

HSURFACE CInterfaceResMgr::CreateSurfaceBlank()
{
    return g_pLTClient->CreateSurfaceFromBitmap("menu\\art\\blanktag.pcx");
};


HSURFACE CInterfaceResMgr::GetSurfaceCursor()
{
	if (!m_hSurfCursor)
        m_hSurfCursor = g_pLTClient->CreateSurfaceFromBitmap("interface\\cursor0.pcx");
	_ASSERT(m_hSurfCursor);
	return m_hSurfCursor;
};


const char *CInterfaceResMgr::GetSoundSelect()
{
	if (m_csSoundSelect.IsEmpty())
	{
		m_csSoundSelect = g_pClientButeMgr->GetInterfaceAttributeString("SelectSound");
	}
	_ASSERT(!m_csSoundSelect.IsEmpty());
	return m_csSoundSelect;
};

const char *CInterfaceResMgr::GetSoundUnselectable()
{
	if (m_csSoundUnselectable.IsEmpty())
	{
		m_csSoundUnselectable = g_pClientButeMgr->GetInterfaceAttributeString("UnselectableSound");
	}
	_ASSERT(!m_csSoundUnselectable.IsEmpty());
	return m_csSoundUnselectable;
};

const char *CInterfaceResMgr::GetSoundChange()
{
	if (m_csSoundChange.IsEmpty())
	{
		m_csSoundChange = g_pClientButeMgr->GetInterfaceAttributeString("SelectChangeSound");
	}
	_ASSERT(!m_csSoundChange.IsEmpty());
	return m_csSoundChange;
};

const char *CInterfaceResMgr::GetSoundPageChange()
{
	if (m_csSoundPageChange.IsEmpty())
	{
		m_csSoundPageChange = g_pClientButeMgr->GetInterfaceAttributeString("PageChangeSound");
	}
	_ASSERT(!m_csSoundPageChange.IsEmpty());
	return m_csSoundPageChange;
};

const char *CInterfaceResMgr::GetSoundArrowUp()
{
	if (m_csSoundArrowUp.IsEmpty())
	{
		m_csSoundArrowUp = g_pClientButeMgr->GetInterfaceAttributeString("ArrowUpSound");
	}
	_ASSERT(!m_csSoundArrowUp.IsEmpty());
	return m_csSoundArrowUp;
};

const char *CInterfaceResMgr::GetSoundArrowDown()
{
	if (m_csSoundArrowDown.IsEmpty())
	{
		m_csSoundArrowDown = g_pClientButeMgr->GetInterfaceAttributeString("ArrowDownSound");
	}
	_ASSERT(!m_csSoundArrowDown.IsEmpty());
	return m_csSoundArrowDown;
};

const char *CInterfaceResMgr::GetSoundArrowLeft()
{
	if (m_csSoundArrowLeft.IsEmpty())
	{
		m_csSoundArrowLeft = g_pClientButeMgr->GetInterfaceAttributeString("ArrowLeftSound");
	}
	_ASSERT(!m_csSoundArrowLeft.IsEmpty());
	return m_csSoundArrowLeft;
};

const char *CInterfaceResMgr::GetSoundArrowRight()
{
	if (m_csSoundArrowRight.IsEmpty())
	{
		m_csSoundArrowRight = g_pClientButeMgr->GetInterfaceAttributeString("ArrowRightSound");
	}
	_ASSERT(!m_csSoundArrowRight.IsEmpty());
	return m_csSoundArrowRight;
};



const char *CInterfaceResMgr::GetObjectiveAddedSound()
{
	if (m_csSoundObjAdd.IsEmpty())
	{
		m_csSoundObjAdd = g_pClientButeMgr->GetInterfaceAttributeString("ObjAddSound");
	}
	_ASSERT(!m_csSoundObjAdd.IsEmpty());
	return m_csSoundObjAdd;
};

const char *CInterfaceResMgr::GetObjectiveRemovedSound()
{
	if (m_csSoundObjRemove.IsEmpty())
	{
		m_csSoundObjRemove = g_pClientButeMgr->GetInterfaceAttributeString("ObjRemoveSound");
	}
	_ASSERT(!m_csSoundObjRemove.IsEmpty());
	return m_csSoundObjRemove;
};

const char *CInterfaceResMgr::GetObjectiveCompletedSound()
{
	if (m_csSoundObjComplete.IsEmpty())
	{
		m_csSoundObjComplete = g_pClientButeMgr->GetInterfaceAttributeString("ObjCompleteSound");
	}
	_ASSERT(!m_csSoundObjComplete.IsEmpty());
	return m_csSoundObjComplete;
};


// Creates a surface just large enough for the string.
// You can make the surface a little larger with extraPixelsX and extraPixelsY.
HSURFACE CInterfaceResMgr::CreateSurfaceFromString(CLTGUIFont *pFont, HSTRING hString, HLTCOLOR hBackColor,
												int extraPixelsX, int extraPixelsY, int nWidth)
{
    LTIntPt sz;
	if (nWidth > 0)
		sz = pFont->GetTextExtentsFormat(hString,nWidth);
	else
		sz = pFont->GetTextExtents(hString);

    LTRect rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = sz.x+extraPixelsX;
	rect.bottom = sz.y+extraPixelsY;

    HSURFACE hSurf  = g_pLTClient->CreateSurface(sz.x+extraPixelsX,sz.y+extraPixelsY);
    g_pOptimizedRenderer->FillRect(hSurf,&rect,hBackColor);
	if (nWidth > 0)
		pFont->DrawFormat(hString, hSurf,extraPixelsX/2,extraPixelsY/2,nWidth,kWhite);
	else
		pFont->Draw(hString, hSurf,extraPixelsX/2,extraPixelsY/2,LTF_JUSTIFY_LEFT,kWhite);

	return hSurf;

}

HSURFACE CInterfaceResMgr::CreateSurfaceFromString(CLTGUIFont *pFont, int nStringId, HLTCOLOR hBackColor,
												int extraPixelsX, int extraPixelsY, int nWidth)
{
    HSTRING hString = g_pLTClient->FormatString(nStringId);
	return CreateSurfaceFromString(pFont, hString, hBackColor, extraPixelsX, extraPixelsY, nWidth);
    g_pLTClient->FreeString(hString);
}

HSURFACE CInterfaceResMgr::CreateSurfaceFromString(CLTGUIFont *pFont, char *lpszString, HLTCOLOR hBackColor,
												int extraPixelsX, int extraPixelsY, int nWidth)
{
    HSTRING hString = g_pLTClient->CreateString(lpszString);
	return CreateSurfaceFromString(pFont, hString, hBackColor, extraPixelsX, extraPixelsY, nWidth);
    g_pLTClient->FreeString(hString);
}



// Try "<name>_<N>x.pcx" first, then the name as given. Sets m_bScaledFonts
// the first time a scaled sheet loads. See the note in InitFonts.
LTBOOL CInterfaceResMgr::SetupFontScaled(CLTGUIFont *pFont, LTBOOL bBlend, uint32 dwFlags)
{
	if (m_nFontScale > 1)
	{
		char szOrig[256];
		LTStrCpy(szOrig, g_szFontName, sizeof(szOrig));
		char* pDot = strrchr(g_szFontName, '.');
		if (pDot && (size_t)(pDot - g_szFontName) + 4 < sizeof(g_szFontName) - 8)
		{
			char szTail[16];
			LTStrCpy(szTail, pDot, sizeof(szTail));				// ".pcx"
			sprintf(pDot, "_%dx%s", m_nFontScale, szTail);
			if (SetupFont(pFont, bBlend, dwFlags))
			{
				if (!m_bScaledFonts)
					VRLog::Msg("VRMenuFontScale: %dx sheets in use (screen %ux%u, ratio %.2f)",
										m_nFontScale, m_dwScreenWidth, m_dwScreenHeight, m_fYRatio);
				m_bScaledFonts = LTTRUE;
				return LTTRUE;
			}
			VRLog::Msg("VRMenuFontScale: no %s - using the original", g_szFontName);
			LTStrCpy(g_szFontName, szOrig, sizeof(g_szFontName));
		}
	}
	return SetupFont(pFont, bBlend, dwFlags);
}

LTBOOL CInterfaceResMgr::SetupFont(CLTGUIFont *pFont, LTBOOL bBlend, uint32 dwFlags)
{

	LITHFONTCREATESTRUCT lithFont;
	lithFont.szFontBitmap = g_szFontName;
	lithFont.nGroupFlags = dwFlags;
	if (bBlend)
	{
		lithFont.bChromaKey = LTFALSE;
		lithFont.hTransColor = kBlack;
	}
	else
	{
		lithFont.bChromaKey = LTTRUE;
		lithFont.hTransColor = SETRGB(255,0,255);
	}

    if ( !pFont->Init(g_pLTClient, &lithFont) )
	{
		char szString[512];
		sprintf(szString, "Cannot load font: %s", lithFont.szFontBitmap);
        g_pLTClient->CPrint(szString);
		SDL_Log(szString);
        return LTFALSE;
	}

	
	SDL_Log("Initialized %s",lithFont.szFontBitmap);

    return LTTRUE;
}

// THE MESSAGE FONT IS THE LARGE HD SHEET IN VR. Subtitles, the mission
// captions ("time and place" in the intro), the dialogue window and pickup
// messages all draw with this, at its authored 640x480 size, unscaled - a few
// pixels tall in a 2076-line eye. In the headset the boxes were there but
// far too small to read. Under VRMenuBigSubs they take the sheet the menus
// use. VRMenuBigSubs 0 restores the original.
CLTGUIFont* CInterfaceResMgr::GetMsgForeFont()
{
	extern VarTrack g_vtVRMenuBigSubs;
	// GetFloat(1.0f): the captions and subtitles are INITIALISED before the
	// VR cvars exist, and an uninitialised VarTrack answers with the default
	// it is handed - 0 sized their surfaces for the small font.
	if (g_vtVRMenuBigSubs.GetFloat(1.0f) > 0.0f)
	{
		CLTGUIFont* pBig = GetLargeFont();
		if (pBig) return pBig;
	}
	return m_pMsgForeFont;
}

// THE DIALOGUE BOXES A SHEET SMALLER. The question and choice boxes were right
// in shape but about a quarter too large in the headset.
// Scaling their layout alone would re-wrap the text, so the boxes
// take their own copies of the medium and large faces at VRDialogueSize of the
// menu's sheet (default 0.75: 4x -> 3x at a 2076-line screen) and the layout
// scales by the same ratio (GetDialogueSizeRatio). Menus keep their fonts.
// Outside VR, without scaled sheets, or at 1, these are the ordinary fonts.
static int VRDialogueFontScale(int nMenuScale)
{
	const float f = GetConsoleFloat("VRDialogueSize", 0.75f);
	// "In VR" is the launch's VRStereo as well as a live host: the dialogue
	// window's size is fixed once, at interface start, possibly before the
	// host's first pose arrives, and must agree with the font chosen later.
	const bool bVR = VRShared::IsLive() || GetConsoleInt("VRStereo", 0) > 0;
	if (!bVR || f <= 0.0f || f >= 1.0f || nMenuScale <= 1) return nMenuScale;
	int n = (int)((float)nMenuScale * f + 0.5f);
	if (n < 2) n = 2;
	return (n < nMenuScale) ? n : nMenuScale;
}
float CInterfaceResMgr::GetDialogueSizeRatio()
{
	if (!m_bScaledFonts || m_nFontScale <= 1) return 1.0f;
	GetDialogueFont(LTFALSE);	// settles m_nDlgFontScale (it can fall back to the menu's)
	const int n = m_nDlgFontScale ? m_nDlgFontScale : m_nFontScale;
	return (float)n / (float)m_nFontScale;
}
CLTGUIFont* CInterfaceResMgr::GetDialogueFont(LTBOOL bLarge)
{
	CLTGUIFont* pMenu = bLarge ? GetMsgForeFont() : m_pMediumFont;
	if (!m_bScaledFonts) return pMenu;
	const int nWant = VRDialogueFontScale(m_nFontScale);
	if (nWant == m_nFontScale) { m_nDlgFontScale = m_nFontScale; return pMenu; }
	const int d = bLarge ? 1 : 0;
	if (!m_pDlgFont[d])
	{
		m_pDlgFont[d] = debug_new(CLTGUIFont);
		const int nSaveScale = m_nFontScale;
		const LTBOOL bSaveScaled = m_bScaledFonts;
		char szSave[256];
		LTStrCpy(szSave, g_szFontName, sizeof(szSave));
		if (bLarge) g_pLayoutMgr->GetLargeFontBase(g_szFontName, sizeof(g_szFontName));
		else        g_pLayoutMgr->GetMediumFontBase(g_szFontName, sizeof(g_szFontName));
		m_nFontScale = nWant;
		m_bScaledFonts = LTFALSE;			// so the helper says which sheet loaded
		const LTBOOL bOk = SetupFontScaled(m_pDlgFont[d]);
		const LTBOOL bGotScaled = m_bScaledFonts;
		m_nFontScale = nSaveScale;
		m_bScaledFonts = bSaveScaled;
		LTStrCpy(g_szFontName, szSave, sizeof(g_szFontName));
		if (!bOk || !bGotScaled)
		{
			// No sheet at that size: the box keeps the menu's font, and the
			// layout ratio falls back to 1 with it.
			if (m_pDlgFont[d]) { m_pDlgFont[d]->Term(); debug_delete(m_pDlgFont[d]); m_pDlgFont[d] = LTNULL; }
			m_nDlgFontScale = m_nFontScale;
			VRLog::Msg("VRDialogueSize: no %dx sheet for the %s dialogue font - keeping the menu's", nWant, bLarge ? "large" : "medium");
			return pMenu;
		}
		m_nDlgFontScale = nWant;
		VRLog::Msg("VRDialogueSize: %s dialogue font at %dx (menus %dx)", bLarge ? "large" : "medium", nWant, m_nFontScale);
	}
	return m_pDlgFont[d] ? m_pDlgFont[d] : pMenu;
}

CLTGUIFont* CInterfaceResMgr::GetLargeFont()
{
	// +VRMenuHDFont 0 puts the original 1096x28 sheet back.
	//
	// The upstream "#if 1" that disabled this is not a mistake and its comment
	// says so - only the LARGE font has HD art, so turning it on makes the menu
	// mix two glyph sizes. On a monitor that is a cosmetic regression and not
	// worth it. In a headset the large font is the one carrying every menu item
	// a player has to read, it measures 0.79 degrees without this, and mixed
	// sizes are a price worth paying. See the note where the cvar is declared.
	if (g_vtVRMenuHDFont.GetFloat(1.0f) > 0.0f
		&& m_pLargeHDFont != LTNULL && GetScreenHeight() > 900)
	{
		return m_pLargeHDFont;
	}

	return m_pLargeFont;
}
// Subtitle.cpp: implementation of the CSubtitle class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Subtitle.h"
extern VarTrack g_vtVRCaptions;
#include "InterfaceMgr.h"
#include "VRShared.h"
#include "ClientUtilities.h"

VarTrack	g_vtSubtitles;
VarTrack	g_vtSubtitleMaxDist;

// THE SPOKEN-DIALOGUE CAPTION IN A HEADSET (headset, 24 September: large and
// right in the face). The 2D layer spans the eye's width, so the old third of
// the screen was a 31-degree block about 6 degrees below the forward ray. Now:
//   VRCaptionWidth  wrap width, fraction of the screen (default 0.30)
//   VRCaptionScale  drawn at this fraction of the font's size (default 0.75)
//   VRCaptionY      top of the block, fraction of the screen (default 0.76,
//                   about 13 degrees below the forward ray in a Quest 3)
// The time-and-place captions (MissionText) and the dialogue window are
// separate and unchanged.
static float VRCaptionWidth() { const float f = GetConsoleFloat("VRCaptionWidth", 0.30f); return (f > 0.05f) ? f : 0.30f; }
static float VRCaptionScale() { const float f = GetConsoleFloat("VRCaptionScale", 0.75f); return (f > 0.1f) ? f : 1.0f; }
static void VRCaptionPlace(const LTIntPt& size, LTIntPt& pos)
{
	const float s = VRCaptionScale();
	pos.x = ((int)g_pInterfaceResMgr->GetScreenWidth() - (int)((float)size.x * s)) / 2;
	pos.y = (int)(g_pInterfaceResMgr->GetScreenHeight() * GetConsoleFloat("VRCaptionY", 0.76f));
}
// ON OR OFF, from the game's own Options > Game > Subtitles row: in VR that
// row reads and writes VRSubtitles (default on), because the game's
// Subtitles defaults to off for English and VR has shown them regardless.
bool VRSubtitlesOn() { return GetConsoleFloat("VRSubtitles", 1.0f) > 0.0f; }




//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSubtitle::CSubtitle()
{
    m_hForeSurf         = LTNULL;
    m_pForeFont         = LTNULL;
	m_bVisible			= LTFALSE;
	m_vSpeakerPos.Init();
	m_fRadius			= 0;
	m_fDuration			= -1.0f;
	m_bOverflow			= LTFALSE;

}



CSubtitle::~CSubtitle()
{
	Clear();
	if (m_hForeSurf)
	{
        g_pLTClient->DeleteSurface(m_hForeSurf);
        m_hForeSurf = LTNULL;
	}
}


void	CSubtitle::Init()
{

	m_pForeFont		= g_pInterfaceResMgr->GetMsgForeFont();
	m_nLineHeight	= m_pForeFont->GetHeight();


	m_CinematicPos		= g_pLayoutMgr->GetSubtitleCinematicPos();
	m_nCinematicWidth    = (uint32)g_pLayoutMgr->GetSubtitleCinematicWidth();
	m_FullScreenPos		= g_pLayoutMgr->GetSubtitleFullScreenPos();
	m_nFullScreenWidth    = (uint32)g_pLayoutMgr->GetSubtitleFullScreenWidth();

	m_nMaxLines		= g_pLayoutMgr->GetSubtitleNumLines();

	m_bVisible		= LTFALSE;
	m_dwWidth		= 0;
	m_dwHeight		= 0;

	LTFLOAT defSubtitles = 0.0f;
	if (!g_pInterfaceResMgr->IsEnglish())
		defSubtitles = 1.0f;
	g_vtSubtitles.Init(g_pLTClient, "Subtitles", LTNULL, defSubtitles);
    g_vtSubtitleMaxDist.Init(g_pLTClient, "SubtitleMaxDist", LTNULL, 1000.0f);

	m_vSpeakerPos.Init();
	m_fRadius = 0.0;
	m_fDuration = -1.0f;

	m_hSubtitleColor = g_pLayoutMgr->GetSubtitleTint();
}
	

void CSubtitle::ClearSurfaces()
{
	uint32 dwWidth  = 0;
	uint32 dwHeight  = 0;

	// During shutdown, OptimizedRenderer gets cleared, thus causing crashing.
	if (g_pOptimizedRenderer) {
		g_pLTClient->GetSurfaceDims(m_hForeSurf, &dwWidth, &dwHeight);
		LTRect rcFore(0, 0, dwWidth, dwHeight);
		g_pOptimizedRenderer->FillRect(m_hForeSurf, &rcFore, LTNULL);
		g_pLTClient->OptimizeSurface(m_hForeSurf, LTNULL);
	}
}


void CSubtitle::Show(int nStringId, LTVector vSpeakerPos, LTFLOAT fRadius, LTFLOAT fDuration)
{
	HSTRING hText = g_pLTClient->FormatString(nStringId);
	if (!hText) return;

	
	m_vSpeakerPos = vSpeakerPos;
	m_fRadius = fRadius > 0 ? fRadius : g_vtSubtitleMaxDist.GetFloat();

	m_fDuration = fDuration;

	// THE FONT AS IT IS NOW, not as it was at Init. The message font is
	// rebuilt when the resolution changes and swapped for the large sheet
	// under VRMenuBigSubs, and a line height taken once at start-up went
	// stale: the scroll window was three SMALL lines tall while the text was
	// set in the large face, so a four-line exchange in the apartment showed
	// its first and last lines cut in half. Desk shot, T01S02.
	m_pForeFont   = g_pInterfaceResMgr->GetMsgForeFont();
	m_nLineHeight = m_pForeFont->GetHeight();
	// And room for a whole exchange: a floating caption that scrolls
	// through four lines two and a half at a time is worse than one that
	// simply shows them.
	if (g_vtVRCaptions.GetFloat(1.0f) > 0.0f && m_nMaxLines < 64) m_nMaxLines = 64;

	uint32 width = 0;
	if (g_pGameClientShell->IsUsingExternalCamera())
	{
		m_pos.x = (int)(g_pInterfaceResMgr->GetXRatio() * (LTFLOAT)m_CinematicPos.x);
		m_pos.y = (int)(g_pInterfaceResMgr->GetYRatio() * (LTFLOAT)m_CinematicPos.y);
		width = (uint32)(g_pInterfaceResMgr->GetXRatio() * (LTFLOAT)m_nCinematicWidth);
	}
	else
	{
		m_pos.x = (int)(g_pInterfaceResMgr->GetXRatio() * (LTFLOAT)m_FullScreenPos.x);
		m_pos.y = (int)(g_pInterfaceResMgr->GetYRatio() * (LTFLOAT)m_FullScreenPos.y);
		width = (uint32)(g_pInterfaceResMgr->GetXRatio() * (LTFLOAT)m_nFullScreenWidth);
	}
	 
	// VR CAPTIONS (VRCaptions, default 1): a block of white text with a dark
	// shadow, wrapped to about a third of the eye, sitting a little below the
	// centre of the view, where the eyes already are. The authored layout puts
	// it along the bottom edge of a 4:3 monitor, which in a headset is the
	// bottom of the picture. VRCaptions 0 is the authored placement.
	if (g_vtVRCaptions.GetFloat(1.0f) > 0.0f)
		width = (uint32)(VRCaptionWidth() * (float)g_pInterfaceResMgr->GetScreenWidth());
	uint32 height = m_nMaxLines * m_nLineHeight;

	m_dwWidth = 0;
	m_dwHeight = 0;
	
	if (m_hForeSurf)
	{
		g_pLTClient->GetSurfaceDims(m_hForeSurf,&m_dwWidth,&m_dwHeight);
	}
	m_txtSize = m_pForeFont->GetTextExtentsFormat(hText,(int)width);
	const LTBOOL bVRCap = (g_vtVRCaptions.GetFloat(1.0f) > 0.0f) ? LTTRUE : LTFALSE;
	if (bVRCap)
	{
		// THE WHOLE EXCHANGE AT ONCE. The authored window is three lines
		// that scroll, and a scrolling window shows its first and last line
		// cut in half most of the time. A floating caption simply holds every
		// line; the window is the text's own height.
		VRCaptionPlace(m_txtSize, m_pos);
		height  = (uint32)m_txtSize.y;
	}


	if ((uint32)m_txtSize.x > m_dwWidth || (uint32)m_txtSize.y > m_dwHeight)
	{
		if (m_hForeSurf) g_pLTClient->DeleteSurface(m_hForeSurf);
		m_hForeSurf = g_pLTClient->CreateSurface(m_txtSize.x,m_txtSize.y);
		m_dwWidth = m_txtSize.x;
		m_dwHeight = m_txtSize.y;
	}
	ClearSurfaces();

	int numLines = m_txtSize.y / m_nLineHeight;
	//g_pLTClient->CPrint("Lines: %d",numLines);
	//g_pLTClient->CPrint("Duration: %0.2f",fDuration);

	if ((uint32)numLines > m_nMaxLines)
	{
		m_bOverflow = LTTRUE;
		LTFLOAT fTimePerLine = m_fDuration / ((LTFLOAT)numLines + 1.0f);
		LTFLOAT fRemainingLines = (LTFLOAT)((uint32)numLines - m_nMaxLines);
		LTFLOAT fDelay = (LTFLOAT)m_nMaxLines * fTimePerLine;
		m_fScrollStartTime = fDelay + g_pLTClient->GetTime();

		m_fScrollSpeed = (LTFLOAT)m_nLineHeight / fTimePerLine;
		m_fMaxOffset = (LTFLOAT)((uint32)m_txtSize.y - height);

	}
	else
	{
		m_bOverflow = LTFALSE;

		// The authored placement centres in its column and sits on the
		// window's floor; the VR caption is already placed.
		if (!bVRCap)
		{
			m_pos.x += (width - m_txtSize.x) / 2;
			m_pos.y += (height - m_txtSize.y);
		}
		m_rcSrcRect = LTRect(0,0,width,height);
		m_fScrollSpeed = 0.0f;

	}

	// THE VR CAPTION'S SOURCE IS THE TEXT, NOT THE WRAP WIDTH. The surface is
	// created to the text's own size, and a short line ("gurgle") is far
	// narrower than the wrap width - so a source rectangle of the whole wrap
	// width read past the surface and the blit stretched the word across it.
	uint32 wSrc = width;
	if (bVRCap && m_txtSize.x > 0 && (uint32)m_txtSize.x < width) wSrc = (uint32)m_txtSize.x;
	m_rcBaseRect = LTRect(0,0,wSrc-1,height-1);
	m_rcSrcRect = m_rcBaseRect;
	m_fOffset = 0.0f;

	m_pForeFont->DrawFormat(hText,m_hForeSurf,0,0,(uint32)width,kWhite);

	m_bVisible			= LTTRUE;

	if (hText)
	{
        g_pLTClient->FreeString(hText);
	}

}

void CSubtitle::Clear()
{
	ClearSurfaces();
	m_bVisible			= LTFALSE;

}


void CSubtitle::Draw()
{
	if (!m_bVisible) return;
	// VRCaptions implies subtitles: the game's own default is OFF for English.
	if (VRShared::IsLive() && !VRSubtitlesOn()) return;
	if (g_vtSubtitles.GetFloat() == 0.0f && g_vtVRCaptions.GetFloat(1.0f) <= 0.0f) return;

	// Show subtitles if conversations in range)...

	LTVector vListenerPos;
	LTBOOL bListenerInClient;
	LTRotation rRot;
	g_pLTClient->GetListener(&bListenerInClient, &vListenerPos, &rRot);

	//HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();
	//if (!hPlayerObj) return;
	// g_pLTClient->GetObjectPos(hPlayerObj, &vListenerPos);

	LTBOOL bForceDraw = (LTBOOL)(m_vSpeakerPos == LTVector(0, 0, 0));
	bForceDraw = g_pGameClientShell->IsUsingExternalCamera() ? LTTRUE : bForceDraw;

	LTVector vPos = m_vSpeakerPos - vListenerPos;
	if (bForceDraw || vPos.Mag() <= m_fRadius)
	{
		LTSurfaceBlend  oldBlend = LTSURFACEBLEND_ALPHA;
//		g_pLTClient->GetOptimized2DBlend(oldBlend);

		// The shadow: one pixel suited a 12-pixel font; the large sheet wants a couple.
		const int nS = (g_vtVRCaptions.GetFloat(1.0f) > 0.0f) ? ((m_nLineHeight >= 40) ? 3 : 2) : 1;
		// THE VR CAPTION DRAWN SMALLER. It is set in the large sheet the
		// dialogue window also uses (that window is right as it is), so the
		// caption is shrunk at the blit instead: VRCaptionScale of its size.
		const float fSc = (g_vtVRCaptions.GetFloat(1.0f) > 0.0f) ? VRCaptionScale() : 1.0f;
		if (fSc != 1.0f)
		{
			LTRect rcSrc = m_rcSrcRect;
			const int w = (int)((float)(rcSrc.right - rcSrc.left) * fSc);
			const int h = (int)((float)(rcSrc.bottom - rcSrc.top) * fSc);
			const int nSs = (nS > 1) ? nS - 1 : 1;
			LTRect rcShadow(m_pos.x + nSs, m_pos.y + nSs, m_pos.x + nSs + w, m_pos.y + nSs + h);
			LTRect rcText(m_pos.x, m_pos.y, m_pos.x + w, m_pos.y + h);
			g_pLTClient->SetOptimized2DBlend(LTSURFACEBLEND_MASK);
			g_pLTClient->ScaleSurfaceToSurface(g_pLTClient->GetScreenSurface(), m_hForeSurf, &rcShadow, &rcSrc);
			g_pLTClient->SetOptimized2DBlend(LTSURFACEBLEND_ADD);
			g_pLTClient->SetOptimized2DColor(m_hSubtitleColor);
			g_pLTClient->ScaleSurfaceToSurface(g_pLTClient->GetScreenSurface(), m_hForeSurf, &rcText, &rcSrc);
			g_pLTClient->SetOptimized2DColor(kWhite);
			g_pLTClient->SetOptimized2DBlend(oldBlend);
			return;
		}
		g_pLTClient->SetOptimized2DBlend(LTSURFACEBLEND_MASK);
		g_pLTClient->DrawSurfaceToSurface(g_pLTClient->GetScreenSurface(), m_hForeSurf, &m_rcSrcRect, m_pos.x+nS, m_pos.y+nS);
		g_pLTClient->SetOptimized2DBlend(LTSURFACEBLEND_ADD);
		g_pLTClient->SetOptimized2DColor(m_hSubtitleColor);
		g_pLTClient->DrawSurfaceToSurface(g_pLTClient->GetScreenSurface(), m_hForeSurf, &m_rcSrcRect, m_pos.x, m_pos.y);
		g_pLTClient->SetOptimized2DColor(kWhite);
		g_pLTClient->SetOptimized2DBlend(oldBlend);
	}
}


void CSubtitle::Update()
{
	if (!m_bVisible) return;
	if (!m_bOverflow) return;

	if (m_fScrollStartTime < g_pLTClient->GetTime())
	{
		LTFLOAT fElapsedTime = g_pLTClient->GetTime() - m_fScrollStartTime;
		m_fOffset = (fElapsedTime * m_fScrollSpeed);
		if (m_fOffset > m_fMaxOffset) m_fOffset = m_fMaxOffset;
		m_rcSrcRect = m_rcBaseRect;
		m_rcSrcRect.top += (int)m_fOffset;
		m_rcSrcRect.bottom += (int)m_fOffset;
	}
}

void CSubtitle::ScreenDimsChanged()
{
	if (!m_bVisible) return;
	if (g_vtSubtitles.GetFloat() == 0.0f) return;

	uint32 width = 0;
	if (g_pGameClientShell->IsUsingExternalCamera())
	{
		m_pos.x = (int)(g_pInterfaceResMgr->GetXRatio() * (LTFLOAT)m_CinematicPos.x);
		m_pos.y = (int)(g_pInterfaceResMgr->GetYRatio() * (LTFLOAT)m_CinematicPos.y);
		width = (uint32)(g_pInterfaceResMgr->GetXRatio() * (LTFLOAT)m_nCinematicWidth);
	}
	else
	{
		m_pos.x = (int)(g_pInterfaceResMgr->GetXRatio() * (LTFLOAT)m_FullScreenPos.x);
		m_pos.y = (int)(g_pInterfaceResMgr->GetYRatio() * (LTFLOAT)m_FullScreenPos.y);
		width = (uint32)(g_pInterfaceResMgr->GetXRatio() * (LTFLOAT)m_nFullScreenWidth);
	}
	
	uint32 height = m_nMaxLines * m_nLineHeight;

	if (g_vtVRCaptions.GetFloat(1.0f) > 0.0f)
	{
		// Placed as Show() places it, whatever the new size is.
		VRCaptionPlace(m_txtSize, m_pos);
		return;
	}

	if (m_bOverflow)
	{
	}
	else
	{
		m_pos.x += (width - m_txtSize.x) / 2;
		m_pos.y += (height - m_txtSize.y);
	}
}

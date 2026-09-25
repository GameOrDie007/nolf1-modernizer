// WeaponChooser.cpp: implementation of the CWeaponChooser class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "WeaponChooser.h"
#include "InterfaceMgr.h"
#include "GameClientShell.h"
#include "WinUtil.h"
#include "SoundMgr.h"
#include "LayoutMgr.h"

extern CGameClientShell* g_pGameClientShell;


namespace
{
	const int kLastWeapon = (NUM_WEAPON_ICONS - 1);
	const int kLastAmmo = (NUM_AMMO_ICONS - 1);
	const int kCurrWeapon = 1;
	const int kCurrAmmo = 0;
	const int kIconWidth	= 48;
	const int kIconSpacing  = 16;
    const float kfIconY     = 400.0f;
	const float kfDelayTime	= 3.0f;
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CWeaponChooser::CWeaponChooser()
{
    memset(m_hWeaponSurf, LTNULL, sizeof(m_hWeaponSurf));
	memset(m_nWeapons, -1, sizeof(m_nWeapons));
    m_hWeaponStr = LTNULL;
    m_szWeaponCommand[0] = LTNULL;
    m_bIsOpen = LTFALSE;
	m_fStartTime = 0.0f;
}

CWeaponChooser::~CWeaponChooser()
{
}

void CWeaponChooser::Term()
{
	if (m_bIsOpen)
		Close();
}

LTBOOL CWeaponChooser::Open()
{
	if (m_bIsOpen)
        return LTTRUE;

	m_nWeapons[0] = g_pGameClientShell->GetWeaponModel()->PrevWeapon();
	m_nWeapons[1] = g_pGameClientShell->GetWeaponModel()->GetWeaponId();
	m_nWeapons[2] = g_pGameClientShell->GetWeaponModel()->NextWeapon();

	if (m_nWeapons[1] == m_nWeapons[2])
	{
		for (int i = 0; i < NUM_WEAPON_ICONS; i++)
			m_nWeapons[i] = -1;
        m_bIsOpen = LTFALSE;
        return LTFALSE;
	}
    m_bIsOpen = LTTRUE;

    WEAPON* pWeapon = LTNULL;
	for (int i = 0; i < NUM_WEAPON_ICONS; i++)
	{
		pWeapon = g_pWeaponMgr->GetWeapon(m_nWeapons[i]);
        if (pWeapon)
		{
            m_hWeaponSurf[i] = g_pLTClient->CreateSurfaceFromBitmap(pWeapon->szIcon);
            g_pLTClient->OptimizeSurface(m_hWeaponSurf[i],SETRGB_T(255,0,255));
		}
	}

	pWeapon = g_pWeaponMgr->GetWeapon(m_nWeapons[1]);
	if (pWeapon)
	{
        m_hWeaponStr = g_pLTClient->FormatString(pWeapon->nNameId);
        SetCommandStr(m_nWeapons[1]);
        g_pLTClient->SetSurfaceAlpha(m_hWeaponSurf[0],0.5f);
        g_pLTClient->SetSurfaceAlpha(m_hWeaponSurf[1],1.0f);
        g_pLTClient->SetSurfaceAlpha(m_hWeaponSurf[2],0.5f);
	}

    m_fStartTime = g_pLTClient->GetTime();

    return LTTRUE;

}

void CWeaponChooser::Close()
{
	if (!m_bIsOpen)
		return;
	for (int i = 0; i < NUM_WEAPON_ICONS; i++)
	{
		m_nWeapons[i] = -1;
		if (m_hWeaponSurf[i])
		{
            g_pLTClient->DeleteSurface(m_hWeaponSurf[i]);
            m_hWeaponSurf[i] = LTNULL;
		}
	}
	if (m_hWeaponStr)
	{
        g_pLTClient->FreeString(m_hWeaponStr);
        m_hWeaponStr = LTNULL;
	}
	m_szWeaponCommand[0] = LTNULL;

    m_bIsOpen = LTFALSE;
	m_fStartTime = 0.0f;
}

void CWeaponChooser::NextWeapon()
{
	if (m_hWeaponSurf[0])
	{
        g_pLTClient->DeleteSurface(m_hWeaponSurf[0]);
        m_hWeaponSurf[0] = LTNULL;
	}
    int i;
    for (i = 0; i < kLastWeapon; i++)
	{
		m_nWeapons[i] = m_nWeapons[i+1];
		m_hWeaponSurf[i] = m_hWeaponSurf[i+1];
	}
	m_nWeapons[kLastWeapon] = g_pGameClientShell->GetWeaponModel()->NextWeapon(m_nWeapons[kLastWeapon-1]);

	WEAPON* pWeapon = g_pWeaponMgr->GetWeapon(m_nWeapons[kLastWeapon]);
	if (pWeapon)
	{
        m_hWeaponSurf[kLastWeapon] = g_pLTClient->CreateSurfaceFromBitmap(pWeapon->szIcon);
        g_pLTClient->OptimizeSurface(m_hWeaponSurf[kLastWeapon],SETRGB_T(255,0,255));
	}
	if (m_hWeaponStr)
	{
        g_pLTClient->FreeString(m_hWeaponStr);
        m_hWeaponStr = LTNULL;
	}
	m_szWeaponCommand[0] = LTNULL;

	pWeapon = g_pWeaponMgr->GetWeapon(m_nWeapons[1]);
	if (pWeapon)
	{
        m_hWeaponStr = g_pLTClient->FormatString(pWeapon->nNameId);
		SetCommandStr(m_nWeapons[1]);
        g_pLTClient->SetSurfaceAlpha(m_hWeaponSurf[0],0.5f);
        g_pLTClient->SetSurfaceAlpha(m_hWeaponSurf[1],1.0f);
        g_pLTClient->SetSurfaceAlpha(m_hWeaponSurf[2],0.5f);
	}

    g_pClientSoundMgr->PlayInterfaceSound((char*)g_pInterfaceResMgr->GetSoundSelect());

    m_fStartTime = g_pLTClient->GetTime();

}

void CWeaponChooser::PrevWeapon()
{
	if (m_hWeaponSurf[kLastWeapon])
	{
        g_pLTClient->DeleteSurface(m_hWeaponSurf[kLastWeapon]);
        m_hWeaponSurf[kLastWeapon] = LTNULL;
	}
    int i;
    for (i = kLastWeapon; i > 0; i--)
	{
		m_nWeapons[i] = m_nWeapons[i-1];
		m_hWeaponSurf[i] = m_hWeaponSurf[i-1];
	}
	m_nWeapons[0] = g_pGameClientShell->GetWeaponModel()->PrevWeapon(m_nWeapons[1]);

	WEAPON* pWeapon = g_pWeaponMgr->GetWeapon(m_nWeapons[0]);
	if (pWeapon)
	{
        m_hWeaponSurf[0] = g_pLTClient->CreateSurfaceFromBitmap(pWeapon->szIcon);
        g_pLTClient->OptimizeSurface(m_hWeaponSurf[0],SETRGB_T(255,0,255));
	}
	if (m_hWeaponStr)
	{
        g_pLTClient->FreeString(m_hWeaponStr);
        m_hWeaponStr = LTNULL;
	}
	m_szWeaponCommand[0] = LTNULL;


	pWeapon = g_pWeaponMgr->GetWeapon(m_nWeapons[1]);
	if (pWeapon)
	{
        m_hWeaponStr = g_pLTClient->FormatString(pWeapon->nNameId);
		SetCommandStr(m_nWeapons[1]);
        g_pLTClient->SetSurfaceAlpha(m_hWeaponSurf[0],0.5f);
        g_pLTClient->SetSurfaceAlpha(m_hWeaponSurf[1],1.0f);
        g_pLTClient->SetSurfaceAlpha(m_hWeaponSurf[2],0.5f);
	}

    g_pClientSoundMgr->PlayInterfaceSound((char*)g_pInterfaceResMgr->GetSoundSelect());

    m_fStartTime = g_pLTClient->GetTime();

}

void CWeaponChooser::Draw()
{
    float fTime = g_pLTClient->GetTime() - m_fStartTime;
	if (m_fStartTime > 0.0f && fTime > kfDelayTime)
	{
        g_pClientSoundMgr->PlayInterfaceSound((char*)g_pInterfaceResMgr->GetSoundSelect());
		Close();
		return;
	}


    HSURFACE hScreen = g_pLTClient->GetScreenSurface();
    uint32 nScreenHeight, nScreenWidth;
    g_pLTClient->GetSurfaceDims (hScreen, &nScreenWidth, &nScreenHeight);
	float yRatio = (float)nScreenHeight / 480.0f;

	int x = (int)nScreenWidth / 2;
	int y = (int) (yRatio * kfIconY);

	CLTGUIFont* pFont = g_pInterfaceResMgr->GetChooserFont();
	if (pFont)
	{
		pFont->Draw(m_hWeaponStr,hScreen,x,y+(kIconWidth + kIconSpacing),LTF_JUSTIFY_CENTER,kWhite);
	}

	x -= (NUM_WEAPON_ICONS * (kIconWidth + kIconSpacing)) / 2;

	if (m_hWeaponSurf[0] && m_nWeapons[0] != m_nWeapons[kLastWeapon])
        g_pLTClient->DrawSurfaceToSurfaceTransparent(hScreen, m_hWeaponSurf[0], LTNULL, x, y,SETRGB_T(255,0,255));

	x += (kIconWidth + kIconSpacing);
	if (m_hWeaponSurf[1])
	{
        LTVector vColor = g_pLayoutMgr->GetChooserHighlightColor();

        LTRect rect(x-3,y-3,x+kIconWidth+3,y+kIconWidth+3);
        g_pOptimizedRenderer->FillRect(hScreen,&rect,SETRGB_T(vColor.x,vColor.y,vColor.z));
        g_pLTClient->DrawSurfaceToSurface(hScreen, m_hWeaponSurf[1], LTNULL, x, y);
	}

// commented out drawing of weapon command - jrg 9/22
	if (pFont && m_szWeaponCommand[0])
	{
		pFont->Draw(m_szWeaponCommand,hScreen,x,y,LTF_JUSTIFY_LEFT,SETRGB(255,255,0));
	}

	x += (kIconWidth + kIconSpacing);


	if (m_hWeaponSurf[2] && m_nWeapons[2] != m_nWeapons[1])
        g_pLTClient->DrawSurfaceToSurfaceTransparent(hScreen, m_hWeaponSurf[2], LTNULL, x, y,SETRGB_T(255,0,255));

}

void CWeaponChooser::SetCommandStr(int nWeaponId)
{
	int commandId = g_pWeaponMgr->GetCommandId(nWeaponId);
	GetCommandKeyStr(commandId,m_szWeaponCommand,sizeof(m_szWeaponCommand));

}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CAmmoChooser::CAmmoChooser()
{
    memset(m_hAmmoSurf, LTNULL, sizeof(m_hAmmoSurf));
	memset(m_nAmmo, -1, sizeof(m_nAmmo));
    m_hAmmoStr = LTNULL;
    m_bIsOpen = LTFALSE;
	m_fStartTime = 0.0f;
}

CAmmoChooser::~CAmmoChooser()
{
}

void CAmmoChooser::Term()
{
	if (m_bIsOpen)
		Close();
}

LTBOOL CAmmoChooser::Open()
{
	// Don't allow the chooser to be opened if we're selecting/deselecting a
	// weapon...

	WeaponState eState = g_pGameClientShell->GetWeaponModel()->GetState();
	if (W_DESELECT == eState || W_SELECT == eState) return LTFALSE;


	if (m_bIsOpen)
        return LTTRUE;

	m_nAmmo[kCurrAmmo] = g_pGameClientShell->GetWeaponModel()->GetAmmoId();
	m_nAmmo[kLastAmmo] = g_pGameClientShell->GetWeaponModel()->NextAmmo();

	if (m_nAmmo[kCurrAmmo] == m_nAmmo[kLastAmmo])
	{
		for (int i = 0; i < NUM_AMMO_ICONS; i++)
			m_nAmmo[i] = -1;
        m_bIsOpen = LTFALSE;
        return LTFALSE;
	}
    m_bIsOpen = LTTRUE;

    AMMO* pAmmo = LTNULL;

	for (int i = 0; i < NUM_AMMO_ICONS; i++)
	{
		pAmmo = g_pWeaponMgr->GetAmmo(m_nAmmo[i]);
		if (pAmmo)
		{
            m_hAmmoSurf[i] = g_pLTClient->CreateSurfaceFromBitmap(pAmmo->szIcon);
            g_pLTClient->OptimizeSurface(m_hAmmoSurf[i],SETRGB_T(255,0,255));
		}
	}

	pAmmo = g_pWeaponMgr->GetAmmo(m_nAmmo[kLastAmmo]);
	if (pAmmo)
	{
        m_hAmmoStr = g_pLTClient->FormatString(pAmmo->nNameId);
        g_pLTClient->SetSurfaceAlpha(m_hAmmoSurf[kCurrAmmo],1.0f);
        g_pLTClient->SetSurfaceAlpha(m_hAmmoSurf[kLastAmmo],0.5f);
	}

    m_fStartTime = g_pLTClient->GetTime();

    return LTTRUE;
}

void CAmmoChooser::Close()
{
	if (!m_bIsOpen)
		return;
	for (int i = 0; i < NUM_AMMO_ICONS; i++)
	{
		m_nAmmo[i] = -1;
		if (m_hAmmoSurf[i])
		{
            g_pLTClient->DeleteSurface(m_hAmmoSurf[i]);
            m_hAmmoSurf[i] = LTNULL;
		}
	}
	if (m_hAmmoStr)
	{
        g_pLTClient->FreeString(m_hAmmoStr);
        m_hAmmoStr = LTNULL;
	}
    m_bIsOpen = LTFALSE;
	m_fStartTime = 0.0f;
}

void CAmmoChooser::NextAmmo()
{
	if (m_hAmmoSurf[kCurrAmmo])
	{
        g_pLTClient->DeleteSurface(m_hAmmoSurf[kCurrAmmo]);
        m_hAmmoSurf[kCurrAmmo] = LTNULL;
	}
	m_nAmmo[kCurrAmmo] = m_nAmmo[kLastAmmo];
	m_hAmmoSurf[kCurrAmmo] = m_hAmmoSurf[kLastAmmo];

	m_nAmmo[kLastAmmo] = g_pGameClientShell->GetWeaponModel()->NextAmmo(m_nAmmo[kCurrAmmo]);

	AMMO* pAmmo = g_pWeaponMgr->GetAmmo(m_nAmmo[kLastAmmo]);
	if (pAmmo)
	{
        m_hAmmoSurf[kLastAmmo] = g_pLTClient->CreateSurfaceFromBitmap(pAmmo->szIcon);
        g_pLTClient->OptimizeSurface(m_hAmmoSurf[kLastAmmo],SETRGB_T(255,0,255));
	}
	if (m_hAmmoStr)
	{
        g_pLTClient->FreeString(m_hAmmoStr);
        m_hAmmoStr = LTNULL;
	}

	pAmmo = g_pWeaponMgr->GetAmmo(m_nAmmo[kCurrAmmo]);
	if (pAmmo)
	{
        m_hAmmoStr = g_pLTClient->FormatString(pAmmo->nNameId);
        g_pLTClient->SetSurfaceAlpha(m_hAmmoSurf[kCurrAmmo],1.0f);
        g_pLTClient->SetSurfaceAlpha(m_hAmmoSurf[kLastAmmo],0.5f);
	}

	g_pClientSoundMgr->PlayInterfaceSound((char*)g_pInterfaceResMgr->GetSoundSelect());

    m_fStartTime = g_pLTClient->GetTime();

}


void CAmmoChooser::Draw()
{
    float fTime = g_pLTClient->GetTime() - m_fStartTime;
	if (m_fStartTime > 0.0f && fTime > kfDelayTime)
	{
		g_pClientSoundMgr->PlayInterfaceSound((char*)g_pInterfaceResMgr->GetSoundSelect());
		Close();
		return;
	}

	CPlayerStats *pStats = g_pInterfaceMgr->GetPlayerStats();

    HSURFACE hScreen = g_pLTClient->GetScreenSurface();
    uint32 nScreenHeight, nScreenWidth;
    g_pLTClient->GetSurfaceDims (hScreen, &nScreenWidth, &nScreenHeight);
	float yRatio = (float)nScreenHeight / 480.0f;

	int x = (int)nScreenWidth / 2;
	int y = (int) (yRatio * kfIconY);

	CLTGUIFont *pFont = g_pInterfaceResMgr->GetChooserFont();
	pFont->Draw(m_hAmmoStr,hScreen,x,y+(kIconWidth + kIconSpacing),LTF_JUSTIFY_CENTER,kWhite);

	x -= (NUM_AMMO_ICONS * (kIconWidth + kIconSpacing)) / 2;
	x -= kIconWidth / 2;

	if (m_hAmmoSurf[kCurrAmmo])
	{
        LTRect rect(x-3,y-3,x+kIconWidth+3,y+kIconWidth+3);
        g_pOptimizedRenderer->FillRect(hScreen,&rect,SETRGB_T(220,192,255));
        g_pLTClient->DrawSurfaceToSurface(hScreen, m_hAmmoSurf[kCurrAmmo], LTNULL, x, y);
		int count = pStats->GetAmmoCount(m_nAmmo[kCurrAmmo]);
		if (count > 0 && count < 1000)
		{
			char szStr[5] = "";
			sprintf(szStr,"%d",count);
			pFont->Draw(szStr,hScreen,(x+kIconWidth/2),y + kIconWidth - 16,LTF_JUSTIFY_CENTER,kWhite);
		}
	}
	x += (kIconWidth + kIconSpacing);

	if (m_hAmmoSurf[kLastAmmo])
	{
        g_pLTClient->DrawSurfaceToSurfaceTransparent(hScreen, m_hAmmoSurf[kLastAmmo], LTNULL, x, y,SETRGB_T(255,0,255));
		int count = pStats->GetAmmoCount(m_nAmmo[kLastAmmo]);
		if (count > 0 && count < 1000)
		{
			char szStr[5] = "";
			sprintf(szStr,"%d",count);
			pFont->Draw(szStr,hScreen,(x+kIconWidth/2),y + kIconWidth - 16,LTF_JUSTIFY_CENTER);
		}
	}

}


// ----------------------------------------------------------------------- //
//
//	CVRWeaponWheel - see WeaponChooser.h.
//
// ----------------------------------------------------------------------- //

#include "PlayerStats.h"
#include "VRLog.h"
#include <math.h>

extern VarTrack g_vtVRMenuBigSubs;
extern VarTrack g_vtVRWheelSize;

CVRWeaponWheel::CVRWeaponWheel()
{
	m_bIsOpen = LTFALSE;
	m_nSlots = 0;
	m_nHighlight = -1;
	m_bDeflected = LTFALSE;
	m_hName = LTNULL;
	m_fOpenTime = 0.0f;
	for (int i = 0; i < VRWHEEL_MAX_SLOTS; i++)
	{
		m_nWeapon[i] = 0; m_hSurf[i] = LTNULL; m_nSurfW[i] = m_nSurfH[i] = 0;
	}
}

CVRWeaponWheel::~CVRWeaponWheel()
{
	Term();
}

void CVRWeaponWheel::Init()
{
}

void CVRWeaponWheel::Term()
{
	FreeSurfaces();
	if (m_hName) { g_pLTClient->FreeString(m_hName); m_hName = LTNULL; }
	m_bIsOpen = LTFALSE;
}

void CVRWeaponWheel::FreeSurfaces()
{
	for (int i = 0; i < VRWHEEL_MAX_SLOTS; i++)
	{
		if (m_hSurf[i]) { g_pLTClient->DeleteSurface(m_hSurf[i]); m_hSurf[i] = LTNULL; }
	}
	m_nSlots = 0;
}

LTBOOL CVRWeaponWheel::Open()
{
	if (m_bIsOpen) return LTTRUE;
	if (!g_pWeaponMgr || !g_pInterfaceMgr) return LTFALSE;
	CPlayerStats* pStats = g_pInterfaceMgr->GetPlayerStats();
	if (!pStats) return LTFALSE;

	FreeSurfaces();

	// Every weapon the player carries that has a command id, in id order -
	// the same set the chooser walks with next/previous.
	const int nNum = g_pWeaponMgr->GetNumWeapons();
	for (int id = 0; id < nNum && m_nSlots < VRWHEEL_MAX_SLOTS; id++)
	{
		if (!pStats->HaveWeapon((uint8)id)) continue;
		if (g_pWeaponMgr->GetCommandId(id) < 0) continue;
		WEAPON* pW = g_pWeaponMgr->GetWeapon(id);
		if (!pW) continue;
		const int k = m_nSlots++;
		m_nWeapon[k] = (uint8)id;
		m_hSurf[k] = g_pLTClient->CreateSurfaceFromBitmap(pW->szIcon);
		if (m_hSurf[k])
		{
			g_pLTClient->OptimizeSurface(m_hSurf[k], SETRGB_T(255,0,255));
			g_pLTClient->GetSurfaceDims(m_hSurf[k], &m_nSurfW[k], &m_nSurfH[k]);
		}
	}

	if (m_nSlots < 1) { VRLog::Msg("VR wheel: nothing to choose from"); return LTFALSE; }

	// Start on the weapon in the hand, so a click-click is a no-op.
	m_nHighlight = -1;
	const int nCur = g_pGameClientShell->GetWeaponModel()->GetWeaponId();
	for (int k = 0; k < m_nSlots; k++)
		if (m_nWeapon[k] == nCur) m_nHighlight = k;
	m_bDeflected = LTFALSE;
	m_bIsOpen = LTTRUE;
	m_fOpenTime = g_pLTClient->GetTime();
	if (m_hName) { g_pLTClient->FreeString(m_hName); m_hName = LTNULL; }
	if (m_nHighlight >= 0)
	{
		WEAPON* pW = g_pWeaponMgr->GetWeapon(m_nWeapon[m_nHighlight]);
		if (pW) m_hName = g_pLTClient->FormatString(pW->nNameId);
	}
	g_pClientSoundMgr->PlayInterfaceSound((char*)g_pInterfaceResMgr->GetSoundSelect());
	VRLog::Msg("VR wheel: open with %d weapons, in hand %d", m_nSlots, nCur);
	return LTTRUE;
}

void CVRWeaponWheel::Close(LTBOOL bSelect)
{
	if (!m_bIsOpen) return;
	m_bIsOpen = LTFALSE;
	if (bSelect && m_nHighlight >= 0 && m_nHighlight < m_nSlots)
	{
		const int nId = m_nWeapon[m_nHighlight];
		const int nCmd = g_pWeaponMgr->GetCommandId(nId);
		VRLog::Msg("VR wheel: selected weapon %d (command %d)", nId, nCmd);
		if (nCmd >= 0 && nId != g_pGameClientShell->GetWeaponModel()->GetWeaponId())
			g_pGameClientShell->GetWeaponModel()->ChangeWeapon((uint8)nCmd);
	}
	else VRLog::Msg("VR wheel: closed without a choice");
	FreeSurfaces();
	if (m_hName) { g_pLTClient->FreeString(m_hName); m_hName = LTNULL; }
}

void CVRWeaponWheel::Update(float fStickX, float fStickY)
{
	if (!m_bIsOpen || m_nSlots < 1) return;
	const float fMag = (float)sqrt(fStickX * fStickX + fStickY * fStickY);
	{
		// The stick as the wheel sees it, on every crossing of its thresholds.
		static int s_nZone = -1;
		const int nZone = (fMag > 0.5f) ? 2 : (fMag < 0.25f) ? 0 : 1;
		if (nZone != s_nZone)
		{
			s_nZone = nZone;
			VRLog::Msg("VR wheel: stick %.2f,%.2f mag %.2f -> zone %d (deflected %d, highlight %d)",
				fStickX, fStickY, fMag, nZone, (int)m_bDeflected, m_nHighlight);
		}
	}
	if (fMag > 0.5f)
	{
		// Slot 0 at the top, clockwise. OpenXR sticks: +Y is up, +X right.
		float fAng = (float)atan2((double)fStickX, (double)fStickY);	// 0 = up, +ve = right
		if (fAng < 0.0f) fAng += 6.2831853f;
		const float fStep = 6.2831853f / (float)m_nSlots;
		int k = (int)floor((fAng + fStep * 0.5f) / fStep) % m_nSlots;
		if (k < 0) k += m_nSlots;
		if (k != m_nHighlight)
		{
			m_nHighlight = k;
			if (m_hName) { g_pLTClient->FreeString(m_hName); m_hName = LTNULL; }
			WEAPON* pW = g_pWeaponMgr->GetWeapon(m_nWeapon[k]);
			if (pW) m_hName = g_pLTClient->FormatString(pW->nNameId);
			g_pClientSoundMgr->PlayInterfaceSound((char*)g_pInterfaceResMgr->GetSoundChange());
		}
		m_bDeflected = LTTRUE;
	}
	else if (fMag < 0.25f && m_bDeflected)
	{
		// The flick: out to a slot and back to centre is the choice.
		Close(LTTRUE);
	}
}

void CVRWeaponWheel::Draw()
{
	if (!m_bIsOpen || m_nSlots < 1) return;

	HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	uint32 nScreenW = 0, nScreenH = 0;
	g_pLTClient->GetSurfaceDims(hScreen, &nScreenW, &nScreenH);
	if (!nScreenW || !nScreenH) return;
	const float yRatio = (float)nScreenH / 480.0f;
	{
		static int s_nSaidDims = 0;
		if (s_nSaidDims++ < 2)
			VRLog::Msg("VR wheel: screen surface %ux%u, interface says %ux%u (ratio %.2f)",
				nScreenW, nScreenH, g_pInterfaceResMgr->GetScreenWidth(),
				g_pInterfaceResMgr->GetScreenHeight(), g_pInterfaceResMgr->GetYRatio());
	}

	// SIZED FOR THE EYE, NOT THE SCREEN. The HUD is drawn on the logical
	// screen (2560x1384, both eyes wide) and the renderer fits that width
	// into the room each eye has beside its optical centre - about 0.38 of
	// logical size (render2d: fKx = room * eyeW / screenW). A ring a quarter
	// of the height here came out a tenth in the headset (desk-measured).
	// ...and the ring cannot leave the logical screen: anything past its top
	// or bottom edge is clipped before the eye ever sees it (desk: the top
	// and bottom slots of a six-slot wheel simply missing). So the ring is as
	// large as the logical height allows - radius 0.36 with icons 0.22, the
	// highlighted one 0.27 - which lands at about a seventh of the eye. A
	// bigger wheel needs a bigger HUD layer, which is the renderer's fit and
	// not this file's. VRWheelSize scales all of it.
	const float fSize = (g_vtVRWheelSize.GetFloat(1.0f) > 0.1f) ? g_vtVRWheelSize.GetFloat(1.0f) : 1.0f;
	const int cx = (int)nScreenW / 2;
	const int cy = (int)nScreenH / 2;
	const int nRadius = (int)(nScreenH * 0.36f * fSize);
	const int nIcon   = (int)(nScreenH * 0.22f * fSize);
	const int nIconHi = (int)(nScreenH * 0.27f * fSize);

	const LTVector vHi = g_pLayoutMgr->GetChooserHighlightColor();

	for (int k = 0; k < m_nSlots; k++)
	{
		const float fAng = 6.2831853f * (float)k / (float)m_nSlots;	// 0 = up, clockwise
		const int x = cx + (int)(nRadius * sin(fAng));
		const int y = cy - (int)(nRadius * cos(fAng));
		const int n = (k == m_nHighlight) ? nIconHi : nIcon;
		LTRect rc(x - n / 2, y - n / 2, x + n / 2, y + n / 2);
		if (k == m_nHighlight)
		{
			LTRect rb(rc.left - 4, rc.top - 4, rc.right + 4, rc.bottom + 4);
			g_pOptimizedRenderer->FillRect(hScreen, &rb, SETRGB_T(vHi.x, vHi.y, vHi.z));
		}
		if (m_hSurf[k])
			g_pLTClient->ScaleSurfaceToSurfaceTransparent(hScreen, m_hSurf[k], &rc, LTNULL, SETRGB_T(255,0,255));
	}

	// The centre dot, and the highlighted weapon's name under it.
	{
		const int d = (int)(nScreenH * 0.008f * fSize);
		LTRect rd(cx - d, cy - d, cx + d, cy + d);
		g_pOptimizedRenderer->FillRect(hScreen, &rd, SETRGB_T(255, 255, 255));
	}
	CLTGUIFont* pFont = (g_vtVRMenuBigSubs.GetFloat(1.0f) > 0.0f)
		? g_pInterfaceResMgr->GetLargeFont() : g_pInterfaceResMgr->GetChooserFont();
	if (pFont && m_hName)
		pFont->Draw(m_hName, hScreen, cx, cy + (int)(nScreenH * 0.06f * fSize), LTF_JUSTIFY_CENTER, kWhite);
}

// One console variable, a global default, and an optional override per weapon.
//
// Every weapon in NOLF sits in the hand differently. A single VRWeaponDist or
// VRWeaponScale is necessarily wrong for thirty-one of the thirty-two, and
// thirty-two separate variables would be unreadable and unmaintainable.
//
// So one name resolves against an optional "@<weapon>" suffix:
//
//     VRWeaponScale             the default, for every weapon
//     VRWeaponScale@coin        this weapon only
//     VRAngleYaw@revolver       a per-weapon angle trim
//
// and a per-weapon value is one more console variable - set from the console,
// from vrweapons.cfg, or by the tuner into vrtune.cfg - never a code change.
//
// THE SUFFIX IS DERIVED, NOT MAPPED. It is NOLF's own WEAPON::szName lowercased
// with spaces replaced by underscores:
//
//     "P38"                  -> p38
//     "AK47"                 -> ak47
//     "Grenade launcher"     -> grenade_launcher
//     "Lipstick Impact Bomb" -> lipstick_impact_bomb
//
// The slug comes from the same WeaponMgr record the game has already loaded,
// so there is no mapping table to write and none to keep in step with anything.

#ifndef __VRWeaponVar_H__
#define __VRWeaponVar_H__

#include "iltclient.h"
#include "WeaponMgr.h"
#include <string.h>
#include <stdio.h>

extern CWeaponMgr* g_pWeaponMgr;

// "Lipstick Impact Bomb" -> "lipstick_impact_bomb". Anything that is not a
// letter or digit becomes an underscore, so a name none of us has thought about
// still produces a legal, predictable variable name rather than a broken one.
inline void VRWeaponSlug(const char* pszName, char* pszOut, int nOutSize)
{
    int o = 0;
    if (!pszName || nOutSize < 1) { if (nOutSize > 0) pszOut[0] = 0; return; }

    for (int i = 0; pszName[i] && o < nOutSize - 1; ++i)
    {
        const char c = pszName[i];
        if      (c >= 'A' && c <= 'Z') pszOut[o++] = (char)(c - 'A' + 'a');
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) pszOut[o++] = c;
        else                                                       pszOut[o++] = '_';
    }
    pszOut[o] = 0;
}

// The slug for a weapon id, or an empty string if the id is not a real weapon.
inline void VRWeaponSlugForId(int nWeaponId, char* pszOut, int nOutSize)
{
    if (nOutSize > 0) pszOut[0] = 0;
    if (!g_pWeaponMgr || nWeaponId == WMGR_INVALID_ID) return;

    WEAPON* pW = g_pWeaponMgr->GetWeapon((uint8)nWeaponId);
    if (!pW) return;

    VRWeaponSlug(pW->szName, pszOut, nOutSize);
}

// A base name plus a per-weapon override, resolved once per weapon change.
//
// The lookup is cached against the weapon id rather than repeated per frame.
// Not for speed - a console lookup is cheap - but so that the resolved name can
// be LOGGED once when it changes. A per-weapon override that silently does not
// exist looks exactly like one that does nothing, and this project has lost
// whole test rounds to modes that were never actually in force.
class VRWeaponVar
{
public:
    VRWeaponVar() : m_pszBase(NULL), m_fDefault(0.0f), m_nCachedId(-2),
                    m_bOverride(false), m_bAdditive(false) {}

    // bAdditive: the override is ADDED to the global rather than replacing it.
    //
    // For a scale, replace is right - a size is absolute. For the ANGLES the
    // override is a delta on purpose.
    //
    // A per-weapon angle is a trim from a neutral hand pose, and the neutral
    // is wherever the game's own authored weapon offset puts the model, which
    // is the same for every weapon. So a tuned angle is two things at once: an
    // offset common to all of them, and the difference BETWEEN weapons, which
    // is a property of the models themselves.
    //
    // Additive keeps that difference intact and leaves the common offset on the
    // global, where one console variable corrects all 21 weapons at once.
    // Under replace semantics there would be no such knob: tuning the global
    // would move only the weapons nobody had tuned.
    void Init(const char* pszBase, float fDefault, bool bAdditive = false)
    {
        m_pszBase   = pszBase;
        m_fDefault  = fDefault;
        m_nCachedId = -2;
        m_bOverride = false;
        m_bAdditive = bAdditive;
    }

    // The value for this weapon: the override if one exists, otherwise the
    // global, otherwise the compiled-in default. In additive mode, the global
    // plus the override.
    float Get(int nWeaponId)
    {
        if (!m_pszBase || !g_pLTClient) return m_fDefault;

        if (nWeaponId != m_nCachedId)
        {
            m_nCachedId = nWeaponId;
            m_bOverride = false;
            m_szResolved[0] = 0;

            char szSlug[64];
            VRWeaponSlugForId(nWeaponId, szSlug, sizeof(szSlug));

            if (szSlug[0])
            {
                sprintf(m_szResolved, "%s@%s", m_pszBase, szSlug);
                m_bOverride = (g_pLTClient->GetConsoleVar(m_szResolved) != LTNULL);
            }

            if (!m_bOverride) sprintf(m_szResolved, "%s", m_pszBase);
        }

        HCONSOLEVAR hVar = g_pLTClient->GetConsoleVar(m_szResolved);
        if (!hVar) return m_fDefault;

        const float fVal = g_pLTClient->GetVarValueFloat(hVar);

        if (m_bAdditive && m_bOverride)
        {
            // The 2001 interface takes char*, not const char*, and does not
            // write through it.
            HCONSOLEVAR hBase = g_pLTClient->GetConsoleVar((char*)m_pszBase);
            return fVal + (hBase ? g_pLTClient->GetVarValueFloat(hBase) : 0.0f);
        }

        return fVal;
    }

    // Which variable actually answered, for the log.
    const char* ResolvedName() const { return m_szResolved; }
    bool        UsingOverride() const { return m_bOverride; }

private:
    const char* m_pszBase;
    float       m_fDefault;
    int         m_nCachedId;
    bool        m_bOverride;
    bool        m_bAdditive;
    char        m_szResolved[128];
};

#endif // __VRWeaponVar_H__

// ----------------------------------------------------------------------- //
//
// MODULE  : VRPrims.cpp
//
// PURPOSE : See VRPrims.h. Four sources, one frame:
//
//   PARTICLE SYSTEMS - every OT_PARTICLESYSTEM near the eye. The engine keeps
//   the particles (GetParticles walks them) in the SYSTEM's frame, so each
//   is carried through the system's position and rotation. One POINTS run
//   per system; the renderer faces the quads per eye.
//
//   LINE SYSTEMS - rain. Two coloured points per line, in the system's
//   frame as well.
//
//   POLYGRIDS - water. The engine holds a byte height field and a colour
//   table; the FX code animates the bytes. Triangulated here into a TRIS run
//   with the sprite the level named as its surface.
//
//   CANVASES - CBasePolyDrawFX keeps one engine canvas for every poly-line
//   effect (vehicle trails, the zip cord, breaking glass). The retail
//   renderer called its draw function with a D3D7 ILTCustomDraw; this file
//   calls it with one that records the polygons instead.
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "VRPrims.h"
#include "VRShared.h"
#include "VRLog.h"
#include "GameClientShell.h"
#include "SFXMgr.h"
#include "PolyGridFX.h"
#include "SFXMsgIds.h"
#include "iltclient.h"
#include "iltcustomdraw.h"
#include "BasePolyDrawFX.h"
#include "VarTrack.h"
#include "CommonUtilities.h"		// g_pPhysicsLT
#include "GameClientShell.h"		// the camera object
#include <windows.h>
#include <string.h>
#include <math.h>

extern ILTClient* g_pLTClient;
extern double VRMuzzleFlashLastShowTime();
extern HLOCALOBJ VRMuzzleFlashLastObject();

VarTrack g_vtVRPrims;				// 0 turns the whole channel off
VarTrack g_vtVRParticleScale;		// world half-width per engine size unit
VarTrack g_vtVRPolyGridHeight;		// height scale for the water surface
VarTrack g_vtVRPolyGridTexRepeat;	// texture repeats across a water grid, on top of the engine's scale
VarTrack g_vtVRPolyGridPan;			// texture pan per second per pan unit
VarTrack g_vtVRPolyGridDiffuseAlpha;	// 1: a water surface's opacity is its vertex alpha alone, not times the texture's alpha
VarTrack g_vtVRPolyGridRipple;		// texture-coordinate ripple per full height byte
static VRViewRebase s_Rebase;		// see VRPrims.h
VarTrack g_vtVRGunFrameBody;		// VRGunFrameBody <0-1>: gun-frame offsets through the camera basis

namespace
{
	// ---- what the engine will not tell us back ------------------------
	//
	// SetupParticleSystem and SetPolyGridTexture are plain function-pointer
	// members of ILTClient. The client calls them through g_pLTClient, so
	// swapping the pointer catches every caller.
	struct TexRec { HLOCALOBJ h; char szTex[64]; float fRadius; uint32 nStamp; };
	const int kRecs = 512;
	TexRec s_PS[kRecs];
	TexRec s_PG[kRecs];
	uint32 s_nStamp = 0;

	TexRec* FindRec(TexRec* pTab, HLOCALOBJ h, bool bMake)
	{
		TexRec* pOld = &pTab[0];
		for (int i = 0; i < kRecs; ++i)
		{
			if (pTab[i].h == h) return &pTab[i];
			if (pTab[i].nStamp < pOld->nStamp) pOld = &pTab[i];
		}
		if (!bMake) return LTNULL;
		pOld->h = h; pOld->szTex[0] = 0; pOld->fRadius = 0.0f;
		return pOld;
	}

	typedef LTRESULT (*SetupPSFn)(HLOCALOBJ, char*, float, uint32, float);
	typedef LTRESULT (*SetPGTexFn)(HLOCALOBJ, char*);
	SetupPSFn  s_pfnSetupPS = LTNULL;
	SetPGTexFn s_pfnSetPGTex = LTNULL;

	// ---- the skins an object is CREATED with --------------------------
	//
	// 12 September: the menus drew in the wrong colours because the renderer
	// took a model's skin from the engine's texture heap ADDRESS, and the menu
	// recycles those between its many small models. The client is the one
	// that names the skin, right here, in the ObjectCreateStruct - so every
	// CreateObject that names a model and a skin hands the names to the
	// renderer, keyed by the object handle. Wrapping the function-pointer
	// member catches all thirty call sites at once, the menu art included.
	typedef HLOCALOBJ (*CreateObjectFn)(ObjectCreateStruct*);
	typedef void (__cdecl* VRPubSkinFn)(uint32, const char*, const char*, const char*);
	CreateObjectFn s_pfnCreateObject = LTNULL;
	long s_nSkinsPublished = 0;

	// THE TABLE OUTLIVES THE RENDERER. The engine frees and reloads the
	// render DLL on every focus change and once more at startup, after the
	// main menu's models already exist, and the renderer's own map goes with
	// it. So the names live here and the whole table is handed over again
	// every few frames: a reload costs a few frames of the old guess, not a
	// session of it. 512 entries, oldest overwritten; a recycled handle is
	// overwritten by the CreateObject that recycles it, and the renderer
	// only honours an entry whose model filename still agrees.
	struct SkinRec { HLOCALOBJ h; char szModel[80]; char szSkin[2][80]; uint32 nStamp; };
	const int kSkinRecs = 512;
	SkinRec s_Skins[kSkinRecs];
	uint32 s_nSkinStamp = 0;
	VRPubSkinFn s_pfnPubSkin = LTNULL;
	HMODULE s_hPubSkinSeen = (HMODULE)(uintptr_t)1;

	VRPubSkinFn PubSkinEntry()
	{
		HMODULE hm = GetModuleHandleA("d3dstub.ren");
		if (hm != s_hPubSkinSeen)
		{
			s_hPubSkinSeen = hm;
			s_pfnPubSkin = hm ? (VRPubSkinFn)GetProcAddress(hm, "R3D_PublishObjectSkins") : LTNULL;
			VRLog::Msg("VRSkins: publish entry %s", s_pfnPubSkin ? "resolved" : "missing");
		}
		return s_pfnPubSkin;
	}

	HLOCALOBJ MyCreateObject(ObjectCreateStruct* p)
	{
		HLOCALOBJ h = s_pfnCreateObject ? s_pfnCreateObject(p) : LTNULL;
		if (h && p && p->m_ObjectType == OT_MODEL && p->m_Filename[0] && p->m_SkinNames[0][0])
		{
			SkinRec* pOld = &s_Skins[0]; SkinRec* pRec = LTNULL;
			for (int i = 0; i < kSkinRecs; ++i)
			{
				if (s_Skins[i].h == h) { pRec = &s_Skins[i]; break; }
				if (s_Skins[i].nStamp < pOld->nStamp) pOld = &s_Skins[i];
			}
			if (!pRec) pRec = pOld;
			pRec->h = h; pRec->nStamp = ++s_nSkinStamp;
			strncpy(pRec->szModel, p->m_Filename, sizeof pRec->szModel - 1); pRec->szModel[sizeof pRec->szModel - 1] = 0;
			strncpy(pRec->szSkin[0], p->m_SkinNames[0], sizeof pRec->szSkin[0] - 1); pRec->szSkin[0][sizeof pRec->szSkin[0] - 1] = 0;
			strncpy(pRec->szSkin[1], p->m_SkinNames[1], sizeof pRec->szSkin[1] - 1); pRec->szSkin[1][sizeof pRec->szSkin[1] - 1] = 0;
			VRPubSkinFn pfn = PubSkinEntry();
			if (pfn)
			{
				pfn((uint32)(uintptr_t)h, pRec->szModel, pRec->szSkin[0], pRec->szSkin[1]);
				if (++s_nSkinsPublished <= 12)
					VRLog::Msg("VRSkins: object %08X %s -> %s%s%s", (unsigned)(uintptr_t)h,
							   pRec->szModel, pRec->szSkin[0],
							   pRec->szSkin[1][0] ? " + " : "", pRec->szSkin[1]);
			}
		}
		return h;
	}

	// Every 15th frame, the whole table again. See SkinRec.
	void RepublishSkins()
	{
		static uint32 s_nCall = 0;
		if ((++s_nCall % 15) != 0) return;
		VRPubSkinFn pfn = PubSkinEntry();
		if (!pfn) return;
		for (int i = 0; i < kSkinRecs; ++i)
			if (s_Skins[i].h)
				pfn((uint32)(uintptr_t)s_Skins[i].h, s_Skins[i].szModel, s_Skins[i].szSkin[0], s_Skins[i].szSkin[1]);
	}

	LTRESULT MySetupParticleSystem(HLOCALOBJ hObj, char* pTex, float fGrav, uint32 nFlags, float fRadius)
	{
		TexRec* p = FindRec(s_PS, hObj, true);
		if (p)
		{
			p->nStamp = ++s_nStamp;
			p->fRadius = fRadius;
			if (pTex) strncpy(p->szTex, pTex, sizeof p->szTex - 1); else p->szTex[0] = 0;
			p->szTex[sizeof p->szTex - 1] = 0;
		}
		return s_pfnSetupPS ? s_pfnSetupPS(hObj, pTex, fGrav, nFlags, fRadius) : LT_ERROR;
	}
	LTRESULT MySetPolyGridTexture(HLOCALOBJ hObj, char* pTex)
	{
		TexRec* p = FindRec(s_PG, hObj, true);
		if (p)
		{
			p->nStamp = ++s_nStamp;
			if (pTex) strncpy(p->szTex, pTex, sizeof p->szTex - 1); else p->szTex[0] = 0;
			p->szTex[sizeof p->szTex - 1] = 0;
		}
		return s_pfnSetPGTex ? s_pfnSetPGTex(hObj, pTex) : LT_ERROR;
	}

	// ---- the frame ------------------------------------------------------
	VRPrimFrame s_frame;			// ~5.4 MB, static
	uint32 s_nDroppedRuns = 0, s_nDroppedVerts = 0;

	VRPrimRun* BeginRun(uint32 nType, uint32 nFlags, const char* pszTex, uint32 nKind, HLOCALOBJ h)
	{
		if (s_frame.nRuns >= VRPRIM_MAX_RUNS) { ++s_nDroppedRuns; return LTNULL; }
		VRPrimRun& r = s_frame.runs[s_frame.nRuns];
		memset(&r, 0, sizeof r);
		r.nType = nType; r.nFlags = nFlags; r.nKind = nKind;
		r.nObject = (uint32)(uintptr_t)h;
		r.nStart = s_frame.nVerts; r.nCount = 0;
		if (pszTex && pszTex[0]) strncpy(r.szTex, pszTex, sizeof r.szTex - 1);
		else r.nFlags |= VRPRIM_F_NOTEX;
		return &r;
	}
	VRPrimVert* AddVert(VRPrimRun* r)
	{
		if (!r || s_frame.nVerts >= VRPRIM_MAX_VERTS) { ++s_nDroppedVerts; return LTNULL; }
		VRPrimVert* v = &s_frame.verts[s_frame.nVerts++];
		++r->nCount;
		return v;
	}
	void EndRun(VRPrimRun* r, const LTVector& vCentre)
	{
		if (!r) return;
		if (!r->nCount) { s_frame.nVerts = r->nStart; return; }	// nothing in it
		r->fCentre[0] = vCentre.x; r->fCentre[1] = vCentre.y; r->fCentre[2] = vCentre.z;
		++s_frame.nRuns;
	}

	uint32 BlendFlagsOf(HLOCALOBJ h)
	{
		uint32 dwF2 = 0, nOut = 0;
		g_pLTClient->Common()->GetObjectFlags(h, OFT_Flags2, dwF2);
		if (dwF2 & FLAG2_ADDITIVE) nOut |= VRPRIM_F_ADDITIVE;
		else if (dwF2 & FLAG2_MULTIPLY) nOut |= VRPRIM_F_MULTIPLY;
		return nOut;
	}

	// ---- particle systems ----------------------------------------------
	uint32 s_nPSSeen = 0, s_nPSNoTex = 0, s_nParticles = 0;
	// Camera-relative particle systems DROPPED for want of a gun pose, and
	// the ones saved by last frame's. The muzzle flash is one of these.
	uint32 s_nPSCloseDropped = 0, s_nPSCloseLast = 0;
	// Camera-relative particle systems placed on the DRAWN barrel end rather
	// than through the fK re-base. Counted so its silence is never mistaken for
	// success: zero here with a gun in hand means the flash is still going
	// through the old path.
	uint32 s_nPSCloseMuzzle = 0;
	// Camera-relative prim objects that reached the publish step at all.
	uint32 s_nCloseKept = 0;
	// WHY a dynamic light did not make it. NOLF's first-person muzzle flash
	// is a 7.5 ms particle puff plus a 200-unit light, and the light is most
	// of what a player actually sees of it.
	uint32 s_nLightsSeen = 0, s_nLightsHidden = 0, s_nLightsTiny = 0, s_nLightsFar = 0;
	// Did the object query return everything, or hit its cap?
	uint32 s_nObjOut = 0, s_nObjFound = 0;
	uint32 s_nCloseAny = 0, s_nPSHidden = 0;
	// Lights that reached the renderer only because they were handed to us.
	uint32 s_nLightsExtra = 0;
	// Objects handed to us by the client - see VRPrims_AddExtra.
	const int  kExtras = 8;
	HLOCALOBJ  s_Extra[kExtras];
	int        s_nExtra = 0;
	uint32     s_nExtraPublished = 0, s_nExtraNull = 0, s_nExtraHidden = 0;
	uint32     s_nExtraOffered = 0;

	void PublishParticleSystem(HLOCALOBJ h)
	{
		++s_nPSSeen;
		TexRec* pRec = FindRec(s_PS, h, false);
		if (!pRec || !pRec->szTex[0]) { ++s_nPSNoTex; return; }
		LTVector vPos; g_pLTClient->GetObjectPos(h, &vPos);
		LTRotation rRot; g_pLTClient->GetObjectRotation(h, &rRot);
		LTVector vU, vR, vF;
		g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);
		float cr = 1.0f, cg = 1.0f, cb = 1.0f, ca = 1.0f;
		g_pLTClient->GetObjectColor(h, &cr, &cg, &cb, &ca);
		float fScale = g_vtVRParticleScale.GetFloat();
		// The first-person muzzle smoke is camera-relative: onto the VR gun.
		float fUnit = 1.0f;
		const bool bClose = (g_pLTClient->GetObjectFlags(h) & FLAG_REALLYCLOSE) != 0;
		if (bClose)
		{
			// LAST FRAME'S GUN POSE WILL DO, AND IT HAS TO.
			//
			// s_Rebase is filled when the VIEW WEAPON is published, and the
			// view weapon is published LAST - so everything that runs before
			// it, this included, saw bKnown false and RETURNED. A returned
			// muzzle flash is not a misplaced muzzle flash: it is no muzzle
			// flash at all, which is what headset testing reported on 11 September and
			// what three desk runs then failed to photograph.
			//
			// A pose one frame old is about 11 ms stale, which nothing can
			// see. The count of each case is reported below so this can never
			// again be a silent drop.
			const VRViewRebase& rbUse = s_Rebase.bKnown ? s_Rebase : VRPrims_RebaseLast();
			if (!rbUse.bKnown) { ++s_nPSCloseDropped; return; }
			if (!s_Rebase.bKnown) ++s_nPSCloseLast;
			// THE BARREL END IF WE HAVE IT. with
			// the tracer fixed, the trail came out of the barrel but the muzzle
			// flash did not. For this weapon the flash is not a model at all -
			// the diagnostic reads "scale no particles yes light yes" - so it
			// never went through the model path, it comes through here, and the
			// re-base expands it by fK (17) exactly as the tracer used to be.
			//
			// A camera-relative particle system IS the first-person muzzle
			// effect; the comment at the top of this branch has said so since it
			// was written. So the drawn barrel end is where it belongs, and it
			// is the same point the tracer now uses, which is the whole reason
			// they should agree.
			// MEASURED AND REVERTED, 19 September: placing these on the drawn
			// barrel end was tried here and the counter read 0 - no muzzle
			// particle system in this build carries FLAG_REALLYCLOSE, so this
			// branch never runs for the flash and the change was a no-op. The
			// counter is kept so the next person can see that for themselves
			// rather than re-deriving it. If it ever reads non-zero with a gun
			// in hand, this is the place to put the barrel end.
			vPos = s_Rebase.bKnown ? VRPrims_RebasePoint(vPos)
								   : VRPrims_RebasePointLast(vPos);
			g_pLTClient->GetRotationVectors(const_cast<LTRotation*>(&rbUse.rGunWorld), &vU, &vR, &vF);
			fUnit = rbUse.fK;
			fScale *= fUnit;
		}

		LTParticle *pCur = LTNULL, *pTail = LTNULL;
		if (!g_pLTClient->GetParticles(h, &pCur, &pTail)) return;
		VRPrimRun* r = BeginRun(VRPRIM_T_POINTS, BlendFlagsOf(h), pRec->szTex, 0, h);
		if (!r) return;
		int nGuard = 0;
		while (pCur && pCur != pTail && nGuard++ < 8192)
		{
			LTVector p; g_pLTClient->GetParticlePos(h, pCur, &p);
			p *= fUnit;
			VRPrimVert* v = AddVert(r);
			if (!v) break;
			v->fPos[0] = vPos.x + vR.x * p.x + vU.x * p.y + vF.x * p.z;
			v->fPos[1] = vPos.y + vR.y * p.x + vU.y * p.y + vF.y * p.z;
			v->fPos[2] = vPos.z + vR.z * p.x + vU.z * p.y + vF.z * p.z;
			v->fColour[0] = (pCur->m_Color.x / 255.0f) * cr;
			v->fColour[1] = (pCur->m_Color.y / 255.0f) * cg;
			v->fColour[2] = (pCur->m_Color.z / 255.0f) * cb;
			v->fColour[3] = pCur->m_Alpha * ca;
			v->fUV[0] = 0.0f; v->fUV[1] = 0.0f;
			v->fSize = pCur->m_Size * fScale;
			pCur = pCur->m_pNext;
		}
		s_nParticles += r->nCount;
		// THE FLASH'S OWN PARTICLE SYSTEM, as drawn. The one log line that says
		// whether a particle-flash gun's burst reached the renderer, where it
		// went, and how big it is - so a missing flash is diagnosed from the
		// log and not from a still.
		if (h == VRMuzzleFlashLastObject())
		{
			static uint32 s_nFlashPSFrames = 0;
			if ((s_nFlashPSFrames++ % 30) == 0)
			{
				const VRPrimVert* v0 = r->nCount ? &s_frame.verts[r->nStart] : LTNULL;
				VRLog::Msg("VRFlashPS: flash particles %s, %u particles, system at %.0f %.0f %.0f"
						   " (eye %.0f %.0f %.0f, gun %.0f %.0f %.0f)%s | first at %.0f %.0f %.0f size %.1f alpha %.2f | tex '%s'",
						   bClose ? "CAMERA-RELATIVE (re-based)" : "world",
						   r->nCount, vPos.x, vPos.y, vPos.z,
						   s_Rebase.vEye.x, s_Rebase.vEye.y, s_Rebase.vEye.z,
						   s_Rebase.vGunWorld.x, s_Rebase.vGunWorld.y, s_Rebase.vGunWorld.z,
						   "",
						   v0 ? v0->fPos[0] : 0.0f, v0 ? v0->fPos[1] : 0.0f, v0 ? v0->fPos[2] : 0.0f,
						   v0 ? v0->fSize : 0.0f, v0 ? v0->fColour[3] : 0.0f, pRec->szTex);
			}
		}
		// ONE SYSTEM, WATCHED. The engine moved the particles under the retail
		// renderer; whether it still does with ours is a question this line
		// answers: the first system's first particle every 30 frames.
		{
			static uint32 s_nWatchFrames = 0;
			static HLOCALOBJ s_hWatch = LTNULL;
			static bool s_bWatchClose = false;
			// A camera-relative system (the player's own muzzle smoke) is the
			// one worth watching when there is one: it shows the re-basing.
			if (bClose && !s_bWatchClose) { s_hWatch = h; s_bWatchClose = true; }
			if (!s_hWatch || s_hWatch == h)
			{
				s_hWatch = h;
				if ((s_nWatchFrames++ % 30) == 0 && r->nCount)
				{
					const VRPrimVert& v0 = s_frame.verts[r->nStart];
					VRLog::Msg("VRPrims: watched system %08X '%s'%s - %u particles, first at %.0f %.0f %.0f"
						" size %.1f alpha %.2f colour %.2f %.2f %.2f, system at %.0f %.0f %.0f%s",
						(unsigned)(uintptr_t)h, pRec->szTex, bClose ? " (CAMERA-RELATIVE, re-based onto the gun)" : "",
						r->nCount, v0.fPos[0], v0.fPos[1], v0.fPos[2], v0.fSize, v0.fColour[3],
						v0.fColour[0], v0.fColour[1], v0.fColour[2], vPos.x, vPos.y, vPos.z,
						bClose ? "" : "");
					if (bClose)
						VRLog::Msg("VRPrims:   the gun is at %.0f %.0f %.0f (retail cam-relative %.2f %.2f %.2f, K %.0f), the eye at %.0f %.0f %.0f",
							s_Rebase.vGunWorld.x, s_Rebase.vGunWorld.y, s_Rebase.vGunWorld.z,
							s_Rebase.vGunCam.x, s_Rebase.vGunCam.y, s_Rebase.vGunCam.z, s_Rebase.fK,
							s_Rebase.vEye.x, s_Rebase.vEye.y, s_Rebase.vEye.z);
				}
			}
			else if ((s_nWatchFrames % 300) == 0) s_hWatch = LTNULL;	// let it move on
		}
		EndRun(r, vPos);
	}

	// ---- line systems ---------------------------------------------------
	uint32 s_nLSSeen = 0, s_nLines = 0;

	void PublishLineSystem(HLOCALOBJ h)
	{
		++s_nLSSeen;
		LTVector vPos; g_pLTClient->GetObjectPos(h, &vPos);
		LTRotation rRot; g_pLTClient->GetObjectRotation(h, &rRot);
		LTVector vU, vR, vF;
		g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);
		VRPrimRun* r = BeginRun(VRPRIM_T_LINES, BlendFlagsOf(h), LTNULL, 1, h);
		if (!r) return;
		HLTLINE hL = g_pLTClient->GetNextLine(h, LTNULL);
		int nGuard = 0;
		while (hL && nGuard++ < 8192)
		{
			LTLine ln; g_pLTClient->GetLineInfo(hL, &ln);
			for (int k = 0; k < 2; ++k)
			{
				VRPrimVert* v = AddVert(r);
				if (!v) { hL = LTNULL; break; }
				const LTVector& p = ln.m_Points[k].m_Pos;
				v->fPos[0] = vPos.x + vR.x * p.x + vU.x * p.y + vF.x * p.z;
				v->fPos[1] = vPos.y + vR.y * p.x + vU.y * p.y + vF.y * p.z;
				v->fPos[2] = vPos.z + vR.z * p.x + vU.z * p.y + vF.z * p.z;
				v->fColour[0] = ln.m_Points[k].r; v->fColour[1] = ln.m_Points[k].g;
				v->fColour[2] = ln.m_Points[k].b; v->fColour[3] = ln.m_Points[k].a;
				v->fUV[0] = v->fUV[1] = 0.0f; v->fSize = 0.0f;
			}
			if (!hL) break;
			hL = g_pLTClient->GetNextLine(h, hL);
		}
		s_nLines += r->nCount / 2;
		EndRun(r, vPos);
	}

	// ---- polygrids ------------------------------------------------------
	uint32 s_nPGSeen = 0, s_nPGNoTex = 0;
	bool s_bPGSaid = false;

	void PublishPolyGrid(HLOCALOBJ h, float fTime)
	{
		++s_nPGSeen;
		char* pBytes = LTNULL; uint32 nW = 0, nH = 0; PGColor* pTable = LTNULL;
		if (g_pLTClient->GetPolyGridInfo(h, &pBytes, &nW, &nH, &pTable) != LT_OK) return;
		if (!pBytes || !pTable || nW < 2 || nH < 2 || nW > 256 || nH > 256) return;
		TexRec* pRec = FindRec(s_PG, h, false);
		const char* pszTex = (pRec && pRec->szTex[0]) ? pRec->szTex : LTNULL;
		if (!pszTex) ++s_nPGNoTex;
		LTVector vPos; g_pLTClient->GetObjectPos(h, &vPos);
		LTRotation rRot; g_pLTClient->GetObjectRotation(h, &rRot);
		LTVector vU, vR, vF;
		g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);
		LTVector vScale(1.0f, 1.0f, 1.0f);
		g_pLTClient->GetObjectScale(h, &vScale);
		float cr = 1.0f, cg = 1.0f, cb = 1.0f, ca = 1.0f;
		g_pLTClient->GetObjectColor(h, &cr, &cg, &cb, &ca);
		float fXPan = 0.0f, fYPan = 0.0f, fXScale = 1.0f, fYScale = 1.0f;
		g_pLTClient->GetPolyGridTextureInfo(h, &fXPan, &fYPan, &fXScale, &fYScale);
		// HOW BIG IS THE GRID, REALLY? The engine hands back a byte array and a
		// scale, and "scale" could mean one cell or the whole sheet. Guessing
		// costs a water surface a quarter too small for its own pool. The
		// object's own DIMS settle it: the grid has to span them.
		LTVector vDims(0.0f, 0.0f, 0.0f);
		if (g_pPhysicsLT) g_pPhysicsLT->GetObjectDims(h, &vDims);
		// IS THE WATER ACTUALLY MOVING? A checksum of the height field, for
		// ONE grid, reported every few seconds with how much it changed.
		//
		// the waterfall did not move. The cause was
		// not in the renderer at all - PolyGridFX::Update returns before it
		// touches the plasma when the PolyGrids setting is off, and that
		// setting did not EXIST in the tester's config, so it read as zero. A frozen
		// height field and a correctly drawn still surface look identical in
		// a screenshot, so the honest instrument is the numbers themselves.
		{
			static HLOCALOBJ s_hWatch = LTNULL;
			static uint32 s_nWas = 0;
			static double s_fSaidAt = -1e9;
			static int s_nSame = 0, s_nMoved = 0;
			if (!s_hWatch) s_hWatch = h;
			if (h == s_hWatch)
			{
				uint32 nSum = 0;
				const uint32 nCells = nW * nH;
				for (uint32 i = 0; i < nCells; ++i)
					nSum = nSum * 31u + (uint32)(uint8)pBytes[i];
				if (nSum == s_nWas) ++s_nSame; else ++s_nMoved;
				s_nWas = nSum;
				const double fNow = VRLog::NowMs() / 1000.0;
				if (fNow - s_fSaidAt > 5.0)
				{
					s_fSaidAt = fNow;
					VRLog::Msg("VRPrims: polygrid motion - of the last %d samples"
							   " %d changed and %d were identical%s",
							   s_nSame + s_nMoved, s_nMoved, s_nSame,
							   // ONE sample that did not change is not a frozen
							   // surface, it is one sample. The first report
							   // fires on the first frame and said FROZEN every
							   // time, which is the kind of false alarm that
							   // teaches a reader to ignore the line.
							   // AND A SETTLED POOL IS NOT A FROZEN ONE EITHER.
							   // This is a checksum of the HEIGHT FIELD, and
							   // NOLF's plasma damps: with nothing disturbing
							   // it - no player in the water, no rain - the
							   // ripples decay and the surface goes flat and
							   // stays flat. That is correct, and it is what a
							   // chain run standing still at a spawn point
							   // produces after fifteen seconds. The waterfall
							   // is a different thing entirely: its motion is
							   // the surface TEXTURE pan, which this cannot
							   // see. So the line now says what it measured
							   // and leaves the reading to the reader.
							   (s_nMoved == 0 && s_nSame >= 30)
								 ? "   <- the height field has not moved at all;"
								   " expected for still water nobody has"
								   " disturbed, wrong if this is a waterfall or"
								   " a pool being walked through"
								 : "");
					s_nSame = 0; s_nMoved = 0;
				}
			}
		}
		// ONCE PER GRID, not once per run: a one-shot hid every grid but the
		// first, and the HQ's waterfall (Water0's rotated surface) could not
		// be told apart from "never published" (13 September).
		static HLOCALOBJ s_hSaid[16]; static int s_nSaid = 0;
		bool bSaidThis = false;
		for (int q = 0; q < s_nSaid; ++q) if (s_hSaid[q] == h) { bSaidThis = true; break; }
		if (!bSaidThis && s_nSaid < 16)
		{
			s_hSaid[s_nSaid++] = h;
			s_bPGSaid = true;
			VRLog::Msg("VRPrims: polygrid %08X is %u x %u, scale %.1f %.1f %.1f, pos %.0f %.0f %.0f,"
				" pan %.2f %.2f, texture scale %.2f %.2f, alpha %.2f, surface '%s'",
				(unsigned)(uintptr_t)h, nW, nH, vScale.x, vScale.y, vScale.z,
				vPos.x, vPos.y, vPos.z, fXPan, fYPan, fXScale, fYScale, ca,
				pszTex ? pszTex : "(none)");
			VRLog::Msg("VRPrims:   its axes: right (%.2f %.2f %.2f) up (%.2f %.2f %.2f) forward (%.2f %.2f %.2f)",
				vR.x, vR.y, vR.z, vU.x, vU.y, vU.z, vF.x, vF.y, vF.z);
			VRLog::Msg("VRPrims:   its dims are %.0f %.0f %.0f (so it spans %.0f x %.0f);"
				" (n-1) x scale = %.0f x %.0f, n x scale = %.0f x %.0f <- this one",
				vDims.x, vDims.y, vDims.z, vDims.x * 2.0f, vDims.z * 2.0f,
				(float)(nW - 1) * vScale.x, (float)(nH - 1) * vScale.z,
				(float)nW * vScale.x, (float)nH * vScale.z);
		}
		// THE WAVE'S HEIGHT IS AUTHORED, AND WE HAD BEEN INVENTING IT.
		//
		// 17 September. Every previous version of this line derived the
		// amplitude from something we chose - a flat 20 units, then a
		// fraction of the cell edge, which put the HQ pool at 39. The game
		// states it outright and it is nowhere near either number:
		//
		//   VolumeBrush::CreateSurface (ObjectDLL) creates the surface
		//   PolyGrid and sets vDims.y = the brush's SurfaceHeight property
		//   before sending it to the client, where CPolyGridFX::Init copies
		//   it into m_vDims. The HQ pool - Water0 in WORLDS/T01S02.DAT -
		//   authors SurfaceHeight 3.0, and PolyGrid's own default is 5.
		//
		// The height byte is g_SinTable[], a signed char at sin * 128, so a
		// grid swings the full +/-dims.y. The HQ pool therefore moves +/-3
		// units, and we were drawing +/-39: thirteen times life size, which
		// is not a pool heaving, it is a sheet flapping.
		//
		// ILTClient::GetObjectDims returns (1,1,1) for a polygrid, which is
		// why this was written off as unavailable and a number invented in
		// its place. The FX object has had the authored value all along -
		// see the note in the census below, and the general lesson in the
		// skill: ask the object that owns the value, not the API that is
		// willing to answer.
		//
		// VRPolyGridHeight is now a MULTIPLIER on the authored height, 1.0
		// being the game's own. It only has to be touched if the eyes say
		// the authored value reads small in a headset, which is a different
		// claim from the one this code used to make.
		float fAuthoredY = 0.0f;
		if (g_pGameClientShell && g_pGameClientShell->GetSFXMgr())
		{
			// BY THE CLIENT OBJECT. We are walking the client's own object
			// list, so we hold GetObject()'s handle, not GetServerObj()'s.
			CSpecialFX* pFX = g_pGameClientShell->GetSFXMgr()
								->FindSpecialFXByClientObj(SFX_POLYGRID_ID, h);
			if (pFX) fAuthoredY = ((CPolyGridFX*)pFX)->GetDims().y;
		}
		const float fCellMax = (vScale.x > vScale.z) ? vScale.x : vScale.z;
		// The fallback is the old cell fraction, and it SAYS SO once, because
		// a grid silently drawn by the guess would look like the fix failing.
		float fHScale;
		if (fAuthoredY > 0.01f)
		{
			fHScale = fAuthoredY * g_vtVRPolyGridHeight.GetFloat();
		}
		else
		{
			fHScale = 0.3f * ((fCellMax > 1.0f) ? fCellMax : 1.0f);
			static int s_nSaidFallback = 0;
			if (s_nSaidFallback < 4)
			{
				++s_nSaidFallback;
				VRLog::Msg("VRPrims: grid %08X has NO authored height (no PolyGridFX"
					" or dims.y 0) - falling back to 0.3 x the %.0f-unit cell = %.1f units",
					(unsigned)(uintptr_t)h, fCellMax, fHScale);
			}
		}
		const float fPanK = g_vtVRPolyGridPan.GetFloat();
		const float fRipple = g_vtVRPolyGridRipple.GetFloat();
		// HOW OFTEN THE WATER TEXTURE REPEATS. At the engine's scale of 1.0 the
		// sprite was stretched once over the whole 912-unit pool and came out
		// as a smooth wash; retail's pond is mottled at a much finer grain
		// (the 14 September clip). VRPolyGridTexRepeat multiplies the scale.
		const float fRep = (g_vtVRPolyGridTexRepeat.GetFloat() > 0.0f) ? g_vtVRPolyGridTexRepeat.GetFloat() : 1.0f;
		// HOW WIDE IS THE SHEET? n x scale, and this was MEASURED rather than
		// reasoned - twice, because the first answer was wrong.
		//
		// The HQ pool's brush is 2560 x 2048 (from the level file). The grid
		// comes back 4 x 4 with scale 640 x 512, so scale is extent / n - one
		// CELL, not the gap between vertices. Spacing four vertices one cell
		// apart spans three cells, 1920 x 1536: the water sat a quarter short
		// of its own pool on every side, which is what shipped last night.
		//
		// The object's DIMS looked like the honest way to ask, and are not:
		// the engine returns (1, 1, 1) for a polygrid, so trusting them drew
		// a two-unit puddle. They are logged beside this for the next reader
		// and used for nothing.
		// WHAT THE GRID'S SHADING AND HEIGHTS ACTUALLY ARE, once a second per
		// grid: the colour table's range (the wave shows through it in retail)
		// and the height bytes' range. 14 September: the pond read as a flat
		// sheet of glass - the numbers say whether a wave is even being handed on.
		{
			static uint32 s_nShadeSaidAt = 0;
			if (s_frame.nFrame - s_nShadeSaidAt >= 90)
			{
				s_nShadeSaidAt = s_frame.nFrame;
				int nMinB = 127, nMaxB = -128; float fMinL = 1e9f, fMaxL = -1e9f;
				for (uint32 q = 0; q < nW * nH; ++q)
				{
					const int hb = (signed char)pBytes[q];
					if (hb < nMinB) nMinB = hb; if (hb > nMaxB) nMaxB = hb;
					const PGColor& c = pTable[hb + 128];
					const float fL = (c.x + c.y + c.z) / 3.0f;
					if (fL < fMinL) fMinL = fL; if (fL > fMaxL) fMaxL = fL;
				}
				// The byte range and the amplitude it is multiplied by, so a
				// run says in one line how far the water actually moves:
				// (max-min)/127 x fHScale is the peak-to-peak swing in units,
				// and retail's authored figure for the HQ pool is 6.
				VRLog::Msg("VRPrims: grid %08X heights %d..%d -> %.1f units peak-to-peak"
					" (authored %.1f x VRPolyGridHeight %.2f = %.1f), shade %.0f..%.0f of 255",
					(unsigned)(uintptr_t)h, nMinB, nMaxB,
					((float)(nMaxB - nMinB) / 127.0f) * fHScale,
					fAuthoredY, g_vtVRPolyGridHeight.GetFloat(), fHScale, fMinL, fMaxL);
				// WHAT THE TEXTURE IS DOING, because a loop in the picture has
				// to be a loop in one of these. CPolyGridFX::UpdateSurface
				// ramps the scale between the level's XScaleMin and XScaleMax
				// over XScaleDuration and then REVERSES, so the sheet breathes;
				// the pan is the brush's current and is zero on this pool.
				VRLog::Msg("VRPrims:   its texture: scale %.3f x %.3f, pan %.3f x %.3f",
					fXScale, fYScale, fXPan, fYPan);
			}
		}
		const float fSpanX = (float)nW * vScale.x;
		const float fSpanZ = (float)nH * vScale.z;
		// THE SURFACE'S OPACITY IS THE VERTEX'S. The engine's polygrid
		// renderer, when the water texture names an environment map (every
		// water texture in the game does), selects the alpha from the diffuse
		// colour and ignores the texture's. WA0010's own alpha averages 0.55,
		// so multiplying it by the level's SurfaceAlpha 0.8 left the HQ pool at
		// 0.44 - a faint film with the green floor showing through, where
		// retail's pool is the blue texture at 0.8 (the retail screenshot,
		// 22 September). VRPolyGridDiffuseAlpha 0 multiplies them again.
		uint32 nPGFlags = BlendFlagsOf(h);
		if (g_vtVRPolyGridDiffuseAlpha.GetFloat() > 0.0f) nPGFlags |= VRPRIM_F_DIFFUSEALPHA;
		VRPrimRun* r = BeginRun(VRPRIM_T_TRIS, nPGFlags, pszTex, 2, h);
		if (!r) return;
		// (The old note, superseded by the measurement above:
		// the HQ's Water2 brush is 2560 x 2048 across and a 4 x 4 grid
		// the extent over the vertex count. So a vertex sits at (i - (w-1)/2)
		// cells from the centre. The y scale came back 0, so a byte's height
		// is its fraction of 127 in world units times VRPolyGridHeight.
		// PAN IS A CURRENT, AND 1.0 MEANS NONE.
		//
		// VolumeBrush::CreateSurface sends the polygrid
		//     fXPan = 1.0 + (Current.x * 0.01)
		// so the value is an OFFSET FROM ONE, not a speed: a brush with no
		// current sends exactly 1.0. We were multiplying that 1.0 by time and
		// scrolling the pool's texture forever at a hundredth of a repeat a
		// second - slow, but never still, and never what the level asked for.
		//
		// Measured against the retail clip of the HQ pool (2026-09-17
		// 01-58-48.mp4, 60 fps): a patch of open water cross-correlated
		// frame to frame moves 0.00 px/frame in x and in y over sixty
		// frames. Retail does not scroll this pool's texture AT ALL, and
		// T01S02's Water0 authors Current (0,0,0), which is the same fact
		// from the other end.
		//
		// TWO SOURCES, TWO CONVENTIONS, AND ONLY ONE OF THEM IS AN OFFSET.
		//
		// A VolumeBrush surface sends 1.0 + current*0.01, so it sits within a
		// whisker of 1.0 and the current is the remainder. A STANDALONE
		// PolyGrid object (four levels: M15S02, T01S01, T05S01, M16S04) sends
		// its own XPan property instead, whose default is 10 - subtracting one
		// from that and calling it a speed would pan those grids five times
		// faster than they have ever panned here.
		//
		// So: near 1.0 means the brush encoding and the remainder is the
		// current; anything else is a raw authored rate and keeps exactly the
		// hundredth-scale it had before this change. Every water body in the
		// game reaches one branch or the other with its own number, and no
		// level's behaviour changes except the ones that were wrong.
		float fPanX, fPanY;
		if (fXPan > 0.5f && fXPan < 1.5f) fPanX = fXPan - 1.0f; else fPanX = fXPan * 0.01f;
		if (fYPan > 0.5f && fYPan < 1.5f) fPanY = fYPan - 1.0f; else fPanY = fYPan * 0.01f;
		if (fPanX >  0.5f) fPanX =  0.5f; if (fPanX < -0.5f) fPanX = -0.5f;
		if (fPanY >  0.5f) fPanY =  0.5f; if (fPanY < -0.5f) fPanY = -0.5f;
		const float fU0 = fPanX * fTime * fPanK, fV0 = fPanY * fTime * fPanK;
		for (uint32 j = 0; j + 1 < nH; ++j)
		{
			for (uint32 i = 0; i + 1 < nW; ++i)
			{
				const uint32 idx[6] = { j*nW + i, j*nW + i + 1, (j+1)*nW + i,
										j*nW + i + 1, (j+1)*nW + i + 1, (j+1)*nW + i };
				for (int k = 0; k < 6; ++k)
				{
					VRPrimVert* v = AddVert(r);
					if (!v) { j = nH; i = nW; break; }
					const uint32 ii = idx[k] % nW, jj = idx[k] / nW;
					const signed char hb = (signed char)pBytes[idx[k]];
					const float lx = ((float)ii / (float)(nW - 1) - 0.5f) * fSpanX;
					const float lz = ((float)jj / (float)(nH - 1) - 0.5f) * fSpanZ;
					const float ly = ((float)hb / 127.0f) * fHScale
								   * (vScale.y > 0.0f ? vScale.y : 1.0f);
					v->fPos[0] = vPos.x + vR.x * lx + vU.x * ly + vF.x * lz;
					v->fPos[1] = vPos.y + vR.y * lx + vU.y * ly + vF.y * lz;
					v->fPos[2] = vPos.z + vR.z * lx + vU.z * ly + vF.z * lz;
					const PGColor& c = pTable[(int)hb + 128];
					v->fColour[0] = (c.x / 255.0f) * cr;
					v->fColour[1] = (c.y / 255.0f) * cg;
					v->fColour[2] = (c.z / 255.0f) * cb;
					v->fColour[3] = (c.a / 255.0f) * ca;
					const float fHb = (float)hb / 127.0f;
					// THE SCALE BREATHES ABOUT THE CENTRE, NOT A CORNER.
					//
					// CPolyGridFX::UpdateSurface ramps the texture scale from
					// the level's XScaleMin to XScaleMax over XScaleDuration
					// and then reverses - on the HQ pool, 1.0 to 2.0 over
					// sixty seconds, measured climbing 1.000 -> 1.335 in the
					// first twenty. Anchored at ii = 0 that zoom also
					// TRANSLATES: the far edge of the sheet walks a full
					// texture repeat across the pool and then walks back when
					// the ramp turns round. the
					// texture still had a loop to it that read as a stutter.
					//
					// Scaling about 0.5 makes it breathe symmetrically - the
					// middle stays put and the two edges move equally and
					// oppositely - which is a zoom rather than a slide, and
					// has no net travel to reverse.
					v->fUV[0] = (((float)ii / (float)(nW - 1)) - 0.5f) * fXScale * fRep + 0.5f + fU0 + fHb * fRipple;
					v->fUV[1] = (((float)jj / (float)(nH - 1)) - 0.5f) * fYScale * fRep + 0.5f + fV0 + fHb * fRipple;
					v->fSize = 0.0f;
				}
			}
		}
		EndRun(r, vPos);
	}

	// ---- canvases -------------------------------------------------------
	//
	// The recording ILTCustomDraw. State defaults are the ones the interface
	// header lists beside each enumerator.
	class CVRRecordDraw : public ILTCustomDraw
	{
	public:
		uint32	m_nState[NUM_LTRSTATES];
		char	m_szTex[64];
		VRPrimRun* m_pRun;
		uint32	m_nRunFlags;
		LTVector m_vSum; uint32 m_nSumN;
		uint32	m_nPolys, m_nPolysClose;

		CVRRecordDraw() { Reset(); }
		void Reset()
		{
			m_nState[LTRSTATE_ALPHABLENDENABLE] = 0;
			m_nState[LTRSTATE_ZREADENABLE] = 1;
			m_nState[LTRSTATE_ZWRITEENABLE] = 1;
			m_nState[LTRSTATE_SRCBLEND] = LTBLEND_SRCALPHA;
			m_nState[LTRSTATE_DESTBLEND] = LTBLEND_INVSRCALPHA;
			m_nState[LTRSTATE_TEXADDR] = LTTEXADDR_WRAP;
			m_nState[LTRSTATE_COLOROP] = LTOP_MODULATE;
			m_nState[LTRSTATE_ALPHAOP] = LTOP_SELECTTEXTURE;
			m_szTex[0] = 0; m_pRun = LTNULL; m_nRunFlags = 0;
			m_vSum.Init(); m_nSumN = 0; m_nPolys = 0; m_nPolysClose = 0;
		}
		uint32 FlagsNow(uint32 nPrimFlags) const
		{
			uint32 f = 0;
			if (m_nState[LTRSTATE_ALPHABLENDENABLE])
			{
				const uint32 s = m_nState[LTRSTATE_SRCBLEND], d = m_nState[LTRSTATE_DESTBLEND];
				if (s == LTBLEND_ONE && d == LTBLEND_ONE) f |= VRPRIM_F_ADDITIVE;
				else if (s == LTBLEND_ZERO && d == LTBLEND_SRCCOLOR) f |= VRPRIM_F_MULTIPLY;
			}
			if (!m_nState[LTRSTATE_ZREADENABLE]) f |= VRPRIM_F_NOZREAD;
			if (m_nState[LTRSTATE_ZWRITEENABLE] && !m_nState[LTRSTATE_ALPHABLENDENABLE]) f |= VRPRIM_F_ZWRITE;
			if (m_nState[LTRSTATE_TEXADDR] == LTTEXADDR_CLAMP) f |= VRPRIM_F_CLAMP;
			if (m_nState[LTRSTATE_ALPHAOP] == LTOP_SELECTDIFFUSE) f |= VRPRIM_F_DIFFUSEALPHA;
			if (!m_szTex[0]) f |= VRPRIM_F_NOTEX;
			if (nPrimFlags & FLAG_REALLYCLOSE) f |= VRPRIM_F_REALLYCLOSE;
			return f;
		}
		void CloseRun()
		{
			if (m_pRun)
			{
				LTVector c = m_vSum;
				if (m_nSumN) c *= (1.0f / (float)m_nSumN);
				EndRun(m_pRun, c);
			}
			m_pRun = LTNULL; m_vSum.Init(); m_nSumN = 0;
		}
		virtual LTRESULT DrawPrimitive(LTVertex* pVerts, uint32 nVerts, uint32 flags)
		{
			if (!pVerts || nVerts < 3) return LT_OK;
			const uint32 f = FlagsNow(flags);
			if (f & VRPRIM_F_REALLYCLOSE) ++m_nPolysClose;
			++m_nPolys;
			if (!m_pRun || m_nRunFlags != f
				|| (m_pRun->nCount + (nVerts - 2) * 3) > 60000)
			{
				CloseRun();
				m_pRun = BeginRun(VRPRIM_T_TRIS, f, m_szTex, 3, CBasePolyDrawFX::GetGlobalCanvaseObj());
				m_nRunFlags = f;
				if (!m_pRun) return LT_OK;
			}
			// A fan, as D3D7 drew a convex polygon.
			for (uint32 t = 1; t + 1 < nVerts; ++t)
			{
				const uint32 ix[3] = { 0, t, t + 1 };
				for (int k = 0; k < 3; ++k)
				{
					VRPrimVert* v = AddVert(m_pRun);
					if (!v) return LT_OK;
					const LTVertex& s = pVerts[ix[k]];
					v->fPos[0] = s.m_Vec.x; v->fPos[1] = s.m_Vec.y; v->fPos[2] = s.m_Vec.z;
					v->fColour[0] = s.m_Color.r / 255.0f; v->fColour[1] = s.m_Color.g / 255.0f;
					v->fColour[2] = s.m_Color.b / 255.0f; v->fColour[3] = s.m_Color.a / 255.0f;
					v->fUV[0] = s.m_TU; v->fUV[1] = s.m_TV; v->fSize = 0.0f;
					m_vSum += s.m_Vec; ++m_nSumN;
				}
			}
			return LT_OK;
		}
		virtual LTRESULT SetState(LTRState state, uint32 val)
		{
			if ((int)state >= 0 && state < NUM_LTRSTATES) m_nState[state] = val;
			return LT_OK;
		}
		virtual LTRESULT GetState(LTRState state, uint32& val)
		{
			val = ((int)state >= 0 && state < NUM_LTRSTATES) ? m_nState[state] : 0;
			return LT_OK;
		}
		virtual LTRESULT SetTexture(const char* pTexture)
		{
			if (pTexture && pTexture[0]) { strncpy(m_szTex, pTexture, sizeof m_szTex - 1); m_szTex[sizeof m_szTex - 1] = 0; }
			else m_szTex[0] = 0;
			return LT_OK;
		}
		virtual LTRESULT GetTexelSize(float& fSizeU, float& fSizeV)
		{
			fSizeU = 1.0f / 256.0f; fSizeV = 1.0f / 256.0f;
			return LT_OK;
		}
	};
	CVRRecordDraw s_Draw;
	uint32 s_nCanvasPolys = 0, s_nCanvasClose = 0;

	void PublishCanvas()
	{
		HOBJECT hCanvas = CBasePolyDrawFX::GetGlobalCanvaseObj();
		if (!hCanvas) return;
		CanvasDrawFn fn = LTNULL; void* pUser = LTNULL;
		if (g_pLTClient->GetCanvasFn(hCanvas, fn, pUser) != LT_OK || !fn) return;

		// THE RIBBONS FACE THE HEAD, NOT THE BODY.
		//
		// A poly-line builds its width by transforming its points into CAMERA
		// space, offsetting sideways there, and transforming back - which is
		// how a vehicle trail or the zip cord stays face-on however you look
		// at it. It reads the camera object to do that, and outside
		// RenderWorldEyes the camera carries the BODY rotation only: the head
		// is applied inside the eye pass and put back in the __finally.
		//
		// So every ribbon in the game was oriented to the body's forward, and
		// turning your head far enough would have shown one edge-on - a
		// zip cord that vanishes as you look along it. The camera wears the
		// composed rotation for the length of this call and is put straight
		// back, the same borrow-and-restore the eye pass makes.
		HOBJECT hCam = g_pGameClientShell ? g_pGameClientShell->GetCamera() : LTNULL;
		LTRotation rWas;
		bool bBorrowed = false;
		if (hCam && s_Rebase.bKnown && VRShared::IsLive())
		{
			const VRSharedState& cs = VRShared::State();
			LTRotation rHead;
			rHead.Init(-cs.fHeadQuatX, -cs.fHeadQuatY,
					   cs.fHeadQuatZ, cs.fHeadQuatW);
			g_pLTClient->GetObjectRotation(hCam, &rWas);
			LTRotation rView = rWas * rHead;
			g_pLTClient->SetObjectRotation(hCam, &rView);
			bBorrowed = true;
		}

		s_Draw.Reset();
		fn(&s_Draw, hCanvas, pUser);
		s_Draw.CloseRun();
		s_nCanvasPolys += s_Draw.m_nPolys;
		s_nCanvasClose += s_Draw.m_nPolysClose;

		if (bBorrowed) g_pLTClient->SetObjectRotation(hCam, &rWas);
	}
}

// ---- dynamic lights -------------------------------------------------------
namespace
{
	VRLightFrame s_lights;
	uint32 s_nLightsPeak = 0;

	void PublishLights(HLOCALOBJ* pObjs, uint32 nObjs, uint32 nFrame, const LTVector& vEye, float fRange)
	{
		static VRLightPublishFn s_pfnL = LTNULL;
		static HMODULE s_hSeenL = (HMODULE)(uintptr_t)1;
		HMODULE h = GetModuleHandleA("d3dstub.ren");
		if (h != s_hSeenL)
		{
			s_hSeenL = h;
			s_pfnL = h ? (VRLightPublishFn)GetProcAddress(h, "R3D_PublishLights") : LTNULL;
			VRLog::Msg("VRLights: publish entry %s", s_pfnL ? "resolved" : "missing");
		}
		if (!s_pfnL) return;
		s_lights.nMagic = VRLIGHT_MAGIC; s_lights.nVersion = VRLIGHT_VERSION;
		s_lights.nFrame = nFrame; s_lights.nCount = 0;
		for (uint32 i = 0; i < nObjs && s_lights.nCount < VRLIGHT_MAX; ++i)
		{
			if (g_pLTClient->GetObjectType(pObjs[i]) != OT_LIGHT) continue;
			++s_nLightsSeen;
			const uint32 dwFlags = g_pLTClient->GetObjectFlags(pObjs[i]);
			if (!(dwFlags & FLAG_VISIBLE)) { ++s_nLightsHidden; continue; }
			const float fR = g_pLTClient->GetLightRadius(pObjs[i]);
			if (fR <= 1.0f) { ++s_nLightsTiny; continue; }
			LTVector v; g_pLTClient->GetObjectPos(pObjs[i], &v);
			{
				LTVector d; VEC_SUB(d, v, vEye);
				if (VEC_MAG(d) - fR > fRange) { ++s_nLightsFar; continue; }
			}
			VRLightInst& L = s_lights.lights[s_lights.nCount++];
			L.fPos[0] = v.x; L.fPos[1] = v.y; L.fPos[2] = v.z;
			L.fRadius = fR;
			float r = 1.0f, g = 1.0f, b = 1.0f;
			g_pLTClient->GetLightColor(pObjs[i], &r, &g, &b);
			L.fColour[0] = r; L.fColour[1] = g; L.fColour[2] = b;
			L.nFlags = 0;
			if (!(dwFlags & FLAG_ONLYLIGHTOBJECTS)) L.nFlags |= VRLIGHT_F_WORLD;
			if (!(dwFlags & FLAG_ONLYLIGHTWORLD))   L.nFlags |= VRLIGHT_F_OBJECTS;
			L.nObject = (uint32)(uintptr_t)pObjs[i];
		}
		// AND THE HANDED-OVER LIGHTS. The muzzle flash's own light is an
		// OT_LIGHT, and the engine's object query never returns it - the
		// same blind spot that hides the flash's particles. Without this it
		// was offered to the prim loop, counted as published, and then fell
		// through a switch that only routes particles, lines and polygrids:
		// the room never lit up when the gun fired.
		for (int e = 0; e < s_nExtra && s_lights.nCount < VRLIGHT_MAX; ++e)
		{
			HLOCALOBJ hX = s_Extra[e];
			if (!hX || g_pLTClient->GetObjectType(hX) != OT_LIGHT) continue;
			const uint32 dwX = g_pLTClient->GetObjectFlags(hX);
			const double fSinceL = s_frame.fTime - VRMuzzleFlashLastShowTime();
			const bool bFreshL = (VRMuzzleFlashLastShowTime() >= 0.0
								  && fSinceL >= 0.0 && fSinceL <= 0.05);
			if (!(dwX & FLAG_VISIBLE) && !bFreshL) continue;
			const float fRX = g_pLTClient->GetLightRadius(hX);
			if (fRX <= 1.0f) continue;
			LTVector vX; g_pLTClient->GetObjectPos(hX, &vX);
			VRLightInst& LX = s_lights.lights[s_lights.nCount++];
			LX.fPos[0] = vX.x; LX.fPos[1] = vX.y; LX.fPos[2] = vX.z;
			LX.fRadius = fRX;
			float rx = 1.0f, gx = 1.0f, bx = 1.0f;
			g_pLTClient->GetLightColor(hX, &rx, &gx, &bx);
			LX.fColour[0] = rx; LX.fColour[1] = gx; LX.fColour[2] = bx;
			LX.nFlags = VRLIGHT_F_WORLD | VRLIGHT_F_OBJECTS;
			LX.nObject = (uint32)(uintptr_t)hX;
			++s_nLightsExtra;
		}
		if (s_lights.nCount > s_nLightsPeak) s_nLightsPeak = s_lights.nCount;
		s_pfnL(&s_lights);
	}
}

// The model file an object was created with, from the skin records above,
// or "" - the interface census names the menu's objects with it.
const char* VRSkins_ModelFile(HLOCALOBJ h)
{
	for (int i = 0; i < kSkinRecs; ++i)
		if (s_Skins[i].h == h && s_Skins[i].nStamp) return s_Skins[i].szModel;
	return "";
}

void VRPrims_AddExtra(HLOCALOBJ h)
{
	if (!h || s_nExtra >= kExtras) return;
	for (int i = 0; i < s_nExtra; ++i) if (s_Extra[i] == h) return;
	s_Extra[s_nExtra++] = h;
}

VRViewRebase& VRPrims_Rebase() { return s_Rebase; }

// THE LAST GUN POSE THAT WAS KNOWN, kept across frames.
//
// s_Rebase is cleared at the top of every frame and filled when the VIEW
// WEAPON is published - and the view weapon is added to the object list LAST,
// so anything processed before it sees bKnown false. Camera-relative MODELS
// are processed before it and need the transform too, so they use the pose
// from the frame before. A gun one frame stale is about 11 ms behind, which
// nothing can see; a muzzle flash left at the map origin is unmissable.
static VRViewRebase s_RebaseLast;
static bool         s_bRebaseLastKnown = false;

void VRPrims_RememberRebase()
{
	if (s_Rebase.bKnown) { s_RebaseLast = s_Rebase; s_bRebaseLastKnown = true; }
}

bool VRPrims_RebaseEverKnown() { return s_bRebaseLastKnown; }

const VRViewRebase& VRPrims_RebaseLast() { return s_RebaseLast; }

// THE ROTATION THAT CARRIES BODY YAW. See VRPrims.h.
//
// Measured on a right-stick turn, 20 September (VRFlashBack, 400
// samples): the flash sat a near-constant 13.5 units from the drawn gun while
// its heading moved only with the player's HAND - the body turned and the offset did
// not. Three fixes that changed the origin, or where the rotation came from,
// measured worse and were reverted. The offset had the right length and the
// wrong FRAME.
//
// The renderer places a view-weapon node at
//     vEye + camBasis * (vOrig*K + (R_obj * rel)*K*S)
// and rotates it by camBasis * R_obj (VRPublishModels, the node loop). R_obj is
// GetObjectRotation on a FLAG_REALLYCLOSE object: the hand's rotation in the
// CAMERA object's space, and that object carries the body's yaw, not the head's.
// Every gun-frame offset in the client - the authored muzzle, the tuner's trims,
// the casing's ejection axes - was applied as R_obj * v in world axes and
// stopped there, one basis short of what the picture is drawn through. At body
// yaw zero the two agree, which is why every still capture looked right.
static LTVector VRPrims_ObjToWorld(const VRViewRebase& rb, const LTRotation& rObj, const LTVector& v)
{
	LTVector vU, vR, vF;
	g_pLTClient->GetRotationVectors(const_cast<LTRotation*>(&rObj), &vU, &vR, &vF);
	const LTVector d = vR * v.x + vU * v.y + vF * v.z;		// the object's parent space
	if (!g_vtVRGunFrameBody.IsInitted())
		g_vtVRGunFrameBody.Init(g_pLTClient, "VRGunFrameBody", LTNULL, 1.0f);
	if (g_vtVRGunFrameBody.GetFloat() <= 0.0f) return d;	// the old arm: world axes
	return rb.vCamR * d.x + rb.vCamU * d.y + rb.vCamF * d.z;
}

LTVector VRPrims_GunFrameToWorld(const VRViewRebase& rb, const LTVector& vGunFrame)
{
	return VRPrims_ObjToWorld(rb, rb.rGunWorld, vGunFrame);
}

LTVector VRPrims_GunFrameToWorldLast(const LTRotation& rObj, const LTVector& vGunFrame)
{
	if (!s_bRebaseLastKnown)
	{
		LTVector vU, vR, vF;
		g_pLTClient->GetRotationVectors(const_cast<LTRotation*>(&rObj), &vU, &vR, &vF);
		return vR * vGunFrame.x + vU * vGunFrame.y + vF * vGunFrame.z;
	}
	return VRPrims_ObjToWorld(s_RebaseLast, rObj, vGunFrame);
}

LTVector VRPrims_RebasePointLast(const LTVector& vCam)
{
	if (!s_bRebaseLastKnown) return vCam;
	const VRViewRebase& rb = s_RebaseLast;
	LTVector d; VEC_SUB(d, vCam, rb.vGunCam); d *= rb.fK;
	LTVector vU, vR, vF;
	g_pLTClient->GetRotationVectors(const_cast<LTRotation*>(&rb.rGunWorld), &vU, &vR, &vF);
	LTVector o = rb.vGunWorld;
	o += vR * d.x; o += vU * d.y; o += vF * d.z;
	return o;
}

// The drawn barrel end, noted by VRPublishModels' node walk. One frame behind
// the weapon that reads it, exactly like the rebase pose beside it.
static LTVector s_vDrawnMuzzle(0.0f, 0.0f, 0.0f);
static bool     s_bDrawnMuzzle = false;

void VRPrims_NoteDrawnMuzzle(const LTVector& vWorld)
{
	s_vDrawnMuzzle = vWorld;
	s_bDrawnMuzzle = true;
}

bool VRPrims_DrawnMuzzle(LTVector& vOut)
{
	if (!s_bDrawnMuzzle) return false;
	vOut = s_vDrawnMuzzle;
	return true;
}

static LTVector s_vDrawnCentre(0.0f, 0.0f, 0.0f);
static bool     s_bDrawnCentre = false;

void VRPrims_NoteDrawnGunCentre(const LTVector& vWorld)
{
	s_vDrawnCentre = vWorld;
	s_bDrawnCentre = true;
}

bool VRPrims_DrawnGunCentre(LTVector& vOut)
{
	if (!s_bDrawnCentre) return false;
	vOut = s_vDrawnCentre;
	return true;
}

// A WORLD-UNIT offset from the gun, placed in the gun's own frame.
//
// RebasePointLast above exists for points the ENGINE holds in its compressed
// camera-relative space - the muzzle flash object, the camera-relative sprites -
// and fK (VRViewModelScale, 17) is what expands that space into world units.
// It is correct there and must stay.
//
// It is wrong for an offset the GAME authored in game units. The muzzle is
// exactly that: CWeaponModel places the gun at vOffset and the muzzle at
// vOffset + vMuzzleOffset, both in game units, so the muzzle's offset FROM THE
// GUN is vMuzzleOffset and nothing needs scaling. Passing it through fK
// multiplied a ~14-unit offset by 17 and laid the result along the gun's own
// right/up/forward axes - which is why the headset showed casings and tracers about
// thirteen feet away, out to the RIGHT and near the GROUND, rather than merely
// somewhere random (19 September, confirmed in a headset against 270 logged
// shots that measured the same distance on 14 September).
LTVector VRPrims_GunPointFromOffsetLast(const LTVector& vOffsetFromGun)
{
	// The caller guards on VRPrims_RebaseEverKnown(); this is belt and braces.
	if (!s_bRebaseLastKnown) return vOffsetFromGun;
	const VRViewRebase& rb = s_RebaseLast;
	// Through the camera basis - see VRPrims_GunFrameToWorld above.
	return rb.vGunWorld + VRPrims_GunFrameToWorld(rb, vOffsetFromGun);
}

LTVector VRPrims_RebasePoint(const LTVector& vCam)
{
	const VRViewRebase& rb = s_Rebase;
	if (!rb.bKnown) return vCam;
	// The offset from the retail gun, in the camera's own axes - which is
	// the retail gun's frame too, the gun pointing down the camera's forward.
	LTVector d; VEC_SUB(d, vCam, rb.vGunCam); d *= rb.fK;
	LTVector vU, vR, vF;
	g_pLTClient->GetRotationVectors(const_cast<LTRotation*>(&rb.rGunWorld), &vU, &vR, &vF);
	LTVector o = rb.vGunWorld;
	o += vR * d.x; o += vU * d.y; o += vF * d.z;
	return o;
}

bool VRPrims_Active()
{
	return GetModuleHandleA("d3dstub.ren") != NULL;
}

void VRPrims_Init()
{
	g_vtVRPrims.Init(g_pLTClient, "VRPrims", LTNULL, 1.0f);
	g_vtVRGunFrameBody.Init(g_pLTClient, "VRGunFrameBody", LTNULL, 1.0f);
	// THE SIZE IS A GUESS UNTIL A HEADSET SAYS OTHERWISE. The game divides a
	// particle's radius by the SCREEN WIDTH before the engine sees it (see
	// CBaseParticleSystemFX::SetupSystem), so a size of 0.6 arrived for an
	// impact puff at 2560 wide. The art was sized for 640: 2560/640 is 4,
	// and the retail projection doubles it at 90 degrees, hence 8. A puff is
	// then about ten units across, steam thirty. +VRParticleScale to adjust.
	g_vtVRParticleScale.Init(g_pLTClient, "VRParticleScale", LTNULL, 8.0f);
	// 20, NOT 6 (14 September): six units on 150-unit cells is a flat plane
	// in stereo, however the shading moves. Retail's authored height is 3-4,
	// which a flat screen reads as hills through the shading; a headset
	// needs the geometry itself to rise. In the headset it now waves up and
	// down as real movement rather than a texture that moves.
	// A MULTIPLIER ON THE GAME'S OWN SURFACE HEIGHT, not a height. See the
	// derivation in PublishPolyGrid: 1.0 is exactly what the level authored.
	//
	// AND THE AUTHORED FIGURE IS NOT LEGIBLE IN STEREO. Headset testing, 17
	// September, on a build running the authored value: the pool at the bottom
	// did not bounce up and down, it only had a moving texture. The log for
	// that session says the surface was moving its full 8 units peak to
	// peak, so this is not the change failing to arrive - it is 8 units
	// spread across a 900-unit pool, a surface tilt of about one degree,
	// which a flat screen sells through the colour ramp and a headset simply
	// does not.
	//
	// So this is a VR LEGIBILITY setting, in the same family as HUD scale,
	// and it is the one number here that is deliberately NOT the game's.
	// 4.0 is a starting point for the eyes, not a claim about the level; the
	// cvar is the dial and it persists, so it can be tuned in one session
	// without a rebuild.
	// 17 September, with the glass sheet off it at last: the water finally
	// bounced up and down properly, and only wanted to be a bit bigger
	// vertically. 4 -> 6, so the HQ pool swings 48
	// units rather than 32.
	// 12: at 6 the HQ pool still read as a very small wave bounce (19 Sep).
	g_vtVRPolyGridHeight.Init(g_pLTClient, "VRPolyGridHeight", LTNULL, 12.0f);
	// THE AUTHORED TEXTURE SCALE, UNMULTIPLIED. This was 4, on the reading
	// that retail's pond is mottled at a much finer grain - and the level
	// disagrees: T01S02's Water0 authors XScaleMin 1.0 rising to XScaleMax
	// 2.0 over sixty seconds, so the sprite is meant to be stretched one to
	// two times across the whole pool, not eight. At 4x the texture repeated
	// far below what a 7x7 grid can carry and mip-mapped down to a flat
	// wash, which is exactly the pale slab in the desk captures.
	g_vtVRPolyGridTexRepeat.Init(g_pLTClient, "VRPolyGridTexRepeat", LTNULL, 1.0f);
	// The pan is texture repeats per second per pan unit. PolyGrid.cpp's
	// property default is 10 and the HQ waterfall asks for 1.0; at the old
	// 0.01 the waterfall and Morocco's pool stood still (headset testing, 13
	// September: no water animation). 1.0 is the plain reading of the number; the tester's eye
	// against retail decides whether it is too fast or too slow.
	// A still pool carries pan 1.0 (VolumeBrush::CreateSurface: 1.0 plus a
	// hundredth of its Current) and retail's still water barely drifts, so
	// 1.0 repeats/second was far too fast (
	// the pool raced and the fountain flowed). 0.1 until a retail clip of the HQ
	// waterfall and pool gives the real rate.
	// BACK TO 0.01 (headset testing, 13 September evening): at 1.0 the pool raced, at
	// 0.1 the fountain still flowed; retail's ground water moves as a WAVE,
	// which the height field below already is. The pan is not the answer.
	// A MULTIPLIER on the level's own current (see the derivation in
	// PublishPolyGrid). 1.0 is what the brush asked for.
	g_vtVRPolyGridPan.Init(g_pLTClient, "VRPolyGridPan", LTNULL, 1.0f);
	// THE RIPPLE IS IN THE TEXTURE COORDINATES, not only in the height.
	// Retail's HQ waterfall (a retail clip, 13 September, 30 fps): the
	// streaks of the water texture sway sideways every frame with no scroll
	// at all - the engine offsets each vertex's texture coordinate by its
	// height byte, which is what a 7 x 7 sheet of plasma looks like head-on.
	// Ours moved the vertices only, invisible on a face seen straight on, so
	// the waterfall stood still. 0.06 of the texture per full byte, by eye
	// against that clip.
	// OFF (0): tried at 0.06 and it made the fountain sway unnaturally; the
	// retail motion is the vertices, not the picture. Kept as a switch.
	// 0.03 (14 September): the retail clip sways the texture with the height;
	// 0.06 read as unnatural in the fountain when the fountain heaved as much
	// as the pool. It no longer does.
	// NOT AN AUTHORED EFFECT. CPolyGridFX::UpdateSurface pans and scales the
	// texture and does nothing else; nothing in the game perturbs a vertex's
	// texture coordinate by its height. Kept as a knob, off by default.
	g_vtVRPolyGridRipple.Init(g_pLTClient, "VRPolyGridRipple", LTNULL, 0.0f);
	g_vtVRPolyGridDiffuseAlpha.Init(g_pLTClient, "VRPolyGridDiffuseAlpha", LTNULL, 1.0f);
	memset(s_PS, 0, sizeof s_PS);
	memset(s_PG, 0, sizeof s_PG);
	memset(s_Skins, 0, sizeof s_Skins);
	if (!g_pLTClient) return;
	if (!s_pfnSetupPS && g_pLTClient->SetupParticleSystem
		&& g_pLTClient->SetupParticleSystem != MySetupParticleSystem)
	{
		s_pfnSetupPS = g_pLTClient->SetupParticleSystem;
		g_pLTClient->SetupParticleSystem = MySetupParticleSystem;
	}
	if (!s_pfnSetPGTex && g_pLTClient->SetPolyGridTexture
		&& g_pLTClient->SetPolyGridTexture != MySetPolyGridTexture)
	{
		s_pfnSetPGTex = g_pLTClient->SetPolyGridTexture;
		g_pLTClient->SetPolyGridTexture = MySetPolyGridTexture;
	}
	if (!s_pfnCreateObject && g_pLTClient->CreateObject
		&& g_pLTClient->CreateObject != MyCreateObject)
	{
		s_pfnCreateObject = g_pLTClient->CreateObject;
		g_pLTClient->CreateObject = MyCreateObject;
	}
	VRLog::Msg("VRPrims: engine entries wrapped - particle textures %s, polygrid textures %s,"
			   " object skins %s",
		s_pfnSetupPS ? "yes" : "NO", s_pfnSetPGTex ? "yes" : "NO",
		s_pfnCreateObject ? "yes" : "NO");
}

// THE SCOPE, as the publisher set it this frame. See VRPrims_SetScopeLens.
static bool     s_bScope = false;
static LTVector s_vScopeObj, s_vScopeEye, s_vScopeR, s_vScopeU;
static LTRotation s_rScopeAxis;
static float    s_fScopeRadius = 0.0f, s_fScopeFov = 20.0f;
static int      s_nScopeZoom = 0;
typedef void (__cdecl *VRScopePublishFn)(const VRScopeFrame*);

void VRPrims_SetScopeLens(const LTVector& vObjective, const LTRotation& rAxis,
						  const LTVector& vEyepiece, const LTVector& vRight,
						  const LTVector& vUp, float fRadius, int nZoom, float fFovDeg)
{
	s_bScope = true;
	s_vScopeObj = vObjective; s_rScopeAxis = rAxis; s_vScopeEye = vEyepiece;
	s_vScopeR = vRight; s_vScopeU = vUp; s_fScopeRadius = fRadius;
	s_nScopeZoom = nZoom; s_fScopeFov = fFovDeg;
}
void VRPrims_ClearScopeLens() { s_bScope = false; }

// Tell the renderer where the scope looks from, and put its eyepiece in the
// effects frame as a disc that wears the scope's own picture.
static void PublishScope(uint32 nFrame)
{
	static VRScopePublishFn s_pfnS = LTNULL;
	static HMODULE s_hSeenS = (HMODULE)(uintptr_t)1;
	HMODULE h = GetModuleHandleA("d3dstub.ren");
	if (h != s_hSeenS)
	{
		s_hSeenS = h;
		s_pfnS = h ? (VRScopePublishFn)GetProcAddress(h, "R3D_PublishScope") : LTNULL;
		VRLog::Msg("VRScope: publish entry %s", s_pfnS ? "resolved" : "missing");
	}
	if (!s_pfnS) return;
	VRScopeFrame f;
	memset(&f, 0, sizeof f);
	f.nMagic = VRSCOPE_MAGIC; f.nVersion = VRSCOPE_VERSION; f.nFrame = nFrame;
	f.nActive = s_bScope ? 1 : 0;
	if (s_bScope)
	{
		f.fPos[0] = s_vScopeObj.x; f.fPos[1] = s_vScopeObj.y; f.fPos[2] = s_vScopeObj.z;
		f.fQuat[0] = s_rScopeAxis.m_Quat[0]; f.fQuat[1] = s_rScopeAxis.m_Quat[1];
		f.fQuat[2] = s_rScopeAxis.m_Quat[2]; f.fQuat[3] = s_rScopeAxis.m_Quat[3];
		f.fFovDeg = s_fScopeFov; f.fNear = 1.0f; f.fFar = 100000.0f; f.nZoom = s_nScopeZoom;
	}
	s_pfnS(&f);
	if (!s_bScope) return;

	// The eyepiece: a fan of 32 triangles, drawn as a list, uv over the whole
	// picture. Depth-tested and written, so the scope body and the hand
	// occlude it as they would glass. Clamped, so the rim never wraps.
	VRPrimRun* r = BeginRun(VRPRIM_T_TRIS, VRPRIM_F_SCOPELENS | VRPRIM_F_CLAMP | VRPRIM_F_ZWRITE,
							"scope-lens", 3, LTNULL);
	if (!r) return;
	const int kSeg = 32;
	for (int i = 0; i < kSeg; ++i)
	{
		const float a0 = (float)i       * 6.2831853f / kSeg;
		const float a1 = (float)(i + 1) * 6.2831853f / kSeg;
		const LTVector p0 = s_vScopeEye + s_vScopeR * (cosf(a0) * s_fScopeRadius) + s_vScopeU * (sinf(a0) * s_fScopeRadius);
		const LTVector p1 = s_vScopeEye + s_vScopeR * (cosf(a1) * s_fScopeRadius) + s_vScopeU * (sinf(a1) * s_fScopeRadius);
		const LTVector* pts[3] = { &s_vScopeEye, &p0, &p1 };
		const float uv[3][2] = { { 0.5f, 0.5f },
								 { 0.5f + 0.5f * cosf(a0), 0.5f - 0.5f * sinf(a0) },
								 { 0.5f + 0.5f * cosf(a1), 0.5f - 0.5f * sinf(a1) } };
		for (int k = 0; k < 3; ++k)
		{
			VRPrimVert* v = AddVert(r);
			if (!v) break;
			v->fPos[0] = pts[k]->x; v->fPos[1] = pts[k]->y; v->fPos[2] = pts[k]->z;
			v->fColour[0] = v->fColour[1] = v->fColour[2] = v->fColour[3] = 1.0f;
			v->fUV[0] = uv[k][0]; v->fUV[1] = uv[k][1]; v->fSize = 0.0f;
		}
	}
	EndRun(r, s_vScopeEye);
}

void VRPrims_Publish(const LTVector& vEye, float fRange, uint32 nFrame)
{
	RepublishSkins();
	// THE HANDED-OVER LIST MUST BE DRAINED ON EVERY PATH OUT.
	//
	// The client adds its two objects - the muzzle flash's particles and
	// its light - before every call, and this function is the only thing
	// that empties the list. Both early returns below can fire for many
	// frames in a row: the second one fires whenever d3dstub.ren is not
	// loaded, which is exactly what a renderer reload does. Four such
	// frames fill the eight slots, and from then on AddExtra silently
	// drops everything FOREVER - the muzzle flash would stop appearing
	// after a few focus losses and never come back.
	if (g_vtVRPrims.GetFloat() <= 0.0f || !g_pLTClient) { s_nExtra = 0; return; }
	static VRPrimPublishFn s_pfn = LTNULL;
	static HMODULE s_hSeen = (HMODULE)(uintptr_t)1;
	HMODULE h = GetModuleHandleA("d3dstub.ren");
	if (h != s_hSeen)
	{
		s_hSeen = h;
		s_pfn = h ? (VRPrimPublishFn)GetProcAddress(h, "R3D_PublishPrims") : LTNULL;
		VRLog::Msg("VRPrims: publish entry %s", s_pfn ? "resolved" : "missing");
	}
	if (!s_pfn) { s_nExtra = 0; return; }

	s_frame.nMagic = VRPRIM_MAGIC; s_frame.nVersion = VRPRIM_VERSION;
	s_frame.nFrame = nFrame; s_frame.nRuns = 0; s_frame.nVerts = 0;
	s_frame.fTime = g_pLTClient->GetTime();

	// THE WHOLE LEVEL, THEN A RULE PER KIND. Berlin's rain is a line system
	// whose OBJECT sits at the weather volume's centre, thousands of units
	// off, while its lines fall around the viewer - a 4000-unit sphere never
	// found it. So every object is asked for and the distance rule is the
	// kind's own: a particle system or a polygrid must be within range of
	// the eye, a line system always counts, a light counts when its reach
	// comes within range.
	// THE HANDED-OVER OBJECTS FIRST. They are camera-relative, so no
	// distance rule applies to them and the query below would not return
	// them in any case.
	for (int e = 0; e < s_nExtra; ++e)
	{
		HLOCALOBJ h = s_Extra[e];
		if (!h) { ++s_nExtraNull; continue; }
		++s_nExtraOffered;
		const uint32 dwE = g_pLTClient->GetObjectFlags(h);
		{
			// SAY WHAT THE OBJECT ACTUALLY IS, once a second. Counting how
			// often it was hidden cannot tell an object that is switched off
			// from a handle that no longer names anything.
			static double s_fSaidExtra = 0.0;
			static int    s_nSaidExtra = 0;
			const double fNowE = g_pLTClient->GetTime();
			if (fNowE - s_fSaidExtra > 1.0 && s_nSaidExtra < 20)
			{
				s_fSaidExtra = fNowE;
				++s_nSaidExtra;
				LTVector vE; g_pLTClient->GetObjectPos(h, &vE);
				VRLog::Msg("VRExtra: handed object %p (the flash last showed %p,"
						   " %.3f s ago) type %u flags %#x"
						   " (visible %s, reallyclose %s) at %.0f %.0f %.0f",
						   (void*)h, (void*)VRMuzzleFlashLastObject(),
						   fNowE - VRMuzzleFlashLastShowTime(),
						   (unsigned)g_pLTClient->GetObjectType(h),
						   (unsigned)dwE, (dwE & FLAG_VISIBLE) ? "yes" : "no",
						   (dwE & FLAG_REALLYCLOSE) ? "yes" : "no",
						   vE.x, vE.y, vE.z);
			}
		}
		// A HANDED-OVER OBJECT IS PUBLISHED IF IT FIRED RECENTLY, whatever
		// its visibility flag says right now. See VRMuzzleFlashLastShowTime.
		const double fSince = s_frame.fTime - VRMuzzleFlashLastShowTime();
		const bool   bFresh = (VRMuzzleFlashLastShowTime() >= 0.0
							   && fSince >= 0.0 && fSince <= 0.05);
		if (!(dwE & FLAG_VISIBLE) && !bFresh) { ++s_nExtraHidden; continue; }
		++s_nExtraPublished;
		switch (g_pLTClient->GetObjectType(h))
		{
			case OT_PARTICLESYSTEM: PublishParticleSystem(h); break;
			case OT_LINESYSTEM:     PublishLineSystem(h); break;
			case OT_POLYGRID:       PublishPolyGrid(h, s_frame.fTime); break;
			default: break;
		}
	}

	static HLOCALOBJ objs[4096];
	uint32 nOut = 0, nFound = 0;
	LTVector vC = vEye;
	g_pLTClient->FindObjectsInSphere(&vC, 1000000.0f, objs, 4096, &nOut, &nFound);
	// A CAP THAT IS HIT LOSES OBJECTS SILENTLY, and 4096 is not obviously
	// enough for a NOLF level. Reported so it can never be assumed again.
	s_nObjOut = nOut; s_nObjFound = nFound;
	PublishLights(objs, nOut, nFrame, vEye, fRange);
	for (uint32 i = 0; i < nOut; ++i)
	{
		const uint32 dwFlags = g_pLTClient->GetObjectFlags(objs[i]);
		// COUNT WHAT IS BEING SKIPPED, and why. A muzzle flash is visible for
		// ONE frame; if the publish pass runs at the wrong point of the client
		// frame it will find it hidden every single time, which is
		// indistinguishable from it never existing.
		if (dwFlags & FLAG_REALLYCLOSE) ++s_nCloseAny;
		if (!(dwFlags & FLAG_VISIBLE))
		{
			if (g_pLTClient->GetObjectType(objs[i]) == OT_PARTICLESYSTEM) ++s_nPSHidden;
			continue;
		}
		const uint32 nType = g_pLTClient->GetObjectType(objs[i]);
		if (nType != OT_PARTICLESYSTEM && nType != OT_LINESYSTEM && nType != OT_POLYGRID) continue;
		// A CAMERA-RELATIVE OBJECT IS NOWHERE NEAR THE EYE, and the range
		// test below is measured from the eye - so it threw away every one of
		// them, silently, before anything could publish it.
		//
		// LithTech keeps a FLAG_REALLYCLOSE object's position in CAMERA
		// space, which puts it a few units from the world ORIGIN while the
		// player stands wherever the level put him. The distance this
		// measures is therefore the player's distance from the origin - a few
		// thousand units in most levels - and the effect is dropped. The
		// muzzle flash's particle system is exactly such an object, which is
		// why there has never been a first-person muzzle flash in this port
		// and why three desk runs on 11 September photographed nothing while
		// the gun was firing. PublishParticleSystem re-bases it onto the VR
		// gun once it gets there.
		const bool bCloseObj = (dwFlags & FLAG_REALLYCLOSE) != 0;
		if (bCloseObj) ++s_nCloseKept;
		if (nType != OT_LINESYSTEM && !bCloseObj)
		{
			LTVector p; g_pLTClient->GetObjectPos(objs[i], &p);
			LTVector d; VEC_SUB(d, p, vEye);
			if (VEC_MAG(d) > fRange) continue;
		}
		switch (nType)
		{
			case OT_PARTICLESYSTEM: PublishParticleSystem(objs[i]); break;
			case OT_LINESYSTEM:     PublishLineSystem(objs[i]); break;
			case OT_POLYGRID:       PublishPolyGrid(objs[i], s_frame.fTime); break;
			default: break;
		}
	}
	// Consumed: the prim loop and PublishLights have both seen them.
	s_nExtra = 0;
	PublishCanvas();

	PublishScope(nFrame);
	s_pfn(&s_frame);

	static uint32 s_nSaidAt = 0;
	static uint32 s_nPeakVerts = 0, s_nPeakRuns = 0;
	if (s_frame.nVerts > s_nPeakVerts) s_nPeakVerts = s_frame.nVerts;
	if (s_frame.nRuns > s_nPeakRuns) s_nPeakRuns = s_frame.nRuns;
	if (nFrame - s_nSaidAt >= 450)
	{
		s_nSaidAt = nFrame;
		VRLog::Msg("VRPrims: this frame %u runs / %u verts (peak %u / %u); over the period"
			" %u particle systems seen (%u with no texture recorded, %u particles,"
			" %u camera-relative saved by last frame's gun pose, %u DROPPED for"
			" want of one; %u camera-relative prim objects kept that the eye"
			" range test used to throw away; %u of %u objects returned by the"
			" query; %u lights seen - %u hidden, %u tiny, %u out of range,"
			" %u on the DRAWN BARREL END,"
			" %u handed over;"
			" %u camera-relative objects of any type, %u particle systems"
			" skipped as not visible; handed over: %u offered, %u had no object,"
			" %u were hidden at publish time, %u published),"
			" %u line systems (%u lines), %u polygrids (%u untextured), %u canvas polygons"
			" (%u view-relative, not drawn) | %u dynamic lights now (peak %u)%s",
			s_frame.nRuns, s_frame.nVerts, s_nPeakRuns, s_nPeakVerts,
			s_nPSSeen, s_nPSNoTex, s_nParticles, s_nPSCloseLast, s_nPSCloseDropped,
			s_nPSCloseMuzzle,
			s_nCloseKept, s_nObjOut, s_nObjFound,
			s_nLightsSeen, s_nLightsHidden, s_nLightsTiny, s_nLightsFar, s_nLightsExtra,
			s_nCloseAny, s_nPSHidden, s_nExtraOffered, s_nExtraNull,
			s_nExtraHidden, s_nExtraPublished,
			s_nLSSeen, s_nLines,
			s_nPGSeen, s_nPGNoTex, s_nCanvasPolys, s_nCanvasClose,
			s_lights.nCount, s_nLightsPeak,
			(s_nDroppedRuns || s_nDroppedVerts) ? "  <- the frame was FULL; some were dropped" : "");
		s_nPSSeen = s_nPSNoTex = s_nParticles = 0; s_nLSSeen = s_nLines = 0;
		s_nPSCloseDropped = s_nPSCloseLast = 0; s_nCloseKept = 0;
		s_nLightsSeen = s_nLightsHidden = s_nLightsTiny = s_nLightsFar = 0;
		s_nLightsExtra = 0;
		s_nCloseAny = s_nPSHidden = 0; s_nExtraPublished = 0;
		s_nExtraNull = s_nExtraHidden = s_nExtraOffered = 0;
		s_nPGSeen = s_nPGNoTex = 0; s_nCanvasPolys = s_nCanvasClose = 0;
		s_nDroppedRuns = s_nDroppedVerts = 0;
	}
}

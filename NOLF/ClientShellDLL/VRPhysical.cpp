// VRPhysical.cpp: physical play, see VRPhysical.h.
//
// Three spaces meet here, so every quantity says which it is in:
//   TRACKING  the host's OpenXR LOCAL space, metres, +Y up, -Z forward, where
//             the controllers and the head are measured (VRShared).
//   BODY      the slots' frame: from the neck point, x toward the GUN hand's
//             side, y up, z FORWARD (positive in front), metres.
//   WORLD     the game's units (58.75 to the metre), for anything that meets a
//             character: the hand placed from the camera eye along the camera's
//             body basis, exactly as the drawn body's arms are (GameClientShell).

#include "stdafx.h"
#include "VRPhysical.h"
#include "VRShared.h"
#include "VRLog.h"
#include "VRWeaponVar.h"
#include "GameClientShell.h"
#include "WeaponModel.h"
#include "PlayerStats.h"
#include "ClientUtilities.h"
#include "CharacterFX.h"
#include "SFXMgr.h"
#include "ModelButeMgr.h"
#include "MsgIDs.h"
#include "ClientSoundMgr.h"
#include "FXButeMgr.h"
#include "VRPrims.h"
#include "InterfaceMgr.h"
#include "Overlays.h"

#include <math.h>

extern CGameClientShell* g_pGameClientShell;

namespace VRPhysical
{
namespace
{
	const float kWorldPerMetre = 58.75f;
	const float kD2R = 0.01745329f;

	// --- the slots ---------------------------------------------------------
	enum { SLOT_HIP, SLOT_SHOULDER, SLOT_CHEST, SLOT_BELT, SLOT_POUCH, SLOT_COUNT };
	enum { CAT_NONE = -1, CAT_SIDEARM, CAT_LONG, CAT_GADGET, CAT_THROWN, CAT_COUNT };

	struct SlotDef
	{
		const char*	pName;
		float		x, y, z;	// BODY frame, metres from the neck point
		float		r;			// reach radius, metres
		int			nHand;		// 1 gun hand, 0 off hand
		int			nCat;		// what it holds (CAT_NONE: the ammo pouch)
		bool		bBelow;		// moves with VRHolsterHeight
	};
	// Where a person wears these, measured from under the head. The neck point
	// sits 8 cm behind and 10 cm below the eyes, turned with the head, so it
	// stays over the shoulders when the head looks down at a holster.
	const SlotDef kSlots[SLOT_COUNT] = {
		{ "hip",           0.20f, -0.50f,  0.00f, 0.13f, 1, CAT_SIDEARM, true  },
		{ "shoulder",      0.15f, -0.08f, -0.12f, 0.16f, 1, CAT_LONG,    false },
		{ "chest",        -0.10f, -0.28f,  0.12f, 0.13f, 1, CAT_GADGET,  true  },
		{ "belt",          0.02f, -0.48f,  0.15f, 0.13f, 1, CAT_THROWN,  true  },
		{ "ammo pouch",   -0.18f, -0.46f,  0.06f, 0.13f, 0, CAT_NONE,    true  },
	};
	const char* const kCatName[CAT_COUNT] = { "sidearm", "long gun", "gadget", "throwable" };

	// --- one hand's track --------------------------------------------------
	struct Sample { double t; float p[3]; };
	const int kRing = 32;
	struct HandTrack
	{
		Sample	ring[kRing];
		int		nCount, nHead;			// nHead: the newest
		float	v[3];					// TRACKING m/s, over ~33 ms
		float	fSpeed;
		float	fPeak;					// the most over the last 80 ms
		double	tPeakWin[8]; float fPeakWin[8]; int nPeakWin;
		bool	bGripWas;
		bool	bTaken;					// the grip belongs to physical play until it opens
		int		nInSlot;				// the slot the hand is inside, -1 none
		LTVector vWorldPrev, vWorldNow;	// WORLD
		bool	bWorldValid;
		double	tLastHit;
		int		nSaidPass;				// the last node passed under speed, per pass
	};
	HandTrack	s_h[2];
	uint32_t	s_nLastHostFrame = 0xFFFFFFFF;
	bool		s_bSwapWas = false;
	double		s_tNow = 0.0;

	// --- the body ----------------------------------------------------------
	float		s_fBodyYawDeg = 0.0f;	// TRACKING yaw the slots face, degrees, OpenXR sign
	bool		s_bBodyInit = false;
	uint32_t	s_nRecenterWas = 0;
	float		s_vNeck[3] = { 0, 0, 0 };	// TRACKING

	// --- holsters ----------------------------------------------------------
	int			s_nAssigned[SLOT_COUNT] = { -1, -1, -1, -1, -1 };	// weapon ids put there by hand
	int			s_nLastUsed[CAT_COUNT] = { -1, -1, -1, -1 };
	int			s_nWantWeapon = -1;		// a draw asked for, to report when it lands
	double		s_tWantAt = 0.0;
	int			s_nWantFrom = -1;
	int			s_nWantSlot = -1;

	// --- manual reload -----------------------------------------------------
	bool		s_bClipInHand = false;
	int			s_nClipMem[256];		// the clip each weapon was put away with
	int			s_nClipMemAmmo[256];
	bool		s_bClipMemInit = false;

	// --- melee -------------------------------------------------------------
	HOBJECT		s_hHitTarget = LTNULL;	// the last character struck, watched to see it go down
	double		s_tHitAt = 0.0;
	int			s_nHitNode = -1;

	// --- switches, read once a frame --------------------------------------
	bool s_bHolsters = false, s_bManual = false, s_bMelee = false, s_bWrist = false;
	bool s_bGlasses = false;	// VRGlassesToFace
	bool s_bThrow = false;		// VRThrowByHand
	bool s_bTwoHand = false;	// VRTwoHanded
	bool s_bTwoHandOn = false;	// the off hand holds the long gun's front now
	// The throw: 0 nothing, 1 readied (trigger held), 2 thrown (the fire is
	// held until the weapon sends it, or 0.35 s).
	int      s_nThrowState = 0;
	double   s_tThrowAt = 0.0;
	bool     s_bThrowSent = false;
	LTVector s_vThrowDir(0.0f, 0.0f, 1.0f);
	float    s_fThrowSpeed = 0.0f;	// m/s, the hand's
	bool s_bGlassesOn = false;	// the sunglasses in hand are at the eyes, worn
	int  s_nSaidSwitches = -1;

	double NowSec()
	{
		static LARGE_INTEGER s_f = { 0 };
		if (!s_f.QuadPart) QueryPerformanceFrequency(&s_f);
		LARGE_INTEGER c; QueryPerformanceCounter(&c);
		return (double)c.QuadPart / (double)s_f.QuadPart;
	}

	float Len3(const float* a) { return (float)sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); }

	// The textbook rotation of v by the unit quaternion q (x,y,z,w), written
	// out here, NOT LithTech's Cross, whose operands are the other way round.
	void QuatRotate(const float* q, const float* v, float* out)
	{
		const float qx = q[0], qy = q[1], qz = q[2], qw = q[3];
		// t = 2 * (q.xyz x v)
		const float tx = 2.0f * (qy * v[2] - qz * v[1]);
		const float ty = 2.0f * (qz * v[0] - qx * v[2]);
		const float tz = 2.0f * (qx * v[1] - qy * v[0]);
		// v' = v + w t + q.xyz x t
		out[0] = v[0] + qw * tx + (qy * tz - qz * ty);
		out[1] = v[1] + qw * ty + (qz * tx - qx * tz);
		out[2] = v[2] + qw * tz + (qx * ty - qy * tx);
	}

	// A controller's pointing direction in TRACKING space from the host's
	// angles: yaw about +Y (positive turns LEFT), then pitch about +X
	// (positive up), applied to -Z.
	void HandForward(const VRHandState& h, float* f)
	{
		const float y = h.fYawDeg * kD2R, p = h.fPitchDeg * kD2R;
		f[0] = -(float)sin(y) * (float)cos(p);
		f[1] =  (float)sin(p);
		f[2] = -(float)cos(y) * (float)cos(p);
	}

	// The gun hand's side: +1 when the gun is in the right hand, -1 in the
	// Leftorium. BODY x is multiplied by it, so every slot mirrors.
	float GunSide() { return VRShared::SwapHands() ? -1.0f : 1.0f; }

	// A slot's centre in TRACKING space.
	void SlotPoint(int nSlot, float* out)
	{
		const SlotDef& d = kSlots[nSlot];
		const float b = s_fBodyYawDeg * kD2R;
		// Body axes in TRACKING: forward (-sin b, 0, -cos b), right (cos b, 0, -sin b).
		const float fx = -(float)sin(b), fz = -(float)cos(b);
		const float rx =  (float)cos(b), rz = -(float)sin(b);
		float y = d.y;
		if (d.bBelow) y += 0.01f * GetConsoleFloat("VRHolsterHeight", 0.0f);
		const float x = d.x * GunSide();
		out[0] = s_vNeck[0] + rx * x + fx * d.z;
		out[1] = s_vNeck[1] + y;
		out[2] = s_vNeck[2] + rz * x + fz * d.z;
	}

	float Dist3(const float* a, const float* b)
	{
		const float d[3] = { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
		return Len3(d);
	}

	// Distance from point p to the segment a-b (any space, same units).
	float SegDist(const LTVector& a, const LTVector& b, const LTVector& p, float* pT = LTNULL)
	{
		const LTVector ab = b - a;
		const float l2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
		float t = 0.0f;
		if (l2 > 1e-6f) t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y + (p.z - a.z) * ab.z) / l2;
		if (t < 0.0f) t = 0.0f;
		if (t > 1.0f) t = 1.0f;
		if (pT) *pT = t;
		const LTVector c = a + ab * t;
		return (p - c).Mag();
	}

	// --- weapons -----------------------------------------------------------
	int CategoryOf(int nWeaponId)
	{
		if (!g_pWeaponMgr) return CAT_NONE;
		WEAPON* pW = g_pWeaponMgr->GetWeapon(nWeaponId);
		if (!pW) return CAT_NONE;
		AMMO* pA = g_pWeaponMgr->GetAmmo(pW->nDefaultAmmoType);
		if (pA && pA->eInstDamageType == DT_MELEE) return CAT_NONE;	// the fists
		if (pW->nAniType == 3) return CAT_THROWN;		// the coin and the lipsticks
		if (pW->bGadget || !_strnicmp(pW->szName, "Perfume", 7)) return CAT_GADGET;
		if (pW->nAniType == 0 || !_stricmp(pW->szName, "Rocket Launcher")) return CAT_LONG;
		if (pW->nAniType == 1) return CAT_SIDEARM;
		return CAT_NONE;
	}

	bool Drawable(CWeaponModel* pWM, CPlayerStats* pStats, int nId)
	{
		if (nId < 0 || !g_pWeaponMgr || !pStats) return false;
		if (!g_pWeaponMgr->IsPlayerWeapon(nId)) return false;
		if (!pStats->HaveWeapon((uint8)nId)) return false;
		if (pWM->IsOutOfAmmo((uint8)nId)) return false;
		return true;
	}

	const char* WeaponName(int nId)
	{
		WEAPON* pW = (g_pWeaponMgr && nId >= 0) ? g_pWeaponMgr->GetWeapon(nId) : LTNULL;
		return pW ? pW->szName : "nothing";
	}

	bool AssignedElsewhere(int nId, int nSlot)
	{
		for (int s = 0; s < SLOT_COUNT; ++s)
			if (s != nSlot && s_nAssigned[s] == nId) return true;
		return false;
	}

	// What the slot holds: what was put there by hand, else the last used of
	// its kind, else the first of its kind in the game's weapon order.
	int PickForSlot(CWeaponModel* pWM, CPlayerStats* pStats, int nSlot)
	{
		const int a = s_nAssigned[nSlot];
		if (a >= 0 && Drawable(pWM, pStats, a)) return a;
		const int nCat = kSlots[nSlot].nCat;
		if (nCat < 0) return -1;
		const int l = s_nLastUsed[nCat];
		if (l >= 0 && Drawable(pWM, pStats, l) && !AssignedElsewhere(l, nSlot)) return l;
		const int n = g_pWeaponMgr ? g_pWeaponMgr->GetNumWeapons() : 0;
		for (int i = 0; i < n; ++i)
			if (CategoryOf(i) == nCat && Drawable(pWM, pStats, i) && !AssignedElsewhere(i, nSlot)) return i;
		return -1;
	}

	void Buzz(int nHand, float fAmp, float fMs)
	{
		if (GetConsoleFloat("VRHaptics", 1.0f) > 0.0f) VRShared::Haptic(nHand, fAmp, fMs);
	}

	// The camera eye and the BODY basis in WORLD (no head: the head is applied
	// only inside the eye renders). False with no camera.
	bool CameraFrame(LTVector& vEye, LTVector& vR, LTVector& vU, LTVector& vF)
	{
		HLOCALOBJ hCam = g_pGameClientShell ? g_pGameClientShell->GetCamera() : LTNULL;
		if (!hCam) return false;
		LTRotation rC;
		g_pLTClient->GetObjectPos(hCam, &vEye);
		g_pLTClient->GetObjectRotation(hCam, &rC);
		g_pLTClient->GetRotationVectors(&rC, &vU, &vR, &vF);
		return true;
	}

	// A TRACKING point in WORLD, measured from the head as the drawn arms are.
	LTVector TrackToWorld(const VRSharedState& s, const float* p, const LTVector& vEye,
						  const LTVector& vR, const LTVector& vU, const LTVector& vF)
	{
		const float dx = (p[0] - s.fHeadPosX) * kWorldPerMetre;
		const float dy = (p[1] - s.fHeadPosY) * kWorldPerMetre;
		const float dz = -(p[2] - s.fHeadPosZ) * kWorldPerMetre;
		return vEye + vR * dx + vU * dy + vF * dz;
	}
	LTVector TrackDirToWorld(const float* d, const LTVector& vR, const LTVector& vU, const LTVector& vF)
	{
		return vR * d[0] + vU * d[1] + vF * (-d[2]);
	}

	void ReadSwitches()
	{
		s_bHolsters = GetConsoleFloat("VRHolsters", 0.0f) > 0.0f;
		s_bManual   = GetConsoleFloat("VRManualReload", 0.0f) > 0.0f;
		s_bMelee    = GetConsoleFloat("VRSwingMelee", 0.0f) > 0.0f;
		s_bWrist    = GetConsoleFloat("VRWristHud", 0.0f) > 0.0f;
		s_bGlasses  = GetConsoleFloat("VRGlassesToFace", 0.0f) > 0.0f;
		s_bThrow    = GetConsoleFloat("VRThrowByHand", 0.0f) > 0.0f;
		s_bTwoHand  = GetConsoleFloat("VRTwoHanded", 0.0f) > 0.0f;
		const int n = (s_bHolsters ? 1 : 0) | (s_bManual ? 2 : 0) | (s_bMelee ? 4 : 0) | (s_bWrist ? 8 : 0)
					| (s_bGlasses ? 16 : 0) | (s_bThrow ? 32 : 0) | (s_bTwoHand ? 64 : 0);
		if (n != s_nSaidSwitches)
		{
			s_nSaidSwitches = n;
			VRLog::Msg("VRPhysical: holsters %s, manual reload %s, swing to hit %s, wrist display %s,"
				" glasses to the eyes %s, throw by hand %s, two hands on long guns %s"
				" (holster height %+.0f cm, swing speed %.1f m/s)",
				s_bHolsters ? "ON" : "off", s_bManual ? "ON" : "off", s_bMelee ? "ON" : "off",
				s_bWrist ? "ON" : "off", s_bGlasses ? "ON" : "off", s_bThrow ? "ON" : "off",
				s_bTwoHand ? "ON" : "off",
				GetConsoleFloat("VRHolsterHeight", 0.0f),
				GetConsoleFloat("VRSwingSpeed", 2.0f));
		}
	}

	// --- tracking ----------------------------------------------------------
	void TrackHands(const VRSharedState& s)
	{
		const bool bSwap = VRShared::SwapHands();
		if (bSwap != s_bSwapWas)
		{
			// The Leftorium changed which controller is which: old samples
			// belong to the other hand now.
			s_bSwapWas = bSwap;
			for (int h = 0; h < 2; ++h) { s_h[h].nCount = 0; s_h[h].nPeakWin = 0; }
		}
		if (s.nFrameCounter == s_nLastHostFrame) return;	// no new pose: speeds stand
		s_nLastHostFrame = s.nFrameCounter;

		for (int h = 0; h < 2; ++h)
		{
			HandTrack& t = s_h[h];
			const VRHandState& hs = s.Hands[h];
			if (!hs.nActive) { t.nCount = 0; t.fSpeed = 0.0f; t.nPeakWin = 0; continue; }
			t.nHead = (t.nHead + 1) % kRing;
			Sample& n = t.ring[t.nHead];
			n.t = s_tNow; n.p[0] = hs.fPosX; n.p[1] = hs.fPosY; n.p[2] = hs.fPosZ;
			if (t.nCount < kRing) ++t.nCount;

			// Back to the newest sample at least 30 ms older (or the oldest
			// kept), never across more than 150 ms of history.
			const Sample* k = LTNULL;
			for (int i = 1; i < t.nCount; ++i)
			{
				const Sample& c = t.ring[(t.nHead - i + kRing) % kRing];
				if (n.t - c.t > 0.15) break;
				k = &c;
				if (n.t - c.t >= 0.03) break;
			}
			if (k && n.t - k->t >= 0.02)
			{
				const float dt = (float)(n.t - k->t);
				for (int a = 0; a < 3; ++a) t.v[a] = (n.p[a] - k->p[a]) / dt;
				t.fSpeed = Len3(t.v);
			}
			else { t.v[0] = t.v[1] = t.v[2] = 0.0f; t.fSpeed = 0.0f; }

			// The peak over the last 80 ms: the hand is already slowing when
			// a grip opens or a blow lands.
			if (t.nPeakWin < 8) ++t.nPeakWin;
			for (int i = t.nPeakWin - 1; i > 0; --i) { t.tPeakWin[i] = t.tPeakWin[i - 1]; t.fPeakWin[i] = t.fPeakWin[i - 1]; }
			t.tPeakWin[0] = n.t; t.fPeakWin[0] = t.fSpeed;
			t.fPeak = 0.0f;
			for (int i = 0; i < t.nPeakWin; ++i)
				if (n.t - t.tPeakWin[i] <= 0.08 && t.fPeakWin[i] > t.fPeak) t.fPeak = t.fPeakWin[i];
		}
	}

	void TrackBody(const VRSharedState& s)
	{
		// The body faces where the head faces, dragged round only when the
		// head turns more than 45 degrees from it, a neck's allowance, the
		// same rule the drawn body stands by.
		const uint32_t nGen = VRShared::RecenterGeneration();
		if (!s_bBodyInit || nGen != s_nRecenterWas)
		{
			s_bBodyInit = true;
			s_nRecenterWas = nGen;
			s_fBodyYawDeg = s.fHeadYawDeg;
		}
		float d = s.fHeadYawDeg - s_fBodyYawDeg;
		while (d >  180.0f) d -= 360.0f;
		while (d < -180.0f) d += 360.0f;
		if (d >  45.0f) s_fBodyYawDeg += d - 45.0f;
		if (d < -45.0f) s_fBodyYawDeg += d + 45.0f;
		while (s_fBodyYawDeg >  180.0f) s_fBodyYawDeg -= 360.0f;
		while (s_fBodyYawDeg < -180.0f) s_fBodyYawDeg += 360.0f;

		// The neck: 8 cm behind and 10 cm below the eyes, in the HEAD's frame.
		const float q[4] = { s.fHeadQuatX, s.fHeadQuatY, s.fHeadQuatZ, s.fHeadQuatW };
		const float qn = Len3(q) * Len3(q) + q[3] * q[3];
		const float off[3] = { 0.0f, -0.10f, 0.08f };
		float r[3] = { off[0], off[1], off[2] };
		if (qn > 0.5f) QuatRotate(q, off, r);
		s_vNeck[0] = s.fHeadPosX + r[0];
		s_vNeck[1] = s.fHeadPosY + r[1];
		s_vNeck[2] = s.fHeadPosZ + r[2];

		static float s_fSaidYaw = -999.0f;
		if (fabsf(s_fBodyYawDeg - s_fSaidYaw) > 10.0f && (s_bHolsters || s_bManual))
		{
			s_fSaidYaw = s_fBodyYawDeg;
			float hip[3], pouch[3];
			SlotPoint(SLOT_HIP, hip);
			SlotPoint(SLOT_POUCH, pouch);
			VRLog::Msg("VRHolster: body faces %.0f deg (head %.0f), neck (%+.2f %+.2f %+.2f);"
				" %s hip (%+.2f %+.2f %+.2f), ammo pouch (%+.2f %+.2f %+.2f)",
				s_fBodyYawDeg, s.fHeadYawDeg, s_vNeck[0], s_vNeck[1], s_vNeck[2],
				VRShared::SwapHands() ? "LEFT (Leftorium)" : "right",
				hip[0], hip[1], hip[2], pouch[0], pouch[1], pouch[2]);
		}
	}

	// Which of this hand's slots it is inside, nearest first; -1 none.
	int SlotAt(int nHand, const float* p, float* pDist)
	{
		int nBest = -1; float fBest = 1e9f;
		for (int sl = 0; sl < SLOT_COUNT; ++sl)
		{
			if (kSlots[sl].nHand != nHand) continue;
			if (sl == SLOT_POUCH && !s_bManual) continue;
			if (sl != SLOT_POUCH && !s_bHolsters) continue;
			float c[3]; SlotPoint(sl, c);
			const float d = Dist3(p, c);
			if (d < kSlots[sl].r && d < fBest) { fBest = d; nBest = sl; }
		}
		if (pDist) *pDist = fBest;
		return nBest;
	}

	// --- holsters ----------------------------------------------------------
	void HolsterGrab(CWeaponModel* pWM, CPlayerStats* pStats, int nSlot, float fDist)
	{
		const int nCur  = pWM->GetWeaponId();
		const int nPick = PickForSlot(pWM, pStats, nSlot);
		const int nMelee = (int)pWM->MeleeWeapon();
		const bool bFists = (nCur == nMelee) || CategoryOf(nCur) == CAT_NONE;
		const SlotDef& d = kSlots[nSlot];

		if (nPick >= 0 && nPick == nCur)
		{
			// The slot of the weapon in the hand: it goes back.
			if (nMelee >= 0 && nMelee != nCur && g_pWeaponMgr->IsValidWeapon(nMelee))
			{
				pWM->SetHolster((uint8)nCur);
				pWM->ChangeWeapon((uint8)g_pWeaponMgr->GetCommandId(nMelee), LTFALSE, LTTRUE);
				VRLog::Msg("VRHolster: grip at the %s (%.2f m) - put the %s (weapon %d) away; weapon now %d (%s)",
					d.pName, fDist, WeaponName(nCur), nCur, pWM->GetWeaponId(), WeaponName(pWM->GetWeaponId()));
				Buzz(1, 0.35f, 25.0f);
			}
			return;
		}
		if (nPick >= 0)
		{
			pWM->ChangeWeapon((uint8)g_pWeaponMgr->GetCommandId(nPick), LTFALSE, LTTRUE);
			s_nWantWeapon = nPick; s_tWantAt = s_tNow; s_nWantFrom = nCur; s_nWantSlot = nSlot;
			VRLog::Msg("VRHolster: grip at the %s (%.2f m) - drew the %s (weapon %d%s); was %s (%d); weapon now %d",
				d.pName, fDist, WeaponName(nPick), nPick,
				s_nAssigned[nSlot] == nPick ? ", put there by hand" : "",
				WeaponName(nCur), nCur, pWM->GetWeaponId());
			Buzz(1, 0.5f, 30.0f);
			return;
		}
		// Nothing of its kind: the weapon in the hand is stowed here.
		if (!bFists && nCur >= 0 && nMelee >= 0 && g_pWeaponMgr->IsValidWeapon(nMelee))
		{
			for (int s = 0; s < SLOT_COUNT; ++s) if (s_nAssigned[s] == nCur) s_nAssigned[s] = -1;
			s_nAssigned[nSlot] = nCur;
			pWM->SetHolster((uint8)nCur);
			pWM->ChangeWeapon((uint8)g_pWeaponMgr->GetCommandId(nMelee), LTFALSE, LTTRUE);
			VRLog::Msg("VRHolster: grip at the empty %s (%.2f m) - stowed the %s (weapon %d) there; weapon now %d",
				d.pName, fDist, WeaponName(nCur), nCur, pWM->GetWeaponId());
			Buzz(1, 0.35f, 25.0f);
			return;
		}
		VRLog::Msg("VRHolster: grip at the %s (%.2f m) - it holds no %s the player has, and the hand is empty",
			d.pName, fDist, kCatName[d.nCat]);
	}

	// --- manual reload -----------------------------------------------------
	// The gun, as a segment in TRACKING space: from the gun hand along its
	// pointing direction, 22 cm, a pistol's grip to a rifle's magazine well.
	float DistToGun(const VRSharedState& s, const float* p)
	{
		const VRHandState& g = s.Hands[1];
		float f[3]; HandForward(g, f);
		const LTVector a(g.fPosX, g.fPosY, g.fPosZ);
		const LTVector b = a + LTVector(f[0], f[1], f[2]) * 0.22f;
		return SegDist(a, b, LTVector(p[0], p[1], p[2]));
	}

	void UpdateReload(const VRSharedState& s, CWeaponModel* pWM, CPlayerStats* pStats, bool bGripDownOff,
					  bool bGripUpOff, int nOffSlot, float fOffSlotDist)
	{
		const bool bApplies = ManualReloadFor(pWM);
		const int  nWeapon = pWM->GetWeaponId();
		// The clip each weapon holds, remembered for when it comes back out.
		if (bApplies && nWeapon >= 0 && nWeapon < 256)
		{
			s_nClipMem[nWeapon] = pWM->GetAmmoInClip();
			s_nClipMemAmmo[nWeapon] = pWM->GetAmmoId();
		}
		const int  nRounds = (pStats && pWM->GetAmmoId() != WMGR_INVALID_ID)
			? pStats->GetAmmoCount((uint8)pWM->GetAmmoId()) : 0;
		const bool bClipOut = bApplies && pWM->GetAmmoInClip() <= 0 && nRounds > 0;
		const float op[3] = { s.Hands[0].fPosX, s.Hands[0].fPosY, s.Hands[0].fPosZ };

		if (!bClipOut && s_bClipInHand)
		{
			s_bClipInHand = false;
			VRLog::Msg("VRReload: the clip in the off hand is not needed any more (weapon %d, clip %d)",
				nWeapon, pWM->GetAmmoInClip());
		}
		if (bClipOut && !s_bClipInHand && bGripDownOff && nOffSlot == SLOT_POUCH)
		{
			s_bClipInHand = true;
			s_h[0].bTaken = true;
			AMMO* pA = g_pWeaponMgr->GetAmmo(pWM->GetAmmoId());
			VRLog::Msg("VRReload: off hand took a clip from the ammo pouch (%.2f m) - %s, %d rounds left;"
				" off hand %.2f m from the gun", fOffSlotDist, pA ? pA->szName : "?", nRounds, DistToGun(s, op));
			Buzz(0, 0.35f, 25.0f);
		}
		if (s_bClipInHand)
		{
			if (bGripUpOff)
			{
				s_bClipInHand = false;
				VRLog::Msg("VRReload: the off hand let go of the clip %.2f m from the gun - it fell; take another",
					DistToGun(s, op));
				return;
			}
			const float fD = DistToGun(s, op);
			if (fD < 0.12f)
			{
				s_bClipInHand = false;
				pWM->ReloadClip(LTFALSE, -1, LTTRUE);
				WEAPON* pW = pWM->GetWeapon();
				if (pW && pW->szReloadSounds[0][0])
					g_pClientSoundMgr->PlaySoundLocal(pW->szReloadSounds[0], SOUNDPRIORITY_PLAYER_HIGH);
				VRLog::Msg("VRReload: clip IN - %s now %d in the clip (%d rounds in all); the off hand reached"
					" %.2f m from the gun", pW ? pW->szName : "?", pWM->GetAmmoInClip(), nRounds, fD);
				Buzz(1, 0.6f, 35.0f);
			}
		}
	}

	// --- melee -------------------------------------------------------------
	void UpdateMelee(const VRSharedState& s, CWeaponModel* pWM, bool bRiding)
	{
		LTVector vEye, vR, vU, vF;
		const bool bCam = CameraFrame(vEye, vR, vU, vF);
		for (int h = 0; h < 2; ++h)
		{
			HandTrack& t = s_h[h];
			const VRHandState& hs = s.Hands[h];
			if (!bCam || !hs.nActive) { t.bWorldValid = false; continue; }
			const float p[3] = { hs.fPosX, hs.fPosY, hs.fPosZ };
			t.vWorldPrev = t.bWorldValid ? t.vWorldNow : TrackToWorld(s, p, vEye, vR, vU, vF);
			t.vWorldNow  = TrackToWorld(s, p, vEye, vR, vU, vF);
			t.bWorldValid = true;
		}
		if (s_hHitTarget && g_pGameClientShell)
		{
			// DID IT GO DOWN? The game removes a character the moment it is
			// knocked out or killed and leaves a body in its place.
			CCharacterFX* pC = g_pGameClientShell->GetSFXMgr()->GetCharacterFX(s_hHitTarget);
			if (!pC)
			{
				VRLog::Msg("VRMelee: the character struck %.2f s ago is DOWN (its character is gone)", s_tNow - s_tHitAt);
				s_hHitTarget = LTNULL;
			}
			else if (s_tNow - s_tHitAt > 3.0)
			{
				VRLog::Msg("VRMelee: the character struck 3 s ago is still up (the game's melee rule: only a blow"
					" to the head of someone unaware takes a person down)");
				s_hHitTarget = LTNULL;
			}
		}
		if (!s_bMelee || bRiding || !bCam || !g_pGameClientShell || !g_pModelButeMgr) return;

		const float fThr = GetConsoleFloat("VRSwingSpeed", 2.0f);
		CSpecialFXList* pList = g_pGameClientShell->GetSFXMgr()->GetFXList(SFX_CHARACTER_ID);
		if (!pList) return;
		HOBJECT hMe = g_pLTClient->GetClientObject();
		// WHO IS WITHIN REACH OF A WALK: the nearest characters, a few times a
		// level, so a run can be read against where people stood.
		{
			static double s_tSaidNear = -100.0;
			static int s_nSaidNear = 0;
			if (s_nSaidNear < 3 && s_tNow - s_tSaidNear > 10.0)
			{
				s_tSaidNear = s_tNow; ++s_nSaidNear;
				const int nFX = pList->GetSize();
				int nSaid = 0;
				for (int i = 0; i < nFX && nSaid < 6; ++i)
				{
					CCharacterFX* pC = (CCharacterFX*)(*pList)[i];
					if (!pC || !pC->GetServerObj() || pC->GetServerObj() == hMe || pC->VRIsPlayer()) continue;
					LTVector vC; g_pLTClient->GetObjectPos(pC->GetServerObj(), &vC);
					const float fD = (vC - vEye).Mag();
					if (fD > 2000.0f) continue;
					++nSaid;
					// Its head, in the player's own right/up/forward metres from the eye.
					LTVector vHead = vC;
					const ModelSkeleton eSk = pC->GetModelSkeleton();
					const int nNodes = g_pModelButeMgr->GetSkeletonNumNodes(eSk);
					for (int n = 0; n < nNodes; ++n)
					{
						if (g_pModelButeMgr->GetSkeletonNodeDamageFactor(eSk, (ModelNode)n) < 50.0f) continue;
						HMODELNODE hN; LTransform tf;
						const char* pszN = g_pModelButeMgr->GetSkeletonNodeName(eSk, (ModelNode)n);
						if (pszN && g_pModelLT->GetNode(pC->GetServerObj(), (char*)pszN, hN) == LT_OK
							&& g_pModelLT->GetNodeTransform(pC->GetServerObj(), hN, tf, LTTRUE) == LT_OK)
						{ g_pTransLT->GetPos(tf, vHead); break; }
					}
					const LTVector rel = vHead - vEye;
					VRLog::Msg("VRMelee: a character at (%.0f %.0f %.0f), %.0f units from the eye (%.0f %.0f %.0f);"
						" its head %.2f right %.2f up %.2f ahead (m)", vC.x, vC.y, vC.z, fD, vEye.x, vEye.y, vEye.z,
						rel.Dot(vR) / kWorldPerMetre, rel.Dot(vU) / kWorldPerMetre, rel.Dot(vF) / kWorldPerMetre);
				}
				if (!nSaid) VRLog::Msg("VRMelee: no character within 2000 units of the eye");
			}
		}
		const int nMelee = (int)pWM->MeleeWeapon();
		const bool bGunInHand = (pWM->GetWeaponId() != nMelee) && CategoryOf(pWM->GetWeaponId()) != CAT_NONE;

		for (int h = 0; h < 2; ++h)
		{
			HandTrack& t = s_h[h];
			if (!t.bWorldValid) continue;
			// A gun swung to aim must not club anyone: the gun hand holding a
			// gun needs a quarter more speed to count as a blow (pistol-whip).
			const float fNeed = (h == 1 && bGunInHand) ? fThr * 1.25f : fThr;
			LTVector a = t.vWorldPrev, b = t.vWorldNow;
			LTVector dir = b - a;
			const float fLen = dir.Mag();
			if (fLen > 0.01f) dir = dir * (1.0f / fLen);
			// The fist is not a point: reach 5 units (8 cm) past the tracked hand.
			b = b + dir * 5.0f;

			// The nearest node this frame's path passes within its hit radius.
			CCharacterFX* pBestC = LTNULL; int nBestNode = -1; float fBestGap = 1e9f;
			LTVector vBestNode(0, 0, 0);
			const int nFX = pList->GetSize();
			for (int i = 0; i < nFX; ++i)
			{
				CCharacterFX* pC = (CCharacterFX*)(*pList)[i];
				if (!pC) continue;
				HOBJECT hC = pC->GetServerObj();
				if (!hC || hC == hMe || pC->VRIsPlayer()) continue;
				LTVector vC; g_pLTClient->GetObjectPos(hC, &vC);
				if ((vC - b).Mag() > 160.0f) continue;
				const ModelSkeleton eSk = pC->GetModelSkeleton();
				const int nNodes = g_pModelButeMgr->GetSkeletonNumNodes(eSk);
				for (int n = 0; n < nNodes; ++n)
				{
					const char* pszNode = g_pModelButeMgr->GetSkeletonNodeName(eSk, (ModelNode)n);
					if (!pszNode || !pszNode[0]) continue;
					HMODELNODE hN;
					if (g_pModelLT->GetNode(hC, (char*)pszNode, hN) != LT_OK) continue;
					LTransform tf;
					if (g_pModelLT->GetNodeTransform(hC, hN, tf, LTTRUE) != LT_OK) continue;
					LTVector vN; g_pTransLT->GetPos(tf, vN);
					const float fR = g_pModelButeMgr->GetSkeletonNodeHitRadius(eSk, (ModelNode)n) + 4.0f;
					const float fGap = SegDist(a, b, vN) - fR;
					if (fGap < fBestGap) { fBestGap = fGap; pBestC = pC; nBestNode = n; vBestNode = vN; }
				}
			}
			if (!pBestC || fBestGap > 0.0f) { t.nSaidPass = -1; continue; }

			HOBJECT hC = pBestC->GetServerObj();
			const ModelSkeleton eSk = pBestC->GetModelSkeleton();
			const char* pszNode = g_pModelButeMgr->GetSkeletonNodeName(eSk, (ModelNode)nBestNode);
			if (t.fSpeed < fNeed)
			{
				// Through a character too slowly to be a blow: say so once a pass.
				if (t.nSaidPass != nBestNode)
				{
					t.nSaidPass = nBestNode;
					VRLog::Msg("VRMelee: %s hand passed through %s at %.2f m/s - under %.2f, no blow",
						h ? "gun" : "off", pszNode, t.fSpeed, fNeed);
				}
				continue;
			}
			if (s_tNow - t.tLastHit < GetConsoleFloat("VRSwingCooldown", 0.3f)) continue;

			// NOT THROUGH A WALL: the eye must see the point struck.
			{
				ClientIntersectQuery q; ClientIntersectInfo ii;
				q.m_From = vEye; q.m_To = vBestNode;
				q.m_Flags = INTERSECT_HPOLY | IGNORE_NONSOLID;
				HOBJECT hFilter[] = { hMe, hC, LTNULL };
				q.m_FilterFn = ObjListFilterFn; q.m_pUserData = hFilter;
				if (g_pLTClient->IntersectSegment(&q, &ii) && IsMainWorld(ii.m_hObject)
					&& (ii.m_Point - vEye).Mag() + 2.0f < (vBestNode - vEye).Mag())
				{
					VRLog::Msg("VRMelee: %s hand at %.2f m/s met %s through the world - no blow",
						h ? "gun" : "off", t.fSpeed, pszNode);
					t.tLastHit = s_tNow;
					continue;
				}
			}

			// THE GAME'S OWN MELEE: its damage type and amount (the fists' ammo),
			// times where it lands (the skeleton's factor, 100 on the head) and
			// how fast the hand was going (1x at the threshold, up to 3x).
			AMMO* pMeleeAmmo = g_pWeaponMgr ? g_pWeaponMgr->GetAmmo("Melee") : LTNULL;
			const float fBase = pMeleeAmmo ? (float)pMeleeAmmo->nInstDamage : 2.0f;
			const float fNodeK = g_pModelButeMgr->GetSkeletonNodeDamageFactor(eSk, (ModelNode)nBestNode);
			float fSpeedK = t.fSpeed / fNeed;
			if (fSpeedK > 3.0f) fSpeedK = 3.0f;
			const float fDamage = fBase * fNodeK * fSpeedK;
			LTVector vDirW = TrackDirToWorld(t.v, vR, vU, vF);
			if (vDirW.Mag() > 0.001f) vDirW.Norm(); else vDirW = vF;

			HMESSAGEWRITE hMsg = g_pLTClient->StartMessage(MID_PLAYER_CLIENTMSG);
			g_pLTClient->WriteToMessageByte(hMsg, CP_DAMAGE);
			g_pLTClient->WriteToMessageByte(hMsg, (uint8)DT_MELEE);
			g_pLTClient->WriteToMessageFloat(hMsg, fDamage);
			g_pLTClient->WriteToMessageVector(hMsg, &vDirW);
			g_pLTClient->WriteToMessageByte(hMsg, 0);
			g_pLTClient->WriteToMessageObject(hMsg, hC);
			g_pLTClient->EndMessage(hMsg);

			t.tLastHit = s_tNow;
			s_hHitTarget = hC; s_tHitAt = s_tNow; s_nHitNode = nBestNode;
			// The fists' own impact sound where the blow lands.
			if (pMeleeAmmo && pMeleeAmmo->pImpactFX && pMeleeAmmo->pImpactFX->szSound[0])
				g_pClientSoundMgr->PlaySoundFromPos(vBestNode, pMeleeAmmo->pImpactFX->szSound,
					(LTFLOAT)pMeleeAmmo->pImpactFX->nSoundRadius, SOUNDPRIORITY_PLAYER_HIGH);
			float fK = (t.fSpeed - fNeed) / 4.0f;
			if (fK < 0.0f) fK = 0.0f;
			if (fK > 1.0f) fK = 1.0f;
			Buzz(h, 0.5f + 0.5f * fK, 40.0f + 40.0f * fK);
			LTVector vCpos; g_pLTClient->GetObjectPos(hC, &vCpos);
			VRLog::Msg("VRMelee: %s hand at %.2f m/s (peak %.2f) HIT %s (x%.2f) of the character at"
				" (%.0f %.0f %.0f) - damage %.0f x %.2f x %.2f = %.1f, melee, sent",
				h ? "gun" : "off", t.fSpeed, t.fPeak, pszNode, fNodeK, vCpos.x, vCpos.y, vCpos.z,
				fBase, fNodeK, fSpeedK, fDamage);
		}
	}
	// --- the wrist display ---------------------------------------------------
	// A controller's orientation as the host's angles compose it (yaw about +Y,
	// then pitch about +X, then roll about +Z), and its axes from that.
	void HandAxes(const VRHandState& h, float* r, float* u, float* f)
	{
		const float hy = h.fYawDeg * kD2R * 0.5f, hp = h.fPitchDeg * kD2R * 0.5f, hr = h.fRollDeg * kD2R * 0.5f;
		const float qy[4] = { 0, (float)sin(hy), 0, (float)cos(hy) };
		const float qp[4] = { (float)sin(hp), 0, 0, (float)cos(hp) };
		const float qr[4] = { 0, 0, (float)sin(hr), (float)cos(hr) };
		auto Mul = [](const float* a, const float* b, float* o)
		{
			o[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
			o[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
			o[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
			o[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
		};
		float t[4], q[4];
		Mul(qy, qp, t); Mul(t, qr, q);
		const float x[3] = { 1, 0, 0 }, y[3] = { 0, 1, 0 }, z[3] = { 0, 0, -1 };
		QuatRotate(q, x, r); QuatRotate(q, y, u); QuatRotate(q, z, f);
	}

	// --- the sunglasses, put on by bringing them to the eyes ----------------
	// Cate's sunglasses are the game's camera, mine detector and infrared
	// viewer, and the flat game puts them on the moment they are drawn. Here
	// they stay in the hand until the hand brings them to the eyes (within
	// VRGlassesOnCm of the headset): then they are ON, in whichever mode the
	// ammo row has chosen, and they STAY on when the hand comes down, a hand
	// held at the face would fill the view. While worn the glasses in the hand
	// are not drawn and the trigger takes the photo, as in the flat game.
	// OFF: the hand back at the eyes with the grip squeezed (once it has been
	// away from them, past VRGlassesOffCm, since they went on), or any
	// change of weapon. The game sets its mode itself on its own animation
	// keys, so the answer is enforced every frame, not just on a change.
	bool IsSunglasses(CWeaponModel* pWM)
	{
		AMMO* pA = pWM ? pWM->GetAmmo() : LTNULL;
		if (!pA || pA->eType != GADGET) return false;
		return pA->eInstDamageType == DT_GADGET_CAMERA || pA->eInstDamageType == DT_GADGET_MINE_DETECTOR
			|| pA->eInstDamageType == DT_GADGET_INFRA_RED;
	}

	bool s_bGlassesArmed = false;	// worn, and the hand has left the face since
	bool s_bGlassesAway  = true;	// taken off: the hand must leave the face before they go back on

	void UpdateGlasses(const VRSharedState& s, CWeaponModel* pWM, bool bAlive, bool bGripDown)
	{
		// What the gadget in hand is, once per change: the log that says why a
		// pair of glasses was or was not taken for sunglasses.
		{
			static int s_nSaidAmmo = -2;
			AMMO* pA = pWM ? pWM->GetAmmo() : LTNULL;
			const int nAmmo = pWM ? pWM->GetAmmoId() : -1;
			if (s_bGlasses && nAmmo != s_nSaidAmmo)
			{
				s_nSaidAmmo = nAmmo;
				VRLog::Msg("VRGlasses: weapon %d ammo %d type %d damage type %d - %s", pWM ? pWM->GetWeaponId() : -1,
					nAmmo, pA ? (int)pA->eType : -1, pA ? (int)pA->eInstDamageType : -1,
					IsSunglasses(pWM) ? "sunglasses, put on at the eyes" : "not a sunglasses mode");
			}
		}
		const bool bHeld = s_bGlasses && bAlive && IsSunglasses(pWM) && !pWM->IsDisabled()
						   && s.Hands[1].nActive;
		if (!bHeld)
		{
			if (s_bGlassesOn)
			{
				s_bGlassesOn = false;
				VRLog::Msg("VRGlasses: put away - off");
			}
			s_bGlassesArmed = false;
			s_bGlassesAway = true;
			return;
		}
		const float dx = s.Hands[1].fPosX - s.fHeadPosX;
		const float dy = s.Hands[1].fPosY - s.fHeadPosY;
		const float dz = s.Hands[1].fPosZ - s.fHeadPosZ;
		const float fDist = (float)sqrt(dx * dx + dy * dy + dz * dz);
		const float fOn  = 0.01f * GetConsoleFloat("VRGlassesOnCm", 16.0f);
		const float fOff = 0.01f * GetConsoleFloat("VRGlassesOffCm", 24.0f);
		if (!s_bGlassesOn)
		{
			if (fDist > fOff) s_bGlassesAway = true;
			if (fDist < fOn && s_bGlassesAway)
			{
				s_bGlassesOn = true;
				s_bGlassesArmed = false;
				Buzz(1, 0.3f, 25.0f);
				VRLog::Msg("VRGlasses: ON - brought to the eyes (hand %.2f m from them)", fDist);
			}
		}
		else
		{
			if (fDist > fOff && !s_bGlassesArmed) s_bGlassesArmed = true;
			if (s_bGlassesArmed && bGripDown && fDist < fOn)
			{
				s_bGlassesOn = false;
				s_bGlassesArmed = false;
				s_bGlassesAway = false;
				s_h[1].bTaken = true;		// this grip is ours until it opens
				Buzz(1, 0.15f, 12.0f);
				VRLog::Msg("VRGlasses: OFF - taken off at the eyes (hand %.2f m from them)", fDist);
			}
		}
		if (s_bGlassesOn)
		{
			if (g_pInterfaceMgr->GetSunglassMode() == SUN_NONE) pWM->GadgetDisable(LTFALSE);
		}
		else if (g_pInterfaceMgr->GetSunglassMode() != SUN_NONE)
		{
			g_pInterfaceMgr->SetSunglassMode(SUN_NONE);
		}
	}

	void UpdateWrist(const VRSharedState& s, CPlayerStats* pStats, bool bShow)
	{
		static int s_nSaidState = -1;
		const VRHandState& h = s.Hands[0];			// the off hand, whichever side the Leftorium put it
		if (!s_bWrist || !bShow || !h.nActive || !pStats) { s_nSaidState = -1; return; }
		LTVector vEye, vR, vU, vF;
		if (!CameraFrame(vEye, vR, vU, vF)) return;
		int nW = 0, nH = 0;
		HSURFACE hSurf = pStats->VRWristSurface(&nW, &nH);
		if (!hSurf || nW <= 0 || nH <= 0) return;

		float cr[3], cu[3], cf[3];
		HandAxes(h, cr, cu, cf);
		// THE BACK OF THE HAND. A hand around a controller has its palm on the
		// controller's inner side, so the back of the hand faces the outer
		// side: -right for the left hand, +right for the right. The top of
		// the controller (its up) is NOT it: a hand resting on a desk points
		// the top up toward the eyes by 0.55 to 0.65 (measured in the headset)
		// and kept the panel faintly on. Turned like a watch, the forearm
		// rolls and the back of the hand faces the eyes. VRWristFace 1 goes
		// back to the controller's top.
		const float fOut = VRShared::SwapHands() ? 1.0f : -1.0f;
		float cn[3];
		const bool bTop = GetConsoleInt("VRWristFace", 0) > 0;
		for (int a = 0; a < 3; ++a) cn[a] = bTop ? cu[a] : cr[a] * fOut;
		// On the back of the wrist: behind the controller (toward the elbow)
		// and out on the back of the hand, facing the way it does. Clear of
		// Cate's glove: at 0.13 back and 0.035 out the panel's front edge was
		// inside the back of her hand, so it now sits over the forearm, past
		// the cuff, and further out.
		const float fBack = GetConsoleFloat("VRWristBack", 0.16f);
		const float fUp   = GetConsoleFloat("VRWristUp", 0.055f);
		const float fWide = GetConsoleFloat("VRWristSize", 0.085f);
		const float fTall = fWide * (float)nH / (float)nW;
		float c[3];
		for (int a = 0; a < 3; ++a) c[a] = (&h.fPosX)[a] - cf[a] * fBack + cn[a] * fUp;
		// Read along the forearm: text runs toward the fingers on the left
		// wrist and toward the elbow on the right (the Leftorium), so it reads
		// left to right with the arm held across the chest either way; its up
		// is then the textbook normal x right.
		const float fSide = VRShared::SwapHands() ? -1.0f : 1.0f;
		const float tr[3] = { cf[0] * fSide, cf[1] * fSide, cf[2] * fSide };
		const float tu[3] = { cn[1] * tr[2] - cn[2] * tr[1], cn[2] * tr[0] - cn[0] * tr[2], cn[0] * tr[1] - cn[1] * tr[0] };
		// Shown as the back of the wrist turns to the eyes, gone as it turns away.
		float toEye[3] = { s.fHeadPosX - c[0], s.fHeadPosY - c[1], s.fHeadPosZ - c[2] };
		const float fL = Len3(toEye);
		if (fL > 0.001f) for (int a = 0; a < 3; ++a) toEye[a] /= fL;
		const float fFace = cn[0] * toEye[0] + cn[1] * toEye[1] + cn[2] * toEye[2];
		// ONLY WHEN TURNED LIKE A WATCH: it starts at VRWristFrom (0.70) and is
		// solid by VRWristFull (0.88); straight at the eyes is 1.
		float fFrom = GetConsoleFloat("VRWristFrom", 0.70f);
		float fFull = GetConsoleFloat("VRWristFull", 0.88f);
		if (fFull < fFrom + 0.05f) fFull = fFrom + 0.05f;
		float fAlpha = (fFace - fFrom) / (fFull - fFrom);
		if (fAlpha < 0.0f) fAlpha = 0.0f;
		if (fAlpha > 1.0f) fAlpha = 1.0f;
		const int nState = (fAlpha > 0.0f) ? 1 : 0;
		if (nState != s_nSaidState)
		{
			s_nSaidState = nState;
			VRLog::Msg("VRWrist: %s - the back of the %s wrist faces the eyes by %.2f (alpha %.2f), %.2f m from them;"
				" panel %.3f x %.3f m at (%+.2f %+.2f %+.2f)", nState ? "SHOWN" : "hidden (turned away)",
				VRShared::SwapHands() ? "right" : "left", fFace, fAlpha, fL, fWide, fTall, c[0], c[1], c[2]);
		}
		if (fAlpha <= 0.0f) return;
		float p[4][3];
		const float sx[4] = { -0.5f, 0.5f, 0.5f, -0.5f }, sy[4] = { 0.5f, 0.5f, -0.5f, -0.5f };
		LTVector vW[4];
		for (int k = 0; k < 4; ++k)
		{
			for (int a = 0; a < 3; ++a) p[k][a] = c[a] + tr[a] * sx[k] * fWide + tu[a] * sy[k] * fTall;
			vW[k] = TrackToWorld(s, p[k], vEye, vR, vU, vF);
		}
		VRPrims_SetSurfaceQuad(hSurf, vW, fAlpha);
	}
}	// namespace

const char* SlotName(int nSlot)
{
	return (nSlot >= 0 && nSlot < SLOT_COUNT) ? kSlots[nSlot].pName : "none";
}

bool GripTaken(int nHand)
{
	return (nHand == 0 || nHand == 1) && s_h[nHand].bTaken;
}

void Idle()
{
	for (int h = 0; h < 2; ++h)
	{
		s_h[h].bTaken = false;
		s_h[h].bGripWas = false;
		s_h[h].nInSlot = -1;
		s_h[h].bWorldValid = false;
	}
	s_bClipInHand = false;
}

void OnWorldEntered()
{
	for (int i = 0; i < 256; ++i) { s_nClipMem[i] = -1; s_nClipMemAmmo[i] = -1; }
	s_bClipMemInit = true;
	s_hHitTarget = LTNULL;
	Idle();
}

bool ManualReloadFor(CWeaponModel* pWM)
{
	if (!s_bManual || !pWM || !VRShared::IsLive()) return false;
	WEAPON* pW = pWM->GetWeapon();
	AMMO* pA = pWM->GetAmmo();
	if (!pW || !pA) return false;
	if (pW->bInfiniteAmmo || pW->nShotsPerClip < 1 || pA->eType == GADGET) return false;
	const int nCat = CategoryOf(pWM->GetWeaponId());
	if (nCat != CAT_SIDEARM && nCat != CAT_LONG) return false;
	return pWM->VRHasReloadAni() ? true : false;
}

bool OnReloadCommand(CWeaponModel* pWM)
{
	if (!ManualReloadFor(pWM)) return false;
	const int n = pWM->GetAmmoInClip();
	if (n > 0)
	{
		pWM->VRSetAmmoInClip(0);
		VRLog::Msg("VRReload: reload button - clip EJECTED from the %s with %d rounds (they go back to the"
			" pouches); now dry until a clip goes in", pWM->GetWeapon()->szName, n);
		Buzz(1, 0.3f, 20.0f);
	}
	else
		VRLog::Msg("VRReload: reload button - the clip is already out; bring one from the ammo pouch");
	return true;
}

void OnClipRanOut(CWeaponModel* pWM)
{
	VRLog::Msg("VRReload: the %s fired its clip empty - it drops; the next clip comes from the ammo pouch",
		(pWM && pWM->GetWeapon()) ? pWM->GetWeapon()->szName : "?");
	Buzz(1, 0.25f, 20.0f);
}

void OnSelect(CWeaponModel* pWM)
{
	if (!ManualReloadFor(pWM) || !s_bClipMemInit) return;
	const int nW = pWM->GetWeaponId();
	if (nW < 0 || nW >= 256 || s_nClipMem[nW] < 0 || s_nClipMemAmmo[nW] != pWM->GetAmmoId()) return;
	const int nFull = pWM->GetAmmoInClip();
	const int nKeep = (s_nClipMem[nW] < nFull) ? s_nClipMem[nW] : nFull;
	if (nKeep != nFull)
	{
		pWM->VRSetAmmoInClip(nKeep);
		VRLog::Msg("VRReload: the %s comes out with the clip it was put away with: %d (a fresh one would be %d)",
			pWM->GetWeapon()->szName, nKeep, nFull);
	}
}

void Update(CGameClientShell* pShell, bool bRiding)
{
	// A CUTSCENE IS NOT PLAY: the level's camera has the view, so nothing of
	// physical play (the wrist display, a slot's tick, a throw) belongs in it.
	if (pShell && (pShell->IsUsingExternalCamera() || !pShell->IsFirstPerson())) { Idle(); return; }
	ReadSwitches();
	s_tNow = NowSec();
	if (!s_bClipMemInit) OnWorldEntered();

	const VRSharedState& s = VRShared::State();
	TrackHands(s);
	TrackBody(s);

	CWeaponModel* pWM = pShell ? pShell->GetWeaponModel() : LTNULL;
	CPlayerStats* pStats = pShell ? pShell->GetPlayerStats() : LTNULL;
	if (!pWM || !pStats || !g_pWeaponMgr) return;

	const bool bAlive = pShell->IsPlayerInWorld() && !pShell->IsPlayerDead();
	const int nCurW = pWM->GetWeaponId();
	{
		const int c = CategoryOf(nCurW);
		if (c >= 0) s_nLastUsed[c] = nCurW;
	}
	if (s_nWantWeapon >= 0 && (nCurW == s_nWantWeapon || s_tNow - s_tWantAt > 1.0))
	{
		VRLog::Msg("VRHolster: %s from the %s: weapon %d -> %d (%s), %.2f s after the grip",
			nCurW == s_nWantWeapon ? "IN HAND" : "NOT drawn", SlotName(s_nWantSlot), s_nWantFrom, nCurW,
			WeaponName(nCurW), s_tNow - s_tWantAt);
		s_nWantWeapon = -1;
	}

	// Grip edges per hand, by the analog value or the button.
	bool bDown[2], bUp[2];
	for (int h = 0; h < 2; ++h)
	{
		const VRHandState& hs = s.Hands[h];
		const bool bGrip = hs.nActive && ((hs.nButtons & VRBTN_GRIP) != 0 || hs.fGrip > 0.5f);
		bDown[h] = bGrip && !s_h[h].bGripWas;
		bUp[h] = !bGrip && s_h[h].bGripWas;
		s_h[h].bGripWas = bGrip;
		if (bUp[h]) s_h[h].bTaken = false;
	}

	// Which slot each hand is in. A tick in the hand when it arrives, so a
	// slot can be found without looking.
	int nSlotOf[2] = { -1, -1 }; float fSlotDist[2] = { 0, 0 };
	for (int h = 0; h < 2; ++h)
	{
		const VRHandState& hs = s.Hands[h];
		if (!hs.nActive || !bAlive || bRiding) { s_h[h].nInSlot = -1; continue; }
		const float p[3] = { hs.fPosX, hs.fPosY, hs.fPosZ };
		nSlotOf[h] = SlotAt(h, p, &fSlotDist[h]);
		// The ammo pouch only means something with the clip out.
		if (nSlotOf[h] == SLOT_POUCH && !(ManualReloadFor(pWM) && pWM->GetAmmoInClip() <= 0))
			nSlotOf[h] = -1;
		if (nSlotOf[h] != s_h[h].nInSlot)
		{
			if (nSlotOf[h] >= 0)
			{
				Buzz(h, 0.15f, 12.0f);
				static int s_nSaidReach = 0;
				if (s_nSaidReach++ < 400)
				{
					const int nPick = (nSlotOf[h] != SLOT_POUCH) ? PickForSlot(pWM, pStats, nSlotOf[h]) : -1;
					VRLog::Msg("VRHolster: %s hand reached the %s (%.2f m from its centre)%s%s",
						h ? "gun" : "off", SlotName(nSlotOf[h]), fSlotDist[h],
						nSlotOf[h] != SLOT_POUCH ? " - holds " : "",
						nSlotOf[h] != SLOT_POUCH ? WeaponName(nPick) : "");
				}
			}
			s_h[h].nInSlot = nSlotOf[h];
		}
	}

	// HOLSTERS: the gun hand's grip closing inside one of its slots.
	if (s_bHolsters && bAlive && !bRiding && bDown[1] && nSlotOf[1] >= 0 && nSlotOf[1] != SLOT_POUCH
		&& !pWM->IsDisabled())
	{
		s_h[1].bTaken = true;
		HolsterGrab(pWM, pStats, nSlotOf[1], fSlotDist[1]);
	}

	// MANUAL RELOAD: the off hand and the pouch.
	if (bAlive && !bRiding)
		UpdateReload(s, pWM, pStats, bDown[0], bUp[0], nSlotOf[0], fSlotDist[0]);
	else
		s_bClipInHand = false;

	// SWING MELEE.
	UpdateMelee(s, pWM, bRiding || !bAlive);

	// THE WRIST DISPLAY.
	UpdateWrist(s, pStats, bAlive);

	// THE SUNGLASSES, at the eyes.
	UpdateGlasses(s, pWM, bAlive && !bRiding, bDown[1]);

	// TWO HANDS ON A LONG GUN. The off hand's grip closing in front of the gun
	// hand, near the line the gun points along, takes the front of the gun:
	// from then until that grip opens the gun aims from the gun hand to the off
	// hand, as a rifle held in two hands does, steadier, and turned by both
	// arms. The gun hand keeps its roll. Rifles, SMGs, the shotgun and the
	// launchers (the long-gun holster's list); a pistol's support hand is the
	// look it always was. The grip is taken, so it does not also run.
	{
		const bool bLong = s_bTwoHand && bAlive && !bRiding && CategoryOf(nCurW) == CAT_LONG
						   && !pWM->IsDisabled() && s.Hands[0].nActive && s.Hands[1].nActive;
		const float r[3] = { s.Hands[0].fPosX - s.Hands[1].fPosX, s.Hands[0].fPosY - s.Hands[1].fPosY,
							 s.Hands[0].fPosZ - s.Hands[1].fPosZ };
		const float fDist = Len3(r);
		const bool bOffGrip = s.Hands[0].nActive && ((s.Hands[0].nButtons & VRBTN_GRIP) != 0 || s.Hands[0].fGrip > 0.5f);
		if (!s_bTwoHandOn)
		{
			if (bLong && bDown[0] && nSlotOf[0] < 0 && !s_h[0].bTaken)
			{
				float f[3]; HandForward(s.Hands[1], f);
				const float fAlong = r[0] * f[0] + r[1] * f[1] + r[2] * f[2];
				const float px = r[0] - f[0] * fAlong, py = r[1] - f[1] * fAlong, pz = r[2] - f[2] * fAlong;
				const float fOff = (float)sqrt(px * px + py * py + pz * pz);
				if (fAlong > GetConsoleFloat("VRTwoHandMinAhead", 0.10f) && fDist < GetConsoleFloat("VRTwoHandReach", 0.65f)
					&& fOff < GetConsoleFloat("VRTwoHandOffLine", 0.20f))
				{
					s_bTwoHandOn = true;
					s_h[0].bTaken = true;
					Buzz(0, 0.25f, 18.0f);
					VRLog::Msg("VRTwoHand: the off hand took the %s's front, %.2f m ahead and %.2f m off its line",
						WeaponName(nCurW), fAlong, fOff);
				}
			}
		}
		else if (!bLong || !bOffGrip || fDist > 0.85f)
		{
			s_bTwoHandOn = false;
			VRLog::Msg("VRTwoHand: let go (%s)", !bLong ? "no long gun" : !bOffGrip ? "the grip opened" : "the hands too far apart");
		}
		if (s_bTwoHandOn && fDist > 0.05f)
		{
			// The host's own angles for a direction (HandForward, inverted):
			// forward (-sin y cos p, sin p, -cos y cos p).
			const float d[3] = { r[0] / fDist, r[1] / fDist, r[2] / fDist };
			const float fYaw = (float)atan2(-d[0], -d[2]) * 57.29578f;
			const float fPitch = (float)asin(d[1] < -1.0f ? -1.0f : d[1] > 1.0f ? 1.0f : d[1]) * 57.29578f;
			static int s_nSaid = 0;
			if ((s_nSaid++ % 90) == 0)
				VRLog::Msg("VRTwoHand: aiming yaw %.1f pitch %.1f (the gun hand alone: yaw %.1f pitch %.1f)",
					fYaw, fPitch, s.Hands[1].fYawDeg, s.Hands[1].fPitchDeg);
			VRShared::OverrideGunAim(fYaw, fPitch);
		}
	}
}

bool TwoHanded()
{
	return s_bTwoHand && s_bTwoHandOn;
}

bool GlassesBlockFire(CWeaponModel* pWM)
{
	return s_bGlasses && IsSunglasses(pWM) && !s_bGlassesOn;
}

bool GlassesWorn()
{
	return s_bGlasses && s_bGlassesOn;
}

// --- THROWN BY HAND ----------------------------------------------------------
// The coin and the lipsticks are the player's throwables. With VRThrowByHand the
// trigger READIES a throw instead of firing, and letting it go throws along the
// way the hand is moving at that moment, as fast as the hand was moving: a hand
// at VRThrowFullSpeed (4 m/s, a firm overarm throw) throws at the game's own
// speed for that item, a harder one up to 1.5 times it, a lob down to 0.3. A
// hand let go slower than VRThrowMinSpeed (1 m/s) has not thrown, nothing
// leaves it. The speed travels to the server with the fire message, which uses
// the game's own velocity override (the one its AI grenade throws use).
static bool IsThrowable(CWeaponModel* pWM)
{
	AMMO* pA = pWM ? pWM->GetAmmo() : LTNULL;
	if (!pA || pA->eType != PROJECTILE) return false;
	char sz[64];
	strncpy(sz, pA->szName, sizeof(sz) - 1); sz[sizeof(sz) - 1] = 0;
	for (char* c = sz; *c; ++c) *c = (char)tolower((unsigned char)*c);
	return strstr(sz, "coin") != LTNULL || strstr(sz, "lipstick") != LTNULL;
}

bool ThrowTrigger(CWeaponModel* pWM, bool bTrigger, bool* pbFire)
{
	*pbFire = false;
	if (!s_bThrow || !IsThrowable(pWM) || pWM->IsDisabled())
	{
		s_nThrowState = 0;
		return false;
	}
	if (s_nThrowState == 3 && (s_bThrowSent || s_tNow - s_tThrowAt > 1.5)) s_nThrowState = 0;
	if (s_nThrowState == 2)
	{
		// Held until the weapon has sent it, the throw animation lets go of
		// the coin about 0.67 s after the fire starts (desk), and never longer
		// than VRThrowHoldSec. What was thrown stays waiting for the fire
		// message a little longer still (TakeThrow).
		if (s_bThrowSent || s_tNow - s_tThrowAt > GetConsoleFloat("VRThrowHoldSec", 1.0f))
		{
			if (!s_bThrowSent) VRLog::Msg("VRThrow: the weapon did not fire it within %.1f s", GetConsoleFloat("VRThrowHoldSec", 1.0f));
			s_nThrowState = s_bThrowSent ? 0 : 3;	// 3: no longer held, still waiting to be sent
		}
		else *pbFire = true;
		return true;
	}
	// THE FASTEST MOMENT OF THE LAST 100 MS, speed AND direction: a hand is
	// already slowing, or stopped, when the fingers open, so neither can be
	// read at the release itself (desk: a 5.7 m/s swing let go 20 ms after it
	// ended read 0 m/s there).
	static double s_tSwing[12]; static float s_vSwing[12][3]; static float s_fSwing[12];
	static int s_nSwing = 0;
	if (s_nThrowState == 1 || bTrigger)
	{
		for (int i = 11; i > 0; --i)
		{
			s_tSwing[i] = s_tSwing[i - 1]; s_fSwing[i] = s_fSwing[i - 1];
			for (int a = 0; a < 3; ++a) s_vSwing[i][a] = s_vSwing[i - 1][a];
		}
		s_tSwing[0] = s_tNow; s_fSwing[0] = s_h[1].fSpeed;
		for (int a = 0; a < 3; ++a) s_vSwing[0][a] = s_h[1].v[a];
		if (s_nSwing < 12) ++s_nSwing;
	}
	if (bTrigger)
	{
		if (s_nThrowState == 0)
		{
			s_nThrowState = 1;
			VRLog::Msg("VRThrow: readied");
		}
		return true;
	}
	if (s_nThrowState == 1)
	{
		int nBest = -1;
		for (int i = 0; i < s_nSwing; ++i)
			if (s_tNow - s_tSwing[i] <= 0.10 && (nBest < 0 || s_fSwing[i] > s_fSwing[nBest])) nBest = i;
		const float fSpeed = (nBest >= 0) ? s_fSwing[nBest] : 0.0f;
		s_nSwing = 0;
		const float fMin = GetConsoleFloat("VRThrowMinSpeed", 1.0f);
		LTVector vEye, vR, vU, vF;
		if (fSpeed >= fMin && nBest >= 0 && CameraFrame(vEye, vR, vU, vF))
		{
			LTVector d = TrackDirToWorld(s_vSwing[nBest], vR, vU, vF);
			if (d.Mag() > 0.0001f)
			{
				d.Norm();
				s_vThrowDir = d;
				s_fThrowSpeed = fSpeed;
				s_nThrowState = 2;
				s_tThrowAt = s_tNow;
				s_bThrowSent = false;
				*pbFire = true;
				VRLog::Msg("VRThrow: let go at %.1f m/s - thrown; hand moving (%.2f %.2f %.2f) in tracking space,"
					" camera forward (%.2f %.2f %.2f) right (%.2f %.2f %.2f)", fSpeed,
					s_vSwing[nBest][0], s_vSwing[nBest][1], s_vSwing[nBest][2], vF.x, vF.y, vF.z, vR.x, vR.y, vR.z);
				return true;
			}
		}
		VRLog::Msg("VRThrow: let go at %.1f m/s - too slow, nothing thrown (VRThrowMinSpeed %.1f)", fSpeed, fMin);
		s_nThrowState = 0;
	}
	return true;
}

bool TakeThrow(CWeaponModel* pWM, LTVector* pDir, float* pfVelocity)
{
	if ((s_nThrowState != 2 && s_nThrowState != 3) || s_bThrowSent || !IsThrowable(pWM)) return false;
	if (s_tNow - s_tThrowAt > 1.5) { s_nThrowState = 0; return false; }
	AMMO* pA = pWM->GetAmmo();
	const float fBase = (pA && pA->pProjectileFX) ? (float)pA->pProjectileFX->nVelocity : 0.0f;
	float k = s_fThrowSpeed / GetConsoleFloat("VRThrowFullSpeed", 4.0f);
	if (k < 0.3f) k = 0.3f;
	if (k > 1.5f) k = 1.5f;
	*pDir = s_vThrowDir;
	*pfVelocity = fBase * k;
	s_bThrowSent = true;
	if (s_nThrowState == 3) s_nThrowState = 0;
	VRLog::Msg("VRThrow: %s sent at %.0f units/s (%.2f of its own %.0f), toward (%.2f %.2f %.2f)",
		pA ? pA->szName : "?", *pfVelocity, k, fBase, s_vThrowDir.x, s_vThrowDir.y, s_vThrowDir.z);
	return true;
}

}	// namespace VRPhysical

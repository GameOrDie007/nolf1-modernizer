// VRHands.cpp: Game Or Die Hands in NOLF VR. See VRHands.h.
#include "stdafx.h"
#include "VRHands.h"
#include "VRShared.h"
#include "VRLog.h"
#include "VRPrims.h"
#include "VarTrack.h"
#include "ClientUtilities.h"
#include "GameClientShell.h"
#include "CMoveMgr.h"
#include "CharacterFX.h"
#include "ModelButeMgr.h"

extern CGameClientShell* g_pGameClientShell;
#include "gdh_hand.h"
#include "gdh_fit.h"

#include <cmath>
#include <cstring>
#include <vector>

extern ILTClient* g_pLTClient;

namespace
{
	VarTrack g_vtOn;			// VRCateHands		0 off (the default), 1 on
	VarTrack g_vtAtX, g_vtAtY, g_vtAtZ, g_vtAtPitch, g_vtAtYaw, g_vtAtRoll;	// VRHandsAttach*: the hand on the palm
	VarTrack g_vtSize;			// VRHandsSize		the hand at the controller, x our size
	VarTrack g_vtGunCurl;		// VRHandsGunCurl	how far the gun hand's three fingers close round the grip
	VarTrack g_vtOnGun;			// VRHandsOnGun		1: the gun hand sits where the gun's own hand is
	VarTrack g_vtGunScale;		// VRHandsGunScale	1: on the gun, the fitted size instead of ours
	VarTrack g_vtHideGun;		// VRHandsHideGun	0: leave the gun's own hands to StubHideViewArms (desk overlays)
	VarTrack g_vtOutfit;		// VRHandsOutfit	0 our painted glove, 1 Action .. 5 Winter (her outfits' own), 6 (default) whatever she wears
	VarTrack g_vtTex;			// (string) VRHandsGlove, any texture, relative to the game folder; overrides the outfit
	bool g_bVars = false;

	gdh::Hand g_Hand[2];		// by physical side: 0 left, 1 right
	gdh::FingerDriver g_Drv[2];
	int g_nMesh[2] = { -1, -1 };
	int g_nPalm[2][6];			// our Wrist, Thumb_Metacarpal, Index..Little_Proximal
	bool g_bTried = false, g_bLoaded = false;
	// THE CUFFS: by physical side, the vertices on the cuff's flat end (within 3 mm of the
	// model's furthest point up the forearm) and a ring 12-25 mm in from it, whose centre
	// against the end's gives the way the cuff faces. Found once from the model.
	std::vector<uint32_t> g_Cap[2], g_Ring[2];
	struct Cuff { LTVector c, n; float r; bool ok; };
	Cuff g_Cuff[2];									// by physical side, this frame
	struct CuffReq { HSURFACE h; int w, ht; float rgb[3]; };
	CuffReq g_Req[2] = { { LTNULL, 0, 0, { 0, 0, 0 } }, { LTNULL, 0, 0, { 0, 0, 0 } } };	// by hand: 0 off, 1 gun
	bool g_bCuffsFound = false;
	double g_fLastTime = -1.0;

	// NOLF's names for the same six joints on a view weapon's right hand.
	const char* const kGunPalm[6] = { "wristr", "thumbr1", "pointerr1", "middler1", "ringr1", "pinkyr1" };
	const char* const kOurPalm[6] = { "Wrist", "Thumb_Metacarpal", "Index_Proximal", "Middle_Proximal",
									  "Ring_Proximal", "Little_Proximal" };

	void InitVars()
	{
		if (g_bVars || !g_pLTClient) return;
		g_bVars = true;
		const gdh::HandAttach a;		// the fit the hands viewer settled (palm centroid on the grip, -45)
		g_vtOn.Init(g_pLTClient, (char*)"VRCateHands", LTNULL, 1.0f);
		g_vtAtX.Init(g_pLTClient, (char*)"VRHandsAttachX", LTNULL, a.x);
		g_vtAtY.Init(g_pLTClient, (char*)"VRHandsAttachY", LTNULL, a.y);
		g_vtAtZ.Init(g_pLTClient, (char*)"VRHandsAttachZ", LTNULL, a.z);
		g_vtAtPitch.Init(g_pLTClient, (char*)"VRHandsAttachPitch", LTNULL, a.pitch);
		g_vtAtYaw.Init(g_pLTClient, (char*)"VRHandsAttachYaw", LTNULL, a.yaw);
		g_vtAtRoll.Init(g_pLTClient, (char*)"VRHandsAttachRoll", LTNULL, a.roll);
		g_vtSize.Init(g_pLTClient, (char*)"VRHandsSize", LTNULL, 1.0f);
		g_vtGunCurl.Init(g_pLTClient, (char*)"VRHandsGunCurl", LTNULL, 0.85f);
		g_vtOnGun.Init(g_pLTClient, (char*)"VRHandsOnGun", LTNULL, 0.0f);
		g_vtGunScale.Init(g_pLTClient, (char*)"VRHandsGunScale", LTNULL, 0.0f);
		g_vtHideGun.Init(g_pLTClient, (char*)"VRHandsHideGun", LTNULL, 1.0f);
		g_vtOutfit.Init(g_pLTClient, (char*)"VRHandsOutfit", LTNULL, 6.0f);
		g_vtTex.Init(g_pLTClient, (char*)"VRHandsGlove", (char*)"", 0.0f);
	}

	int BoneIndex(const gdh::Hand& h, const char* stem, char side)
	{
		char want[40];
		_snprintf(want, sizeof(want), "%s_%c", stem, side);
		want[sizeof(want) - 1] = 0;
		for (size_t i = 0; i < h.bones.size(); ++i)
			if (!_stricmp(h.bones[i].name, want)) return (int)i;
		return -1;
	}

	bool Load()
	{
		const char* const kFile[2] = { "gdh\\cate_hand_l.gdh", "gdh\\cate_hand_r.gdh" };
		const char* const kMesh[2] = { "mesh_Glove_L", "mesh_Glove_R" };
		for (int s = 0; s < 2; ++s)
		{
			if (!g_Hand[s].Load(kFile[s]))
			{
				VRLog::Msg("VRHands: %s not loaded (%s) - Cate's hands are off", kFile[s], g_Hand[s].error.c_str());
				return false;
			}
			if (!g_Drv[s].Bind(g_Hand[s]))
			{
				VRLog::Msg("VRHands: %s lacks the finger poses - Cate's hands are off", kFile[s]);
				return false;
			}
			g_nMesh[s] = g_Hand[s].FindMesh(kMesh[s]);
			if (g_nMesh[s] < 0)
			{
				VRLog::Msg("VRHands: %s has no %s - Cate's hands are off", kFile[s], kMesh[s]);
				return false;
			}
			for (int j = 0; j < 6; ++j)
				if ((g_nPalm[s][j] = BoneIndex(g_Hand[s], kOurPalm[j], s ? 'R' : 'L')) < 0)
				{
					VRLog::Msg("VRHands: %s has no %s joint - Cate's hands are off", kFile[s], kOurPalm[j]);
					return false;
				}
		}
		// The cuffs, from each model as it stands: our hand's forearm runs up +Z (its
		// fingers along -Z, OpenXR's forward).
		g_bCuffsFound = true;
		for (int s = 0; s < 2; ++s)
		{
			const gdh::Mesh& m = g_Hand[s].meshes[g_nMesh[s]];
			float zMax = -1.0e9f;
			for (size_t i = 0; i < m.verts.size(); ++i) if (m.verts[i].pos[2] > zMax) zMax = m.verts[i].pos[2];
			g_Cap[s].clear(); g_Ring[s].clear();
			for (size_t i = 0; i < m.verts.size(); ++i)
			{
				const float z = m.verts[i].pos[2];
				if (z > zMax - 0.003f) g_Cap[s].push_back((uint32_t)i);
				else if (z > zMax - 0.025f && z < zMax - 0.012f) g_Ring[s].push_back((uint32_t)i);
			}
			if (g_Cap[s].size() < 6 || g_Ring[s].size() < 6) g_bCuffsFound = false;
			VRLog::Msg("VRHands: %s cuff - %u vertices on its end, %u in the ring behind it",
					   s ? "right" : "left", (unsigned)g_Cap[s].size(), (unsigned)g_Ring[s].size());
		}
		VRLog::Msg("VRHands: Cate's hands loaded (%u + %u triangles)",
				   (unsigned)g_Hand[0].meshes[g_nMesh[0]].indices.size() / 3,
				   (unsigned)g_Hand[1].meshes[g_nMesh[1]].indices.size() / 3);
		return true;
	}

	// A hand's placement in the world: v_world = scale * rot * v_local + pos, where v_local is our
	// hand model's space converted to LithTech's axes (OpenXR's -Z forward becomes +Z: (x, y, -z)).
	// LithTech's world is left-handed and OpenXR's right-handed, so that conversion is the one
	// reflection the whole chain needs: our right hand stays a right hand.
	struct Place { double s; double R[3][3]; double t[3]; bool ok; };

	// The six palm joints of our posed hand, LithTech axes, metres.
	void OurPalm(int side, double out[6][3])
	{
		for (int j = 0; j < 6; ++j)
		{
			const gdh::Mat4& m = g_Hand[side].model[g_nPalm[side][j]];
			out[j][0] = m.m[12]; out[j][1] = m.m[13]; out[j][2] = -m.m[14];
		}
	}

	// ON THE GUN: our palm fitted onto the view weapon's own right hand, as published this frame.
	bool PlaceOnGun(const VRModelFrame& f, int nViewInst, HLOCALOBJ hVW, int side, double fFixedUnits, Place& p)
	{
		if (nViewInst < 0 || (uint32)nViewInst >= f.nCount || !hVW) return false;
		const VRModelInst& mi = f.inst[nViewInst];
		int idx[6] = { -1, -1, -1, -1, -1, -1 };
		HMODELNODE hN = INVALID_MODEL_NODE, hNx = INVALID_MODEL_NODE;
		uint32 n = 0;
		while (n < mi.nNodeCount && g_pLTClient->GetNextModelNode(hVW, hN, &hNx) == LT_OK)
		{
			hN = hNx;
			char sz[64] = "";
			g_pLTClient->GetModelNodeName(hVW, hN, sz, sizeof(sz));
			for (int j = 0; j < 6; ++j)
				if (!_stricmp(sz, kGunPalm[j])) idx[j] = (int)n;
			++n;
		}
		double theirs[6][3], ours[6][3];
		for (int j = 0; j < 6; ++j)
		{
			if (idx[j] < 0) return false;
			const VRModelNode& nd = f.nodes[mi.nNodeFirst + idx[j]];
			if (nd.m[0] == 0.0f && nd.m[1] == 0.0f && nd.m[2] == 0.0f) return false;	// unposed sentinel
			theirs[j][0] = nd.m[3]; theirs[j][1] = nd.m[7]; theirs[j][2] = nd.m[11];
		}
		OurPalm(side, ours);
		const gdh::Similarity sim = gdh::FitSimilarity(ours, theirs, 6);
		if (!sim.ok) return false;
		// THE FIT, joint by joint, once per model: how far each of our joints lands from the gun's.
		{
			static uint32 s_nSaidNodes[8] = {};
			static int s_nSaid = 0;
			bool bNew = true;
			for (int z = 0; z < s_nSaid; ++z) if (s_nSaidNodes[z] == mi.nNodeCount) bNew = false;
			if (bNew && s_nSaid < 8)
			{
				s_nSaidNodes[s_nSaid++] = mi.nNodeCount;
				for (int j = 0; j < 6; ++j)
				{
					double w[3];
					for (int r = 0; r < 3; ++r)
						w[r] = sim.scale * (sim.rot[r][0] * ours[j][0] + sim.rot[r][1] * ours[j][1] + sim.rot[r][2] * ours[j][2]) + sim.pos[r];
					VRLog::Msg("VRHands: fit %-10s node %2d at (%7.2f %7.2f %7.2f), ours lands %.2f units off; ours at (%+.3f %+.3f %+.3f) m",
							   kGunPalm[j], idx[j], theirs[j][0], theirs[j][1], theirs[j][2],
							   sqrt((w[0] - theirs[j][0]) * (w[0] - theirs[j][0]) + (w[1] - theirs[j][1]) * (w[1] - theirs[j][1])
									+ (w[2] - theirs[j][2]) * (w[2] - theirs[j][2])),
							   ours[j][0], ours[j][1], ours[j][2]);
				}
			}
		}
		// A fit that stretches our hand past plausible is a model whose names mean something
		// else: keep the controller placement instead. 10..300 world units a metre.
		if (sim.scale < 10.0 || sim.scale > 300.0)
		{
			static int s_nSaid = 0;
			if (s_nSaid++ < 3) VRLog::Msg("VRHands: the gun's hand fit was %.1f units/m - ignored", sim.scale);
			return false;
		}
		// THE SIZE IS OURS, not the fit's. NOLF's hand is short and very wide at the knuckles
		// (desk-measured: its knuckle row nearly as wide as the palm is long; ours is half that),
		// so a fitted scale is a compromise that drew our hand at half size. The fit gives the
		// place and the turn; the size is the same units per metre as the off hand (fFixedUnits).
		// VRHandsGunScale 1 uses the fitted scale instead.
		const double s = (g_vtGunScale.GetFloat() > 0.0f) ? sim.scale : fFixedUnits;
		double ms[3] = { 0, 0, 0 }, md[3] = { 0, 0, 0 };
		for (int j = 0; j < 6; ++j) for (int r = 0; r < 3; ++r) { ms[r] += ours[j][r] / 6.0; md[r] += theirs[j][r] / 6.0; }
		p.s = s;
		for (int r = 0; r < 3; ++r)
		{
			for (int c = 0; c < 3; ++c) p.R[r][c] = sim.rot[r][c];
			p.t[r] = md[r] - s * (sim.rot[r][0] * ms[0] + sim.rot[r][1] * ms[1] + sim.rot[r][2] * ms[2]);
		}
		static int s_nScaleSaid = 0;
		if ((++s_nScaleSaid % 300) == 1)
			VRLog::Msg("VRHands: on the gun, fitted scale %.1f units/m, drawn at %.1f; gun wrist (%.1f %.1f %.1f), joint centre (%.1f %.1f %.1f)",
					   sim.scale, s, theirs[0][0], theirs[0][1], theirs[0][2], md[0], md[1], md[2]);
		p.ok = true;
		return true;
	}

	// AT THE CONTROLLER: the aim pose the gun is placed from (the same position and the same axes,
	// built as the IK arms build them), times the palm in aim space the host measured (v20), times
	// the fit of our hand on the palm.
	// 0..1: how far the gun hand has turned to hold the gun (see PlaceAtController).
	float g_fGunHold = 0.0f;

	bool PlaceAtController(int h, int side, const LTVector& vEye, const LTVector& vCamR,
						   const LTVector& vCamU, const LTVector& vCamF, float fUnits, Place& p)
	{
		const VRSharedState& st = VRShared::State();
		const VRHandState& hs = st.Hands[h];
		if (!hs.nActive) return false;
		// the controller in the camera's basis, metres
		LTVector d((hs.fPosX - st.fHeadPosX), (hs.fPosY - st.fHeadPosY), -(hs.fPosZ - st.fHeadPosZ));
		if (GetConsoleFloat((char*)"VRHeadAsMouse", 0.0f) > 0.0f)
		{
			const float y = st.fHeadYawDeg * 0.01745329f, c = cosf(y), s = sinf(y);
			d = LTVector(d.x * c - d.z * s, d.y, d.x * s + d.z * c);
		}
		const LTVector P = vEye + (vCamR * d.x + vCamU * d.y + vCamF * d.z) * fUnits;
		// the controller's axes in the world
		LTVector vR, vU, vF;
		{
			const float k = 0.01745329f;
			LTRotation rC; rC.Init();
			g_pLTClient->EulerRotateY(&rC, -hs.fYawDeg * k);
			g_pLTClient->EulerRotateX(&rC, -hs.fPitchDeg * k);
			g_pLTClient->EulerRotateZ(&rC, (hs.fRollDeg - st.fHeadRollDeg) * k);
			LTVector cu, cr, cf;
			g_pLTClient->GetRotationVectors(&rC, &cu, &cr, &cf);
			vR = vCamR * cr.x + vCamU * cr.y + vCamF * cr.z;
			vU = vCamR * cu.x + vCamU * cu.y + vCamF * cu.z;
			vF = vCamR * cf.x + vCamU * cf.y + vCamF * cf.z;
		}
		// hand model -> palm -> aim, OpenXR axes, metres
		gdh::HandAttach a;
		a.x = g_vtAtX.GetFloat(); a.y = g_vtAtY.GetFloat(); a.z = g_vtAtZ.GetFloat();
		a.pitch = g_vtAtPitch.GetFloat(); a.yaw = g_vtAtYaw.GetFloat(); a.roll = g_vtAtRoll.GetFloat();
		gdh::Mat4 M = gdh::AttachMatrix(a, side == 0);
		if (st.nGripValid[h])
		{
			const float* g = st.fAimToGrip[h];
			gdh::Quat q = gdh::Normalize({ g[3], g[4], g[5], g[6] });
			// HOLDING A GUN, the hand turns down onto the grip. Each gun's
			// angle was tuned to the aim pose, and at the palm's own tilt her
			// hand sat up on the gun with the finger through the slide.
			// VRGunHoldTurn degrees toward the aim pose, over a moment, not in
			// a frame. (All the way, the 60 degrees of a Quest 3, put the
			// wrist above the slide.)
			if (h == 1 && g_fGunHold > 0.0f && GetConsoleFloat((char*)"VRGunAtPalm", 1.0f) > 0.0f)
			{
				const float fAng = 2.0f * (float)acos(fabs(q.w) > 1.0f ? 1.0f : fabs(q.w)) * 57.29578f;
				const float fTurn = GetConsoleFloat((char*)"VRGunHoldTurn", 22.0f);
				if (fAng > 0.5f && fTurn > 0.0f)
				{
					const float k = (fTurn < fAng ? fTurn / fAng : 1.0f) * g_fGunHold;
					q = gdh::Slerp(q, gdh::Quat{}, k);
				}
			}
			M = gdh::Mul(gdh::FromTR({ g[0], g[1], g[2] }, q), M);
			// ...and up the grip by VRGunHoldUp cm, so the finger lies on the
			// trigger, and forward by VRGunHoldFwd cm (the hand tuner's GUN HOLD).
			if (h == 1 && g_fGunHold > 0.0f && GetConsoleFloat((char*)"VRGunAtPalm", 1.0f) > 0.0f)
			{
				const float fUp  = 0.01f * GetConsoleFloat((char*)"VRGunHoldUp", 1.0f) * g_fGunHold;
				const float fFwd = 0.01f * GetConsoleFloat((char*)"VRGunHoldFwd", 0.0f) * g_fGunHold;
				M = gdh::Mul(gdh::FromTR({ 0.0f, fUp, -fFwd }, gdh::Quat{}), M);
			}
		}
		const float fSize = (g_vtSize.GetFloat() > 0.2f && g_vtSize.GetFloat() < 5.0f) ? g_vtSize.GetFloat() : 1.0f;
		// LithTech axes of the aim frame are (+x right, +y up, +z forward) = D * OpenXR, D = diag(1,1,-1):
		// world = P + units * [vR vU vF] * D * (M * size * v)
		const LTVector col[3] = { vR, vU, vF };
		double A[3][3];
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
				A[r][c] = (double)(&col[c].x)[r];
		const double D[3] = { 1.0, 1.0, -1.0 };
		// our model's vertices will come in LithTech axes (x, y, -z) = D*v, so M is conjugated by D
		double MD[3][3];
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
				MD[r][c] = D[r] * M.m[c * 4 + r] * D[c];
		for (int r = 0; r < 3; ++r)
		{
			for (int c = 0; c < 3; ++c)
				p.R[r][c] = A[r][0] * MD[0][c] + A[r][1] * MD[1][c] + A[r][2] * MD[2][c];
			const double tl[3] = { M.m[12], M.m[13], -M.m[14] };
			p.t[r] = (double)(&P.x)[r] + fUnits * (A[r][0] * tl[0] + A[r][1] * tl[1] + A[r][2] * tl[2]);
		}
		p.s = fUnits * fSize;
		// the palm offset in M is NOT scaled by the hand size, only the hand: fold size into R, not t
		for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) p.R[r][c] *= fSize;
		p.s = fUnits;
		p.ok = true;
		return true;
	}

	// Fingers from the controller. A controller without touch sensors: the finger is on the
	// trigger when it pulls, and the thumb rests down.
	void Fingers(int h, int side, float dt, bool bOnGun)
	{
		const VRSharedState& st = VRShared::State();
		const VRHandState& hs = st.Hands[h];
		gdh::HandInput in;
		in.trigger = hs.fTrigger;
		in.grip = hs.fGrip;
		const uint32_t t = st.nTouch[h];
		if (t & VRTOUCH_KNOWN)
		{
			in.triggerTouch = (t & VRTOUCH_TRIGGER) != 0;
			in.thumbTouch = (t & VRTOUCH_THUMB) != 0;
		}
		else
		{
			in.triggerTouch = hs.fTrigger > 0.05f;
			in.thumbTouch = true;
		}
		if (bOnGun)
		{
			// round the grip, whatever the grip button says; the thumb over it
			const float c = g_vtGunCurl.GetFloat();
			if (in.grip < c) in.grip = c;
			in.thumbTouch = true;
			in.triggerTouch = true;
		}
		g_Drv[side].Update(in, dt, g_Hand[side]);
		g_Hand[side].Skin();
	}

	// Skin one hand into world-space triangles.
	uint32_t Emit(int side, const Place& p, VRHandVert* out, uint32_t nRoom, float fCentre[3])
	{
		const gdh::Hand& H = g_Hand[side];
		const gdh::Mesh& m = H.meshes[g_nMesh[side]];
		const uint32_t nIdx = (uint32_t)m.indices.size();
		if (nIdx > nRoom) return 0;
		double cx = 0, cy = 0, cz = 0;
		static std::vector<VRHandVert> s_V;
		s_V.resize(m.verts.size());
		for (size_t i = 0; i < m.verts.size(); ++i)
		{
			const gdh::Vertex& v = m.verts[i];
			float px = 0, py = 0, pz = 0, nx = 0, ny = 0, nz = 0;
			for (int k = 0; k < 4; ++k)
			{
				const float w = v.weight[k];
				if (w <= 0.0f) continue;
				const float* s = H.skin[v.bone[k]].m;
				px += w * (s[0] * v.pos[0] + s[4] * v.pos[1] + s[8] * v.pos[2] + s[12]);
				py += w * (s[1] * v.pos[0] + s[5] * v.pos[1] + s[9] * v.pos[2] + s[13]);
				pz += w * (s[2] * v.pos[0] + s[6] * v.pos[1] + s[10] * v.pos[2] + s[14]);
				nx += w * (s[0] * v.nrm[0] + s[4] * v.nrm[1] + s[8] * v.nrm[2]);
				ny += w * (s[1] * v.nrm[0] + s[5] * v.nrm[1] + s[9] * v.nrm[2]);
				nz += w * (s[2] * v.nrm[0] + s[6] * v.nrm[1] + s[10] * v.nrm[2]);
			}
			const double l[3] = { px, py, -pz }, ln[3] = { nx, ny, -nz };		// LithTech axes
			VRHandVert& o = s_V[i];
			double nn = 0;
			for (int r = 0; r < 3; ++r)
			{
				o.fPos[r] = (float)(p.s * (p.R[r][0] * l[0] + p.R[r][1] * l[1] + p.R[r][2] * l[2]) + p.t[r]);
				o.fNrm[r] = (float)(p.R[r][0] * ln[0] + p.R[r][1] * ln[1] + p.R[r][2] * ln[2]);
				nn += (double)o.fNrm[r] * o.fNrm[r];
			}
			nn = nn > 1e-20 ? 1.0 / std::sqrt(nn) : 0.0;
			for (int r = 0; r < 3; ++r) o.fNrm[r] = (float)(o.fNrm[r] * nn);
			o.fUV[0] = v.uv[0]; o.fUV[1] = v.uv[1];
			cx += o.fPos[0]; cy += o.fPos[1]; cz += o.fPos[2];
		}
		for (uint32_t i = 0; i < nIdx; ++i) out[i] = s_V[m.indices[i]];
		{
			Cuff& cf = g_Cuff[side];
			cf.ok = false;
			if (g_bCuffsFound)
			{
				LTVector c(0.0f, 0.0f, 0.0f), rc(0.0f, 0.0f, 0.0f);
				for (size_t k = 0; k < g_Cap[side].size(); ++k)
				{ const float* q = s_V[g_Cap[side][k]].fPos; c += LTVector(q[0], q[1], q[2]); }
				for (size_t k = 0; k < g_Ring[side].size(); ++k)
				{ const float* q = s_V[g_Ring[side][k]].fPos; rc += LTVector(q[0], q[1], q[2]); }
				c = c / (float)g_Cap[side].size();
				rc = rc / (float)g_Ring[side].size();
				LTVector nrm = c - rc;
				const float fL = nrm.Mag();
				if (fL > 1.0e-4f)
				{
					float fR = 0.0f;
					for (size_t k = 0; k < g_Cap[side].size(); ++k)
					{ const float* q = s_V[g_Cap[side][k]].fPos; fR += (LTVector(q[0], q[1], q[2]) - c).Mag(); }
					cf.c = c; cf.n = nrm / fL; cf.r = fR / (float)g_Cap[side].size(); cf.ok = true;
				}
			}
		}
		const double n = m.verts.empty() ? 1.0 : (double)m.verts.size();
		fCentre[0] = (float)(cx / n); fCentre[1] = (float)(cy / n); fCentre[2] = (float)(cz / n);
		return nIdx;
	}

	// THE GLOVE: her outfit's own when setup has made it from the game's files
	// (gdh\cate_glove_<outfit>.tga), else our painted one, which always ships.
	const char* GloveTexture()
	{
		const char* pszOver = g_vtTex.GetStr((char*)"");
		if (pszOver && pszOver[0]) return pszOver;
		static const char* const kFile[6] = { "gdh\\cate_glove.tga", "gdh\\cate_glove_action.tga", "gdh\\cate_glove_casual.tga",
											  "gdh\\cate_glove_scuba.tga", "gdh\\cate_glove_undercover2.tga", "gdh\\cate_glove_winter.tga" };
		static int s_nChecked = -1;
		static bool s_bHave = false;
		int n = (int)(g_vtOutfit.GetFloat() + 0.5f);
		// 6, THE DEFAULT: WHATEVER SHE IS WEARING. The player's own character
		// names its outfit (the model style: Action, Casual, Scuba, Undercover2,
		// Winter), so the gloves match it level by level. A fixed outfit drew
		// black Action gloves in a level where she wears purple ones.
		if (n == 6)
		{
			n = 1;
			CCharacterFX* pChar = (g_pGameClientShell && g_pGameClientShell->GetMoveMgr())
				? g_pGameClientShell->GetMoveMgr()->GetCharacterFX() : LTNULL;
			const char* pszStyle = (pChar && g_pModelButeMgr && pChar->GetModelStyle() != eModelStyleInvalid
									&& (int)pChar->GetModelStyle() < g_pModelButeMgr->GetNumStyles())
				? g_pModelButeMgr->GetStyleName(pChar->GetModelStyle()) : LTNULL;
			static const char* const kStyle[6] = { "", "Action", "Casual", "Scuba", "Undercover2", "Winter" };
			if (pszStyle)
				for (int s = 1; s < 6; ++s)
					if (_stricmp(pszStyle, kStyle[s]) == 0) { n = s; break; }
			static int s_nSaidStyle = -2;
			if (n != s_nSaidStyle)
			{
				s_nSaidStyle = n;
				VRLog::Msg("VRHands: her outfit is %s - its gloves", pszStyle ? pszStyle : "(not known yet)");
			}
		}
		if (n < 0 || n > 5) n = 0;
		if (n != s_nChecked)
		{
			s_nChecked = n;
			FILE* f = fopen(kFile[n], "rb");
			s_bHave = (f != LTNULL);
			if (f) fclose(f);
			VRLog::Msg("VRHands: glove %s %s", kFile[n], s_bHave ? "found" : "not made yet - the painted glove instead");
		}
		return s_bHave ? kFile[n] : kFile[0];
	}

	VRHandsPublishFn Entry()
	{
		static VRHandsPublishFn s_pfn = LTNULL;
		static HMODULE s_hSeen = (HMODULE)(uintptr_t)1;
		HMODULE h = GetModuleHandleA("d3dstub.ren");
		if (h != s_hSeen)
		{
			s_hSeen = h;
			s_pfn = h ? (VRHandsPublishFn)GetProcAddress(h, "R3D_PublishHands") : LTNULL;
			VRLog::Msg("VRHands: publish entry %s", s_pfn ? "resolved" : "missing");
		}
		return s_pfn;
	}
}

bool VRHands_On()
{
	InitVars();
	return g_bVars && g_vtOn.GetFloat() > 0.0f && g_bLoaded;
}

void VRHands_Publish(const VRModelFrame& f, int nViewInst, HLOCALOBJ hViewWeapon,
					 const LTVector& vEye, const LTVector& vCamR, const LTVector& vCamU,
					 const LTVector& vCamF, float fGunUnits, bool bShow)
{
	InitVars();
	if (!g_bVars) return;
	static VRHandsFrame s_out;
	static uint32 s_nFrame = 0;
	s_out.nMagic = VRHANDS_MAGIC; s_out.nVersion = VRHANDS_VERSION;
	s_out.nFrame = ++s_nFrame; s_out.nFlags = 0; s_out.nRuns = 0; s_out.nVerts = 0;
	VRHandsPublishFn pfn = Entry();

	const bool bWant = g_vtOn.GetFloat() > 0.0f;
	if (bWant && !g_bTried) { g_bTried = true; g_bLoaded = Load(); }
	if (!bWant || !g_bLoaded || !bShow || !VRShared::IsLive())
	{
		g_Req[0].h = g_Req[1].h = LTNULL;	// no hands, no cuffs to show anything on
		if (pfn) pfn(&s_out);			// an empty frame: nothing stale is drawn
		return;
	}

	const double now = g_pLTClient->GetTime();
	float dt = (g_fLastTime < 0.0) ? 0.0f : (float)(now - g_fLastTime);
	if (dt < 0.0f || dt > 0.25f) dt = 0.25f;
	g_fLastTime = now;

	const bool bSwap = VRShared::SwapHands();
	for (int h = 0; h < 2; ++h)			// 0 the off hand, 1 the gun hand, as the snapshot has them
	{
		const int side = bSwap ? 1 - h : h;	// the physical hand, which model to draw
		Place p{};
		// the fingers first (the fit reads the posed palm), then where the hand goes
		// The gun hand closes round the gun whenever one is out: the gun is placed at the
		// controller by the per-weapon grip offsets tuned in the headset, so its handle is where
		// the real hand is, and so is ours, at the controller's palm.
		const bool bGunOut = (h == 1) && nViewInst >= 0;
		if (h == 1)
		{
			const float fStep = (dt > 0.0f) ? dt / 0.15f : 1.0f;
			g_fGunHold += bGunOut ? fStep : -fStep;
			if (g_fGunHold < 0.0f) g_fGunHold = 0.0f;
			if (g_fGunHold > 1.0f) g_fGunHold = 1.0f;
		}
		Fingers(h, side, dt, bGunOut);
		const float fSize = (g_vtSize.GetFloat() > 0.2f && g_vtSize.GetFloat() < 5.0f) ? g_vtSize.GetFloat() : 1.0f;
		// VRHandsOnGun 1 (off by default): fit ours to the view weapon's own hand joints instead.
		// Desk-measured, NOLF's gun hand is shaped too differently from ours (short, very wide at
		// the knuckles) and posed for the flat screen, so this is kept for comparison only.
		bool bOnGun = bGunOut && g_vtOnGun.GetFloat() > 0.0f
			&& PlaceOnGun(f, nViewInst, hViewWeapon, side, (double)fGunUnits * fSize, p);
		if (!bOnGun && !PlaceAtController(h, side, vEye, vCamR, vCamU, vCamF, fGunUnits, p)) continue;
		VRHandRun& run = s_out.runs[s_out.nRuns];
		const uint32_t n = Emit(side, p, s_out.verts + s_out.nVerts, VRHANDS_MAX_VERTS - s_out.nVerts, run.fLightAt);
		if (!n) continue;
		if (GetConsoleFloat((char*)"VRDebugTrace", 0.0f) > 0.0f)
			VRLog::Msg("VRTrace: cate %s hand %.2f %.2f %.2f view inst %d hold %.2f world %.3f %.3f %.3f",
					   h ? "gun" : "off", run.fLightAt[0] - vEye.x, run.fLightAt[1] - vEye.y, run.fLightAt[2] - vEye.z,
					   nViewInst, g_fGunHold, run.fLightAt[0], run.fLightAt[1], run.fLightAt[2]);
		static int s_nWhere = 0;
		if ((++s_nWhere % 600) == 1)
			VRLog::Msg("VRHands: %s hand centre (%.1f %.1f %.1f), eye (%.1f %.1f %.1f), %s",
					   h ? "gun" : "off", run.fLightAt[0], run.fLightAt[1], run.fLightAt[2], vEye.x, vEye.y, vEye.z,
					   bOnGun ? "on the gun" : "at the controller");
		strncpy(run.szTex, GloveTexture(), sizeof(run.szTex) - 1);
		run.szTex[sizeof(run.szTex) - 1] = 0;
		run.nStart = s_out.nVerts;
		run.nCount = n;
		s_out.nVerts += n;
		++s_out.nRuns;
		static int s_nSaid[2] = { 0, 0 };
		if (s_nSaid[h] < 2 || (s_nSaid[h] < 6 && bOnGun != (s_nSaid[h] & 1)))
		{
			++s_nSaid[h];
			VRLog::Msg("VRHands: %s hand (%s) %s, %u vertices, %.1f units/m, at (%.0f %.0f %.0f)",
					   h ? "gun" : "off", side ? "right" : "left", bOnGun ? "ON THE GUN" : "at the controller",
					   n, p.s, run.fLightAt[0], run.fLightAt[1], run.fLightAt[2]);
		}
	}
	s_out.nFlags = (g_vtHideGun.GetFloat() > 0.0f) ? VRHANDS_F_HIDEGUNHANDS : 0u;
	if (pfn) pfn(&s_out);

	// THE CUFFS' SCREENS, from the cuffs just placed. Upright to the eyes (world up laid
	// on the cuff's face), a little proud of the lip so the rim does not cut it, as big
	// as fits inside the rim, and fading in as the cuff turns to the eyes (Doom VR's).
	for (int h = 0; h < 2; ++h)
	{
		CuffReq& rq = g_Req[h];
		const Cuff& cf = g_Cuff[bSwap ? 1 - h : h];
		if (rq.h && cf.ok && rq.w > 0 && rq.ht > 0)
		{
			const float fUnitsM = fGunUnits > 0.0f ? fGunUnits : 58.75f;
			// 2 mm proud of the lip: the disc closes the cuff's end
			const LTVector c = cf.c + cf.n * (GetConsoleFloat((char*)"VRCuffOut", 0.002f) * fUnitsM);
			LTVector toEye = vEye - c;
			const float fD = toEye.Mag();
			const float fFace = fD > 0.001f ? (toEye.Dot(cf.n) / fD) : 0.0f;
			const float fFrom = GetConsoleFloat((char*)"VRCuffFrom", 0.30f), fFull = GetConsoleFloat((char*)"VRCuffFull", 0.60f);
			float fA = (fFull > fFrom + 0.01f) ? (fFace - fFrom) / (fFull - fFrom) : 1.0f;
			if (fA > 1.0f) fA = 1.0f;
			LTVector up = LTVector(0.0f, 1.0f, 0.0f) - cf.n * cf.n.y;
			if (up.Mag() < 0.2f) up = vCamU - cf.n * cf.n.Dot(vCamU);	// the cuff facing straight up or down
			if (fA > 0.0f && up.Mag() > 1.0e-3f)
			{
				up.Norm();
				// the viewer's right, looking along -n with this up (n x up read mirrored at the desk)
				LTVector rt = up.Cross(cf.n);
				if (GetConsoleFloat((char*)"VRCuffFlip", 0.0f) > 0.0f) rt = -rt;
				// A FULL ROUND DISC over the cuff's end in the panel's own colour, and the numbers
				// on it a hair in front: the panel's black (or red) edges vanish into the disc, so
				// only the icons and digits show (Doom VR's look).
				const float fDiscR = cf.r * GetConsoleFloat((char*)"VRCuffDisc", 0.97f);
				VRPrims_SetDisc(c, rt, up, fDiscR, rq.rgb, fA);
				const float a = (float)rq.ht / (float)rq.w;
				const float fW = 2.0f * fDiscR / (float)sqrt(1.0f + a * a) * GetConsoleFloat((char*)"VRCuffScreenSize", 0.92f);
				const float fH = fW * a;
				const LTVector c2 = c + cf.n * (0.001f * fUnitsM);
				const float sx[4] = { -0.5f, 0.5f, 0.5f, -0.5f }, sy[4] = { 0.5f, 0.5f, -0.5f, -0.5f };
				LTVector vW[4];
				for (int k = 0; k < 4; ++k) vW[k] = c2 + rt * (sx[k] * fW) + up * (sy[k] * fH);
				VRPrims_SetSurfaceQuad(rq.h, vW, fA);
			}
			static int s_nSaidCuff[2] = { 0, 0 };
			if (s_nSaidCuff[h] < 3)
			{
				++s_nSaidCuff[h];
				VRLog::Msg("VRHands: %s cuff screen - %.1f cm across the cuff, faces the eyes by %.2f (alpha %.2f), %.2f m from them",
						   h ? "gun hand's" : "off hand's", 2.0f * cf.r / fUnitsM * 100.0f, fFace, fA > 0.0f ? fA : 0.0f, fD / fUnitsM);
			}
		}
		rq.h = LTNULL;
	}
}

bool VRHands_CuffReady()
{
	return g_bLoaded && g_bCuffsFound && g_vtOn.GetFloat() > 0.0f
		&& GetConsoleFloat((char*)"VRCuffScreens", 1.0f) > 0.0f;
}

void VRHands_CuffScreen(int nHand, HSURFACE hSurf, int nW, int nH, const float* pDiscRGB)
{
	if (nHand < 0 || nHand > 1) return;
	g_Req[nHand].h = hSurf; g_Req[nHand].w = nW; g_Req[nHand].ht = nH;
	for (int a = 0; a < 3; ++a) g_Req[nHand].rgb[a] = pDiscRGB ? pDiscRGB[a] : 0.0f;
}

// VRHands.cpp: Game Or Die Hands in NOLF VR. See VRHands.h.
#include "stdafx.h"
#include "VRHands.h"
#include "VRShared.h"
#include "VRLog.h"
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
			M = gdh::Mul(gdh::FromTR({ g[0], g[1], g[2] }, gdh::Normalize({ g[3], g[4], g[5], g[6] })), M);
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
}

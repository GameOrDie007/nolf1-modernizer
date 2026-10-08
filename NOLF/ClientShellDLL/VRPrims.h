// ----------------------------------------------------------------------- //
//
// MODULE  : VRPrims.h
//
// PURPOSE : The effects the retail renderer drew from the engine's object
//           list and ours never saw: particle systems, line systems,
//           polygrids and the game's canvas polygons. Gathered here once a
//           frame into a VRPrimFrame (see VRShared.h) and handed to the
//           renderer.
//
// ----------------------------------------------------------------------- //

#ifndef __VRPRIMS_H__
#define __VRPRIMS_H__

#include "ltbasedefs.h"

// Once, after g_pLTClient exists and before any world loads. Wraps the two
// engine calls that name a particle system's and a polygrid's texture, since
// the engine offers no way to ask for them afterwards.
void VRPrims_Init();

// Once a frame from VRPublishModels: everything within fRange of the eye.
void VRPrims_Publish(const LTVector& vEye, float fRange, uint32 nFrame);
// HAND AN OBJECT OVER RATHER THAN LET THE GATHER LOOK FOR IT.
//
// FindObjectsInSphere does not return FLAG_REALLYCLOSE objects at all -
// measured on 11 September: with the muzzle flash firing fourteen times in
// one run, the query returned ZERO camera-relative objects of any type, so
// no filter in the gather could ever have been the reason they were
// missing. The view weapon has always been added explicitly for the same
// reason. Cleared every frame; call before VRPrims_Publish.
void VRPrims_AddExtra(HLOCALOBJ h);
// The model file an object was created with (from the skin records), or "".
const char* VRSkins_ModelFile(HLOCALOBJ h);

// CAMERA-RELATIVE OBJECTS ONTO THE VR GUN. The first-person muzzle flash and
// its smoke carry FLAG_REALLYCLOSE: their positions are in the retail
// camera's space, next to where the retail gun sat. Our gun is at the HAND,
// placed by the publish path, so those effects are re-based: their offset
// from the retail gun, scaled by the view-model factor, carried through the
// hand gun's rotation to its world position. The publish path fills this
// once a frame for the view weapon; bKnown is false when there is none.
struct VRViewRebase
{
	bool		bKnown;
	LTVector	vEye, vCamR, vCamU, vCamF;	// the camera's frame
	LTRotation	rCam;						// the same frame as a rotation (the body's, no head)
	LTVector	vGunCam;					// the retail gun, camera-relative
	LTVector	vGunWorld;					// our gun, world
	LTRotation	rGunWorld;					// our gun's rotation (the hand's)
	float		fK;							// units per camera-relative unit
};
VRViewRebase& VRPrims_Rebase();
LTVector VRPrims_RebasePoint(const LTVector& vCam);
// The same transform against the LAST KNOWN gun pose rather than this frame's,
// for anything published BEFORE the view weapon. See VRPrims.cpp.
LTVector VRPrims_RebasePointLast(const LTVector& vCam);
// A point given as a WORLD-UNIT offset FROM THE GUN, placed in the gun's own
// frame. RebasePointLast above is for points in the engine's compressed
// camera-relative space and scales them by fK; an offset the game authored in
// game units must NOT be scaled, and multiplying one by fK (17) is what put
// the casings thirteen feet away. See the comment at the definition.
LTVector VRPrims_GunPointFromOffsetLast(const LTVector& vOffsetFromGun);

// A vector given in the GUN'S OWN FRAME - its right, up and forward - carried
// into WORLD axes. The view weapon's rotation is CAMERA-RELATIVE: the renderer
// draws every one of its nodes as (camera basis x object rotation), so an
// offset rotated by the object rotation alone is one basis short and stays
// fixed in the world while the body turns with the stick. VRGunFrameBody 0
// is the old arm. See the definition in VRPrims.cpp.
LTVector VRPrims_GunFrameToWorld(const VRViewRebase& rb, const LTVector& vGunFrame);
// The same against the LAST KNOWN camera basis, with a rotation the caller
// supplies - usually CWeaponModel::VRGunRot(), the live object rotation.
LTVector VRPrims_GunFrameToWorldLast(const LTRotation& rObj, const LTVector& vGunFrame);

// WHERE THE RENDERER ACTUALLY DREW THE END OF THE BARREL, in world units.
//
// Every offset-based answer has to agree with three separate things - the gun's
// rotation, its published origin, and the K*S its nodes are scaled by - and a
// mistake in any one of them lands the flash off the gun. The drawn nodes have
// already been through all three, which is why the node capture in
// VRPublishModels says the picture is the truth "whatever the rotation
// arithmetic thinks it did". Noted each frame from the view weapon's furthest
// node and read back by the weapon the next frame.
void VRPrims_NoteDrawnMuzzle(const LTVector& vWorld);
bool VRPrims_DrawnMuzzle(LTVector& vOut);

// THE MIDDLE OF THE DRAWN GUN, for things that leave the weapon's BODY rather
// than its barrel - the shell casing above all. A casing coming out of the
// muzzle is wrong on every weapon, and a fixed offset back from the muzzle is
// wrong on all but one, because the guns are different lengths (headset
// testing, 19 September). The centroid of the drawn nodes is neither: it scales with
// whatever model is in the player's hand.
void VRPrims_NoteDrawnGunCentre(const LTVector& vWorld);
bool VRPrims_DrawnGunCentre(LTVector& vOut);
void     VRPrims_RememberRebase();
bool     VRPrims_RebaseEverKnown();
// The pose that RebasePointLast actually used, for logging. Reading
// VRPrims_Rebase() instead reports this frame's, which is empty for anything
// published before the view weapon - i.e. for every caller of the Last form.
const VRViewRebase& VRPrims_RebaseLast();

// True when our renderer is the one loaded. PolyGridFX waits for the engine's
// FLAG_WASDRAWN before it animates, and only the retail renderer sets that.
bool VRPrims_Active();

// THE SCOPE. Set once a frame by the model publisher, from the placed scope:
// the objective lens (where the third pass looks from) and its axis, and the
// eyepiece disc (centre, radius, and the disc's right/up in world) that the
// effects frame carries as a VRPRIM_F_SCOPELENS run. nZoom picks the field.
void VRPrims_SetScopeLens(const LTVector& vObjective, const LTRotation& rAxis,
						  const LTVector& vEyepiece, const LTVector& vRight,
						  const LTVector& vUp, float fRadius, int nZoom, float fFovDeg);
void VRPrims_ClearScopeLens();

// A PANEL IN THE WORLD wearing a 2D surface the client drew (physical play's
// wrist display): four WORLD corners, top-left, top-right, bottom-right,
// bottom-left, and an alpha for the whole panel. Good for the next publish
// only, set it every frame it should show. LTNULL clears it.
void VRPrims_SetSurfaceQuad(HSURFACE hSurf, const LTVector* pCorners, float fAlpha);
// Where the scope's picture is taken from, as last published; false when the
// weapon in hand draws no scope lens.
bool VRPrims_GetScopeObjective(LTVector& vObjective);
// ...and the axis the scope looks along (world), which a scoped shot follows.
bool VRPrims_GetScopeAxis(LTRotation& rAxis);

#endif // __VRPRIMS_H__

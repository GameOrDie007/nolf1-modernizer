// VRHands.h: Game Or Die Hands in NOLF VR: Cate's own articulated hands, drawn at the controllers.
//
// Our hand model (the Game Or Die Hands project's hand, reshaped into a woman's hand, CC0-based,
// ours, shipped in game\gdh\) is posed every frame from the trigger, grip and touch sensors, and
// placed:
//   - on the gun hand, where the view weapon's OWN right hand holds the gun (a fit of our wrist,
//     thumb base and knuckles onto the model's wristR/thumbR1/pointerR1/... nodes), so the gun stays
//     exactly where the port already puts it and our hand wraps it;
//   - otherwise at the controller's palm (the grip pose), with the fit the hands viewer settled.
// It is skinned here and handed to the renderer as world-space triangles (R3D_PublishHands).
// Behind VRCateHands (default 0); with it off nothing here runs and nothing is published
// but an empty frame.
#pragma once

#include "ltbasedefs.h"

struct VRModelFrame;

// VRCateHands on, and Cate's hand files loaded. Read by the publish path to stop drawing the
// view weapon's own mirrored support hand while ours are drawn.
bool VRHands_On();

// Once a frame, from the model publish, after the frame is complete and before it is handed over.
//   nViewInst       the view weapon's index in f.inst, or -1
//   hViewWeapon     the view weapon object (for its node names), or LTNULL
//   vEye, vCamR/U/F the camera position and the BODY's basis the gun is placed in
//   fGunUnits       world units per metre of the gun's placement (VRViewModelScale x VRHandPosScale)
//   bShow           false in menus, cards, on a vehicle: publish nothing
void VRHands_Publish(const VRModelFrame& f, int nViewInst, HLOCALOBJ hViewWeapon,
					 const LTVector& vEye, const LTVector& vCamR, const LTVector& vCamU,
					 const LTVector& vCamF, float fGunUnits, bool bShow);

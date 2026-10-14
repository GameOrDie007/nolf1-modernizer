// VRRoom.h: ROOM-SCALE. The body walks where the player walks.
//
// The head's position has always moved the eyes; this moves the player too.
// Each frame the body tries to follow the head across the floor, in short
// steps through the game's own collision (sliding along a wall it meets), and
// what the body has carried is taken back off the eyes, so the view stays on
// the body. Where the body cannot follow - a wall, a ledge - the eyes still go
// with the head, and the wall fade (WallFade) darkens the view as they enter
// the wall. VRRoomScale 0 turns the body's part off.
//
// Distances are in TRACKING metres, in the frame of the head reference the
// camera keeps (it moves on a recenter, a teleport or a level change, and the
// carried distance starts again from nothing).

#ifndef _VRROOM_H_
#define _VRROOM_H_

#include "ltbasedefs.h"

namespace VRRoom
{
	// The camera took a new head reference (tracking metres): nothing is carried.
	void	NoteHeadRef(float x, float y, float z);

	// Once a frame, after the player's own move: walk the body after the head.
	// fYaw is the body's yaw (radians, the game's sign).
	void	Step(HOBJECT hObj, float fYaw, bool bAllowed);

	// What the body has carried so far, tracking metres (x right, z back).
	void	Carried(float* pX, float* pZ);

	// The view through a wall: 0 clear .. 1 black, from the body's eye point
	// to the eye that has gone on with the head (world units). Draws the fade.
	float	WallFade(const LTVector& vBodyEye, const LTVector& vEye, HOBJECT hIgnore);
}

#endif

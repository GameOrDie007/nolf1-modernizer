// VRWalk.h: which way "forward" is, and how fast the stick says to go.
//
// WALK DIRECTION (VRWalkDir, VR Options > Turning and Moving).
//   0 Body      forward is the body's facing, which only the right stick turns.
//               What the game has always done.
//   1 Head      forward is wherever the head faces, physical turns included.
//               What an omnidirectional treadmill needs: the player turns with
//               their whole body and the treadmill only ever says "forward".
//   2 Off hand  forward is where the off-hand controller points, so the head
//               is free to look around while walking straight.
//
// ANALOG WALKING (VRAnalogWalk). Off: the stick is eight-way and full speed,
// as it always was. On: the stick's angle is the direction exactly and how far
// it is pushed is the speed, from a slow walk to a run, which is how a
// treadmill reports the player's pace.
//
// Both only change the MOVEMENT frame and speed in CMoveMgr. The eight-way
// command flags the rest of the game reads (animation, footsteps, the drawn
// legs) are still sent as before, so nothing else sees a difference.

#ifndef _VRWALK_H_
#define _VRWALK_H_

namespace VRWalk
{
	// Once per movement update, before anything below. False when VR is not
	// driving the player (no headset, controls off, a vehicle): then nothing
	// here applies and the game moves exactly as it did.
	bool	Update(bool bVehicle);

	// Added to the body's yaw for the movement frame, in LithTech's radians.
	float	YawOffset();

	// Analog walking, this update: whether it applies, the stick's direction
	// (X right, Y forward, normalised) and the speed as 0..1 of a run.
	bool	Analog();
	float	DirX();
	float	DirY();
	float	Speed();

	// Analog walking pushed far enough to count as running (for the footstep
	// noise the guards hear and the run animation).
	bool	WantsRun();

	// For the drawn body. Live: the last Update applied (in play, no vehicle).
	// Mode: 0 body, 1 head, 2 off hand. Moving: the move stick is past its
	// dead zone.
	bool	Live();
	int		Mode();
	bool	Moving();
}

#endif

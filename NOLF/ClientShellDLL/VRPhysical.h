// VRPhysical.h: PHYSICAL PLAY, the game in your hands (VR Options > Physical
// Play). Every part is its own switch, every switch is OFF by default, and
// the button way always stays.
//
//   VRHolsters      Holsters on the body. The gun hand draws by gripping where
//                   a weapon is worn: gun-side hip = sidearm, over the gun-side
//                   shoulder = long gun, off-side chest = gadget, belt front =
//                   coin and lipsticks. Gripping at the slot of the weapon in
//                   the hand puts it away (the fists come out); gripping at a
//                   slot with nothing of its kind stows the weapon in the hand
//                   there, and that slot keeps it.
//   VRManualReload  The reload button ejects the clip (and a clip fired empty
//                   drops by itself). The gun then only dry-fires until the
//                   off hand grips at the ammo pouch (off-side hip) and brings
//                   the clip to the gun.
//   VRSwingMelee    A hand swung fast enough hits the character it passes
//                   through, with the game's own melee damage type and amount,
//                   scaled by where it lands (the skeleton's damage factors)
//                   and by the hand's speed. NOLF's rule stands: people only
//                   go down to a blow to the head they did not see coming.
//   VRWristHud      Health, armor and the clip on the off-hand wrist, in the
//                   game's own HUD font. Turn the wrist to read it.
//   VRGlassesToFace The sunglasses (camera, mine detector, infrared) go on
//                   when the hand brings them to the eyes and stay on; the hand
//                   back at the eyes with the grip takes them off. In the hand
//                   and not on, the trigger takes no photo.
//
// THE BODY. Slots hang from a neck point under the head (a neck model, so
// looking down at a holster does not move it), turned by a body yaw that
// follows the head outside +/-45 degrees, the way the drawn body stands.
// Heights are offsets from that neck point; VRHolsterHeight (cm) moves them
// all. The Leftorium mirrors every slot: "gun side" is the hand holding the gun.
//
// HAND SPEED is measured here from the tracked positions (about 33 ms apart,
// never less than 20), so it needs nothing from the host that it does not
// already send. Every event is logged with its numbers so a headset run can be
// read back.

#ifndef _VRPHYSICAL_H_
#define _VRPHYSICAL_H_

#include "ltbasedefs.h"

class CGameClientShell;
class CWeaponModel;
struct WEAPON;
struct AMMO;

namespace VRPhysical
{
	// Once a frame while the controllers drive the game in play (from
	// VRUpdateControllerInput, after VRBinds::Update and before any grip is
	// read). bRiding: on a vehicle, where the grips are the handlebars.
	void	Update(CGameClientShell* pShell, bool bRiding);

	// Not in play (a menu, a load, no controllers): let go of anything held.
	void	Idle();
	// A world was entered (a level or a save): the clips remembered per weapon
	// belong to the last one.
	void	OnWorldEntered();

	// This hand's grip belongs to physical play right now, it closed on a
	// slot or it is carrying a clip, so running, the scope zoom and the
	// support hand leave it alone until it opens. 0 off hand, 1 gun hand.
	bool	GripTaken(int nHand);

	// MANUAL RELOAD, asked by the weapon.
	// Does it apply to the weapon in hand right now (switch on, VR live, a
	// firearm with a clip, a reload of its own)?
	bool	ManualReloadFor(CWeaponModel* pWM);
	// The reload command: with manual reload in force it ejects the clip and
	// returns true (the game's reload is then not run).
	bool	OnReloadCommand(CWeaponModel* pWM);
	// A clip fired empty drops: called by the weapon when it would reload.
	void	OnClipRanOut(CWeaponModel* pWM);
	// The weapon was just selected: the clip it had when it was put away.
	void	OnSelect(CWeaponModel* pWM);

	// The sunglasses are in hand but not at the eyes: the trigger does nothing.
	bool	GlassesBlockFire(CWeaponModel* pWM);
	// They are on: the glasses in the hand are not drawn.
	bool	GlassesWorn();

	// THROWN BY HAND (VRThrowByHand): with the coin or a lipstick in hand the
	// trigger readies and letting go throws. True when it has the trigger;
	// *pbFire is what the fire command should be this frame.
	bool	ThrowTrigger(CWeaponModel* pWM, bool bTrigger, bool* pbFire);
	// The weapon is sending its fire message: the thrown direction and the
	// speed for the server's velocity override (world units per second).
	bool	TakeThrow(CWeaponModel* pWM, LTVector* pDir, float* pfVelocity);

	// TWO HANDS ON A LONG GUN (VRTwoHanded): the off hand holds its front and
	// the gun aims from hand to hand.
	bool	TwoHanded();

	// For the wrist display and the log.
	const char* SlotName(int nSlot);
}

#endif

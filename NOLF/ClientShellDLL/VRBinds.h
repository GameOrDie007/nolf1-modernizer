// VRBinds.h: which controller button does which game action, the Controls
// page in Options > VR, and the one place gameplay asks.
//
// ONE MAP PER CONTROLLER. The Quest's Touch controllers and the Steam Frame's
// are laid out differently (the Frame's left hand has a D-pad where Touch has
// X and Y), so each keeps its own map, and the page shows the one in the
// player's hands. Stored as console variables, VRBindQuest<Action> and
// VRBindFrame<Action>, holding a Pad number; a variable that is not there
// means the default, so a config written before this existed plays exactly
// as it did.
//
// NOT MAPPABLE, on purpose: the right trigger (fire; the wheel's confirm; the
// throttle on a vehicle), the grips (run, the scope's zoom, the support hand,
// steering, the recenter chord), moving and turning on the sticks and the
// menu button. (The turning stick's UP and DOWN are free, and are pads.) Those jobs
// depend on WHICH hand and on holding, and moving them breaks something else.
//
// LEFTORIUM. On Touch the whole map mirrors with the hands, as the buttons
// always have (X and Y swap with A and B). The Frame's face buttons are all on
// its right controller, so its map stays where it is.

#ifndef _VRBINDS_H_
#define _VRBINDS_H_

#include "ltbasetypes.h"

namespace VRBinds
{
	enum Pad
	{
		PAD_NONE = 0,
		PAD_A, PAD_B, PAD_X, PAD_Y,
		PAD_LSTICK, PAD_RSTICK, PAD_LTRIGGER,
		// The turning stick pushed up or down, where many VR games put jump
		// and duck. Not stored anywhere before v1.2, so inserting them here
		// moved only the Frame's numbers, which had not shipped.
		PAD_RUP, PAD_RDOWN,
		// Steam Frame only
		PAD_DUP, PAD_DDOWN, PAD_DLEFT, PAD_DRIGHT,
		PAD_LSHOULDER, PAD_RSHOULDER,
		PAD_COUNT
	};

	enum Act
	{
		ACT_USE, ACT_RELOAD, ACT_JUMP, ACT_DUCK,
		ACT_FLASHLIGHT, ACT_WHEEL, ACT_HOLSTER, ACT_QUICKSAVE,
		ACT_PREVWEAPON, ACT_NEXTWEAPON, ACT_NEXTAMMO,
		ACT_COUNT
	};

	const char*	ActLabel(int nAct);				// "Use / activate"
	const char*	PadLabel(int nPad, bool bFrame);	// "X (left)", "D-pad up"
	int			PadCount(bool bFrame);			// pads this controller has, PAD_NONE included

	int			Default(int nAct, bool bFrame);
	int			Get(int nAct, bool bFrame);
	void		Set(int nAct, bool bFrame, int nPad);
	void		ResetDefaults(bool bFrame);

	// Is the Steam Frame's profile the one bound right now?
	bool		Frame();

	// Once per frame, before any of the three below: reads the controllers.
	// bStickBusy: the right stick belongs to something else (the weapon
	// wheel, a menu), so its up and down are not buttons; after that it must
	// come back to the middle before they are again.
	void		Update(bool bStickBusy);
	bool		Held(int nAct);
	bool		Pressed(int nAct);		// went down this frame
	bool		Released(int nAct);		// came up this frame
	// The pad itself, for the vehicle rule (the left trigger is the brake).
	int			PadOf(int nAct);
}

#endif

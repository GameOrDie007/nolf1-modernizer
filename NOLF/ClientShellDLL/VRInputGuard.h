// ----------------------------------------------------------------------- //
//
// MODULE  : VRInputGuard.h
//
// PURPOSE : The DirectInput start-up fix, from inside the client
//
// ----------------------------------------------------------------------- //

#ifndef __VR_INPUT_GUARD_H__
#define __VR_INPUT_GUARD_H__

// Called from the client shell's constructor, which runs before the engine
// starts DirectInput. If the game's own DINPUT.dll (the proxy beside
// lithtech.exe, host\dinputproxy) is the one loaded, does nothing. If Windows
// loaded its own dinput.dll instead, patches lithtech.exe's imports so the
// same two fixes apply: the all-devices sweep is limited to keyboard and
// mouse, and the keyboard keeps working with the window behind.
void VRInputGuard_Install();

#endif // __VR_INPUT_GUARD_H__

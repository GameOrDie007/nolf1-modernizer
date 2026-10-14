// VRGunParts.h: the moving parts of each gun's own model - the slide or bolt
// that racks, and the magazine - so the hands can work them.
//
// Found by watching the game's own fire and reload animations (VRDebugGunParts):
// most models name their parts nothing better than "cube7", so they are listed
// here per gun by the names the animations moved. Travel is how far the slide
// or bolt goes back, in the gun model's own units.

#ifndef _VRGUNPARTS_H_
#define _VRGUNPARTS_H_

namespace VRGunParts
{
	enum Role { NONE = 0, SLIDE = 1, MAGAZINE = 2 };

	// What this node of this gun is, by the gun's slug and the node's name.
	// For a SLIDE, *pfTravel is how far it goes back.
	Role	RoleOf(const char* pszGun, const char* pszNode, float* pfTravel);
}

#endif

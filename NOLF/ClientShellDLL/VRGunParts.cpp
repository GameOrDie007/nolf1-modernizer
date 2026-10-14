// VRGunParts.cpp: see VRGunParts.h.

#include "stdafx.h"
#include "VRGunParts.h"

#include <string.h>

namespace VRGunParts
{
namespace
{
	struct GunDef
	{
		const char*	pszGun;
		const char*	apSlide[4];
		float		fTravel;
		const char*	apMag[3];
	};
	// From the desk measurement of the game's own animations (13 Oct 2026).
	// The revolver and the crossbow have no slide or bolt: nothing racks. A
	// magazine is taken off only where the model names it one: the P38's
	// reload moves parts that carry the gun's whole body (desk, 13 Oct).
	const GunDef kGuns[] = {
		{ "p38",         { "cube7", "cube8", "cyl4", 0 },                       0.29f, { 0, 0, 0 } },
		{ "luger",       { "cyl7", "cyl8", 0, 0 },                              0.28f, { 0, 0, 0 } },
		{ "delisle",     { "Delisle_bolt2", "Delisle_bolthandle2", "cyl2", 0 }, 0.40f, { "Delisle_magazine2", 0, 0 } },
		{ "sterling",    { "cyl129", 0, 0, 0 },                                 0.36f, { "clip3", 0, 0 } },
		{ "walther_smg", { "cube54", "cyl142", 0, 0 },                          0.34f, { 0, 0, 0 } },
		{ "ak47",        { "extru15_2_1", "extru16_2_1", 0, 0 },                0.21f, { 0, 0, 0 } },
			};
}

Role RoleOf(const char* pszGun, const char* pszNode, float* pfTravel)
{
	if (!pszGun || !pszNode || !pszNode[0]) return NONE;
	for (int g = 0; g < (int)(sizeof(kGuns) / sizeof(kGuns[0])); ++g)
	{
		if (_stricmp(kGuns[g].pszGun, pszGun) != 0) continue;
		for (int k = 0; k < 4 && kGuns[g].apSlide[k]; ++k)
			if (_stricmp(kGuns[g].apSlide[k], pszNode) == 0)
			{
				if (pfTravel) *pfTravel = kGuns[g].fTravel;
				return SLIDE;
			}
		for (int k = 0; k < 3 && kGuns[g].apMag[k]; ++k)
			if (_stricmp(kGuns[g].apMag[k], pszNode) == 0) return MAGAZINE;
		return NONE;
	}
	return NONE;
}

}	// namespace VRGunParts

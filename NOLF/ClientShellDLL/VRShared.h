// ----------------------------------------------------------------------- //
//
// MODULE  : VRShared.h
//
// PURPOSE : The data contract between the x64 OpenXR host and the x86 game
//           client. Shared memory, written by the host, read by the client.
//
//           Included by BOTH processes - one copy, no duplication. The host
//           build adds this directory to its include path.
//
//           LAYOUT RULES: only uint32_t and float. No pointers, no size_t, no
//           bool, no enums, nothing whose size or alignment differs between
//           x86 and x64. Every field is 4 bytes with 4-byte alignment, so the
//           struct is identical in both.
//
// ----------------------------------------------------------------------- //

#ifndef __VRSHARED_H__
#define __VRSHARED_H__

#include <windows.h>
#include <stdint.h>

#define VRSHARED_NAME		"Local\\NOLFVR_SharedState"

// The shared eye texture the renderer publishes. Opened by name rather than by
// handle: the game is 32-bit and the host is 64-bit, and a name crosses that
// without marshalling anything.
#define VRSHARED_EYETEX_NAME	L"Local\\NOLFVR_EyeTex"
// The pause menu's own picture: one eye wide, transparent where there is
// no menu art. Presented by the host as a world-anchored quad.
#define VRSHARED_OVLTEX_NAME	L"Local\\NOLFVR_OvlTex"
#define VRSHARED_MAGIC		0x56464C4E		// 'NLFV'
#define VRSHARED_VERSION	20		// 20: Game Or Die Hands (grip pose, touch)  19: host process id  18: recenter  17: haptics  16: the frame marker in memory  15: pause overlay  7: optical centres  8: exact pose  9: fov calibration  10: head-as-mouse  11: body yaw  12: per-eye frustum  13: shared eye texture  14: thumbstick axes

// Bits packed into VRHandState::nButtons. These belong to the contract, not
// to either process: the host sets them and the client reads them. They lived
// only in host/xrvr.h until version 14, which is why nothing on the client
// side could name a button.
#define VRBTN_TRIGGER		(1u << 0)
#define VRBTN_GRIP			(1u << 1)
#define VRBTN_PRIMARY		(1u << 2)	// A on the right hand, X on the left
#define VRBTN_SECONDARY		(1u << 3)	// B / Y
#define VRBTN_THUMBCLICK	(1u << 4)
#define VRBTN_MENU			(1u << 5)	// the left Touch controller's menu button
// The Steam Frame's own buttons, set by the host only under the Frame's native
// profile (host/xrvr.h has the same numbers). VRShared::Poll re-lays them into
// the bits above before anything else reads the hands, see there.
#define VRBTN_PADX			(1u << 6)	// right X
#define VRBTN_PADY			(1u << 7)	// right Y
#define VRBTN_DPAD_UP		(1u << 8)	// left D-pad
#define VRBTN_DPAD_DOWN		(1u << 9)
#define VRBTN_DPAD_LEFT		(1u << 10)
#define VRBTN_DPAD_RIGHT	(1u << 11)
#define VRBTN_SHOULDER		(1u << 12)	// on the hand it belongs to
#define VRBTN_VIEW			(1u << 13)	// left View
#define VRBTN_PADMENU		(1u << 14)	// right Menu
#define VRBTN_FRAME			(1u << 15)	// both hands: the Frame's profile is bound

// Bits in VRSharedState::nFlags, set by the client.
// The game is closing. Without it the host saw the eye texture go stale and
// showed the flat window in the headset (the exit splash, stretched over the
// whole view), then held its last frame until the window closed, about 4 s.
// With it the host ends the headset view at once.
#define VRSHARED_F_QUITTING	(1u << 0)

// --------------------------------------------------------------------------
// MODELS: the client tells the renderer what to draw.
//
// Not through the shared block and not through shared memory at all. CShell
// and d3dstub.ren are both loaded into lithtech.exe, so this is one process
// talking to itself: the client resolves R3D_PublishModels out of the
// renderer with GetProcAddress and hands over a pointer. Nothing is copied
// across a boundary because there is no boundary.
//
// Why the client is the one that knows: the renderer has no model slot among
// its 37, and the retail renderer's model phase takes no arguments - it reads
// a global an earlier phase filled, so there is no list to intercept on that
// side. The client has the engine's own object list through the SDK.
//
// The node transforms are the engine's, posed this frame. A character is a
// hierarchy posed every frame and that is the expensive half of drawing one;
// ILTModel::GetNodeTransform hands it over for nothing.
// --------------------------------------------------------------------------

#define VRMODELS_MAGIC		0x4C444D56	// 'VMDL'
// 192 instances, and it is ENOUGH - but only because the client publishes
// them nearest-first. Which models make the cut used to be decided by
// enumeration order, so a character at point-blank range competed with a
// crate 4000 units away and lost about a third of the time; that is why
// one came and went depending on which way the player faced.
//
// Raising this to 512 was tried and REVERTED. It does remove the drops
// (245-256 published, 0 dropped) and it costs too much: the world render
// went from ~0 to 11.5-13.0 ms and the frame from 11 to 14-16 ms, because
// every extra instance is skinned on the CPU. NOLF advances a fixed
// timestep per frame, so 65 FPS is not a slower picture, it is a slower
// GAME. Dropping the FARTHEST models costs nothing visible; skinning them
// costs the frame rate. Fix the cost before raising the cap.
//
// RAISED TO 512, AND THE 192 IS NOW A RUNTIME CAP RATHER THAN A CEILING.
// The paragraph above is still true - skinning 512 instances on the CPU cost
// 3-5 ms of frame time when it was tried - but "fix the cost before raising
// the cap" needs both arms in ONE build to be measurable at all, and a
// compile-time constant cannot give that. `+VRModelCap <n>` sets how many are
// published; the array is what the pair of DLLs agrees to carry.
//
// The VERSION goes with it: the struct is a different size now, and a client
// and a renderer built at different times must refuse each other loudly rather
// than memcpy past the end of an array.
// 5: the instance and node arrays are larger, so the FRAME IS A DIFFERENT
// SIZE and a client and a renderer built either side of this must refuse
// each other rather than read past the end of an array.
#define VRMODELS_VERSION	7	// 7: VRModelInst::nShowNodes  6: VRModelInst::nHideMask
#define VRMODEL_F_PLAYER	(1u << 0)
#define VRMODEL_F_NOLIGHT	(1u << 1)
// FLAG2_ADDITIVE, out of the object's SECOND flag word. The menu's mod
// flower motifs are models wearing deliberately DARK art - HORZBARB.DTX
// has a mean of 14,49,0 and SMADDITIVE1.DTX 16,55,0, and that filename
// says what it is for. Added onto the blue panel those values lift it
// into the pastel shapes retail shows; drawn opaque they are black blobs.
#define VRMODEL_F_ADDITIVE	(1u << 2)
// FLAG2_MULTIPLY, the OTHER blend mode in that same word.
//
// Multiplied colour: the source is a MASK, not a picture. Its clear area is
// WHITE, and white multiplied into the scene is the scene - the same trick
// FLAG2_ADDITIVE plays with black. Blood splats are the case that found this:
// SFX/TEST/BLOODL3.DTX is 64x64 with alpha 0 on every one of its 4096 texels,
// so there is no alpha channel to fall back on. Drawn alpha-blended it is a
// white square with a dark blob in it, which is exactly the white-outlined
// stains headset testing reported after shooting an enemy.
#define VRMODEL_F_MULTIPLY	(1u << 5)
// FLAG_VISIBLE IS NOT SET ON THIS OBJECT.
//
// The publish block reads the object's flag word already - it has to, for
// FLAG_NOLIGHT - and has never looked at the one bit that says whether the
// engine draws this object at all. So an object retail hides, we draw. That is
// the same class of defect as the editor's marker geometry: not a wrong
// picture of the right thing, but the right picture of a thing that should not
// be on screen.
//
// Sent as a FLAG rather than acted on in the client, so the renderer can count
// it before anything skips anything. A rule that deletes has to be able to say
// how much it deleted and why.
#define VRMODEL_F_INVISIBLE	(1u << 3)
// THE FIRST-PERSON WEAPON, which is not like any other instance in the list.
//
// It is positioned CAMERA-RELATIVE by the retail path - "this is all now
// camera-relative", in WeaponModel.cpp's own words - so in world coordinates it
// sits near the MAP ORIGIN, thousands of units from the player. The publish
// finds its instances with FindObjectsInSphere around the CAMERA, so the view
// weapon has never been in the list, and every line of the VR hand-aim code has
// been writing to an object nobody draws. Measured: driving the hand three
// metres sideways put it at x +167.8 units and left the capture byte-identical.
//
// So it is appended by hand, with its node transforms carried into world space,
// and flagged - because the renderer skips anything sitting at the camera, and
// this one has to be the exception.
#define VRMODEL_F_VIEWMODEL	(1u << 4)
// THE PLAYER OWNS IT: her own object or one of its attachments (the carried
// weapons). The renderer's "skip any model whose box holds the camera" rule
// was written for these, and applied to everything: walk up to a character
// until the eye is inside its box - 12 units, a forearm's length - and the
// character vanished. Headset testing, HQ: characters disappeared when the
// headset moved too close. Now the rule asks for this flag.
#define VRMODEL_F_OWNED		(1u << 6)

// FLAG_ENVIRONMENTMAP on the object. Its skin's alpha channel is the
// environment-map MASK, not transparency: the snowmobile's SNOW_PV.DTX is
// graded on 95% of texels and the renderer's glass-by-texture rule drew the
// sled as a blue film with a black hole where its body should be (16
// September). With this bit the renderer draws the skin opaque.
#define VRMODEL_F_ENVMAP	(1u << 7)

// The player-view VEHICLE (snow_pv, moto_pv, windshield). A view model
// whose hands belong in the picture: the renderer hides a view model's arm
// pieces because the VR gun sits at the controller, and a sled's hands on
// its bars are not that.
#define VRMODEL_F_VEHICLE	(1u << 8)

// The ridden VEHICLE drawn as its HANDS only: the whole vehicle model is
// under the rider (VRVehicleBody) and carries its own bars and dash.
#define VRMODEL_F_HANDSONLY	(1u << 9)

// THE PLAYER'S BODY IN ITS TWO ROLES. Looking down (Options > VR > Show body)
// it is drawn in the eyes only, without its head and arms - the view weapon
// is your hands - and HIDENODES says so: nShowNodes then lists the nodes whose
// triangles are NOT drawn. In a mirror (See yourself in mirrors) it is drawn
// in mirror passes only, whole, its arms bent to the controllers: MIRRORONLY.
// With both on, the client sends the body twice, once per role.
#define VRMODEL_F_HIDENODES	(1u << 10)
#define VRMODEL_F_MIRRORONLY	(1u << 11)

// 4096, up from 512, so persistent shell casings are not silently dropped
// by the TRANSPORT rather than by the game's own cap. VRModelInst is 84
// bytes, so this array is 344 KB - the shared block is not where this gets
// expensive. The per-frame mesh build is, and that is measured.
#define VRMODELS_MAX_INST	4096
// AND THE NODES WITH THEM, or the instances have nowhere to put their
// skeletons. A casing is a one or two node model, so four thousand of them
// can eat the whole old budget and leave nothing for the characters, who
// need twenty-five each.
#define VRMODELS_MAX_NODES	32768

#pragma pack(push, 4)

// A node's world transform: a 3x4 ROW-MAJOR matrix, the translation in the
// FOURTH COLUMN of each row - m[3], m[7], m[11] - which is LTMatrix's own
// layout with the unused fourth row dropped.
//
//   world.x = m[0]*p.x + m[1]*p.y + m[2] *p.z + m[3]
//   world.y = m[4]*p.x + m[5]*p.y + m[6] *p.z + m[7]
//   world.z = m[8]*p.x + m[9]*p.y + m[10]*p.z + m[11]
//
// This comment used to say "three basis rows then the translation", i.e. that
// the translation was m[9..11]. It is not, and reading it that way puts part of
// the translation inside the rotation: a 98-vertex prop the engine places at
// (895.9 -1432.0 1475.9) reconstructed with half-extents of 18504 units. Read
// correctly it lands on that position to the decimal, 98 of 98 vertices inside
// the engine's own box. Verify against the object's published position before
// trusting any change here - p = (0,0,0) must give the object's origin.
struct VRModelNode
{
	float	m[12];
};

struct VRModelInst
{
	uint32_t	nObject;		// the engine's HOBJECT, for walking to the mesh
	float		fPos[3];		// world position
	float		fRot[4];		// world rotation, x y z w
	float		fDims[3];		// half-extents, so a box can stand in for the mesh
	uint32_t	nNodeFirst;		// index into VRModelFrame::nodes
	uint32_t	nNodeCount;
	// 1 = the player's own object.
	// 2 = FLAG_NOLIGHT: the engine is told not to light this object, so
	//     it wants its texture at full brightness. Every object in the
	//     interface scene is created that way - there is no light in
	//     that scene and Monolith's own commented-out experiment to add
	//     one is still in InterfaceMgr.cpp - so without this the menu's
	//     mod flower motifs draw near-black over the logo.
	uint32_t	nFlags;
	// THE OBJECT'S OWN SCALE, which used to be dropped on the floor.
	// The engine poses the NODES in world space, so a node transform
	// already carries this; the mesh offsets it skins do not. Sending
	// only the nodes puts every joint 30% further out than the limb
	// reaching it on the main menu's Cate (scale 1.300), which is why
	// her head floated above a stretched neck.
	float		fScale[3];
	// THE OBJECT'S OWN COLOUR AND ALPHA, which nothing has ever read.
	//
	// The world-model publish has read GetObjectColor since it was written -
	// it is how glass is told from a vending machine - and the sprite publish
	// reads it too. The MODEL publish never has, so every model in the game is
	// drawn at the colour its texture happens to be.
	//
	// The Morocco hotel lamps are the case that found this: PROPS/SKINS/
	// LAMP_05A and 05B are the same near-white picture, mean (216 232 225)
	// over the 11% of the sheet that is opaque, and retail draws that shade
	// YELLOW. The yellow is not in the texture, is not in the model - one
	// piece, 148 verts, bind box identical to its world box - and is not the
	// lighting. It is the colour the game sets on the object.
	//
	// 1,1,1,1 for anything untinted, which is nearly everything.
	float		fColour[4];
	// THE PIECES THE GAME HAS HIDDEN, bit k = piece k (the engine answers
	// for the first 32 only). A model can carry alternate pieces - the
	// credits Cate in T01S01 has 'torso' and 'torso2' - and drawing all of
	// them at once is a patchwork. 0 when nothing is hidden.
	uint32_t	nHideMask;
	// THE SUPPORT HAND (view weapon only): bit n = node n. When any bit is
	// set, the view model's Hand* pieces - normally not drawn at all, see the
	// renderer - are drawn, but only the triangles whose every vertex is
	// weighted mostly to one of these nodes. Zero for everything else.
	uint32_t	nShowNodes[4];
	// ...REFLECTED in this plane (n.x n.y n.z d, world, n.p = d) when n is
	// not zero. The client names the RIGHT hand's nodes and the gun's centre
	// plane: the right hand grips the pistol as the game animates it, and its
	// mirror image is a left hand wrapped round the other side of the same
	// grip. The game's own left hand is posed away from the gun (a flat game
	// keeps it out of frame), so it cannot be used where it stands.
	float		fShowMirror[4];
};

struct VRModelFrame
{
	uint32_t	nMagic;
	uint32_t	nVersion;
	uint32_t	nFrame;			// so the renderer can tell a stale list
	uint32_t	nCount;			// instances actually filled
	uint32_t	nNodeCount;
	uint32_t	nDropped;		// instances that did not fit, so a cap is visible
	VRModelInst	inst[VRMODELS_MAX_INST];
	VRModelNode	nodes[VRMODELS_MAX_NODES];
};

#pragma pack(pop)

// What the client resolves out of d3dstub.ren. The pointer stays valid until
// the next call; the renderer copies nothing.
typedef void (__cdecl *VRModelPublishFn)(const VRModelFrame*);

// --------------------------------------------------------------------------
// WORLD MODELS THAT MOVE
//
// A door, a lift, a rotating sign: LithTech builds these as WorldModels, and
// the renderer draws every WorldModel at the vertex positions the level file
// authored. Nothing updates them, so every door in the game is frozen shut
// while the engine has plainly opened it - the player can hear it, and walk through
// it, and still see it closed across the doorway.
//
// The engine keeps the vertices still and moves the OBJECT. Only the client can
// see that: OT_WORLDMODEL objects have a live position and rotation like any
// other. So the client sends them across and the renderer applies them.
//
// The link between the two is the thing that has to be found: the client knows
// an HOBJECT, the renderer knows a WorldModel struct in the heap. R3D probes
// for the offset that joins them, the same way the model pointer at +0x1DC and
// the texture name at [[+0x24]+0x1C] were found.
#define VRWORLD_MAGIC		0x444C5257	// 'WRLD'
// 2: VRWorldInst carries nFlags. The struct is a different size, so a
// client and a renderer built at different times must refuse each other
// loudly rather than read past the end of the array.
#define VRWORLD_VERSION		2
// FLAG_VISIBLE IS CLEAR ON THIS BRUSH.
//
// The world publish has never carried a flag word, so the renderer has
// had no way to know the engine had HIDDEN a world model - and a brush
// the engine hides is one we keep drawing. A gate lock that has been shot
// off is the case that found this.
//
// Sent as a flag rather than acted on in the client, so the renderer can
// COUNT it before anything skips anything. The model publish learned the
// same lesson the hard way: a rule that deletes has to be able to say how
// much it deleted and why.
#define VRWORLD_F_INVISIBLE	(1u << 0)
// FLAG_FOGDISABLE, the TranslucentWorldModel's FogDisable property. The
// retail renderer draws a sky object that carries it with fog OFF, which is
// how a level keeps its cloud sheet and sun clear while its sky fog sinks the
// panorama walls: the factory's Clouds1, Moon and MoonFlare all set it, and
// its sky fog is -1000..100 against walls 320 units off, so everything the
// renderer fogged came out one flat colour (desk, 22 September).
#define VRWORLD_F_FOGDISABLE	(1u << 1)
#define VRWORLD_MAX			1024

#pragma pack(push, 4)
struct VRWorldInst
{
	uint32_t	nObject;		// the client's HOBJECT, as a number
	float		fPos[3];		// live world position
	float		fRot[9];		// live rotation, three basis rows
	// THE AUTHORED ALPHA, which is the only honest answer to "is this glass".
	// NOLF's TranslucentWorldModel class is used for plenty of OPAQUE props -
	// a vending machine, a bench - and guessing from the model's NAME made
	// drinking fountains see-through because one is called Water_Fountain.
	// The object knows: GetObjectColor returns what the level author set.
	float		fAlpha;
	// VRWORLD_F_*. See the note above.
	uint32_t	nFlags;
};

struct VRWorldFrame
{
	uint32_t	nMagic;
	uint32_t	nVersion;
	uint32_t	nFrame;
	uint32_t	nCount;
	VRWorldInst	inst[VRWORLD_MAX];
};
#pragma pack(pop)

typedef void (__cdecl *VRWorldPublishFn)(const VRWorldFrame*);

// --------------------------------------------------------------------------
// SPRITES
//
// Every visual effect NOLF has is a sprite: lamp glows, muzzle flashes, smoke,
// light coronas - 142 of them on the Morocco level alone - AND the main menu's
// whole background, which is CInterfaceMgr::m_BackSprite, a CBaseScaleFX of
// type OT_SPRITE rendered through the interface camera.
//
// This renderer draws world geometry and skinned models and has never drawn a
// sprite, which is why the menu is black and why the game has no effects. A
// sprite is a camera-facing textured quad at a position, with a scale and a
// colour, so the only hard part is finding its texture - and the object is
// probed for that the same way the model pointer and the world model were.
#define VRSPRITE_MAGIC		0x54525053	// 'SPRT'
// 4: VRSpriteInst carries its own RIGHT and UP, so a decal can lie flat on
// the surface it was put on instead of turning to face the player.
#define VRSPRITE_VERSION	4

// A CORONA IS ADDITIVE ART. Its texture is a glow on a BLACK field, and black
// added to a scene is the scene - which is the whole reason it can be a square
// quad hanging in the air and still look like a halo. Blend it as alpha
// instead and that black field is opaque: a hard black box round every
// streetlight and lit window, which is exactly what the headset showed.
//
// The flag was never published. The model path has read FLAG2_ADDITIVE since
// the additive work; the sprite path read position, scale and colour and
// stopped there.
#define VRSPRITE_F_ADDITIVE	(1u << 0)
// FLAG2_MULTIPLY. See VRMODEL_F_MULTIPLY - a mask on a WHITE field, where
// additive art is a glow on a black one. Every bullet mark and blood splat in
// the game arrives on this path.
#define VRSPRITE_F_MULTIPLY	(1u << 1)
// FLAG_ROTATEABLESPRITE: THIS SPRITE HAS ITS OWN ORIENTATION AND MUST NOT BE
// BILLBOARDED.
//
// Every sprite here has been drawn as a camera-facing quad, which is right for
// a muzzle flash, a lamp corona or a lens flare - they have no orientation and
// should always face the viewer. It is wrong for a DECAL. A bullet hole and a
// blood splat are created with FLAG_ROTATEABLESPRITE and a rotation whose
// forward is the surface normal, precisely so they lie flat on the wall.
//
// Billboarded, a decal turns to follow the viewer: on a monitor a subtle
// wrongness nobody names, in a headset unmistakable - testing reported blood and
// holes that followed the player around the world and with head movement
// and would not sit flat. It is also why the same splats read as standing off
// the wall: a billboard pivoting about a point proud of a surface sweeps.
#define VRSPRITE_F_ROTATABLE	(1u << 2)
// NO DEPTH TEST. The aim dot only. Its ray stops at the first solid thing, so
// it is never behind a wall; what it CAN be behind is the body it is placed on
// (a character's box is a column wider than the character, and the dot is put
// on the character's own axis, not on the box face) and the player's gun. Both
// should lose to it. A flag in the record, not a name match in the renderer.
#define VRSPRITE_F_NODEPTH	(1u << 3)
// Bullet holes and blood splats are SPRITES, and they are to be kept.
// VRSpriteInst is 44 bytes, so 4096 of them is 180 KB.
#define VRSPRITE_MAX		4096

#pragma pack(push, 4)
struct VRSpriteInst
{
	uint32_t	nObject;
	uint32_t	nFlags;			// VRSPRITE_F_*
	float		fPos[3];
	float		fScale[3];		// GetObjectScale; sprites are scaled, not sized
	float		fColour[4];		// r,g,b,a as the author set them
	// THE SPRITE'S OWN AXES, for the ones that have them. Right and up rather
	// than a full rotation because that is exactly what building a quad needs.
	// Meaningless unless VRSPRITE_F_ROTATABLE is set.
	float		fRight[3];
	float		fUp[3];
};

struct VRSpriteFrame
{
	uint32_t		nMagic;
	uint32_t		nVersion;
	uint32_t		nFrame;
	uint32_t		nCount;
	VRSpriteInst	inst[VRSPRITE_MAX];
};
#pragma pack(pop)

typedef void (__cdecl *VRSpritePublishFn)(const VRSpriteFrame*);

// The level's fog. Not a frame struct - six numbers the client reads straight
// off the engine console variables the level set, so there is nothing to
// version and nothing to copy.
typedef void (__cdecl *VRFogPublishFn)(int, float, float, float, float, float);

// ---------------------------------------------------------------------------
// PRIMITIVES - everything the retail renderer drew that is not a model, a
// world polygon or a sprite: PARTICLE SYSTEMS (steam, smoke, sparks, blood,
// snow), LINE SYSTEMS (rain), POLYGRIDS (water surfaces) and the game's own
// CANVAS polygons (vehicle trails, the zip cord, breaking-glass shards).
// None of these reach a renderer DLL by themselves: in the retail path the
// renderer walked the engine's object list for them, and ours reads the
// heap only for the world. So the client, which has an API for every one of
// them, turns each into plain coloured, optionally textured geometry and
// hands it over once a frame, the way the sprites go.
//
// One channel for all four, because to the renderer they are the same thing:
// a RUN (one texture, one blend, one depth rule) over a range of VERTICES.
// A POINTS run is one vertex per particle and the renderer builds the
// camera-facing quad per eye - the client cannot, it does not know either
// eye. A LINES run is two vertices per line. A TRIS run is what it says.
// ---------------------------------------------------------------------------
#define VRPRIM_MAGIC		0x4D495250	// 'PRIM'
#define VRPRIM_VERSION		1
#define VRPRIM_MAX_RUNS		2048
#define VRPRIM_MAX_VERTS	131072

#define VRPRIM_T_TRIS		0
#define VRPRIM_T_POINTS		1		// fSize is the world-unit half-width
#define VRPRIM_T_LINES		2

#define VRPRIM_F_ADDITIVE	(1u << 0)	// FLAG2_ADDITIVE / LTBLEND_ONE,ONE
#define VRPRIM_F_MULTIPLY	(1u << 1)	// FLAG2_MULTIPLY / LTBLEND_ZERO,SRCCOLOR
#define VRPRIM_F_NOZREAD	(1u << 2)	// drawn on top of everything
#define VRPRIM_F_ZWRITE		(1u << 3)	// writes depth (opaque canvases)
#define VRPRIM_F_CLAMP		(1u << 4)	// LTTEXADDR_CLAMP
#define VRPRIM_F_NOTEX		(1u << 5)	// colour only, no texture
#define VRPRIM_F_DIFFUSEALPHA (1u << 6)	// LTOP_SELECTDIFFUSE: alpha from the vertex, not the texture
#define VRPRIM_F_REALLYCLOSE (1u << 7)	// FLAG_REALLYCLOSE: view-relative; not drawn yet
#define VRPRIM_F_SCOPELENS   (1u << 8)	// the run's texture is the scope's own render, not szTex
#define VRPRIM_F_SURFACE     (1u << 9)	// the run's texture is a 2D SURFACE the client drew (its handle in nObject)

#pragma pack(push, 4)
struct VRPrimVert
{
	float		fPos[3];
	float		fColour[4];		// r,g,b,a 0-1
	float		fUV[2];
	float		fSize;			// POINTS only
};

struct VRPrimRun
{
	char		szTex[64];		// .dtx or .spr, the game's own path; "" with NOTEX
	uint32_t	nType;			// VRPRIM_T_*
	uint32_t	nFlags;			// VRPRIM_F_*
	uint32_t	nStart, nCount;	// into verts[]
	float		fCentre[3];		// for back-to-front ordering
	uint32_t	nObject;		// the engine object, for the log
	uint32_t	nKind;			// 0 particles, 1 lines, 2 polygrid, 3 canvas
};

struct VRPrimFrame
{
	uint32_t	nMagic;
	uint32_t	nVersion;
	uint32_t	nFrame;
	uint32_t	nRuns;
	uint32_t	nVerts;
	float		fTime;			// the engine's clock, for animated textures
	VRPrimRun	runs[VRPRIM_MAX_RUNS];
	VRPrimVert	verts[VRPRIM_MAX_VERTS];
};
#pragma pack(pop)

typedef void (__cdecl *VRPrimPublishFn)(const VRPrimFrame*);

// ---------------------------------------------------------------------------
// GAME OR DIE HANDS: Cate's own articulated hands (VRHands.cpp). The client
// poses them from the controllers and skins them itself, so what crosses is
// finished world-space triangles, one run per hand, one texture each, lit
// by the renderer like any model standing where the hand is. Non-indexed,
// three vertices a triangle, the way the model mesh is built.
// VRHANDS_F_HIDEGUNHANDS: the view weapon's own Hand* pieces are not drawn at
// all this frame (not even the mirrored support hand), ours are the hands.
// ---------------------------------------------------------------------------
#define VRHANDS_MAGIC		0x53444E48	// 'HNDS'
#define VRHANDS_VERSION		1
#define VRHANDS_MAX_VERTS	24576
#define VRHANDS_F_HIDEGUNHANDS	(1u << 0)

#pragma pack(push, 4)
struct VRHandVert
{
	float		fPos[3];		// world units
	float		fNrm[3];
	float		fUV[2];
};

struct VRHandRun
{
	char		szTex[128];		// a 32-bit .tga, relative to the game folder
	uint32_t	nStart, nCount;	// into verts[]
	float		fLightAt[3];	// where the hand is, for the light it stands in
};

struct VRHandsFrame
{
	uint32_t	nMagic;
	uint32_t	nVersion;
	uint32_t	nFrame;
	uint32_t	nFlags;			// VRHANDS_F_*
	uint32_t	nRuns;			// 0..2
	uint32_t	nVerts;
	VRHandRun	runs[2];
	VRHandVert	verts[VRHANDS_MAX_VERTS];
};
#pragma pack(pop)

typedef void (__cdecl *VRHandsPublishFn)(const VRHandsFrame*);

// ---------------------------------------------------------------------------
// DYNAMIC LIGHTS. Every OT_LIGHT the client can see: muzzle flashes,
// explosions, the flickering and strobing lamps a level's LightFX objects
// drive, the flashlight. The retail renderer added them to the world's
// lightmaps and to every model they reached; ours read only the level file's
// static lights. Colour 0-1, radius in world units, linear falloff.
// ---------------------------------------------------------------------------
// THE SCOPE. The client says where the scope's objective lens is and which
// way it looks, in world units and the world's basis, and how wide a field
// the current zoom level wants. The renderer draws a third world pass from
// there into its own small texture, and any primitive run flagged
// VRPRIM_F_SCOPELENS is drawn with that texture - the client publishes the
// eyepiece as such a run, a disc of triangles with uv mapping the whole
// texture onto it. See R3D_PublishScope / R3D_DrawScopePass.
#define VRSCOPE_MAGIC		0x45504353	// 'SCPE'
#define VRSCOPE_VERSION		1
struct VRScopeFrame
{
	uint32_t	nMagic;
	uint32_t	nVersion;
	uint32_t	nFrame;
	uint32_t	nActive;		// 0: no scope this frame
	float		fPos[3];		// the objective lens, world
	float		fQuat[4];		// the scope's axis as a rotation (engine quaternion order)
	float		fFovDeg;		// horizontal field of the pass
	float		fNear, fFar;
	int32_t		nZoom;			// the game's level, for the log
};

#define VRLIGHT_MAGIC		0x54474C44	// 'DLGT'
#define VRLIGHT_VERSION		1
#define VRLIGHT_MAX			256
#define VRLIGHT_F_WORLD		(1u << 0)	// lights the world
#define VRLIGHT_F_OBJECTS	(1u << 1)	// lights models

#pragma pack(push, 4)
struct VRLightInst
{
	float		fPos[3];
	float		fRadius;
	float		fColour[3];
	uint32_t	nFlags;
	uint32_t	nObject;
};
struct VRLightFrame
{
	uint32_t	nMagic, nVersion, nFrame, nCount;
	VRLightInst	lights[VRLIGHT_MAX];
};
#pragma pack(pop)
typedef void (__cdecl *VRLightPublishFn)(const VRLightFrame*);

#pragma pack(push, 4)

// Reserved now so adding motion controllers post-v0.1 does not change the
// shape of the struct and break version compatibility between the processes.
struct VRHandState
{
	uint32_t	nActive;
	float		fPosX, fPosY, fPosZ;			// metres
	float		fYawDeg, fPitchDeg, fRollDeg;
	float		fTrigger, fGrip;				// 0..1
	uint32_t	nButtons;

	// Thumbstick, -1..+1, X right and Y up. Added in version 14: without an
	// analog stick there was no way to walk, and the player could not reach a
	// keyboard. The reserved space this struct's comment promised is what
	// made it a version bump rather than a redesign.
	float		fStickX, fStickY;
};

struct VRSharedState
{
	uint32_t	nMagic;
	uint32_t	nVersion;

	// Seqlock. The writer makes this odd before writing and even after, so a
	// reader that sees an odd or changed value knows it read a torn frame.
	// Avoids needing a mutex across a 32/64-bit process boundary.
	uint32_t	nSequence;
	uint32_t	nFlags;

	// Head pose. Euler degrees rather than a quaternion, deliberately: it
	// sidesteps the right-handed/left-handed conversion between OpenXR and
	// LithTech, and it lets a wrong axis be corrected with a console variable
	// instead of a rebuild.
	float		fHeadYawDeg;
	float		fHeadPitchDeg;
	float		fHeadRollDeg;

	float		fHeadPosX, fHeadPosY, fHeadPosZ;	// metres, OpenXR LOCAL space

	float		fIpdMeters;
	float		fFovLeftRad, fFovRightRad, fFovUpRad, fFovDownRad;

	uint32_t	nFrameCounter;
	uint32_t	nHostAliveTick;					// host's GetTickCount()

	VRHandState	Hands[2];						// post-v0.1

	// Written by the CLIENT, read by the host. The game's internal screen
	// surface, which is NOT the window client area - LithTech's client rect is
	// 16px larger in each axis, so cropping the client splits at the wrong
	// place and hands each eye a sliver of the other.
	uint32_t	nGameScreenW;
	uint32_t	nGameScreenH;

	// Also written by the CLIENT: the field of view it actually handed to the
	// renderer, full angles in radians. The host declares exactly this to the
	// runtime, so tuning the game's FOV keeps the projection consistent
	// automatically instead of the host guessing what was rendered.
	float		fGameFovXRad;
	float		fGameFovYRad;

	// 1 while the game is drawing a menu, loading screen or cutscene rather
	// than the world. Those are drawn ONCE across the whole window, not in
	// stereo, so splitting them hands each eye a different half of one flat
	// picture. The host shows the whole window to both eyes instead.
	uint32_t	nInMenu;

	// The same head rotation as a quaternion, in OpenXR's frame. The Euler
	// fields above lose information: the client rebuilt the rotation with three
	// sequential axis rotations, which only reproduces the original for motion
	// about a single axis. Pitch combined with roll - tilting the head - landed
	// the camera somewhere slightly different from the pose declared to the
	// runtime, which then reprojected to make up the difference.
	//
	// Taken from the reserved block, so the struct size does not change.
	float		fHeadQuatX, fHeadQuatY, fHeadQuatZ, fHeadQuatW;

	// How far behind the current pose the displayed image actually is. The
	// host tags each submitted frame with the pose from this long ago, so it
	// has to match the real client-render-to-submit delay: too high and the
	// runtime over-corrects and the world swims behind the head, too low and it
	// snaps. Published by the client so it can be swept from the console
	// instead of needing a relaunch per value.
	float		fPoseLagMs;

	// 1 once the client has published fPoseLagMs. Needed because 0 is both the
	// zero-initialised state and a legitimate value - and it turned out to be
	// the best one, so "nonzero means set" would have made the measured answer
	// the one setting that could not be applied.
	uint32_t	nPoseLagValid;

	// Per-eye optical centre, radians, in OpenXR's frame. The Quest's lenses
	// are canted, so each eye's frustum centre sits about 7 degrees inward and
	// 5.5 degrees down from the eye's forward axis. Rendering on the forward
	// axis and declaring that honestly forces the runtime to resample into a
	// frustum pointing somewhere else, and that error grows toward the edges -
	// static while still, bending as soon as the head turns.
	//
	// Index 0 is the left eye. Written by the host every frame.
	float		fEyeCentreYawRad[2];
	float		fEyeCentrePitchRad[2];

	// Set by the CLIENT when it is actually rendering about those centres, so
	// the host knows to declare the matching rotated pose. Declaring one
	// without the other is worse than doing neither - that mismatch is what
	// ProjMode 1 did, and it put 14 degrees between the eyes.
	uint32_t	nAsymActive;

	// What the client ACTUALLY rotated each eye by. The host declares these
	// verbatim rather than re-deriving them from the centres above.
	//
	// Both ends deriving the same value independently is how the eyes end up
	// disagreeing: any difference in sign convention, composition order or
	// which components are enabled becomes divergence between the eyes, which
	// is unfusable rather than merely wrong. One side decides, the other obeys.
	float		fAppliedYawRad[2];
	float		fAppliedPitchRad[2];

	// 1 = the host should submit each image with the pose the client's frame
	// marker says it was rendered from, rather than the newest pose.
	//
	// Measured staleness is ~12 ms on average but ranges from 6 to 67 ms, so
	// the spread is larger than the mean and no fixed VRPoseLag can be right.
	// The marker gives the exact pose per image, which is the only version of
	// this correction that has a chance of working.
	uint32_t	nExactPose;

	// Calibration: the client renders the LEFT half at the normal aim and the
	// RIGHT half yawed by fCalibYawRad, with no IPD offset, so one captured
	// image contains the same scene from two known angles. The host correlates
	// the halves to find the pixel shift and hence the renderer's true field.
	//
	// Doing it within one frame avoids having to match two captures in time,
	// which cannot be done reliably when the game produces ~85 frames a second
	// and the compositor delivers 60.
	uint32_t	nCalibActive;
	float		fCalibYawRad;

	// 1 = the client is feeding head rotation through the game's own mouse-look
	// path, so the camera it rendered from ALREADY contains the head rotation
	// and the host must not ask the runtime to account for it again.
	//
	// The host answers by submitting the projection layer in VIEW space with
	// head-relative eye poses, which is the OpenXR way of saying "this image is
	// nailed to the display": the compositor then performs no reprojection at
	// all, for rotation or position.
	//
	// This is an experiment, not a mode. It splits the world bending with head
	// movement into its only two possible sources - the camera we render from,
	// and the reprojection the runtime applies to what we submit - by removing
	// the second one entirely. If the bending survives, it is ours; if it goes,
	// it is the declaration. Either answer closes a question that four separate
	// tuning attempts have failed to close. See docs/HEAD-AS-MOUSE.md.
	uint32_t	nHeadLocked;

	// The player's body yaw, radians, as the CLIENT holds it.
	//
	// The host has never known this, and that is the warping.
	//
	// The client renders with the head rotation composed onto the body's aim,
	// in the BODY's frame. The host declares the head pose in the runtime's
	// ROOM frame. Those two frames differ by exactly this angle, so when the
	// compositor corrects for head motion between render and display it turns
	// the image about axes that are rotated away from the ones it was drawn
	// with. The correction is misdirected, and by how much depends on where the
	// player is facing.
	//
	// Measured, head pitch +20 with no head roll, as the body yaw varies: at 0
	// the two frames agree and nothing is wrong; at 90 degrees a pitch becomes
	// a roll; at 180 a pitch is inverted. That is the shape of the fault.
	//
	// With this published the host can put the same rotation into the OpenXR
	// reference space it declares poses in and submits the layer in, so both
	// ends describe the image in the same frame.
	float		fBodyYawRad;

	// 0 = off, the shipped behaviour: declare in LOCAL and let the frames
	//     disagree.
	// 1 = on.
	// 2 = on with the opposite sign.
	//
	// The sign is a convention question between LithTech's yaw and OpenXR's,
	// and this project has guessed at signs and frames wrongly four times in
	// one day. It is cheaper to expose both and settle it with one binary look
	// than to be confident and wrong a fifth time.
	uint32_t	nYawSpaceMode;

	// THE PER-EYE FRUSTUM, as the runtime reports it. Index 0 is the left
	// eye. Written by the host every frame.
	//
	// fFovLeftRad and its three neighbours above are eye 0's only, and
	// that was enough while the client had to render a symmetric frustum
	// anyway - d3d.ren's entire camera API is two symmetric scalars. It
	// covered the asymmetric one by inflating it and then rotating each
	// eye onto its own optical centre, which is a correction applied to
	// the camera rather than to the projection, and correcting a
	// projection with a rotation is only exact at the centre of the image.
	//
	// Owning the renderer removes the constraint, so the eye's real
	// frustum can be built as a matrix. That needs all eight angles.
	float		fEyeFovLeftRad[2];
	float		fEyeFovRightRad[2];
	float		fEyeFovUpRad[2];
	float		fEyeFovDownRad[2];

	// 1 while the RENDERER is building each eye's true asymmetric
	// projection itself.
	//
	// The only field written by d3dstub.ren rather than by the client, and
	// that is deliberate: the component that decides whether it is doing
	// this is the component that should say so. Deriving it on both sides
	// from a console variable is how the two ends end up disagreeing, and
	// this block already carries two fields (fAppliedYawRad and
	// nAsymActive) that exist because of exactly that lesson.
	//
	// The host answers by declaring the runtime's own frustum verbatim
	// over the WHOLE image, rather than a symmetric one over a slice.
	uint32_t	nNativeFrustum;

	// --- the shared eye texture, version 13 -------------------------------
	//
	// All written by the RENDERER, for the same reason nNativeFrustum is: the
	// component that owns the texture is the component that should describe it.
	//
	// nEyeTexSerial is the whole synchronisation contract. It increments once
	// per present, so the host knows a frame is new by comparing a number
	// rather than by decoding swatches out of the image. 0 means nothing is
	// being published and the host should fall back to window capture.
	uint32_t	nEyeTexSerial;
	uint32_t	nEyeTexW;
	uint32_t	nEyeTexH;
	uint32_t	nEyeTexFormat;			// a DXGI_FORMAT value

	// The adapter the renderer's device is on. A shared texture only opens on
	// the SAME adapter, and on a machine with two GPUs the two processes can
	// pick different ones without either of them being wrong. Published so
	// that failure is a mismatch the host can name, rather than an empty
	// texture with no reason attached.
	int32_t		nAdapterLuidLo;
	int32_t		nAdapterLuidHi;

	// THE PAUSE MENU AS ITS OWN PICTURE (v15). Drawn into the eyes, the pause
	// menu was welded to the head - the world-lock shift in the eye buffer
	// depended on the client's camera turn, the renderer's shift and the
	// host's layer pose all agreeing, and they did not. The renderer now
	// draws the pause menu's 2D art into VRSHARED_OVLTEX_NAME instead of the
	// eyes, and the host presents it as a quad anchored in the world when it
	// first appears, over the stereo world - what the main menu already does.
	uint32_t	nPauseQuad;				// 1 while the overlay carries a pause menu
	uint32_t	nOvlSerial;				// increments per overlay publish
	uint32_t	nOvlW, nOvlH;

	// THE FRAME MARKER IN MEMORY (v16). The client paints the host frame's
	// low byte as swatches so the host knows which pose the picture was
	// rendered with. Reading those swatches back meant a GPU readback in the
	// host every frame, and that read waited behind the host's copy out of
	// the shared texture, which waited on the GPU for the game's release of
	// its mutex, which waited behind the host - 82-148 ms holds every two
	// to three seconds in a host log, every one in the "marker" step.
	// So the number rides here instead. nMarkerPainted: the client, when it
	// paints. nEyeTexMarker: the renderer, when it publishes the eye texture
	// that carries that paint, written before nEyeTexSerial. The swatches
	// stay painted; the pixel read is the fallback for an old block.
	uint32_t	nMarkerPainted;
	uint32_t	nEyeTexMarker;

	// HAPTICS (v17). The CLIENT asks, the HOST pulses the controller. The
	// client fills hand, amplitude and duration, then bumps the serial; the
	// host fires one xrApplyHapticFeedback per serial it has not seen. Headset
	// testing found the shots felt forceless and the gunplay a little cheap
	// without it - a shot you feel in the hand is the VR answer.
	uint32_t	nHapticSerial;
	uint32_t	nHapticHand;			// 0 left, 1 right
	float		fHapticAmp;				// 0..1
	float		fHapticMs;				// duration
	// RECENTER. The client bumps nRecenterReq (both stick clicks, or the
	// VRRecenter console command); the host folds the head's current yaw and
	// position into the reference space it declares poses in and bumps
	// nRecenterGen. The host also bumps nRecenterGen when the RUNTIME moves
	// its LOCAL space (the headset's own recenter gesture). The client
	// re-references head height and position on every change of
	// nRecenterGen - the half-metre jump rule alone misses a sit-down.
	uint32_t	nRecenterReq;
	uint32_t	nRecenterGen;
	// THE HOST'S PROCESS ID (v19), written once by the host when it creates the
	// block. The block cannot say the host is gone - the client's own handle
	// keeps the mapping alive after the host exits, and nHostAliveTick only
	// ticks once tracking starts. So the client waits on the process itself.
	// A host that vanished while waiting for a headset left the game playing
	// on, side by side on the monitor, with nothing left to close it.
	uint32_t	nHostPid;

	// GAME OR DIE HANDS (v20). Where each palm is and what the fingers touch -
	// what an articulated hand needs and the aim pose alone cannot give.
	// fAimToGrip: the controller's GRIP pose (the palm, as OpenXR defines it)
	// located in its own AIM space: position px py pz in metres, then the
	// rotation qx qy qz qw, OpenXR axes (+X right, +Y up, -Z forward). The
	// client already places the gun from the aim pose; the palm is that times
	// this. nGripValid 0 when the runtime gave no grip pose this frame.
	// nTouch: bit 0 finger on the trigger, bit 1 thumb down (stick, thumbrest
	// or a face button), bit 31 set when the controller HAS those sensors.
	// Indexed like Hands[], and swapped with them for the Leftorium.
	float		fAimToGrip[2][7];
	uint32_t	nGripValid[2];
	uint32_t	nTouch[2];
};

#define VRTOUCH_TRIGGER	(1u << 0)
#define VRTOUCH_THUMB	(1u << 1)
#define VRTOUCH_KNOWN	(1u << 31)

#pragma pack(pop)

// Both sides must agree. If this fires, the contract has drifted and the two
// processes would silently read each other's fields at the wrong offsets.
#ifdef __cplusplus
static_assert(sizeof(VRHandState) == 48, "VRHandState layout changed");
static_assert(sizeof(VRSharedState) == 452, "VRSharedState layout changed");
#endif


// --------------------------------------------------------------------------
// Client-side reader. Implemented in VRShared.cpp, which the host does not
// compile - the host writes the block itself.
// --------------------------------------------------------------------------
namespace VRShared
{
	// Attaches to the host's shared block if it exists. Safe to call every
	// frame; retries at a low rate so the host can be started at any time.
	bool	Poll();

	// Last consistent snapshot. Only meaningful when Poll() returned true.
	const VRSharedState& State();

	// True while the host has updated within the last second. Going false is
	// how the client falls back to a static camera rather than freezing on a
	// stale pose (a project rule).
	bool	IsLive();

	// True once the host PROCESS has exited (block v19). Checked at most
	// twice a second; false when there is no host id to watch, so a desk
	// harness that writes no id is never mistaken for a dead host.
	bool	HostGone();

	// THE LEFTORIUM (left-handed play). On, the two controllers swap as they
	// enter the game (Poll): the weapon hand is the physical LEFT controller.
	// Code that DRAWS mirrors by the physical hand: the gun held in the left
	// hand is a mirror image, and its right-side offsets change sign.
	void	SetSwapHands(bool bSwap);
	bool	SwapHands();
	// The Steam Frame's native profile is bound (VRBTN_FRAME on either hand).
	bool	FrameLayout();
	// The Frame's D-pad (always the physical left controller's), for menus.
	uint32_t FramePadButtons();
	// The Frame's raw buttons by PHYSICAL hand (0 left, 1 right), whatever
	// the Leftorium swapped. Zero on any other controller. VRBinds reads these.
	uint32_t FramePhysical(int nHand);
	// TWO HANDS ON A LONG GUN (VRPhysical): the gun hand's aim for the rest of
	// this frame, in the host's own degrees, so the drawn gun, the shot and
	// Cate's gun hand all take it. The next Poll puts the controller's back.
	void	OverrideGunAim(float fYawDeg, float fPitchDeg);
	// The sticks are separate (VRSwapSticks): off, the left physical stick
	// moves and the right turns, with or without the Leftorium.
	void	SetSwapSticks(bool bSwap);
	bool	SwapSticks();

	// Tells the host the game's true render surface size, which it cannot
	// discover from the window.
	void	PublishScreenSize(uint32_t nWidth, uint32_t nHeight);

	// The FOV actually set on the camera, full angles in radians.
	void	PublishFov(float fFovXRad, float fFovYRad);
	void	PublishMarker(uint32_t nHostFrame);	// the marker the client is painting this frame
	void	Haptic(int nHand, float fAmp, float fMs);	// ask the host for one pulse (v17)

	// How stale a pose the host should tag submitted frames with.
	void	PublishPoseLag(float fMs);

	// Whether the client is rendering about the per-eye optical centres, and
	// exactly what rotation it applied to each eye.
	void	PublishAsymActive(bool bActive);
	void	PublishAppliedCentre(int nEye, float fYawRad, float fPitchRad);

	// Whether the host should use the frame marker's exact pose.
	void	PublishExactPose(bool bOn);

	// Ask the host to measure the renderer's field from this frame.
	void	PublishCalib(bool bActive, float fYawRad);

	// Whether the head rotation is already baked into the rendered camera, so
	// the host should submit head-locked and let the runtime reproject nothing.
	void	PublishHeadLocked(bool bLocked);

	// The player's body yaw and whether the host should carry it into the
	// reference space it declares poses in.
	void	PublishBodyYaw(float fYawRad, int nMode);
	// Ask the host to recenter, and read how many times it has (either side).
	void		RequestRecenter();
	// The game is closing: the host ends the headset view (VRSHARED_F_QUITTING).
	void		SetQuitting();
	uint32_t	RecenterGeneration();

	// Whether the game is drawing a menu rather than the world.
	void	PublishInMenu(bool bInMenu);

	// Commands the headset's controllers are holding down.
	//
	// CGameClientShell::OnCommandOn is a NOTIFICATION the engine sends to the
	// shell; it does not set the engine's command state. Movement, firing,
	// jump, duck and run are all read the other way round, by CMoveMgr polling
	// g_pLTClient->IsCommandOn every frame - so calling OnCommandOn for them
	// fires the notification and moves nobody. Measured at the desk: the log
	// said 'first fire of command 0' and the camera had not moved a unit after
	// forty seconds of held stick.
	//
	// Command ids used here are all below 32, so one word holds them.
	void	SetCommandHeld(int nCmd, bool bHeld);
	bool	CommandOn(int nCmd);
	void	ClearCommands();

	// Written by the renderer, not by the client - see nNativeFrustum.
	void	PublishNativeFrustum(bool bOn);

	void	Close();
}

#endif // __VRSHARED_H__

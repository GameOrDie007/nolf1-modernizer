// ----------------------------------------------------------------------- //
//
// MODULE  : GameClientShell.cpp
//
// PURPOSE : Game Client Shell - Implementation
//
// CREATED : 9/18/97
//
// (c) 1997-2000 Monolith Productions, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include <map>			// the world-model attachment gate
#include "GameClientShell.h"
#include "VRLog.h"
#include "VRShared.h"
#include "VRPrims.h"
#include "VRWeaponVar.h"
// Defined in WeaponModel.cpp - drops the per-weapon variable cache.
extern void VRFlashOffsetsInvalidate();
#include "MsgIds.h"
#include "CommandIds.h"
#include "WeaponModel.h"
#include "ClientUtilities.h"
#include "vkdefs.h"
#include "ClientRes.h"
#include "SoundTypes.h"
#include "Music.h"
#include "VolumeBrushFX.h"
#include "client_physics.h"
#include "CameraFX.h"
#include "WinUtil.h"
#include "WeaponStringDefs.h"
#include "CMoveMgr.h"
#include "iltmath.h"
#include "iltphysics.h"
#include "VarTrack.h"
#include "GameButes.h"
#include "LTWnd.h"
#include "LTMaskedWnd.h"
#include "AssertMgr.h"
#include "SystemDependant.h"
#include "SurfaceFunctions.h"
#include "VehicleMgr.h"
#include "BodyFX.h"
#include "PlayerShared.h"
#include "CharacterFX.h"
#include <time.h>

#if _MSC_VER >= 1300
#include <iostream>
#include <fstream>
#else
#include <iostream.h>
#include <fstream.h>
#endif

#include "CRC32.h"

#include <stdarg.h>
#include <stdio.h>

#include <SDL.h>
#include "ConsoleMgr.h"
#include "DetourMgr.h"

extern ConsoleMgr* g_pConsoleMgr;

#ifdef STRICT
	WNDPROC g_pfnMainWndProc = NULL;
#else
	FARPROC	g_pfnMainWndProc = NULL;
#endif

#define min(a,b)	((a) < (b) ? (a) : (b))
#define max(a,b)	((a) > (b) ? (a) : (b))

#define FOVX_ZOOMED		20.0f
#define FOVX_ZOOMED1	7.0f
#define FOVX_ZOOMED2	2.0f
#define ZOOM_TIME		0.5f

#define DEFAULT_LOD_OFFSET				0.0f
#define LOD_ZOOMADJUST					-5.0f

#define MAX_SHAKE_AMOUNT				10.0f
#define MAX_FRAME_DELTA					0.1f

#define WEAPON_MOVE_INC_VALUE_SLOW		0.005f
#define WEAPON_MOVE_INC_VALUE_FAST		0.02f

#define DEFAULT_CSENDRATE				7.0f

#define VK_TOGGLE_EDITMODE				VK_F2
#define VK_TOGGLE_SCREENSHOTMODE		VK_F3

#define MAX_TIMEOUT_RETRIES 2

uint32              g_dwSpecial         = 2342349;
LTBOOL              g_bScreenShotMode   = LTFALSE;
HLTCOLOR            g_hColorTransparent = LTNULL;

HWND				g_hMainWnd = NULL;
RECT*				g_prcClip = NULL;

CGameClientShell*   g_pGameClientShell = LTNULL;

LTVector            g_vWorldWindVel(0.0f, 0.0f, 0.0f);
LTVector            g_vNVModelColor(0.0f, 0.0f, 0.0f);      // model color modifier for night vision
LTVector            g_vIRModelColor(0.0f, 0.0f, 0.0f);      // model color modifier for infrared

PhysicsState		g_normalPhysicsState;
PhysicsState		g_waterPhysicsState;

VarTrack			g_vtFOVXNormal;
VarTrack			g_vtFOVYNormal;
VarTrack			g_vtInterfceFOVX;
VarTrack			g_vtInterfceFOVY;
VarTrack			g_CV_CSendRate;		// The SendRate console variable.
VarTrack			g_vtPlayerRotate;	// The PlayerRotate console variable
VarTrack			g_vtShowTimingTrack;
VarTrack			g_vtNormalTurnRate;
VarTrack			g_vtFastTurnRate;
VarTrack			g_vtLookUpRate;
VarTrack			g_vtCameraSwayXFreq;
VarTrack			g_vtCameraSwayYFreq;
VarTrack			g_vtCameraSwayXSpeed;
VarTrack			g_vtCameraSwayYSpeed;
VarTrack			g_vtCameraSwayDuckMult;
VarTrack			g_vtChaseCamPitchAdjust;
VarTrack			g_vtChaseCamOffset;
VarTrack			g_vtChaseCamDistUp;
VarTrack			g_vtChaseCamDistBack;
VarTrack			g_vtActivateOverride;
VarTrack			g_vtCamDamage;
VarTrack			g_vtCamDamagePitch;
VarTrack			g_vtCamDamageRoll;
VarTrack			g_vtCamDamageTime1;
VarTrack			g_vtCamDamageTime2;
VarTrack			g_vtCamDamageVal;
VarTrack			g_vtCamDamagePitchMin;
VarTrack			g_vtCamDamageRollMin;
VarTrack			g_varStartLevelScreenFadeTime;
VarTrack			g_varStartLevelScreenFade;
VarTrack			g_vtUseCamRecoil;
VarTrack			g_vtCamRecoilRecover;
VarTrack			g_vtBaseCamRecoilPitch;
VarTrack			g_vtMaxCamRecoilPitch;
VarTrack			g_vtBaseCamRecoilYaw;
VarTrack			g_vtMaxCamRecoilYaw;
VarTrack			g_vtFireJitterDecayTime;
VarTrack			g_vtFireJitterMaxPitchDelta;
VarTrack            g_vtCamRotInterpTime;
VarTrack			g_vtRespawnWaitTime;
VarTrack			g_vtMultiplayerRespawnWaitTime;
VarTrack			g_vtUseSoundFilters;
VarTrack			g_vtScreenFadeInTime;
VarTrack			g_vtScreenFadeOutTime;
VarTrack			g_vtSpecial;
VarTrack			g_vtModelGlowTime;
VarTrack			g_vtModelGlowMin;
VarTrack			g_vtModelGlowMax;

VarTrack			g_vtSunZoomLevel1MaxDist;
VarTrack			g_vtSunZoomLevel2MaxDist;

VarTrack			g_vtFOVYMaxUW;
VarTrack			g_vtFOVYMinUW;
VarTrack			g_vtUWFOVRate;
VarTrack			g_vtPlayerName;

// New!
VarTrack			g_vtLockFPS;					// FramerateLock			<0-1>
VarTrack			g_vtShowFPS;					// ShowFramerate			<0-1>
VarTrack			g_vtOldMouseLook;				// OldMouseLook				<0-1>
VarTrack			g_vtNoFunMenus;					// NoFunMenus				<0-1>
VarTrack			g_vtLockCinematicAspectRatio;   // RestrictCinematicsTo4x3	<0-1>
VarTrack			g_vtQuickSwitch;				// QuickSwitch				<0-1>
VarTrack			g_vtUIScale;					// UIScale					<0.0-1.0>
VarTrack			g_vtUseGOTYMenu;				// UseGotyMenu				<0-1>
VarTrack			g_vtNoRawInput;					// NoRawInput				<0-1>
VarTrack			g_vtConsoleBackdrop;			// ConsoleBackdrop			<0-2>
VarTrack			g_vtBigHeadMode;				// BigHeadMode				<0-1>
VarTrack			g_vtModPatchNum;				// ModPatchNum
VarTrack			g_vtEnableScreenTint;			// EnableScreenTinting		<0-1>
VarTrack			g_vtEnableLightScale;			// EnableLightScale			<0-1>
static LTBOOL		g_bLogVRGeometry = LTFALSE;		// dump camera geometry for one frame

VarTrack			g_vtVRStereo;					// VRStereo					<0-2>
VarTrack			g_vtVRIPD;						// VRIPD					world units
VarTrack			g_vtVRSwapEyes;					// VRSwapEyes				<0-1>
VarTrack			g_vtVRFovAdjust;				// VRFovAdjust				<0-1>
VarTrack			g_vtVRSceneMode;				// VRSceneMode				<0-1>
VarTrack			g_vtVROffsetFovScale;			// VROffsetFovScale			multiplier
VarTrack			g_vtVRDirectBlit;				// VRDirectBlit				<0-1>
VarTrack			g_vtVRHalfSbs;					// VRHalfSbs				<0-1>
VarTrack			g_vtVRHideHud;					// VRHideHud				<0-1>
VarTrack			g_vtVRHeadTracking;				// VRHeadTracking			<0-1>
VarTrack			g_vtVRYawScale;					// VRYawScale				+/-1
VarTrack			g_vtVRPitchScale;				// VRPitchScale				+/-1
VarTrack			g_vtVRRollScale;				// VRRollScale				+/-1
VarTrack			g_vtVRUseHeadsetFov;			// VRUseHeadsetFov			<0-1>
VarTrack			g_vtVRFovYTest;					// VRFovYTest				multiplier
VarTrack			g_vtVRFovYScale;				// VRFovYScale				TANGENT multiplier
VarTrack			g_vtVRFovXTest;					// VRFovXTest				multiplier
VarTrack			g_vtVRQuitAfter;				// VRQuitAfter				seconds, 0 = never
VarTrack			g_vtVRQuitAfterSweep;			// VRQuitAfterSweep			<0-1>
VarTrack			g_vtVRBorderless;				// VRBorderless				<0-1>
VarTrack			g_vtVRFovMargin;				// VRFovMargin				multiplier
VarTrack			g_vtVRRenProbe;					// VRRenProbe				<0-1>
VarTrack			g_vtVRFramerate;				// VRFramerate				target fps
VarTrack			g_vtVRWindowMonitor;			// VRWindowMonitor			<0-1>
VarTrack			g_vtVRQuatHead;					// VRQuatHead				<0-1>
VarTrack			g_vtVRPoseLag;					// VRPoseLag				milliseconds
VarTrack			g_vtVRAsymFrustum;				// VRAsymFrustum			<0-2>
VarTrack			g_vtVRFrameMarker;				// VRFrameMarker			<0-1>
VarTrack			g_vtVRExactPose;				// VRExactPose				<0-1>
VarTrack			g_vtVRCrosshair;				// VRCrosshair				<0-1>
VarTrack			g_vtVRAimMarker;				// VRAimMarker				<0-1>
VarTrack			g_vtVRAimMarkerSize;			// VRAimMarkerSize
VarTrack			g_vtVRAimMarkerStyle;			// VRAimMarkerStyle		<0-2>
VarTrack			g_vtVRAimMarkerWorld;			// VRAimMarkerWorld		<0-1>
VarTrack			g_vtVRAimMarkerWorldSize;		// VRAimMarkerWorldSize	units
VarTrack			g_vtVRAimMarkerMinSize;			// VRAimMarkerMinSize		angular floor
VarTrack			g_vtVRHideCrosshair;			// VRHideCrosshair			<0-1>
VarTrack			g_vtVRHandAim;
VarTrack g_vtVRHandPosScale;
VarTrack g_vtVRHandFire;
VarTrack g_vtVRGunAtHand;
VarTrack g_vtVRArmIK;
VarTrack g_vtVRBodyBack;
VarTrack g_vtVRHandRoll;
VarTrack g_vtVRHaptics;
VarTrack g_vtVRWheelSize;
VarTrack g_vtVRSpriteRange;
VarTrack g_vtVRViewModel;
VarTrack g_vtVRViewModelScale;
VarTrack g_vtVRVehicleDrop;
VarTrack g_vtVRVehicleBody;			// VRVehicleBody 1: draw the whole vehicle under the rider
VarTrack g_vtVRVehicleBodyDown;		// its origin this far below the eye (world units)
VarTrack g_vtVRVehicleBodyFwd;		// and this far ahead of it
VarTrack g_vtVRVehicleBodyDownSnow;
VarTrack g_vtVRVehicleBodyFwdSnow;
VarTrack g_vtVRVehicleBack;			// VRVehicleBack <units>: pull the ridden handlebars toward the rider			// VRVehicleDrop <units>: lower the ridden handlebars
VarTrack g_vtVRVehicleStickPedals;	// VRVehicleStickPedals 1: left stick forward/back also drives a vehicle
VarTrack g_vtVRViewModelSize;					// VRHandAim				<0-3>
// SUPERSEDED, AND KEPT ONLY BECAUSE THE NAMES ARE STILL THE RIGHT ONES.
//
// These five VarTracks are registered in OnEngineInitialized and then read by
// NOTHING - audited 11 September, zero GetFloat calls between them. The live
// readers are the VRWeaponVar instances in WeaponModel.cpp, which resolve the
// SAME console variable names plus an optional "@<weapon>" override, so
// setting VRWeaponDist still does what it says; it is just not these objects
// that do it.
//
// Harmless rather than merely untidy: VarTrack::Init only writes a value when
// the variable does not already exist (VarTrack.h:36), so registering a name
// twice cannot clobber a value from the command line or from autoexec.cfg, and
// both registrations use the same defaults.
//
// Left in place because deleting them would also mean deleting the externs in
// WeaponModel.cpp for no behavioural gain. Do not write new code against them.
VarTrack			g_vtVRWeaponDist;				// VRWeaponDist				multiplier
VarTrack			g_vtVRWeaponScale;				// VRWeaponScale			multiplier
VarTrack			g_vtVRHeadAsMouse;				// VRHeadAsMouse			<0-1>
VarTrack			g_vtVRProbeField;				// VRProbeField				<0-1>
VarTrack			g_vtVRSweepField;				// VRSweepField				<0-1>
VarTrack			g_vtVRAutoQuickLoad;			// VRAutoQuickLoad			<0-1>
// PUT THE CAMERA ANYWHERE, so the desk can verify things that are not at a
// spawn point. Every capture this project has ever taken was from wherever
// +runworld drops the player or wherever the quick save happens to be, which
// is why the door handle in M06S01 - at (-994 -128 854), four thousand units
// from that level's spawn - could only ever be checked in a headset.
//   +VRTele 1 +VRTeleX -994 +VRTeleY -128 +VRTeleZ 854
// LOOK AROUND WHILE PAUSED. Item 6 on a list, and NOT what item 5
// delivered: the pause backdrop shows the right PLACE but is a frozen
// snapshot, because the client's eye loop does not run in a folder state.
// This turns the interface camera with the head so the picture follows the player.
VarTrack			g_vtVRPauseLook;				// VRPauseLook				<0-1>
VarTrack			g_vtVRTele;						// VRTele					<0-1>
VarTrack			g_vtVRTeleX;
VarTrack			g_vtVRTeleY;
VarTrack			g_vtVRTeleZ;
VarTrack			g_vtVRTeleAt;					// seconds after the world is up
VarTrack			g_vtVRCheats;					// VRCheats <bits>: 1 god, 2 everything, 4 all missions
VarTrack			g_vtVRTour;						// VRTour 1: walk game/vrtour.txt on a timer
VarTrack			g_vtVRTourEvery;				// seconds per stop
VarTrack			g_vtVRHeadListener;				// VRHeadListener			<0-1>

// ===========================================================================
// VR DEBUG TOOLS - TO BE REMOVED BEFORE RELEASE
//
// Everything for the debug level-skip is gathered under this one switch so
// that taking it out is deleting the marked blocks and this define, rather
// than hunting for fragments. There are exactly four:
//   1. this block
//   2. the cvar's Init, marked VR_DEBUG_TOOLS
//   3. the chord in VRUpdateControllerInput, marked VR_DEBUG_TOOLS
//   4. CCheatMgr::VRExitLevel in MessageMgr.h, and the toggle in FolderVR
//
// It is OFF by default and only reachable from Options > VR, so a player who
// never turns it on cannot meet it by accident.
// ===========================================================================
#define VR_DEBUG_TOOLS 0

// For files that cannot see the macro: FolderVR shows its Debug rows only
// in a build that can act on them.
bool VRDebugToolsBuilt() { return VR_DEBUG_TOOLS != 0; }
extern bool g_bVRPhotoMode;		// InterfaceMgr.cpp: the pause menu hidden for a screenshot

#if VR_DEBUG_TOOLS
VarTrack			g_vtVRDebugSkip;				// VRDebugSkip <0-1>
VarTrack			g_vtVRDebugGod;					// VRDebugGod <0-1>
VarTrack			g_vtVRDebugClip;				// VRDebugClip <0-1>
VarTrack			g_vtVRDebugNoAI;				// VRDebugNoAI <0-1>
VarTrack			g_vtVRDebugMissions;			// VRDebugMissions <0-1>
VarTrack			g_vtVRDebugArsenal;				// VRDebugArsenal <0-1>
VarTrack			g_vtVRDebugPos;					// VRDebugPos <0-1>
VarTrack			g_vtVRDebugWeapon;				// VRDebugWeapon <n>: draw the nth gun
VarTrack			g_vtVRDebugEndCinematic;		// VRDebugEndCinematic <secs>
VarTrack			g_vtVRDebugFire;				// VRDebugFire <secs>: pull the trigger at the desk
VarTrack			g_vtVRDebugFailAt;				// VRDebugFailAt <secs>: fail the mission at the desk
VarTrack			g_vtVRDebugCmdAt;				// VRDebugCmdAt <secs>: run VRDebugCmd once, that far into a world
#endif
// OUTSIDE THE GUARD: code that runs in every build reads these (the gun tuner,
// the position log, the timed quick save), so a VR_DEBUG_TOOLS 0 build did not
// compile. Each is off by default; the debug build always behaved this way.
// THE MUZZLE FLASH TUNER. Numpad nudges the flash, like the Prey port.
VarTrack			g_vtVRMuzzleScale;				// 0 = use the weapon's own scale
VarTrack			g_vtVRFlashTune;
VarTrack			g_vtVRFlashOffR;
VarTrack			g_vtVRFlashOffU;
VarTrack			g_vtVRFlashOffF;
static double		s_fVRTuneSavedAt = -100.0;	// when the tuner last wrote autoexec.cfg
VarTrack			g_vtVRFlashHold;
VarTrack			g_vtVRLogPos;					// VRLogPos <0-1>  player position to the log once a second
VarTrack			g_vtVRDebugQuickSave;			// VRDebugQuickSave <secs>: save once
// NOT inside the fence above, and deliberately: its four use sites are in
// ordinary render code, so fencing the cvar and not them breaks the build
// the moment VR_DEBUG_TOOLS goes to 0 for a release - which is the one
// build nobody compiles until the day it matters.
VarTrack			g_vtVRLogWeaponPos;				// VRLogWeaponPos 1: the per-frame view-weapon placement trace
VarTrack			g_vtVRAutoReloadSecs;			// VRAutoReloadSecs			seconds
VarTrack			g_vtVRWorld2At;					// VRWorld2At				seconds
VarTrack			g_vtVRSoundReset;				// VRSoundReset <0-1>
VarTrack			g_vtVRCloseRebase;				// VRCloseRebase <0-1>: camera-relative models onto the gun
// How often the muzzle flash was switched on - see MuzzleFlashFX.cpp. A
// 7.5 ms effect cannot be sampled; it can only be counted.
static void VRAlignTrimSet(int nWeaponId, float fPitch, float fYaw);	// the table sits by VRDebugSelectWeapon
namespace VRTuneSave { static void Remember(const char* pszName, float f); static bool Write(); }
extern unsigned long VRMuzzleFlashShownCount();
VarTrack			g_vtVRWorld2Exit;				// VRWorld2Exit <0-1>: leave by the level's own exit
VarTrack			g_vtVRWorld2Count;				// VRWorld2Count: how many times, re-armed per world
static double		g_fW2Since = -1.0;				// wall clock at world entry
// HOW MANY WORLDS THIS PROCESS HAS ENTERED. Bumped in OnEnterWorld, read by
// anything that has to do something once per world. Sampling IsInWorld()
// from Update() does NOT see a level change: Update returns early for the
// whole of the load, so the flag reads true before and true after, and the
// first chained run re-armed nothing on its second world.
static int			g_nVRWorldEntries = 0;
#if VR_DEBUG_TOOLS
// VRDebugEndCinematic has decided this world's opening cinematic is over.
//
// It cannot be done by clearing USRFLG_CAMERA_LIVE alone: that flag is the
// SERVER's, replicated down every update, so a client-side clear is undone
// within a frame - measured 11 September, the camera was back 0.27 s later
// and stayed back for the remaining 21 s of the run. What DOES hold is
// refusing to honour a live camera at all, which is what a player gets when
// the server finally clears it. Reset on every world entry.
static bool			g_bVRCineEnded = false;
#endif
static int			g_nW2Fired = 0;
static double		g_fW2FolderSince = -1.0;			// wall clock at the mission summary
static int			g_nW2FolderFiredFor = -1;
VarTrack			g_vtVRAttachMeasure;			// VRAttachMeasure			<0-1>
VarTrack			g_vtVRWorldAttach;				// VRWorldAttach			<0-1>
VarTrack			g_vtVRControls;					// VRControls				<0-1>
VarTrack			g_vtVRStickDeadzone;			// VRStickDeadzone			0..1
VarTrack			g_vtVRSnapTurn;					// VRSnapTurn				<0-1>
VarTrack			g_vtVRSnapTurnDeg;				// VRSnapTurnDeg			degrees
VarTrack			g_vtVRMenuRepeatMs;				// VRMenuRepeatMs			ms
VarTrack			g_vtVRModelDump;				// VRModelDump				<0-1>
VarTrack			g_vtVRModels;					// VRModels					<0-1>
VarTrack			g_vtVRModelRange;				// VRModelRange				units
VarTrack			g_vtVRModelRangeAnim;			// VRModelRangeAnimated		units, for rigs of 13+ nodes
VarTrack			g_vtVRNodeReuse;				// VRNodeReuse				1: an unchanged prop republishes last frame's nodes (default 0)
// THE PUBLISH ASKED THE ENGINE FOR EVERY NODE OF EVERY MODEL, EVERY FRAME:
// GetNodeTransform in world space, per node - 3.2 ms a frame in M01S02's
// market with nothing moving, and more in a fight. A prop whose position,
// rotation, scale and main-tracker state (animation, time, playing) are all
// what they were last frame has the same node transforms, so they are copied
// from last frame instead. Never for characters or bodies (extra trackers,
// node controllers: head look, lip sync), the view weapon, view mods, the
// ridden vehicle or the player - each of those has its own path below.
struct VRNodeCacheEnt
{
	float    fPos[3], fRot[4], fScale[3];
	uint32   nAnim, nTime;
	int      nPlaying;
	std::vector<VRModelNode> nodes;
};
static std::map<HOBJECT, VRNodeCacheEnt> s_VRNodeCache;
static long s_nVRNodeReused = 0, s_nVRNodeQueried = 0;
static bool		s_bVRIfaceCamTurned = false;	// the pause-look has turned the interface camera
static uint32		s_nVRFarAnimSkipped = 0, s_nVRFarSceneryKept = 0;	// past the animated range, per report period

// The interface's own object list - defined in InterfaceMgr.cpp. A sphere
// query cannot find these: at the menu there is no world loaded, so there is
// nothing in the spatial partition it searches.
extern HLOCALOBJ	g_hVRIfaceObjs[64];
extern int			g_nVRIfaceObjs;
VarTrack			g_vtVRModelCap;					// VRModelCap				instances
VarTrack			g_vtVRPersistentFX;				// VRPersistentFX			<0-1>
VarTrack			g_vtVRMenuModels;				// VRMenuModels			<0-1>
VarTrack			g_vtVRMenuHDFont;				// VRMenuHDFont			<0-1>
// SUB-FOLDERS ARE AUTHORED SMALL AND HAVE NO HD ART. Item 7: the main menu
// font and size look right in the headset but the sub menus are all smaller. They are -
// most of them set FontSize 0 or 1 in LayoutNew.txt, which selects the small
// or medium sheet, and only the LARGE sheet has an HD replacement. At 640x480
// that was legible; at 2560x1384 in a headset it is not.
VarTrack			g_vtVRMenuBigSubs;				// VRMenuBigSubs		<0-1>
VarTrack			g_vtVRGunCentre;
VarTrack			g_vtVRRecoil;
VarTrack			g_vtVRGunTrimPitch;				// VRGunTrimPitch	degrees; + tilts the muzzle down					// VRRecoil			<0-1> camera kick on firing (0 in VR)
VarTrack			g_vtVRGunAnchor;
VarTrack			g_vtVRRootIdleFrames;			// frames of W_IDLE before the gun's rest pose is taken
VarTrack			g_vtVRRootCaptureSkew;			// desk: spoil the first rest-pose capture by N degrees
VarTrack			g_vtVRRootRecaptureDeg;			// a rest pose this far off the idle one is taken again
VarTrack			g_vtVRGunRigid;
VarTrack			g_vtVRGunAutoTrim;				// VRGunAutoTrim	<0-1> each weapon's barrel aligned to the hand from its Flash socket
VarTrack			g_vtVRPauseFreeze;
VarTrack			g_vtVREyeClamp;
VarTrack			g_vtVRCaptions;					// VRCaptions			<0-1> subtitles and captions as a floating block below centre				// VRPauseFreeze		<0-1> models frozen behind the pause menu					// VRGunRigid		<0-1> the gun rigid on the hand, animation kept inside it				// VRGunAnchor		<0-2> 0 centroid, 1 named node, 2 named node else centroid
static LTVector		s_vGunSum;		// GUN DIR: node accumulators, view weapon only
static int			s_nGunNodes = 0;
static float		s_fGunFar = -1.0f;
static LTVector		s_vGunFar;
static long			s_nLastGunReportFrame = -1000;
static LTVector		s_vBarrelA, s_vBarrelB;
// The node the muzzle effects hang off, learned once per weapon. See the note
// at the node walk: re-choosing it every frame makes the flash jump between
// two nodes as the gun animates.
static char			s_szMuzzleNode[64] = "";
static int			s_nMuzzleNodeWeapon = -1;
static LTMatrix		s_mRootRef;			// the gun root's rest orientation in the object's frame
// THE GUN'S PLACEMENT, STASHED FOR THE MODS ON IT. The silencer, scope
// and laser are separate camera-relative objects the engine keeps at the
// gun's sockets, so their engine transforms live in the same camera-
// relative frame as the gun's nodes and take the same rigid mapping:
// root^-1 into the root's frame, out again with the placed rotation, then
// out of the near plane by K. Filled where the gun's nodes are placed; a
// mod that comes through the loop before the gun uses last frame's.
static struct VRModPlace
{
	bool     bValid;
	LTMatrix mPlace, mRootT;
	LTVector vGunRootPos, vOrig, vGripW;
	float    K, S;
} s_ModPlace = { false };
static inline LTVector VRMatMul(const LTMatrix& m, const LTVector& v)
{
	return LTVector(m.m[0][0]*v.x + m.m[0][1]*v.y + m.m[0][2]*v.z,
					m.m[1][0]*v.x + m.m[1][1]*v.y + m.m[1][2]*v.z,
					m.m[2][0]*v.x + m.m[2][1]*v.y + m.m[2][2]*v.z);
}

// A SCOPE THAT IS PART OF THE GUN. Contender_pv.abc and Dragunov_pv.abc have
// no sockets at all (read from the files), so CWeaponModel::CreateScope never
// makes a scope object for them: their scope is a tube in the gun's own mesh
// and the flat game only ever zoomed the camera. Headset, 21 September: every
// other scope showed the magnified picture, those two did not. The tube is
// found in each mesh by fitting a cylinder to the vertices of one node, in
// that node's own space (which is pose-independent - the rest pose in the
// file is the lowered gun, so model space is useless). At draw time the lens
// is placed from that node's published transform, exactly as the renderer
// places the node's vertices: node position + rotation * (local * K*S*scale).
// Centre, axis, the two ends along the axis and the radius, in model units.
// An empty node name means FROM THE ROOT: the engine never poses the
// Contender's tube node (revol2_1_1 is absent from its node walk, headset
// 21 September - no lens at all), so its lens is the tuner's LENS offset from
// the gun's first node along the aim, with the mesh's length and radius. The
// Dragunov's tube node is posed, so the mesh places it and LENS corrects it.
struct VRIntScope { const char* szWeapon; const char* szNode; float c[3]; float a[3]; float fT0, fT1, fR; };
// The radius the mesh gave the lens last frame, world units: the tuner's LENS
// SIZE mode starts from it, the way SCALE starts from the size in use.
static float s_fVRLensMeshRadius = 0.85f;
static const VRIntScope s_IntScopes[] = {
	{ "contender", "",       { 0.0f, 0.0f, 0.0f },              { 0.0f, 0.0f, 1.0f },     0.00f, 1.09f, 0.079f },
	{ "dragunov",  "cyl8_5", { -0.022f,  0.089f, -0.010f },     { 0.02f, 0.00f, -1.00f }, -0.83f, 0.67f, 0.077f },
};
static const VRIntScope* VRIntScopeFor(int nWeaponId)
{
	char szSlug[64] = ""; VRWeaponSlugForId(nWeaponId, szSlug, sizeof(szSlug));
	for (size_t k = 0; k < sizeof(s_IntScopes) / sizeof(s_IntScopes[0]); ++k)
		if (stricmp(szSlug, s_IntScopes[k].szWeapon) == 0) return &s_IntScopes[k];
	return LTNULL;
}
static HLOCALOBJ	s_hRootRefObj = LTNULL;
static bool			s_bRootRefValid = false;
// AND THE WEAPON IT WAS CAPTURED FOR. The weapon model keeps ONE engine
// object and swaps the model file inside it, so the handle is the same for
// the Walther and the S&W - and a reference keyed on the handle alone stayed
// valid across the swap. The S&W was drawn with the WALTHER's root rest
// orientation. In headset testing in Morocco the revolver came up pointing
// at the player but shot the way the player was facing - the log's far-node yaw read
// +179. Keyed on the weapon id as well, the rest pose is recaptured from each
// weapon's own idle. (See the heap-address-is-not-identity note: same trap,
// one level up - an engine object is not a model either.)
static int			s_nRootRefWeapon = -1;
static int			s_nBarrelHave = 0;				// VRGunCentre			<0-1> the gun's body on the hand, not its origin
VarTrack			g_vtVRIPDAuto;					// VRIPDAuto				<0-1>
VarTrack			g_vtVRUnitMM;					// VRUnitMM					mm per world unit
VarTrack			g_vtVRHeadPos;					// VRHeadPos				<0-1>

// The head position the current level was entered at, in metres, OpenXR LOCAL
// space. Head translation is applied RELATIVE to this: the host publishes a
// room-space position, and using it raw would displace the camera by wherever
// the player happens to be standing.
static LTBOOL	s_bHeadRefValid = LTFALSE;
static float	s_fHeadRefX = 0.0f, s_fHeadRefY = 0.0f, s_fHeadRefZ = 0.0f;
// Where the head was LAST frame. The re-reference test needs a per-frame
// delta, not a distance from the reference: a player who walks half a metre
// from where they spawned has not teleported, and must not be re-centred.
static float	s_fHeadPrevX = 0.0f, s_fHeadPrevY = 0.0f, s_fHeadPrevZ = 0.0f;
VarTrack			g_vtVRProbeReadback;			// VRProbeReadback			frames to sample
VarTrack			g_vtVRFovUniform;				// VRFovUniform				<0-1>
VarTrack			g_vtVRFieldRun;					// VRFieldRun				frames to capture
VarTrack			g_vtVRYawSpace;					// VRYawSpace				<0-2>
VarTrack			g_vtVRFakeHeadPitch;			// VRFakeHeadPitch			degrees
VarTrack			g_vtVRHeadLocked;				// VRHeadLocked				<0-1>

// 5 degrees: large enough to shift the image well beyond correlation noise,
// small enough that the two halves still overlap for most of their width.
const float CGameClientShell::kCalibYawRad = 5.0f * 0.01745329f;

// --- choosing which display the game window lives on ---------------------- //
//
// A windowed game is composited by the desktop, so it can never present faster
// than the display it sits on refreshes. On a 60 Hz panel that caps the entire
// VR chain at 60 fps no matter what VRFramerate asks for - the headset then
// repeats frames and reprojection has to invent the difference, which is what
// smears when you turn.
//
// Measured on the player's machine: the only physical monitor tops out at 60 Hz
// at every resolution it offers, while two Virtual Desktop virtual displays sit
// at 120 Hz. Moving the window onto one of those lifts the ceiling, and has the
// side benefit of taking the side-by-side image off the physical desktop.

struct VRMonPick
{
	int		nWantH;			// window must fit vertically
	DWORD	dwBestHz;		// start at the primary's rate - only beat it
	int		nX, nY, nW, nH;
	bool	bFound;
	char	szName[32];
};

static BOOL CALLBACK VRMonProc(HMONITOR hMon, HDC, LPRECT, LPARAM lp)
{
	VRMonPick* p = (VRMonPick*)lp;

	MONITORINFOEXA mi;
	memset(&mi, 0, sizeof(mi));
	mi.cbSize = sizeof(mi);
	if (!GetMonitorInfoA(hMon, &mi))			return TRUE;
	if (mi.dwFlags & MONITORINFOF_PRIMARY)		return TRUE;

	DEVMODEA dm;
	memset(&dm, 0, sizeof(dm));
	dm.dmSize = sizeof(dm);
	if (!EnumDisplaySettingsA(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm)) return TRUE;

	const int nH = mi.rcMonitor.bottom - mi.rcMonitor.top;
	if (nH < p->nWantH)								return TRUE;	// would not fit
	if (dm.dmDisplayFrequency <= p->dwBestHz + 5)	return TRUE;	// not meaningfully faster

	p->dwBestHz = dm.dmDisplayFrequency;
	p->nX = mi.rcMonitor.left;
	p->nY = mi.rcMonitor.top;
	p->nW = mi.rcMonitor.right - mi.rcMonitor.left;
	p->nH = nH;
	p->bFound = true;
	strncpy(p->szName, mi.szDevice, sizeof(p->szName) - 1);
	p->szName[sizeof(p->szName) - 1] = '\0';
	return TRUE;
}

LTFLOAT             s_fDemoTime     = 0.0f;
LTFLOAT             s_fDeadTimer    = 0.0f;
LTFLOAT             s_fDeathDelay   = 0.0f;

LTVector            g_vPlayerCameraOffset = g_kvPlayerCameraOffset;

// SDL Logging
STD fstream 			g_SDLLogFile;
SDL_Window* 		g_SDLWindow = NULL;
bool 				g_CursorCenterHack = false;

int					g_nCinSaveModelShadows = 0;

static uint8		s_nLastCamType = CT_FULLSCREEN;

extern CCheatMgr*	g_pCheatMgr;

void IRModelHook(ModelHookData *pData, void *pUser);
void NVModelHook(ModelHookData *pData, void *pUser);
void DefaultModelHook(ModelHookData *pData, void *pUser);
BOOL HookWindow();
void UnhookWindow();
void QuickLoadCallBack(LTBOOL bReturn, void *pData);

BOOL OnSetCursor(HWND hwnd, HWND hwndCursor, UINT codeHitTest, UINT msg);
LRESULT CALLBACK HookedWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

extern void CalcNonClipPos(LTVector & vPos, LTRotation & rRot);

LTRESULT(*g_pRegisterConsoleProgram)(char* pName, ConsoleProgramFn fn) = NULL;
LTRESULT(*g_pUnregisterConsoleProgram)(char* pName);

// We can build a list of registered console programs here :)!
LTRESULT proxyRegisterConsoleProgram(char* pName, ConsoleProgramFn fn)
{
	LTRESULT result = g_pRegisterConsoleProgram(pName, fn);

	if (result == LT_OK) {
		g_pConsoleMgr->AddToHelp(pName);
	}

	return result;
}

LTRESULT proxyUnregisterConsoleProgram(char* pName)
{
	LTRESULT result = g_pUnregisterConsoleProgram(pName);

	if (result == LT_OK) {
		g_pConsoleMgr->RemoveFromHelp(pName);
	}

	return result;
}

void SDLLog(void* userdata, int category, SDL_LogPriority priority, const char* message)
{
	// Open up SDL Log File
	g_SDLLogFile.open("Debug.log", STD ios::out | STD ios::app);

	g_SDLLogFile << message << "\n";

	g_SDLLogFile.close();
} 


// Setup..
SETUP_CLIENTSHELL();


IClientShell* CreateClientShell(ILTClient *pClientDE)
{
	// VR: claim DPI awareness before the engine creates its window.
	//
	// The HIGHDPIAWARE compatibility shim on lithtech.exe does not hold
	// reliably - measured window clients of both 1936x1096 (unscaled) and
	// 2895x1635 (scaled 1.507x) for the same 1920x1080 render, apparently
	// depending on the display configuration at launch. When it scales, Windows
	// stretches a 1080p frame to 2895 wide, we capture the blur, and the
	// headset upscales that again.
	//
	// This is the earliest code in the process we control. Done dynamically
	// because the entry point only exists on Windows 10 1703 and later, and
	// failing to set it must never stop the game from running.
	{
		typedef BOOL (WINAPI *PFN_SetCtx)(HANDLE);
		if (HMODULE hUser = GetModuleHandleA("user32.dll"))
		{
			PFN_SetCtx pfn = (PFN_SetCtx)GetProcAddress(hUser, "SetProcessDpiAwarenessContext");
			// -4 is DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
			if (pfn) pfn((HANDLE)-4);
		}
	}

	// Hook up the AssertMgr

	CAssertMgr::Enable();

	// Get our ClientDE pointer

    g_pLTClient  = pClientDE;
    _ASSERT(g_pLTClient);

	// Register our proxy functions, these will allow us to populate a list of console commands
	g_pRegisterConsoleProgram = g_pLTClient->RegisterConsoleProgram;
	g_pLTClient->RegisterConsoleProgram = proxyRegisterConsoleProgram;

	g_pUnregisterConsoleProgram = g_pLTClient->UnregisterConsoleProgram;
	g_pLTClient->UnregisterConsoleProgram = proxyUnregisterConsoleProgram;

	CGameClientShell* pShell = debug_new(CGameClientShell);
	_ASSERT(pShell);

	// Init our LT subsystems

    g_pMathLT = g_pLTClient->GetMathLT();
    g_pModelLT = g_pLTClient->GetModelLT();
    g_pTransLT = g_pLTClient->GetTransformLT();
    g_pPhysicsLT = g_pLTClient->Physics();
	g_pBaseLT = static_cast<ILTCSBase*>(g_pLTClient);

	g_pPhysicsLT->SetStairHeight(DEFAULT_STAIRSTEP_HEIGHT);

    return ((IClientShell*)pShell);
}

void DeleteClientShell(IClientShell *pInputShell)
{
	// Delete our client shell

	if (pInputShell)
	{
		debug_delete(((CGameClientShell*)pInputShell));
	}

	// Unhook the AssertMgr and let the CRT handle asserts once again

	CAssertMgr::Disable();
}

static LTBOOL LoadLeakFile(ILTClient *g_pLTClient, char *pFilename);


void LeakFileFn(int argc, char **argv)
{
	if (argc < 1)
	{
        g_pLTClient->CPrint("LeakFile <filename>");
		return;
	}

    if (LoadLeakFile(g_pLTClient, argv[0]))
	{
        g_pLTClient->CPrint("Leak file %s loaded successfully!", argv[0]);
	}
	else
	{
        g_pLTClient->CPrint("Unable to load leak file %s", argv[0]);
	}
}

LTBOOL ConnectToTcpIpAddress(ILTClient* pClientDE, char* sAddress);

void ConnectFn(int argc, char **argv)
{
	if (argc <= 0)
	{
        g_pLTClient->CPrint("Connect <tcpip address> (use '*' for local net)");
		return;
	}

    ConnectToTcpIpAddress(g_pLTClient, argv[0]);
}

void FragSelfFn(int argc, char **argv)
{
    HMESSAGEWRITE hWrite = g_pLTClient->StartMessage(MID_FRAG_SELF);
    g_pLTClient->EndMessage(hWrite);
}

void CheatFn(int argc, char **argv)
{
	if (g_pGameClientShell)
	{
		g_pGameClientShell->HandleCheat(argc, argv);
	}
}

void SunglassFn(int argc, char **argv)
{
	if (g_pInterfaceMgr)
	{
		int mode = (int)g_pInterfaceMgr->GetSunglassMode();
		mode++;
		if (mode == NUM_SUN_MODES)
		{
			g_pInterfaceMgr->SetSunglassMode(SUN_NONE);
            g_pLTClient->CPrint("sunglasses off");
		}
		else
		{
			g_pInterfaceMgr->SetSunglassMode((eSunglassMode)mode);
            g_pLTClient->CPrint("sunglasses %d",mode);
		}
	}
}

void ReloadWeaponAttributesFn(int argc, char **argv)
{
    g_pWeaponMgr->Reload(g_pLTClient);
    g_pLTClient->CPrint("Reloaded weapons attributes file...");
}

void ReloadSurfacesAttributesFn(int argc, char **argv)
{
    g_pSurfaceMgr->Reload(g_pLTClient);
    g_pLTClient->CPrint("Reloaded surface attributes file...");
}

void ReloadFXAttributesFn(int argc, char **argv)
{
    g_pFXButeMgr->Reload(g_pLTClient);
    g_pLTClient->CPrint("Reloaded fx attributes file...");

	// Make sure we re-load the weapons and surface data, it has probably
	// changed...
	ReloadWeaponAttributesFn(0, 0);
	ReloadSurfacesAttributesFn(0, 0);
}

void RecordFn(int argc, char **argv)
{
	if (g_pGameClientShell)
	{
		g_pGameClientShell->HandleRecord(argc, argv);
	}
}

void PlayDemoFn(int argc, char **argv)
{
	if (g_pGameClientShell)
	{
		g_pGameClientShell->HandlePlaydemo(argc, argv);
	}
}

void ExitLevelFn(int argc, char **argv)
{
	if (g_pGameClientShell)
	{
		g_pGameClientShell->HandleExitLevel(argc, argv);
	}
}

void ChaseToggleFn(int argc, char **argv)
{
	if (g_pGameClientShell)
	{
		g_pGameClientShell->ToggleDebugCheat(CHEAT_CHASETOGGLE);
	}
}

void ChangeTeamFn(int argc, char **argv)
{
    uint8 team = 0;
	if (argc >= 1)
	{
        team = (uint8)atoi(argv[0]);
	}

    HMESSAGEWRITE hWrite = g_pLTClient->StartMessage(MID_PLAYER_CHANGETEAM);
    g_pLTClient->WriteToMessageByte(hWrite, team);
    g_pLTClient->EndMessage(hWrite);

}

void ExitGame(LTBOOL bResponse, uint32 nUserData)
{
	if (bResponse)
	{
        g_pLTClient->Shutdown();
	}
}

void InitSoundFn(int argc, char **argv)
{
	if (g_pGameClientShell)
	{
		g_pGameClientShell->InitSound();
	}
}

void MusicFn(int argc, char **argv)
{
	if (!g_pGameClientShell->GetMusic()->IsInitialized())
	{
        g_pLTClient->CPrint("Direct Music hasn't been initialized!");
	}

	if (argc < 2)
	{
        g_pLTClient->CPrint("Music <command>");
        g_pLTClient->CPrint("  Commands: (syntax -> Command <Required> [Optional])");
        g_pLTClient->CPrint("    I  <intensity number to set> [when to enact change]");
        g_pLTClient->CPrint("    PS <segment name> [when to begin playing]");
        g_pLTClient->CPrint("    PM <motif style> <motif name> [when to begin playing]");
        g_pLTClient->CPrint("    V  <volume adjustment in db>");
        g_pLTClient->CPrint("    SS <segment name> [when to stop]");
        g_pLTClient->CPrint("    SM <motif name> [when to stop]");
        g_pLTClient->CPrint(" ");
        g_pLTClient->CPrint("  Enact Change Values:");
        g_pLTClient->CPrint("    Default     - Will use the default value that is defined in the Control File or by DirectMusic");
        g_pLTClient->CPrint("    Immediately - Will happen immediately");
        g_pLTClient->CPrint("    Beat        - Will happen on the next beat");
        g_pLTClient->CPrint("    Measure     - Will happen on the next measure");
        g_pLTClient->CPrint("    Grid        - Will happen on the next grid");
        g_pLTClient->CPrint("    Segment     - Will happen on the next segment transition");

		return;
	}

	// Build the command...

	char buf[512];
	buf[0] = '\0';
	sprintf(buf, "Music");
	for (int i=0; i < argc; i++)
	{
		strcat(buf, " ");
		strcat(buf, argv[i]);
	}

	g_pGameClientShell->GetMusic()->ProcessMusicMessage(buf);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::CGameClientShell()
//
//	PURPOSE:	Initialization
//
// ----------------------------------------------------------------------- //

CGameClientShell::CGameClientShell()
{
	m_nVRModelFrame = 0;
	m_nVRUpdateTick = 0;
	m_nVRPublishedTick = 0xFFFFFFFFu;
	m_bVRLoadPublish = false;
	//AfxSetAllocStop(37803);

	g_pGameClientShell = this;

	m_fFrameTime			= 0.0f;

	g_vWorldWindVel.Init();

    m_hCamera               = LTNULL;

	// VR: off-screen surface used to hold one eye while the other renders.
	m_hEyeStash             = LTNULL;
	m_hMarkerOn             = LTNULL;
	m_hMarkerOff            = LTNULL;
	m_hMarkerAlt            = LTNULL;
	m_nCalibFrames          = 0;
	m_nFieldFrame           = -1;
	m_nFieldFrames          = 0;
	m_fLastFieldRatio       = 0.0f;
	m_fLastFieldFocal       = 0.0f;
	m_bDumpCalibPasses      = LTFALSE;
	m_bSweepRotSaved        = LTFALSE;
	m_nEyeStashW            = 0;
	m_nEyeStashH            = 0;

	// VR: head-as-mouse experiment.
	m_fVRHeadPrevYawDeg     = 0.0f;
	m_fVRHeadPrevPitchDeg   = 0.0f;
	m_fVRHeadAccumYaw       = 0.0f;
	m_fVRHeadAccumPitch     = 0.0f;
	m_bVRHeadRefValid       = LTFALSE;

    m_hInterfaceCamera      = LTNULL;

    m_bUseWorldFog          = LTTRUE;
    m_bMainWindowMinimized  = LTFALSE;

    m_bStrafing             = LTFALSE;
    m_bHoldingMouseLook     = LTFALSE;

    m_bGamePaused           = LTFALSE;
	m_resSoundInit			= LT_ERROR;

	m_fYawBackup				= 0.0f;
	m_fPitchBackup				= 0.0f;
    m_bRestoreOrientation       = LTFALSE;
    m_bAllowPlayerMovement      = LTTRUE;
    m_bLastAllowPlayerMovement  = LTTRUE;
    m_bWasUsingExternalCamera   = LTFALSE;
    m_bUsingExternalCamera      = LTFALSE;
	m_bCamIsListener			= LTFALSE;

    m_bNightVision  = LTFALSE;
	m_vNVScreenTint.Init(0.0f, 0.0f, 0.0f);

	m_vDefaultLightScale.Init(1.0f, 1.0f, 1.0f);

	m_nPlayerInfoChangeFlags	= 0;
	m_fPlayerInfoLastSendTime	= 0.0f;

    m_rRotation.Init();

	m_fPitch			= 0.0f;
	m_fYaw				= 0.0f;
	m_fRoll				= 0.0f;

	m_fPlayerPitch		= 0.0f;
	m_fPlayerYaw		= 0.0f;
	m_fPlayerRoll		= 0.0f;

	m_fFireJitterPitch	= 0.0f;
	m_fFireJitterYaw	= 0.0f;

	m_dwPlayerFlags			= 0;
	m_ePlayerState			= PS_UNKNOWN;
    m_bSpectatorMode        = LTFALSE;
    m_bTweakingWeapon       = LTFALSE;
    m_bTweakingWeaponMuzzle = LTFALSE;
    m_bAdjustWeaponBreach   = LTFALSE;
    m_bAdjust1stPersonCamera = LTFALSE;

	m_vShakeAmount.Init();

    m_bFlashScreen      = LTFALSE;
	m_fFlashTime		= 0.0f;
	m_fFlashStart		= 0.0f;
	m_fFlashRampUp		= 0.0f;
	m_fFlashRampDown	= 0.0f;
	m_vFlashColor.Init();

	memset(m_strCurrentWorldName, 0, 256);

	m_vIRLightScale.Init(1.0f,1.0,1.0f);
	m_vCurContainerLightScale.Init(-1.0f, -1.0f, -1.0f);

	m_nZoomView				= 0;
    m_bZooming              = LTFALSE;
    m_bZoomingIn            = LTFALSE;
	m_fSaveLODScale			= DEFAULT_LOD_OFFSET;
    m_bInWorld              = LTFALSE;
    m_bStartedLevel         = LTFALSE;
	m_nCurrentLevel			= 0;
	m_nCurrentMission		= 0;
	m_nMPNameId				= 0;
	m_nMPBriefingId			= 0;


    m_bStartedDuckingDown   = LTFALSE;
    m_bStartedDuckingUp     = LTFALSE;
	m_fCamDuck				= 0.0f;
	m_fDuckDownV			= -75.0f;
	m_fDuckUpV				= 75.0f;
	m_fMaxDuckDistance		= -20.0f;
	m_fStartDuckTime		= 0.0f;

    m_h3rdPersonCrosshair   = LTNULL;
    m_hVRAimMarker          = LTNULL;
    m_bVRAimMarkerOn        = LTFALSE;
    m_hContainerSound       = LTNULL;
	m_eCurContainerCode		= CC_NO_CONTAINER;
	m_nSoundFilterId		= 0;
	m_nGlobalSoundFilterId	= 0;

    m_bShowPlayerPos        = LTFALSE;
	m_bShowCamPosRot		= LTFALSE;
    m_hDebugInfo            = LTNULL;

    m_bAdjustLightScale     = LTFALSE;
    m_bAdjustLightAdd       = LTFALSE;
    m_bAdjustFOV            = LTFALSE;

	m_fContainerStartTime	= -1.0f;
	m_fFovXFXDir			= 1.0f;

    m_bPanSky               = LTFALSE;
	m_fPanSkyOffsetX		= 1.0f;
	m_fPanSkyOffsetZ		= 1.0f;
	m_fPanSkyScaleX			= 1.0f;
	m_fPanSkyScaleZ			= 1.0f;
	m_fCurSkyXOffset		= 0.0f;
	m_fCurSkyZOffset		= 0.0f;

	m_vCurModelGlow.Init(127.0f, 127.0f, 127.0f);
	m_vMaxModelGlow.Init(255.0f, 255.0f, 255.0f);
	m_vMinModelGlow.Init(50.0f, 50.0f, 50.f);
	m_fModelGlowCycleTime	= 0.0f;
    m_bModelGlowCycleUp     = LTTRUE;

    m_bFirstUpdate          = LTFALSE;
    m_bRestoringGame        = LTFALSE;
    m_bQuickSave            = LTFALSE;

    m_bCameraPosInited      = LTFALSE;

	m_eDifficulty			= GD_NORMAL;
	m_bFadeBodies			= LTFALSE;
	m_eGameType				= SINGLE;
	m_eLevelEnd				= LE_UNKNOWN;
	m_nEndString			= 0;

	m_fEarliestRespawnTime	= 0.0f;
    m_hBoundingBox          = LTNULL;

    m_bMainWindowFocus      = LTFALSE;

    m_bPlayerPosSet         = LTFALSE;

	m_fNextSoundReverbTime	= 0.0f;
    m_bUseReverb            = LTFALSE;
	m_fReverbLevel			= 0.0f;
	m_bCameraAttachedToHead	= LTFALSE;

	m_vLastReverbPos.Init();

	m_szServerAddress[0]	= LTNULL;
	m_nServerPort			= -1;
	m_szServerName[0]		= LTNULL;
	memset(m_fServerOptions, 0, sizeof(m_fServerOptions));

	m_bForceDisconnect		= LTFALSE;

	m_nDisconnectCode = 0;
	m_nDisconnectSubCode = 0;
	m_pDisconnectMsg = LTNULL;

	// Start up SDL! -- Maybe trim down what we're initing here...
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

	// Setup the logging functions
	SDL_LogSetOutputFunction(&SDLLog, NULL);

	// Clear file
	g_SDLLogFile.open("Debug.log", STD ios::out | STD ios::trunc);
	g_SDLLogFile.close();

	SDL_Log("-- Hello World, We're all set here. Enjoy the show!");

	// Just some default
	m_lNextUpdate = 1L;
	m_iPreviousMouseX = 0;
	m_iPreviousMouseY = 0;
	m_iCurrentMouseX = 0;
	m_iCurrentMouseY = 0;

	// Get a reference for currentMouseX/Y!
	m_bGetBaseMouse = LTTRUE;

	m_bLockFramerate = LTTRUE;

	// If we can't get timer frequency, we can't limit the framerate!
	if(!QueryPerformanceFrequency(&m_lTimerFrequency)) {
		SDL_Log("Device doesn't support high resolution timer! Can't lock framerate.");
		m_bLockFramerate = LTFALSE;
	}

	m_lFrametime = (m_lTimerFrequency.QuadPart / 60);

	m_nTimeoutBugRetriesLeft = MAX_TIMEOUT_RETRIES;
	m_sRetryAddress = "";
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::~CGameClientShell()
//
//	PURPOSE:	Destruction
//
// ----------------------------------------------------------------------- //

CGameClientShell::~CGameClientShell()
{
	if (m_hDebugInfo)
	{
        g_pLTClient->DeleteSurface(m_hDebugInfo);
		m_hDebugInfo = NULL;
	}

	if (m_hBoundingBox)
	{
        g_pLTClient->DeleteObject(m_hBoundingBox);
	}

	if (g_prcClip)
	{
		debug_delete(g_prcClip);
        g_prcClip = LTNULL;
	}

	if (m_pDisconnectMsg)
	{
		debug_deletea(m_pDisconnectMsg);
		m_pDisconnectMsg = LTNULL;
	}

    g_pGameClientShell = LTNULL;

	SDL_Quit();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::InitSound
//
//	PURPOSE:	Initialize the sounds
//
// ----------------------------------------------------------------------- //

void CGameClientShell::InitSound()
{
	CGameSettings* pSettings = m_InterfaceMgr.GetSettings();
	if (!pSettings) return;

	Sound3DProvider *pSound3DProviderList, *pSound3DProvider;
	InitSoundInfo soundInfo;
	ReverbProperties reverbProperties;
	HCONSOLEVAR hVar;
    uint32 dwProviderID;
	char sz3dSoundProviderName[_MAX_PATH + 1];
	int nError;

	m_resSoundInit = LT_ERROR;

    uint32 dwAdvancedOptions = m_InterfaceMgr.GetAdvancedOptions();
	if (!(dwAdvancedOptions & AO_SOUND)) return;

    soundInfo.Init();

	// Reload the sounds if there are any...

	soundInfo.m_dwFlags	= INITSOUNDINFOFLAG_RELOADSOUNDS;

	// Get the 3d sound provider id....

    hVar = g_pLTClient->GetConsoleVar("3DSoundProvider");
	if (hVar)
	{
        dwProviderID = ( uint32 )g_pLTClient->GetVarValueFloat( hVar );
	}
	else
	{
		dwProviderID = SOUND3DPROVIDERID_NONE;
	}

	// Can also be set by provider name, in which case the id will be set to
	// UNKNOWN...

	if ( dwProviderID == SOUND3DPROVIDERID_NONE ||
		 dwProviderID == SOUND3DPROVIDERID_UNKNOWN )
	{
		sz3dSoundProviderName[0] = 0;
        hVar = g_pLTClient->GetConsoleVar("3DSoundProviderName");
		if ( hVar )
		{
            SAFE_STRCPY( sz3dSoundProviderName, g_pLTClient->GetVarValueString( hVar ));
			dwProviderID = SOUND3DPROVIDERID_UNKNOWN;
		}
	}

	// See if the provider exists....

	// ---- WHICH 3D SOUND PROVIDERS DOES THIS MACHINE HAVE? ---------------
	//
	// NOLF never writes 3DSoundProvider itself, and neither our config nor a
	// real retail one has it - so dwProviderID is NONE and the game runs with
	// no positional audio provider at all, only the engine's own stereo panning
	// from the listener. In a headset that is most of why a head-tracked
	// listener is hard to NOTICE: turning your head changes a pan, not a
	// direction. the sound direction went unnoticed.
	//
	// The game ships seven providers (MSSDS3DS.M3D, MSSEAX.M3D, MSSDOLBY.M3D
	// and the rest), so the choice can be made from evidence rather than from
	// assumption. +VRSoundProvider "<name>" selects one by name, opt in, and
	// touches none of the game's own options.
	// FALLBACKS, in preference order, for the init loop below: every preferred
	// provider this machine lists, so a provider that lists but will not open
	// costs one failed init and not the whole sound engine.
	char aszVRFallback[8][_MAX_PATH + 1];
	int  nVRFallback = 0;
	// THE PROVIDER THAT ACTUALLY CAME UP LAST TIME. InitSound runs again at
	// every level end (VRSoundReset) and on an options change; the failures
	// below are not worth repeating each time, and each one pops the speakers.
	static char s_szVRProven[_MAX_PATH + 1] = "";
	{
		// THE VERIFIED LIST, not the unverified one. 12 September: the
		// unverified list named twelve providers and 'DirectSound3D 7+
		// Software - Full HRTF' was chosen from it, InitSound then FAILED on
		// that provider, and the client never looked at the result - so the
		// game ran for a whole headset session with NO SOUND ENGINE AT ALL:
		// every PlaySound, 2D or 3D, came back LT_ERROR: in the headset there was no
		// spoken dialog and no mouth movement. Five of the twelve open on this
		// machine's Windows 11 and none of the HRTF or hardware ones do.
		// bVerify asks the engine to open each one before listing it.
		Sound3DProvider* pVRList = LTNULL;
		g_pLTClient->GetSound3DProviderLists(pVRList, LTTRUE);
		int nVRProv = 0;
		for (Sound3DProvider* q = pVRList; q; q = q->m_pNextProvider, ++nVRProv)
			VRLog::Msg("VRSound: 3D provider available (verified) - id %u '%s' caps 0x%x",
					   (unsigned)q->m_dwProviderID, q->m_szProvider, (unsigned)q->m_dwCaps);
		if (!nVRProv) VRLog::Msg("VRSound: this machine offers NO 3D sound provider");
		VRLog::Msg("VRSound: the game asked for provider id %u%s",
				   (unsigned)dwProviderID,
				   (dwProviderID == SOUND3DPROVIDERID_NONE)
					 ? "  <- NONE: nothing places a sound in 3D, the listener only pans"
					 : "");
		// AND CHOOSE ONE BY DEFAULT, because the game never does.
		//
		// NOLF writes no 3DSoundProvider of its own, so a player who has not
		// been into Options > Sound runs with NONE - the listener pans left
		// and right and nothing is placed in space. In a headset that is most
		// of why turning your head does not move a sound; in headset testing the
		// sound direction went unnoticed.
		//
		// The name cannot be hardcoded: this machine offers twelve providers
		// and another may offer none of them. So walk a preference order and
		// take the first that exists, best for headphones first. A name in
		// VRSoundProvider still wins, and VRSoundProvider 0 turns the whole
		// thing off and restores the retail default of nothing.
		const char* pszWant = LTNULL;
		HCONSOLEVAR hVRP = g_pLTClient->GetConsoleVar("VRSoundProvider");
		if (hVRP) pszWant = g_pLTClient->GetVarValueString(hVRP);
		const bool bOff = (!pszWant || !*pszWant || stricmp(pszWant, "0") == 0);
		// OPT IN, NOT DEFAULT. 12 September, evening, headset testing on the software
		// emulation provider: noticeable high-pitched screeching and hitching
		// of the music walking through the Morocco gate - after
		// days of clean audio with no provider at all, and with HRTF, the
		// reason for a provider, not available on this machine. The retail
		// state is the default again; +VRSoundProvider auto walks the
		// preference list, +VRSoundProvider "<name>" picks one by name.
		const bool bAuto = (pszWant && (stricmp(pszWant, "auto") == 0 || stricmp(pszWant, "1") == 0));
		if (bOff)
			VRLog::Msg("VRSound: no 3D provider by default (the retail state);"
					   " +VRSoundProvider auto or a name opts in");
		if (pszWant && *pszWant && !bOff && !bAuto)
		{
			SAFE_STRCPY(sz3dSoundProviderName, pszWant);
			dwProviderID = SOUND3DPROVIDERID_UNKNOWN;
			VRLog::Msg("VRSound: VRSoundProvider asked for '%s'", pszWant);
		}
		// NONE, or the equally dead state of UNKNOWN with no name to match:
		// the block above sets UNKNOWN whenever the 3DSoundProviderName var
		// merely EXISTS, empty or not, and an empty name matches no provider.
		else if (bAuto && (dwProviderID == SOUND3DPROVIDERID_NONE
						   || (dwProviderID == SOUND3DPROVIDERID_UNKNOWN
							   && !sz3dSoundProviderName[0])))
		{
			dwProviderID = SOUND3DPROVIDERID_NONE;   // so the loop below can end
			static const char* const kPrefer[] = {
				"DirectSound3D 7+ Software - Full HRTF",	// true binaural
				"DirectSound3D 7+ Software - Light HRTF",
				"DirectSound3D Hardware Support",
				"Creative Labs EAX 2 (TM)",
				"Creative Labs EAX (TM)",
				"DirectSound3D Software Emulation",
			};
			for (int iP = 0; iP < (int)(sizeof kPrefer / sizeof kPrefer[0]); ++iP)
			{
				for (Sound3DProvider* q = pVRList; q; q = q->m_pNextProvider)
				{
					if (stricmp(q->m_szProvider, kPrefer[iP]) != 0) continue;
					if (nVRFallback < 8)
						SAFE_STRCPY(aszVRFallback[nVRFallback++], q->m_szProvider);
					if (dwProviderID != SOUND3DPROVIDERID_NONE) break;
					SAFE_STRCPY(sz3dSoundProviderName, q->m_szProvider);
					dwProviderID = SOUND3DPROVIDERID_UNKNOWN;
					VRLog::Msg("VRSound: no provider was chosen by the game, so"
							   " taking '%s' - the best of the %d this machine"
							   " offers for headphones. VRSoundProvider 0 goes"
							   " back to none.", q->m_szProvider, nVRProv);
					break;
				}
			}
			// Start from the one that came up last time, if there is one.
			if (s_szVRProven[0] && dwProviderID != SOUND3DPROVIDERID_NONE
				&& stricmp(s_szVRProven, sz3dSoundProviderName) != 0)
			{
				VRLog::Msg("VRSound: '%s' is the provider that initialised last time -"
						   " starting there rather than at '%s'",
						   s_szVRProven, sz3dSoundProviderName);
				SAFE_STRCPY(sz3dSoundProviderName, s_szVRProven);
			}
		}
		g_pLTClient->ReleaseSound3DProviderList(pVRList);
	}

	if ( dwProviderID != SOUND3DPROVIDERID_NONE )
	{
        g_pLTClient->GetSound3DProviderLists( pSound3DProviderList, LTFALSE );
		if ( !pSound3DProviderList )
		{
			m_resSoundInit = LT_NO3DSOUNDPROVIDER;
			return;
		}

		pSound3DProvider = pSound3DProviderList;
		while ( pSound3DProvider )
		{
			// If the provider is selected by name, then compare the names.
			if (  dwProviderID == SOUND3DPROVIDERID_UNKNOWN )
			{
				if ( _mbscmp(( const unsigned char * )sz3dSoundProviderName, ( const unsigned char * )pSound3DProvider->m_szProvider ) == 0 )
					break;
			}
			// Or compare by the id's.
			else if ( pSound3DProvider->m_dwProviderID == dwProviderID )
				break;

			// Not this one, try next one.
			pSound3DProvider = pSound3DProvider->m_pNextProvider;
		}

		// DID IT TAKE? Asking for a provider and getting one are different
		// things, and the log only recorded the asking - so a run with
		// VRSoundProvider set looked identical to one without. The list is
		// walked by NAME here, and a name that does not match exactly leaves
		// the game with no positional audio at all, silently, which is the
		// state it was already in.
		VRLog::Msg("VRSound: 3D provider %s%s%s", pSound3DProvider ? "SELECTED: '" : "NOT selected",
				   pSound3DProvider ? pSound3DProvider->m_szProvider : " - nothing matched the name asked for;"
									  " the listener will only pan",
				   pSound3DProvider ? "'" : "");

		// Check if we found one.
		if (pSound3DProvider)
		{
			// Use this provider.
			SAFE_STRCPY( soundInfo.m_sz3DProvider, pSound3DProvider->m_szProvider);

			// Get the maximum number of 3d voices to use.
            hVar = g_pLTClient->GetConsoleVar("Max3DVoices");
			if (hVar)
			{
                soundInfo.m_nNum3DVoices = (uint8)g_pLTClient->GetVarValueFloat(hVar);
			}
			else
			{
				soundInfo.m_nNum3DVoices = 16;
			}
		}

        g_pLTClient->ReleaseSound3DProviderList(pSound3DProviderList);
	}

	// Get the maximum number of sw voices to use.
    hVar = g_pLTClient->GetConsoleVar("MaxSWVoices");
	if (hVar)
	{
        soundInfo.m_nNumSWVoices = (uint8)g_pLTClient->GetVarValueFloat(hVar);
	}
	else
	{
		soundInfo.m_nNumSWVoices = 32;
	}

	soundInfo.m_nSampleRate		= 22050;
	soundInfo.m_nBitsPerSample	= 16;
	soundInfo.m_nVolume			= (unsigned short)pSettings->SoundVolume();

	if ( !pSettings->Sound16Bit( ) )
	{
		soundInfo.m_dwFlags |= INITSOUNDINFOFLAG_CONVERT16TO8;
	}

	soundInfo.m_fDistanceFactor = 1.0f / 64.0f;

	// Go initialize the sounds...

    m_bUseReverb = LTFALSE;
	// VERIFY BY RESULT, AND FALL BACK. Asking for a provider and getting a
	// sound engine are different things (see the block above). Try what was
	// chosen; if InitSound refuses it, try the next preferred provider this
	// machine lists; and if every one refuses, try NONE - the retail state,
	// in which the game has had working audio for twenty-five years. A
	// provider named by hand (VRSoundProvider or the Options page) falls
	// back the same way, with the reason in the log, rather than leaving the
	// player in silence the way retail's error box would.
	{
		char szTried[_MAX_PATH + 1];
		SAFE_STRCPY(szTried, soundInfo.m_sz3DProvider);
		int iFB = 0;
		for (int nAttempt = 0; nAttempt < 10; ++nAttempt)
		{
			m_resSoundInit = g_pLTClient->InitSound(&soundInfo);
			VRLog::Msg("VRSound: InitSound with provider '%s' -> %s (rc %u)%s",
					   soundInfo.m_sz3DProvider[0] ? soundInfo.m_sz3DProvider : "<none>",
					   (m_resSoundInit == LT_OK) ? "OK" : "FAILED - the sound engine is DOWN with this provider",
					   (unsigned)m_resSoundInit,
					   (m_resSoundInit == LT_OK && (soundInfo.m_dwResults & INITSOUNDINFORESULTS_REVERB)) ? ", reverb" : "");
			if (m_resSoundInit == LT_OK)
			{
				if (soundInfo.m_sz3DProvider[0]) SAFE_STRCPY(s_szVRProven, soundInfo.m_sz3DProvider);
				break;
			}
			if (!soundInfo.m_sz3DProvider[0]) break;	// none failed too; nothing left to try
			const char* pszNext = "";
			while (iFB < nVRFallback)
			{
				const char* c = aszVRFallback[iFB++];
				if (stricmp(c, szTried) == 0 || stricmp(c, soundInfo.m_sz3DProvider) == 0) continue;
				pszNext = c;
				break;
			}
			VRLog::Msg("VRSound: falling back to %s%s%s", pszNext[0] ? "'" : "NO provider (the retail state)",
					   pszNext, pszNext[0] ? "'" : "");
			SAFE_STRCPY(soundInfo.m_sz3DProvider, pszNext);
		}
	}
    if (m_resSoundInit == LT_OK)
	{
		if (soundInfo.m_dwResults & INITSOUNDINFORESULTS_REVERB)
		{
            m_bUseReverb = LTTRUE;
		}

        hVar = g_pLTClient->GetConsoleVar("ReverbLevel");
		if (hVar)
		{
            m_fReverbLevel = g_pLTClient->GetVarValueFloat(hVar);
		}
		else
		{
			m_fReverbLevel = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_DEFAULTLEVEL);
		}

		reverbProperties.m_dwParams = REVERBPARAM_VOLUME;
		reverbProperties.m_fVolume = m_fReverbLevel;
        g_pLTClient->SetReverbProperties(&reverbProperties);
	}
	else
	{
		if (m_resSoundInit == LT_NO3DSOUNDPROVIDER)
		{
			nError = IDS_INVALID3DSOUNDPROVIDER;
		}
		else
		{
			nError = IDS_SOUNDNOTINITED;
		}

		// DoMessageBox( nError, TH_ALIGN_CENTER );
	}

}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::PreChangeGameState
//
//	PURPOSE:	Handle pre setting of game state
//
// ----------------------------------------------------------------------- //

LTBOOL CGameClientShell::PreChangeGameState(GameState eNewState)
{
	switch (eNewState)
	{
		case GS_FOLDER :
		{
			if (m_bUsingExternalCamera)
			{
				TurnOffAlternativeCamera(CT_FULLSCREEN);
                m_bCameraPosInited = LTFALSE;

				// Special case, we want to still be using the external camera
				// when we return to the game...

                m_bUsingExternalCamera = LTTRUE;
			}
		}
		break;

		default : break;
	}

    return LTTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::PostChangeGameState
//
//	PURPOSE:	Handle post setting of game state
//
// ----------------------------------------------------------------------- //

LTBOOL CGameClientShell::PostChangeGameState(GameState eOldState)
{
    return LTTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::CSPrint
//
//	PURPOSE:	Displays a line of text on the client
//
// ----------------------------------------------------------------------- //

void CGameClientShell::CSPrint(char* msg, ...)
{
	// parse the message

	char pMsg[256];
	va_list marker;
	va_start (marker, msg);
	int nSuccess = vsprintf (pMsg, msg, marker);
	va_end (marker);

	if (nSuccess < 0) return;

	// now display the message
	m_InterfaceMgr.GetMessageMgr()->AddLine(pMsg);
}

void CGameClientShell::UpdateConfigSettings()
{

	// Update other config settings if available!
	if (g_pInterfaceMgr)
	{
		g_pInterfaceMgr->UpdateConfigSettings();
	}
}

void CGameClientShell::ClearBindings()
{
	uint32 devices[3] =
	{
		DEVICETYPE_KEYBOARD,
		DEVICETYPE_MOUSE,
		DEVICETYPE_JOYSTICK
	};


	for (int i = 0; i < 3; ++i)
	{
		DeviceBinding* pBindings = g_pLTClient->GetDeviceBindings(devices[i]);
		if (!pBindings)
		{
			continue;
		}

		char str[128];
		DeviceBinding* ptr = pBindings;
		while (ptr)
		{
			if (ptr->strTriggerName[0] == ';')
				sprintf(str, "rangebind \"%s\" \"##39\" 0 0 \"\"", ptr->strDeviceName);
			else
				sprintf(str, "rangebind \"%s\" \"%s\" 0 0 \"\"", ptr->strDeviceName, ptr->strTriggerName);
			g_pLTClient->RunConsoleString(str);

			ptr = ptr->pNext;
		}

		g_pLTClient->FreeDeviceBindings(pBindings);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::OnEngineInitialized
//
//	PURPOSE:	Called after engine is fully initialized
//				Handle object initialization here
//
// ----------------------------------------------------------------------- //

uint32 CGameClientShell::OnEngineInitialized(RMode *pMode, LTGUID *pAppGuid)
{
	//CWinUtil::DebugBreak();

	// VR: first thing that happens, so an early failure still leaves a log.
	VRLog::Init();
	VRLog::Msg("OnEngineInitialized");

	*pAppGuid = NOLFGUID;

    char strTimeDiff[64];
	float fStartTime = CWinUtil::GetTime();

	if (!g_hMainWnd)
	{
		HookWindow();
	}

	// Initialize all the global bute mgrs...

	if (!m_GlobalMgr.Init())
	{
		return LT_ERROR;
	}

	ConsoleMgr* conMgr = new ConsoleMgr();

	// Initialize global console variables...

    g_vtFOVXNormal.Init(g_pLTClient, "FovX", NULL, 90.0f);
    g_vtFOVYNormal.Init(g_pLTClient, "FovY", NULL, 78.0f);

    g_vtInterfceFOVX.Init(g_pLTClient, "FovXInterface", NULL, 90.0f);
    g_vtInterfceFOVY.Init(g_pLTClient, "FovYInterface", NULL, 75.0f);

    g_CV_CSendRate.Init(g_pLTClient, "CSendRate", NULL, DEFAULT_CSENDRATE);
    g_vtPlayerRotate.Init(g_pLTClient, "PlayerRotate", NULL, 1.0f);

    g_vtShowTimingTrack.Init(g_pLTClient, "ShowTiming", NULL, 0.0f);

    g_vtFastTurnRate.Init(g_pLTClient, "FastTurnRate", NULL, 2.3f);
    g_vtNormalTurnRate.Init(g_pLTClient, "NormalTurnRate", NULL, 1.5f);
    g_vtLookUpRate.Init(g_pLTClient, "LookUpRate", NULL, 2.5f);

    g_vtCameraSwayXFreq.Init(g_pLTClient, "CameraSwayXFreq", NULL, 13.0f);
    g_vtCameraSwayYFreq.Init(g_pLTClient, "CameraSwayYFreq", NULL, 5.0f);
    g_vtCameraSwayXSpeed.Init(g_pLTClient, "CameraSwayXSpeed", NULL, 12.0f);
    g_vtCameraSwayYSpeed.Init(g_pLTClient, "CameraSwayYSpeed", NULL, 1.5f);
    g_vtCameraSwayDuckMult.Init(g_pLTClient, "CameraSwayCrouchMultiplier", NULL, 0.5f);

    g_vtChaseCamOffset.Init(g_pLTClient, "ChaseCamOffset", NULL, 50.0f);
    g_vtChaseCamPitchAdjust.Init(g_pLTClient, "ChaseCamPitchAdjust", NULL, 0.0f);
    g_vtChaseCamDistUp.Init(g_pLTClient, "ChaseCamDistUp", NULL, 10.0f);
    g_vtChaseCamDistBack.Init(g_pLTClient, "ChaseCamDistBack", NULL, 100.0f);

    g_vtActivateOverride.Init(g_pLTClient, "ActivateOverride", " ", 0.0f);

    g_vtCamDamage.Init(g_pLTClient, "CamDamage", NULL, 1.0f);
    g_vtCamDamagePitch.Init(g_pLTClient, "CamDamagePitch", NULL, 1.0f);
    g_vtCamDamageRoll.Init(g_pLTClient, "CamDamageRoll", NULL, 1.0f);
    g_vtCamDamageTime1.Init(g_pLTClient, "CamDamageTime1", NULL, 0.1f);
    g_vtCamDamageTime2.Init(g_pLTClient, "CamDamageTime2", NULL, 0.25f);
    g_vtCamDamageVal.Init(g_pLTClient, "CamDamageVal", NULL, 5.0f);
    g_vtCamDamagePitchMin.Init(g_pLTClient, "CamDamagePitchMin", NULL, 0.7f);
    g_vtCamDamageRollMin.Init(g_pLTClient, "CamDamageRollMin", NULL, 0.7f);

    g_varStartLevelScreenFade.Init(g_pLTClient, "ScreenFadeAtLevelStart", NULL, 1.0f);
    g_varStartLevelScreenFadeTime.Init(g_pLTClient, "ScreenFadeInAtLevelStartTime", NULL, 3.0f);
	g_vtScreenFadeInTime.Init(g_pLTClient, "ScreenFadeInTime", LTNULL, 3.0f);
	g_vtScreenFadeOutTime.Init(g_pLTClient, "ScreenFadeOutTime", LTNULL, 5.0f);

    g_vtUseCamRecoil.Init(g_pLTClient, "CamRecoil", NULL, 0.0f);
    g_vtCamRecoilRecover.Init(g_pLTClient, "CamRecoilRecover", NULL, 0.3f);
    g_vtBaseCamRecoilPitch.Init(g_pLTClient, "CamRecoilBasePitch", NULL, 5.0f);
    g_vtMaxCamRecoilPitch.Init(g_pLTClient, "CamRecoilMaxPitch", NULL, 75.0f);
    g_vtBaseCamRecoilYaw.Init(g_pLTClient, "CamRecoilBaseYaw", NULL, 3.0f);
    g_vtMaxCamRecoilYaw.Init(g_pLTClient, "CamRecoilMaxYaw", NULL, 35.0f);
    g_vtCamRotInterpTime.Init(g_pLTClient, "CamRotInterpTime", NULL, 0.15f);

	g_vtRespawnWaitTime.Init(g_pLTClient, "RespawnWaitTime", NULL, 1.0f);
	g_vtMultiplayerRespawnWaitTime.Init(g_pLTClient, "RespawnMultiWaitTime", NULL, 0.5f);

	g_vtUseSoundFilters.Init(g_pLTClient, "SoundFilters", LTNULL, 0.0f);

	g_vtSpecial.Init(g_pLTClient, "ShowSpecial", LTNULL, 0.0f);

	g_vtModelGlowTime.Init(g_pLTClient, "ModelGlowTime", LTNULL, 1.5f);
	g_vtModelGlowMin.Init(g_pLTClient, "ModelGlowMin", LTNULL, -25.0f);
	g_vtModelGlowMax.Init(g_pLTClient, "ModelGlowMax", LTNULL, 75.0f);

	g_vtSunZoomLevel1MaxDist.Init(g_pLTClient, "SunZoomLevel1MaxDist", LTNULL, 100.0f);
	g_vtSunZoomLevel2MaxDist.Init(g_pLTClient, "SunZoomLevel2MaxDist", LTNULL, 700.0f);

	// Default these to use values specified in the weapon...(just here for
	// tweaking values...)

    g_vtFireJitterDecayTime.Init(g_pLTClient, "FireJitterDecayTime", NULL, -1.0f);
    g_vtFireJitterMaxPitchDelta.Init(g_pLTClient, "FireJitterMaxPitchDelta", NULL, -1.0f);

    HSTRING hStr = g_pLTClient->FormatString(IDS_PLAYER);
    g_vtPlayerName.Init(g_pLTClient, "NetPlayerName", g_pLTClient->GetStringData(hStr), 0.0f);
	g_pLTClient->FreeString (hStr);


	g_vtFOVYMaxUW.Init(g_pLTClient, "FOVYUWMax", NULL, 78.0f);
    g_vtFOVYMinUW.Init(g_pLTClient, "FOVYUWMin", NULL, 77.0f);
    g_vtUWFOVRate.Init(g_pLTClient, "FOVUWRate", NULL, 0.3f);

    HCONSOLEVAR hIsSet = g_pLTClient->GetConsoleVar("UpdateRateInitted");
    if (!hIsSet || g_pLTClient->GetVarValueFloat(hIsSet) != 1.0f)
	{
		// Initialize the update rate.
        g_pLTClient->RunConsoleString("+UpdateRateInitted 1");
        g_pLTClient->RunConsoleString("+UpdateRate 6");
	}

	// Init new console vars
	g_vtLockFPS.Init(g_pLTClient, "FramerateLock", NULL, 1.0f);
	g_vtShowFPS.Init(g_pLTClient, "ShowFramerate", NULL, 0.0f);
	g_vtOldMouseLook.Init(g_pLTClient, "OldMouseLook", NULL, 0.0f);
	g_vtNoFunMenus.Init(g_pLTClient, "NoFunMenus", NULL, 0.0f);
	g_vtLockCinematicAspectRatio.Init(g_pLTClient, "RestrictCinematicsTo4x3", NULL, 0.0f);
	g_vtQuickSwitch.Init(g_pLTClient, "QuickSwitch", NULL, 0.0f);
	g_vtUIScale.Init(g_pLTClient, "UIScale", NULL, 0.5f);
	g_vtUseGOTYMenu.Init(g_pLTClient, "UseGotyMenu", NULL, 1.0f);
	g_vtNoRawInput.Init(g_pLTClient, "NoRawInput", NULL, 0.0f);
	g_vtConsoleBackdrop.Init(g_pLTClient, "ConsoleBackdrop", NULL, 0.0f);
	g_vtBigHeadMode.Init(g_pLTClient, "BigHeadMode", NULL, 0.0f);
	g_vtEnableScreenTint.Init(g_pLTClient, "EnableScreenTinting", LTNULL, 1.0f);

	// VR: 0 = off (one render, stock behaviour)
	//     1 = world drawn twice with the SAME camera (the M2 correctness test)
	//     2 = side-by-side stereo, half IPD per eye (M3)
	// Cycled in-game with F9, per a project rule.
	g_vtVRStereo.Init(g_pLTClient, "VRStereo", LTNULL, 2.0f);

	// Interpupillary distance in world units.
	//
	// MEASURED, not assumed. A downward raycast puts the camera 94 units above
	// the floor; at a 1.6m standing eye height that makes one world unit about
	// 17mm, so a 62.1mm IPD is ~3.65 units.
	//
	// The earlier value of 2.5 came from assuming 1 unit = 1 inch, derived from
	// the -20 unit crouch distance and a textbook IPD. That would have put the
	// camera 2.39m off the ground. Being 46% too small gave too little
	// parallax, so the world read as larger and further away - an error that is
	// negligible at distance and severe up close, which is exactly how it
	// presented and why no FOV value could fix it.
	g_vtVRIPD.Init(g_pLTClient, "VRIPD", LTNULL, 3.65f);

	// Swaps which half of the window each eye occupies. Parallel (wall-eyed)
	// free-viewing wants left eye on the left; cross-eyed viewing wants the
	// opposite. Most people can only do one.
	g_vtVRSwapEyes.Init(g_pLTClient, "VRSwapEyes", LTNULL, 0.0f);

	// Whether to narrow horizontal FOV to match the half-width viewport.
	// It is not yet established whether this renderer derives its projection
	// from the camera rect or from the FOV pair - if from the rect, adjusting
	// FOV double-corrects and distorts. Set 0 to test that directly.
	g_vtVRFovAdjust.Init(g_pLTClient, "VRFovAdjust", LTNULL, 1.0f);

	// 0 = one Start3D/End3D around both eyes, 1 = a scene block per eye.
	// Tests whether the renderer builds its projection at Start3D, which would
	// explain a viewport at x=960 rendering with full-screen proportions.
	g_vtVRSceneMode.Init(g_pLTClient, "VRSceneMode", LTNULL, 0.0f);

	// Horizontal FOV multiplier applied ONLY to the eye whose viewport does not
	// start at x=0. This renderer draws a wider field into an offset viewport
	// than into one at the origin, and d3d.ren is a closed binary, so the
	// relationship is being measured rather than derived. The value that makes
	// both halves match is the measurement.
	g_vtVROffsetFovScale.Init(g_pLTClient, "VROffsetFovScale", LTNULL, 1.0f);

	// 0 = route the first eye through an off-screen surface: two copies.
	// 1 = single screen-to-screen copy. TESTED AND BROKEN - d3d.ren produces a
	// black destination for a same-surface blit, so the right eye vanishes.
	// Kept only to document the attempt; the two-copy route is the default.
	// Surface copying dominates the stereo path's cost and is the core M4
	// question, so a cheaper transport is M4's job, not a fix to retry here.
	g_vtVRDirectBlit.Init(g_pLTClient, "VRDirectBlit", LTNULL, 0.0f);

	// 1 = "half SBS", the convention 3D viewers expect: each eye is rendered
	// horizontally squeezed into its half, and the viewer stretches it back to
	// full width. 0 = each half is already correct-aspect ("full SBS").
	// Wrong choice looks like everything being twice as wide or half as wide.
	g_vtVRHalfSbs.Init(g_pLTClient, "VRHalfSbs", LTNULL, 1.0f);

	// The HUD is drawn once across the whole window, so it straddles the seam
	// and lands in different places in each eye - uncomfortable in a headset.
	// Hide it while judging stereo. Proper per-eye HUD is M6.
	g_vtVRHideHud.Init(g_pLTClient, "VRHideHud", LTNULL, 0.0f);

	// Head tracking, read from the host's shared block. Off means the camera
	// behaves exactly as it does without a headset.
	g_vtVRHeadTracking.Init(g_pLTClient, "VRHeadTracking", LTNULL, 1.0f);

	// Per-axis sign, measured rather than derived. OpenXR is right-handed with
	// -Z forward; LithTech is left-handed with +Z forward. That difference is
	// exactly a Z-axis flip, which inverts rotations about X and Y and leaves
	// rotation about Z alone.
	//
	// Confirmed in the headset: yaw (about Y) inverted, pitch (about X)
	// inverted, roll (about Z) correct. All three agree with the Z-flip, so
	// these defaults are the conversion, not a fudge.
	// 1 = apply the head rotation as the quaternion the runtime reported.
	// 0 = the old path, rebuilt from three Euler angles with the signs below.
	//
	// The Euler path is kept so the two can be compared in-game without a
	// rebuild. It is exact for rotation about one axis, which is why single
	// axis testing never showed a problem; it drifts as soon as pitch and roll
	// are combined, which is what tilting the head does.
	// DEFAULT 2 - a deliberate choice, made with the trade-off known.
	//
	// Mode 1 is the CORRECT camera composition (measured; see
	// LogRotationConvention, which prints the proof every run). Mode 2 is not:
	// it applies the head rotation in the world frame, and the resulting
	// corruption rotates with the body yaw rather than being a fixed swap:
	//
	//   head pitch +20, no head roll -> what the view does
	//     body yaw    0 deg   pitch -20  roll   0     identical to mode 1
	//     body yaw   45 deg   pitch -14  roll -14     a diagonal
	//     body yaw   90 deg   pitch   0  roll -20     a full pitch/roll swap
	//     body yaw  180 deg   pitch +20  roll   0     pitch INVERTED
	//
	// But mode 2 does not bend the world, and mode 1 does. Bending had resisted
	// a month of work; the head-axis corruption is understood and bounded. The
	// owner judged the trade and chose this, and reverting it unilaterally on 1
	// September was a mistake - it threw away the fix to the hard problem in
	// order to escape the easy one.
	//
	// REVERTED to 1 on 2 September, with numbers this time.
	//
	// The waypoint above rested on two claims. Both are now measured, and
	// both are false.
	//
	// "Telling the host the body yaw fixes mode 1." It cannot. The runtime
	// resamples by P^-1 * P2, the declared render pose against the display
	// pose, both located in whatever space the layer is submitted in. Rotate
	// that space by any T and both poses become T^-1 * P, so T cancels
	// exactly and the correction is unchanged. The yaw-carrying reference
	// space is inert by construction - it was never going to do anything.
	//
	// "Mode 2 agrees with the frame the host declares in." The opposite.
	// Mode 1 renders C = body * head, so C^-1 * C2 = head^-1 * head2, which
	// is exactly the correction the runtime applies - the body cancels.
	// Mode 2 renders C = head * body, so C^-1 * C2 = body^-1 (head^-1 head2)
	// body, and the compositor turns the picture about an axis rotated away
	// from the one the scene moved on. Worst case measured at 5.7 degrees
	// per frame of ordinary head motion, changing every frame. A wide flat
	// image resampled that way every frame is what the reported walls bending
	// like paper looks like, and it happens only when the HEAD moves because
	// only head motion drives the correction at all.
	//
	// tools/frame-agreement.py prints the table; LogFrameAgreement() below
	// prints the same table from inside the engine, so the two conventions
	// are the real ones. F5 still cycles if it needs comparing.
	g_vtVRQuatHead.Init(g_pLTClient, "VRQuatHead", LTNULL, 1.0f);

	// Milliseconds between the client reading a head pose and the host putting
	// the resulting image in front of the eye. The host tags each submitted
	// frame with the pose from this long ago; too high and the runtime
	// over-corrects and the world swims behind the head, too low and it snaps
	// ahead. 45 was an early guess and has never been measured against the
	// pipeline that actually exists, so it is a console variable now.
	// DEFAULT 0, measured. Swept in the headset from 0 to 100 ms while moving
	// the head: 100 ms swims noticeably, 0 ms is tight. 45 was an early guess
	// and was making things worse - it told the runtime the image was rendered
	// from a pose 45 ms stale, so the runtime rotated it further than the frame
	// actually needed and the world lagged the head.
	g_vtVRPoseLag.Init(g_pLTClient, "VRPoseLag", LTNULL, 0.0f);

	// Render each eye about its own optical centre instead of its forward
	// axis. 0 = off (the shipped behaviour), 1 = on, 2 = on with the centre
	// signs flipped, in case the handedness reasoning is backwards.
	//
	// DEFAULT 1, confirmed in the headset: mode 1 removed the warping that had
	// been open since M5, with no double image. 2 and 3 add the horizontal
	// centre, which is the axis that can diverge the eyes; 2 did exactly that
	// before the client began publishing the rotation it actually applied.
	g_vtVRAsymFrustum.Init(g_pLTClient, "VRAsymFrustum", LTNULL, 1.0f);

	// Stamp each frame with the pose it was rendered from, so the host can
	// measure staleness instead of guessing at it. Eight 8x8 blocks in the
	// extreme top-left corner, inside the margin the runtime crops away.
	g_vtVRFrameMarker.Init(g_pLTClient, "VRFrameMarker", LTNULL, 1.0f);

	// Submit each image with the pose the marker says it was rendered from,
	// instead of the newest pose. DEFAULT 1.
	//
	// Measured: staleness averages ~12 ms but ranges 6 to 67, so the spread is
	// larger than the mean. That is why no fixed VRPoseLag ever won a sweep,
	// and why the one earlier attempt at holding a pose across repeats was
	// unplayable - it had the right idea with the wrong pose. F7 off, F8 on.
	g_vtVRExactPose.Init(g_pLTClient, "VRExactPose", LTNULL, 1.0f);

	// A crosshair drawn in each eye. The game's own is drawn once across the
	// whole window and lands on the stereo seam, so in VR there effectively is
	// none. Drawn at the centre of each half, which is the view axis - correct
	// until motion controls aim the weapon independently of the head.
	g_vtVRCrosshair.Init(g_pLTClient, "VRCrosshair", LTNULL, 1.0f);

	// AND THE ONE THAT REPLACES IT ONCE THE HAND IS AIMING.
	//
	// The 2D crosshair above is drawn at the centre of each eye, which is
	// the VIEW axis. That was right while the gun followed the head. It is
	// wrong now: the weapon is aimed by the right controller, so the mark
	// on screen and the bullet go to different places, and the player has
	// no way to tell where the player is pointing.
	//
	// This one is a SPRITE PLACED IN THE WORLD at the point the gun's own
	// ray hits, built from CWeaponModel::GetFireInfo - the same call the
	// bullet is built from, so the two cannot disagree. VRAimMarker 0
	// turns it off and gives the 2D crosshair back.
	g_vtVRAimMarker.Init(g_pLTClient, "VRAimMarker", LTNULL, 1.0f);
	// Angular size. The dot is scaled with its distance so it stays the
	// same size on screen; a fixed world size vanishes across a room.
	//
	// 0.5 WAS TOO BIG BY A LOT. The art is Red1.spr, a soft flare rather than
	// a dot, so at 0.5 it reads as a glowing blob sitting over whatever you
	// are trying to shoot - an ugly, massive red dot. A red-dot sight wants
	// to be the smallest mark you can still find, and the clamp below wants to
	// stop it growing into a flare at long range, which the old ceiling of 6
	// allowed. 0.16 puts it at roughly a third the width it was.
	// 0.07: a sight wants to be a few pixels across a wall. 0.16 was
	// still large and distracting in the headset.
	g_vtVRAimMarkerSize.Init(g_pLTClient, "VRAimMarkerSize", LTNULL, 0.07f);
	// 1 = a hard red dot (Dot1.spr, ours), 0 = the game's soft flare.
	// 1, the red disc: the Quake II dot the tester keeps pointing at. 2 is the
	// yellow cross, 0 the flare.
	g_vtVRAimMarkerStyle.Init(g_pLTClient, "VRAimMarkerStyle", LTNULL, 1.0f);
	// Fixed WORLD size, in units (17 mm each). 1.0 is a real laser dot.
	g_vtVRAimMarkerWorld.Init(g_pLTClient, "VRAimMarkerWorld", LTNULL, 1.0f);
	// 2.0 units = 34 mm. At 1.0 the desk measured ONE PIXEL on both a near and
	// a far wall at 1280 px per eye - a real laser dot is too small to be a
	// sight. Quake II's reads as about a centimetre and a half at two metres,
	// which at the headset's pixel density needs this.
	g_vtVRAimMarkerWorldSize.Init(g_pLTClient, "VRAimMarkerWorldSize", LTNULL, 2.0f);
	g_vtVRAimMarkerMinSize.Init(g_pLTClient, "VRAimMarkerMinSize", LTNULL, 0.012f);
	// Stand the game's own 2D crosshair down while the world marker is up.
	// VRHideCrosshair 0 is the control arm - it puts both on screen at once,
	// which is also the only way to SEE the disagreement between them.
	g_vtVRHideCrosshair.Init(g_pLTClient, "VRHideCrosshair", LTNULL, 1.0f);

	// Point the weapon where the right controller points.
	//
	// DEFAULT 0 - the coordinate frame is not established yet.
	//
	// The weapon position is built as vNewPos += vU*y + vR*x + vF*z, from up,
	// right and forward basis vectors, and the comment above it says
	// "camera-relative". That was read as camera-LOCAL axes with X right, Y up,
	// Z forward, and a hand offset was added on that basis. It is wrong: the
	// gun could only be seen with the controller lying on a desk while looking
	// left, which is not a sign error in one axis but a different frame.
	//
	// Sign-flipping until it looks right is exactly the approach that has cost
	// this project its worst regressions. What is needed is to establish what
	// space vNewPos is in - whether SetObjectPos with LTTRUE means parent
	// relative, and what GetWeaponOffset's components mean - and then apply the
	// hand offset in that space. Rotation is likely fine; it is the translation
	// that is misplaced.
	// 0 = off, 1 = move with the hand, 2 = rotate with the hand, 3 = both.
	//
	// DEFAULT 1. The space question above is settled and the earlier note was
	// wrong: vU/vR/vF are taken from an IDENTITY rotation two lines before
	// their use, so they are (1,0,0), (0,1,0), (0,0,1) - plain camera-local
	// axes, exactly as the hand offset assumed.
	//
	// That leaves the rotation as the likely fault. The model turns about its
	// own origin, so if that origin is not where the geometry sits, rotating
	// swings the arms out of frame - which fits the symptom far better than a
	// misplaced translation. Hence the split: 1 and 2 fail in different and
	// obvious ways, so one look separates them.
	// DEFAULT 2 - rotate with the hand, swinging about the eye.
	//
	// Mode 1's free positional tracking is off by default: even
	// a small hand movement threw the model off screen. Anything that can
	// leave the frame at all is the wrong mechanism here, and no scale factor
	// repairs that - the authored distance from the face is what keeps the
	// weapon usable.
	//
	// Mode 2 gives what was actually asked for: aim the gun in any direction,
	// and see it in a natural place while doing so.
	g_vtVRHandAim.Init(g_pLTClient, "VRHandAim", LTNULL, 2.0f);
	// Units per metre for HAND MOVEMENT in the weapon's camera-relative
	// space. NOT the world's 58.75 - see the derivation in
	// WeaponModel.cpp. 58.75 restores the old behaviour exactly.
	// 3.46, NOT 3. The derivation in WeaponModel.cpp fitted "about 2.8" from
	// the authored offset and an assumed arm; the exact figure is the world
	// scale over the view-model push-out, 58.75 / 17 = 3.456, which makes a
	// centimetre of real hand travel a centimetre on screen. At 3.0 the gun
	// trailed the hand by 13% of its reach - 8 cm at arm's length - which
	// is the reported offset from where the hands are.
	g_vtVRHandPosScale.Init(g_pLTClient, "VRHandPosScale", LTNULL, 3.46f);
	// FIRE ALONG THE HAND. Only does anything when a host is attached and the
	// right controller is tracking, so a flat run is untouched by the default.
	g_vtVRHandFire.Init(g_pLTClient, "VRHandFire", LTNULL, 1.0f);
	// How far out to look for sprites. The query buffer holds 512 and the
	// Morocco quick save has 909, so an unbounded radius silently drops 44% of
	// them - including bullet holes. 4000 matches VRModelRange.
	g_vtVRSpriteRange.Init(g_pLTClient, "VRSpriteRange", LTNULL, 4000.0f);
	// THE PLACEMENT: the gun sits AT the controller, no authored screen
	// offset. The hand's displacement from the head IS the position.
	//
	// DEFAULT 1 SINCE 8 SEPTEMBER, and it now takes effect in the PUBLISH
	// path rather than on the object - see the long note in VRPublishModels.
	// Writing the position onto m_hObject never reached what is drawn, which
	// is why hand movement did nothing for so long and why the gun felt
	// pinned to the view instead of held.
	g_vtVRGunAtHand.Init(g_pLTClient, "VRGunAtHand", LTNULL, 1.0f);
	// Bend the player body's right arm to the controller. Needs +StubBody 1
	// on the renderer side to be worth anything - it poses an arm nobody is
	// drawing otherwise.
	g_vtVRArmIK.Init(g_pLTClient, "VRArmIK", LTNULL, 0.0f);
	// How far behind the head the body sits, in METRES. Derived from the
	// geometry: the eyes are at the front of the skull and a character model's
	// head is centred on its shoulders, so the body belongs about half a head's
	// depth behind the eyes - an adult head is roughly 19 cm front to back,
	// hence 0.09. A cvar because only a headset can judge the last centimetre.
	g_vtVRBodyBack.Init(g_pLTClient, "VRBodyBack", LTNULL, 0.09f);
	// Take the hand's ROLL as well as its yaw and pitch. Without it, turning
	// the controller over does nothing to the gun.
	g_vtVRHandRoll.Init(g_pLTClient, "VRHandRoll", LTNULL, 1.0f);
	// A pulse in the hand per shot (block v17). 0 turns it off.
	g_vtVRHaptics.Init(g_pLTClient, "VRHaptics", LTNULL, 1.0f);
	// The weapon wheel's size, a multiplier on its eye-fitted layout.
	g_vtVRWheelSize.Init(g_pLTClient, "VRWheelSize", LTNULL, 1.0f);
	// The effects channel: particles, rain, water, canvas polygons. Wraps two
	// engine entries, so it must run before any world loads. See VRPrims.h.
	VRPrims_Init();
	// Publish the FIRST-PERSON WEAPON, which FindObjectsInSphere never returns
	// because its position is camera-relative.
	//
	// DEFAULT 1 SINCE 8 SEPTEMBER. It was 0 for one reason - it changes the
	// publish path and a headset was wanted to judge it - and the cost of that
	// caution was that every headset session ever run showed the wrong gun.
	//
	// With this off the player is handed the THIRD-PERSON weapon: the model
	// other characters are seen holding, on a 128x128 skin, drawn from the
	// bottom centre of the view because that is where it sits on Cate's body.
	// With it on the player gets the model the game built for this purpose, on
	// a 256x256 skin, in the right hand. logs/viewmodel-ab.png is the pair and
	// the difference is not subtle.
	//
	// the gun models looked very low quality, and
	// the massive red dot was ugly. The first half of that was this switch
	// being off. VRViewModel 0 is the way back.
	g_vtVRViewModel.Init(g_pLTClient, "VRViewModel", LTNULL, 1.0f);
	// How far out of the near plane to push the view weapon, and how much to
	// scale it by to keep its angular size. See the derivation in the publish.
	g_vtVRViewModelScale.Init(g_pLTClient, "VRViewModelScale", LTNULL, 17.0f);
	g_vtVRVehicleBack.Init(g_pLTClient, "VRVehicleBack", LTNULL, 12.0f);	// arms run off the view at 50 deg down (desk, 24 Sep)
	g_vtVRVehicleBody.Init(g_pLTClient, "VRVehicleBody", LTNULL, 0.0f);
	g_vtVRVehicleBodyDown.Init(g_pLTClient, "VRVehicleBodyDown", LTNULL, 52.0f);	// wheels on the ground, bars in the hands (desk, 24 Sep)
	g_vtVRVehicleBodyFwd.Init(g_pLTClient, "VRVehicleBodyFwd", LTNULL, -3.0f);
	g_vtVRVehicleBodyDownSnow.Init(g_pLTClient, "VRVehicleBodyDownSnow", LTNULL, 68.0f);	// skis on the ground
	g_vtVRVehicleBodyFwdSnow.Init(g_pLTClient, "VRVehicleBodyFwdSnow", LTNULL, 0.0f);
	g_vtVRVehicleDrop.Init(g_pLTClient, "VRVehicleDrop", LTNULL, 16.0f);	// grips ~30 deg below the eye (desk, 24 Sep)
	g_vtVRVehicleStickPedals.Init(g_pLTClient, "VRVehicleStickPedals", LTNULL, 0.0f);
	// How BIG the view model is, independent of how far out it sits. K alone
	// cannot change the apparent size at all - see the note in the publish.
	// 1.4, NOT 1. At 1.0 the Walther draws at its true 174 mm and reads as
	// really small in the headset - a gun drawn visibly over life-size is
	// what reads as "held" in a headset. 1.4 is a guess at the proportion
	// that does; "Gun size" on the VR page is the lever.
	g_vtVRViewModelSize.Init(g_pLTClient, "VRViewModelSize", LTNULL, 1.4f);

	// How far from the eye the weapon sits, as a multiple of the game's own
	// offset. DEFAULT 2.5 because the player reports the silencer alone filling
	// the screen top to bottom. See the note at the use site: that is the
	// opposite of what the field of view predicts, so distance is a workaround
	// and the real cause is probably the weapon being drawn with a different
	// field from the world.
	g_vtVRWeaponDist.Init(g_pLTClient, "VRWeaponDist", LTNULL, 2.5f);


	// Per-weapon overrides from vrweapons.cfg, read AFTER the globals above so
	// the file can only add "@<weapon>" entries, never silently move a default
	// out from under them. A missing file is normal: every weapon uses the global.
	{
		// AND THE TUNER'S OWN FILE, AFTER IT, so a value tuned in the headset
		// beats a default for the same weapon.
		const LTRESULT resTune = g_pLTClient->ReadConfigFile("vrtune.cfg");
		VRLog::Msg("per-weapon tuning: vrtune.cfg %s",
				   (resTune == LT_OK) ? "loaded" : "absent (nothing tuned yet)");
		const LTRESULT res = g_pLTClient->ReadConfigFile("vrweapons.cfg");
		VRLog::Msg("per-weapon placement: vrweapons.cfg %s",
			(res == LT_OK) ? "loaded" : "not present - every weapon uses the global");
	}

	// Weapon model size. DEFAULT 0.4.
	//
	// Distance was not the lever - 2.5x further away changed the apparent size
	// not at all, which cannot happen under a perspective projection. With the
	// rotation working (so it is this object) and mode 1 having thrown it off
	// screen (so position is applied), the remaining explanation is that the
	// model is simply enormous, which is normal for a player-view weapon of
	// this era: authored oversized and placed close, because the flat game
	// draws it through a narrow field. A 120 degree field does not forgive
	// that.
	g_vtVRWeaponScale.Init(g_pLTClient, "VRWeaponScale", LTNULL, 0.4f);

	// EXPERIMENT (F1). Feed head rotation in as if it were mouse movement, and
	// tell the host to submit head-locked so the runtime reprojects nothing.
	//
	// Everything about the warping points at one comparison the player has made
	// repeatedly: mouse look feels correct, head look does not. The two differ
	// in exactly two ways - the camera path they take, and the fact that only
	// head look gives the runtime something to reproject against. This makes
	// head look take the mouse's path AND removes the reprojection, so if the
	// bending survives it is in the client and if it goes it is in what we
	// declare. See docs/HEAD-AS-MOUSE.md.
	//
	// DEFAULT 0. It is a diagnostic, and it costs two real things: head ROLL is
	// dropped (the mouse path has no roll axis) and the whole image lags the
	// head rigidly, because nothing is correcting for render-to-display time.
	g_vtVRHeadAsMouse.Init(g_pLTClient, "VRHeadAsMouse", LTNULL, 0.0f);

	// Runs the Get3DCameraPt field probe on the next frame, then clears itself.
	// It also runs once automatically per session, so the numbers are in every
	// log without anyone having to ask for them.
	g_vtVRProbeField.Init(g_pLTClient, "VRProbeField", LTNULL, 0.0f);

	// Sweeps the asked field and reports whether the renderer's tangent ratio
	// is constant. Off by default: it renders 20 extra frames in one frame's
	// time and would be a visible hitch if it ran every session.
	g_vtVRSweepField.Init(g_pLTClient, "VRSweepField", LTNULL, 0.0f);

	// Loads the quick save shortly after the main menu appears, so a
	// measurement that needs a WORLD can run unattended. The field calibration
	// correlates rendered pixels and there is nothing to correlate on a menu.
	//
	// +playdemo was tried first and is a trap: it puts the game into
	// GS_LOADINGLEVEL and leaves it there. A 73-second run sat in that state
	// spinning at 84,000 empty frames a second and wrote a 735 MB log.
	g_vtVRAutoQuickLoad.Init(g_pLTClient, "VRAutoQuickLoad", LTNULL, 0.0f);
	g_vtVRPauseLook.Init(g_pLTClient, "VRPauseLook", LTNULL, 1.0f);
	g_vtVRTele.Init(g_pLTClient, "VRTele", LTNULL, 0.0f);
	g_vtVRTeleX.Init(g_pLTClient, "VRTeleX", LTNULL, 0.0f);
	g_vtVRTeleY.Init(g_pLTClient, "VRTeleY", LTNULL, 0.0f);
	g_vtVRTeleZ.Init(g_pLTClient, "VRTeleZ", LTNULL, 0.0f);
	g_vtVRTeleAt.Init(g_pLTClient, "VRTeleAt", LTNULL, 4.0f);
	// THE LEVEL TOUR. play-vr -God passes 7; without the switch it passes 0,
	// because the engine writes every cvar back to autoexec.cfg at exit and
	// a tour must not leave god mode behind for an ordinary session.
	g_vtVRCheats.Init(g_pLTClient, "VRCheats", LTNULL, 0.0f);
	// THE UNATTENDED TOUR. See the VRTour block in Update: a list of places
	// in game/vrtour.txt, visited on a timer, so a night's run photographs
	// every level from inside its own rooms instead of from its spawn point.
	g_vtVRTour.Init(g_pLTClient, "VRTour", LTNULL, 0.0f);
	g_vtVRTourEvery.Init(g_pLTClient, "VRTourEvery", LTNULL, 6.0f);
	// THE EARS FOLLOW THE HEAD. See the VRHeadListener block in Update.
	g_vtVRHeadListener.Init(g_pLTClient, "VRHeadListener", LTNULL, 1.0f);
#if VR_DEBUG_TOOLS
	// VR_DEBUG_TOOLS: the testing switches, all reachable from Options > VR
	// and all off by default. Hold the RIGHT GRIP and press B to end a level.
	g_vtVRDebugSkip.Init(g_pLTClient, "VRDebugSkip", LTNULL, 0.0f);
	g_vtVRDebugGod.Init(g_pLTClient, "VRDebugGod", LTNULL, 0.0f);
	g_vtVRDebugClip.Init(g_pLTClient, "VRDebugClip", LTNULL, 0.0f);
	g_vtVRDebugNoAI.Init(g_pLTClient, "VRDebugNoAI", LTNULL, 0.0f);
	g_vtVRDebugMissions.Init(g_pLTClient, "VRDebugMissions", LTNULL, 0.0f);
	g_vtVRDebugArsenal.Init(g_pLTClient, "VRDebugArsenal", LTNULL, 0.0f);
	g_vtVRDebugPos.Init(g_pLTClient, "VRDebugPos", LTNULL, 0.0f);
#endif
	// VRFlashTune 1 turns on the numpad tuner and the on-screen readout.
	// VRFlashHold 1 keeps the flash lit continuously - a 50 ms flicker
	// cannot be aimed at, and tuning something you only glimpse is how
	// this effect has been slow to tune.
	g_vtVRMuzzleScale.Init(g_pLTClient, "VRMuzzleScale", LTNULL, 0.0f);
	g_vtVRFlashTune.Init(g_pLTClient, "VRFlashTune", LTNULL, 0.0f);
	g_vtVRFlashOffR.Init(g_pLTClient, "VRFlashOffR", LTNULL, 0.0f);
	g_vtVRFlashOffU.Init(g_pLTClient, "VRFlashOffU", LTNULL, 0.0f);
	g_vtVRFlashOffF.Init(g_pLTClient, "VRFlashOffF", LTNULL, 0.0f);
	g_vtVRFlashHold.Init(g_pLTClient, "VRFlashHold", LTNULL, 1.0f);
	g_vtVRLogPos.Init(g_pLTClient, "VRLogPos", LTNULL, 0.0f);
	g_vtVRDebugQuickSave.Init(g_pLTClient, "VRDebugQuickSave", LTNULL, 0.0f);
	// RELOAD ON A TIMER, so the desk can reproduce the one bug that has only
	// ever been triggered on the machine: reload the quick save from the menu
	// and the NPC skins scramble, differently every time. Off unless asked
	// for; +VRAutoReloadSecs 15 reloads every fifteen seconds of play.
	g_vtVRAutoReloadSecs.Init(g_pLTClient, "VRAutoReloadSecs", LTNULL, 0.0f);
	// LOAD A SECOND WORLD, ONCE, N SECONDS IN. +VRWorld2 names it.
	// The desk harness could only ever measure a level as the FIRST world
	// of a process, which is how a bug that only appears on the second one
	// survived a 103-level sweep. See tools/sweep-levels.ps1 -Anchor.
	g_vtVRWorld2At.Init(g_pLTClient, "VRWorld2At", LTNULL, 0.0f);
	// THE ELEVATOR'S PATH, NOT A CONSOLE LOAD. The first headset crash was the
	// HQ elevator: the level's own exit trigger, which goes MID_PLAYER_EXITLEVEL
	// to the server and back to ExitLevel(), through the loading screen and
	// its thread. VRWorld2 loaded by name, which is a different door. With
	// VRWorld2Exit 1 the one-shot leaves the way the game does, and with
	// VRWorld2Count N it does so N times, re-armed on each world entry - the
	// whole of a mission's scenes in one process, unattended.
	// RE-INITIALISE THE SOUND ENGINE WHEN A LEVEL ENDS.
	//
	// With sound ON the game crashes inside mss32.dll at a level transition,
	// twice at the IDENTICAL instruction (mss32+0x1BBD6), reading 16-bit
	// samples through a pointer that is no longer valid. OnExitWorld only
	// PAUSES sounds before the engine frees the level's audio, and a paused
	// voice still holds its buffer, so the next load walks freed data.
	//
	// The SDK has no kill-all - KillSound wants a handle per sound - so the
	// blunt instrument is a re-init, which drops every voice.
	//
	// HOW WELL THIS WORKS IS STILL OPEN. The first comparison said 3 crashes
	// in ten missions without it against 0 with it, and that comparison was
	// confounded: the arm without it lost window focus 45 times to my own
	// progress polling, and a focus loss reloads the render DLL and is where
	// DirectSound loses its buffers. A clean control is what decides this.
	// See the development notes.
	g_vtVRSoundReset.Init(g_pLTClient, "VRSoundReset", LTNULL, 1.0f);
	// THE MUZZLE FLASH'S TRANSFORM, with an off switch so it can be shown to
	// fail. A camera-relative model carries a CAMERA-SPACE position, which is
	// a few units from the map origin; 1 puts it back on the gun, 0 leaves it
	// where the engine put it. Without the 0 arm, a capture that shows a flash
	// at the muzzle proves nothing - it might have been there all along.
	g_vtVRCloseRebase.Init(g_pLTClient, "VRCloseRebase", LTNULL, 1.0f);
#if VR_DEBUG_TOOLS
	// Test only, and off by default: it takes the weapon choice away from the
	// player for the first frame of every world.
	g_vtVRDebugWeapon.Init(g_pLTClient, "VRDebugWeapon", LTNULL, 0.0f);
	// The failure screen and the Load folder after it cannot be reached at
	// the desk without dying. Fails the mission once, N wall-clock seconds
	// into the first world played. Off by default; a harness sets it.
	g_vtVRDebugFailAt.Init(g_pLTClient, "VRDebugFailAt", LTNULL, 0.0f);
	// A console command at the desk, e.g. +VRDebugCmd Trigger~h2oKF~on to start
	// a level's scripted event without playing to it. '~' stands for a space,
	// which the command line would otherwise split. Off by default.
	g_vtVRDebugCmdAt.Init(g_pLTClient, "VRDebugCmdAt", LTNULL, 0.0f);
	// A LEVEL STARTED WITH +runworld NEVER LEAVES ITS OPENING CAMERA.
	//
	// The client sits in the alternative-camera state for the whole level:
	// player camera in CHASE, the view weapon hidden AND disabled, and with it
	// disabled there can be no muzzle flash, no weapon animation and no
	// first-person effects at all. Every desk harness this project has - the
	// tour, the sweep, the chain, look-shot - starts levels that way, so all
	// of them have been photographing a state no player is ever in.
	//
	// Off by default because it ends a cinematic the player would watch. A
	// harness sets it; play never does.
	g_vtVRDebugEndCinematic.Init(g_pLTClient, "VRDebugEndCinematic", LTNULL, 0.0f);
	// ---- VRDebugFire: PULL THE TRIGGER AT THE DESK ---------------------
	//
	// Nothing at the desk could fire a gun, so the muzzle flash could only
	// ever be confirmed by asking the tester to put the headset on and shoot
	// something - and headset testing time is limited.
	// Worse, the harness reported "PV flash shown 0 times" run after run and
	// that was never evidence about the flash at all: every harness run sat in
	// the opening cinematic's CHASE camera with the view weapon DISABLED, so
	// there was no first-person weapon to flash. See VRDebugEndCinematic.
	//
	// With the camera fixed, this supplies the missing half. Off by default;
	// a harness sets it, play never does.
	g_vtVRDebugFire.Init(g_pLTClient, "VRDebugFire", LTNULL, 0.0f);
#endif
	// OFF, and it has to be: this trace was 185,304 of the 192,784 lines in
	// one chain run - 96% of the log, three lines a frame, written to disk
	// during play in every build. It answered "where did the gun go", that
	// question is closed, and it stays available for the next one.
	g_vtVRLogWeaponPos.Init(g_pLTClient, "VRLogWeaponPos", LTNULL, 0.0f);
	g_vtVRWorld2Exit.Init(g_pLTClient, "VRWorld2Exit", LTNULL, 0.0f);
	g_vtVRWorld2Count.Init(g_pLTClient, "VRWorld2Count", LTNULL, 1.0f);
	// The attachment measurement, not the attachment processing. Off by
	// default: it is per-attachment queries on every published model every
	// frame, and the processing itself is what the picture needs.
	g_vtVRAttachMeasure.Init(g_pLTClient, "VRAttachMeasure", LTNULL, 0.0f);
	// Process the attachments of WORLD MODELS - doors, gates, lifts - which
	// the model pass has always skipped on a type test. Default ON: without it
	// a door handle stays in the air when its door opens. VRWorldAttach 0 is
	// the control arm.
	// DEFAULT 1, AND THE MEASUREMENT THAT SAID 0 WAS ASKING THE WRONG
	// WITNESS. The first version tested for brush attachments with
	// ILTCommon::GetAttachments, got zero on every world model, and turned
	// this off. Measuring POSITIONS across the call instead of asking that
	// API: 216 objects move, all of them OT_MODEL, and they need correcting
	// again every frame - which is what an attachment whose parent nothing
	// updates looks like. Door handles, lock plates and padlocks are all
	// separate prop models in this game (PROPS/MODELS/DOORKNOB_01L.ABC,
	// LOCK.ABC, PLATE_01.ABC), which is the reported floating handle exactly.
	//
	// Gated on the parent having MOVED, so a level of standing-still doors
	// costs one position compare each rather than a ProcessAttachments each.
	g_vtVRWorldAttach.Init(g_pLTClient, "VRWorldAttach", LTNULL, 1.0f);

	// Controller input. On by default: without it there is no way to walk or
	// to work a menu in the headset, and the player cannot always reach a
	// keyboard. VRControls 0 is the A/B and the way back to keyboard only.
	g_vtVRControls.Init(g_pLTClient, "VRControls", LTNULL, 1.0f);
	g_vtVRStickDeadzone.Init(g_pLTClient, "VRStickDeadzone", LTNULL, 0.25f);
	// Snap turning, and how far each snap goes. Off by default: which of
	// the two suits somebody is a preference, and both are on the VR page.
	g_vtVRSnapTurn.Init(g_pLTClient, "VRSnapTurn", LTNULL, 0.0f);
	g_vtVRSnapTurnDeg.Init(g_pLTClient, "VRSnapTurnDeg", LTNULL, 30.0f);
	g_vtVRMenuRepeatMs.Init(g_pLTClient, "VRMenuRepeatMs", LTNULL, 250.0f);

	// Enumerate the engine's own objects once, and say what they are. The
	// renderer cannot see models at all, and the retail renderer's model phase
	// takes no arguments - it reads a global an earlier phase filled - so
	// there is no list to intercept on that side. The CLIENT has one through
	// the SDK, which is a far shorter road than reverse-engineering the
	// renderer's private lists.
	g_vtVRModelDump.Init(g_pLTClient, "VRModelDump", LTNULL, 0.0f);

	// Publish the engine's model list to the renderer every frame. On by
	// default: it costs one sphere query and is the only route models have.
	g_vtVRModels.Init(g_pLTClient, "VRModels", LTNULL, 1.0f);
	// THE FAR CLIP, NOT 4000. Retail draws a prop wherever the camera's far
	// plane reaches (FarZ 100000); 4000 units is 68 m, and in the open night
	// Morocco level the palms beyond it vanished and came back as the player
	// crossed the line (trees popping in and
	// out with distance). The publish sorts by distance before its cap, so the
	// near objects keep their places. Sprites keep VRSpriteRange 4000: their
	// 512-entry buffer is where a fresh bullet hole competes.
	g_vtVRModelRange.Init(g_pLTClient, "VRModelRange", LTNULL, 100000.0f);
	// -1: characters and bodies draw as far as the level's fog reaches (see
	// the publish); a positive number fixes it; 0 is no limit.
	g_vtVRModelRangeAnim.Init(g_pLTClient, "VRModelRangeAnimated", LTNULL, -1.0f);
	// OFF BY DEFAULT since the 24 September headset round: with it on, a body
	// froze mid-death away from where the character fell, door handles floated
	// where their doors had been closed, and bodies vanished. The key (position,
	// rotation, scale, main tracker) does not see everything that poses a model
	// - extra trackers, attachments posed by the engine. It saved 0.7 ms.
	g_vtVRNodeReuse.Init(g_pLTClient, "VRNodeReuse", LTNULL, 0.0f);
	// HOW MANY MODELS ARE PUBLISHED. The array holds VRMODELS_MAX_INST; this
	// says how much of it to fill, so both arms of the cap experiment live in
	// one build and one run. 192 is the shipped behaviour and stays the
	// default until the cost of raising it has been measured rather than
	// assumed - see the note in VRShared.h.
	// THE CAP, RAISED. It was 192 against a shared array of 512, and on the
	// Morocco save the sphere finds 244 objects - so 52 models near the player
	// were simply not drawn. What made 192 the number was the cost of the mesh
	// build, and that cost is now two thirds lower with the vertex cache, so
	// the reason for the low cap has gone. The array is the real ceiling.
	g_vtVRModelCap.Init(g_pLTClient, "VRModelCap", LTNULL,
						(float)VRMODELS_MAX_INST);

	// BLOOD, BULLET HOLES AND SHELL CASINGS THAT STAY.
	//
	// All three fade or expire on a timer NOLF chose for a 2000-era machine:
	// blood 5-10 seconds, marks 3 solid then 3 fading, casings 10-15. The tester
	// asked for them to stay, and the game is 25 years old.
	//
	// One switch for the three of them, because they are one decision. Each
	// still has its own cvar underneath (BloodSplatsMaxLifetime, MarkSolidTime,
	// MarkFadeTime) for anyone who wants a middle setting.
	g_vtVRPersistentFX.Init(g_pLTClient, "VRPersistentFX", LTNULL, 1.0f);
	// The main menu backdrop. On by default; it can only ADD to a screen
	// that is currently black behind the text.
	g_vtVRMenuModels.Init(g_pLTClient, "VRMenuModels", LTNULL, 1.0f);

	// THE MODERNIZER SHIPS AN HD MENU FONT AND NOTHING HAS EVER USED IT.
	//
	// The menu's POSITIONS scale with resolution - every folder lays out
	// through GetYRatio(), which is screen height over 480 - but the glyphs are
	// a BITMAP font blitted 1:1, so they do not. At 1384 tall the layout is
	// 2.88x the authored size and the text is still 28 pixels, which is why a
	// menu line measures 0.79 degrees in the headset and the player cannot read it.
	//
	// font_large_0_hd.pcx is 1981x52 against the original's 1096x28 - 1.857x -
	// and InterfaceResMgr has been LOADING it and returning the small one from
	// behind an "#if 1" since the fork. Enabling it is the whole fix for the
	// large font; the small, medium and title fonts have no HD art, which is
	// what the upstream comment there means.
	g_vtVRMenuHDFont.Init(g_pLTClient, "VRMenuHDFont", LTNULL, 1.0f);
	// DEFAULT 1 SINCE 8 SEPTEMBER. Sub-folders ask for a font size that has no
	// HD art behind it, so they came out about a third the size of the main
	// menu and unreadable in a headset. Desk-verified across the menu tree;
	// VRMenuBigSubs 0 is the way back if a page overruns its panel.
	g_vtVRMenuBigSubs.Init(g_pLTClient, "VRMenuBigSubs", LTNULL, 1.0f);
	g_vtVRGunCentre.Init(g_pLTClient, "VRGunCentre", LTNULL, 1.0f);
	g_vtVRRecoil.Init(g_pLTClient, "VRRecoil", LTNULL, 0.0f);
	// -14.2: measured from the Walther's own barrel nodes at the desk under the
	// RIGID placement (the gun's root frame, not the animated pose): with the
	// controller level and 11.6 of trim in, the barrel read 25.8 down, so the
	// trim that levels it is 11.6 - 25.8. Positive lowers the muzzle. Every
	// player-view model shares the hand rig, so one number.
	// 11.7: measured from the Walther's own barrel nodes at the desk under the
	// rigid placement WITH the root's rest orientation kept: controller level,
	// barrel +11.7 up, steady across reports. Positive lowers the muzzle.
	g_vtVRGunTrimPitch.Init(g_pLTClient, "VRGunTrimPitch", LTNULL, 11.7f);
	g_vtVRGunAnchor.Init(g_pLTClient, "VRGunAnchor", LTNULL, 2.0f);
	// 0 is the old rule: the first frame that reports W_IDLE.
	g_vtVRRootIdleFrames.Init(g_pLTClient, "VRRootIdleFrames", LTNULL, 20.0f);
	g_vtVRRootRecaptureDeg.Init(g_pLTClient, "VRRootRecaptureDeg", LTNULL, 8.0f);
	// The positive control for the retake: 0 = off, which is the default.
	g_vtVRRootCaptureSkew.Init(g_pLTClient, "VRRootCaptureSkew", LTNULL, 0.0f);
	g_vtVRGunRigid.Init(g_pLTClient, "VRGunRigid", LTNULL, 1.0f);
	g_vtVRGunAutoTrim.Init(g_pLTClient, "VRGunAutoTrim", LTNULL, 1.0f);
	g_vtVRPauseFreeze.Init(g_pLTClient, "VRPauseFreeze", LTNULL, 1.0f);
	g_vtVREyeClamp.Init(g_pLTClient, "VREyeClamp", LTNULL, 1.0f);
	g_vtVRCaptions.Init(g_pLTClient, "VRCaptions", LTNULL, 1.0f);

	// Take the eye separation from the headset's own reported IPD instead of a
	// fixed number.
	//
	// This is drift protection, not a correction. A headset's IPD wheel can be
	// mechanically loose and wander between 59 and 62 mm; it
	// is set to 62 and the range cannot be told apart by eye. At 62 mm the
	// derived value is 3.64 against the shipped 3.65, so on a normal day this
	// changes nothing. What it does is stop a run silently using a 5% different
	// stereo baseline because the wheel moved, which would otherwise be an
	// invisible confound in every A/B a tester is asked to judge.
	//
	// Falls back to the fixed VRIPD whenever the host is not live or the
	// reported value is implausible.
	g_vtVRIPDAuto.Init(g_pLTClient, "VRIPDAuto", LTNULL, 1.0f);

	// Millimetres per world unit. 1600 mm assumed eye height / 94 units
	// measured by raycast.
	g_vtVRUnitMM.Init(g_pLTClient, "VRUnitMM", LTNULL, 17.02f);

	// Apply the head's TRANSLATION to the camera, not only its rotation.
	//
	// Without this the mod is 3DoF: the camera turns with the head but never
	// moves with it. Yaw and roll survive that almost unharmed because the eyes
	// sit close to those axes. Pitch does not - a head pitches about the neck,
	// about 12 cm below the eyes, so looking up or down translates them several
	// centimetres, which is over three times the half-IPD offset that IS
	// applied. The resulting error is depth-dependent, moving near geometry far
	// more than distant geometry, which is exactly the reported effect of the
	// ground pulling toward the viewer and the tops of buildings leaning back.
	//
	// 0 restores the old rotation-only behaviour for an A/B.
	g_vtVRHeadPos.Init(g_pLTClient, "VRHeadPos", LTNULL, 1.0f);

	// How many finished frames to time a full back-buffer readback on, through
	// the proxy DDRAW.dll. Counts itself down, so setting it again re-arms.
	//
	// 30 is enough to separate a steady cost from a first-call outlier and cheap
	// enough to leave on: if the proxy is absent this does nothing at all, and
	// says so once.
	g_vtVRProbeReadback.Init(g_pLTClient, "VRProbeReadback", LTNULL, 30.0f);

	// Ask the renderer for EXACTLY the field we declare to the runtime, on both
	// axes, so the render-to-declare mapping is a uniform scale.
	//
	// Today it is not. The horizontal request is multiplied by
	// (screenW/viewportW) * VRFovXTest = 2.0 * 0.8 = 1.6 while the vertical is
	// multiplied by 1.0, so the runtime stretches the image 60% horizontally to
	// fill the frustum it was promised. A non-uniform scale is not a rigid
	// transform and its squash axis is locked to the HEAD, not the world - so
	// straight lines bend and swing as you look around, worst on roll. That is
	// the symptom this project has called "the warping" since M5, and it is the
	// same fault the Descent VR port found in sc_aspect and fixed.
	//
	// The 2.0 came from a belief that d3d.ren measures FOV against the whole
	// screen surface rather than the camera rect. Get3DCameraPt measured that
	// directly and it is false: the engine maps FOV to the VIEWPORT, exactly,
	// at every viewport width tested (44.96 / 44.92 / 44.85 degrees against
	// 45.00 asked). That measurement needs no render and is unaffected by the
	// two faults that broke the image-correlation instrument.
	//
	// Default 0 so nothing changes until it is judged in the headset. F10.
	g_vtVRFovUniform.Init(g_pLTClient, "VRFovUniform", LTNULL, 0.0f);

	// Capture N CONSECUTIVE NORMAL FRAMES, each with a known extra yaw, and
	// write each one out as an image.
	//
	// This replaces the instrument that produced four wrong answers and two
	// retractions (docs/FIELD-INSTRUMENT-BROKEN.md). Two faults killed that one
	// and both are avoided here by construction:
	//
	//   1. It drove Start3D/RenderCamera/End3D five times inside ONE frame.
	//      Measured, that yields one valid frame and then progressively broken
	//      ones - the fifth differed from the reference by 49.86 mean absolute,
	//      with whole walls missing. A projection cannot be measured from
	//      frames the renderer did not draw correctly. Here each sample is an
	//      ordinary frame the engine drew through its own path.
	//
	//   2. It correlated a statistic nobody ever looked at. Here the frames are
	//      written to disk as images and correlated offline, where they can be
	//      opened and where the sample band can be chosen to avoid the player's
	//      weapon - which has zero parallax and correlates perfectly with
	//      itself at zero shift.
	//
	// Frame k is yawed by k degrees, so the shift must be proportional to
	// tan(k). A set of samples that is not a straight line through the origin
	// is not measuring a projection, and that is checkable afterwards rather
	// than assumed.
	g_vtVRFieldRun.Init(g_pLTClient, "VRFieldRun", LTNULL, 0.0f);

	// Carry the player's body yaw into the reference space the HOST declares
	// poses in, so both ends describe the image in the same frame.
	//
	// This is the fix for the world bending, and the whole day's eliminations
	// point at it: stereo, head roll, anisotropy and the renderer's field were
	// all measured out, and the two configurations that do NOT bend are the two
	// where the compositor's correction is not misdirected - head-locked, which
	// removes the correction entirely, and VRQuatHead 2, which accidentally puts
	// the head rotation in the same frame the host declares in, at the cost of
	// swapping the head's own axes.
	//
	// At body yaw 0 those modes are identical to the correct one, and nothing
	// bends. Every degree of mouse turn away from that is a degree the
	// compositor turns the image about the wrong axis.
	//
	// 0 = off, shipped behaviour.  1 = on.  2 = on, opposite sign.
	//
	// DEFAULT 0: it changes what the host does with every frame and has never
	// been in a headset. The sign is a convention question between LithTech's
	// yaw and OpenXR's, and this project has guessed signs and frames wrongly
	// four times in one day - so both are offered and one look settles it.
	g_vtVRYawSpace.Init(g_pLTClient, "VRYawSpace", LTNULL, 0.0f);

	// Inject a synthetic head PITCH, in degrees, with no headset and no host.
	//
	// The question "does looking up show the sky, or roll the camera" is a
	// property of the rendered image, and the rendered image is on the desktop.
	// So it can be answered by a screenshot rather than by someone wearing a
	// headset and describing what they see - which is how it has been asked
	// four times without a definite answer.
	//
	// If the picture rolls, the fault is in the client's camera. If the picture
	// looks correctly at the sky, the client is innocent and the roll is added
	// afterwards, by the host's declaration or the compositor's reprojection.
	g_vtVRFakeHeadPitch.Init(g_pLTClient, "VRFakeHeadPitch", LTNULL, 0.0f);

	// Submit head-locked WITHOUT routing the head through the mouse path.
	//
	// These two were tied together in the head-as-mouse experiment and there is
	// no reason for it. That experiment changed the camera AND removed the
	// reprojection, and it did not bend - but it also dropped head roll and
	// mouse pitch, so it was never usable.
	//
	// The camera is not the problem. Injecting 35 degrees of head pitch through
	// the normal path and screenshotting the result shows the sky, a level
	// horizon and no roll at all. The roll appears only in the headset, which
	// puts it after the client - in the compositor's reprojection, which turns
	// the image about the axes of the pose we DECLARE while the image was drawn
	// about the axes of the camera, and those differ by the body yaw.
	//
	// Head-locked submission removes the reprojection entirely, so there is
	// nothing left to misdirect. The camera still follows the head, because the
	// client still composes it - so the world should simply be correct, at the
	// cost of the image being about one frame late rather than warped forward.
	g_vtVRHeadLocked.Init(g_pLTClient, "VRHeadLocked", LTNULL, 0.0f);

	// Settle the rotation convention with numbers, once, in every log.
	LogRotationConvention();
	LogHeadAxisTable();
	LogFrameAgreement();

	g_vtVRYawScale.Init(g_pLTClient, "VRYawScale", LTNULL, -1.0f);
	g_vtVRPitchScale.Init(g_pLTClient, "VRPitchScale", LTNULL, -1.0f);
	g_vtVRRollScale.Init(g_pLTClient, "VRRollScale", LTNULL, 1.0f);

	// Take the camera FOV from the headset when one is connected, instead of
	// deriving it from the window aspect. Required for the projection to match
	// what the runtime expects, or depth reads as subtly wrong.
	g_vtVRUseHeadsetFov.Init(g_pLTClient, "VRUseHeadsetFov", LTNULL, 1.0f);

	// Diagnostic. Multiplies ONLY the vertical FOV handed to SetCameraFOV.
	//
	// It is still unestablished whether this renderer honours the vertical FOV
	// it is given, or ignores it and derives vertical extent from the viewport
	// aspect. Those two models demand opposite corrections, and three attempts
	// to reason it out were wrong. Changing this on a monitor settles it: if
	// the vertical framing moves, the renderer honours fovY.
	g_vtVRFovYTest.Init(g_pLTClient, "VRFovYTest", LTNULL, 1.0f);

	// The vertical counterpart of VRFovXTest, and the axis nothing has ever
	// corrected.
	//
	// VRFovXTest multiplies the horizontal TANGENT before it is handed to
	// SetCameraFOV; VRFovYTest above multiplies the vertical ANGLE, which is a
	// different quantity and cannot cancel a tangent-space error. So the
	// vertical request has effectively gone to the renderer untouched since
	// M3, while the horizontal has been trimmed three times.
	//
	// That asymmetry is the shape of the remaining warping. If d3d.ren scales
	// the two axes by different amounts - and the horizontal is measurably
	// scaled, which is the entire reason VRFovXTest exists - then what reaches
	// the runtime is stretched on one axis relative to what we declare. A
	// non-uniform scale locked to the head is not a rigid transform: straight
	// lines bend and swing as you look around, worst away from centre. It is
	// invisible to VRFovXTest because that knob moves only one of the two.
	//
	// 1.0 is a no-op, so this ships inert until the measurement names a value.
	// VRSweepField measures it.
	g_vtVRFovYScale.Init(g_pLTClient, "VRFovYScale", LTNULL, 1.0f);

	// Horizontal FOV trim. DEFAULT 0.8, measured by the player.
	//
	// This deliberately renders NARROWER than what is declared to the runtime,
	// so the headset spreads the image wider - about a 1.25x horizontal
	// stretch. That is not geometric correctness, and it is intentional.
	//
	// NOLF at 16:9 is itself stretched: fovX 90 with fovY 78 describes a ~1.24
	// aspect, not 1.78, so the flat game has always looked roughly 1.4x wide.
	// A geometrically exact VR projection looks stretched tall to anyone
	// who knows the game, because it is the flat presentation that is wrong.
	// Set 1.0 for true geometry if that is ever wanted.
	// DEFAULT 0.70, MEASURED. A 5 degree calibration yaw shifted the image 36
	// px at a half-width of 800, giving a focal length of 411 px and a real
	// half-field of 62.8 degrees where 59.4 was being published - the renderer
	// draws 15% wider in tangent than we declare.
	//
	// That gap is an angular gain error, and it is why turning the head felt
	// like the world swinging around the player while mouse look felt correct:
	// a mismatch between declared and rendered field only shows up when the
	// head moves. 0.80 came from judging appearance while standing still,
	// which is the wrong criterion for it.
	// DEFAULT 0.5, which exactly cancels the screen/viewport factor of 2.
	//
	// That factor was the original mistake. It assumed d3d.ren measures
	// horizontal FOV against the whole screen surface while each eye renders
	// into half of it. The calibration says otherwise: asked for a 67.2 degree
	// half-field, the renderer produced 62.8 - near 1:1, against the 2:1 the
	// factor assumes. So the FOV applies to the VIEWPORT and we have been
	// rendering nearly twice the horizontal field we wanted.
	//
	// The symptom is an aspect mismatch, not just a width error. At 0.70 the
	// camera was set to 134.2 x 120.6 degrees, a tangent ratio of 1.343, into a
	// 1600x1660 viewport whose aspect is 0.964 - so everything was drawn 1.39x
	// too tall and too thin. That is the weapon looking like a blade, and it is
	// the stretched-tall look the player has reported since M3.
	//
	// At 0.5 the camera gets 119.0 x 120.6, a ratio of 0.968, which matches the
	// viewport. Every value tried before - 1.2, 1.0, 0.8, 0.7 - was above this,
	// so every one of them stretched, which is why none of them ever looked
	// right and why judging it by appearance could only ever pick the least bad.
	// BACK TO 0.8. 0.5 made it markedly worse - the world went short and wide
	// and head tilt warped diagonally again.
	//
	// 0.5 came from the calibration reading that the renderer produces roughly
	// what it is asked for, so the screen/viewport factor of 2 should cancel.
	// That reading had a correlation peak of 0.774 against a runner-up of
	// 0.771 - flagged as weak at the time, then relied on anyway. It should not
	// have been acted on without a second reading.
	//
	// The decisive observation is the player's next one: the distortion appears
	// IN THE HEADSET BUT NOT ON THE DESKTOP. The desktop shows the captured
	// image as rendered; the headset shows it after the runtime maps it into
	// the eye. If the desktop looks right, the render is right, and the fault
	// is in what we DECLARE - the published FOV or the submitted pose - not in
	// the camera at all.
	//
	// That also means VRFovXTest is the wrong knob for this, and every attempt
	// to fix it there was aimed at the wrong stage. Left at the value the player
	// has consistently judged least bad while the declaration is investigated.
	// DEFAULT 0.8, and do not change it again without a measurement taken IN
	// THE EYE VIEWPORT.
	//
	// 0.5 has now been shipped twice and rejected twice with the same observation:
	// markedly worse - world short and wide, head tilt warping diagonally
	// (15 August, dead4b6) and again on 2 September, where the player reported
	// bending on pitch and roll with yaw clean.
	//
	// Both times the argument for 0.5 was a measurement that could not see the
	// thing it was being used to disprove. The claim this knob encodes is that
	// d3d.ren maps horizontal FOV across the SCREEN SURFACE width rather than
	// the eye VIEWPORT width - a factor of screenW/viewportW = 2.0 that exists
	// only when the viewport is half the surface. FIELD-MEASURED-PROPERLY.md
	// correlated across the full screen width, where that factor is 1 by
	// construction, so it was silent about the horizontal rather than
	// contradicting it. A vertical measurement cannot see it either: the eye
	// viewport is the FULL screen height, so screenH/viewportH is 1 too.
	//
	// The measurement that would settle it renders the EYE viewport and yaws.
	// Until that exists, 0.8 is the value chosen by eye in the headset three
	// times, and the headset is the instrument this project actually has.
	g_vtVRFovXTest.Init(g_pLTClient, "VRFovXTest", LTNULL, 0.8f);

	// Quit cleanly after N seconds. Zero, the default, never quits.
	//
	// This exists so the SHUTDOWN path can be regression-tested without a
	// human and without a headset. NOLF's window does not act on WM_CLOSE, so
	// there was no way for a script to end a run except to kill the process -
	// and a killed process never unloads CShell.dll, which is precisely the
	// window the exit crash lived in. A test that cannot reach the bug cannot
	// show it fixed. See docs/EXIT-CRASH.md and tools/verify-exit.ps1.
	g_vtVRQuitAfter.Init(g_pLTClient, "VRQuitAfter", LTNULL, 0.0f);

	// Quit as soon as the field sweep has written its verdict.
	//
	// The sweep is an unattended measurement, but VRQuitAfter has to be set to
	// a generous wall-clock guess because how long the game takes to reach the
	// menu varies - one run loaded the save at 3.7 seconds and the next was
	// still on the intro at 20. Every second of that margin is a game window
	// on the player's screen. This ends the run at the moment the work is done.
	g_vtVRQuitAfterSweep.Init(g_pLTClient, "VRQuitAfterSweep", LTNULL, 0.0f);

	// Strip the window border and place the client at the top-left of the
	// screen. Without this the title bar and frame eat into the display, so the
	// render resolution has to stay below the monitor size and every eye is a
	// blurry upscale. Borderless lets the client be exactly the screen size.
	g_vtVRBorderless.Init(g_pLTClient, "VRBorderless", LTNULL, 1.0f);

	// 1 = move the window to the fastest attached display; 0 = leave it on the
	// primary. Only ever applied while the VR host is live, so the flat game
	// never disappears onto a display the player cannot see. See VRMonProc.
	//
	// DEFAULT 1, and this time for a measured reason.
	//
	// It was first added on the theory that a 60 Hz desktop capped the frame
	// rate. That was wrong - the client renders ~89 fps with FlipScreen at
	// 0.285 ms, so rendering is not display bound - and it was switched off.
	//
	// The display does bind something else. The host captures this window
	// through Windows Graphics Capture, which delivers a frame when the
	// COMPOSITOR presents the window, at the refresh rate of the display the
	// window is on. Measured on a 60 Hz monitor: 59.2 fresh frames per second
	// against 90 submitted, so 34% of the frames reaching the headset were
	// duplicates. That is the stutter, and no amount of rendering faster can
	// fix it - the frames were being drawn and then never handed over.
	g_vtVRWindowMonitor.Init(g_pLTClient, "VRWindowMonitor", LTNULL, 1.0f);

	// Render wider than the headset displays, so the runtime's reprojection has
	// real pixels to warp when the head moves between render and display.
	// Without margin it runs off the edge of the image and stretches the last
	// column instead - the smearing seen while turning. We render ~9 degrees
	// wider than needed at margin 1.0; 1.25 gives roughly 25 degrees.
	// DEFAULT 1.15.
	//
	// The Quest's frustum reaches 54 degrees outward and 55 down. Rendering
	// exactly the headset's extent leaves nothing at the edges: we declared
	// +/-51.8 horizontally and exactly +/-55 vertically, falling ~2 degrees
	// short on each outer edge and landing on the boundary at the bottom. The
	// runtime has no image there and shows black, which reads as looking at a
	// screen rather than being inside the world.
	//
	// A large uniform margin is the wrong cure. The real problem was the eye
	// images being the wrong SHAPE: the Quest frustum wants an aspect of
	// tan(54)/tan(55) = 0.964, and a 1280x1440 eye is 0.889 - too narrow. Too
	// little horizontal coverage, so margin was being used to paper over it and
	// throwing away sharpness in every direction at once.
	//
	// With a resolution whose per-eye aspect matches the headset, 1.05 is
	// enough: a few degrees for reprojection and nothing more.
	// 1.10: at 1.05 a sliver of black still reached the far periphery. Each
	// step costs sharpness, so this is deliberately the smallest value that
	// covers the view rather than a safe over-estimate.
	g_vtVRFovMargin.Init(g_pLTClient, "VRFovMargin", LTNULL, 1.10f);

	// Drive each renderer-visible operation a distinctive number of times so
	// the d3d.ren shim's per-slot counters name themselves. Desk-only; see
	// ProbeRendererSlots and docs/PHASE0-RENDERSTRUCT.md.
	g_vtVRRenProbe.Init(g_pLTClient, "VRRenProbe", LTNULL, 0.0f);

	// Framerate target for Modernizer's limiter, which is otherwise hardcoded
	// to 60. VR wants 90 or more: at 60 the headset repeats frames and relies
	// on reprojection to fill the gaps, which is exactly what smears.
	//
	// NOLF's animation timing is framerate sensitive - Modernizer locks to 60
	// partly for that reason - so if anything runs fast, set this back to 60.
	g_vtVRFramerate.Init(g_pLTClient, "VRFramerate", LTNULL, 90.0f);
	g_vtEnableLightScale.Init(g_pLTClient, "EnableLightScale", LTNULL, 1.0f);

	// Currently saved patch number, if the version changes we can do some upgradin'
	g_vtModPatchNum.Init(g_pLTClient, "ModPatchNum", NULL, 0.0f);

	//

	// Jake: This should fix any weird black box issues.
	g_pLTClient->RunConsoleString("optimizesurfaces 1");


    m_MoveMgr.Init();
	m_editMgr.Init();
	m_cheatMgr.Init();
	m_LightScaleMgr.Init();

    m_AttachButeMgr.Init(g_pLTClient);

	// Init the jukebox attribute manager
	m_JukeBoxButeMgr.Init(g_pLTClient);

	m_CameraOffsetMgr.Init();
	m_HeadBobMgr.Init();

    g_pLTClient->RegisterConsoleProgram("Cheat", CheatFn);
    g_pLTClient->RegisterConsoleProgram("Sunglass", SunglassFn);
    g_pLTClient->RegisterConsoleProgram("LeakFile", LeakFileFn);
//  g_pLTClient->RegisterConsoleProgram("Connect", ConnectFn);
    g_pLTClient->RegisterConsoleProgram("FragSelf", FragSelfFn);
    g_pLTClient->RegisterConsoleProgram("ReloadWeapons", ReloadWeaponAttributesFn);
    g_pLTClient->RegisterConsoleProgram("ReloadSurfaces", ReloadSurfacesAttributesFn);
    g_pLTClient->RegisterConsoleProgram("ReloadFX", ReloadFXAttributesFn);
    g_pLTClient->RegisterConsoleProgram("Record", RecordFn);
    g_pLTClient->RegisterConsoleProgram("PlayDemo", PlayDemoFn);
    g_pLTClient->RegisterConsoleProgram("InitSound", InitSoundFn);
    g_pLTClient->RegisterConsoleProgram("ExitLevel", ExitLevelFn);
    g_pLTClient->RegisterConsoleProgram("ChaseToggle", ChaseToggleFn);
    g_pLTClient->RegisterConsoleProgram("ChangeTeam", ChangeTeamFn);
    g_pLTClient->RegisterConsoleProgram("Music", MusicFn);

    g_pLTClient->SetModelHook((ModelHookFn)DefaultModelHook, this);

	// Make sure the save directory exists...
	if (!CWinUtil::DirExist("Save"))
	{
		CWinUtil::CreateDir("Save");
	}

	// Add to NumRuns count...

	float nNumRuns = 0.0f;
    HCONSOLEVAR hVar = g_pLTClient->GetConsoleVar("NumRuns");
	if (hVar)
	{
        nNumRuns = g_pLTClient->GetVarValueFloat(hVar);
	}
	nNumRuns++;

	char strConsole[64];
	sprintf (strConsole, "+NumRuns %f", nNumRuns);
    g_pLTClient->RunConsoleString(strConsole);

    LTBOOL bNetworkGameStarted = LTFALSE;

	// Initialize the renderer
    LTRESULT hResult = g_pLTClient->SetRenderMode(pMode);
	if (hResult != LT_OK)
	{
        g_pLTClient->DebugOut("%s Error: Couldn't set render mode!\n", GAME_NAME);

		RMode rMode;

		// If an error occurred, try 800x600x32...

		// This was 640x480...the game doesn't even support that low of a resolution afaik!
		rMode.m_Width		= 800;
		rMode.m_Height		= 600;
		rMode.m_BitDepth	= 32;
		rMode.m_bHardware	= pMode->m_bHardware;

		sprintf(rMode.m_RenderDLL, "%s", pMode->m_RenderDLL);
		sprintf(rMode.m_InternalName, "%s", pMode->m_InternalName);
		sprintf(rMode.m_Description, "%s", pMode->m_Description);

        g_pLTClient->DebugOut("Setting render mode to 800x600x32...\n");

        if (g_pLTClient->SetRenderMode(&rMode) != LT_OK)
		{
			// Okay, that didn't work, looks like we're stuck with software...

            rMode.m_bHardware = LTFALSE;

			sprintf(rMode.m_RenderDLL, "d3d.ren");
			sprintf(rMode.m_InternalName, "");
			sprintf(rMode.m_Description, "");

            g_pLTClient->DebugOut("Setting render mode to D3D Emulation mode...\n");

            if (g_pLTClient->SetRenderMode(&rMode) != LT_OK)
			{
                g_pLTClient->DebugOut("%s Error: Couldn't set to D3D Emulation mode.\nShutting down %s...\n", GAME_NAME, GAME_NAME);
                g_pLTClient->ShutdownWithMessage("%s Error: Couldn't set D3D Emulation mode.\nShutting down %s...\n", GAME_NAME, GAME_NAME);
				return LT_ERROR;
			}
		}
	}

	// Setup the global transparency color

	g_hColorTransparent = SETRGB_T(255,0,255);


	// Setup the music stuff...(before we setup the interface!)

    uint32 dwAdvancedOptions = m_InterfaceMgr.GetAdvancedOptions();

	if (!m_Music.IsInitialized() && (dwAdvancedOptions & AO_MUSIC))
	{
        m_Music.Init(g_pLTClient);
	}


	// Interface stuff...

	if (!m_InterfaceMgr.Init())
	{
		// Don't call ShutdownWithMessage since InterfaceMgr will have called
		// that, so calling it here will overwrite the message...
 		return LT_ERROR;
	}

    if (!m_PlayerSummary.Init(g_pLTClient))
	{
        char errorBuf[256];
		sprintf(errorBuf, "ERROR in CGameClientShell::OnEngineInitialized()\n\nCouldn't initialize PlayerSummaryMgr!");
        g_pLTClient->ShutdownWithMessage(errorBuf);
		return LT_ERROR;
	}

    if (!m_IntelItemMgr.Init())
	{
        char errorBuf[256];
		sprintf(errorBuf, "ERROR in CGameClientShell::OnEngineInitialized()\n\nCouldn't initialize IntelItemMgr!");
        g_pLTClient->ShutdownWithMessage(errorBuf);
		return LT_ERROR;
	}


	//init these here because it needs to access layout mgr through interface mgr
	s_fDeathDelay = g_pLayoutMgr->GetDeathDelay();
	g_vNVModelColor = g_pLayoutMgr->GetNightVisionModelColor();
	m_vNVScreenTint = g_pLayoutMgr->GetNightVisionScreenTint();
	m_vIRLightScale = g_pLayoutMgr->GetInfraredLightScale();
	g_vIRModelColor = g_pLayoutMgr->GetInfraredModelColor();

	m_weaponModel.Init();


	// Create the camera...

    uint32 dwWidth = 640;
    uint32 dwHeight = 480;

    g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &dwWidth, &dwHeight);

	ObjectCreateStruct theStruct;
	INIT_OBJECTCREATESTRUCT(theStruct);

	theStruct.m_ObjectType = OT_CAMERA;

    m_hCamera = g_pLTClient->CreateObject(&theStruct);
	_ASSERT(m_hCamera);

    g_pLTClient->SetCameraRect(m_hCamera, LTFALSE, 0, 0, dwWidth, dwHeight);
	SetCameraFOV(DEG2RAD(g_vtFOVXNormal.GetFloat()), DEG2RAD(g_vtFOVYNormal.GetFloat()));

	// Create the Interface camera...

    m_hInterfaceCamera = g_pLTClient->CreateObject(&theStruct);
	_ASSERT(m_hInterfaceCamera);

    g_pLTClient->SetCameraRect(m_hInterfaceCamera, LTFALSE, 0, 0, dwWidth, dwHeight);
    g_pLTClient->SetCameraFOV(m_hInterfaceCamera, DEG2RAD(g_vtInterfceFOVX.GetFloat()), DEG2RAD(g_vtInterfceFOVY.GetFloat()));

	// Initialize the global physics states...

	g_normalPhysicsState.m_vGravityAccel.Init(0.0f, -1000.0f, 0.0f);
	g_normalPhysicsState.m_fVelocityDampen = 0.5f;
	g_waterPhysicsState.m_vGravityAccel.Init(0.0f, -500.0f, 0.0f);
	g_waterPhysicsState.m_fVelocityDampen = 0.25f;

	// Player camera (non-1st person) stuff...

    if (m_PlayerCamera.Init(g_pLTClient))
	{
		InitPlayerCamera();
		m_PlayerCamera.GoFirstPerson();
	}
	else
	{
		CSPrint ("Could not init player camera!");
	}


	// Init the special fx mgr...
    if (!m_sfxMgr.Init(g_pLTClient))
	{
        g_pLTClient->ShutdownWithMessage("Could not initialize SFXMgr!");
		return LT_ERROR;
	}

	// Init the damage fx mgr...

    if (!m_DamageFXMgr.Init(g_pLTClient))
	{
        g_pLTClient->ShutdownWithMessage("Could not initialize DamageFXMgr!");
		return LT_ERROR;
	}


	// [blg] Check for a multiplayer launch...

	LTBOOL bMultiConnect = LTFALSE;
	LTBOOL bMultiLaunch  = LTFALSE;

	char* sConnect = NULL;

    if (hVar = g_pLTClient->GetConsoleVar("connect"))
	{
		sConnect = g_pLTClient->GetVarValueString(hVar);
		if (sConnect)
		{
			bMultiConnect = LTTRUE;
		}
	}

    if (hVar = g_pLTClient->GetConsoleVar("multiplayer"))
	{
		LTFLOAT fMulti = g_pLTClient->GetVarValueFloat(hVar);
		if (fMulti != 0.0f)
		{
			bMultiLaunch = LTTRUE;
		}
	}

	if (bMultiConnect || bMultiLaunch)
	{
		bNetworkGameStarted = LTTRUE;
	}


	// Okay, start the game...

	if (!bNetworkGameStarted)
	{
		HCONSOLEVAR hVar;

        if (hVar = g_pLTClient->GetConsoleVar("NumConsoleLines"))
		{
			// UNCOMMENT this for final builds...Show console output
			// for debugging...
            // g_pLTClient->RunConsoleString("+NumConsoleLines 0");
		}

        if (hVar = g_pLTClient->GetConsoleVar("runworld"))
		{
            if (!LoadWorld(g_pLTClient->GetVarValueString(hVar)))
			{
                HSTRING hString = g_pLTClient->FormatString(IDS_NOLOADLEVEL);
                g_pLTClient->ShutdownWithMessage(g_pLTClient->GetStringData(hString));
                g_pLTClient->FreeString(hString);
				return LT_ERROR;
			}
		}
		else if (GetConsoleInt("SkipTitle",0))
		{
			m_InterfaceMgr.SwitchToFolder(g_pInterfaceMgr->GetMainFolder());
		}
		else
		{
			m_InterfaceMgr.ChangeState(GS_SPLASHSCREEN);
		}
	}
	else
	{
		if (bMultiConnect && sConnect)
		{
			if (!DoJoinGame(sConnect))
			{
				m_InterfaceMgr.LoadFailed();
				m_InterfaceMgr.SwitchToFolder(g_pInterfaceMgr->GetMainFolder());
				HSTRING hString = g_pLTClient->FormatString(IDS_CANT_CONNECT_TO_SERVER);
				if (hString)
				{
					m_InterfaceMgr.ShowMessageBox(hString,LTMB_OK,LTNULL,LTNULL);
					g_pLTClient->FreeString(hString);
				}
			}
		}
		else if (bMultiLaunch)
		{
			m_InterfaceMgr.SwitchToFolder(g_pInterfaceMgr->GetMainFolder());
		}
		else
		{
			m_InterfaceMgr.ChangeState(GS_SPLASHSCREEN);
		}
	}



	// Determine how long it took to initialize the game...
	sprintf(strTimeDiff, "Game initialized in %f seconds.\n", CWinUtil::GetTime() - fStartTime);
	CWinUtil::DebugOut(strTimeDiff);

	int nDiff = GetConsoleInt("Difficulty",1);
	if (nDiff < GD_EASY)
		nDiff = GD_EASY;
	else if (nDiff > GD_VERYHARD)
		nDiff = GD_VERYHARD;

	m_eDifficulty = (GameDifficulty)nDiff;
	m_bFadeBodies = (LTBOOL)GetConsoleInt("FadeBodies",0);

	// Check for playdemo....

    if (hVar = g_pLTClient->GetConsoleVar("playdemo"))
	{
        DoLoadWorld("", NULL, NULL, LOAD_NEW_GAME, NULL, g_pLTClient->GetVarValueString(hVar));
	}

	// Quickfix in case this gets stuck on.
	g_pLTClient->RunConsoleString("CursorCenter 0");

	// Say hello to the cool folks who speedrun the game, and give them a little present!
	SDL_Log("Hello Speedrunners, I believe you need these: MissionPtr <%p> ScenePtr <%p>", &g_nCurrentMission, &g_nCurrentLevel);

	// Gonna do something silly, create a new loadingscreen, log the static variable address and kill the loading screen.
	CLoadingScreen* dummyLoad = new CLoadingScreen();
	dummyLoad->LogForSpeedRunners();
	delete(dummyLoad);
	dummyLoad = NULL;

	// Disclaimer so they don't complain about the addresses changing :)
	SDL_Log("Make sure the game isn't loading from the .rez file, otherwise these will change everytime you load the game!");

	// The constructor publishes itself as g_pDetourMgr, and OnEngineTerm()
	// deletes it through that. Do not let this pointer be the only one: it
	// goes out of scope on the next line.
	DetourMgr* detourMgr = new DetourMgr();
	detourMgr->Init();

	// Do some upgrading!
	if (g_vtModPatchNum.GetFloat() < g_pVersionMgr->GetLatestPatchVersion()) {

		// Ruin their controls by applying the defaults.
		if (g_vtModPatchNum.GetFloat() < 3.1f) { // 1.006 - 3.0 was beta release, 3.1 is main release

			// Bump up bulletholes - From AVP2 autoexec.cfg
			WriteConsoleFloat("BulletHoles", 100.0f);

			// Reset controls
			LTRESULT result = g_pLTClient->ReadConfigFile("defctrls.cfg");
			if (result != LT_ERROR)
			{
				ClearBindings();
				g_pLTClient->ReadConfigFile("defctrls.cfg");
			}

			SDL_Log("One time upgrade to 1.006! Controls reset.");
		}
	}

	WriteConsoleFloat("ModPatchNum", g_pVersionMgr->GetLatestPatchVersion());

	// Save any changes from the upgrade!
	g_pLTClient->WriteConfigFile("autoexec.cfg");


	return LT_OK;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::OnEngineTerm()
//
//	PURPOSE:	Called before the engine terminates itself
//				Handle object destruction here
//
// ----------------------------------------------------------------------- //

void CGameClientShell::OnEngineTerm()
{
    UnhookWindow();

	// Take the console detour out FIRST. It redirects the engine's console
	// print at lithtech.exe+0x41b880 into df_Console inside this DLL, and the
	// engine keeps printing while it shuts down - so if the detour is still
	// installed when CShell.dll unloads, the next print jumps into freed
	// memory. It was: DetourMgr was leaked to a local that went out of scope,
	// so ~DetourMgr and its Term() had never run on any build. Every run this
	// project made between July and August 2026 ended in an access violation
	// at cshell.dll_unloaded+0x6d9b0, which symbolizes to df_Console.
	// See docs/EXIT-CRASH.md.
	if (g_pDetourMgr)
	{
		delete g_pDetourMgr;		// ~DetourMgr -> Term() -> DetourDetach
		g_pDetourMgr = NULL;		// the destructor clears this too
	}

	if (m_hCamera)
	{
        g_pLTClient->DeleteObject(m_hCamera);
        m_hCamera = LTNULL;
	}

	if (m_hInterfaceCamera)
	{
        g_pLTClient->DeleteObject(m_hInterfaceCamera);
        m_hInterfaceCamera = LTNULL;
	}

	m_InterfaceMgr.Term();

	m_Music.Term();
	m_LightScaleMgr.Term();

	if (m_hEyeStash)
	{
		g_pLTClient->DeleteSurface(m_hEyeStash);
		m_hEyeStash = LTNULL;
	}
	if (m_hMarkerOn)  { g_pLTClient->DeleteSurface(m_hMarkerOn);  m_hMarkerOn  = LTNULL; }
	if (m_hMarkerOff) { g_pLTClient->DeleteSurface(m_hMarkerOff); m_hMarkerOff = LTNULL; }
	if (m_hMarkerAlt) { g_pLTClient->DeleteSurface(m_hMarkerAlt); m_hMarkerAlt = LTNULL; }

	VRLog::Msg("OnEngineTerm");
	VRLog::Shutdown();
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::OnEvent()
//
//	PURPOSE:	Called for asynchronous errors that cause the server
//				to shut down
//
// ----------------------------------------------------------------------- //

void CGameClientShell::OnEvent(uint32 dwEventID, uint32 dwParam)
{
	switch(dwEventID)
	{
		// Client disconnected from server.  dwParam will
		// be a error flag found in de_codes.h.

		case LTEVENT_DISCONNECT :
		{
			if (g_pGameClientShell->IsMultiplayerGame())
			{
				auto nCode = g_pGameClientShell->GetDisconnectCode();

				//
				// TIMEOUT BUG!
				// So for unknown reasons sometimes folks immediately timeout between server level changes
				// So uhh, if we hit that..request a reconnect by filling up our m_sRetryAddress.
				// We need a partial update before we can reconnect, so we can't do it here :(
				//
				if (nCode == LT_DISCON_TIMEOUT)
				{
					// So we don't hit an infinite loop...
					if (m_nTimeoutBugRetriesLeft > 0)
					{
						m_nTimeoutBugRetriesLeft--;

						g_pGameClientShell->ClearDisconnectCode();
						g_pInterfaceMgr->ClearAllScreenBuffers();
						g_pInterfaceMgr->StartingNewGame();

						auto szAddress = g_pGameClientShell->GetServerAddress();
						auto nPort = m_nServerPort;

						// Do all the usual stuff we do when we disconnect
						m_szServerAddress[0] = LTNULL;
						m_nServerPort = -1;
						m_szServerName[0] = LTNULL;
						memset(m_fServerOptions, 0, sizeof(m_fServerOptions));
						m_bInWorld = LTFALSE;

						g_pLTClient->CPrint("[Attempt %d] Possible connection timeout bug, reconnecting...", m_nTimeoutBugRetriesLeft);

						char sIp[MAX_SGR_STRINGLEN] = { "" };
						sprintf(sIp, "%s:%d", szAddress, nPort);

						m_sRetryAddress = sIp;
						
						return;
					}
				}

				m_szServerAddress[0]	= LTNULL;
				m_nServerPort			= -1;
				m_szServerName[0]		= LTNULL;
				memset(m_fServerOptions, 0, sizeof(m_fServerOptions));
			}
			m_bInWorld = LTFALSE;

		} break;

        case LTEVENT_LOSTFOCUS:
		{
			m_bMainWindowFocus = FALSE;
		}
		break;

		case LTEVENT_GAINEDFOCUS:
		{
			m_bMainWindowFocus = TRUE;
		}
		break;

        case LTEVENT_RENDERTERM:
		{
			m_bMainWindowFocus = FALSE;
		}
		break;

		case LTEVENT_RENDERINIT:
		{
			m_bMainWindowFocus = TRUE;

			if (!g_hMainWnd)
			{
				HookWindow();
			}

			// Clip the cursor if we're NOT in a window...

            HCONSOLEVAR hVar = g_pLTClient->GetConsoleVar("Windowed");
			BOOL bClip = TRUE;
			if (hVar)
			{
                float fVal = g_pLTClient->GetVarValueFloat(hVar);
				if (fVal == 1.0f)
				{
					bClip = FALSE;
				}
			}

			if (bClip)
			{
				if (!g_prcClip)
				{
					g_prcClip = debug_new(RECT);
				}

				GetWindowRect(g_hMainWnd, g_prcClip);
				ClipCursor(g_prcClip);
			}

			g_pInterfaceMgr->InitCursor();

			// Slight hack: In windowed mode, the window gets repositioned incorrectly on re-focus.
			// Just centre it again.
			SDL_SetWindowPosition(g_SDLWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		}
		break;
	}

	m_InterfaceMgr.OnEvent(dwEventID, dwParam);
}


LTRESULT CGameClientShell::OnObjectMove(HOBJECT hObj, LTBOOL bTeleport, LTVector *pPos)
{
	return m_MoveMgr.OnObjectMove(hObj, bTeleport, pPos);
}


LTRESULT CGameClientShell::OnObjectRotate(HOBJECT hObj, LTBOOL bTeleport, LTRotation *pNewRot)
{
	return m_MoveMgr.OnObjectRotate(hObj, bTeleport, pNewRot);
}


LTRESULT CGameClientShell::OnTouchNotify(HOBJECT hMain, CollisionInfo *pInfo, float forceMag)
{
	m_sfxMgr.OnTouchNotify(hMain, pInfo, forceMag);
	return LT_OK;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::PreLoadWorld()
//
//	PURPOSE:	Called before world loads
//
// ----------------------------------------------------------------------- //

void CGameClientShell::PreLoadWorld(char *pWorldName)
{
	if (IsMainWindowMinimized())
	{
//		NetStart_RestoreMainWnd();
		RestoreMainWindow();
	}

	SAFE_STRCPY(m_strCurrentWorldName, pWorldName);
	// Which level a headset report came from: nothing else in either log names it.
	VRLog::Msg("VRWorldName: loading '%s'", pWorldName ? pWorldName : "");
	s_VRNodeCache.clear();		// object handles are recycled across levels
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::OnEnterWorld()
//
//	PURPOSE:	Handle entering world
//
// ----------------------------------------------------------------------- //

void CGameClientShell::OnEnterWorld()
{
	g_pLTClient->ResumeSounds();

	g_pPhysicsLT->SetStairHeight(DEFAULT_STAIRSTEP_HEIGHT);

	m_bFirstUpdate = LTTRUE;
	m_ePlayerState = PS_UNKNOWN;

	m_vShakeAmount.Init();

    m_bPlayerPosSet = LTFALSE;
    m_bInWorld      = LTTRUE;
	++g_nVRWorldEntries;
#if VR_DEBUG_TOOLS
	g_bVRCineEnded = false;
#endif
	m_nZoomView		= 0;
    m_bZooming      = LTFALSE;
    m_bZoomingIn    = LTFALSE;

	m_eCurContainerCode		= CC_NO_CONTAINER;

	m_bCameraAttachedToHead = LTFALSE;

	m_LightScaleMgr.Init();
	m_InterfaceMgr.AddToClearScreenCount();

    g_pLTClient->ClearInput();

	m_HeadBobMgr.OnEnterWorld();
	m_InterfaceMgr.OnEnterWorld(m_bRestoringGame);

    SetExternalCamera(LTFALSE);

    m_bRestoringGame        = LTFALSE;
    m_bCameraPosInited      = LTFALSE;
	m_nPlayerInfoChangeFlags |= CLIENTUPDATE_PLAYERROT | CLIENTUPDATE_ALLOWINPUT;

	m_vLastReverbPos.Init();

	m_MoveMgr.OnEnterWorld();

	// Reset our retries!
	m_nTimeoutBugRetriesLeft = MAX_TIMEOUT_RETRIES;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::OnExitWorld()
//
//	PURPOSE:	Handle exiting the world
//
// ----------------------------------------------------------------------- //

static void VRVehBodyForget(bool bDelete);

void CGameClientShell::OnExitWorld()
{
	VRVehBodyForget(false);		// the engine deletes client objects with the world
    g_pLTClient->PauseSounds();

    m_bInWorld      = LTFALSE;
    m_bStartedLevel = LTFALSE;

	m_LightScaleMgr.Term();
	ClearScreenTint();
    HandleZoomChange(m_weaponModel.GetWeaponId(), LTTRUE);
	EndZoom();
	m_InterfaceMgr.EndUnderwater();

	memset(m_strCurrentWorldName, 0, 256);

	if (m_h3rdPersonCrosshair)
	{
        g_pLTClient->DeleteObject(m_h3rdPersonCrosshair);
        m_h3rdPersonCrosshair = LTNULL;
	}

	// The aim marker is a world object and does not survive the world.
	if (m_hVRAimMarker)
	{
        g_pLTClient->DeleteObject(m_hVRAimMarker);
        m_hVRAimMarker = LTNULL;
	}
	m_bVRAimMarkerOn = LTFALSE;

	m_DamageFXMgr.Clear();					// Remove all the sfx
	m_sfxMgr.RemoveAll();					// Remove all the sfx

	// AND DROP EVERY VOICE, not merely pause it. See VRSoundReset above: a
	// paused voice keeps its sample buffer, the engine frees the level's
	// audio after this returns, and the next world load reads it. Done after
	// RemoveAll so the SFX have already released whatever they own.
	if (g_vtVRSoundReset.GetFloat() > 0.0f)
	{
		VRLog::Msg("VRSound: level over - re-initialising the sound engine so no"
				   " paused voice survives into the next world");
		InitSound();
	}
    m_PlayerCamera.AttachToObject(LTNULL);   // Detatch camera

	m_weaponModel.Reset();
	m_FlashLight.TurnOff();

	if (m_hContainerSound)
	{
        g_pLTClient->KillSound(m_hContainerSound);
        m_hContainerSound = LTNULL;
	}

	m_InterfaceMgr.OnExitWorld();

	m_MoveMgr.OnExitWorld();
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::PreUpdate()
//
//	PURPOSE:	Handle client pre-updates
//
// ----------------------------------------------------------------------- //

void CGameClientShell::PreUpdate()
{
	// Conditions in which we don't want to clear the screen

	if ((m_ePlayerState == PS_UNKNOWN && m_bInWorld))
	{
		return;
	}

	// See if we're using an external camera now - if so, clear the screen
	// immediately, and add to the clearscreen count
	if (m_bUsingExternalCamera && !m_bWasUsingExternalCamera)
	{
        m_bWasUsingExternalCamera = LTTRUE;
		m_InterfaceMgr.AddToClearScreenCount();
	}
	else if (m_bWasUsingExternalCamera && !m_bUsingExternalCamera)
	{
        m_bWasUsingExternalCamera = LTFALSE;
		m_InterfaceMgr.AddToClearScreenCount();
	}

	// Big Head Mode is neat and all, but we need to keep multiplayer consistent. 
	if (IsMultiplayerGame()) {
		g_vtBigHeadMode.SetFloat(0.0f);
	}

	m_InterfaceMgr.PreUpdate();
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::VRUpdateControllerInput()
//
//	PURPOSE:	Drive the game from the headset's controllers
//
// ----------------------------------------------------------------------- //

namespace
{
	// Every command the sticks and buttons can HOLD. One list, so that releasing
	// all of them - controller lost, menu opened, feature switched off - is a
	// single loop that cannot miss one. A held command that is never released is
	// a player walking into a wall until the process exits, and in a headset it
	// would read as a physics bug rather than an input bug.
	// The commands CMoveMgr POLLS. Activate and reload are not here: they are
	// acted on in CGameClientShell::OnCommandOn's switch, so for them the
	// notification IS the mechanism and an edge is all they need.
	const int kVRHeld[] = {
		COMMAND_ID_FORWARD,		COMMAND_ID_REVERSE,
		COMMAND_ID_STRAFE_LEFT,	COMMAND_ID_STRAFE_RIGHT,
		COMMAND_ID_LEFT,		COMMAND_ID_RIGHT,
		COMMAND_ID_RUN,			COMMAND_ID_DUCK,
		COMMAND_ID_JUMP,		COMMAND_ID_FIRING,
	};
	const int kVRHeldCount = sizeof(kVRHeld) / sizeof(kVRHeld[0]);
	bool g_bVRHeldOn[kVRHeldCount] = { false };
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::VRPublishModels()
//
//	PURPOSE:	Hand the renderer the engine's model list, once a frame
//
// ----------------------------------------------------------------------- //

// THE RENDER-RELEVANT OBJECT FLAGS THIS PORT DOES NOT ACT ON, counted so the
// group can be judged at once instead of one art bug at a time. See the note
// at the increment site.
static uint32 s_nFlagSeen = 0, s_nFlagClose = 0, s_nFlagTint = 0;
static uint32 s_nFlagEnvMap = 0, s_nFlagDetail = 0, s_nFlagShadow = 0;
static uint32 s_nFlagAnimTrans = 0, s_nFlagPortal = 0;


// Rotate one bone's node transform from where it pointed to where it now
// points, KEEPING its own orientation.
//
// The mesh is bound expecting each node's orientation, so replacing it outright
// twists the arm even when all three joints land in exactly the right places.
// What is wanted is the delta: the rotation taking the old bone direction to
// the new one, applied on top of whatever the animation had.
//
// The published node is a 3x4 ROW-MAJOR matrix with the translation in the
// fourth COLUMN - m[3], m[7], m[11] - which is the layout VRShared.h warns
// about, and reading it the other way once cost a whole reconstruction.
static void VRArmBone(VRModelNode& n,
					  const LTVector& vOldA, const LTVector& vOldB,
					  const LTVector& vNewA, const LTVector& vNewB)
{
	LTVector o = vOldB - vOldA, w = vNewB - vNewA;
	if (o.Mag() < 0.0001f || w.Mag() < 0.0001f) return;
	o.Norm(); w.Norm();

	LTVector ax = o.Cross(w);
	const float fS = ax.Mag();
	float fC = o.Dot(w);
	if (fC > 1.0f) fC = 1.0f;
	if (fC < -1.0f) fC = -1.0f;
	if (fS < 0.0001f)
	{
		if (fC > 0.0f) return;			// already aligned
		// Exactly opposed: any perpendicular axis will do.
		ax = LTVector(o.y, -o.x, 0.0f);
		if (ax.Mag() < 0.0001f) ax = LTVector(0.0f, -o.z, o.y);
	}
	ax.Norm();
	const float ang = (float)atan2(fS, fC);
	const float c = (float)cos(ang), si = (float)sin(ang), t = 1.0f - c;

	// Rodrigues, as a 3x3 in the same row-major sense as the node.
	const float R[9] = {
		t*ax.x*ax.x + c,        t*ax.x*ax.y - si*ax.z,  t*ax.x*ax.z + si*ax.y,
		t*ax.x*ax.y + si*ax.z,  t*ax.y*ax.y + c,        t*ax.y*ax.z - si*ax.x,
		t*ax.x*ax.z - si*ax.y,  t*ax.y*ax.z + si*ax.x,  t*ax.z*ax.z + c };

	const float M[9] = { n.m[0], n.m[1], n.m[2],
						 n.m[4], n.m[5], n.m[6],
						 n.m[8], n.m[9], n.m[10] };
	float O[9];
	for (int r = 0; r < 3; ++r)
		for (int cc = 0; cc < 3; ++cc)
			O[r*3+cc] = R[r*3+0]*M[0*3+cc] + R[r*3+1]*M[1*3+cc]
					  + R[r*3+2]*M[2*3+cc];
	n.m[0]=O[0]; n.m[1]=O[1]; n.m[2]=O[2];
	n.m[4]=O[3]; n.m[5]=O[4]; n.m[6]=O[5];
	n.m[8]=O[6]; n.m[9]=O[7]; n.m[10]=O[8];
}



// THE FOLDERS RETAIL DRAWS AS A FULL CARD, with no world behind them. Every
// other folder that can be up with a world loaded is the pause family, and
// the pause menu keeps the world by this port's own choice (the project rules, M6).
bool VRIsCardFolder(eFolderID eId)
{
	switch (eId)
	{
	case FOLDER_ID_BRIEFING:
	case FOLDER_ID_OBJECTIVES:
	case FOLDER_ID_WEAPONS:
	case FOLDER_ID_GADGETS:
	case FOLDER_ID_MODS:
	case FOLDER_ID_GEAR:
	case FOLDER_ID_INVENTORY:
	case FOLDER_ID_VIEW_INV:
	case FOLDER_ID_MISSION:
	case FOLDER_ID_STATS:
	case FOLDER_ID_INTEL:
	case FOLDER_ID_SUMMARY:
	case FOLDER_ID_AWARDS:
	case FOLDER_ID_FAILURE:
		return true;
	default:
		return false;
	}
}

// THE INTERFACE SCENE IS THE WHOLE PICTURE in three cases, not one.
//
// 1. A full-card folder (VRIsCardFolder).
// 2. ANY folder once the player has left the world. A failed mission calls
//    SetPlayerNotInWorld, so from the failure screen on the host shows a flat
//    panel (PublishInMenu) - and the Load and Main menu folders reached from
//    there were treated as the pause menu: the level drawn inside that panel
//    and their card left out. Retail draws each folder's own card there
//    (FolderLoad in LayoutNew.txt: WeaponBackgroundSpr1, the green text
//    boxes, the flower), and the unselected items are drawn black ON that
//    card; over a dark street they could not be read.
// 3. The loading screen, while VRPublishLoadingScreen publishes it.
// SQUARED UP BEFORE A CARD IS BUILT, not after. Every folder places its 3D
// pieces - the briefing's orange sheet, the flower - in front of the interface
// camera AS IT IS when the folder opens (CreateInterfaceSFX reads its
// rotation). The pause-look turns that camera with the head, and the square-up
// below in the update loop ran the frame AFTER the new folder was built, so a
// briefing opened from a loaded level's pause menu put its card wherever the
// head had been, and then the camera turned away from it: a black screen with
// only the 2D title and arrows. Reproduced at the desk along that exact path
// (level, Esc, Main Menu, New Game, briefing): card sprite at (-150 0 0), camera
// squared to face +Z. Only for card scenes - the pause family keeps its pieces
// in front of the turned camera, as before.
void CGameClientShell::VRSquareInterfaceCameraFor(int nFolderId)
{
	if (!s_bVRIfaceCamTurned || !m_hInterfaceCamera) return;
	if (!VRIsCardFolder((eFolderID)nFolderId) && IsPlayerInWorld()) return;
	LTRotation rStraight;
	g_pLTClient->SetupEuler(&rStraight, 0.0f, 0.0f, 0.0f);
	g_pLTClient->SetObjectRotation(m_hInterfaceCamera, &rStraight);
	s_bVRIfaceCamTurned = false;
	VRLog::Msg("VRPauseLook: interface camera squared up before folder %d built its card", nFolderId);
}

bool CGameClientShell::VRCardScene()
{
	if (m_bVRLoadPublish) return true;
	if (m_InterfaceMgr.GetGameState() != GS_FOLDER) return false;
	return VRIsCardFolder(m_InterfaceMgr.GetCurrentFolder()) || !IsPlayerInWorld();
}

// THE LOADING SCREEN IS DRAWN BY ANOTHER THREAD, and nothing published it.
//
// CLoadingScreen runs its own thread that renders the orange card (the
// LoadScreenSingle interface objects: LoadingSpr1, the pinwheel, the
// henchmen, the picture box) through the interface camera while the main
// thread is blocked loading the level. The model and sprite publish, the
// interface-only flag, the 2D folder id and the host's flat-panel flag are
// all sent from this thread's Update - which does not run during a load. So
// the card never reached the renderer, the flags kept the values of the last
// frame played, and the renderer drew the old level behind the photo and the
// title with the lime shade bars either side (headset report: only part of
// the loading screen, and the level behind it).
//
// Published ONCE, here, on the main thread, before the loading thread starts
// - never from that thread, which would race the level load for the engine's
// object lists. The renderer keeps the last frame it was given, so the card
// stays up for the whole load; the pinwheel holds still where retail turns it.
void CGameClientShell::VRPublishLoadingScreen()
{
	HMODULE hR = GetModuleHandleA("d3dstub.ren");
	if (!hR) return;
	typedef void (__cdecl *VRFolderFn)(int);
	VRFolderFn pfnFold = (VRFolderFn)GetProcAddress(hR, "R3D_PublishFolder2D");
	VRFolderFn pfnCard = (VRFolderFn)GetProcAddress(hR, "R3D_PublishInterfaceOnly");
	if (pfnFold) pfnFold(999);		// the splash/loading id, as the Update sends it
	if (pfnCard) pfnCard(1);
	VRShared::PublishInMenu(true);
	m_bVRLoadPublish = true;
	VRPublishModels();
	m_bVRLoadPublish = false;
	static int s_nSaidLoad = 0;
	if (s_nSaidLoad < 6)
	{
		++s_nSaidLoad;
		VRLog::Msg("VRLoadScreen: published the loading screen, %d interface objects,"
				   " interface-only %s", g_nVRIfaceObjs, pfnCard ? "sent" : "MISSING");
	}
}

// THE WHOLE VEHICLE UNDER THE RIDER (VRVehicleBody, default 0).
//
// The ridden view models (moto_pv, snow_pv) are only the bars, the dash and
// the arms: on a monitor nothing below them shows. In a headset the rider looks
// down and floats (24 September). The parked model - Props\Models\Motorcycle
// or Snowmobile, what the level shows before you mount - is drawn under the
// eye, turned with the body, and the ridden model is cut to its hands
// (VRMODEL_F_HANDSONLY) so there is one set of bars. A plain client object, so
// the sphere publishes it like any prop. Placed from the camera each frame.
static HOBJECT s_hVRVehBody = LTNULL;
static int     s_nVRVehBodyKind = 0;		// 1 motorcycle, 2 snowmobile

static void VRVehBodyForget(bool bDelete)
{
	if (s_hVRVehBody && bDelete) g_pLTClient->DeleteObject(s_hVRVehBody);
	s_hVRVehBody = LTNULL;
	s_nVRVehBodyKind = 0;
}

void CGameClientShell::VRUpdateVehicleBody()
{
	CVehicleMgr* pVM = m_MoveMgr.GetVehicleMgr();
	int nKind = 0;
	if (g_vtVRVehicleBody.GetFloat() > 0.0f && pVM && pVM->IsVehiclePhysics()
		&& m_hCamera && VRShared::IsLive())
	{
		const PlayerPhysicsModel e = pVM->GetPhysicsModel();
		nKind = (e == PPM_MOTORCYCLE) ? 1 : (e == PPM_SNOWMOBILE) ? 2 : 0;
	}
	if (nKind != s_nVRVehBodyKind) VRVehBodyForget(true);
	if (!nKind) return;

	LTVector vEye; g_pLTClient->GetObjectPos(m_hCamera, &vEye);
	LTRotation rCam; g_pLTClient->GetObjectRotation(m_hCamera, &rCam);
	LTVector vU, vR, vF; g_pLTClient->GetRotationVectors(&rCam, &vU, &vR, &vF);
	vF.y = 0.0f;
	const float fLen = vF.Mag();
	if (fLen < 0.01f) return;
	vF.x /= fLen; vF.z /= fLen;
	const float fYaw = (float)atan2(vF.x, vF.z);
	LTRotation rBody; g_pLTClient->SetupEuler(&rBody, 0.0f, fYaw, 0.0f);

	const float fDown = (nKind == 1) ? g_vtVRVehicleBodyDown.GetFloat() : g_vtVRVehicleBodyDownSnow.GetFloat();
	const float fFwd  = (nKind == 1) ? g_vtVRVehicleBodyFwd.GetFloat()  : g_vtVRVehicleBodyFwdSnow.GetFloat();
	LTVector vPos(vEye.x + vF.x * fFwd, vEye.y - fDown, vEye.z + vF.z * fFwd);

	if (!s_hVRVehBody)
	{
		ObjectCreateStruct cs;
		INIT_OBJECTCREATESTRUCT(cs);
		cs.m_ObjectType = OT_MODEL;
		SAFE_STRCPY(cs.m_Filename, nKind == 1 ? "Props\\Models\\Motorcycle.abc" : "Props\\Models\\Snowmobile.abc");
		SAFE_STRCPY(cs.m_SkinName, nKind == 1 ? "Props\\Skins\\Motorcycle.dtx" : "Props\\Skins\\Snowmobile.dtx");
		cs.m_Flags = FLAG_VISIBLE | (nKind == 2 ? FLAG_ENVIRONMENTMAP : 0);
		cs.m_Pos = vPos;
		cs.m_Rotation = rBody;
		s_hVRVehBody = g_pLTClient->CreateObject(&cs);
		s_nVRVehBodyKind = s_hVRVehBody ? nKind : 0;
		VRLog::Msg("VRVehicleBody: %s the %s under the rider (down %.0f, forward %.0f)",
			s_hVRVehBody ? "drawing" : "COULD NOT CREATE", nKind == 1 ? "motorcycle" : "snowmobile", fDown, fFwd);
		if (!s_hVRVehBody) return;
	}
	g_pLTClient->SetObjectPos(s_hVRVehBody, &vPos);
	g_pLTClient->SetObjectRotation(s_hVRVehBody, &rBody);
}

// Where the publish's time goes, per section, reported every 900 frames:
// 0 search + candidates, 1 the model loop, 2 world models, 3 sprites and the rest.
static double   s_fPubSec[4] = { 0, 0, 0, 0 };
static LONGLONG s_qPubMark = 0;
static long     s_nPubSecFrames = 0;
static void VRPubMark(int nSec)
{
	LARGE_INTEGER q; QueryPerformanceCounter(&q);
	if (nSec >= 0 && s_qPubMark)
	{
		LARGE_INTEGER f; QueryPerformanceFrequency(&f);
		s_fPubSec[nSec] += (double)(q.QuadPart - s_qPubMark) * 1000.0 / (double)f.QuadPart;
	}
	s_qPubMark = q.QuadPart;
}

void CGameClientShell::VRPublishModels()
{
	if (g_vtVRModels.GetFloat() <= 0.0f) return;
	VRPubMark(-1);
	if (++s_nPubSecFrames >= 900)
	{
		VRLog::Msg("VRPublishSections (ms per frame): search+candidates %.2f | model loop %.2f | world models %.2f | sprites+rest %.2f",
			s_fPubSec[0] / 900.0, s_fPubSec[1] / 900.0, s_fPubSec[2] / 900.0, s_fPubSec[3] / 900.0);
		s_fPubSec[0] = s_fPubSec[1] = s_fPubSec[2] = s_fPubSec[3] = 0.0;
		s_nPubSecFrames = 0;
	}

	// THE MENU IS A 3D SCENE TOO, and this used to refuse to publish for it.
	//
	// The main menu's backdrop - Cate moving about behind the text - is not a
	// picture. CInterfaceMgr::UpdateInterfaceSFX builds an object list and calls
	// RenderObjects(hInterfaceCamera, ...), so the renderer really is asked to
	// draw a scene; it just has no world and no published models, which is why
	// docs/MENU-IS-BLACK.md ends with "whatever fills it arrives as models".
	//
	// So publish in GS_FOLDER as well, around the INTERFACE camera rather than
	// the player's - there is no player object at the menu and the sphere would
	// be centred on the origin. +VRMenuModels 0 turns it off.
	const bool bFolder = (m_InterfaceMgr.GetGameState() == GS_FOLDER) || m_bVRLoadPublish;
	// DIALOGUE DRAWS THE WORLD TOO, so it publishes like PLAYING. Gated on
	// PLAYING alone, a conversation froze the list at its last playing frame:
	// everything already in it kept drawing, and anything that became visible
	// during the choice did not. A cutscene camera switches on with the
	// dialogue, and in the frame before it the player's own body was still
	// hidden for first person - so Cate was missing from every HQ conversation
	// shot until the choice closed it.
	const bool bWorldState = (m_InterfaceMgr.GetGameState() == GS_PLAYING
						   || m_InterfaceMgr.GetGameState() == GS_DIALOGUE);
	if (!bWorldState
		&& !(bFolder && g_vtVRMenuModels.GetFloat() > 0.0f)) return;

	// Resolved out of the renderer by name, once. Both DLLs are loaded into
	// lithtech.exe, so this is one process talking to itself and nothing is
	// marshalled. If the retail d3d.ren is running the symbol is absent and
	// this quietly does nothing, which is the correct behaviour.
	// RE-RESOLVED WHENEVER THE RENDERER'S MODULE HANDLE CHANGES. This was
	// looked up once and kept, and the engine unloads the renderer at the end
	// of the intro level (the movie, the restart): the next call went into an
	// unmapped page. Windows named it - "d3dstub.ren_unloaded +0x10c00", which
	// is exactly R3D_PublishFolder2D's export - on the night sweep of
	// 9 September. GetModuleHandle is cheap; a handle that differs from the one
	// the pointer came from, including NULL, means the pointer is dead.
	static VRModelPublishFn s_pfn = NULL;
	static HMODULE s_hRenSeen = (HMODULE)(uintptr_t)1;		// never looked
	{
		HMODULE h = GetModuleHandleA("d3dstub.ren");
		if (h != s_hRenSeen)
		{
			s_hRenSeen = h;
			s_pfn = h ? (VRModelPublishFn)GetProcAddress(h, "R3D_PublishModels") : NULL;
			VRLog::Msg("VRModels: renderer %s, publish entry %s",
				h ? "found" : "NOT FOUND (unloaded, or retail d3d.ren)",
				s_pfn ? "resolved" : "missing");
		}
	}

	// Nodes the engine would not pose. Reported with the model counts, because
	// a single one of these used to corrupt every index after it.
	static uint32 s_nUnposedNodes = 0;
	// Attachments moved by the ProcessAttachments pass below. A hat that was
	// already in the right place does not move, so nAttMoved is how many were
	// being drawn somewhere they are not.
	static uint32 s_nAttSeen = 0, s_nAttMoved = 0;
	// The same three for WORLD MODELS, which had no pass at all until now.
	static uint32 s_nWMAttSeen = 0, s_nWMAttMoved = 0, s_nWMWithAtt = 0;
	static uint32 s_nWMNamed = 0;
	static float  s_fWMAttMax = 0.0f;
	static float  s_fAttMoveSum = 0.0f, s_fAttMoveMax = 0.0f;

	static VRModelFrame s_frame;		// ~150 KB, static rather than on the stack
	memset(&s_frame, 0, sizeof(uint32_t) * 6);
	VRPrims_Rebase().bKnown = false;		// filled below if a view weapon is published
	s_frame.nMagic   = VRMODELS_MAGIC;
	s_frame.nVersion = VRMODELS_VERSION;
	s_frame.nFrame   = ++m_nVRModelFrame;

	HLOCALOBJ hPlayer = g_pLTClient->GetClientObject();
	LTVector vEye(0.0f, 0.0f, 0.0f);

	// AND AN IN-GAME PAUSE MENU IS NOT THE MAIN MENU, though the engine gives
	// them the same state and the same camera.
	//
	// Pressing ESC during play is GS_FOLDER as well. Centring the sphere on the
	// interface camera there publishes whatever happens to stand near the world
	// ORIGIN - which in Morocco is a rooftop several streets away - and nothing
	// at all where the player is standing. The renderer now draws that backdrop
	// from the player's last eye pose, so the models it is given have to come
	// from there too or the level will be drawn empty of everything in it.
	//
	// The main menu has no client object, and that is the test: no player, no
	// world, publish around the interface camera as before.
	// A FULL-CARD FOLDER PUBLISHES AROUND THE INTERFACE CAMERA TOO: its
	// card, cubes and flowers stand at the origin like the main menu's.
	const bool bMainMenu = (bFolder && !hPlayer)
		|| (bFolder && VRCardScene());

	if (bMainMenu && m_hInterfaceCamera)
		g_pLTClient->GetObjectPos(m_hInterfaceCamera, &vEye);
	else if (m_hCamera) g_pLTClient->GetObjectPos(m_hCamera, &vEye);
	else if (hPlayer) g_pLTClient->GetObjectPos(hPlayer, &vEye);

	if (bFolder)
	{
		static int s_nSaidFolder = 0;
		if (s_nSaidFolder < 2)
		{
			++s_nSaidFolder;
			VRLog::Msg("VRModels: folder state - %s, publishing around "
					   "(%.1f %.1f %.1f)",
					   bMainMenu ? "MAIN MENU, the interface camera"
								 : "PAUSE menu, the player's camera",
					   vEye.x, vEye.y, vEye.z);
		}
	}

	// A sphere around the CAMERA, not the player: they are the same in first
	// person and are not during a cutscene.
	//
	// 1024, not 512: the sphere already returns 455-499 objects, close
	// enough to the ceiling that the engine's own truncation would decide
	// what the distance sort below never gets to see.
	// 4096, WITH THE TRANSPORT. The sphere already returned 455-499 objects
	// before anything persisted; with three thousand casings and holes kept
	// it will return far more, and an array that truncates decides which
	// objects exist at all - silently, and before the distance sort below
	// ever sees them.
	HLOCALOBJ objs[4096];
	uint32 nOut = 0, nFound = 0;
	// The interface scene is a handful of objects a few hundred units from the
	// interface camera, so the play range is the wrong shape for it - too small
	// to be sure and measured for a different question. Take everything.
	// THE WHOLE ARRAY. This passed 1024 for a 4096-entry array, and the
	// sphere returns EVERY object type - Morocco has 1086 within range - so
	// the tail was never looked at, and a character or a pickup in that tail
	// was never published. The engine's census (below) already uses 4096.
	g_pLTClient->FindObjectsInSphere(&vEye,
		bFolder ? 1000000.0f : g_vtVRModelRange.GetFloat(),
		objs, 4096, &nOut, &nFound);
	// AT THE MENU THE SPHERE IS THE WRONG INSTRUMENT. No world is loaded,
	// so nothing is in the partition it searches: it returns 6 objects
	// where the interface holds 15, and the nine it misses are the blue
	// panel, the logo, THE OPERATIVE and the help box. Retail's census
	// agrees with ours object for object, which proves the objects were
	// never missing and the QUERY was wrong.
	// ONLY WHEN THERE IS NO WORLD. THE PAUSE MENU IS A FOLDER TOO.
	//
	// The interface objects are posed around the INTERFACE camera - the panel
	// at 0 0 200, the card at 60 15 160 - and the renderer draws whatever is
	// published using the camera it is rendering the scene with. At the main
	// menu those are the same camera and the substitution is right. At the
	// PAUSE menu a world is loaded and the scene is drawn from the PLAYER's
	// camera, so the same objects land in the room as free-floating cards, one
	// per interface object, at a fixed distance - which is different in each
	// eye, so they read as double vision - in the headset the pause menu was
	// in double vision.
	//
	// A client object exists exactly when a world is being played and not at
	// the main menu, so it is the test. The 2D text layer is unaffected and
	// already draws the pause menu correctly.
	// ...AND A FULL-CARD FOLDER IS THE INTERFACE SCENE TOO, world or no
	// world: only its own objects are published, exactly as at the main
	// menu, because retail's RenderObjects(hInterfaceCamera) draws only
	// those. bMainMenu already says so (see VRIsCardFolder).
	if (bMainMenu && g_nVRIfaceObjs > 0)
	{
		nOut = 0;
		for (int z = 0; z < g_nVRIfaceObjs && nOut < 4096; ++z)
			objs[nOut++] = g_hVRIfaceObjs[z];
		nFound = nOut;
	}
	// A CARD OVER A LOADED LEVEL WITH NO INTERFACE LIST publishes nothing:
	// what the sphere found is the level around the origin, not the card.
	else if (bMainMenu && hPlayer)
	{
		nOut = 0; nFound = 0;
	}
	// AND TAKE THEM OUT AGAIN WHEN A WORLD IS LOADED.
	//
	// Not substituting them was not enough. With a world up, the interface SFX
	// are in the spatial partition like anything else, so the sphere FINDS
	// them on its own - and they are posed for the interface camera, so they
	// land in the room as olive cards a few feet from the player's face, in a
	// different place in each eye. Excluding them by handle is the only test
	// that works, because in every other respect they are ordinary objects.
	if (hPlayer && !bMainMenu && g_nVRIfaceObjs > 0 && nOut)
	{
		uint32 nKeep = 0;
		for (uint32 z = 0; z < nOut; ++z)
		{
			bool bIface = false;
			for (int q = 0; q < g_nVRIfaceObjs; ++q)
				if (objs[z] == g_hVRIfaceObjs[q]) { bIface = true; break; }
			if (!bIface) objs[nKeep++] = objs[z];
		}
		nOut = nKeep;
	}

	// ---- THE DENOMINATOR, which nobody had ever taken -------------------
	//
	// "122 instances published" is a part. The question it cannot answer is
	// the one the tester was actually asking - where is the furniture - because a
	// part looks healthy whether the whole is 130 or 1300. So: every object
	// the engine has in this level, by TYPE, against how many of them are
	// near enough to matter and how many we hand over.
	//
	// Taken periodically and with a radius large enough to enclose any NOLF
	// level, so it is a census and not another sample. It costs one
	// FindObjectsInSphere every five seconds.
	{
		static uint32 s_nCensusAt = 0;
		if (m_nVRModelFrame - s_nCensusAt >= 450)
		{
			s_nCensusAt = m_nVRModelFrame;
			static HLOCALOBJ all[4096];
			uint32 nAll = 0, nAllFound = 0;
			g_pLTClient->FindObjectsInSphere(&vEye, 1000000.0f, all, 4096,
											 &nAll, &nAllFound);
			uint32 nType[8] = { 0 };
			uint32 nModelFar = 0;
			const float fRange = g_vtVRModelRange.GetFloat();
			for (uint32 z = 0; z < nAll; ++z)
			{
				const uint32 t = g_pLTClient->GetObjectType(all[z]);
				if (t < 8) ++nType[t];
				if (t == OT_MODEL)
				{
					LTVector q; g_pLTClient->GetObjectPos(all[z], &q);
					if ((q - vEye).Mag() > fRange) ++nModelFar;
				}
			}
			// ---- DO WORLD MODELS ACTUALLY MOVE? ----------------------
			//
			// The renderer draws every world model at its AUTHORED position and
			// never updates it, so the HQ doors are frozen shut. Before building a
			// transform bridge for that, one thing has to be true: the OBJECT's
			// position must actually change when a door opens. If the engine moves
			// a door some other way - swapping geometry, or animating vertices we
			// cannot see - the bridge would carry a transform that never varies and
			// the doors would stay shut with more code behind them.
			//
			// So: remember where every WorldModel was the first time it was seen,
			// and report the ones that have moved since. A door the tester walks up to
			// either appears here or it does not, and that decides the design.
			{
				// AND DO THEY TURN? Position alone missed a wall that rises on
				// the near side while the far end holds still (the club, desk,
				// 21 September): that is a rotation, and this tracker was blind
				// to it. Each world model's forward is kept from first sight.
				struct WMSeen { uint32 h; LTVector vFirst; LTVector vLast;
								float fMoved; LTVector vFwdFirst; float fTurned; };
				static WMSeen s_WM[512];
				static uint32 s_nWM = 0;
				uint32 nMoved = 0, nTurned = 0;
				for (uint32 z = 0; z < nAll; ++z)
				{
					if (g_pLTClient->GetObjectType(all[z]) != OT_WORLDMODEL) continue;
					LTVector q; g_pLTClient->GetObjectPos(all[z], &q);
					LTRotation rq; g_pLTClient->GetObjectRotation(all[z], &rq);
					LTVector vU, vR, vF; g_pLTClient->GetRotationVectors(&rq, &vU, &vR, &vF);
					const uint32 h = (uint32)(uintptr_t)all[z];
					uint32 k = 0;
					for (; k < s_nWM; ++k) if (s_WM[k].h == h) break;
					if (k == s_nWM)
					{
						if (s_nWM >= 512) continue;
						k = s_nWM++;
						s_WM[k].h = h; s_WM[k].vFirst = q; s_WM[k].fMoved = 0.0f;
						s_WM[k].vFwdFirst = vF; s_WM[k].fTurned = 0.0f;
					}
					s_WM[k].vLast = q;
					const float d = (q - s_WM[k].vFirst).Mag();
					if (d > s_WM[k].fMoved) s_WM[k].fMoved = d;
					if (s_WM[k].fMoved > 1.0f) ++nMoved;
					float fDot = vF.Dot(s_WM[k].vFwdFirst); if (fDot > 1.0f) fDot = 1.0f; if (fDot < -1.0f) fDot = -1.0f;
					const float fDeg = acosf(fDot) * 57.29578f;
					if (fDeg > s_WM[k].fTurned) s_WM[k].fTurned = fDeg;
					if (s_WM[k].fTurned > 0.5f)
					{
						++nTurned;
						if (nTurned <= 6)
						{
							LTVector dm(0,0,0); if (g_pPhysicsLT) g_pPhysicsLT->GetObjectDims(all[z], &dm);
							VRLog::Msg("VRWorldTurn: obj %08X has TURNED %.1f deg since first seen - at (%.0f %.0f %.0f) dims (%.0f %.0f %.0f), fwd now (%.2f %.2f %.2f)",
								h, s_WM[k].fTurned, q.x, q.y, q.z, dm.x, dm.y, dm.z, vF.x, vF.y, vF.z);
						}
					}
				}
				if (nTurned) VRLog::Msg("VRWorldTurn: %u of %u WorldModels have TURNED more than half a degree", nTurned, s_nWM);
				VRLog::Msg("VRWorldMove: %u of %u WorldModels have MOVED from where"
					" they were first seen%s", nMoved, s_nWM,
					nMoved ? "  <- the object position DOES change, so a transform"
							 " bridge will work"
						  : "  <- nothing has moved yet; open a door and look again");
				uint32 nSaid = 0;
				for (uint32 k = 0; k < s_nWM && nSaid < 8; ++k)
					if (s_WM[k].fMoved > 1.0f)
					{
						++nSaid;
						VRLog::Msg("    obj %08X moved %.1f units: (%.0f %.0f %.0f)"
							" -> (%.0f %.0f %.0f)", s_WM[k].h, s_WM[k].fMoved,
							s_WM[k].vFirst.x, s_WM[k].vFirst.y, s_WM[k].vFirst.z,
							s_WM[k].vLast.x, s_WM[k].vLast.y, s_WM[k].vLast.z);
					}
			}

			VRLog::Msg("VRCensus: the engine has %u objects in this level"
				" (%u returned, cap 4096) - %u MODELS, %u WorldModels, %u sprites,"
				" %u lights, %u particles, %u cameras, %u invisible"
				" | %u models are further than %.0f units and are never considered",
				nAllFound, nAll, nType[OT_MODEL], nType[OT_WORLDMODEL],
				nType[OT_SPRITE], nType[OT_LIGHT], nType[OT_PARTICLESYSTEM],
				nType[OT_CAMERA], nType[OT_NORMAL], nModelFar, fRange);
			if (nAllFound > nAll)
				VRLog::Msg("VRCensus: TRUNCATED - the level has more objects than"
					" the census array holds, so every number above is a floor");
		}
	}

	// THE CENSUS RUNS FOR BOTH RENDERERS, AND THE EARLY-OUT COMES AFTER IT.
	//
	// It used to sit above, so a retail d3d.ren run returned before taking
	// any census at all and the log said only "renderer NOT FOUND". That
	// makes the one comparison worth having impossible: the menu's blue
	// panel and its logo are supposed to be sprite objects created by
	// CBaseFolder::CreateScaleFX, and our census sees ONE sprite. Whether
	// retail sees the same is the difference between "they exist and we do
	// not draw them" and "they are never created" - two completely
	// different searches, and nothing above this line needs the renderer
	// to decide which.
	if (!s_pfn) return;

	// NEAREST FIRST. The array holds 192 and the desk publishes 190 of them
	// standing still, so the list is effectively always full and the question
	// is only which models make it. Enumeration order answered that until now,
	// which is why an enemy at point-blank range came and went while distant
	// scenery stayed: it was competing with a crate 4000 units away on equal
	// terms. Sorted by distance, a drop is always the farthest thing.
	// IN FRONT FIRST, THEN NEAREST. Sorting by distance alone spends the whole
	// publish budget in every direction at once, and the renderer then throws
	// away everything behind the camera before it draws - so on m01s02 the
	// R3D MODEL ACCOUNT read "192 published, 25 drawn, 165 BEHIND THE CAMERA".
	// Six sevenths of the budget was being spent on furniture nobody could see.
	//
	// The test is the same weak one the renderer culls with - a bounding sphere
	// ENTIRELY behind the eye cannot fall in either eye's frustum however
	// asymmetric they are - so the two agree by construction and this cannot
	// drop anything that would have been drawn.
	//
	// Behind-camera models are still published with whatever budget is left, and
	// still nearest-first, so turning round finds the near ones already there.
	LTRotation rCam;
	LTVector vCamF(0.0f, 0.0f, 1.0f), vCamR, vCamU;
	if (m_hCamera)
	{
		g_pLTClient->GetObjectRotation(m_hCamera, &rCam);
		g_pLTClient->GetRotationVectors(&rCam, &vCamU, &vCamR, &vCamF);
	}

	struct VRCand { uint32 i; float d2; int nBehind; };
	VRCand cand[4096];
	uint32 nCand = 0;

	// THE FIRST-PERSON WEAPON, APPENDED BY HAND.
	//
	// FindObjectsInSphere searches around the CAMERA, and the view weapon is
	// positioned CAMERA-RELATIVE - so in world coordinates it sits near the map
	// origin, thousands of units away, and the sphere has never returned it.
	// Every line of the VR hand-aim path has been writing to an object that is
	// not in this list and is therefore never drawn. Measured: three metres of
	// hand movement put it at x +167.8 units and the capture came back byte
	// identical.
	//
	// Behind VRViewModel, default 0. With the cvar off not one line below this
	// executes, which is the only reason it is safe to add to the publish path
	// without a headset to judge it.
	uint32 nViewWeapon = 0xFFFFFFFFu;
	// NOT INTO THE INTERFACE SCENE. A card folder over a loaded level draws
	// from the interface camera at the origin, and the view weapon - camera-
	// relative, so also near the origin - stood in front of the mission
	// objectives card at the desk on 13 September. The menus have no gun.
	// 4096, the array's size. These appends stopped at 1024 after the search was
	// widened to 4096, and the M05S01 docks (34 rain volumes of splash sprites)
	// return ~1120 objects: the gun was never appended, so it was not drawn and
	// its flash, tracer and casings sat at a stale place.
	if (g_vtVRViewModel.GetFloat() > 0.0f && nOut < 4096 && !bMainMenu)
	{
		HLOCALOBJ hVW = m_weaponModel.GetHandle();
		if (hVW)
		{
			bool bAlready = false;
			for (uint32 z = 0; z < nOut; ++z)
				if (objs[z] == hVW) { bAlready = true; break; }
			if (!bAlready)
			{
				nViewWeapon = nOut;
				objs[nOut++] = hVW;
			}
		}
	}

	// THE VEHICLE YOU ARE SITTING ON, appended the same way. snow_pv.abc and
	// moto_pv.abc (and the windshield) are FLAG_REALLYCLOSE client objects
	// positioned in camera space by CVehicleMgr::UpdateVehicleModel, so the
	// sphere never returns them and the player rode an invisible sled - found
	// at the desk on 16 September, mounting T10S01's snowmobile: the log said
	// SNOWMOBILE and the capture showed an empty road. They take the view
	// weapon's route below (camera-relative origin and nodes pushed out by K)
	// with the gun-only parts - the hand, the rigid root, the size lever -
	// left off.
	uint32 nViewVeh[2] = { 0xFFFFFFFFu, 0xFFFFFFFFu };
	if (g_vtVRViewModel.GetFloat() > 0.0f && !bMainMenu
		&& m_MoveMgr.GetVehicleMgr() && m_MoveMgr.GetVehicleMgr()->IsVehiclePhysics())
	{
		HLOCALOBJ hV[2] = { m_MoveMgr.GetVehicleMgr()->GetVehicleModel(),
							m_MoveMgr.GetVehicleMgr()->GetVehicleAttachModel() };
		for (int v = 0; v < 2; ++v)
		{
			if (!hV[v] || nOut >= 4096) continue;
			bool bAlready = false;
			for (uint32 z = 0; z < nOut; ++z)
				if (objs[z] == hV[v]) { bAlready = true; break; }
			if (!bAlready) { nViewVeh[v] = nOut; objs[nOut++] = hV[v]; }
		}
	}

	// THE MODS ON THE GUN. The silencer, the scope and the laser are the
	// engine's own client objects, camera-relative like the gun and kept at
	// its sockets by CWeaponModel::UpdateMods. The sphere query never returns
	// a camera-relative object and nothing appended these, so the Hampton
	// Carbine drew without its scope and the P38 without its silencer
	//. Appended after the gun, placed below
	// through the gun's own rigid transform, so they sit on its sockets in
	// the hand. Only while the game shows them: a mod the player lacks is
	// kept hidden by the same code that hides it flat.
	uint32 nViewMod[3] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
	VRPrims_ClearScopeLens();		// set again below if a scope is drawn this frame
	if (nViewWeapon != 0xFFFFFFFFu && !bMainMenu)
	{
		for (int k = 0; k < 3; ++k)
		{
			HOBJECT hM = m_weaponModel.VRModObject(k);
			if (!hM || !m_weaponModel.VRModShown(k) || nOut >= 4096) continue;
			if (!(g_pLTClient->GetObjectFlags(hM) & FLAG_VISIBLE)) continue;
			bool bAlready = false;
			for (uint32 z = 0; z < nOut; ++z)
				if (objs[z] == hM) { bAlready = true; break; }
			if (!bAlready) { nViewMod[k] = nOut; objs[nOut++] = hM; }
		}
	}

	uint32 nBehindSeen = 0;
	for (uint32 i = 0; i < nOut && nCand < 4096; ++i)
	{
		if (g_pLTClient->GetObjectType(objs[i]) != OT_MODEL) continue;
		LTVector p; g_pLTClient->GetObjectPos(objs[i], &p);
		const float dx = p.x - vEye.x, dy = p.y - vEye.y, dz = p.z - vEye.z;
		cand[nCand].i = i;
		cand[nCand].d2 = dx * dx + dy * dy + dz * dz;

		// TWO RANGES: SCENERY TO THE FAR CLIP, CHARACTERS TO 4000.
		// VRModelRange reaches the far clip so trees and props stop popping
		// at 68 m (see its Init); publishing every distant CHARACTER as well
		// cost 3.4 ms of world render a frame at the desk (M01S04: 0.80 ms at
		// 44 models, 4.25 ms at 216). A character or a body - an object with a
		// CharacterFX or BodyFX - keeps the old range. The first version told
		// them apart by node count (more than twelve = a rig) and the swaying
		// M01S04 palms have more than twelve: two down the canyon sat either
		// side of 4000 units from the path and blinked in and out as the
		// player walked. VRModelRangeAnimated 0: no
		// second range.
		// HOW FAR: -1 (the default) is AS FAR AS THE LEVEL'S FOG LETS YOU SEE -
		// FogFarZ when fog is on (nothing is visible past it in retail either;
		// never under 4000), the whole level when it is off. The vehicle levels
		// author 2500-6000 of fog; Morocco at night 10240; some none at all.
		{
			float fAnim = g_vtVRModelRangeAnim.GetFloat();
			if (fAnim < 0.0f)
			{
				if (GetConsoleInt("FogEnable", 0))
				{
					const float fFog = (float)GetConsoleInt("FogFarZ", 5000);
					fAnim = (fFog > 4000.0f) ? fFog : 4000.0f;
				}
				else fAnim = 0.0f;		// no fog: no second range
			}
			if (fAnim > 0.0f && !bMainMenu && cand[nCand].d2 > fAnim * fAnim
				&& i != nViewWeapon && i != nViewVeh[0] && i != nViewVeh[1])
			{
				CSFXMgr* pSFX = GetSFXMgr();
				const bool bRig = pSFX
					&& (pSFX->FindSpecialFX(SFX_CHARACTER_ID, objs[i])
						|| pSFX->FindSpecialFX(SFX_BODY_ID, objs[i]));
				if (bRig) { ++s_nVRFarAnimSkipped; continue; }
				++s_nVRFarSceneryKept;
			}
		}

		LTVector d(16.0f, 16.0f, 16.0f);
		if (g_pPhysicsLT) g_pPhysicsLT->GetObjectDims(objs[i], &d);
		const float fRad = (float)sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
		const float fwd = dx * vCamF.x + dy * vCamF.y + dz * vCamF.z;
		cand[nCand].nBehind = (fwd < -fRad) ? 1 : 0;
		// The view weapon's position is camera-relative, so the distance and
		// behind-ness computed above are both meaningless for it. Force it to
		// the front of the order: it is the one instance the player is
		// guaranteed to be looking at.
		if (i == nViewWeapon || i == nViewVeh[0] || i == nViewVeh[1])
		{
			cand[nCand].d2 = -1.0f;
			cand[nCand].nBehind = 0;
		}
		if (cand[nCand].nBehind) ++nBehindSeen;
		++nCand;
	}
	// Insertion sort: nCand is at most 512 and is usually a few hundred, and
	// this runs once a frame. Not worth a dependency.
	for (uint32 a = 1; a < nCand; ++a)
	{
		const VRCand key = cand[a];
		uint32 b = a;
		while (b > 0 && (cand[b - 1].nBehind > key.nBehind
						 || (cand[b - 1].nBehind == key.nBehind
							 && cand[b - 1].d2 > key.d2)))
		{ cand[b] = cand[b - 1]; --b; }
		cand[b] = key;
	}

	// AN ATTACHMENT IS ONLY UPDATED WHEN ITS PARENT IS RENDERED, and nothing
	// renders these. iltclient.h says it outright, beside ProcessAttachments:
	// "Attachments are always automatically updated when the object is
	// rendered." Retail draws a model through the engine, so the engine walks
	// the attachment list and puts the hat back on the head every frame. We
	// build the mesh ourselves out of the HOBJECT and the engine's model path
	// never runs, so an attachment stays wherever it was last put.
	//
	// While a character is ALIVE this is hidden: CCharacterFX::Update calls
	// ProcessAttachments on its own server object every frame and the hat
	// follows. A dead one is replaced by a Body prop, which has no
	// CharacterFX and so has nobody to do it - which is exactly when the tester
	// saw an NPC's fez stay in the air after the body fell.
	//
	// So do the step the renderer owes, over every model about to be
	// published and BEFORE any node transform is read, so a parent is always
	// processed before its child is published.
	{
		// The MEASUREMENT is off by default. It costs a GetAttachments and two
		// GetObjectPos per attachment on every published model every frame,
		// and this file's own history says a diagnostic left in a per-frame
		// path is paid for in every arm. It proved the fix; +VRAttachMeasure 1
		// brings it back when something needs proving again.
		const bool bMeasure = (g_vtVRAttachMeasure.GetFloat() > 0.0f);
		ILTCommon* pCommon = bMeasure ? g_pLTClient->Common() : LTNULL;

		for (uint32 c = 0; c < nCand; ++c)
		{
			HLOCALOBJ hPar = objs[cand[c].i];

			HLOCALOBJ kids[8];
			LTVector  before[8];
			uint32    nKids = 0, nTotal = 0;
			if (pCommon && pCommon->GetAttachments(hPar, kids, 8, nKids, nTotal) == LT_OK)
			{
				if (nKids > 8) nKids = 8;
				for (uint32 k = 0; k < nKids; ++k)
					g_pLTClient->GetObjectPos(kids[k], &before[k]);
			}
			else nKids = 0;

			g_pLTClient->ProcessAttachments(hPar);

			for (uint32 k = 0; k < nKids; ++k)
			{
				LTVector after;
				g_pLTClient->GetObjectPos(kids[k], &after);
				const float d = (after - before[k]).Mag();
				++s_nAttSeen;
				if (d > 0.5f)
				{
					++s_nAttMoved;
					s_fAttMoveSum += d;
					if (d > s_fAttMoveMax) s_fAttMoveMax = d;
				}
			}
		}

		// AND A DOOR IS A WORLD MODEL, NOT A MODEL.
		//
		// The loop above skips anything that is not OT_MODEL, so a door, a
		// gate or a lift never has its attachments processed at all - and
		// whatever hangs off it keeps the position it held when the level
		// loaded. That is the door handle the tester watched stay in mid-air while
		// the door swung away from it, and the lock that stayed on a gate the player
		// had already shot it off.
		//
		// The same defect as the fez above, one object type over: the fix
		// there was written for models and the type test quietly excluded
		// every brush entity in the game.
		//
		// Cheap: an object with no attachments makes this very nearly free,
		// and there are 232 world models on Morocco against 192 models.
		// `VRWorldAttach 0` is the control arm.
		if (g_vtVRWorldAttach.GetFloat() > 0.0f)
		{
			// EVERY object's position before the pass, so the comparison does
			// not depend on any API's idea of who is attached to whom.
			static std::vector<LTVector> s_before;
			s_before.resize(nOut);
			for (uint32 i = 0; i < nOut; ++i)
				g_pLTClient->GetObjectPos(objs[i], &s_before[i]);

			// ONLY THE BRUSHES THAT HAVE ACTUALLY MOVED.
			//
			// Processing all 232 world models every frame pays for 232 calls
			// to fix the two that opened. An attachment can only be in the
			// wrong place if its parent went somewhere, so the parent's own
			// movement is the trigger - and a door that is standing still
			// costs one position compare.
			//
			// The first frames after a load are processed unconditionally:
			// nothing has a baseline yet, and a door that loaded already open
			// has moved as far as its handle is concerned.
			static std::map<uint32, LTVector> s_lastWM;
			static int s_nWMFrames = 0;
			const bool bWarmUp = (++s_nWMFrames < 30);
			for (uint32 i = 0; i < nOut; ++i)
			{
				if (g_pLTClient->GetObjectType(objs[i]) != OT_WORLDMODEL)
					continue;

				const uint32 key = (uint32)(uintptr_t)objs[i];
				LTVector wp;
				g_pLTClient->GetObjectPos(objs[i], &wp);
				std::map<uint32, LTVector>::iterator itW = s_lastWM.find(key);
				const bool bMoved = bWarmUp || itW == s_lastWM.end()
								 || (wp - itW->second).MagSqr() > 0.01f;
				s_lastWM[key] = wp;
				if (!bMoved) continue;

				// THE MEASUREMENT THAT SAYS THE MECHANISM IS REAL, rather than
				// that the call was made.
				//
				// THE FIRST VERSION OF THIS ASKED THE WRONG WITNESS. It used
				// ILTCommon::GetAttachments to find a brush entity's children,
				// got zero on every world model on Morocco, and concluded that
				// doors have no attachments - which would mean this whole pass
				// is pointless. That conclusion was only as good as the API,
				// and that API may simply not answer for brush entities.
				//
				// So the honest test does not ask. It snapshots the position of
				// EVERY object in the list, calls ProcessAttachments on the
				// world models, and compares - anything that moved was being
				// held in place by nothing else. Door handles and lock plates
				// are separate prop models in this game (PROPS/MODELS/
				// DOORKNOB_01L.ABC, LOCK.ABC, PLATE_01.ABC), so if they are
				// parented to their door this is what will say so.
				g_pLTClient->ProcessAttachments(objs[i]);
				++s_nWMWithAtt;
			}

			// WHO MOVED. Anything whose position changed across that pass was
			// being held in the wrong place by nothing but the missing call.
			for (uint32 i = 0; i < nOut; ++i)
			{
				LTVector after;
				g_pLTClient->GetObjectPos(objs[i], &after);
				const float d = (after - s_before[i]).Mag();
				++s_nWMAttSeen;
				if (d > 0.5f)
				{
					++s_nWMAttMoved;
					if (d > s_fWMAttMax) s_fWMAttMax = d;
					if (s_nWMNamed < 8)
					{
						++s_nWMNamed;
						// The client cannot name a model file - that API is
						// server side - so it logs what it CAN see. The
						// renderer names models by position when it has to.
						LTVector dims(0.0f, 0.0f, 0.0f);
						if (g_pPhysicsLT)
							g_pPhysicsLT->GetObjectDims(objs[i], &dims);
						VRLog::Msg("VRWorldAttach: MOVED %.1f units - type %d"
								   " at (%.0f %.0f %.0f) dims (%.0f %.0f %.0f)",
								   d, (int)g_pLTClient->GetObjectType(objs[i]),
								   after.x, after.y, after.z,
								   dims.x, dims.y, dims.z);
					}
				}
			}
		}
	}

	// THE OBJECTS THE PLAYER OWNS, so that "the engine is not drawing this" can
	// be told from "the engine draws this somewhere else".
	//
	// NOLF hides a first-person player's attachments - HideShowAttachments
	// clears FLAG_VISIBLE on them and sets FLAG_PORTALVISIBLE instead - and
	// draws the weapon through the close-up pass rather than the ordinary
	// object list. So the WEAPON has FLAG_VISIBLE clear while being the most
	// visible thing on screen, and a rule that skipped every unset object took
	// it away: measured, the gun vanishes from the Morocco quick save.
	//
	// Two levels, because a weapon is an attachment of the player's body
	// rather than of the player.
	//
	// AND IT IS OFF WHEN THE VIEW MODEL IS DRAWN, which is the whole reason
	// the tester saw TWO guns and two sets of hands.
	//
	// What this exemption actually protects is the player's HAND-HELD weapon -
	// the one attached to Cate's body that other people see. NOLF hides it in
	// first person on purpose, and the exemption kept us drawing it. That was
	// right while it was the only gun we had: it stood in for a first-person
	// weapon that was never published at all.
	//
	// Now the real player-view weapon IS published, explicitly, and it does not
	// need this - it is found by handle, not by the sphere. So keeping the
	// exemption draws the stand-in AND the real thing: the orange-gripped gun
	// that has been in every capture for weeks, plus the PV Walther with its
	// hands, at once.
	//
	// One or the other, never both.
	HLOCALOBJ hOwned[64];
	uint32 nOwned = 0;
	if (hPlayer)
	{
		hOwned[nOwned++] = hPlayer;
		for (uint32 nPass = 0, nSeen = 0; nPass < 2; ++nPass)
		{
			const uint32 nEnd = nOwned;
			for (uint32 z = nSeen; z < nEnd && nOwned < 64; ++z)
			{
				HLOCALOBJ att[20];
				uint32 nSize = 0, nNum = 0;
				g_pLTClient->GetAttachments(hOwned[z], att, 20, &nSize, &nNum);
				const uint32 n = (nNum <= nSize) ? nNum : nSize;
				for (uint32 q = 0; q < n && nOwned < 64; ++q)
					hOwned[nOwned++] = att[q];
			}
			nSeen = nEnd;
		}
	}

	// HOW MANY THINGS THE PLAYER TURNS OUT TO OWN. GetAttachments is a server
	// notion and this is the client; if it comes back with only the player
	// himself then the player test can never fire and every rule built on it is
	// inert - which looks exactly like the rule being wrong.
	{
		static uint32 s_nSaidOwned = 0xFFFFFFFFu;
		if (nOwned != s_nSaidOwned)
		{
			s_nSaidOwned = nOwned;
			VRLog::Msg("VRPlayer: %u objects owned by the player (the player"
				" himself plus %u attachments)", nOwned,
				nOwned ? nOwned - 1 : 0);
		}
	}

	VRPubMark(0);
	for (uint32 c = 0; c < nCand; ++c)
	{
		const uint32 i = cand[c].i;
		// The runtime cap, never past what the array can hold.
		uint32 nCap = (uint32)g_vtVRModelCap.GetFloat();
		if (!nCap || nCap > VRMODELS_MAX_INST) nCap = VRMODELS_MAX_INST;
		if (s_frame.nCount >= nCap) { ++s_frame.nDropped; continue; }

		VRModelInst& mi = s_frame.inst[s_frame.nCount];
		memset(&mi, 0, sizeof(mi));
		mi.nObject = (uint32_t)(uintptr_t)objs[i];

		LTVector p; g_pLTClient->GetObjectPos(objs[i], &p);
		// THE VIEW WEAPON'S POSITION IS CAMERA-RELATIVE AND THE RENDERER CULLS
		// ON IT. Transforming only the NODES was not enough: fPos is what the
		// behind-the-camera and at-the-camera tests read, and a camera-relative
		// position sits near the map origin, which is behind you on most of a
		// level. Published 62 units out to the right, skinned, textures
		// resolved - and never drawn, because it was culled before any of that
		// mattered.
		LTVector vOrigCamRel(0.0f, 0.0f, 0.0f);
		// The vehicle takes the view weapon's camera-relative route, minus
		// everything that is about the hand.
		const bool bViewVeh = (i == nViewVeh[0] || i == nViewVeh[1]);
		const bool bViewMod = (i == nViewMod[0] || i == nViewMod[1] || i == nViewMod[2]);
		// The object's position as the ENGINE has it, before the hand replaces
		// it: the node transforms below are relative to THIS, not to the hand.
		LTVector vObjEngineCamRel(0.0f, 0.0f, 0.0f);
		// The mesh's own centre in the engine's frame - the mean of its node
		// positions about the object - so the gun's body, not its authored
		// origin, is what lands on the hand.
		LTVector vMeshAnchor(0.0f, 0.0f, 0.0f);
		// The gun's root node this frame: its engine transform, for the rigid
		// placement, and the object's own rotation, which the placement uses
		// in place of the animation's.
		bool bGunRootFound = false;
		LTVector vGunRootPos(0.0f, 0.0f, 0.0f);
		LTRotation rGunRoot; rGunRoot.Init();
		char szGunRoot[64] = "";
		LTRotation rObjNow; rObjNow.Init();
		LTVector vGripW(0.0f, 0.0f, 0.0f);	// the GRIP offset in world, view weapon only
		if (i == nViewWeapon || bViewVeh)
		{
			vObjEngineCamRel = p;
			g_pLTClient->GetObjectRotation(objs[i], &rObjNow);
			const float K = (g_vtVRViewModelScale.GetFloat() > 0.1f)
				? g_vtVRViewModelScale.GetFloat() : 17.0f;

			// PUT THE GUN WHERE THE HAND IS, HERE, AND NOT ON THE OBJECT.
			//
			// CWeaponModel::UpdateWeaponModel has written a hand position onto
			// m_hObject since the hand work started, and its own log says the
			// write never reached the picture: "a hand offset of 167 units -
			// the gun thrown right out of the frame - left the picture
			// byte-identical". Desk-measured again on 8 September with
			// VRHandAim 3 and VRGunAtHand 1: two captures 25 cm apart in hand
			// position differed by 352 pixels, none of them on the weapon.
			//
			// THIS is the path that feeds the renderer - changing K or S here
			// moves the gun every time - so the placement belongs here, where
			// there is nothing downstream left to overrule it.
			//
			// Applied BEFORE vOrigCamRel is taken, so the node transform below
			// carries the mesh with the origin: it places each node at
			// vOrig * K + (node - vOrig) * K * S, which follows any origin.
			if (!bViewVeh && g_vtVRGunAtHand.GetFloat() > 0.0f && VRShared::IsLive())
			{
				const VRSharedState& vs = VRShared::State();
				const VRHandState&   vh = vs.Hands[1];		// right hand
				if (vh.nActive)
				{
					// Units per metre for the weapon's CAMERA-RELATIVE space,
					// which is not the world's 58.75 - the authored offset is
					// about 1.5 units for a whole arm's length. See the
					// derivation in WeaponModel.cpp; 3 is that number.
					const float U = (g_vtVRHandPosScale.GetFloat() > 0.0f)
						? g_vtVRHandPosScale.GetFloat() : 3.0f;

					LTVector vHand;
					vHand.x =  ((vh.fPosX - vs.fHeadPosX)) * U;
					vHand.y =  ((vh.fPosY - vs.fHeadPosY)) * U;
					vHand.z = -((vh.fPosZ - vs.fHeadPosZ)) * U;	// OpenXR -Z fwd

					// THE CAMERA OBJECT DOES NOT CARRY THE HEAD. This used to
					// undo the head's yaw "into camera space", on the theory
					// that the basis below includes the head. It does not: the
					// head is applied inside RenderWorldEyes and put back in
					// its __finally, and this runs from PostUpdate and from
					// RenderCamera BEFORE that, so vCamR/U/F are the BODY's.
					// The fire direction (WeaponModel.cpp) uses the hand's
					// ABSOLUTE angles in this same basis, and shoots where the player
					// points. A position rotated by the head yaw is therefore
					// wrong by exactly the head yaw: look 30 degrees to the
					// right of the body and the drawn gun sits 30 degrees to
					// the left of the controller. In the headset the gun did not feel
					// like it sat in the hand - not fully 1:1.
					//
					// The one case the undo is right is VRHeadAsMouse, where
					// the body's yaw is driven BY the head and so carries it.
					const float fHY = (g_vtVRHeadAsMouse.GetFloat() > 0.0f)
						? vs.fHeadYawDeg * 0.01745329f : 0.0f;
					const float fHC = (float)cos(fHY), fHS = (float)sin(fHY);

					p.x = vHand.x * fHC - vHand.z * fHS;
					p.y = vHand.y;
					p.z = vHand.x * fHS + vHand.z * fHC;

					// WHERE IT LANDED, once per real move of the hand. The
					// object-side version of this line reported a position
					// that was never drawn, which is the whole reason this
					// block exists - so this one is taken from the value that
					// is actually published.
					{
						static float s_fSaidHandX = -99999.0f;
						if (fabsf(p.x - s_fSaidHandX) > 0.05f)
						{
							s_fSaidHandX = p.x;
							VRLog::Msg("  VIEW WEAPON at the hand:"
								" cam-relative %+.2f %+.2f %+.2f units"
								" (x%.0f = %+.0f %+.0f %+.0f world)"
								"  hand-head %+.2f %+.2f %+.2f m",
								p.x, p.y, p.z, K, p.x * K, p.y * K, p.z * K,
								vh.fPosX - vs.fHeadPosX,
								vh.fPosY - vs.fHeadPosY,
								vh.fPosZ - vs.fHeadPosZ);
						}
					}
				}
			}

			// THE HANDLEBARS SIT LOWER IN A HEADSET. The flat game hangs the
			// ridden model from the camera where a monitor crops it; in a headset
			// its cut-off lower edge showed and the bars rode high, like a prop
			// held up. World units, straight down the
			// camera's up axis. The nodes are placed from their OWN positions
			// relative to this origin, so moving the origin alone cancels out
			// (desk, 24 September: 0 and 10 drew identical frames); the node
			// loop takes the same drop. See fVehDropCam there.
			vOrigCamRel = p;			// kept for the node transform below
			if (bViewVeh && K > 0.0f)
			{
				p.y -= g_vtVRVehicleDrop.GetFloat() / K;
				// ...AND NEARER. Lowered alone it still sat far out, and from there
				// the underside and the cut-off arms showed.
				p.z -= g_vtVRVehicleBack.GetFloat() / K;
			}
			p = vEye + (vCamR * p.x + vCamU * p.y + vCamF * p.z) * K;
			// GRIP: the gun's origin moved in the gun's own frame, world units,
			// set by eye per weapon (tuner mode 5). The nodes below and
			// rb.vGunWorld take the same shift, so the muzzle, the flash, the
			// trail and the casings follow the gun wherever it is put.
			if (i == nViewWeapon && !bViewVeh)
			{
				const LTVector vGrip = m_weaponModel.VRGripOffset();
				if (vGrip.MagSqr() > 0.0f)
				{
					LTVector vGU, vGR, vGF;
					g_pLTClient->GetRotationVectors(&rObjNow, &vGU, &vGR, &vGF);
					const LTVector g = vGR * vGrip.x + vGU * vGrip.y + vGF * vGrip.z;
					vGripW = vCamR * g.x + vCamU * g.y + vCamF * g.z;
					p += vGripW;
				}
			}
		}
		// A CAMERA-RELATIVE MODEL THAT IS NOT THE GUN - the muzzle flash.
		//
		// NOLF's first-person muzzle flash is a scale FX, and a scale FX is an
		// OT_MODEL unless its bute says otherwise. It carries FLAG_REALLYCLOSE,
		// so GetObjectPos hands back a CAMERA-SPACE position - a few units from
		// the map origin, wherever the player happens to be. The view weapon
		// above converts its own; nothing converted anything else, so the flash
		// was published at the origin every time it fired. 
		// the muzzle flash was not attached to the gun.
		//
		// The census that found this is in the sprite path: 0 camera-relative
		// SPRITES near the origin and 12 camera-relative MODELS, which is what
		// says the flash is a model and was never mis-placed by the re-basing
		// code - it had simply never been offered to it.
		// ...and not in the INTERFACE SCENE, whose camera-relative models -
		// the menu's cards, bars and flowers - belong where they are. See the
		// same gate in the sprite publish.
		if (i != nViewWeapon && !bViewVeh && !bViewMod && VRPrims_RebaseEverKnown()
			&& g_vtVRCloseRebase.GetFloat() > 0.0f
			&& !m_InterfaceMgr.UseInterfaceCamera()
			&& (g_pLTClient->GetObjectFlags(objs[i]) & FLAG_REALLYCLOSE))
		{
			// BOTH POSITIONS, because one is not evidence. "re-based to 4231
			// 63 32" could be anywhere; "was at 3 -2 22, now at 4231 63 32,
			// and the gun is at 4230 64 31" is the claim and its check in one
			// line. Written for a headset session that nobody is reading live.
			const LTVector vWas = p;
			// THE MUZZLE FLASH GOES ON THE BARREL, not through the re-base.
			//
			// with the tracer fixed, the trail came out
			// of the barrel and the muzzle flash did not. They are different
			// objects taking different roads - the tracer starts at
			// m_vFlashPos, which is now the drawn barrel end, while the flash is
			// a scale FX carrying FLAG_REALLYCLOSE and comes through here, where
			// RebasePointLast expands it by fK (17). That expansion is right for
			// the camera-relative sprites and stays; it was never right for
			// something that should sit exactly where the barrel is drawn.
			//
			// The flash's three objects - the scale model, its particles and its
			// dynamic light - all belong at the same point.
			LTVector vDrawnMuz;
			const HLOCALOBJ hF  = m_weaponModel.VRGetFlashHandle();
			const HLOCALOBJ hFP = m_weaponModel.VRFlashParticleObject();
			const HLOCALOBJ hFL = m_weaponModel.VRFlashLightObject();
			const bool bIsFlash = (hF && objs[i] == hF) || (hFP && objs[i] == hFP)
							   || (hFL && objs[i] == hFL);
			const bool bHaveMuz = VRPrims_DrawnMuzzle(vDrawnMuz);
			const float fMuzFromEye = bHaveMuz ? (vDrawnMuz - vEye).Mag() : -1.0f;
			const bool bUsedMuz = (bIsFlash && bHaveMuz && fMuzFromEye <= 200.0f);
			// A FLASH MUST NEVER GO THROUGH THE RE-BASE. That is the twenty feet.
			//
			// RebasePointLast expands a camera-relative point by fK (17). For a
			// camera-relative SPRITE that is right and stays. For the muzzle
			// flash it is catastrophic: the authored muzzle offset is about
			// <2,-2,12>, and times 17 that is <34,-34,204> - 204 units forward
			// and 34 to the side, which is eleven feet out and off to the
			// right. Headset testing has now reported that twice, unchanged by making the
			// muzzle single-sourced, because the muzzle was never the part that
			// was wrong: the FALLBACK was.
			//
			// Measured here at the desk, first frame after a level load:
			//   "the model went through the RE-BASE (x17) - drawn muzzle
			//    available (2034 units from the eye)"
			// The muzzle is stale for a frame, the 200-unit guard correctly
			// refuses it, and the flash is then thrown across the room rather
			// than simply left on the gun.
			//
			// So fall back to the gun's own body, which is what the shell
			// casings use and is published by this same walk - it cannot be
			// staler than the frame it is computed in. The re-base is kept only
			// for the case where there is no drawn gun at all, which means
			// there is no weapon in view for the flash to sit on anyway.
			LTVector vGunBody;
			bool bUsedBody = false;
			if (bUsedMuz)
			{
				p = vDrawnMuz;
			}
			else if (bIsFlash && VRPrims_DrawnGunCentre(vGunBody)
					 && (vGunBody - vEye).Mag() <= 200.0f)
			{
				p = vGunBody;
				bUsedBody = true;
			}
			else
			{
				p = VRPrims_RebasePointLast(p);
			}

			// WHICH BRANCH, AND WHY. muzzle flashes were
			// way off to the right, about twenty feet away, unchanged by
			// making the muzzle single-sourced - so the flash is not reading
			// the muzzle at all and the reasoning about which object it is has
			// to stop and be measured instead.
			//
			// The else branch is the suspect: RebasePointLast expands a
			// camera-relative point by fK (17), and an authored muzzle offset
			// of about <2,-2,12> times 17 is <34,-34,204> - 204 units forward
			// and 34 right, which is 11 feet out and to the side. That is the
			// shape of what the tester is describing.
			//
			// The tester's earlier log said "flash handle NO" while particles and light
			// were present, so the three objects may not agree on the branch.
			// This prints each one separately for that reason.
			if (bIsFlash)
			{
				static int s_nSaidFlashBranch = 0;
				if (s_nSaidFlashBranch < 12)
				{
					++s_nSaidFlashBranch;
					const char* szWhich = (hF && objs[i] == hF) ? "model"
										: (hFP && objs[i] == hFP) ? "particles"
										: "light";
					const LTVector vFromEye = p - vEye;
					VRLog::Msg("VRFlashPos: the %s went %s - drawn muzzle %s"
							   " (%.0f units from the eye); ends up %.0f %.0f %.0f,"
							   " %.0f units from the eye",
							   szWhich,
							   bUsedMuz ? "ON THE MUZZLE"
									: bUsedBody ? "onto the GUN'S BODY (muzzle refused)"
												: "through the RE-BASE (x17)",
							   bHaveMuz ? "available" : "NOT AVAILABLE",
							   fMuzFromEye, p.x, p.y, p.z, vFromEye.Mag());
				}
			}
			static int s_nSaidCloseM = 0;
			if (s_nSaidCloseM < 8)
			{
				++s_nSaidCloseM;
				const VRViewRebase& rbG = VRPrims_RebaseLast();
				VRLog::Msg("VRClose: camera-relative MODEL re-based - was at"
						   " %.0f %.0f %.0f (camera space, near the map origin),"
						   " now %.0f %.0f %.0f; the gun it was put on is at"
						   " %.0f %.0f %.0f",
						   vWas.x, vWas.y, vWas.z, p.x, p.y, p.z,
						   rbG.vGunWorld.x, rbG.vGunWorld.y, rbG.vGunWorld.z);
			}
		}
		// A MOD'S POSITION goes through the gun's placement, as its nodes do
		// below, so the culling that reads fPos sees it where it is drawn.
		if (bViewMod && s_ModPlace.bValid)
		{
			const LTVector d(p.x - s_ModPlace.vGunRootPos.x, p.y - s_ModPlace.vGunRootPos.y, p.z - s_ModPlace.vGunRootPos.z);
			const LTVector r = VRMatMul(s_ModPlace.mPlace, VRMatMul(s_ModPlace.mRootT, d));
			const LTVector vLocal = s_ModPlace.vOrig * s_ModPlace.K + r * (s_ModPlace.K * s_ModPlace.S);
			p = vEye + (vCamR * vLocal.x + vCamU * vLocal.y + vCamF * vLocal.z) + s_ModPlace.vGripW;
			// THE SCOPE'S TWO ENDS, for the third pass and the eyepiece. Its
			// axis is the drawn barrel's (the aim rotation through the camera
			// basis, as the flash's shape is); its length and girth are the
			// engine's own dims for the object, through the same K*S the
			// nodes take. The eyepiece disc is a little inside the tube's
			// rear rim so the rim occludes its edge.
			if (i == nViewMod[1] && g_pPhysicsLT && VRShared::IsLive())
			{
				LTVector vDims(1.0f, 1.0f, 1.0f);
				g_pPhysicsLT->GetObjectDims(objs[i], &vDims);
				const float fKS = s_ModPlace.K * s_ModPlace.S;
				LTRotation rAxis = rCam * m_weaponModel.VRAimRot();
				LTVector vAU, vAR, vAF;
				g_pLTClient->GetRotationVectors(&rAxis, &vAU, &vAR, &vAF);
				// The engine's dims for an attachment are its animation box, wider
				// than the tube by a lot: at the full dims the eyepiece disc drew
				// 16 cm across on the gun. Fractions,
				// tunable: VRScopeRadiusK of the narrower half-dim for the disc,
				// VRScopeEyepieceK / VRScopeObjectiveK of the half-length for the
				// two ends (the objective a little AHEAD of the tube, so the pass
				// starts in open air).
				static VarTrack s_vtScRad, s_vtScEye, s_vtScObj;
				if (!s_vtScRad.IsInitted()) s_vtScRad.Init(g_pLTClient, "VRScopeRadiusK",    LTNULL, 0.18f);
				if (!s_vtScEye.IsInitted()) s_vtScEye.Init(g_pLTClient, "VRScopeEyepieceK",  LTNULL, 1.2f);
				if (!s_vtScObj.IsInitted()) s_vtScObj.Init(g_pLTClient, "VRScopeObjectiveK", LTNULL, 1.5f);
				const float fHalfLen = vDims.z * fKS;
				const float fRadMesh = ((vDims.x < vDims.y) ? vDims.x : vDims.y) * fKS * s_vtScRad.GetFloat();
				// PER GUN, THROUGH THE TUNER'S LENS MODES, as the integrated
				// scopes are. The fractions above are shared by every gun that
				// takes the scope, so one gun's lens could not be moved without
				// moving all of them; a gun with no LENS value draws exactly as
				// before.
				s_fVRLensMeshRadius = fRadMesh;
				const float fRadTune = m_weaponModel.VRLensRadius();
				const float fRadius  = (fRadTune > 0.0f) ? fRadTune : fRadMesh;
				const LTVector vTune = VRPrims_GunFrameToWorldLast(m_weaponModel.VRGunRot(), m_weaponModel.VRLensOffset());
				const LTVector vObjective = p + vAF * (fHalfLen * s_vtScObj.GetFloat()) + vTune;
				const LTVector vEyepiece  = p - vAF * (fHalfLen * s_vtScEye.GetFloat()) + vTune;
				const int nZ = VRScopeZoomLevel();
				const float fFov = (nZ <= 0) ? 20.0f : (nZ == 1 ? 7.0f : 2.0f);
				VRPrims_SetScopeLens(vObjective, rAxis, vEyepiece, vAR, vAU, fRadius, nZ, fFov);
				static float s_fSaidScope = -1.0f;
				if (fabsf(fRadius - s_fSaidScope) > 0.01f)
				{
					s_fSaidScope = fRadius;
					VRLog::Msg("VRScope: dims %.2f %.2f %.2f x K*S %.2f -> half-length %.1f, disc radius %.1f world units",
						vDims.x, vDims.y, vDims.z, fKS, fHalfLen, fRadius);
				}
			}
		}
		mi.fPos[0] = p.x; mi.fPos[1] = p.y; mi.fPos[2] = p.z;
		if (i == nViewWeapon && s_nGunNodes > 0 && s_nLastGunReportFrame + 45 < (long)s_frame.nFrame)
		{
			// the previous frame's nodes are the freshest complete set
			s_nLastGunReportFrame = (long)s_frame.nFrame;
			const LTVector vC = s_vGunSum / (float)s_nGunNodes;
			const LTVector vDir = s_vGunFar - vC;
			const float fH = sqrtf(vDir.x * vDir.x + vDir.z * vDir.z);
			const float fPitch = atan2f(vDir.y, fH) * 57.29578f;
			const float fYaw   = atan2f(vDir.x, vDir.z) * 57.29578f;
			const VRSharedState& vs2 = VRShared::State();
			float fBP = 0.0f, fBY = 0.0f, fBL = 0.0f;
			if (s_nBarrelHave == 3)
			{
				const LTVector vB = s_vBarrelB - s_vBarrelA;
				fBL = vB.Mag();
				fBP = atan2f(vB.y, sqrtf(vB.x * vB.x + vB.z * vB.z)) * 57.29578f;
				fBY = atan2f(vB.x, vB.z) * 57.29578f;
			}
			VRLog::Msg("  GUN DIR: far-node pitch %+.1f yaw %+.1f  | BARREL extru4->cyl6 pitch %+.1f yaw %+.1f (len %.1f)"
				"  | hand pitch %+.1f yaw %+.1f  head pitch %+.1f yaw %+.1f  | centre %+.1f %+.1f %+.1f from the eye",
				fPitch, fYaw, fBP, fBY, fBL,
				vs2.Hands[1].fPitchDeg, vs2.Hands[1].fYawDeg, vs2.fHeadPitchDeg, vs2.fHeadYawDeg,
				vC.x - vEye.x, vC.y - vEye.y, vC.z - vEye.z);
			s_nBarrelHave = 0;
		}

		LTVector d(16.0f, 16.0f, 16.0f);
		if (g_pPhysicsLT) g_pPhysicsLT->GetObjectDims(objs[i], &d);
		mi.fDims[0] = d.x; mi.fDims[1] = d.y; mi.fDims[2] = d.z;

		// THE OBJECT'S OWN SCALE. Measured before it was sent: all three
		// main-menu models - Cate, her sunglasses and the Walther - report
		// 1.300 uniform, and VRModelInst had nowhere to put it.
		{
			LTVector sc(1.0f, 1.0f, 1.0f);
			g_pLTClient->GetObjectScale(objs[i], &sc);
			mi.fScale[0] = sc.x; mi.fScale[1] = sc.y; mi.fScale[2] = sc.z;
		}
		// EVERY DYNAMIC LIGHT THE CLIENT CAN SEE, ONCE.
		//
		// The lamp investigation ran out of other explanations: the model, its
		// skin, its geometry, its alpha test and its object colour are all
		// correct, and the shade still draws white where retail draws it
		// yellow. What is left is the LIGHT falling on it - and this renderer
		// lights every model with a fixed directional stand-in, never with the
		// level's own lights. ltengineobjects.h even has a class for it:
		// "ObjectLight ... these lights only light objects, they don't light
		// the world."
		//
		// So: is there a warm light where the lamps are? One survey line, once
		// per run, before any of this is built.
		{
			// HOW MANY CASINGS AND MARKS ARE ACTUALLY ALIVE.
			//
			// In the headset, casings still disappear after about two full magazine
			// unloads (around 20 or so). The per-type ceiling in
			// s_nDynArrayMaxNums says 200 for casings, and SFX_SHELLCASING_ID
			// maps straight to that row, so the number the player sees is not that
			// cap. Reading more code was not converging; this counts them.
			{
				static float s_fNextFXCount = 0.0f;
				if (g_pLTClient->GetTime() > s_fNextFXCount)
				{
					s_fNextFXCount = g_pLTClient->GetTime() + 2.0f;
					CSpecialFXList* pShell = m_sfxMgr.GetFXList(SFX_SHELLCASING_ID);
					CSpecialFXList* pMark  = m_sfxMgr.GetFXList(SFX_MARK_ID);
					CSpecialFXList* pScale = m_sfxMgr.GetFXList(SFX_SCALE_ID);
					// AND WHERE THE CASINGS ARE. The count keeps climbing while
					// the tester sees them vanish, so they are not being deleted -
					// they are going somewhere. A casing that sinks through
					// the floor after coming to rest would look exactly like
					// one that disappeared, and would leave the count alone.
					float fLo = 1e9f, fHi = -1e9f, fSum = 0.0f; int nSeen = 0;
					if (pShell)
					{
						for (int z = 0; z < pShell->GetSize(); ++z)
						{
							CSpecialFX* pF = (*pShell)[z];
							if (!pF || !pF->GetObject()) continue;
							LTVector vp;
							g_pLTClient->GetObjectPos(pF->GetObject(), &vp);
							if (vp.y < fLo) fLo = vp.y;
							if (vp.y > fHi) fHi = vp.y;
							fSum += vp.y; ++nSeen;
						}
					}
					// AGAINST THE FLOOR, not against the eye. "90 units below
					// the camera" means nothing without knowing how high the
					// camera stands; the player object's own position is the
					// feet, and that is the floor the casings should be lying
					// on.
					float fFeet = vEye.y;
					HLOCALOBJ hPl = m_MoveMgr.GetObject();
					if (hPl)
					{
						LTVector vpl;
						g_pLTClient->GetObjectPos(hPl, &vpl);
						LTVector vpd(0.0f, 0.0f, 0.0f);
						if (g_pPhysicsLT) g_pPhysicsLT->GetObjectDims(hPl, &vpd);
						fFeet = vpl.y - vpd.y;		// the bottom of the player
					}
					// ABOVE, and say which way. The first version of this line
					// printed "N units BELOW THE FLOOR" from a subtraction
					// whose sign was the other way round, so a perfectly
					// healthy casing lying ON the floor was announced as
					// having sunk through it. An instrument that names a
					// direction has to get the direction right.
					VRLog::Msg("VRFXPos: %d casings, y from %.0f to %.0f, mean"
							   " %.0f | player feet %.0f, eye %.0f -> mean sits"
							   " %.0f units ABOVE the floor",
							   nSeen, nSeen ? fLo : 0.0f, nSeen ? fHi : 0.0f,
							   nSeen ? fSum / nSeen : 0.0f, fFeet, vEye.y,
							   nSeen ? (fSum / nSeen - fFeet) : 0.0f);
					VRLog::Msg("VRFXCount: casings %d of %d | marks %d of %d |"
							   " scale fx (blood) %d of %d",
							   pShell ? pShell->GetNumItems() : -1,
							   pShell ? pShell->GetSize() : -1,
							   pMark ? pMark->GetNumItems() : -1,
							   pMark ? pMark->GetSize() : -1,
							   pScale ? pScale->GetNumItems() : -1,
							   pScale ? pScale->GetSize() : -1);
				}
			}

			static int s_bSaidLights = 0;
			// NOT on the first frame that has any objects at all: that is the
			// loading state, six objects and no world. Wait for a populated
			// list or the survey reports the menu.
			if (!s_bSaidLights && nOut > 50)
			{
				s_bSaidLights = 1;
				uint32 nLights = 0;
				char szBuf[600]; szBuf[0] = 0;
				for (uint32 z = 0; z < nOut; ++z)
				{
					if (g_pLTClient->GetObjectType(objs[z]) != OT_LIGHT) continue;
					++nLights;
					float lr = 0.0f, lg = 0.0f, lb = 0.0f;
					g_pLTClient->GetLightColor(objs[z], &lr, &lg, &lb);
					const float rad = g_pLTClient->GetLightRadius(objs[z]);
					LTVector lp; g_pLTClient->GetObjectPos(objs[z], &lp);
					if (strlen(szBuf) < 480)
					{
						char szOne[128];
						sprintf_s(szOne, "[%.2f %.2f %.2f r%.0f @(%.0f %.0f %.0f)] ",
								  lr, lg, lb, rad, lp.x, lp.y, lp.z);
						strcat_s(szBuf, szOne);
					}
				}
				VRLog::Msg("VRLights: %u dynamic lights in the object list"
						   " (of %u objects). %s", nLights, nOut,
						   nLights ? szBuf : "NONE - the level's lighting is not"
						   " in the client object list at all");
			}
		}

		// AND THE COLOUR THE GAME SET ON IT. See VRShared.h: the world and
		// sprite publishes have always read this and the model publish never
		// has, which is why a white lampshade the game tints yellow comes out
		// white.
		{
			float mcr = 1.0f, mcg = 1.0f, mcb = 1.0f, mca = 1.0f;
			g_pLTClient->GetObjectColor(objs[i], &mcr, &mcg, &mcb, &mca);
			mi.fColour[0] = mcr; mi.fColour[1] = mcg;
			mi.fColour[2] = mcb; mi.fColour[3] = mca;
		}
		// WHICH PIECES THE GAME HAS HIDDEN. The engine keeps a per-object hide
		// status for a model's first 32 pieces and the renderer never saw it:
		// alternate pieces were drawn on top of each other (T01S01's credits
		// Cate, 'torso' and 'torso2'). The engine answers an error past the
		// last piece, which ends the walk. VRPieceHide 0 publishes none.
		mi.nHideMask = 0;
		if (GetConsoleInt("VRPieceHide", 1) > 0)
		{
			ILTModel* pML = g_pLTClient->GetModelLT();
			for (uint32 k = 0; pML && k < 32; ++k)
			{
				LTBOOL bHid = LTFALSE;
				if (pML->GetPieceHideStatus(objs[i], (HMODELPIECE)k, bHid) != LT_OK) break;
				if (bHid) mi.nHideMask |= (1u << k);
			}
		}

		// THE FRAME THE SCALE IS EXPRESSED IN, which the scale is useless
		// without as soon as it stops being uniform.
		//
		// fRot has been in VRModelInst since it was written and has never
		// been filled - the renderer scales each vertex's BONE-space offset,
		// which is only the same thing as an object-space scale while the
		// scale is the same on all three axes. Every character is (1.3 1.3
		// 1.3) so nothing showed. The intro van's headlight beams are
		// (6 6 320) on a model whose one node turns the mesh 90 degrees, so
		// the 320 landed on the wrong axis and drew a 1593-unit shaft
		// straight up out of each lamp.
		{
			// LTRotation stores m_Quat[4] and nothing else - QX QY QZ QW, which
			// is the order VRModelInst::fRot is documented in and the order
			// QuatBasis reads. There is no .x on this class.
			LTRotation r;
			g_pLTClient->GetObjectRotation(objs[i], &r);
			mi.fRot[0] = r.m_Quat[QX]; mi.fRot[1] = r.m_Quat[QY];
			mi.fRot[2] = r.m_Quat[QZ]; mi.fRot[3] = r.m_Quat[QW];

			// THE MUZZLE FLASH'S ORIENTATION AS PUBLISHED, against the gun's.
			// This is the rotation the engine poses the flash's nodes from and
			// the renderer skins, so it is the one that reaches the picture; a
			// readback taken earlier in the frame reported identity while the
			// picture showed the flash on the barrel's line. 1.00 = turns with
			// the gun. The tally counts frames under 0.99, so a flicker between
			// the two states cannot hide between the sampled lines.
			{
				const HLOCALOBJ hFl = m_weaponModel.VRGetFlashHandle();
				if (hFl && objs[i] == hFl && VRPrims_RebaseEverKnown())
				{
					static int s_nRotSeen = 0, s_nRotOff = 0, s_nRotSaid = 0;
					LTVector vU, vR, vF;
					g_pLTClient->GetRotationVectors(&r, &vU, &vR, &vF);
					LTVector vGunF = VRPrims_GunFrameToWorldLast(VRPrims_RebaseLast().rGunWorld, LTVector(0.0f, 0.0f, 1.0f));
					if (vGunF.Mag() > 0.001f) vGunF.Norm();
					const float fDot = vF.Dot(vGunF);
					++s_nRotSeen;
					if (fDot < 0.99f) ++s_nRotOff;
					if ((s_nRotSeen % 15) == 1 && s_nRotSaid < 200)
					{
						++s_nRotSaid;
						VRLog::Msg("VRFlashDrawnRot: flash fwd.gun fwd %.2f | frames seen %d, under 0.99: %d"
								   " | flash fwd %+.2f %+.2f %+.2f gun fwd %+.2f %+.2f %+.2f",
								   fDot, s_nRotSeen, s_nRotOff,
								   vF.x, vF.y, vF.z, vGunF.x, vGunF.y, vGunF.z);
					}
				}
			}
		}

		if (i == nViewWeapon)
		{
			// Where the VR gun is and where the retail one was, for the
			// camera-relative effects that hang off it. See VRViewRebase.
			VRViewRebase& rb = VRPrims_Rebase();
			rb.bKnown = true;
			rb.vEye = vEye; rb.vCamR = vCamR; rb.vCamU = vCamU; rb.vCamF = vCamF;
			rb.rCam = rCam;
			rb.vGunCam = vObjEngineCamRel;
			rb.vGunWorld.Init(mi.fPos[0], mi.fPos[1], mi.fPos[2]);
			rb.fK = (g_vtVRViewModelScale.GetFloat() > 0.1f) ? g_vtVRViewModelScale.GetFloat() : 17.0f;
			g_pLTClient->GetObjectRotation(objs[i], &rb.rGunWorld);

			// THE NUMBERS THE FLASH FIX TURNS ON, measured instead of reasoned
			// about. RebasePointLast does
			//     d = (vCam - vGunCam) * fK ;  out = vGunWorld + R*d
			// so a camera-space point offset from vGunCam by X comes out X*fK
			// world units from the gun. Publishing the flash with a raw
			// camera-space muzzle offset therefore threw it fK (17) times too
			// far and off screen, which is why it vanished.
			//
			// What is needed is the drawn gun's true world size: if the gun's
			// node spread is S world units and its camera-space spread is s,
			// the honest conversion is S/s, and that is what the authored
			// MuzzlePos must be multiplied by - not fK on faith.
			{
				static int s_nSaidRb = 0;
				if (s_nSaidRb < 3 && s_nGunNodes > 1)
				{
					++s_nSaidRb;
					const LTVector vC = s_vGunSum / (float)s_nGunNodes;
					const float fSpreadWorld = (s_vGunFar - vC).Mag();
					VRLog::Msg("VRRebase: gunCam %.2f %.2f %.2f | gunWorld %.0f %.0f %.0f"
							   " | fK %.1f | drawn node spread %.1f world units"
							   " (%.2f m) | eye %.0f %.0f %.0f | gun is %.0f units"
							   " from the eye",
							   rb.vGunCam.x, rb.vGunCam.y, rb.vGunCam.z,
							   rb.vGunWorld.x, rb.vGunWorld.y, rb.vGunWorld.z,
							   rb.fK, fSpreadWorld, fSpreadWorld * 0.01692f,
							   vEye.x, vEye.y, vEye.z,
							   (rb.vGunWorld - vEye).Mag());
				}
			}

			// THE MUZZLE FOR THIS FRAME, NOT LAST FRAME'S.
			//
			// This is the drift found in headset testing: when the gun moves at all
			// the flash does not travel with it properly, and the further out it
			// goes the more it gets away from the tip when the gun is swung.
			//
			// The flash was placed by VRPrims_GunPointFromOffsetLast, which is
			// correct in shape - gun position plus the authored offset in the
			// gun's own frame - but reads s_RebaseLast, the pose from the
			// PREVIOUS frame. A one-frame rotation lag puts the muzzle out by
			// about r * dTheta, which is zero when the gun is still and grows
			// with the arm length as it turns. That is the report exactly,
			// and it is why every static capture at my desk looked right: a
			// still hand has no lag to show.
			//
			// Here the current frame's gun position and rotation are both in
			// hand, so the muzzle is computed with no lag at all.
			{
				// THE AUTHORED OFFSET IS IN THE MODEL'S UNITS, AND THE MODEL IS
				// DRAWN SCALED. One factor, not forty-five hand-tuned numbers.
				//
				// The ak47's authored MuzzlePos forward is 16.54, and the flash
				// only reached its barrel tip at VRFlashOffF +24 - about 40 in
				// total, a ratio of 2.4. The view weapons draw at
				// VRWeaponScale 0.400, whose inverse is 2.5. That is not a
				// coincidence: the offset is expressed in the model's own units
				// and everything drawn from it has been scaled since, so the
				// offset has to be scaled the same way or it lands short by
				// exactly that factor - which is what a flash at the
				// magazine instead of the muzzle looks like on a long weapon.
				//
				// VRMuzzleScale overrides it for testing; 0 means "use the
				// weapon's own scale", which is the honest default because it is
				// per weapon already.
				LTVector vMzRaw = m_weaponModel.GetMuzzleOffset();
				if (VRShared::SwapHands()) vMzRaw.x = -vMzRaw.x;	// the Leftorium's mirrored gun
				float fMzK = g_vtVRMuzzleScale.GetFloat();
				if (fMzK <= 0.0f)
				{
					const float fWS = m_weaponModel.VRWeaponScale();
					fMzK = (fWS > 0.01f) ? (1.0f / fWS) : 1.0f;
				}
				const LTVector vMz = vMzRaw * fMzK;
				// THROUGH THE CAMERA BASIS, LIKE THE NODES. The node loop below
				// draws this gun as camBasis x R_obj; an offset rotated by R_obj
				// alone is one basis short and stays put in the world when the
				// body turns with the stick (headset testing, 20 September, three times
				// before it was believed). See VRPrims_GunFrameToWorld.
				const LTVector vMuzNow = rb.vGunWorld + VRPrims_GunFrameToWorld(rb, vMz);
				VRPrims_NoteDrawnMuzzle(vMuzNow);

				{
					static int s_nSaidMzK = 0;
					static int s_nMzKWeapon = -1;
					const int nW = (int)m_weaponModel.GetWeaponId();
					if (nW != s_nMzKWeapon && s_nSaidMzK < 12)
					{
						s_nMzKWeapon = nW; ++s_nSaidMzK;
						VRLog::Msg("VRMuzzleScale: authored %.2f %.2f %.2f x%.2f"
								   " -> %.2f %.2f %.2f (weapon scale %.3f)",
								   vMzRaw.x, vMzRaw.y, vMzRaw.z, fMzK,
								   vMz.x, vMz.y, vMz.z, m_weaponModel.VRWeaponScale());
					}
				}
			}

			// Keep it for the models published BEFORE the weapon next frame.
			VRPrims_RememberRebase();
		}

		if (objs[i] == hPlayer) mi.nFlags |= VRMODEL_F_PLAYER;
		// FLAG_NOLIGHT travels with the instance. The interface sets it
		// on everything it creates; in the world it is rarer but it means
		// the same thing, so this is not menu-only code.
		{
			uint32 dwOF = 0;
			g_pLTClient->Common()->GetObjectFlags(objs[i], OFT_Flags, dwOF);
			if (dwOF & FLAG_NOLIGHT) mi.nFlags |= VRMODEL_F_NOLIGHT;
			if (dwOF & FLAG_ENVIRONMENTMAP) mi.nFlags |= VRMODEL_F_ENVMAP;
			if (bViewVeh) mi.nFlags |= VRMODEL_F_VEHICLE;
			// Only the motorcycle: the full Snowmobile.abc has no bars of its
			// own (desk, 24 September - the gloves held nothing), so snow_pv
			// stays whole on top of it and the full model is the chassis.
			if (bViewVeh && s_hVRVehBody && s_nVRVehBodyKind == 1) mi.nFlags |= VRMODEL_F_HANDSONLY;
			// AND WHETHER THE ENGINE DRAWS IT AT ALL. Read from the same word
			// that was already fetched, so it costs nothing. Reported by the
			// renderer, not acted on here - see VRMODEL_F_INVISIBLE.
			// ...unless the player owns it. See the note at hOwned: a
			// first-person weapon is hidden in the object list and drawn by
			// the engine's close-up pass, so its cleared FLAG_VISIBLE means
			// "not drawn HERE", not "not drawn".
			// FLAG_PORTALVISIBLE IS THE ENGINE'S OWN "DRAW ME ANYWAY" BIT -
			// ltbasedefs.h says "Draw in portals even if the object is
			// invisible" - so an object carrying it is NOT hidden, it is
			// hidden from one path and drawn by another. That is the same
			// thing the player's weapon turned out to be, stated by the
			// engine instead of inferred by us: HideShowAttachments sets
			// exactly this bit when it clears FLAG_VISIBLE.
			//
			// It is a third of the instance-frames on M14S02, so this is not a
			// theoretical exception. The player list is kept as well, because
			// it covers the player's own object rather than its attachments.
			if (i == nViewWeapon || bViewVeh || bViewMod)
			{
				mi.nFlags |= VRMODEL_F_VIEWMODEL;
				// The same K the nodes were pushed out by. Without it the gun
				// is 17 times further away at its original size, which is a
				// speck.
				const float K = (g_vtVRViewModelScale.GetFloat() > 0.1f)
					? g_vtVRViewModelScale.GetFloat() : 17.0f;
				const float S = bViewVeh ? 1.0f
					: ((g_vtVRViewModelSize.GetFloat() > 0.01f)
					   ? g_vtVRViewModelSize.GetFloat() : 1.0f);
				mi.fScale[0] *= K * S; mi.fScale[1] *= K * S;
				mi.fScale[2] *= K * S;
			}
			// HIDDEN, AND WHOSE HIDING IT IS.
			//
			// FLAG_PORTALVISIBLE is the engine's "draw me anyway" bit and
			// exempting it was right for the world. It is exactly WRONG for
			// the player's own things, because HideShowAttachments sets that
			// bit AT THE SAME MOMENT it clears FLAG_VISIBLE - it is how NOLF
			// hides what Cate is carrying from her own eyes while keeping it
			// visible through a portal. So the exemption preserved precisely
			// the objects the game was hiding from the player, and her
			// Walther, her SMG and her S&W all drew floating in front of the
			// camera. Two guns and two sets of hands in the headset.
			//
			// So: an object the PLAYER owns that the engine has hidden is
			// hidden, full stop - PORTALVISIBLE does not rescue it. Anything
			// else keeps the exemption.
			//
			// Unless there is nothing else to draw. With the view model off,
			// the hand-held weapon is the ONLY weapon we have and taking it
			// away leaves the player empty-handed, which is worse than it
			// being in the wrong place. Measured: it is what disappeared when
			// this rule was first written without the player test.
			// WHAT IS SITTING INSIDE THE PLAYER'S HEAD, and what its flags are.
			//
			// Cate's carried Walther, SMG and S&W all draw floating in front of
			// the camera, and GetAttachments does not know about them - the
			// client reports 2 objects owned by the player, the player and one
			// attachment. So the player test cannot reach them and something
			// else has to. This says what they are.
			if (i != nViewWeapon && !bViewVeh)
			{
				const float ddx = p.x - vEye.x, ddy = p.y - vEye.y,
							ddz = p.z - vEye.z;
				const float d2n = ddx*ddx + ddy*ddy + ddz*ddz;
				if (d2n < 60.0f * 60.0f)
				{
					static int s_nNear = 0;
					if (s_nNear < 10)
					{
						++s_nNear;
						VRLog::Msg("VRNear: object %08X at %.1f units - flags"
							" %08X (visible %d portal %d)",
							(unsigned)(uintptr_t)objs[i], (float)sqrt(d2n),
							dwOF, (dwOF & FLAG_VISIBLE) ? 1 : 0,
							(dwOF & FLAG_PORTALVISIBLE) ? 1 : 0);
					}
				}
			}

			// Owned or not, for the renderer's camera-inside rule (see
			// VRMODEL_F_OWNED). Computed here once, used below as before.
			bool bOwnedAny = false;
			for (uint32 z = 0; z < nOwned; ++z)
				if (objs[i] == hOwned[z]) { bOwnedAny = true; break; }
			if (bOwnedAny) mi.nFlags |= VRMODEL_F_OWNED;

			if (!(dwOF & FLAG_VISIBLE))
			{
				const bool bOwned = bOwnedAny;

				// FLAG_PORTALVISIBLE MEANS "DRAW ME IN PORTALS". WE DO NOT
				// RENDER PORTALS.
				//
				// I read that flag as the engine's general "draw me anyway"
				// bit and exempted it. It is not general - ltbasedefs.h says
				// "Draw in portals even if the object is invisible", and a
				// portal view is not this view. HideShowAttachments sets it at
				// the same moment it clears FLAG_VISIBLE, which is how NOLF
				// hides what Cate carries from her own eyes while keeping it
				// visible through a portal.
				//
				// Measured, 40 to 47 units from the eye - on her body:
				//   object 03455210  flags 0000BC82  visible 0 portal 1
				//   object 03454FC8  flags 00000402  visible 0 portal 1
				//
				// Those are her Walther, her SMG and her S&W, and the
				// exemption drew every one of them floating in front of the
				// camera. GetAttachments cannot reach them either - the client
				// reports the player owning 2 objects - so the player test was
				// never going to catch this. The flag is the answer.
				//
				// KEPT ONLY AS THE FALLBACK. With no view model the hand-held
				// weapon is the only weapon there is, and an empty-handed
				// player is worse than one in the wrong place.
				const bool bHaveViewModel = (g_vtVRViewModel.GetFloat() > 0.0f);
				if (bHaveViewModel || !(dwOF & FLAG_PORTALVISIBLE))
					mi.nFlags |= VRMODEL_F_INVISIBLE;
				(void)bOwned;
			}
			// Additive lives in the SECOND flag word, not the first.
			uint32 dwOF2 = 0;
			g_pLTClient->Common()->GetObjectFlags(objs[i], OFT_Flags2, dwOF2);
			if (dwOF2 & FLAG2_ADDITIVE) mi.nFlags |= VRMODEL_F_ADDITIVE;
			if (dwOF2 & FLAG2_MULTIPLY) mi.nFlags |= VRMODEL_F_MULTIPLY;

			// THE FLAGS WE DO NOT ACT ON, COUNTED AS A GROUP.
			//
			// FLAG_VISIBLE was in this word since the publish block was
			// written and was never read, and it turned out to be why four
			// levels drew models untextured. That is the shape of the whole
			// class: a field that is present, free to read, and quietly
			// ignored does not announce itself - it presents as an art bug
			// somewhere else entirely.
			//
			// So rather than find the next one the same way, count them all.
			// A flag that never occurs in this game costs nothing to ignore
			// and can be struck off; one that occurs on hundreds of objects
			// is a real difference from retail waiting to be noticed.
			//
			// FLAG_REALLYCLOSE is the one to watch. It is how LithTech draws
			// an object that would otherwise clip through the near plane -
			// the first-person weapon - and we draw those in the ordinary
			// projection, which is a candidate for the weapon sitting higher
			// and larger in our frame than in retail's.
			{
				if (dwOF  & FLAG_REALLYCLOSE)   ++s_nFlagClose;
				if (dwOF  & FLAG_MODELTINT)     ++s_nFlagTint;
				if (dwOF  & FLAG_ENVIRONMENTMAP)++s_nFlagEnvMap;
				if (dwOF  & FLAG_DETAILTEXTURE) ++s_nFlagDetail;
				if (dwOF  & FLAG_SHADOW)        ++s_nFlagShadow;
				// NOT FOGDISABLE. THE FLAG BITS ARE SHARED BY OBJECT TYPE and
				// this list is MODELS, so bit 7 here is FLAG_ANIMTRANSITION -
				// "a 200ms transition between model animations" - and
				// FLAG_FOGDISABLE is what the same bit means on a WorldModel,
				// a sprite, a particle system or a canvas. ltbasedefs.h says
				// "only" in so many words.
				//
				// The first version of this counter called it FOGDISABLE and
				// reported 83-100% of instance-frames carrying it, which read
				// as "almost every model in the game is asking not to be
				// fogged and we fog it anyway" - an alarming and completely
				// false finding. It is just that almost every model animates
				// with a transition. An instrument that names a shared bit
				// after the wrong object type invents a defect.
				if (dwOF  & FLAG_ANIMTRANSITION) ++s_nFlagAnimTrans;
				if (dwOF  & FLAG_PORTALVISIBLE) ++s_nFlagPortal;
				++s_nFlagSeen;
			}
		}

		// The engine's own skeleton, posed this frame. Stored flat; the
		// instance says where its run starts.
		// THE PLAYER'S RIGHT ARM, so it can be bent toward the controller.
		//
		// Her skeleton is 25 nodes and the arm is a clean three-joint chain,
		// read out of CHARS/MODELS/HERO_ACTION.ABC offline:
		//
		//   Right_armu_node      ( 7.706, 31.840, -4.162)  shoulder
		//   Right_arml_node      (12.157, 17.244, -2.810)  elbow
		//   right_arml_hand_node (16.040,  3.937,  4.661)  hand
		//
		// which is a 15.3 unit upper arm and a 15.8 unit forearm - about 53 cm
		// of reach at 17 mm a unit, so it is a real arm and not a placeholder.
		// ILTModel::GetNode looks them up by name at runtime; the walk below
		// records where each lands in the published array so the transforms can
		// be rewritten after it.
		int nArmIdx[3] = { -1, -1, -1 };
		HMODELNODE hArm[3] = { INVALID_MODEL_NODE, INVALID_MODEL_NODE,
							   INVALID_MODEL_NODE };
		const bool bPlayerArm = (mi.nFlags & VRMODEL_F_PLAYER)
							 && (g_vtVRArmIK.GetFloat() > 0.0f);
		if (bPlayerArm)
		{
			static const char* kArm[3] = { "Right_armu_node",
										   "Right_arml_node",
										   "right_arml_hand_node" };
			for (int q = 0; q < 3; ++q)
				g_pLTClient->GetModelLT()->GetNode(objs[i],
					(char*)kArm[q], hArm[q]);
		}

		mi.nNodeFirst = s_frame.nNodeCount;
		bool bNodesFull = false;
		if (g_pLTClient->GetModelLT())
		{
			// THE GUN SITS ON THE HAND BY ITS BODY, NOT BY ITS AUTHORED ORIGIN.
			//
			// Fourth headset test: the gun was still far away. The log
			// has the object placed at the hand (+7 -19 +18 world units from
			// the eye) and the mesh's nodes drawn 40-55 units from it - the
			// view model is authored with the camera at its origin and the
			// gun a couple of units off, and the transform below multiplied
			// that offset by K*S (17 x 1.4), so the gun drew 0.6-0.9 m from
			// the hand. Worse, the offsets were taken about the HAND position
			// rather than the object's engine position, so the hand mostly
			// cancelled out of the picture.
			//
			// Now: offsets are about the object's ENGINE position, and the
			// mean of the node positions - the body of the gun - is what is
			// placed on the hand. VRGunCentre 0 puts the old arithmetic back.
			if (i == nViewWeapon && g_vtVRGunCentre.GetFloat() > 0.0f)
			{
				// THE PIVOT IS THE GUN, NOT THE CLOUD. Fifth headset test: the gun
				// rotated around a large round invisible object. The node
				// centroid includes the model's ARM bones, so the pivot sat
				// between the arms and the gun and turning the wrist swung the
				// gun on an arc. A node named for the gun is the pivot when
				// the model has one (VRGunAnchor 1/2); the names are logged
				// once per model so the rule can be checked against the file.
				HMODELNODE hA = INVALID_MODEL_NODE, hAN = INVALID_MODEL_NODE;
				LTVector vSum(0.0f, 0.0f, 0.0f); int nA = 0;
				LTVector vNamed(0.0f, 0.0f, 0.0f); int nNamedRank = 0;
				bGunRootFound = false;
				static HLOCALOBJ s_hNamesSaid = LTNULL;
				static int       s_nNamesSaidWeapon = -1;
				// Per weapon too, for the same reason as the rest pose below:
				// the revolver never had its nodes listed, because the handle
				// had already been listed for the Walther.
				const int nWeaponNow = (i == nViewWeapon) ? (int)m_weaponModel.GetWeaponId() : -1;
				const bool bSayNames = (s_hNamesSaid != objs[i] || s_nNamesSaidWeapon != nWeaponNow);
				char szNames[600] = ""; int nNamesLen = 0;
				while (g_pLTClient->GetNextModelNode(objs[i], hA, &hAN) == LT_OK)
				{
					hA = hAN;
					LTransform ta;
					if (g_pLTClient->GetModelLT()->GetNodeTransform(objs[i], hA, ta, LTTRUE) != LT_OK)
						continue;
					const LTVector vOff(ta.m_Pos.x - vObjEngineCamRel.x,
										ta.m_Pos.y - vObjEngineCamRel.y,
										ta.m_Pos.z - vObjEngineCamRel.z);
					vSum += vOff; ++nA;
					char szN[64] = "";
					g_pLTClient->GetModelNodeName(objs[i], hA, szN, sizeof(szN));
					// rank: an exact gun/weapon root beats a grip beats a muzzle
					// WALTHER_PV.ABC read from the file: 59 nodes, a left arm,
					// a right arm ending in wristR, and the gun as its own
					// subtree under a node called extru4 - nothing is named
					// "gun". The controller is IN the right hand, so the right
					// wrist bone is the pivot, and every player-view model
					// shares that rig.
					int nRank = 0;
					char szL[64]; strncpy(szL, szN, 63); szL[63] = 0; _strlwr(szL);
					// THE GUN'S OWN ROOT: the first node that is not the rig.
					// WALTHER_PV: Plot_null, two arms of finger bones, then
					// extru4 with the gun under it. The gun is placed RIGIDLY
					// from this node (see the transform below), so the
					// character's idle animation - which sways the whole arm
					// and the gun with it - cannot move the gun in the hand.
					// In the headset it moved while the hand was held perfectly still.
					if (!bGunRootFound && szL[0]
						&& !strstr(szL, "plot") && !strstr(szL, "arm") && !strstr(szL, "wrist")
						&& !strstr(szL, "thumb") && !strstr(szL, "pinky") && !strstr(szL, "ring")
						&& !strstr(szL, "middle") && !strstr(szL, "pointer") && !strstr(szL, "finger")
						&& !strstr(szL, "vol") && !strstr(szL, "hand") && !strstr(szL, "null"))
					{
						bGunRootFound = true;
						vGunRootPos = ta.m_Pos;
						rGunRoot = ta.m_Rot;
						strncpy(szGunRoot, szN, 63); szGunRoot[63] = 0;
					}
					if (strcmp(szL, "wristr") == 0) nRank = 4;
					else if (strstr(szL, "gun") || strstr(szL, "weapon") || strstr(szL, "pistol")
						|| strstr(szL, "rifle") || strstr(szL, "smg")) nRank = 3;
					else if (strstr(szL, "grip") || strstr(szL, "handle") || strstr(szL, "trigger")) nRank = 2;
					else if (strstr(szL, "flash") || strstr(szL, "muzzle")) nRank = 1;
					if (nRank > nNamedRank) { nNamedRank = nRank; vNamed = vOff; }
					if (bSayNames)
					{
						if (nNamesLen > 440)
						{
							VRLog::Msg("  view weapon nodes: %s", szNames);
							szNames[0] = 0; nNamesLen = 0;
						}
						nNamesLen += sprintf(szNames + nNamesLen, "%s(%+.2f %+.2f %+.2f) ", szN, vOff.x, vOff.y, vOff.z);
					}
				}
				if (nA > 0) vMeshAnchor = vSum / (float)nA;
				const int nAnchorMode = (int)g_vtVRGunAnchor.GetFloat();
				if (nAnchorMode >= 1 && nNamedRank > 0) vMeshAnchor = vNamed;
				// THE GUN ROOT'S REST ORIENTATION, captured once from the IDLE
				// pose. The rigid placement takes every node relative to the
				// root and puts the root at the hand - but a root with only
				// the controller's rotation shows the gun as authored in the
				// ROOT'S frame, and WALTHER_PV's root frame is rolled: the
				// gun came out upside down. The root's orientation relative
				// to the object in the idle pose is kept as a constant, so
				// the gun looks as it did at rest and still cannot sway.
				// ...OR AFTER TWO SECONDS OF WAITING. A weapon that never
				// reports W_IDLE - a gadget with its own state loop, a weapon
				// changed while firing - would stay hidden forever under the
				// rule below. Whatever pose it has by then is the rest pose.
				static int s_nRootWaitFrames = 0;
				static int s_nRootWaitWeapon = -2;
				const bool bNeedRoot = bGunRootFound && g_pMathLT
					&& (s_hRootRefObj != objs[i] || s_nRootRefWeapon != nWeaponNow || !s_bRootRefValid);
				if (bNeedRoot)
				{
					if (s_nRootWaitWeapon != nWeaponNow) { s_nRootWaitWeapon = nWeaponNow; s_nRootWaitFrames = 0; }
					++s_nRootWaitFrames;
				}
				const bool bRootTimeout = bNeedRoot && s_nRootWaitFrames > 180;
				if (bRootTimeout)
					VRLog::Msg("  view weapon: weapon %d never reached idle in %d frames (state %d) - taking its current pose as rest",
						nWeaponNow, s_nRootWaitFrames, (int)m_weaponModel.GetState());
				// A FRESH W_IDLE IS NOT A POSED GUN. Entering a level through its
				// select animation, the weapon reports W_IDLE only after that has
				// played, and the idle pose is what gets captured. Loading a save
				// (headset: fail a mission, load the save) the weapon is W_IDLE on
				// the very first frame, before its animation has posed the
				// skeleton - and that pose was taken as the rest orientation for
				// the rest of the level: the gun sat 30-40 degrees off in the hand
				// and the scope's lens, placed in the gun's frame, was off the
				// scope. Now W_IDLE has to hold for VRRootIdleFrames frames.
				static int       s_nIdleRun = 0;
				static int       s_nRootRecaptures = 0;
				static HLOCALOBJ s_hIdleObj = LTNULL;
				static int       s_nIdleWeapon = -2;
				if (s_hIdleObj != objs[i] || s_nIdleWeapon != nWeaponNow)
				{
					s_hIdleObj = objs[i]; s_nIdleWeapon = nWeaponNow; s_nIdleRun = 0;
					s_nRootRecaptures = 0;
				}
				if (m_weaponModel.GetState() == W_IDLE) ++s_nIdleRun; else s_nIdleRun = 0;
				const bool bIdleSettled = s_nIdleRun > (int)g_vtVRRootIdleFrames.GetFloat();
				// THE INSTRUMENT, AND THE SAFETY NET: the raw rest pose kept at
				// capture and measured again once the gun has been idle for 90
				// frames or more. The idle sway is a degree or two; a capture
				// taken from a pose the animation had not set yet is tens, and it
				// is thrown away and taken again (twice at most per gun object),
				// whatever put the bad pose there. VRRootRecaptureDeg 0: report only.
				static LTMatrix s_mRootRaw;
				static int      s_nSinceRootCapture = -1;
				if (s_nSinceRootCapture >= 0 && !bNeedRoot && bGunRootFound && g_pMathLT
					&& ++s_nSinceRootCapture > 1800)
					s_nSinceRootCapture = -1;		// twenty seconds and never idle: let it be
				// Two looks a third of a second apart, both idle: a retake only if
				// they agree with each other (the gun is holding a pose, not in the
				// middle of an idle fidget) and both disagree with the capture.
				static LTMatrix s_mRootCand;
				static int      s_nCandAt = -1;
				if (s_nSinceRootCapture >= 90 && !bNeedRoot && bGunRootFound && g_pMathLT
					&& s_nIdleRun > 20
					&& (s_nCandAt < 0 || s_nSinceRootCapture >= s_nCandAt + 30))
				{
					LTMatrix mR2, mO2, mO2T;
					g_pMathLT->SetupRotationMatrix(mR2, rGunRoot);
					g_pMathLT->SetupRotationMatrix(mO2, rObjNow);
					mO2T.Identity();
					for (int rr = 0; rr < 3; ++rr) for (int cc = 0; cc < 3; ++cc) mO2T.m[rr][cc] = mO2.m[cc][rr];
					const LTMatrix mNow = mO2T * mR2;
					auto fAngle = [](const LTMatrix& a, const LTMatrix& b) -> float
					{
						float fTr = 0.0f;
						for (int k = 0; k < 3; ++k)
							for (int q = 0; q < 3; ++q) fTr += a.m[q][k] * b.m[q][k];
						float fC = (fTr - 1.0f) * 0.5f;
						if (fC > 1.0f) fC = 1.0f; if (fC < -1.0f) fC = -1.0f;
						return RAD2DEG(acosf(fC));
					};
					const float fDrift = fAngle(s_mRootRaw, mNow);
					const float fLimit = g_vtVRRootRecaptureDeg.GetFloat();
					if (s_nCandAt < 0)
					{
						// First look.
						if (fLimit > 0.0f && fDrift > fLimit && s_nRootRecaptures < 2)
						{
							s_mRootCand = mNow;
							s_nCandAt = s_nSinceRootCapture;
						}
						else
						{
							VRLog::Msg("  view weapon: rest pose drift %.1f deg, %d frames after capture (weapon %d, idle %d frames)",
								fDrift, s_nSinceRootCapture, nWeaponNow, s_nIdleRun);
							s_nSinceRootCapture = -1;
						}
					}
					else
					{
						// Second look.
						const float fHold = fAngle(s_mRootCand, mNow);
						const bool  bRetake = fHold < 2.0f && fDrift > fLimit;
						VRLog::Msg("  view weapon: rest pose drift %.1f deg, %d frames after capture, pose held to %.1f deg"
							" (weapon %d, idle %d frames)%s", fDrift, s_nSinceRootCapture, fHold, nWeaponNow, s_nIdleRun,
							bRetake ? "  <- BAD CAPTURE, taking it again"
									: (fHold >= 2.0f ? "  (gun moving, looking again)" : ""));
						s_nCandAt = -1;
						if (bRetake)
						{
							++s_nRootRecaptures;
							s_bRootRefValid = false;
							s_nSinceRootCapture = -1;
						}
						else if (fDrift <= fLimit) s_nSinceRootCapture = -1;
					}
				}
				if (bNeedRoot && (bIdleSettled || bRootTimeout))
				{
					LTMatrix mRoot, mObj, mObjT;
					g_pMathLT->SetupRotationMatrix(mRoot, rGunRoot);
					g_pMathLT->SetupRotationMatrix(mObj, rObjNow);
					mObjT.Identity();
					for (int rr = 0; rr < 3; ++rr) for (int cc = 0; cc < 3; ++cc) mObjT.m[rr][cc] = mObj.m[cc][rr];
					s_mRootRef = mObjT * mRoot;
					{
						// The positive control: the first capture of the process, turned
						// about the object's up axis by VRRootCaptureSkew degrees.
						static bool s_bSkewed = false;
						const float fSkew = g_vtVRRootCaptureSkew.GetFloat();
						if (!s_bSkewed && fSkew != 0.0f)
						{
							s_bSkewed = true;
							const float c = cosf(DEG2RAD(fSkew)), s = sinf(DEG2RAD(fSkew));
							LTMatrix mY; mY.Identity();
							mY.m[0][0] = c; mY.m[0][2] = s; mY.m[2][0] = -s; mY.m[2][2] = c;
							s_mRootRef = mY * s_mRootRef;
							VRLog::Msg("  view weapon: rest pose SPOILED by %.0f deg on purpose (VRRootCaptureSkew)", fSkew);
						}
					}
					s_mRootRaw = s_mRootRef; s_nSinceRootCapture = 0; s_nCandAt = -1;
					// THE WAIT STARTS AGAIN FROM HERE. It was reset only on a weapon
					// CHANGE, so a level reloaded with the same gun found the two
					// seconds already spent and took whatever pose the first frames had.
					s_nRootWaitFrames = 0;
					s_hRootRefObj = objs[i];
					s_nRootRefWeapon = nWeaponNow;
					s_bRootRefValid = true;
					VRLog::Msg("  view weapon: gun root '%s' rest orientation captured from the idle pose (weapon %d, idle %d frames)",
						szGunRoot, nWeaponNow, s_nIdleRun);
					// THE BARREL ALIGNED TO THE HAND, PER WEAPON. The line
					// from the gun's root node to its Flash socket (the
					// muzzle) is the barrel; in the object's frame at rest it
					// should point straight down +z, the hand's forward. One
					// constant (VRGunTrimPitch 11.7) fitted the Walther and
					// left the sub-machine gun pointing below its aim dot
					//. The correction that swings that
					// line onto +z is folded into the rest orientation, so
					// every node comes along. Tried in both senses of the
					// engine's axis rotation and the one that lands is kept.
					// ...PER WEAPON. The line it aligns is between mesh pivots, not a
					// barrel, and on a model whose pivots all sit at the root there
					// is no line at all. The Contender's nodes are every one of them
					// at the origin bar one a quarter of a unit away, and that
					// quarter-unit was taken as the barrel: the gun was swung 37
					// degrees up and 15 across, differently on each run (32/-16,
					// 37/-15), and the flash, the burst and the shot went with the
					// swing while the drawn gun did not.
					// VRAutoTrim@<weapon> 0 in vrtune.cfg leaves that weapon as
					// authored; the guns tuned on top of the alignment keep it.
					static VRWeaponVar s_vrAutoTrim;
					static bool s_bAutoTrimInit = false;
					if (!s_bAutoTrimInit) { s_bAutoTrimInit = true; s_vrAutoTrim.Init("VRAutoTrim", 1.0f); }
					const bool bAutoTrimHere = s_vrAutoTrim.Get(nWeaponNow) > 0.0f;
					if (g_vtVRGunAutoTrim.GetFloat() > 0.0f && !bAutoTrimHere)
					{
						char szSlugT[64] = ""; VRWeaponSlugForId(nWeaponNow, szSlugT, sizeof(szSlugT));
						VRLog::Msg("  view weapon: barrel left as authored on weapon %d - VRAutoTrim@%s is 0", nWeaponNow, szSlugT);
						VRAlignTrimSet(nWeaponNow, 0.0f, 0.0f);
					}
					if (g_vtVRGunAutoTrim.GetFloat() > 0.0f && bAutoTrimHere && g_pModelLT)
					{
						// THE MUZZLE IS THE WEAPON'S AUTHORED MuzzlePos (the
						// attribute the flash draws at), in the object's own
						// axes; no PV model carries a "Flash" socket (14
						// September: every weapon said so). The root node's
						// offset from the object, taken into the same axes,
						// gives the barrel as root -> muzzle - the line the
						// Walther's validated trim was fitted to.
						// ...FROM THE WEAPON'S AUTHORED POSITION (vPos, where the
						// retail renderer put the gun's origin) to its authored
						// MuzzlePos, both in camera axes, which are the object's
						// axes in the retail placement. The root NODE sits at the
						// object's origin on every weapon (measured: root offset
						// under 0.4 units), so it added nothing; and the origin
						// -> muzzle line taken from the CAMERA was the muzzle's
						// place below the eye, not the barrel. For the Walther
						// this line should come out near the +11.7 degrees the player's
						// validated trim took off it.
						// THE GUN'S SHAPE IN THE HAND'S FRAME, once per weapon: every
						// non-arm node's offset from the root, so the barrel line
						// can be chosen from data rather than guessed. Dumped
						// 14 September for exactly that.
						LTVector aShape[96]; char aShapeName[96][32]; int nShapeNodes = 0;
						{
							char szShape[1400] = ""; int nLen = 0;
							HMODELNODE hS = INVALID_MODEL_NODE, hSN = INVALID_MODEL_NODE;
							nShapeNodes = 0;
							while (g_pLTClient->GetNextModelNode(objs[i], hS, &hSN) == LT_OK && nLen < 1300)
							{
								hS = hSN;
								LTransform tn;
								if (g_pLTClient->GetModelLT()->GetNodeTransform(objs[i], hS, tn, LTTRUE) != LT_OK) continue;
								char szNm[64] = ""; g_pLTClient->GetModelNodeName(objs[i], hS, szNm, sizeof(szNm));
								char szLo[64]; strncpy(szLo, szNm, 63); szLo[63] = 0; _strlwr(szLo);
								if (!szLo[0]) continue;		// the model's nameless top node is not a gun part
								if (strstr(szLo, "arm") || strstr(szLo, "wrist") || strstr(szLo, "thumb") || strstr(szLo, "pinky")
									|| strstr(szLo, "ring") || strstr(szLo, "middle") || strstr(szLo, "pointer") || strstr(szLo, "finger")
									|| strstr(szLo, "vol") || strstr(szLo, "hand") || strstr(szLo, "null") || strstr(szLo, "plot")) continue;
								const LTVector wd(tn.m_Pos.x - vGunRootPos.x, tn.m_Pos.y - vGunRootPos.y, tn.m_Pos.z - vGunRootPos.z);
								LTVector od;
								od.x = mObjT.m[0][0]*wd.x + mObjT.m[0][1]*wd.y + mObjT.m[0][2]*wd.z;
								od.y = mObjT.m[1][0]*wd.x + mObjT.m[1][1]*wd.y + mObjT.m[1][2]*wd.z;
								od.z = mObjT.m[2][0]*wd.x + mObjT.m[2][1]*wd.y + mObjT.m[2][2]*wd.z;
								nLen += sprintf(szShape + nLen, "%s(%+.2f %+.2f %+.2f) ", szNm, od.x, od.y, od.z);
								if (nShapeNodes < 96) { aShape[nShapeNodes] = od; strncpy(aShapeName[nShapeNodes], szNm, 31); aShapeName[nShapeNodes][31] = 0; ++nShapeNodes; }
							}
							VRLog::Msg("  view weapon %d shape from root '%s' in the hand's frame: %s", nWeaponNow, szGunRoot, szShape);
						}
						// THE BARREL LINE, FROM THE SHAPE. The tip is the node
						// farthest forward of the root. The base is the ROOT when
						// the root sits in line with the tip (within a quarter
						// of the distance, laterally) - which is the Walther,
						// whose root is its frame at barrel height, and whose
						// root->tip line is the one the tester's validated trim aligned;
						// otherwise the farthest node that IS in line with the
						// tip - which is the sub-machine gun, whose root is the
						// grip: root->tip climbed 16 degrees while its barrel
						// (receiver->muzzle) sits 3 below, so one constant fitted
						// to the pistol left it pointing under its own dot.
						LTVector vMuz(0.0f, 0.0f, 0.0f); LTVector r(0.0f, 0.0f, 0.0f);
						char szTipName[32] = "", szBaseName[32] = "root";
						{
							int nTip = -1; float fTipZ = 0.1f;
							for (int k = 0; k < nShapeNodes; ++k)
								if (aShape[k].z > fTipZ) { fTipZ = aShape[k].z; nTip = k; }
							if (nTip >= 0)
							{
								vMuz = aShape[nTip]; strncpy(szTipName, aShapeName[nTip], 31);
								const float fRootLat = sqrtf(vMuz.x*vMuz.x + vMuz.y*vMuz.y);
								if (fRootLat <= 0.25f * vMuz.Mag()) { r.Init(); }
								else
								{
									float fBest = 0.3f * vMuz.z; int nBase = -1;
									for (int k = 0; k < nShapeNodes; ++k)
									{
										if (k == nTip) continue;
										const LTVector d(vMuz.x - aShape[k].x, vMuz.y - aShape[k].y, vMuz.z - aShape[k].z);
										const float fD = d.Mag();
										if (fD <= fBest) continue;
										const float fLat = sqrtf(d.x*d.x + d.y*d.y);
										if (fLat <= 0.25f * fD) { fBest = fD; nBase = k; }
									}
									if (nBase >= 0) { r = aShape[nBase]; strncpy(szBaseName, aShapeName[nBase], 31); }
								}
							}
						}
						// CAPTURED ONCE, KEPT FOR EVER. The line between two pivots depends
						// on the pose the capture happened to sample: the P38 read
						// root->cyl6 +12/0 in twelve captures and root->cube3 -22/+29 in
						// two, the Luger swapped between -6/+1 and +11/-2 from one load to
						// the next. Every load could therefore seat the gun differently,
						// and the shot line with it (level tour, 21 September). So the
						// first alignment a weapon ever gets is written to vrtune.cfg as
						// VRAlignPitch/VRAlignYaw@<weapon>, and from then on the file is
						// the alignment - the same numbers every tuned grip, flash and
						// angle were set against. Delete the two lines to re-capture.
						LTVector b(0.0f, 0.0f, 0.0f); bool bHaveB = false, bFromFile = false;
						char szSlugA[64] = "", szVP[96] = "", szVY[96] = "";
						VRWeaponSlugForId(nWeaponNow, szSlugA, sizeof(szSlugA));
						if (szSlugA[0])
						{
							sprintf(szVP, "VRAlignPitch@%s", szSlugA);
							sprintf(szVY, "VRAlignYaw@%s", szSlugA);
							HCONSOLEVAR hP = g_pLTClient->GetConsoleVar(szVP);
							HCONSOLEVAR hY = g_pLTClient->GetConsoleVar(szVY);
							if (hP && hY)
							{
								const float fP = g_pLTClient->GetVarValueFloat(hP) * 0.01745329f;
								const float fY = g_pLTClient->GetVarValueFloat(hY) * 0.01745329f;
								b.x = cosf(fP) * sinf(fY); b.y = sinf(fP); b.z = cosf(fP) * cosf(fY);
								bHaveB = true; bFromFile = true;
							}
						}
						if (!bHaveB && vMuz.Mag() > 0.01f)
						{
							b.x = vMuz.x - r.x; b.y = vMuz.y - r.y; b.z = vMuz.z - r.z;
							const float fLen = b.Mag();
							if (fLen > 0.01f) { b /= fLen; bHaveB = true; }
						}
						if (bHaveB)
						{
							const float fPitchB = atan2f(b.y, sqrtf(b.x*b.x + b.z*b.z)) * 57.29578f;
							const float fYawB   = atan2f(b.x, b.z) * 57.29578f;
							LTVector axis(b.y * 1.0f - b.z * 0.0f, b.z * 0.0f - b.x * 1.0f, b.x * 0.0f - b.y * 0.0f);	// b x z
							const float fSin = axis.Mag();
							const float fCos = b.z;
							const float fAng = atan2f(fSin, fCos);
							float fBestZ = 1.0f;
							if (fSin > 1e-4f)
							{
								axis /= fSin;
								LTMatrix mBest; mBest.Identity(); fBestZ = -2.0f;
								for (int sgn = 0; sgn < 2; ++sgn)
								{
									LTRotation rC; rC.Init();
									g_pLTClient->RotateAroundAxis(&rC, &axis, sgn ? -fAng : fAng);
									LTMatrix mC; g_pMathLT->SetupRotationMatrix(mC, rC);
									const float bz = mC.m[2][0]*b.x + mC.m[2][1]*b.y + mC.m[2][2]*b.z;
									if (bz > fBestZ) { fBestZ = bz; mBest = mC; }
								}
								s_mRootRef = mBest * s_mRootRef;
							}
							VRAlignTrimSet(nWeaponNow, fPitchB, fYawB);
							if (!bFromFile && szVP[0])
							{
								VRTuneSave::Remember(szVP, fPitchB);
								VRTuneSave::Remember(szVY, fYawB);
								VRTuneSave::Write();
							}
							VRLog::Msg("  view weapon: barrel %s pitch %+.1f yaw %+.1f in the hand's frame"
									   " (%s) - aligned to the hand (lands at %.3f of forward), weapon %d (VRGunAutoTrim)",
								bFromFile ? "FROM vrtune.cfg" : szBaseName, fPitchB, fYawB,
								bFromFile ? "kept from the file, not re-derived" : "from the pivots - SAVED to vrtune.cfg", fBestZ, nWeaponNow);
						}
						else
						{
							VRLog::Msg("  view weapon: no forward node on weapon %d - barrel left as authored (saved as 0/0)", nWeaponNow);
							VRAlignTrimSet(nWeaponNow, 0.0f, 0.0f);
							if (szVP[0]) { VRTuneSave::Remember(szVP, 0.0f); VRTuneSave::Remember(szVY, 0.0f); VRTuneSave::Write(); }
						}
					}
				}
				if (bSayNames)
				{
					s_hNamesSaid = objs[i];
					s_nNamesSaidWeapon = nWeaponNow;
					VRLog::Msg("  view weapon nodes (%d): %s", nA, szNames);
					VRLog::Msg("  view weapon anchor: %s rank %d at %+.2f %+.2f %+.2f; gun root %s",
						(nAnchorMode >= 1 && nNamedRank > 0) ? "NAMED node" : "centroid",
						nNamedRank, vMeshAnchor.x, vMeshAnchor.y, vMeshAnchor.z,
						bGunRootFound ? szGunRoot : "NOT FOUND (rigid placement off)");
				}
				// NOT DRAWN UNTIL ITS REST POSE IS KNOWN. The rigid placement
				// needs the root's rest orientation from the IDLE pose, and a
				// freshly selected weapon spends its select animation without
				// one - so it drew in the raw root frame, upside down, until
				// the idle came round. In the headset the revolver appeared in the
				// hand upside down and a moment later loaded the correct way.
				// A moment with no gun beats a moment with a wrong one.
				if (i == nViewWeapon && g_vtVRGunRigid.GetFloat() > 0.0f && bGunRootFound
					&& !(s_bRootRefValid && s_hRootRefObj == objs[i] && s_nRootRefWeapon == nWeaponNow))
				{
					mi.nFlags |= VRMODEL_F_INVISIBLE;
					static int s_nHidSaid = 0;
					if (s_nHidSaid++ < 6)
						VRLog::Msg("  view weapon: hidden until its rest pose is captured (weapon %d, state %d)",
							nWeaponNow, (int)m_weaponModel.GetState());
				}
				// The three gates of the rigid placement, once a second.
				if (i == nViewWeapon)
				{
					static uint32 s_nGateSaid = 0;
					if (s_frame.nFrame >= s_nGateSaid + 90)
					{
						s_nGateSaid = s_frame.nFrame;
						if (g_vtVRLogWeaponPos.GetFloat() > 0.0f)
							VRLog::Msg("  view weapon gates: weapon %d state %d root %s '%s' captured %s (for weapon %d, obj %s) hidden %s",
								nWeaponNow, (int)m_weaponModel.GetState(),
								bGunRootFound ? "found" : "NOT FOUND", szGunRoot,
								s_bRootRefValid ? "yes" : "no", s_nRootRefWeapon,
								(s_hRootRefObj == objs[i]) ? "same" : "DIFFERENT",
								(mi.nFlags & VRMODEL_F_INVISIBLE) ? "yes" : "no");
					}
				}
				static float s_fSaidAnchor = -99999.0f;
				if (fabsf(vMeshAnchor.x + vMeshAnchor.y + vMeshAnchor.z - s_fSaidAnchor) > 0.05f)
				{
					s_fSaidAnchor = vMeshAnchor.x + vMeshAnchor.y + vMeshAnchor.z;
					if (g_vtVRLogWeaponPos.GetFloat() > 0.0f)
						VRLog::Msg("  view weapon mesh anchor: %+.2f %+.2f %+.2f cam-units from"
							" the object (%d nodes); object engine pos %+.2f %+.2f %+.2f",
						vMeshAnchor.x, vMeshAnchor.y, vMeshAnchor.z, nA,
						vObjEngineCamRel.x, vObjEngineCamRel.y, vObjEngineCamRel.z);
				}
			}

			if (i == nViewWeapon)
			// -1e9, not -1: s_fGunFar now holds a PROJECTION along the barrel,
			// which is negative for every node behind the eye. A -1 sentinel
			// would have rejected them all and kept last frame's node.
			{ s_vGunSum = LTVector(0.0f, 0.0f, 0.0f); s_nGunNodes = 0; s_fGunFar = -1.0e9f; s_vGunFar = s_vGunSum; }
			HMODELNODE hNode = INVALID_MODEL_NODE, hNext = INVALID_MODEL_NODE;
			// UNCHANGED PROP: last frame's nodes (see s_VRNodeCache).
			bool bNodeCacheable = false, bNodeReused = false;
			VRNodeCacheEnt keyNow;
			if (g_vtVRNodeReuse.GetFloat() > 0.0f && i != nViewWeapon && !bViewVeh
				&& !bViewMod && !bPlayerArm && !(mi.nFlags & VRMODEL_F_PLAYER))
			{
				CSFXMgr* pSFXn = GetSFXMgr();
				LTAnimTracker* pTr = LTNULL;
				if (pSFXn && !pSFXn->FindSpecialFX(SFX_CHARACTER_ID, objs[i])
					&& !pSFXn->FindSpecialFX(SFX_BODY_ID, objs[i])
					&& g_pLTClient->GetModelLT()->GetMainTracker(objs[i], pTr) == LT_OK && pTr)
				{
					HMODELANIM hA = INVALID_MODEL_ANIM;
					uint32 nT = 0;
					g_pLTClient->GetModelLT()->GetCurAnim(pTr, hA);
					g_pLTClient->GetModelLT()->GetCurAnimTime(pTr, nT);
					LTRotation rN;
					g_pLTClient->GetObjectRotation(objs[i], &rN);
					keyNow.fPos[0] = mi.fPos[0]; keyNow.fPos[1] = mi.fPos[1]; keyNow.fPos[2] = mi.fPos[2];
					keyNow.fRot[0] = rN.m_Quat[0]; keyNow.fRot[1] = rN.m_Quat[1];
					keyNow.fRot[2] = rN.m_Quat[2]; keyNow.fRot[3] = rN.m_Quat[3];
					keyNow.fScale[0] = mi.fScale[0]; keyNow.fScale[1] = mi.fScale[1]; keyNow.fScale[2] = mi.fScale[2];
					keyNow.nAnim = (uint32)hA;
					keyNow.nTime = nT;
					keyNow.nPlaying = (g_pLTClient->GetModelLT()->GetPlaying(pTr) == LT_YES) ? 1 : 0;
					bNodeCacheable = true;
					std::map<HOBJECT, VRNodeCacheEnt>::const_iterator itN = s_VRNodeCache.find(objs[i]);
					if (itN != s_VRNodeCache.end())
					{
						const VRNodeCacheEnt& c = itN->second;
						const uint32 n = (uint32)c.nodes.size();
						if (n && memcmp(c.fPos, keyNow.fPos, sizeof(c.fPos)) == 0
							&& memcmp(c.fRot, keyNow.fRot, sizeof(c.fRot)) == 0
							&& memcmp(c.fScale, keyNow.fScale, sizeof(c.fScale)) == 0
							&& c.nAnim == keyNow.nAnim && c.nTime == keyNow.nTime
							&& c.nPlaying == keyNow.nPlaying
							&& s_frame.nNodeCount + n <= VRMODELS_MAX_NODES)
						{
							memcpy(&s_frame.nodes[s_frame.nNodeCount], &c.nodes[0], n * sizeof(VRModelNode));
							s_frame.nNodeCount += n;
							mi.nNodeCount = n;
							bNodeReused = true;
							++s_nVRNodeReused;
						}
					}
				}
			}
			if (!bNodeReused) ++s_nVRNodeQueried;
			if (s_nVRNodeQueried + s_nVRNodeReused >= 200000)
			{
				VRLog::Msg("VRNodeReuse: %ld of %ld model instances republished last frame's nodes (%.0f%%), %u cached",
					s_nVRNodeReused, s_nVRNodeQueried + s_nVRNodeReused,
					100.0 * (double)s_nVRNodeReused / (double)(s_nVRNodeQueried + s_nVRNodeReused),
					(unsigned)s_VRNodeCache.size());
				s_nVRNodeReused = s_nVRNodeQueried = 0;
			}
			if (!bNodeReused)
			while (g_pLTClient->GetNextModelNode(objs[i], hNode, &hNext) == LT_OK)
			{
				hNode = hNext;
				if (s_frame.nNodeCount >= VRMODELS_MAX_NODES) { bNodesFull = true; break; }
				// LTransform, not a matrix: a position and a rotation. Stored
				// as the three basis rows plus the translation, so the renderer
				// can multiply a vertex by it without knowing about quaternions.
				// Publish a node for EVERY skeleton node, posed or not.
				//
				// The node index in the mesh is an ordinal into this object's
				// skeleton. Skipping a node that fails to pose shifted every
				// index after it by one, so every vertex from that point on was
				// placed by a neighbouring bone - valid indices, correct-looking
				// counts, and geometry stretched between a right position and a
				// wrong one. That is what the long thin beams were.
				//
				// A zero matrix is the sentinel: no rotation basis is all-zero,
				// so the renderer can tell an unposed node from a real one and
				// drop the vertices bound to it instead of placing them at the
				// world origin.
				LTransform tf;
				if (bPlayerArm)
				{
					for (int q = 0; q < 3; ++q)
						if (hNode == hArm[q] && hArm[q] != INVALID_MODEL_NODE)
							nArmIdx[q] = (int)s_frame.nNodeCount;
				}
				VRModelNode& vn = s_frame.nodes[s_frame.nNodeCount++];
				if (g_pLTClient->GetModelLT()->GetNodeTransform(objs[i], hNode,
						tf, LTTRUE) != LT_OK)
				{
					memset(vn.m, 0, sizeof(vn.m));
					++mi.nNodeCount;			// keep the INDEX aligned
					++s_nUnposedNodes;
					continue;
				}
				LTMatrix mat;
				if (g_pMathLT) g_pMathLT->SetupRotationMatrix(mat, tf.m_Rot);
				else mat.Identity();
				// THE VIEW WEAPON'S NODES ARE CAMERA-RELATIVE, like its
				// position, because "world space" for an object the engine
				// keeps in camera space IS camera space. The renderer skins
				// from these transforms and draws in the world, so they have to
				// be carried across: rotate by the camera's basis and offset by
				// the camera's position.
				if (i == nViewWeapon || bViewVeh)
				{
					// WHAT THE ENGINE POSED, before we touch it. The object's
					// position is written by the weapon update; whether that
					// reaches the NODES - which is what the renderer skins
					// from - is a separate question, and the one that decides
					// whether any of this can work.
					static float s_fSaidNode = -99999.0f;
					if (mi.nNodeCount == 0
						&& fabsf(tf.m_Pos.y - s_fSaidNode) > 0.01f)
					{
						s_fSaidNode = tf.m_Pos.y;
						if (g_vtVRLogWeaponPos.GetFloat() > 0.0f)
							VRLog::Msg("  view weapon node0 posed at %+.2f %+.2f %+.2f"
								" (object at %+.2f %+.2f %+.2f)",
								tf.m_Pos.x, tf.m_Pos.y, tf.m_Pos.z, p.x, p.y, p.z);
					}
					// SCALED OUT OF THE NEAR PLANE, and this is the number
					// the whole feature turns on.
					//
					// Published faithfully, the view weapon lands 1 unit from
					// the eye - measured, delta (-0.4 -0.6 +1.1). At 17 mm a
					// unit that is two centimetres from your eyeball, inside
					// the near plane, which is why it resolved its textures,
					// counted among the drawn instances and could not be seen.
					//
					// The camera-relative space the retail path positions this
					// object in is NOT world units; FLAG_REALLYCLOSE is how the
					// engine draws it and we do not implement that. Two
					// independent derivations put the factor at about 20: the
					// authored offset implies 2.8 units per metre against the
					// world's 58.75, and a weapon that should sit 45 cm out
					// (26 units) is being placed at 1.5.
					//
					// So push it out by K and scale the model by the same K -
					// same angular size, outside the near plane.
					// DISTANCE AND SIZE ARE TWO DECISIONS AND THIS USED TO BE
					// ONE NUMBER.
					//
					// Scaling every node's position about the EYE by K moves
					// the model K times further away AND spreads it K times
					// wider, so its ANGULAR SIZE never changes - measured, the
					// arm covers 19148, 19398 and 19177 pixels at K = 8, 17 and
					// 34. K is doing exactly one job, which is getting the
					// model out of the near plane, and there was no way at all
					// to make it smaller.
					//
					// Split: the model's ORIGIN moves out by K, and the model's
					// own extent about that origin scales by K*S. S is the size
					// lever, and at S=1 this is identical to what it replaced.
					const float K = (g_vtVRViewModelScale.GetFloat() > 0.1f)
						? g_vtVRViewModelScale.GetFloat() : 17.0f;
					const float S = bViewVeh ? 1.0f
						: ((g_vtVRViewModelSize.GetFloat() > 0.01f)
						   ? g_vtVRViewModelSize.GetFloat() : 1.0f);
					// p is the object's origin, already carried into world
					// space above; pOrigLocal is it back in camera space.
					const LTVector vOrig = vOrigCamRel;
					LTVector t = tf.m_Pos;
					LTVector vRel(t.x - vOrig.x, t.y - vOrig.y,
								  t.z - vOrig.z);
					// fVehDropCam: the ridden handlebars lowered (VRVehicleDrop).
					if (bViewVeh && K > 0.0f)
					{
						vRel.y -= g_vtVRVehicleDrop.GetFloat() / K;
						vRel.z -= g_vtVRVehicleBack.GetFloat() / K;
					}
					if (!bViewVeh && g_vtVRGunCentre.GetFloat() > 0.0f)
					{
						// see the pre-pass above the node loop
						vRel.x = t.x - vObjEngineCamRel.x - vMeshAnchor.x;
						vRel.y = t.y - vObjEngineCamRel.y - vMeshAnchor.y;
						vRel.z = t.z - vObjEngineCamRel.z - vMeshAnchor.z;
					}
					// THE GUN IS RIGID ON THE HAND. In the headset it felt like Cate's
					// idle animation was driving the gun rather than the gun model
					// being mapped to the controller. Every node is taken RELATIVE to
					// the gun's own root node - position in the root's frame,
					// rotation as root^-1 * node - which strips the animation's
					// movement of the gun as a whole and keeps what happens
					// inside it (the slide, a reload). The root then sits at the
					// hand with the object's rotation, which is the controller's.
					// VRGunRigid 0 puts the animated placement back.
					bool bRigid = false;
					LTMatrix mRelRot; mRelRot.Identity();
					if (!bViewVeh && g_vtVRGunRigid.GetFloat() > 0.0f && bGunRootFound && g_pMathLT)
					{
						LTMatrix mRoot, mNode, mObj;
						g_pMathLT->SetupRotationMatrix(mRoot, rGunRoot);
						g_pMathLT->SetupRotationMatrix(mNode, tf.m_Rot);
						g_pMathLT->SetupRotationMatrix(mObj, rObjNow);
						// THE LEFTORIUM: the gun in the LEFT hand is a mirror image
						// of the authored right-handed one - the object's right axis
						// reversed. Everything below (node positions, their
						// orientations, the scope and silencer placed through
						// s_ModPlace) inherits it. Models draw without culling, so
						// the reversed winding does not hide any face.
						// Desk-proven with a fixed head: the left-hand gun seen from
						// the left eye, flipped, is the right-hand gun seen from the
						// right eye (the y and z reflections were not).
						if (i == nViewWeapon && VRShared::SwapHands())
							for (int rr = 0; rr < 3; ++rr) mObj.m[rr][0] = -mObj.m[rr][0];
						// d = node - root, into the root's frame: R_root^T * d
						const LTVector d(t.x - vGunRootPos.x, t.y - vGunRootPos.y, t.z - vGunRootPos.z);
						LTVector vLocalRel;
						vLocalRel.x = mRoot.m[0][0]*d.x + mRoot.m[1][0]*d.y + mRoot.m[2][0]*d.z;
						vLocalRel.y = mRoot.m[0][1]*d.x + mRoot.m[1][1]*d.y + mRoot.m[2][1]*d.z;
						vLocalRel.z = mRoot.m[0][2]*d.x + mRoot.m[1][2]*d.y + mRoot.m[2][2]*d.z;
						// back out with the object's rotation TIMES THE ROOT'S REST
						// orientation (captured above): R_obj * R_rest * rel
						LTMatrix mPlace = mObj;
						if (s_bRootRefValid && s_hRootRefObj == objs[i]
							&& s_nRootRefWeapon == (int)m_weaponModel.GetWeaponId())
							mPlace = mObj * s_mRootRef;
						vRel.x = mPlace.m[0][0]*vLocalRel.x + mPlace.m[0][1]*vLocalRel.y + mPlace.m[0][2]*vLocalRel.z;
						vRel.y = mPlace.m[1][0]*vLocalRel.x + mPlace.m[1][1]*vLocalRel.y + mPlace.m[1][2]*vLocalRel.z;
						vRel.z = mPlace.m[2][0]*vLocalRel.x + mPlace.m[2][1]*vLocalRel.y + mPlace.m[2][2]*vLocalRel.z;
						// R_rel = R_root^T * R_node ; placed rotation = R_obj * R_rel
						LTMatrix mRootT; mRootT.Identity();
						for (int rr = 0; rr < 3; ++rr) for (int cc = 0; cc < 3; ++cc) mRootT.m[rr][cc] = mRoot.m[cc][rr];
						mRelRot = mPlace * (mRootT * mNode);
						bRigid = true;
						if (i == nViewWeapon)
						{
							s_ModPlace.bValid = true; s_ModPlace.mPlace = mPlace; s_ModPlace.mRootT = mRootT;
							s_ModPlace.vGunRootPos = vGunRootPos; s_ModPlace.vOrig = vOrig; s_ModPlace.vGripW = vGripW;
							s_ModPlace.K = K; s_ModPlace.S = S;
						}
					}
					const LTVector vLocal = vOrig * K + vRel * (K * S);
					tf.m_Pos = vEye
						+ (vCamR * vLocal.x + vCamU * vLocal.y
						   + vCamF * vLocal.z);
					tf.m_Pos += vGripW;		// GRIP, see the origin above
					// THE WORLD POSITION ACTUALLY PUBLISHED, against the eye it
					// is supposed to be near. It resolves its textures and is
					// counted among the drawn instances and cannot be seen, so
					// the remaining question is simply where it went.
					{
						static float s_fSaidW = -99999.0f;
						if (fabsf(tf.m_Pos.x - s_fSaidW) > 0.5f)
						{
							s_fSaidW = tf.m_Pos.x;
							if (g_vtVRLogWeaponPos.GetFloat() > 0.0f)
								VRLog::Msg("  view weapon node0 WORLD %+.1f %+.1f %+.1f"
									"  eye %+.1f %+.1f %+.1f  (delta %+.1f %+.1f %+.1f)",
									tf.m_Pos.x, tf.m_Pos.y, tf.m_Pos.z,
									vEye.x, vEye.y, vEye.z,
									tf.m_Pos.x - vEye.x, tf.m_Pos.y - vEye.y,
									tf.m_Pos.z - vEye.z);
						}
					}
					LTMatrix mc;
					if (g_pMathLT) g_pMathLT->SetupRotationMatrix(mc, rCam);
					else mc.Identity();
					mat = bRigid ? (mc * mRelRot) : (mc * mat);
					// THE INTEGRATED SCOPE'S LENS - see VRIntScope. Either from the
					// named tube node, or from the gun's first node along the aim.
					if (bRigid && VRShared::IsLive() && nViewMod[1] == 0xFFFFFFFFu)
					{
						const VRIntScope* pIS = VRIntScopeFor((int)m_weaponModel.GetWeaponId());
						bool bHere = false;
						if (pIS)
						{
							if (pIS->szNode[0])
							{
								char szN[64] = ""; g_pLTClient->GetModelNodeName(objs[i], hNode, szN, sizeof(szN));
								bHere = (stricmp(szN, pIS->szNode) == 0);
							}
							else bHere = (s_nGunNodes == 0);		// the first node of the walk
						}
						CPlayerStats* pSt = bHere ? m_InterfaceMgr.GetPlayerStats() : LTNULL;
						MOD* pScopeMod = (pSt && g_pWeaponMgr) ? g_pWeaponMgr->GetMod((ModType)pSt->GetScope()) : LTNULL;
						if (bHere && pScopeMod && pSt->HaveMod(pScopeMod->nId))
						{
							LTVector sc(1.0f, 1.0f, 1.0f); g_pLTClient->GetObjectScale(objs[i], &sc);
							const float fS = s_ModPlace.K * s_ModPlace.S * sc.x;
							const float fRMesh = pIS->fR * fS;
							s_fVRLensMeshRadius = fRMesh;
							const float fRTune = m_weaponModel.VRLensRadius();
							const float fR = (fRTune > 0.0f) ? fRTune : fRMesh;
							// The tuner's offset, in the gun's frame through the camera basis,
							// the way the flash's is.
							const LTVector vTune = VRPrims_GunFrameToWorldLast(m_weaponModel.VRGunRot(), m_weaponModel.VRLensOffset());
							LTVector vEyepiece, vObjective, vF, vU;
							if (pIS->szNode[0])
							{
								const LTVector vCl(pIS->c[0] * fS, pIS->c[1] * fS, pIS->c[2] * fS);
								const LTVector vC = tf.m_Pos + VRMatMul(mat, vCl);
								LTVector vA = VRMatMul(mat, LTVector(pIS->a[0], pIS->a[1], pIS->a[2])); vA.Norm();
								const LTVector vE0 = vC + vA * (pIS->fT0 * fS);
								const LTVector vE1 = vC + vA * (pIS->fT1 * fS);
								// The eyepiece is the end nearer the eye, whichever way
								// the node's axis happens to point.
								const bool b0Near = (vE0 - vEye).MagSqr() < (vE1 - vEye).MagSqr();
								vEyepiece  = b0Near ? vE0 : vE1;
								vObjective = b0Near ? vE1 : vE0;
								vF = vObjective - vEyepiece; vF.Norm();
								vU = VRMatMul(mat, LTVector(0.0f, 1.0f, 0.0f));
							}
							else
							{
								// From the root, along the drawn barrel (the aim rotation
								// through the camera basis, as the flash's shape is).
								LTRotation rBarrel = rCam * m_weaponModel.VRAimRot();
								LTVector vBR;
								g_pLTClient->GetRotationVectors(&rBarrel, &vU, &vBR, &vF);
								// FROM THE OBJECT'S ORIGIN, NOT A NODE. Anchored to the first
								// node the lens breathed with the idle animation while the
								// tube did not. The origin is where
								// the mods are placed from and it does not animate.
								const LTVector vL0 = s_ModPlace.vOrig * s_ModPlace.K;
								const LTVector vAnchor = vEye + (vCamR * vL0.x + vCamU * vL0.y + vCamF * vL0.z) + s_ModPlace.vGripW;
								vEyepiece  = vAnchor;
								vObjective = vAnchor + vF * ((pIS->fT1 - pIS->fT0) * fS);
							}
							// the disc a little inside the rear rim, the pass a little
							// ahead of the front one, in open air; then the tuner's move
							vEyepiece  += vF * (fR * 0.5f) + vTune;
							vObjective += vF * fR + vTune;
							LTRotation rAxis; rAxis.Init();
							g_pLTClient->AlignRotation(&rAxis, &vF, &vU);
							LTVector vAU, vAR, vAF;
							g_pLTClient->GetRotationVectors(&rAxis, &vAU, &vAR, &vAF);
							const int nZ = VRScopeZoomLevel();
							const float fFov = (nZ <= 0) ? 20.0f : (nZ == 1 ? 7.0f : 2.0f);
							VRPrims_SetScopeLens(vObjective, rAxis, vEyepiece, vAR, vAU, fR * 0.9f, nZ, fFov);
							static int s_nSaidInt = -1;
							if (s_nSaidInt != (int)m_weaponModel.GetWeaponId())
							{
								s_nSaidInt = (int)m_weaponModel.GetWeaponId();
								const LTVector vT = m_weaponModel.VRLensOffset();
								VRLog::Msg("VRScope: '%s' carries its scope in the gun mesh - lens from %s:"
										   " tube %.1f long, radius %.1f world units (mesh %.1f), LENS offset R %+.1f U %+.1f F %+.1f",
										   pIS->szWeapon, pIS->szNode[0] ? pIS->szNode : "the gun origin along the aim",
										   (pIS->fT1 - pIS->fT0) * fS, fR, fRMesh, vT.x, vT.y, vT.z);
							}
						}
					}
				}
				else if (bViewMod && s_ModPlace.bValid)
				{
					// A MOD'S NODE: the gun's rigid mapping, from the placement
					// stashed when the gun's own nodes went through. See s_ModPlace.
					LTMatrix mNode;
					if (g_pMathLT) g_pMathLT->SetupRotationMatrix(mNode, tf.m_Rot); else mNode.Identity();
					const LTVector d(tf.m_Pos.x - s_ModPlace.vGunRootPos.x, tf.m_Pos.y - s_ModPlace.vGunRootPos.y, tf.m_Pos.z - s_ModPlace.vGunRootPos.z);
					const LTVector r = VRMatMul(s_ModPlace.mPlace, VRMatMul(s_ModPlace.mRootT, d));
					const LTVector vLocal = s_ModPlace.vOrig * s_ModPlace.K + r * (s_ModPlace.K * s_ModPlace.S);
					tf.m_Pos = vEye + (vCamR * vLocal.x + vCamU * vLocal.y + vCamF * vLocal.z) + s_ModPlace.vGripW;
					LTMatrix mc;
					if (g_pMathLT) g_pMathLT->SetupRotationMatrix(mc, rCam); else mc.Identity();
					mat = mc * (s_ModPlace.mPlace * (s_ModPlace.mRootT * mNode));
				}
				for (int r = 0; r < 3; ++r)
				{
					vn.m[r * 4 + 0] = mat.m[r][0];
					vn.m[r * 4 + 1] = mat.m[r][1];
					vn.m[r * 4 + 2] = mat.m[r][2];
				}
				vn.m[3] = tf.m_Pos.x; vn.m[7] = tf.m_Pos.y; vn.m[11] = tf.m_Pos.z;
				// THE DRAWN GUN'S DIRECTION, FROM ITS OWN NODES. In the headset the gun
				// still did not pitch up and down correctly, after two
				// opposite fixes judged by eye. The muzzle is the node farthest
				// from the mesh centre along the barrel; the vector from the
				// centre to it is the direction the picture shows, in world
				// units, whatever the rotation arithmetic thinks it did.
				if (i == nViewWeapon)
				{
					s_vGunSum.x += tf.m_Pos.x; s_vGunSum.y += tf.m_Pos.y; s_vGunSum.z += tf.m_Pos.z;
					++s_nGunNodes;
					// The running centroid; the last node of the walk leaves the
					// real one behind. Shell casings leave the weapon's BODY,
					// not its barrel, and this is the only point on the gun
					// that scales with whichever model is in the player's hand.
					VRPrims_NoteDrawnGunCentre(s_vGunSum / (float)s_nGunNodes);
					// ALONG THE BARREL, NOT AWAY FROM THE EYE.
					//
					// Distance from the eye picks the muzzle only while the gun
					// points away from the player. Measured at the desk on
					// 19 September: on one weapon the furthest node came out
					// BEHIND the camera - dot -0.17 against the fire gate's 0.60
					// minimum - so the engine skipped the flash, the casing and
					// the muzzle light entirely on every shot. The barrel tip is
					// the node furthest along the gun's OWN forward axis, and
					// that axis is sound: the weapon object carries the hand's
					// rotation (WeaponModel sets it from fAbsYaw/fAbsPitch).
					LTVector vGU, vGR, vGF;
					g_pLTClient->GetRotationVectors(&rObjNow, &vGU, &vGR, &vGF);
					const LTVector vFromEye = tf.m_Pos - vEye;
					const float fD = vFromEye.x * vGF.x + vFromEye.y * vGF.y + vFromEye.z * vGF.z;

					// THE ARM IS NOT THE GUN, and this is what put the tracer
					// behind it.
					//
					// A player-view weapon model carries the ARM and HAND
					// skeleton as well as the weapon, so "the node furthest
					// along the barrel" can land on a fingertip or the wrist.
					// 
					// the tracers came out below and behind the gun - and a
					// dump of the node layout came back full of pointerR1..R4
					// and RwristVolY, every one of them BEHIND the first node.
					// It also explains why the chosen node moved between
					// weapons (17.9 units on the player's, 23.6 on the Sterling here,
					// 60.3 on another run): it was picking whichever bit of
					// hand happened to reach furthest.
					//
					// The same exclusion list the gun-shape dump already uses,
					// a few hundred lines below. Kept identical on purpose: two
					// different ideas of what counts as the gun is how this
					// sort of thing comes back.
					char szN[64] = "";
					g_pLTClient->GetModelNodeName(objs[i], hNode, szN, sizeof(szN));
					{
						char szLo[64]; strncpy(szLo, szN, 63); szLo[63] = 0; _strlwr(szLo);
						if (!szLo[0]
							|| strstr(szLo, "arm")     || strstr(szLo, "wrist")
							|| strstr(szLo, "thumb")   || strstr(szLo, "pinky")
							|| strstr(szLo, "ring")    || strstr(szLo, "middle")
							|| strstr(szLo, "pointer") || strstr(szLo, "finger")
							|| strstr(szLo, "vol")     || strstr(szLo, "hand")
							|| strstr(szLo, "null")    || strstr(szLo, "plot"))
						{
							++mi.nNodeCount;
							continue;		// an arm part: never the muzzle
						}
					}

					// THE MUZZLE NODE IS CHOSEN ONCE PER WEAPON, NOT PER FRAME.
					//
					// Re-picking the furthest node every frame looks harmless and
					// is not: the gun's nodes animate, so two of them near the
					// end of the barrel trade places and the "muzzle" jumps
					// between them. Measured on the Sterling at the desk,
					// 19 September - consecutive frames put it at 913 3 -1779
					// and 887 3 -1781, twenty-six units apart. A flash that
					// moves forty centimetres every frame is exactly what the headset
					// report of the muzzle flash still not being attached means.
					//
					// So: learn the winning node's NAME on the first frame with
					// a given weapon, then follow that node by name. It also
					// stops the choice changing while the player fires, which is the one
					// moment it would be seen.
					const int nWepNow = (int)m_weaponModel.GetWeaponId();

					// THE WHOLE GUN'S NODE LAYOUT, ONCE PER WEAPON.
					//
					// 
					// the tracers came out below and behind the gun. The node
					// picked as the muzzle is 17.9 units from the gun's origin
					// on the tester's weapon where the Sterling measured 23.6 at the desk,
					// so the choice differs per weapon and picking wrong is
					// invisible from a single number.
					//
					// Each node's offset from the FIRST node seen, decomposed
					// into the gun's own right/up/forward. The barrel tip is
					// whichever has the largest FORWARD; if the one being chosen
					// has a NEGATIVE forward it is behind the grip, which is
					// exactly what the tester is describing.
					{
						static int s_nDumpWeapon = -1;
						static int s_nDumped = 0;
						static LTVector s_vFirstNode(0,0,0);
						if (nWepNow != s_nDumpWeapon) { s_nDumpWeapon = nWepNow; s_nDumped = 0; s_vFirstNode = tf.m_Pos; }
						if (s_nDumped < 40)
						{
							++s_nDumped;
							const LTVector d = tf.m_Pos - s_vFirstNode;
							VRLog::Msg("VRGunNode[%2d] '%s'  r %+.1f  u %+.1f  f %+.1f",
								s_nDumped, szN,
								d.x*vGR.x + d.y*vGR.y + d.z*vGR.z,
								d.x*vGU.x + d.y*vGU.y + d.z*vGU.z,
								d.x*vGF.x + d.y*vGF.y + d.z*vGF.z);
						}
					}
					if (nWepNow != s_nMuzzleNodeWeapon)
					{	// new weapon: forget the old node and learn again
						s_nMuzzleNodeWeapon = nWepNow;
						s_szMuzzleNode[0] = 0;
					}
					if (s_szMuzzleNode[0] && _stricmp(szN, s_szMuzzleNode) == 0)
					{
						// THE SECOND WRITER, AND THAT WAS THE WHOLE PROBLEM.
						//
						// Two places were publishing the muzzle: this one, from
						// the settled drawn node, and CWeaponModel, from the
						// authored per-weapon offset. They do not agree, there
						// is no ordering between them, and whichever ran last
						// owned the frame. The muzzle flash reads this value.
						// muzzle flashes were way off to
						// the right, about twenty feet away - and the rejected
						// muzzles the tracer guard logged that evening were
						// 286 and 371 units out, which is 16 to 21 feet. The reported
						// twenty feet and the log are the same number.
						//
						// The authored offset wins, because it is DATA rather
						// than a guess: ATTRIBUTES/WEAPONS.TXT gives every one
						// of the 45 weapons an exact MuzzlePos in the gun's own
						// frame - P38 <2.04,-2.25,12.72>, Sterling
						// <3.06,-5.64,19.30>, Walther SMG <2.04,-2.20,9.98> -
						// 18 distinct values, forward from 10.0 to 22.8 units.
						// Monolith tuned every gun by hand in 2000 and shipped
						// the table. Picking a node was always an attempt to
						// re-derive, badly, something the art already states.
						//
						// The node walk stays: s_vGunFar is what the GUN DIR
						// diagnostic reads, and the node spread it measures is
						// what proved a node can never be the muzzle - the
						// ak47's gun nodes all sit at one point.
						(void)0;
					}
					if (fD > s_fGunFar)
					{
						s_fGunFar = fD; s_vGunFar = tf.m_Pos;
						if (!s_szMuzzleNode[0])
						{
							strncpy(s_szMuzzleNode, szN, sizeof(s_szMuzzleNode) - 1);
							s_szMuzzleNode[sizeof(s_szMuzzleNode) - 1] = 0;
						}
						// AND HAND IT TO THE EFFECTS. This is the same node the
						// GUN DIR diagnostic already calls the muzzle, in the
						// same world units, after the rotation, the origin and
						// the K*S scale have all been applied to it. The flash,
						// the tracer and the casings all start from
						// m_vFlashPos, and every attempt to compute that from
						// an authored offset has had to agree with those three
						// separately - which is what put it 13 feet out, and
						// then a few inches out.
						// ONLY WHILE WE ARE STILL LEARNING. Once a node is
						// settled it owns the muzzle, and a later node winning
						// this frame's projection test must not overwrite it -
						// that would put the per-frame jitter straight back,
						// which is the bug this whole block exists to stop.
						// NO LONGER THE MUZZLE. CWeaponModel publishes that from
						// the authored offset now - see the note there. The
						// furthest node is kept because the GUN DIR diagnostic
						// below reads it, and because the spread it measures is
						// what proved this approach cannot work: the ak47's gun
						// nodes are ALL at one point.
						(void)0;
					}
					// The barrel's own ends, by name (WALTHER_PV: the gun root
					// extru4 and the slide's far end cyl6), so the direction
					// below is the barrel and not the arm.
					char szB[64] = "";
					g_pLTClient->GetModelNodeName(objs[i], hNode, szB, sizeof(szB));
					if (_stricmp(szB, "extru4") == 0) { s_vBarrelA = tf.m_Pos; s_nBarrelHave |= 1; }
					if (_stricmp(szB, "cyl6") == 0)   { s_vBarrelB = tf.m_Pos; s_nBarrelHave |= 2; }
				}
				++mi.nNodeCount;
			}
			if (bNodeCacheable && !bNodeReused && !bNodesFull && mi.nNodeCount)
			{
				if (s_VRNodeCache.size() > 4096) s_VRNodeCache.clear();
				VRNodeCacheEnt& c = s_VRNodeCache[objs[i]];
				memcpy(c.fPos, keyNow.fPos, sizeof(c.fPos));
				memcpy(c.fRot, keyNow.fRot, sizeof(c.fRot));
				memcpy(c.fScale, keyNow.fScale, sizeof(c.fScale));
				c.nAnim = keyNow.nAnim; c.nTime = keyNow.nTime; c.nPlaying = keyNow.nPlaying;
				c.nodes.assign(&s_frame.nodes[mi.nNodeFirst], &s_frame.nodes[mi.nNodeFirst] + mi.nNodeCount);
			}
		}
		// A skeleton that did not fit is worse than no skeleton: the mesh names
		// nodes that were never sent, those vertices cannot be skinned, and the
		// model draws with pieces missing. Give the nodes back and drop it.
		if (bNodesFull)
		{
			s_frame.nNodeCount = mi.nNodeFirst;
			++s_frame.nDropped;
			continue;
		}
		// PUSH THE BODY BACK FROM THE HEAD.
		//
		// Your eyes are at the FRONT of your skull and a character model's head
		// sits on the middle of its shoulders, so a body placed at the camera
		// puts its own chest and chin in your view. Every VR body has to be
		// shifted back by about half a head's depth - an adult head is roughly
		// 19 cm front to back, hence VRBodyBack's 0.09. It is in METRES, a
		// human dimension, not a world coordinate, so the only scaling it needs
		// is the units-per-metre below.
		//
		// Applied to the instance AND to every node, because the renderer skins
		// from the nodes and moving only the instance would leave the mesh
		// exactly where it was.
		if ((mi.nFlags & VRMODEL_F_PLAYER) && g_vtVRBodyBack.GetFloat() != 0.0f)
		{
			const float fBack = g_vtVRBodyBack.GetFloat() * 58.75f;
			const LTVector vOff = vCamF * -fBack;
			mi.fPos[0] += vOff.x; mi.fPos[1] += vOff.y; mi.fPos[2] += vOff.z;
			for (uint32 z = 0; z < mi.nNodeCount; ++z)
			{
				VRModelNode& vn = s_frame.nodes[mi.nNodeFirst + z];
				vn.m[3] += vOff.x; vn.m[7] += vOff.y; vn.m[11] += vOff.z;
			}
		}

		// BEND THE ARM TO THE CONTROLLER. Two-bone IK, run after the walk
		// because it needs all three joints posed before it can measure them.
		//
		// The bone LENGTHS come from the animated pose, not the bind pose. A
		// bone does not change length, so measuring what is actually there
		// needs no bind data and cannot drift out of step with it.
		// Not in the Leftorium: this is the body's RIGHT arm, and the weapon hand
		// is then the left controller - it would reach across the chest. The arm
		// keeps its animated pose instead (bending the left arm is a later job).
		if (bPlayerArm && nArmIdx[0] >= 0 && nArmIdx[1] >= 0 && nArmIdx[2] >= 0
			&& VRShared::IsLive() && VRShared::State().Hands[1].nActive
			&& !VRShared::SwapHands())
		{
			VRModelNode& nSh = s_frame.nodes[nArmIdx[0]];
			VRModelNode& nEl = s_frame.nodes[nArmIdx[1]];
			VRModelNode& nHa = s_frame.nodes[nArmIdx[2]];
			const LTVector vSh(nSh.m[3], nSh.m[7], nSh.m[11]);
			const LTVector vEl(nEl.m[3], nEl.m[7], nEl.m[11]);
			const LTVector vHa(nHa.m[3], nHa.m[7], nHa.m[11]);
			const float L1 = (vEl - vSh).Mag();
			const float L2 = (vHa - vEl).Mag();

			// The controller, in the world. 58.75 units per metre is the
			// WORLD's scale, and the body IS in the world - unlike the view
			// weapon, whose camera-relative space needed about 3.
			const VRSharedState& st = VRShared::State();
			const VRHandState&   hr = st.Hands[1];
			const LTVector vHL((hr.fPosX - st.fHeadPosX) * 58.75f,
							   (hr.fPosY - st.fHeadPosY) * 58.75f,
							  -(hr.fPosZ - st.fHeadPosZ) * 58.75f);
			LTVector vT = vEye + vCamR * vHL.x + vCamU * vHL.y + vCamF * vHL.z;

			LTVector vSt = vT - vSh;
			const float fWant = vSt.Mag();
			float d = fWant;
			const float dMin = (float)fabs(L1 - L2) + 0.01f;
			const float dMax = L1 + L2 - 0.01f;
			if (d < dMin) d = dMin;
			if (d > dMax) d = dMax;
			if (d > 0.001f && L1 > 0.001f && L2 > 0.001f && fWant > 0.001f)
			{
				LTVector vDir = vSt; vDir.Norm();
				// CLAMPED, so the arm never stretches past its own length. An
				// IK that is allowed to reach further than the bones go pulls
				// the mesh apart, and it does it worst exactly when the player
				// extends their arm - which is when they are looking at it.
				vT = vSh + vDir * d;
				const float a = (L1*L1 - L2*L2 + d*d) / (2.0f * d);
				const float hh = L1*L1 - a*a;
				const float hgt = (hh > 0.0f) ? (float)sqrt(hh) : 0.0f;

				// THE POLE. An elbow otherwise picks an arbitrary point on a
				// circle and flips between frames. Hers goes down and a little
				// out, which is where a human elbow is with the hand in front.
				LTVector vPole = vCamU * -1.0f + vCamR * 0.35f;
				LTVector vPerp = vPole - vDir * vPole.Dot(vDir);
				if (vPerp.Mag() < 0.001f) vPerp = vCamR;
				vPerp.Norm();
				const LTVector vNewE = vSh + vDir * a + vPerp * hgt;

				VRArmBone(nEl, vSh, vEl, vSh, vNewE);
				VRArmBone(nHa, vEl, vHa, vNewE, vT);
				nEl.m[3] = vNewE.x; nEl.m[7] = vNewE.y; nEl.m[11] = vNewE.z;
				nHa.m[3] = vT.x;    nHa.m[7] = vT.y;    nHa.m[11] = vT.z;

				static int s_nArmSaid = 0;
				if (s_nArmSaid < 4)
				{
					++s_nArmSaid;
					VRLog::Msg("VRArm: upper %.1f fore %.1f (reach %.1f),"
						" controller %.1f away%s", L1, L2, L1 + L2, fWant,
						(fWant > dMax) ? "  <- beyond reach, clamped" : "");
				}
			}
		}

		++s_frame.nCount;
	}

	s_pfn(&s_frame);

	// ---- WORLD MODELS, whose transforms the renderer has never had ------
	//
	// Doors, lifts, rotating props. The renderer builds them once from the
	// level's authored vertices and nothing ever moves them, so every door in
	// the game is drawn shut while the engine has opened it. The engine keeps
	// the vertices still and moves the OBJECT, and only the client can see that.
	//
	// Sent every frame, unconditionally: which of them have moved is the
	// renderer's business, and a door that has not moved costs one comparison.
	{
		VRPubMark(1);
		static VRWorldPublishFn s_pfnW = NULL;
		static HMODULE s_hRenSeenW = (HMODULE)(uintptr_t)1;
		{
			HMODULE h = GetModuleHandleA("d3dstub.ren");
			if (h != s_hRenSeenW)		// see the note at s_hRenSeen above
			{
				s_hRenSeenW = h;
				s_pfnW = h ? (VRWorldPublishFn)GetProcAddress(h, "R3D_PublishWorldModels") : NULL;
				VRLog::Msg("VRWorld: publish entry %s", s_pfnW ? "resolved" : "missing");
			}
		}
		if (s_pfnW)
		{
			static VRWorldFrame s_world;
			s_world.nMagic   = VRWORLD_MAGIC;
			s_world.nVersion = VRWORLD_VERSION;
			s_world.nFrame   = m_nVRModelFrame;
			s_world.nCount   = 0;

			// Everything, not a sphere around the camera: a lift three rooms away
			// is still drawn, and drawing it in the wrong place is the bug.
			// SCAN WIDE, PUBLISH THE WORLD MODELS. The sphere returns every
			// object type and this array held VRWORLD_MAX (1024) of them, so
			// a level with more objects than that - Morocco: 1086 - lost its
			// tail, and a DOOR in the tail was never tracked: it drew where
			// the file put it, forever. The client log said so on every frame
			// ("<- TRUNCATED: objects the engine knows about are not being
			// tracked at all") and nobody read it. 4096 is the census's bound.
			static HLOCALOBJ wobjs[4096];
			uint32 nW = 0, nWFound = 0;
			g_pLTClient->FindObjectsInSphere(&vEye, 1000000.0f, wobjs, 4096,
											 &nW, &nWFound);
			for (uint32 z = 0; z < nW && s_world.nCount < VRWORLD_MAX; ++z)
			{
				if (g_pLTClient->GetObjectType(wobjs[z]) != OT_WORLDMODEL) continue;
				VRWorldInst& wi = s_world.inst[s_world.nCount++];
				wi.nObject = (uint32_t)(uintptr_t)wobjs[z];
				LTVector q; g_pLTClient->GetObjectPos(wobjs[z], &q);
				wi.fPos[0] = q.x; wi.fPos[1] = q.y; wi.fPos[2] = q.z;
				LTRotation r;
				g_pLTClient->GetObjectRotation(wobjs[z], &r);
				LTMatrix m;
				if (g_pMathLT) g_pMathLT->SetupRotationMatrix(m, r);
				else m.Identity();
				for (int a = 0; a < 3; ++a)
					for (int b = 0; b < 3; ++b)
						wi.fRot[a * 3 + b] = m.m[a][b];
				// What the level author set. 1.0 means opaque, and most
				// TranslucentWorldModels are exactly that.
				float cr = 1.0f, cg = 1.0f, cb = 1.0f, ca = 1.0f;
				g_pLTClient->GetObjectColor(wobjs[z], &cr, &cg, &cb, &ca);
				wi.fAlpha = ca;
				// AND WHETHER THE ENGINE IS DRAWING IT AT ALL. The model
				// publish has read this flag word for a while; the world
				// publish never has, so a brush the engine hid stayed on
				// screen. Counted by the renderer before anything acts on it.
				wi.nFlags = 0;
				const uint32 dwWF = g_pLTClient->GetObjectFlags(wobjs[z]);
				if (!(dwWF & FLAG_VISIBLE)) wi.nFlags |= VRWORLD_F_INVISIBLE;
				if (dwWF & FLAG_FOGDISABLE) wi.nFlags |= VRWORLD_F_FOGDISABLE;
			}
			// WHAT THE SPHERE COULD NOT FIT, which nothing has ever looked at.
			//
			// nWFound is the number FindObjectsInSphere found; nW is how many
			// it could write into the array. They have been read into two
			// variables since this was written and only one of them used - the
			// same shape as the model publish, which was quietly dropping 52 of
			// 244 instances at its own cap.
			//
			// It matters here in a way the model cap does not: a world model we
			// never publish is not MISSING from the picture, it is drawn from
			// the level FILE at the position the author saved. So it can never
			// open, never move and never be destroyed - a door whose leaf made
			// the list and whose handle did not is a door that opens and leaves
			// its handle hanging in the air.
			{
				static uint32 s_nWorstFound = 0;
				if (nWFound > s_nWorstFound)
				{
					s_nWorstFound = nWFound;
					VRLog::Msg("VRWorld: FindObjectsInSphere found %u objects,"
							   " returned %u (array is %d), of which %u are"
							   " WORLDMODELs and were published%s",
							   nWFound, nW, 4096, s_world.nCount,
							   (nWFound > nW)
								   ? "   <- TRUNCATED: objects the engine knows"
									 " about are not being tracked at all"
								   : "");
				}
			}
			s_pfnW(&s_world);
		}
	}

	// ---- SPRITES, which are every effect the game has ------------------
	//
	// And the main menu's entire background: m_BackSprite is a CBaseScaleFX of
	// type OT_SPRITE, rendered through the interface camera, which is why the
	// menu is black rather than merely misplaced. In the world they are the lamp
	// glows, the muzzle flashes and the light coronas - 142 on Morocco.
	{
		static VRSpritePublishFn s_pfnS = NULL;
		static HMODULE s_hRenSeenS = (HMODULE)(uintptr_t)1;
		{
			HMODULE h = GetModuleHandleA("d3dstub.ren");
			if (h != s_hRenSeenS)
			{
				s_hRenSeenS = h;
				s_pfnS = h ? (VRSpritePublishFn)GetProcAddress(h, "R3D_PublishSprites") : NULL;
				VRLog::Msg("VRSprites: publish entry %s", s_pfnS ? "resolved" : "missing");
			}
		}
		if (s_pfnS)
		{
			static VRSpriteFrame s_spr;
			s_spr.nMagic   = VRSPRITE_MAGIC;
			s_spr.nVersion = VRSPRITE_VERSION;
			s_spr.nFrame   = m_nVRModelFrame;
			s_spr.nCount   = 0;

			static HLOCALOBJ sobjs[VRSPRITE_MAX];
			uint32 nS = 0, nSFound = 0;
			// Everything. A menu sprite sits at the interface camera, nowhere near
			// the player, and a lamp glow across the street is still a lamp glow.
			// A BOUNDED RADIUS IN THE WORLD, everything only at the menu.
			//
			// This asked for a million units - the whole level - and the buffer
			// holds 512. Measured on the Morocco quick save: 909 sprites found,
			// 512 returned, and the other 397 never considered. Which 512 is
			// the engine's business, so a BULLET HOLE created a second ago
			// competes for a place with every corona in the level and usually
			// loses. That is why shooting the ground left no mark.
			//
			// The models already do this - g_vtVRModelRange in the world,
			// everything when a folder is up, because at the menu the sphere is
			// the wrong instrument and nothing is in the partition it searches.
			// The sprites never got the same treatment.
			VRPubMark(2);
			const float fSprRange = bFolder ? 1000000.0f
											: g_vtVRSpriteRange.GetFloat();
			g_pLTClient->FindObjectsInSphere(&vEye, fSprRange, sobjs,
											 VRSPRITE_MAX, &nS, &nSFound);
			// The same correction as the models: at the menu the sphere finds
			// one sprite and the interface holds six.
			// The same test as the models above: the interface scene belongs to
			// the interface camera, and at the pause menu we are not rendering
			// with it.
			const bool bCardS = bFolder && VRCardScene();
			if (bFolder && (!hPlayer || bCardS) && g_nVRIfaceObjs > 0)
			{
				nS = 0;
				for (int z = 0; z < g_nVRIfaceObjs && nS < VRSPRITE_MAX; ++z)
					sobjs[nS++] = g_hVRIfaceObjs[z];
				nSFound = nS;
			}
			// THE MUZZLE FLASH, HANDED OVER RATHER THAN SEARCHED FOR. The
			// sweep below looks for camera-relative sprites near the origin
			// and is the general answer; this is the specific one, and it
			// cannot miss. The view weapon is added the same way.
			if (hPlayer && nS < VRSPRITE_MAX)
			{
				// WHY THERE IS NO FLASH, said once a second while firing.
				//
				// Three different things can produce "no muzzle flash" and
				// they need three different fixes: the view weapon not being
				// FLAG_VISIBLE (CWeaponModel::UpdateFlash hides the flash
				// outright and returns), the weapon having no pPVMuzzleFX in
				// its bute, or the flash existing and being published in the
				// wrong place. Guessing between them cost most of a night.
				{
					// Capped: a line a second for a whole session is noise, and
					// the first forty already answer the question.
					static uint32 s_nFlashSaid = 0;
					static int    s_nFlashLines = 0;
					if (s_frame.nFrame >= s_nFlashSaid + 90 && s_nFlashLines < 40)
					{
						s_nFlashSaid = s_frame.nFrame;
						++s_nFlashLines;
						const HLOCALOBJ hW = m_weaponModel.GetHandle();
						const uint32 dwWF = hW ? g_pLTClient->GetObjectFlags(hW) : 0;
						const HLOCALOBJ hF = m_weaponModel.VRGetFlashHandle();
						VRLog::Msg("VRFlash: weapon obj %s visible %s (wants %s,"
								   " disabled %s) reallyclose %s | flash handle %s |"
								   " scale %s particles %s light %s (object %s) hidden %s bute %s"
								   " silencer %s | state %d | camera %s | ext cam %s | PV flash shown %lu times",
								   hW ? "yes" : "NO",
								   (dwWF & FLAG_VISIBLE) ? "yes" : "NO",
								   m_weaponModel.VRWantsVisible() ? "yes" : "NO",
								   m_weaponModel.IsDisabled() ? "yes" : "no",
								   (dwWF & FLAG_REALLYCLOSE) ? "yes" : "no",
								   hF ? "yes" : "NO",
								   m_weaponModel.VRFlashUsingScale() ? "yes" : "no",
								   m_weaponModel.VRFlashParticles()  ? "yes" : "no",
								   m_weaponModel.VRFlashLight()      ? "yes" : "no",
								   m_weaponModel.VRFlashLightObject() ? "yes" : "NULL",
								   m_weaponModel.VRFlashHidden()     ? "yes" : "no",
								   m_weaponModel.VRHasPVMuzzleFX()   ? "yes" : "NO",
								   m_weaponModel.VRHaveSilencer()    ? "FITTED" : "no",
								   (int)m_weaponModel.GetState(),
								   m_PlayerCamera.IsFirstPerson() ? "first person"
									 : (m_PlayerCamera.IsChaseView() ? "CHASE" : "other"),
								   m_bUsingExternalCamera ? "YES" : "no",
								   VRMuzzleFlashShownCount());
					}
				}
				HLOCALOBJ hFlash = m_weaponModel.VRGetFlashHandle();
				if (hFlash)
				{
					bool bDupF = false;
					for (uint32 q = 0; q < nS; ++q)
						if (sobjs[q] == hFlash) { bDupF = true; break; }
					if (!bDupF) sobjs[nS++] = hFlash;
				}
			}

			// AND THE CAMERA-RELATIVE ONES, WHICH THAT SPHERE CAN NEVER FIND.
			//
			// A FLAG_REALLYCLOSE sprite does not hold a world position at all:
			// LithTech keeps it in CAMERA space, so the muzzle flash sits at
			// something like (12, -18, 12) - a few units from the WORLD ORIGIN,
			// wherever the player happens to be standing. The sphere above is
			// centred on the player's eye, so unless the level's origin is in
			// the room with the player it misses the flash entirely, every frame, in
			// every level.
			//
			// That is why the first-person muzzle flash was never re-based onto
			// the VR gun: it was never COLLECTED. The re-basing code below is
			// correct and had simply never been given the object. Headset testing, 11
			// September: the muzzle flash was not attached to the gun.
			//
			// So the origin gets its own small sweep, and only sprites that
			// really are camera-relative are taken from it. 512 units is far
			// more than any view-weapon effect needs and still a tiny query.
			if (hPlayer && nS < VRSPRITE_MAX)
			{
				static HLOCALOBJ cobjs[64];
				uint32 nC = 0, nCFound = 0;
				LTVector vOrigin(0.0f, 0.0f, 0.0f);
				g_pLTClient->FindObjectsInSphere(&vOrigin, 512.0f, cobjs,
												 64, &nC, &nCFound);
				uint32 nAdded = 0;
				for (uint32 z = 0; z < nC && nS < VRSPRITE_MAX; ++z)
				{
					if (g_pLTClient->GetObjectType(cobjs[z]) != OT_SPRITE) continue;
					if (!(g_pLTClient->GetObjectFlags(cobjs[z]) & FLAG_REALLYCLOSE)) continue;
					bool bDup = false;
					for (uint32 q = 0; q < nS; ++q)
						if (sobjs[q] == cobjs[z]) { bDup = true; break; }
					if (bDup) continue;
					sobjs[nS++] = cobjs[z];
					++nAdded;
				}
				// AND A CENSUS WHEN THERE ARE NONE, because "no flash" and
				// "a flash in the wrong place" are different bugs and the
				// silence between them is what cost a night. Says what IS
				// near the origin and what flags it carries.
				if (!nAdded && nC)
				{
					static double s_fSaidNone = -1e9;
					const double fN2 = VRLog::NowMs() / 1000.0;
					if (fN2 - s_fSaidNone > 5.0)
					{
						s_fSaidNone = fN2;
						uint32 nSpr = 0, nMdl = 0, nClose = 0;
						for (uint32 z = 0; z < nC; ++z)
						{
							const uint32 t = g_pLTClient->GetObjectType(cobjs[z]);
							if (t == OT_SPRITE) ++nSpr;
							else if (t == OT_MODEL) ++nMdl;
							if (g_pLTClient->GetObjectFlags(cobjs[z]) & FLAG_REALLYCLOSE) ++nClose;
						}
						// NOTE WHAT THIS CENSUS CANNOT SEE. It is built from
						// the engine's object query, and that query does not
						// return FLAG_REALLYCLOSE objects AT ALL - measured 11
						// September, zero of them over 900 frames while the
						// muzzle flash fired 14 times. So a zero in the last
						// column is not evidence about the flash; it is the
						// query's own blind spot. The wording used to claim
						// the opposite and sent a night's work the wrong way.
						VRLog::Msg("VRClose: %u objects near the origin (%u sprites,"
								   " %u models), %u carry FLAG_REALLYCLOSE."
								   " The last number is expected to be 0: the engine's"
								   " object query does not return camera-relative"
								   " objects, so nothing here can say whether a muzzle"
								   " flash exists - see VRFlash and VRExtra for that",
								   nC, nSpr, nMdl, nClose);
					}
				}
				if (nAdded)
				{
					static double s_fSaidClose = -1e9;
					const double fN = VRLog::NowMs() / 1000.0;
					if (fN - s_fSaidClose > 2.0)
					{
						s_fSaidClose = fN;
						VRLog::Msg("VRClose: %u camera-relative sprite(s) picked up at"
								   " the origin - the eye sphere cannot reach them",
								   nAdded);
					}
				}
			}

			// The same exclusion as the models: with a world up the sphere
			// finds the interface sprites by itself, and the menu's olive card
			// then hangs in the room.
			if (hPlayer && !bCardS && g_nVRIfaceObjs > 0 && nS)
			{
				uint32 nKeepS = 0;
				for (uint32 z = 0; z < nS; ++z)
				{
					bool bIfaceS = false;
					for (int q = 0; q < g_nVRIfaceObjs; ++q)
						if (sobjs[z] == g_hVRIfaceObjs[q]) { bIfaceS = true; break; }
					if (!bIfaceS) sobjs[nKeepS++] = sobjs[z];
				}
				nS = nKeepS;
			}
			for (uint32 z = 0; z < nS && s_spr.nCount < VRSPRITE_MAX; ++z)
			{
				if (g_pLTClient->GetObjectType(sobjs[z]) != OT_SPRITE) continue;
				// A SPRITE THE GAME HAS HIDDEN IS NOT DRAWN. The sphere returns
				// hidden objects too, and the engine never draws them. The
				// rain's splash sprites wait hidden at the centre of their
				// weather volume, ~300 units up, and are shown for 0.05 s where
				// a drop lands: drawn regardless, the M05S01 docks had splashes
				// frozen in the sky. In a level only -
				// the interface scene is left exactly as it was.
				// VRSpriteHiddenToo 1 draws them again.
				if (!bFolder && GetConsoleInt("VRSpriteHiddenToo", 0) == 0
					&& !(g_pLTClient->GetObjectFlags(sobjs[z]) & FLAG_VISIBLE)) continue;
				VRSpriteInst& si = s_spr.inst[s_spr.nCount++];
				si.nObject = (uint32_t)(uintptr_t)sobjs[z];
				// The flags, read the same way the models read theirs.
				si.nFlags = 0;
				{
					uint32 dwSF2 = 0;
					g_pLTClient->Common()->GetObjectFlags(sobjs[z], OFT_Flags2, dwSF2);
					if (dwSF2 & FLAG2_ADDITIVE) si.nFlags |= VRSPRITE_F_ADDITIVE;
					if (dwSF2 & FLAG2_MULTIPLY) si.nFlags |= VRSPRITE_F_MULTIPLY;
					// A FLARE IS DRAWN OVER THE WALL IT SITS ON. FLAG_SPRITE_NOZ
					// (the first flag word) is what the engine sets on lens
					// flares and light glows so the depth test never cuts them;
					// drawn with depth, a glow beside a ceiling lamp is sliced
					// by the ceiling into a hard-edged blotch (the aeroplane's
					// projector lights, 13 September, reported as weird cutoffs).
					const uint32 dwSF = g_pLTClient->GetObjectFlags(sobjs[z]);
					if (dwSF & FLAG_SPRITE_NOZ) si.nFlags |= VRSPRITE_F_NODEPTH;
				}
				// The aim dot is drawn on top of whatever it is placed on. See
				// UpdateVRAimMarker: on a character it sits on the body's axis,
				// inside the mesh, and it should beat the player's own gun too.
				if (m_hVRAimMarker && sobjs[z] == m_hVRAimMarker)
					si.nFlags |= VRSPRITE_F_NODEPTH;
				LTVector q; g_pLTClient->GetObjectPos(sobjs[z], &q);
				// THE FIRST-PERSON MUZZLE FLASH is camera-relative (FLAG_REALLYCLOSE):
				// published as world it sat by the level's origin. Onto the VR gun,
				// and its size in the gun's units. See VRViewRebase.
				float fCloseK = 1.0f;
				if (g_pLTClient->GetObjectFlags(sobjs[z]) & FLAG_REALLYCLOSE)
				{
					// SAY WHEN THE MUZZLE FLASH IS THROWN AWAY. Dropping it
					// silently is why a muzzle flash not attached to the gun
					// could not be told apart from no flash at all:
					// the re-base needs the VIEW WEAPON to have been published
					// this frame, and when it has not been the sprite simply
					// vanishes. Counted, and said once a second at most.
					static int s_nCloseDropped = 0, s_nCloseKept = 0;
					static double s_fCloseSaid = -1e9;
					// THE INTERFACE SCENE IS NOT THE GUN. Its sprites - the
					// main menu's backdrop and logo, the help boxes - are
					// created relative to the interface camera, which sits at
					// the origin with no rotation, so their camera-space
					// position IS their position in that scene. Re-basing them
					// onto a weapon, or dropping them for want of one, is why
					// the menus lost their backdrop after the muzzle-flash
					// work (the 12 September headset screenshots: a green void
					// where retail has blue, and no logo). Published as they are.
					// A CARD FOLDER OVER A LOADED LEVEL IS THE INTERFACE SCENE
					// TOO, whatever camera the interface manager reports: with
					// a player alive it says the game camera, and on 13
					// September the briefing's orange sheet (half-width 296 at
					// z 150, a camera-relative interface sprite) was read as a
					// screen-covering effect and left out - the reported black cards
					// with only the flower. The interface list is what is being
					// published here (see bCardS), so it is judged as such.
					const bool bIface = m_InterfaceMgr.UseInterfaceCamera()
						|| (bFolder && VRCardScene());
					const bool bHave = bIface || VRPrims_Rebase().bKnown;
					if (bHave) ++s_nCloseKept; else ++s_nCloseDropped;
					const double fNowC = VRLog::NowMs() / 1000.0;
					if (fNowC - s_fCloseSaid > 1.0 && (s_nCloseKept + s_nCloseDropped))
					{
						s_fCloseSaid = fNowC;
						VRLog::Msg("VRClose: camera-relative sprites this second -"
								   " %d re-based onto the gun, %d DROPPED for want"
								   " of a published view weapon", s_nCloseKept,
								   s_nCloseDropped);
						s_nCloseKept = 0; s_nCloseDropped = 0;
					}
					if (!bHave) { --s_spr.nCount; continue; }
					// A SCREEN-COVERING SPRITE IS NOT A MUZZLE FLASH. The poison
					// drug effect in the graveyard is a camera-relative sprite
					// sized to cover the whole view; re-based onto the gun it
					// became grey and green rectangles floating by the hand
					// (the 12 September screenshot batch). Retail tints the screen with
					// it; here it is left out, and said once a second at most.
					// The test is angular: half-width over distance, in camera
					// space, above 0.8 (a 77-degree span) is a screen effect.
					if (!bIface && q.z > 0.01f)
					{
						LTVector dm(0.0f, 0.0f, 0.0f);
						if (g_pPhysicsLT) g_pPhysicsLT->GetObjectDims(sobjs[z], &dm);
						if (dm.x / q.z > 0.8f)
						{
							static double s_fScreenSaid = -1e9;
							if (fNowC - s_fScreenSaid > 1.0)
							{
								s_fScreenSaid = fNowC;
								VRLog::Msg("VRClose: screen-covering camera-relative sprite"
										   " (dims %.0f x %.0f at z %.0f) left out - a screen"
										   " effect, not a gun effect", dm.x, dm.y, q.z);
							}
							--s_spr.nCount; continue;
						}
					}
					// THE MUZZLE FLASH IS ALREADY IN WORLD SPACE. DO NOT RE-BASE IT.
					//
					// This is the flash on the carpet by the fountain, in three
					// or four copies - the tester watched it happen and said so
					// plainly while the desk was still insisting it was the impact.
					// The screenshots show it: the gun off to the right, the
					// flash lying on the floor near the dais.
					//
					// CWeaponModel::UpdateFlash sets the flash's position from
					// m_vFlashPos, which is a WORLD point - VRPrims_
					// GunPointFromOffsetLast has already put the authored muzzle
					// offset in the gun's frame. The object carries
					// FLAG_REALLYCLOSE, so this loop then reads that world value
					// back as though it were CAMERA-RELATIVE and re-bases it a
					// second time. A world coordinate run through the camera
					// rebase lands at a fixed spot in the level and stays there,
					// which is exactly what a flash sitting on the carpet in
					// several copies looks like.
					//
					// So take the muzzle straight. The scale multiplier still
					// applies: the flash is authored in the gun's units and
					// without fK it would be the size of a spark.
					LTVector vMuzNow;
					const HLOCALOBJ hFx  = m_weaponModel.VRGetFlashHandle();
					const HLOCALOBJ hFxP = m_weaponModel.VRFlashParticleObject();
					const HLOCALOBJ hFxL = m_weaponModel.VRFlashLightObject();
					const bool bThisIsFlash = (hFx && sobjs[z] == hFx)
										   || (hFxP && sobjs[z] == hFxP)
										   || (hFxL && sobjs[z] == hFxL);
					if (bThisIsFlash && VRPrims_DrawnMuzzle(vMuzNow))
					{
						q = vMuzNow;
						fCloseK = VRPrims_Rebase().fK;

						static int s_nSaidFlashSpr = 0;
						if (s_nSaidFlashSpr < 6)
						{
							++s_nSaidFlashSpr;
							VRLog::Msg("VRFlashSprite: taken straight to the muzzle"
									   " %.0f %.0f %.0f (no re-base), scale x%.1f",
									   q.x, q.y, q.z, fCloseK);
						}
					}
					else if (!bIface)
					{
						q = VRPrims_RebasePoint(q);
						fCloseK = VRPrims_Rebase().fK;
					}
				}
				si.fPos[0] = q.x; si.fPos[1] = q.y; si.fPos[2] = q.z;
				LTVector sc(1.0f, 1.0f, 1.0f);
				g_pLTClient->GetObjectScale(sobjs[z], &sc);
				si.fScale[0] = sc.x * fCloseK; si.fScale[1] = sc.y * fCloseK; si.fScale[2] = sc.z * fCloseK;
				float cr = 1.0f, cg = 1.0f, cb = 1.0f, ca = 1.0f;
				g_pLTClient->GetObjectColor(sobjs[z], &cr, &cg, &cb, &ca);
				si.fColour[0] = cr; si.fColour[1] = cg;
				si.fColour[2] = cb; si.fColour[3] = ca;

				// AND ITS OWN AXES, WHEN IT HAS THEM. FLAG_ROTATEABLESPRITE is
				// the engine's way of saying "I am not a billboard" - a decal
				// carries a rotation whose forward is the surface normal so it
				// lies flat on the wall. Publishing right and up lets the
				// renderer build the quad in that plane instead of facing the
				// camera; without them every bullet hole and blood splat turns
				// to follow the player's head.
				si.fRight[0] = 1.0f; si.fRight[1] = 0.0f; si.fRight[2] = 0.0f;
				si.fUp[0] = 0.0f; si.fUp[1] = 1.0f; si.fUp[2] = 0.0f;
				{
					uint32 dwSF = g_pLTClient->GetObjectFlags(sobjs[z]);
					if (dwSF & FLAG_ROTATEABLESPRITE)
					{
						si.nFlags |= VRSPRITE_F_ROTATABLE;
						LTRotation rS;
						g_pLTClient->GetObjectRotation(sobjs[z], &rS);
						LTVector vSU, vSR, vSF;
						g_pLTClient->GetRotationVectors(&rS, &vSU, &vSR, &vSF);
						si.fRight[0] = vSR.x; si.fRight[1] = vSR.y;
						si.fRight[2] = vSR.z;
						si.fUp[0] = vSU.x; si.fUp[1] = vSU.y; si.fUp[2] = vSU.z;
					}
				}
			}
			s_pfnS(&s_spr);
			static uint32 s_nSaidSpr = 0;
			if (m_nVRModelFrame - s_nSaidSpr >= 450)
			{
				s_nSaidSpr = m_nVRModelFrame;
				uint32 nAdd = 0;
				for (uint32 z2 = 0; z2 < s_spr.nCount; ++z2)
					if (s_spr.inst[z2].nFlags & VRSPRITE_F_ADDITIVE) ++nAdd;
				// "of 512 objects" IS THE ARRAY SIZE, NOT A FINDING. The
				// sphere is asked for everything in the level and fills the
				// buffer; how many it actually FOUND has never been printed,
				// so a truncated query and a complete one read identically.
				// A bullet mark created a second ago competes with every
				// sprite in the level for a place in an arbitrary 512.
				VRLog::Msg("VRSprites: %u sprites published (of %u returned,"
					" %u FOUND%s),"
					" %u ADDITIVE", s_spr.nCount, nS, nSFound,
					// TRUNCATION MEANS THE BUFFER WAS FULL, not that the two
					// counts differ by one. The cap is VRSPRITE_MAX (4096) and
					// the query routinely reports one more FOUND than it
					// returns - "292 returned, 293 FOUND" was flagged as a
					// truncated query five times in one run with 3800 places
					// still free. A warning that fires when nothing is wrong
					// is how a reader learns to skip the line.
					((nSFound > nS) && (nS >= VRSPRITE_MAX - 1))
						? "  <- TRUNCATED: the buffer was full and the rest were"
						  " never considered" : "",
					nAdd);
			}
		}
	}

	// ---- FOG, which this client has been computing since 2000 -----------
	//
	// UpdateSpecialFX takes the level's fog out of its WorldProperties and
	// writes it into the ENGINE console - FogEnable, FogR/G/B, FogNearZ,
	// FogFarZ - because that is how the retail d3d.ren is told about it. Our
	// renderer has never read those, so no level in the game has had any fog
	// and the intro's night forest has a pure black sky where the original has
	// a dark blue haze.
	//
	// Nothing here computes anything: it hands over six numbers the client
	// already put in the console a moment ago. The renderer ignores a repeat of
	// what it already holds, so publishing every frame costs one call.
	{
		static VRFogPublishFn s_pfnF = NULL;
		static HMODULE s_hRenSeenF = (HMODULE)(uintptr_t)1;
		{
			HMODULE h = GetModuleHandleA("d3dstub.ren");
			if (h != s_hRenSeenF)
			{
				s_hRenSeenF = h;
				s_pfnF = h ? (VRFogPublishFn)GetProcAddress(h, "R3D_PublishFog") : NULL;
				VRLog::Msg("VRFog: publish entry %s", s_pfnF ? "resolved" : "missing");
			}
		}
		if (s_pfnF)
		{
			// The colour is written as 0..255 per channel and the distances in
			// world units, which is what the shader wants once the colour is
			// scaled down.
			const int bOn = GetConsoleInt("FogEnable", 0);
			s_pfnF(bOn,
				   (float)GetConsoleInt("FogR", 0) / 255.0f,
				   (float)GetConsoleInt("FogG", 0) / 255.0f,
				   (float)GetConsoleInt("FogB", 0) / 255.0f,
				   (float)GetConsoleInt("FogNearZ", 100),
				   (float)GetConsoleInt("FogFarZ", 5000));
		}
		// AND THE SKY'S OWN FOG, the same way. UpdateSpecialFX mirrors
		// SkyFogEnable / SkyFogNearZ / SkyFogFarZ from the world properties
		// into the console; the retail renderer fogs its sky pass by them and
		// ours never knew they existed, so the stormy harbour's sky was the
		// cloud sheet's own purple where retail's is half sunk in fog.
		{
			typedef void (__cdecl* VRSkyFogPublishFn)(int, float, float);
			static VRSkyFogPublishFn s_pfnSF = NULL;
			static HMODULE s_hRenSeenSF = (HMODULE)(uintptr_t)1;
			HMODULE h = GetModuleHandleA("d3dstub.ren");
			if (h != s_hRenSeenSF)
			{
				s_hRenSeenSF = h;
				s_pfnSF = h ? (VRSkyFogPublishFn)GetProcAddress(h, "R3D_PublishSkyFog") : NULL;
				VRLog::Msg("VRFog: sky fog publish entry %s", s_pfnSF ? "resolved" : "missing");
			}
			if (s_pfnSF)
				s_pfnSF(GetConsoleInt("SkyFogEnable", 0),
						(float)GetConsoleInt("SkyFogNearZ", 100),
						(float)GetConsoleInt("SkyFogFarZ", 1000));
		}
	}

	// ---- THE GLOBAL LIGHT SCALE, the same way -----------------------------
	//
	// CLightScaleMgr multiplies the whole scene through the engine's global
	// light scale: a container's tint, the damage darkening, the interface
	// fade and the level's time-of-day colour, which is what makes a night
	// level a night level. The retail renderer applied it to every pixel; ours
	// never read it, so the Siberian intro was a third brighter than retail
	// and less blue (12 September, headset screenshots). The engine's copy is
	// read back every frame rather than hooking the six places that write it,
	// so nothing that sets it can be missed. The renderer ignores a repeat.
	{
		typedef void (__cdecl* VRLightScalePublishFn)(float, float, float);
		static VRLightScalePublishFn s_pfnLS = NULL;
		static HMODULE s_hRenSeenLS = (HMODULE)(uintptr_t)1;
		{
			HMODULE h = GetModuleHandleA("d3dstub.ren");
			if (h != s_hRenSeenLS)
			{
				s_hRenSeenLS = h;
				s_pfnLS = h ? (VRLightScalePublishFn)GetProcAddress(h, "R3D_PublishLightScale") : NULL;
				VRLog::Msg("VRLightScale: publish entry %s", s_pfnLS ? "resolved" : "missing");
			}
		}
		if (s_pfnLS)
		{
			LTVector vLS(1.0f, 1.0f, 1.0f);
			g_pLTClient->GetGlobalLightScale(&vLS);
			s_pfnLS(vLS.x, vLS.y, vLS.z);
		}
		// AND THE CAMERA'S LIGHT ADD, the same way. CScreenTintMgr sets it to
		// the maximum of its tints - a water container's LightAdd, the damage
		// flash, the poison's green - and the retail renderer added it over
		// every pixel. Ours never read it: underwater kept only the light
		// SCALE and came out near black, and the poison tinted nothing.
		{
			typedef void (__cdecl* VRLightAddPublishFn)(float, float, float);
			static VRLightAddPublishFn s_pfnLA = NULL;
			static HMODULE s_hRenSeenLA = (HMODULE)(uintptr_t)1;
			HMODULE h = GetModuleHandleA("d3dstub.ren");
			if (h != s_hRenSeenLA)
			{
				s_hRenSeenLA = h;
				s_pfnLA = h ? (VRLightAddPublishFn)GetProcAddress(h, "R3D_PublishLightAdd") : NULL;
				VRLog::Msg("VRLightAdd: publish entry %s", s_pfnLA ? "resolved" : "missing");
			}
			if (s_pfnLA && m_hCamera)
			{
				LTVector vAdd(0.0f, 0.0f, 0.0f);
				g_pLTClient->GetCameraLightAdd(m_hCamera, &vAdd);
				static LTVector s_vLastAdd(-1.0f, -1.0f, -1.0f);
				if (vAdd.x != s_vLastAdd.x || vAdd.y != s_vLastAdd.y || vAdd.z != s_vLastAdd.z)
				{
					s_vLastAdd = vAdd;
					VRLog::Msg("VRLightAdd: camera light add -> %.3f %.3f %.3f", vAdd.x, vAdd.y, vAdd.z);
				}
				s_pfnLA(vAdd.x, vAdd.y, vAdd.z);
			}
		}
	}

	// Report the PEAK, periodically. This used to log once, on the first frame
	// that had any models - the spawn, where nothing is dropped - so it read
	// "0 dropped" for a session in which models were visibly coming and going.
	// A counter sampled where it cannot be non-zero is not a measurement.
	// ---- PARTICLES, RAIN, WATER, CANVASES - see VRPrims.cpp ------------
	// The same sphere as the sprites: an effect across the street is still
	// an effect. Nothing at the menu; the interface has none of these.
	if (!bFolder)
		// THE MUZZLE FLASH, HANDED OVER. Its particle system and its light
		// are FLAG_REALLYCLOSE, and the engine's object query does not
		// return camera-relative objects at all - so no amount of
		// searching finds them. NOLF's first-person flash is a 7.5 ms
		// particle puff plus a 200-unit light (ATTRIBUTES/FX.TXT); with
		// neither of them reaching the renderer there has never been a
		// first-person muzzle flash in this port. 
		// the muzzle flash was not attached to the gun.
		VRPrims_AddExtra(m_weaponModel.VRFlashParticleObject());
		VRPrims_AddExtra(m_weaponModel.VRFlashLightObject());
		VRPrims_Publish(vEye, g_vtVRSpriteRange.GetFloat(), m_nVRModelFrame);

	static uint32 s_nPeakDropped = 0, s_nPeakCount = 0, s_nPeakNodes = 0;
	static uint32 s_nSaidAt = 0;
	if (s_frame.nDropped  > s_nPeakDropped) s_nPeakDropped = s_frame.nDropped;
	if (s_frame.nCount    > s_nPeakCount)   s_nPeakCount   = s_frame.nCount;
	if (s_frame.nNodeCount> s_nPeakNodes)   s_nPeakNodes   = s_frame.nNodeCount;
	if (m_nVRModelFrame - s_nSaidAt >= 450)		// ~5 s at 90 fps
	{
		s_nSaidAt = m_nVRModelFrame;
		// WHICH IGNORED FLAGS THIS LEVEL ACTUALLY USES. Cumulative over the
		// run, so the denominator is instance-frames rather than objects -
		// the ratio is what matters, not the absolute.
		VRLog::Msg("VRFlags: of %u instance-frames, REALLYCLOSE %u, MODELTINT %u,"
			" ENVMAP %u, DETAILTEX %u, SHADOW %u, ANIMTRANSITION %u, PORTALVISIBLE %u"
			"  - none of these are acted on (the bits are shared by object"
			" type; these are the MODEL meanings)",
			s_nFlagSeen, s_nFlagClose, s_nFlagTint, s_nFlagEnvMap,
			s_nFlagDetail, s_nFlagShadow, s_nFlagAnimTrans, s_nFlagPortal);
		VRLog::Msg("VRModels: %u instances, %u nodes, %u dropped this frame"
			" (of %u objects within %.0f units; %u of %u model candidates were"
			" behind the camera and are published last) | PEAK %u inst, %u nodes,"
			" %u DROPPED%s", s_frame.nCount, s_frame.nNodeCount,
			s_frame.nDropped, nOut, g_vtVRModelRange.GetFloat(),
			nBehindSeen, nCand,
			s_nPeakCount, s_nPeakNodes, s_nPeakDropped,
			s_nPeakDropped ? "  <- models are being dropped; nearest are kept"
						   : "");
		VRLog::Msg("VRModels: past the character range (VRModelRangeAnimated %.0f, fog %s %d), %u scenery models kept and %u characters/bodies left out",
			g_vtVRModelRangeAnim.GetFloat(), GetConsoleInt("FogEnable", 0) ? "on" : "off", GetConsoleInt("FogFarZ", 0), s_nVRFarSceneryKept, s_nVRFarAnimSkipped);
		s_nVRFarSceneryKept = 0; s_nVRFarAnimSkipped = 0;
		if (s_nUnposedNodes)
			VRLog::Msg("VRModels: %u nodes the engine would not pose, published"
				" zeroed to keep the indices aligned", s_nUnposedNodes);
		if (s_nAttSeen)
		VRLog::Msg("VRAttach: %u attachments processed, %u were in the wrong"
			" place (mean %.1f units, worst %.1f) - nothing renders these"
			" through the engine, so nothing else updates them",
			s_nAttSeen, s_nAttMoved,
			s_nAttMoved ? s_fAttMoveSum / (float)s_nAttMoved : 0.0f,
			s_fAttMoveMax);
		s_nAttSeen = 0; s_nAttMoved = 0;
		// The world-model half, on its own line. It is a different claim: not
		// "attachments are being fixed up" but "brush entities have attachments
		// at all, and they were in the wrong place".
		if (s_nWMAttSeen)
			// PROCESSED, not "carry attachments" - the first wording claimed
			// something this cannot know. And the movement gate below is NOT
			// yet doing its job: this reads ~235 a frame on Morocco, which is
			// every world model, not the two that opened. The pass is cheap
			// enough that it does not show in the mesh cost, so it ships as
			// it is, but the number is here so the next reader can see that
			// the gate is not gating rather than assume it is.
			VRLog::Msg("VRWorldAttach: %u world models PROCESSED;"
					   " %u objects checked, %u were in the wrong place and"
					   " were moved (worst %.1f units) - door handles, lock"
					   " plates and padlocks are separate prop models here",
					   s_nWMWithAtt, s_nWMAttSeen, s_nWMAttMoved, s_fWMAttMax);
		s_nWMAttSeen = 0; s_nWMAttMoved = 0; s_nWMWithAtt = 0;
		s_fAttMoveSum = 0.0f; s_fAttMoveMax = 0.0f;
	}
	VRPubMark(3);
}

// ---- VRTypeWhenUnfocused: THE CHAT LINE WITH A HEADSET ON -------------
//
// with the headset on, T opens "Say:" and nothing can be
// typed into it. At the desk with the game window focused it works perfectly -
// the tester typed mpmaphole and it skipped the scene - so the bind, the input state
// and the text entry are all healthy.
//
// The variable is FOREGROUND, and it was measured rather than guessed. Opening
// chat is a BIND, which rides DirectInput, and the proxy now acquires the
// keyboard in BACKGROUND so it no longer needs focus. Typing rides WM_CHAR,
// which Windows delivers only to the window holding keyboard focus. With the
// headset live nothing the host knows about holds it: the run logged ZERO
// VRChar in the client and ZERO keys at the preview, while the preview was
// demonstrably up and covering the game window for 29 seconds.
//
// The host was the wrong place to fix it. Trying to take the foreground from
// the host meant AttachThreadInput across processes, which joins two threads'
// input queues - and doing that to a game still loading deadlocked it on the
// splash screen. That attempt is reverted.
//
// So: stop needing focus at all. The physical key state is readable without it,
// and the chat line can be fed directly. Two properties keep this honest:
//
//   * it runs ONLY while the chat line is actually open, so it cannot eat
//     keystrokes meant for anything else during normal play;
//   * it runs ONLY while this process is NOT the foreground window. When the
//     game does have focus WM_CHAR already works, and injecting as well would
//     type every letter twice - which is the obvious way for a fix like this
//     to look broken.
void CGameClientShell::VRTypeWhenUnfocused()
{
	// Our own edge state. GetAsyncKeyState's "pressed since last call" bit is
	// shared process-wide and anything else calling it steals the edge, so the
	// previous state is kept here instead.
	static bool s_bWasDown[256] = { false };
	static bool s_bWasEditing = false;

	CMessageMgr* pMsg = m_InterfaceMgr.GetMessageMgr();
	if (!pMsg || !pMsg->GetEditingState())
	{
		// Not editing: forget the edges so a key held while the line opens is
		// not delivered as a fresh press the moment it does.
		memset(s_bWasDown, 0, sizeof(s_bWasDown));
		s_bWasEditing = false;
		return;
	}

	// THE FIRST FRAME OF THE CHAT LINE DRAINS, IT DOES NOT DELIVER.
	//
	// The tap bit read below means "pressed since this thread last asked", and
	// while the line was closed nobody was asking. Without this, everything
	// typed in the seconds before opening chat - including the T that opened
	// it - arrives at once as the first word of the message.
	if (!s_bWasEditing)
	{
		s_bWasEditing = true;
		for (int vk = 0; vk < 256; ++vk)
		{
			const SHORT n = GetAsyncKeyState(vk);
			s_bWasDown[vk] = (n & 0x8000) != 0;
		}
		return;
	}

	DWORD nFgPid = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &nFgPid);
	if (nFgPid == GetCurrentProcessId()) return;	// focused: WM_CHAR does it

	const bool bShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
	const bool bCaps  = (GetKeyState(VK_CAPITAL) & 1) != 0;

	struct Punct { int vk; char plain; char shifted; };
	static const Punct kPunct[] = {
		{ VK_SPACE,      ' ',  ' '  }, { VK_OEM_MINUS,  '-',  '_'  },
		{ VK_OEM_PLUS,   '=',  '+'  }, { VK_OEM_PERIOD, '.',  '>'  },
		{ VK_OEM_COMMA,  ',',  '<'  }, { VK_OEM_1,      ';',  ':'  },
		{ VK_OEM_2,      '/',  '?'  }, { VK_OEM_7,      '\'', '"'  },
	};

	// WHAT WAS DELIVERED, not how many. A count cannot show that the letters
	// arrived in the right ORDER, or that none was doubled or dropped - and a
	// screenshot cannot help here, because LithTech tears its renderer down on
	// focus loss and every capture taken while unfocused comes back black. The
	// log is the only instrument that can see this, so it records the text.
	int nSent = 0;
	char szSent[32] = "";
	int  nSentLen = 0;
	for (int vk = 0; vk < 256; ++vk)
	{
		// POLLING ALONE LOSES FAST KEYSTROKES, and that is measured rather than
		// feared: driving this with SendKeys, which presses and releases inside
		// one frame, delivered ONE key out of ten. A key that goes down and up
		// between two polls is invisible to "is it down now".
		//
		// GetAsyncKeyState's low bit means "pressed since the last time this
		// thread asked", which is exactly the missing case. Counted only when
		// the key is no longer down, so a key still held is not delivered twice
		// - once by the edge and again by the tap bit.
		const SHORT nState = GetAsyncKeyState(vk);
		const bool  bDown  = (nState & 0x8000) != 0;
		const bool  bTap   = (nState & 0x0001) != 0;
		const bool  bEdge  = bDown && !s_bWasDown[vk];
		s_bWasDown[vk] = bDown;
		if (!bEdge && !(bTap && !bDown)) continue;

		if (vk == VK_RETURN || vk == VK_BACK || vk == VK_ESCAPE)
		{
			// Enter, backspace and escape are not characters: the chat line
			// takes them through the key path, not OnChar.
			m_InterfaceMgr.OnKeyDown(vk, 1);
			++nSent;
			if (nSentLen < 28)
			{
				const char* p = (vk == VK_RETURN) ? "[ENTER]"
							  : (vk == VK_BACK)   ? "[BKSP]" : "[ESC]";
				while (*p && nSentLen < 28) szSent[nSentLen++] = *p++;
				szSent[nSentLen] = 0;
			}
			continue;
		}

		char c = 0;
		if (vk >= 'A' && vk <= 'Z')
			c = (char)((bShift != bCaps) ? vk : (vk - 'A' + 'a'));
		else if (vk >= '0' && vk <= '9' && !bShift)
			c = (char)vk;
		else
			for (size_t p = 0; p < sizeof(kPunct) / sizeof(kPunct[0]); ++p)
				if (kPunct[p].vk == vk) { c = bShift ? kPunct[p].shifted : kPunct[p].plain; break; }

		if (c >= ' ')
		{
			m_InterfaceMgr.OnChar(c);
			++nSent;
			if (nSentLen < 30) { szSent[nSentLen++] = c; szSent[nSentLen] = 0; }
		}
	}

	if (nSent)
	{
		static int s_nSaid = 0;
		if (s_nSaid < 24)
		{
			++s_nSaid;
			VRLog::Msg("VRType: fed '%s' (%d key(s)) to the chat line while the game"
				" was NOT the foreground window (foreground pid %lu, ours %lu)",
				szSent, nSent, nFgPid, GetCurrentProcessId());
		}
	}
}

// ---- VRDebugWeapon: PUT A GUN IN THE PLAYER'S HAND, RETRIED --------------------
//
// The arsenal cheat gives every weapon and selects none, so the player stands
// there holding 'fisty_cuffs', and fists have no muzzle flash. A desk run that
// fires for ten seconds and photographs nothing then looks exactly like a
// broken muzzle flash - which is what it looked like twice on 11 September,
// and the flash was never the thing being measured.
//
// Retried because the weapons come from the SERVER: the command is refused
// until they arrive, a second or two into the world. It stops as soon as the
// weapon model reports the change, so it cannot fight a player who picks
// something else. Called from the controller update because that is a
// function this path is KNOWN to reach every frame - the first attempt sat in
// CGameClientShell::Update beside VRTele and never ran once.
// CAN THE PLAYER ACTUALLY DRAW A FIREARM?
//
// The arsenal cheat is fire-and-forget - the client asks the server and is
// never told whether anything happened - so the only honest test is what the
// player's own stats say. And possession is NOT the test: Cate starts the
// level carrying a pistol with no ammunition, so "has a weapon" reads true
// while the game refuses every attempt to draw one. CWeaponModel::
// CanChangeToWeapon declines an empty weapon, which is why twenty next-weapon
// commands changed nothing and looked like a broken control.
//
// So: a player weapon, not the fists, that the player holds AND has ammunition for.
bool CGameClientShell::VRHasAnyGun()
{
	CPlayerStats* pStats = GetPlayerStats();
	if (!pStats || !g_pWeaponMgr) return false;
	const int nWeapons = g_pWeaponMgr->GetNumWeapons();
	for (int i = 0; i < nWeapons; ++i)
	{
		if (!g_pWeaponMgr->IsPlayerWeapon(i)) continue;
		if (!pStats->HaveWeapon((uint8)i))    continue;
		WEAPON* pW = g_pWeaponMgr->GetWeapon(i);
		if (!pW) continue;
		if (stricmp(pW->szName, "fisty_cuffs") == 0) continue;
		if (m_weaponModel.IsOutOfAmmo((uint8)i))     continue;
		return true;
	}
	return false;
}

extern unsigned long VRMuzzleFlashShownCount();

// THE SHOT LINE FOLLOWS THE ALIGNMENT. The automatic barrel alignment (in the
// rest capture) rotates the drawn gun so a line between two of its pivots
// lies on the hand's +z. The gun's authored +z - the barrel the flat game
// fired along - is carried by that same rotation to the MIRROR of the pivot
// line through +z: pitch -p, yaw -y. The shot, the reticle and the flash
// stayed on +z, so every aligned gun fired above its own barrel by its
// alignment. The P38, aligned 11.7 down: reticle, flash and burst 11.7 above
// the barrel; an AIM of -11.7 put them on it with no
// further press after 120 shots. This table hands WeaponModel the per-weapon
// alignment so the shot line goes with the gun automatically; the tuner's AIM
// is a trim on top. VRAimFollowsAlignment 0 is the old arm.
static float s_fAlignPitch[128], s_fAlignYaw[128];
static void VRAlignTrimSet(int nWeaponId, float fPitch, float fYaw)
{
	if (nWeaponId < 0 || nWeaponId >= 128) return;
	s_fAlignPitch[nWeaponId] = fPitch; s_fAlignYaw[nWeaponId] = fYaw;
}
void VRAlignTrimForWeapon(int nWeaponId, float& fPitch, float& fYaw)
{
	fPitch = 0.0f; fYaw = 0.0f;
	if (nWeaponId < 0 || nWeaponId >= 128) return;
	fPitch = s_fAlignPitch[nWeaponId]; fYaw = s_fAlignYaw[nWeaponId];
}

void CGameClientShell::VRDebugSelectWeapon()
{
#if VR_DEBUG_TOOLS
	// SAY WHY IT DID NOTHING. A switch that silently declines is the thing
	// that cost three runs here already.
	static int s_nWhySaid = 0;
	if (s_nWhySaid < 3)
	{
		++s_nWhySaid;
		VRLog::Msg("VRDebugWeapon: called - cvar %.1f, weapon mgr %s, weapon id %d",
				   g_vtVRDebugWeapon.GetFloat(), g_pWeaponMgr ? "present" : "NULL",
				   m_weaponModel.GetWeaponId());
	}
	// A NAME ALONE IS ENOUGH. On 21 September the Contender was asked for by
	// name with no VRDebugWeapon, this line returned before the name was
	// read, and the quick save's AK47 came up instead.
	const bool bNamed = !GetConsoleString("VRDebugWeaponName", "").empty();
	if ((g_vtVRDebugWeapon.GetFloat() <= 0.0f && !bNamed) || !g_pWeaponMgr) return;

	// THE NTH WEAPON THE PLAYER CAN ACTUALLY DRAW, chosen here rather than asked for
	// by slot or by "next".
	//
	// Both of the obvious routes fail silently. A slot number refuses
	// whenever the player does not hold that exact weapon, and slot 2 of NOLF's list
	// is not one the arsenal hands over. COMMAND_ID_NEXT_WEAPON is not a
	// weapon command at all - it is outside the weapon command range, so
	// OnCommandOn routes it to the weapon CHOOSER, which opens a panel and
	// waits for a confirmation that a harness never sends. Twenty of them
	// changed nothing and read as "the player has no weapons", which was
	// wrong twice over.
	//
	// So walk the weapon list, skip what the player cannot draw, and send the command
	// for the one we picked.
	static int    s_nWant  = -1;
	static double s_fLast  = 0.0;
	static int    s_nTries = 0;
	static int s_nSkip = 0;		// candidates stepped over for being silenced
	const int nWant = (int)g_vtVRDebugWeapon.GetFloat() + s_nSkip;
	// BY NAME, when asked: +VRDebugWeaponName p38 picks that weapon whatever its
	// position in the list, and ignores the P38 exclusion and the silencer skip
	// (VRDebugNoSilencer takes the silencer off). 
	// the AK47 came up instead - the P38 is the one gun the count cannot reach.
	const std::string sByName = GetConsoleString("VRDebugWeaponName", "");
	const bool bByName = !sByName.empty();
	if (nWant != s_nWant) { s_nWant = nWant; s_nTries = 0; }
	if (s_nTries >= 20) return;

	const double fNow = g_pLTClient->GetTime();
	if (fNow - s_fLast <= 0.7) return;
	s_fLast = fNow;

	CPlayerStats* pStats = GetPlayerStats();
	if (!pStats) return;

	// A WEAPON THAT CAN ACTUALLY PRODUCE A FLASH, when asked for one.
	//
	// Two of NOLF's guns will waste a whole run. The P38 is Cate's SILENCED
	// pistol and CWeaponModel deliberately skips the flash for a silenced
	// weapon - "if (!m_bHaveSilencer) StartFlash()" - and the coin, the
	// lighter and the rest of the gadgets are player weapons with no muzzle
	// at all. Both were picked on 11 September and both produced a run that
	// fired for twenty seconds and showed the flash zero times, which reads
	// exactly like a broken muzzle flash.
	//
	// So the list is walked for a weapon that DECLARES a first-person muzzle
	// effect in its bute, and the silenced pistol is stepped over.
	const int nWeapons = g_pWeaponMgr->GetNumWeapons();
	int nFound = 0;
	for (int i = 0; i < nWeapons; ++i)
	{
		if (!g_pWeaponMgr->IsPlayerWeapon(i))     continue;
		if (!pStats->HaveWeapon((uint8)i))        continue;
		WEAPON* pW = g_pWeaponMgr->GetWeapon(i);
		if (!pW)                                  continue;
		// By name, a gadget with no muzzle effect (the coin, the code breaker)
		// is still a valid pick: its size and grip are tuned like any gun's.
		if (!bByName && !pW->pPVMuzzleFX)         continue;
		if (bByName)
		{
			char szSlugN[64] = "";
			VRWeaponSlugForId(i, szSlugN, sizeof(szSlugN));
			if (stricmp(pW->szName, sByName.c_str()) != 0
				&& stricmp(szSlugN, sByName.c_str()) != 0) continue;
		}
		else
		{
			if (stricmp(pW->szName, "P38") == 0)      continue;
			if (m_weaponModel.IsOutOfAmmo((uint8)i))  continue;
			if (++nFound < nWant) continue;
		}
		if (m_weaponModel.GetWeaponId() == i)
		{
			// ALREADY HOLDING IT - but if a silencer is fitted, this weapon
			// can never show a flash (UpdateWeaponModel does not even call
			// UpdateFlash when one is), so step past it and take the next.
			if (m_weaponModel.VRHaveSilencer() && !bByName)
			{
				++s_nSkip;
				VRLog::Msg("VRDebugWeapon: '%s' has a silencer fitted - no muzzle"
						   " flash is possible with it, taking the next one",
						   pW->szName);
				return;
			}
			// AND TOP THE CLIP UP, because the cheat does not.
			//
			// SetKFA -> SetFullWeapons says it "gives us all ammo too", and it
			// does - to the RESERVE. The magazine is left alone, so the quick
			// save loads the AK47 at 0/270, the Contender at 0/22 and the Luger
			// at 0/190. Holding the trigger then makes the weapon reload first
			// and run dry a few seconds later, and the firing window that
			// catches a 30-round AK mid-magazine has already emptied an
			// 8-round Luger.
			//
			// That cost most of a 17-weapon sweep on 20 September: the captures
			// came back showing an idle gun and "0/210" rather than a muzzle
			// flash, and 15 weapons went unverified for want of a bullet.
			//
			// This branch runs every 0.7s for as long as the weapon is held, so
			// refilling here keeps it full THROUGH the burst - no window can
			// empty it and no reload animation interrupts the shot. bForce, and
			// no reload animation, because a harness wants the ammo and not the
			// performance.
			//
			// Only reachable with VRDebugWeapon set, which is a debug-tools
			// build only, so live play cannot see it.
			m_weaponModel.ReloadClip(LTFALSE, -1, LTTRUE);

			// NOT DONE - KEEP WATCHING. The mods arrive from the server after
			// the weapon does, so a check made the moment the gun lands reads
			// "no silencer" and a check two seconds later reads "fitted".
			// Declaring victory on the first one is how the Delisle was
			// accepted three times running. Each look costs a try, so this
			// still terminates.
			++s_nTries;
			return;
		}
		++s_nTries;			// only a look at a real candidate counts as a try

		const int nBefore = m_weaponModel.GetWeaponId();
		const int nCmd    = g_pWeaponMgr->GetCommandId(i);
		// NO DESELECT ANIMATION. The ordinary path sets m_nRequestedWeaponId
		// and waits for the put-away animation to finish; the switch then
		// happens in the animation's completion. Asking for the P38 twelve
		// times changed nothing because every request restarted that wait.
		// bCanDeselect LTFALSE swaps immediately, which is what a harness
		// wants and what a cheat does.
		m_weaponModel.ChangeWeapon((uint8)nCmd, LTFALSE, LTTRUE);
		const int nAfter  = m_weaponModel.GetWeaponId();
		VRLog::Msg("VRDebugWeapon %d: '%s' (weapon %d, command %d) asked for on"
				   " try %d - weapon id went %d -> %d%s",
				   nWant, pW->szName, i, nCmd, s_nTries, nBefore, nAfter,
				   (nAfter == nBefore) ? "  (the select animation may still be"
										 " running)" : "");
		// NOT DONE YET, even if the weapon changed. Whether it can show a
		// flash depends on a SILENCER being fitted, and that is only knowable
		// once the weapon is actually in hand - so let the next tick come
		// back here, take the branch above, and step past it if it is
		// silenced. Every weapon Cate starts with is silenced, which is how
		// three runs in a row measured a gun that is not allowed to flash.
		return;
	}
	// A LOOK THAT FOUND NOTHING IS NOT A TRY. The first version counted it,
	// spent its whole budget in the nine seconds before a quick save had
	// finished loading, and then reported that the player owned no guns while
	// the player was standing there holding one.
	if (nFound == 0) return;
	if (s_nTries >= 20)
		VRLog::Msg("VRDebugWeapon %d: twenty attempts and the weapon never"
				   " changed - only %d of the list are his and loaded",
				   nWant, nFound);
#endif	// VR_DEBUG_TOOLS
}

#if VR_DEBUG_TOOLS
// Held 0.25 s every 2 s once the delay has passed. Long enough that a flash
// lasting 5-7.5 ms lands inside a dumped frame, and spaced far enough apart
// that the weapon finishes its fire animation between bursts instead of being
// re-triggered mid-cycle.
static bool VRDebugFireWants()
{
	if (g_vtVRDebugFire.GetFloat() <= 0.0f || !g_pLTClient) return false;
	static int    s_nFor   = -1;
	static double s_fStart = -1.0;
	const double fNow = g_pLTClient->GetTime();
	if (s_nFor != g_nVRWorldEntries) { s_nFor = g_nVRWorldEntries; s_fStart = fNow; }
	const double fIn = fNow - s_fStart - (double)g_vtVRDebugFire.GetFloat();
	if (fIn < 0.0) return false;
	const double fCycle = fIn - (double)(int)(fIn / 2.0) * 2.0;
	return fCycle < 0.25;
}
#endif	// VR_DEBUG_TOOLS

// AND DRIVE IT FROM Update(), not from the controller path.
//
// VRUpdateControllerInput returns early when no controller is active - "if
// (!L.nActive && !R.nActive) { Local::ReleaseAll(this); return; }" - and that
// is every desk run, because the fake host reports no controllers at all. The
// first version of this switch hung the trigger off that path and therefore
// fired exactly never, which tools/flash-test.ps1 caught on its first run by
// refusing to report a zero as a result. A desk trigger that only works when a
// headset is plugged in is no use to anybody.
//
// Slot 9 of kVRHeld is COMMAND_ID_FIRING, and this keeps that slot's edge
// state so the controller path and this one cannot disagree: whichever writes
// first, the other sees the value already set and does nothing.
#if VR_DEBUG_TOOLS
void CGameClientShell::VRDebugFireDrive()
{
	// ONLY WHILE THE SWITCH IS ON. With VRDebugFire at 0 this wanted "not
	// held" every frame, and whenever the CONTROLLER held the trigger it
	// released it here a frame later, every frame - so the controller path
	// pressed again, this path released again, and the weapon never saw the
	// fire flag by the time it looked. standing next
	// to an enemy and pulling the trigger did nothing. The log had 200
	// presses in two seconds and no release. Firing had been dead since this
	// went in on the 11th. The comment above about the two paths agreeing
	// was wrong: they agreed only while both wanted the same thing.
	if (g_vtVRDebugFire.GetFloat() <= 0.0f) return;
	const bool bWant = VRDebugFireWants();
	if (g_bVRHeldOn[9] == bWant) return;
	g_bVRHeldOn[9] = bWant;
	VRShared::SetCommandHeld(COMMAND_ID_FIRING, bWant);
	if (bWant) OnCommandOn(COMMAND_ID_FIRING);
	else       OnCommandOff(COMMAND_ID_FIRING);

	static int s_nSaidFire = -1;
	if (bWant && s_nSaidFire != g_nVRWorldEntries)
	{
		s_nSaidFire = g_nVRWorldEntries;
		VRLog::Msg("VRDebugFire: holding the trigger at the desk"
				   " (0.25 s on, 2 s apart)");
	}
}
#endif	// VR_DEBUG_TOOLS

void CGameClientShell::VRUpdateControllerInput()
{
	VRDebugSelectWeapon();

	// OnCommandOn and OnCommandOff are TRANSITIONS, not a state the engine
	// samples every frame. Sending On repeatedly makes the game see a key that
	// is pressed and never released, so every hold is edge-triggered here.
	struct Local
	{
		static void Set(CGameClientShell* pShell, int nSlot, bool bWant)
		{
			if (g_bVRHeldOn[nSlot] == bWant) return;
			g_bVRHeldOn[nSlot] = bWant;
			// The mask is what makes the player move - CMoveMgr reads it through
			// VRCmdOn every frame. The notification is sent too, because a real
			// key does both and some handlers listen for it.
			VRShared::SetCommandHeld(kVRHeld[nSlot], bWant);
			if (bWant) pShell->OnCommandOn(kVRHeld[nSlot]);
			else       pShell->OnCommandOff(kVRHeld[nSlot]);
			// EVERY TRIGGER EDGE, both ways. 12 September, headset testing: standing
			// next to an enemy and pulling the trigger did nothing.
			// The log had one "first fire" line and one shot in the session,
			// which cannot tell a trigger the game never saw released from a
			// weapon that refused every pull after the first.
			if (kVRHeld[nSlot] == COMMAND_ID_FIRING)
			{
				static int s_nTrigSaid = 0;
				if (s_nTrigSaid++ < 200)
					VRLog::Msg("VRTrigger: %s", bWant ? "PRESSED" : "released");
			}

			// Each command says so the first time it fires, once. Twelve
			// lines a run at most, and it is the only way to tell a control
			// that is unmapped from one that is mapped to the wrong thing -
			// in a headset both are just a button with no visible effect.
			static bool s_bFirst[kVRHeldCount] = { false };
			if (bWant && !s_bFirst[nSlot])
			{
				s_bFirst[nSlot] = true;
				VRLog::Msg("VRControls: first fire of command %d", kVRHeld[nSlot]);
			}
		}
		static void ReleaseAll(CGameClientShell* pShell)
		{
			for (int i = 0; i < kVRHeldCount; ++i) Set(pShell, i, false);
			VRShared::ClearCommands();	// belt and braces: nothing may survive
		}
	};

	if (g_vtVRControls.GetFloat() <= 0.0f) { Local::ReleaseAll(this); return; }

	const VRSharedState& s = VRShared::State();
	const VRHandState&   L = s.Hands[0];
	const VRHandState&   R = s.Hands[1];

	if (!L.nActive && !R.nActive) { Local::ReleaseAll(this); return; }

	// Button edges are tracked whatever the game state, so that a button held
	// across the play/menu boundary does not read as a fresh press on the far
	// side of it.
	// PER HAND. The same bit means different things on the two controllers -
	// PRIMARY is A on the right and X on the left - so a merged edge would
	// make the jump button open doors as well, which in a headset reads as the
	// game doing something random rather than as an input bug.
	static uint32_t s_nLastBtn[2] = { 0, 0 };
	const uint32_t  nWentL = L.nButtons & ~s_nLastBtn[0];
	const uint32_t  nWentR = R.nButtons & ~s_nLastBtn[1];
	s_nLastBtn[0] = L.nButtons;
	s_nLastBtn[1] = R.nButtons;
	const uint32_t  nWent = nWentL | nWentR;	// menus take either hand

	// Say once that this is live. A run where the controllers were never bound
	// and a run where they were bound and did nothing look identical otherwise.
	static bool s_bSaid = false;
	if (!s_bSaid)
	{
		s_bSaid = true;
		VRLog::Msg("VRControls: controllers are driving the game"
			" (left stick moves, right stick turns)");
	}

	float dz = g_vtVRStickDeadzone.GetFloat();
	if (dz < 0.05f) dz = 0.05f;
	if (dz > 0.90f) dz = 0.90f;
	// ON A VEHICLE THE TRIGGERS ARE THE PEDALS: RIGHT is the throttle, LEFT
	// the brake - the game's own REVERSE command, which brakes and, held at a
	// stop, backs up. The layout the standalone VR version of this game uses
	// (the reference clip, 24 September). The 19 September mapping put the
	// throttle on the LEFT trigger to keep the right one for firing, but
	// CVehicleMgr holsters and disables the weapon on every vehicle model, so
	// the right trigger fired nothing while riding. The stick still drives.
	const bool bRiding = m_MoveMgr.GetVehicleMgr()
		&& m_MoveMgr.GetVehicleMgr()->IsVehiclePhysics();
	const bool bThrottle = bRiding && (R.fTrigger > 0.5f || (R.nButtons & VRBTN_TRIGGER));
	const bool bBrake    = bRiding && (L.fTrigger > 0.5f || (L.nButtons & VRBTN_TRIGGER));

	if (m_InterfaceMgr.GetGameState() == GS_PLAYING)
	{
		// Left stick moves, right stick turns - what every VR game does, so it
		// needs no explaining to someone wearing a headset.
		// ON A VEHICLE THE STICK ONLY STEERS: forward and back are the
		// triggers' job, and a stick pushed while steering also changed speed
		//. VRVehicleStickPedals 1 puts it back.
		const bool bStickY = !bRiding || g_vtVRVehicleStickPedals.GetFloat() > 0.0f;
		Local::Set(this,  0, (bStickY && L.fStickY >  dz) || bThrottle);	// FORWARD (or right trigger on a vehicle)
		Local::Set(this,  1, (bStickY && L.fStickY < -dz) || bBrake);	// REVERSE (or left trigger on a vehicle)
		Local::Set(this,  2, L.fStickX < -dz);	// STRAFE_LEFT
		Local::Set(this,  3, L.fStickX >  dz);	// STRAFE_RIGHT
		// SNAP TURNING, when it is asked for.
		//
		// Smooth stick turning is the single most reliable way to make a
		// person ill in VR, and it is the one comfort option every headset
		// game ships. Off by default, because which one suits somebody is a
		// preference and not a defect, and it is on the VR options page.
		//
		// Edge-triggered on the stick leaving centre, so holding the stick
		// over turns once rather than spinning. m_fYaw is the body yaw the
		// camera is built from and it is what the smooth path drives through
		// the LEFT/RIGHT commands, so adding to it directly lands in exactly
		// the same place.
		const bool bSnap = (g_vtVRSnapTurn.GetFloat() > 0.0f);
		if (bSnap)
		{
			static int s_nSnapWas = 0;			// -1 left, +1 right, 0 centred
			const int nNow = (R.fStickX < -dz) ? -1 : (R.fStickX > dz) ? 1 : 0;
			if (nNow != 0 && s_nSnapWas == 0)
			{
				float fDeg = g_vtVRSnapTurnDeg.GetFloat();
				if (fDeg < 5.0f)  fDeg = 5.0f;
				if (fDeg > 90.0f) fDeg = 90.0f;
				m_fYaw += (LTFLOAT)nNow * fDeg * 0.01745329f;
			}
			s_nSnapWas = nNow;
			Local::Set(this, 4, false);
			Local::Set(this, 5, false);
		}
		else
		{
			Local::Set(this,  4, R.fStickX < -dz);	// LEFT  - turn
			Local::Set(this,  5, R.fStickX >  dz);	// RIGHT - turn
		}

		// THE LEFT GRIP IS ALSO THE TUNER'S MODIFIER, so it must not also RUN.
		//
		// VRFlashTuneUpdate took the left grip as its hold-to-adjust key the
		// same morning this line was read, which would have had the player sprinting
		// on the spot every time the player nudged the muzzle - and controls that fight
		// the player is exactly the report that costs an hour to trace back to two
		// features sharing a button. Only while the tuner is ON; ordinary play
		// keeps its run key.
		const bool bTuning = (g_vtVRFlashTune.GetFloat() > 0.0f);
		Local::Set(this,  6, !bTuning && (L.nButtons & VRBTN_GRIP) != 0);	// RUN
		Local::Set(this,  7, (R.nButtons & VRBTN_SECONDARY) != 0);	// DUCK     B
		Local::Set(this,  8, (R.nButtons & VRBTN_PRIMARY)   != 0);	// JUMP     A
		bool bVRFireNow = !bRiding && (R.nButtons & VRBTN_TRIGGER) != 0;	// the throttle while riding
#if VR_DEBUG_TOOLS
		if (VRDebugFireWants())
		{
			bVRFireNow = true;
			static int s_nSaidFire = -1;
			if (s_nSaidFire != g_nVRWorldEntries)
			{
				s_nSaidFire = g_nVRWorldEntries;
				VRLog::Msg("VRDebugFire: holding the trigger at the desk"
						   " (0.25 s on, 2 s apart)");
			}
		}
#endif	// VR_DEBUG_TOOLS
		Local::Set(this,  9, bVRFireNow);	// FIRING

		// Edge-triggered: handled in OnCommandOn's switch, not polled.
		if (nWentL & VRBTN_PRIMARY)   OnCommandOn(COMMAND_ID_ACTIVATE);	// X
		if (nWentL & VRBTN_SECONDARY) OnCommandOn(COMMAND_ID_RELOAD);	// Y

		// THE CONTROLS THAT WERE MISSING. no additional
		// regular game controls such as weapon changes, weapon wheel or
		// flashlight. Two inputs were still unbound on the controllers - the
		// RIGHT stick click and the LEFT trigger - and these are the two most
		// used commands that had no button. Edge-triggered like the pair
		// above: a click is one weapon, not a spin through the list.
		//
		// A weapon WHEEL is a different piece of work (a radial menu drawn
		// in the world) and is not this.
		// THE WEAPON WHEEL, on the right stick click (which was "next weapon").
		// See CVRWeaponWheel. While it is open the right stick belongs to it:
		// no turning, no firing, and the trigger confirms the choice.
		{
			CVRWeaponWheel& wheel = m_InterfaceMgr.GetVRWheel();
			// NO RECENTER CHORD. Both sticks clicked together used to recenter,
			// but that chord is Virtual Desktop's own overlay too. Recenter is
			// the row at the top of Options > VR, or holding the headset's Meta
			// button (the runtime's own recenter, which the host handles).
			if (nWentR & VRBTN_THUMBCLICK)
			{
				if (wheel.IsOpen()) wheel.Close(LTTRUE);
				else if (IsPlayerInWorld() && !IsPlayerDead()) wheel.Open();
			}
			// ...and from the console: VRRecenter 1.
			{
				static VarTrack s_vtRecenter;
				if (!s_vtRecenter.IsInitted()) s_vtRecenter.Init(g_pLTClient, "VRRecenter", LTNULL, 0.0f);
				if (s_vtRecenter.GetFloat() > 0.0f) { VRShared::RequestRecenter(); s_vtRecenter.SetFloat(0.0f); }
			}
			if (wheel.IsOpen())
			{
				wheel.Update(R.fStickX, R.fStickY);
				if (nWentR & VRBTN_TRIGGER) wheel.Close(LTTRUE);
				Local::Set(this, 4, false);
				Local::Set(this, 5, false);
				Local::Set(this, 9, false);
			}
		}
#if VR_DEBUG_TOOLS
		// VR_DEBUG_TOOLS: THE LEVEL SKIP. Right grip is the one button this
		// port binds to nothing at all, so holding it cannot mean anything
		// else, and B on its own is DUCK - the pair together is a deliberate
		// two-finger gesture nobody makes by accident. Off unless
		// Options > VR > "Debug: skip level" is on.
		//
		// Ends the level the way the game's own mpmaphole cheat does, which
		// is the tested path: it hands off to HandleExitLevel rather than
		// loading a world behind the server's back.
		if ((nWentR & VRBTN_SECONDARY)
			&& (R.nButtons & VRBTN_GRIP)
			&& g_vtVRDebugSkip.GetFloat() > 0.0f
			&& g_pCheatMgr)
		{
			VRLog::Msg("VRDebugSkip: right grip + B - ending the level");
			g_pCheatMgr->VRExitLevel();
			return;
		}
#endif
		// THE SCOPE'S ZOOM: a tap of the right grip steps the level up, and
		// past the top drops back to the first. The lens shows a magnified
		// picture at every level; the eyes never zoom. Nothing happens on a
		// gun without a scope, which is what keeps the grip free elsewhere.
		if ((nWentR & VRBTN_GRIP) && !bTuning && !(R.nButtons & VRBTN_SECONDARY))
		{
			CPlayerStats* pStatsZ = m_InterfaceMgr.GetPlayerStats();
			const uint8 nScopeZ = pStatsZ ? pStatsZ->GetScope() : WMGR_INVALID_ID;
			MOD* pModZ = (nScopeZ != WMGR_INVALID_ID && g_pWeaponMgr) ? g_pWeaponMgr->GetMod(nScopeZ) : LTNULL;
			if (pModZ && pModZ->nZoomLevel > 0 && !m_weaponModel.IsDisabled())
			{
				if (m_nZoomView < pModZ->nZoomLevel) { m_bZooming = LTFALSE; OnCommandOn(COMMAND_ID_ZOOM_IN); }
				else
				{
					int nGuard = 8;
					while (m_nZoomView > 0 && nGuard-- > 0) { m_bZooming = LTFALSE; OnCommandOn(COMMAND_ID_ZOOM_OUT); }
				}
				m_bZooming = LTFALSE;
				VRLog::Msg("VRScope: right grip - zoom level now %d of %d", m_nZoomView, pModZ->nZoomLevel);
			}
		}
		if ((nWentL & VRBTN_TRIGGER) && !bRiding) OnCommandOn(COMMAND_ID_FLASHLIGHT);	// L trigger (throttle while riding)

		// THE MENU, and there was no way to reach it from a headset at all.
		//
		// Every other control is bound; this one was not, so the only way out of
		// a level was the keyboard. LEFT thumbstick click, because it is the one
		// remaining input on either controller that is not spoken for, and
		// because a click on the movement stick is deliberate in a way that a
		// face button next to jump would not be.
		//
		// SwitchToFolder(GetMainFolder()) is what the game's own failure and
		// summary screens call to get here, so this is the same door and not an
		// imitation of one. Everything held is released first: a stick held when
		// the menu opens would otherwise still be walking when it closes.
		// THE MENU BUTTON, 19 September: it should simply be the menu button on
		// the Quest controller. The left stick click that stood in for it
		// is released back to nothing.
		if (nWentL & VRBTN_MENU)
		{
			VRLog::Msg("VRControls: left menu button -> the menu");
			Local::ReleaseAll(this);
			m_InterfaceMgr.SwitchToFolder(g_pInterfaceMgr->GetMainFolder());
			return;
		}
		return;
	}

	// --- menus, loading screens, briefings --------------------------------
	// Nothing may stay held across the boundary, or the player is still walking
	// when the menu closes.
	Local::ReleaseAll(this);

	static int s_nSaidMenu = 0;
	if (s_nSaidMenu++ == 0)
		VRLog::Msg("VRControls: in the menu branch, game state %d",
			(int)m_InterfaceMgr.GetGameState());

	// PHOTO MODE. With the pause menu up over a level, a click of the off-hand
	// stick hides the menu so the player can look around the frozen world and
	// take their own screenshot with the headset; another click brings it
	// back. While it is hidden every other button is swallowed - a press would
	// pick a row nobody can see. Leaving the menu any other way ends it.
	// CInterfaceMgr::UpdateFolderState skips drawing the folder while it is on,
	// so nothing reaches the overlay and the host shows the world alone.
	{
		const bool bPausedOverWorld = (m_InterfaceMgr.GetGameState() == GS_FOLDER) && IsInWorld();
		if (!bPausedOverWorld)
		{
			if (g_bVRPhotoMode) VRLog::Msg("VRPhotoMode: off (the menu closed)");
			g_bVRPhotoMode = false;
		}
		else if (nWentL & VRBTN_THUMBCLICK)
		{
			g_bVRPhotoMode = !g_bVRPhotoMode;
			VRLog::Msg("VRPhotoMode: %s", g_bVRPhotoMode ? "ON - the menu is hidden" : "off - the menu is back");
		}
		if (g_bVRPhotoMode) return;
	}

	// Menus are driven by KEYS, not commands: CInterfaceMgr::OnCommandOn
	// handles only ACTIVATE, INVENTORY, CROSSHAIRTOGGLE and FRAGCOUNT, so
	// navigation has to arrive as VK_ codes through OnKeyDown.
	int nKey = 0;
	if      (L.fStickY >  dz || R.fStickY >  dz) nKey = VK_UP;
	else if (L.fStickY < -dz || R.fStickY < -dz) nKey = VK_DOWN;
	else if (L.fStickX < -dz || R.fStickX < -dz) nKey = VK_LEFT;
	else if (L.fStickX >  dz || R.fStickX >  dz) nKey = VK_RIGHT;

	static int    s_nLastKey  = 0;
	static double s_fRepeatAt = 0.0;
	const  double fNow        = VRLog::NowMs();

	double fRepeat = (double)g_vtVRMenuRepeatMs.GetFloat();
	if (fRepeat < 80.0) fRepeat = 80.0;

	if (nKey == 0) s_nLastKey = 0;
	else if (nKey != s_nLastKey || fNow >= s_fRepeatAt)
	{
		// First press is immediate, holding repeats at VRMenuRepeatMs. Without
		// the throttle a stick held past the deadzone fires once per frame and
		// runs the whole menu past in a tenth of a second.
		s_nLastKey  = nKey;
		s_fRepeatAt = fNow + fRepeat;
		m_InterfaceMgr.OnKeyDown(nKey, 1);
		static int s_nSaidKey = 0;
		if (s_nSaidKey++ < 6)
			VRLog::Msg("VRControls: menu key %d sent, game state %d",
				nKey, (int)m_InterfaceMgr.GetGameState());
	}

	// STRAIGHT TO THE INTERFACE, not through CGameClientShell::OnKeyDown.
	//
	// That function looks like the front door and is not one: it handles
	// F1-F12 and the multiplayer taunts and returns, and never touches
	// m_InterfaceMgr at all - the engine calls the interface's own
	// OnKeyDown separately. Menu keys sent to the shell went nowhere.
	// Measured: holding A on the main menu for twenty seconds left it on
	// the main menu.
	//
	// Either hand: X and A both mean select in a menu, where there is no
	// left/right convention to respect.
	if (nWent & (VRBTN_PRIMARY | VRBTN_TRIGGER))
	{
		VRLog::Msg("VRControls: menu RETURN sent, game state %d",
			(int)m_InterfaceMgr.GetGameState());
		m_InterfaceMgr.OnKeyDown(VK_RETURN, 1);
	}
	if (nWent & VRBTN_SECONDARY)
		m_InterfaceMgr.OnKeyDown(VK_ESCAPE, 1);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::Update()
//
//	PURPOSE:	Handle client updates
//
// ----------------------------------------------------------------------- //

void CGameClientShell::Update()
{
	// VR: the frame boundary. Update() is the only client-shell entry point the
	// engine calls unconditionally every frame - PreUpdate() has early returns.
	VRLog::BeginFrame();

	// Timed clean quit, for unattended shutdown tests. g_pLTClient->Shutdown()
	// is the same call the Quit menu item makes, so this exercises the real
	// teardown path - OnEngineTerm and the DLL unload behind it - rather than
	// the process simply vanishing.
	if (g_vtVRQuitAfter.GetFloat() > 0.0f)
	{
		// VRLog::NowMs() and not GetTime(): GetTime() is GAME time, which does
		// not advance at the main menu - and the main menu is exactly where an
		// unattended shutdown test sits.
		static double s_fFirstSeenMs = -1.0;
		const double fNowMs = VRLog::NowMs();
		if (s_fFirstSeenMs < 0.0) s_fFirstSeenMs = fNowMs;

		if (fNowMs - s_fFirstSeenMs >= g_vtVRQuitAfter.GetFloat() * 1000.0)
		{
			VRLog::Msg("VRQuitAfter %.0f s reached - shutting down cleanly",
				g_vtVRQuitAfter.GetFloat());
			g_vtVRQuitAfter.SetFloat(0.0f);		// do not ask twice
			g_pLTClient->Shutdown();
			return;
		}
	}

#if VR_DEBUG_TOOLS
	// VRDebugOpenVRPage 1: open Options > VR from a menu, once, so the desk
	// can dump the page and check every row fits.
	{
		static VarTrack s_vtOpenVR;
		if (!s_vtOpenVR.IsInitted()) s_vtOpenVR.Init(g_pLTClient, "VRDebugOpenVRPage", LTNULL, 0.0f);
		if (s_vtOpenVR.GetFloat() > 0.0f && m_InterfaceMgr.GetGameState() == GS_FOLDER)
		{
			s_vtOpenVR.SetFloat(0.0f);
			m_InterfaceMgr.SwitchToFolder(FOLDER_ID_VR);
			VRLog::Msg("VRDebugOpenVRPage: switched to Options > VR");
		}
	}
#endif

	// PHOTO MODE ENDS WITH THE MENU, however the menu closed. The toggle lives
	// in the controller's menu branch, which stops running the moment the game
	// resumes - so a menu closed from the keyboard left it on, and the next
	// pause opened an invisible menu (desk test, 24 September).
	if (g_bVRPhotoMode && m_InterfaceMgr.GetGameState() != GS_FOLDER)
	{
		g_bVRPhotoMode = false;
		VRLog::Msg("VRPhotoMode: off (the menu closed)");
	}

	// THE LEFTORIUM (Options > VR, VRLeftorium): the hands swap in
	// VRShared::Poll; the drawing mirrors where it reads VRShared::SwapHands().
	{
		static VarTrack s_vtLeftorium;
		if (!s_vtLeftorium.IsInitted()) s_vtLeftorium.Init(g_pLTClient, "VRLeftorium", LTNULL, 0.0f);
		const bool bLeft = s_vtLeftorium.GetFloat() > 0.0f;
		if (bLeft != VRShared::SwapHands())
			VRLog::Msg("VRLeftorium: %s", bLeft ? "ON - the weapon hand is the LEFT controller" : "off");
		VRShared::SetSwapHands(bLeft);

		// The sticks, separately (Options > VR > Swap sticks): the left-handed
		// player who asked for the Leftorium moves with the left stick.
		static VarTrack s_vtSwapSticks;
		if (!s_vtSwapSticks.IsInitted()) s_vtSwapSticks.Init(g_pLTClient, "VRSwapSticks", LTNULL, 0.0f);
		const bool bSticks = s_vtSwapSticks.GetFloat() > 0.0f;
		if (bSticks != VRShared::SwapSticks())
			VRLog::Msg("VRSwapSticks: %s", bSticks ? "ON - move on the right stick, turn on the left" : "off");
		VRShared::SetSwapSticks(bSticks);
	}

	// THE HOST IS GONE: SAY SO AND CLOSE. Without a host the game draws its
	// side-by-side eyes onto the monitor and nothing reaches a headset. The
	// host closes the game itself when it gives up on a headset, but a host
	// that is ENDED - its window closed, the runtime taking it down - cannot,
	// and one such session left the game playing on, doubled, with the
	// "waiting for your headset" notice already gone. Same Shutdown() as the
	// Quit item. HostGone() is false whenever there is no host id to watch.
	{
		static bool s_bEverLive = false, s_bSaid = false;
		if (VRShared::IsLive()) s_bEverLive = true;
		if (!s_bSaid && VRShared::HostGone())
		{
			s_bSaid = true;
			VRLog::Msg("VRHostGone: the host process has exited %s - telling the player and shutting down",
				s_bEverLive ? "after the headset was live" : "before a headset ever came up");
			MessageBoxA(NULL, s_bEverLive
				? "NOLF VR lost the headset: the VR host program closed.\n\n"
				  "Reconnect the headset (Virtual Desktop, SteamVR or Oculus Link), then "
				  "double-click Play NOLF VR.bat again. Progress since your last save is lost.\n\n"
				  "The game will close now."
				: "NOLF VR could not find a VR headset.\n\n"
				  "Connect the headset first: start streaming in Virtual Desktop, or start "
				  "SteamVR or Oculus Link, and make sure the headset is awake and on your head.\n\n"
				  "Then double-click Play NOLF VR.bat again. The game will close now.",
				"NOLF VR", MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND);
			g_pLTClient->Shutdown();
			return;
		}
	}

	// Auto quick-load, for unattended in-world tests. QuickLoad() itself
	// refuses to run from the menu - it requires GS_PLAYING or a failure
	// screen - so this does what its body does: read SaveGame00 and load the
	// quick save. Waits two seconds after the menu appears, because loading
	// into a folder that is still building itself is not worth the risk.
	if (g_vtVRAutoQuickLoad.GetFloat() > 0.0f &&
		m_InterfaceMgr.GetGameState() == GS_FOLDER)
	{
		static double s_fMenuSinceMs = -1.0;
		const double  fMenuNowMs = VRLog::NowMs();
		if (s_fMenuSinceMs < 0.0) s_fMenuSinceMs = fMenuNowMs;

		if (fMenuNowMs - s_fMenuSinceMs >= 2000.0)
		{
			const float fAutoLoadMode = g_vtVRAutoQuickLoad.GetFloat();
			g_vtVRAutoQuickLoad.SetFloat(0.0f);		// once per run

			// EXACTLY what QuickLoad() does before it loads, and skipping it
			// was a bug: ClearScreenTint() also Terms and Inits the light
			// scale manager, and without that the world loads with the menu's
			// light scale still in force and renders BLACK. Geometry, audio
			// and input all work; nothing is visible. Reported from the desk
			// on 27 August as a black screen with dialog and gunfire still
			// audible.
			//
			// The lesson is the cheap one: when you bypass a function's guard
			// clause to reuse its body, take the whole body.
			ClearScreenTint();

			char szSetting[256];
			memset(szSetting, 0, sizeof(szSetting));
			CWinUtil::WinGetPrivateProfileString(GAME_NAME, "SaveGame00", "",
				szSetting, 256, SAVEGAMEINI_FILENAME);

			char* pWorld = szSetting[0] ? strtok(szSetting, "|") : LTNULL;
			const char* pFile = QUICKSAVE_FILENAME;
			// VRAutoQuickLoad 2: the RELOAD save (the engine's own save at the
			// last mission start, "Reload" in Save1001.ini) instead of the quick
			// save - a desk run in whatever level the player last played, with its
			// loadout, without touching the quick save. 14 September: the
			// lobby quick save holds no gun and the weapon work needed one.
			if (fAutoLoadMode > 1.5f)
			{
				memset(szSetting, 0, sizeof(szSetting));
				CWinUtil::WinGetPrivateProfileString(GAME_NAME, "Reload", "",
					szSetting, 256, SAVEGAMEINI_FILENAME);
				pWorld = szSetting[0] ? strtok(szSetting, "|") : LTNULL;
				pFile = RELOADLEVEL_FILENAME;
			}

			if (pWorld && LoadGame(pWorld, (char*)pFile))
				VRLog::Msg("VRAutoQuickLoad: loading %s from %s", pWorld, pFile);
			else
				VRLog::Msg("VRAutoQuickLoad: FAILED - no save entry, or the load was refused");
		}
	}

	// THE SAME LOAD, ON A TIMER. The tester's trigger for the scrambled skins is the
	// menu's Load, which the desk harness cannot reach - it quick-loads once
	// at startup and has no way to do it twice, which is why that bug has
	// never been measured here. This ends at the same LoadGame call the menu
	// does, so it is the same event and not an imitation of one.
	//
	// Deliberately NOT one-shot like the block above: the interesting part is
	// that every reload scrambles differently, so it has to happen repeatedly
	// inside one run.
	if (g_vtVRAutoReloadSecs.GetFloat() > 0.0f &&
		m_InterfaceMgr.GetGameState() == GS_PLAYING)
	{
		static double s_fLastReloadMs = -1.0;
		const double  fNowMs = VRLog::NowMs();
		if (s_fLastReloadMs < 0.0) s_fLastReloadMs = fNowMs;

		if (fNowMs - s_fLastReloadMs >= g_vtVRAutoReloadSecs.GetFloat() * 1000.0)
		{
			s_fLastReloadMs = fNowMs;

			// Take the whole body, guard clause aside - see the note on the
			// auto-load above. Skipping ClearScreenTint loads the world with
			// the menu's light scale still in force and it renders black.
			ClearScreenTint();

			char szSetting[256];
			memset(szSetting, 0, sizeof(szSetting));
			CWinUtil::WinGetPrivateProfileString(GAME_NAME, "SaveGame00", "",
				szSetting, 256, SAVEGAMEINI_FILENAME);

			char* pWorld = szSetting[0] ? strtok(szSetting, "|") : LTNULL;

			if (pWorld && LoadGame(pWorld, QUICKSAVE_FILENAME))
				VRLog::Msg("VRAutoReload: reloaded %s from the quick save", pWorld);
			else
				VRLog::Msg("VRAutoReload: FAILED - no SaveGame00, or refused");
		}
	}

	// ---- VRWorld2: LOAD A NAMED SECOND WORLD, ONCE ---------------------
	//
	// +runworld puts the harness into ONE world per process, so every
	// measurement this project has ever taken of a level was taken of a
	// FIRST world. That is a structural blind spot, not a gap in coverage:
	// the renderer caches several things on the engine's world POINTER and
	// the allocator hands the same address back for the next level, so a
	// whole class of fault cannot appear until a second load. One of them
	// left every level after the first unlit and survived 103 swept worlds.
	//
	// The quick-load above reaches a second world too, but only the ONE the
	// save happens to hold. This takes a name, so the sweep can ask for any
	// of the 103 as the second world.
	//
	// Deliberately one-shot: it fires once and disarms, so a level that
	// loads slowly cannot start a second load on top of the first.
	{
		// Re-armed on every world entry, so a count of N means N transitions.
		static int s_nW2SeenEntry = -1;
		if (IsInWorld() && g_nVRWorldEntries != s_nW2SeenEntry)
		{
			s_nW2SeenEntry = g_nVRWorldEntries;
			g_fW2Since = -1.0;
		}
	}
#if VR_DEBUG_TOOLS
	if (g_vtVRDebugCmdAt.GetFloat() > 0.0f && IsInWorld()
		&& m_InterfaceMgr.GetGameState() == GS_PLAYING)
	{
		static double s_fCmdSince = -1.0;
		static bool   s_bCmdRun = false;
		const double  fNowC = VRLog::NowMs() / 1000.0;
		if (s_fCmdSince < 0.0) s_fCmdSince = fNowC;
		if (!s_bCmdRun && fNowC - s_fCmdSince >= (double)g_vtVRDebugCmdAt.GetFloat())
		{
			s_bCmdRun = true;
			std::string sCmd = GetConsoleString("VRDebugCmd", "");
			for (size_t k = 0; k < sCmd.size(); ++k) if (sCmd[k] == '~') sCmd[k] = ' ';
			if (!sCmd.empty())
			{
				VRLog::Msg("VRDebugCmd: running '%s'", sCmd.c_str());
				g_pLTClient->RunConsoleString((char*)sCmd.c_str());
			}
		}
	}
	if (g_vtVRDebugFailAt.GetFloat() > 0.0f && IsInWorld()
		&& m_InterfaceMgr.GetGameState() == GS_PLAYING)
	{
		static double s_fFailSince = -1.0;
		static bool   s_bFailed = false;
		const double  fNowF = VRLog::NowMs() / 1000.0;
		if (s_fFailSince < 0.0) s_fFailSince = fNowF;
		if (!s_bFailed && fNowF - s_fFailSince >= (double)g_vtVRDebugFailAt.GetFloat())
		{
			s_bFailed = true;
			VRLog::Msg("VRDebugFailAt: failing the mission, as a death would");
			HandleMissionFailed();
			return;
		}
	}
#endif
	if (g_vtVRWorld2At.GetFloat() > 0.0f && IsInWorld()
		&& m_InterfaceMgr.GetGameState() == GS_PLAYING
		&& g_nW2Fired < (int)g_vtVRWorld2Count.GetFloat())
	{
		// WALL-CLOCK, NOT GAME TIME. GetTime() is game time and it stops
		// whenever the engine pauses - and the engine pauses when the window
		// loses focus, which at the desk is most of the time. The first
		// second-load run of the day waited 170 s for a timer that never
		// moved. VRLog::NowMs() is the clock VRQuitAfter uses.
		const double  fW2Now = VRLog::NowMs() / 1000.0;
		if (g_fW2Since < 0.0) g_fW2Since = fW2Now;
		if (fW2Now - g_fW2Since >= (double)g_vtVRWorld2At.GetFloat())
		{
			g_fW2Since = 1e30;					// not again until re-armed
			++g_nW2Fired;
			if (g_vtVRWorld2Exit.GetFloat() > 0.0f)
			{
				VRLog::Msg("VRWorld2: transition %d of %d - leaving by the"
						   " level's own EXIT (MID_PLAYER_EXITLEVEL), the"
						   " elevator's path", g_nW2Fired,
						   (int)g_vtVRWorld2Count.GetFloat());
				HandleExitLevel(0, LTNULL);
				return;
			}
			char szWorld[256];
			szWorld[0] = 0;
			HCONSOLEVAR hW2 = g_pLTClient->GetConsoleVar("VRWorld2");
			if (hW2)
			{
				const char* pW2 = g_pLTClient->GetVarValueString(hW2);
				if (pW2) { strncpy(szWorld, pW2, sizeof(szWorld) - 1);
						   szWorld[sizeof(szWorld) - 1] = 0; }
			}

			if (szWorld[0])
			{
				// Take the whole body, guard clause aside - see the note on the
				// auto-load above. Skipping ClearScreenTint loads the world with
				// the menu's light scale still in force and it renders black.
				ClearScreenTint();
				VRLog::Msg("VRWorld2: loading %s as the SECOND world of this"
						   " process", szWorld);
				if (!LoadWorld(szWorld))
					VRLog::Msg("VRWorld2: LoadWorld REFUSED %s", szWorld);
			}
			else
			{
				VRLog::Msg("VRWorld2: VRWorld2At is set but VRWorld2 names no"
						   " world - nothing loaded");
			}
		}
	}

	// ---- PAST THE MISSION SUMMARY, the way the player goes ---------------
	//
	// A mission's last exit lands in the summary and briefing folders, and
	// Continue there is StartMission(current) - the load the tester took from the
	// tutorial into Morocco, which built no world at all. The chain used to
	// stop at the summary, so that door was never opened either. Same delay,
	// same counter.
	{
		const bool bAtSummary = (g_vtVRWorld2Exit.GetFloat() > 0.0f
			&& g_vtVRWorld2At.GetFloat() > 0.0f && !IsInWorld()
			&& m_InterfaceMgr.GetGameState() == GS_FOLDER
			&& g_nW2Fired > 0 && g_nW2Fired < (int)g_vtVRWorld2Count.GetFloat());
		if (!bAtSummary) g_fW2FolderSince = -1.0;
		else
		{
			const double fNow = VRLog::NowMs() / 1000.0;
			if (g_fW2FolderSince < 0.0) g_fW2FolderSince = fNow;
			if (fNow - g_fW2FolderSince >= (double)g_vtVRWorld2At.GetFloat()
				&& g_nW2FolderFiredFor != g_nW2Fired)
			{
				g_nW2FolderFiredFor = g_nW2Fired;
				++g_nW2Fired;
				g_fW2FolderSince = -1.0;
				VRLog::Msg("VRWorld2: transition %d of %d - CONTINUING past the mission"
						   " summary into mission %d (StartMission, the briefing's path)",
						   g_nW2Fired, (int)g_vtVRWorld2Count.GetFloat(), GetCurrentMission());
				StartMission(GetCurrentMission());
				return;
			}
		}
	}

	// ---- LOOK AROUND WHILE THE GAME IS PAUSED --------------------------
	//
	// The pause menu draws the world from the last pose each EYE saw it from
	// (docs/PAUSE-MENU-BACKDROP.md), which fixed WHERE it is drawn from and
	// left it frozen - the eye loop does not run in a folder state, so nothing
	// was updating the pose.
	//
	// Turning the INTERFACE camera is the missing half. The position must not
	// change - the simulation is paused and the player has not moved - so the renderer
	// keeps supplying the remembered eye positions and only the rotation comes
	// from here.
	//
	// Same composition as the live path's mode 3: head first, against the
	// body's YAW ONLY. Anything that post-multiplies the full body rotation
	// carries the mouse's pitch into the head and tilts the view when the player looks
	// up, which is a defect this project has already had reported once.

#if VR_DEBUG_TOOLS
	// ---- VRDebugEndCinematic: GIVE THE HARNESS A PLAYABLE STATE --------
	if (g_vtVRDebugEndCinematic.GetFloat() > 0.0f && IsInWorld()
		&& m_bUsingExternalCamera)
	{
		static int    s_nSaidFor = -1;
		static int    s_nLoggedFor = -1;	// the LOG's own guard, kept separate
		static double s_fSeen    = -1.0;
		const double fNowC = g_pLTClient->GetTime();
		if (s_nSaidFor != g_nVRWorldEntries) { s_nSaidFor = g_nVRWorldEntries; s_fSeen = fNowC; }
		if (fNowC - s_fSeen >= (double)g_vtVRDebugEndCinematic.GetFloat())
		{
			// CLEAR THE FLAG, NOT JUST THE CAMERA.
			//
			// TurnOffAlternativeCamera on its own does nothing that lasts, and
			// for weeks it looked like it did. UpdateAlternativeCamera runs
			// every frame, walks the camera FX list, and turns the alternative
			// camera straight back ON for any camera object whose user flags
			// carry USRFLG_CAMERA_LIVE. So this switch ended the cinematic for
			// exactly one frame and the server's flag put it back - which is
			// why every desk capture this project has taken since the switch
			// was written is still in CHASE camera with the view weapon
			// DISABLED, and why the muzzle flash harness reported "PV flash
			// shown 0 times" in run after run. The log said the switch fired,
			// and it had, and it had achieved nothing: the one-shot guard
			// below meant it never noticed it had been overruled.
			//
			// What ends a cinematic for a player is the SERVER clearing that
			// flag. +runworld never reaches the trigger that would, so clear
			// it on the client's own copy instead. Local, and the same thing
			// the replicated flag would say a frame later in real play.
			g_bVRCineEnded = true;		// the part that actually holds
			// The game's own end of a cinematic (TurnOffAlternativeCamera)
			// turns the interface back on; ending it here did not, so no
			// desk capture of a level with an opening camera showed the HUD.
			m_InterfaceMgr.SetDrawInterface(LTTRUE);
			int nCleared = 0;
			CSpecialFXList* pCamsC = m_sfxMgr.GetCameraList();
			if (pCamsC)
			{
				for (int iC = 0; iC < pCamsC->GetSize(); ++iC)
				{
					CCameraFX* pFX = (CCameraFX*)(*pCamsC)[iC];
					if (!pFX) continue;
					HOBJECT hC = pFX->GetServerObj();
					if (!hC) continue;
					uint32 dwUF = 0;
					g_pLTClient->GetObjectUserFlags(hC, &dwUF);
					if (!(dwUF & USRFLG_CAMERA_LIVE)) continue;
					g_pLTClient->SetObjectUserFlags(hC, dwUF & ~USRFLG_CAMERA_LIVE);
					++nCleared;
				}
			}
			TurnOffAlternativeCamera(CT_FULLSCREEN);
			// Retry next frame if it comes back - the guard is on the LOG, not
			// on the action, because an action that can be overruled has to be
			// allowed to answer.
			if (s_nLoggedFor != g_nVRWorldEntries)
			{
				s_nLoggedFor = g_nVRWorldEntries;
				VRLog::Msg("VRDebugEndCinematic: the level's opening camera was"
						   " still live - cleared USRFLG_CAMERA_LIVE on %d camera"
						   " object(s) and ended it, so the view weapon is"
						   " enabled and the camera is first person, as in play",
						   nCleared);
			}
		}
	}
#endif	// VR_DEBUG_TOOLS

	// The whole vehicle under the rider: created and placed here, in the
	// update, not from the model publish, where CreateObject returned nothing.
	if (m_InterfaceMgr.GetGameState() == GS_PLAYING && IsPlayerInWorld()) VRUpdateVehicleBody();

	// ---- VRTele: MOVE THE PLAYER, ONCE, A FEW SECONDS AFTER THE LOAD ----
	//
	// Uses the engine's OWN teleport recipe, copied from
	// CMoveMgr::OnServerForcePos - make the object a point first, move it,
	// then restore the dims pushing anything out of the way. Moving it at full
	// size teleports it clipping into the world.
	//
	// Deliberately client-side and one-shot. It is a DESK instrument for
	// photographing a place the spawn point cannot reach; it is not a
	// movement feature, and the server will correct it the moment anything
	// disagrees - which is fine for a capture and would not be for play.
	if (g_vtVRTele.GetFloat() > 0.0f && IsInWorld()
		&& m_InterfaceMgr.GetGameState() == GS_PLAYING)
	{
		static double s_fFirstSeen = -1.0;
		const double fNow = g_pLTClient->GetTime();
		if (s_fFirstSeen < 0.0) s_fFirstSeen = fNow;

		if (fNow - s_fFirstSeen >= (double)g_vtVRTeleAt.GetFloat())
		{
			g_vtVRTele.SetFloat(0.0f);			// once per run
			// THE MOVE MANAGER'S OBJECT, as OnServerForcePos moves - not the
			// client object. The client simulates movement on CMoveMgr's own
			// object and reports that position to the server; moving the
			// client object reported "landed" and was put back on the next
			// update (desk, 23 September: the camera never left the spawn).
			HLOCALOBJ hMe = m_MoveMgr.GetObject();
			if (!hMe) hMe = g_pLTClient->GetClientObject();
			if (hMe && g_pPhysicsLT)
			{
				LTVector vTo(g_vtVRTeleX.GetFloat(), g_vtVRTeleY.GetFloat(),
							 g_vtVRTeleZ.GetFloat());
				LTVector vCur, vPoint(0.5f, 0.5f, 0.5f), vGot;
				g_pPhysicsLT->GetObjectDims(hMe, &vCur);
				g_pPhysicsLT->SetObjectDims(hMe, &vPoint, 0);
				g_pPhysicsLT->MoveObject(hMe, &vTo, MOVEOBJECT_TELEPORT);
				g_pPhysicsLT->SetObjectDims(hMe, &vCur, SETDIMS_PUSHOBJECTS);
				g_pLTClient->GetObjectPos(hMe, &vGot);
				VRLog::Msg("VRTele: asked for (%.0f %.0f %.0f), landed at"
						   " (%.0f %.0f %.0f)%s",
						   vTo.x, vTo.y, vTo.z, vGot.x, vGot.y, vGot.z,
						   ((vGot - vTo).Mag() > 64.0f)
							   ? "   <- MOVED BY THE WORLD: solid geometry, or"
								 " the server pulled it back" : "");
			}
			else
			{
				VRLog::Msg("VRTele: no client object - nothing moved");
			}
		}
	}

	// ---- VRTour: VISIT A LIST OF PLACES, ON A TIMER ---------------------
	//
	// An unattended audit instrument. A sweep that only LOADS a level proves
	// it loads; it says nothing about what the rooms look like, and the spawn
	// point sees one of them. This walks game/vrtour.txt - one line per stop,
	// "x y z yaw" - teleporting every VRTourEvery seconds and turning to face
	// the given yaw, so a night's run photographs the whole level from inside
	// it. The points come from the LEVEL'S OWN AI nodes and teleport points
	// (tools/tour-points.py), which are places the game itself expects a
	// person to stand.
	//
	// Same teleport recipe as VRTele above: point dims, move, restore.
	// WHY A TOUR STOPPED, SAID ONCE. A tour that ends early reads as a crash
	// in the summary, and most of the time it is a cutscene: the state leaves
	// PLAYING, the hops stop, and nothing says so. Reported here so the triage
	// can tell a conversation from a fault.
	if (g_vtVRTour.GetFloat() > 0.0f && IsInWorld()
		&& m_InterfaceMgr.GetGameState() != GS_PLAYING)
	{
		static int s_nSaidState = -1;
		const int nNow = (int)m_InterfaceMgr.GetGameState();
		if (nNow != s_nSaidState)
		{
			s_nSaidState = nNow;
			VRLog::Msg("VRTour: WAITING - the game state is %d, not PLAYING"
					   " (4 is a dialogue scene, 7 a folder, 9 a failure)", nNow);
		}
	}

	if (g_vtVRTour.GetFloat() > 0.0f && IsInWorld()
		&& m_InterfaceMgr.GetGameState() == GS_PLAYING)
	{
		struct TourStop { float x, y, z, yaw; };
		static TourStop s_Stops[128];
		static int   s_nStops = -1;			// -1 = not read yet
		static int   s_nAt = 0;
		static double s_fNextAt = -1.0;
		const double fNow = g_pLTClient->GetTime();

		if (s_nStops < 0)
		{
			s_nStops = 0;
			FILE* fp = fopen("vrtour.txt", "rt");
			if (fp)
			{
				char szLine[256];
				while (s_nStops < 128 && fgets(szLine, sizeof szLine, fp))
				{
					if (szLine[0] == '#' || szLine[0] == '\n') continue;
					float x, y, z, yaw = 0.0f;
					const int n = sscanf(szLine, "%f %f %f %f", &x, &y, &z, &yaw);
					if (n >= 3)
					{
						s_Stops[s_nStops].x = x; s_Stops[s_nStops].y = y;
						s_Stops[s_nStops].z = z; s_Stops[s_nStops].yaw = yaw;
						++s_nStops;
					}
				}
				fclose(fp);
			}
			VRLog::Msg("VRTour: %d stops read from vrtour.txt, %.1f s each%s",
					   s_nStops, g_vtVRTourEvery.GetFloat(),
					   s_nStops ? "" : "  <- NO FILE OR NO POINTS, the tour does nothing");
			s_fNextAt = fNow + 2.0;			// let the world settle first
		}

		if (s_nStops > 0 && s_nAt < s_nStops && fNow >= s_fNextAt)
		{
			const TourStop& st = s_Stops[s_nAt];
			HLOCALOBJ hMe = g_pLTClient->GetClientObject();
			if (hMe && g_pPhysicsLT)
			{
				LTVector vTo(st.x, st.y, st.z);
				LTVector vCur, vPoint(0.5f, 0.5f, 0.5f), vGot;
				g_pPhysicsLT->GetObjectDims(hMe, &vCur);
				g_pPhysicsLT->SetObjectDims(hMe, &vPoint, 0);
				g_pPhysicsLT->MoveObject(hMe, &vTo, MOVEOBJECT_TELEPORT);
				g_pPhysicsLT->SetObjectDims(hMe, &vCur, SETDIMS_PUSHOBJECTS);
				g_pLTClient->GetObjectPos(hMe, &vGot);
				SetYaw(DEG2RAD(st.yaw));
				SetPlayerYaw(DEG2RAD(st.yaw));
				const float fOff = (vGot - vTo).Mag();
				VRLog::Msg("VRTour: stop %d of %d - asked (%.0f %.0f %.0f) yaw %.0f,"
						   " landed (%.0f %.0f %.0f)%s",
						   s_nAt + 1, s_nStops, st.x, st.y, st.z, st.yaw,
						   vGot.x, vGot.y, vGot.z,
						   (fOff > 64.0f) ? "   <- MOVED BY THE WORLD (solid, or the server pulled it back)" : "");
			}
			// AND HIDE THE BODY. A client-side teleport moves the player
			// object out from under the camera for a moment, so every tour
			// picture had Cate standing in the middle of it - a third of the
			// frame spent on the one thing the tour is not auditing. She is
			// only hidden while the tour is running.
			if (hMe)
			{
				const uint32 dwF = g_pLTClient->GetObjectFlags(hMe);
				g_pLTClient->SetObjectFlags(hMe, dwF & ~FLAG_VISIBLE);
			}
			++s_nAt;
			s_fNextAt = fNow + (double)g_vtVRTourEvery.GetFloat();
			if (s_nAt >= s_nStops)
				VRLog::Msg("VRTour: COMPLETE - %d stops visited", s_nStops);
		}
	}

#if VR_DEBUG_TOOLS
	// ---- VR_DEBUG_TOOLS: keep the testing switches applied --------------
	//
	// A cheat is a one-shot in this engine: typing mpimyourfather flips god
	// mode and the next level starts without it. A menu TOGGLE has to mean
	// "stay this way", so the switches are re-applied whenever one changes
	// and again on entering a world - which is the case that matters when
	// skipping through levels.
	//
	// Only on a change, never every frame: each of these sends a message to
	// the server, and god mode announces itself in the message box.
	//
	// WHAT THE SERVER ACTUALLY DOES WITH THE FLAG, read from
	// GameServerShell.cpp, because the first version of this assumed and
	// killed a man in the opening cutscene:
	//
	//   CHEAT_GOD       ToggleGodMode()        - the flag is IGNORED, it flips
	//   CHEAT_CLIP      SetSpectatorMode(flag) - honoured
	//   CHEAT_REMOVEAI  HandleCheatRemoveAI()  - the flag is IGNORED, every
	//                                            AI dies, whichever way it is
	//                                            called
	//
	// So on world entry the engine's state is taken as OFF for everything
	// (a new CPlayerObj is constructed with m_bGodMode = LTFALSE, and the AI
	// are all alive), and only the switches that are ON are sent. Sending
	// "off" to a toggle would turn it on; sending "off" to the AI remover
	// would empty the level. And the AI switch is never sent off at all.
	{
		static int s_nGod = 0, s_nClip = 0, s_nNoAI = 0, s_nMis = 0, s_nArs = 0;
		static int s_nPos = 0;
		static int s_nSeenEntry = -1;
		static int    s_nArsSends = 0;		// arsenal sends this world
		static double s_fArsNext  = 0.0;	// when the next one may go
		const bool bIn = (IsInWorld() != LTFALSE);
		if (bIn && g_nVRWorldEntries != s_nSeenEntry)
		{
			// A new world: the engine has forgotten all of it - and its
			// forgotten state is OFF, so that is what we remember too.
			s_nSeenEntry = g_nVRWorldEntries;
			s_nGod = s_nClip = s_nNoAI = s_nMis = s_nArs = s_nPos = 0;
			s_nArsSends = 0; s_fArsNext = 0.0;
		}

		if (bIn && g_pCheatMgr)
		{
			const int nGod = (g_vtVRDebugGod.GetFloat()      > 0.0f) ? 1 : 0;
			const int nClip= (g_vtVRDebugClip.GetFloat()     > 0.0f) ? 1 : 0;
			const int nAI  = (g_vtVRDebugNoAI.GetFloat()     > 0.0f) ? 1 : 0;
			const int nMis = (g_vtVRDebugMissions.GetFloat() > 0.0f) ? 1 : 0;
			const int nArs = (g_vtVRDebugArsenal.GetFloat()  > 0.0f) ? 1 : 0;
			const int nPos = (g_vtVRDebugPos.GetFloat()      > 0.0f) ? 1 : 0;

			if (nGod != s_nGod || nClip != s_nClip || nAI != s_nNoAI
				|| nMis != s_nMis || nArs != s_nArs || nPos != s_nPos)
				VRLog::Msg("VRDebug: god %d, clip %d, no-AI %d, missions %d, arsenal %d"
						   " (applying what changed)", nGod, nClip, nAI, nMis, nArs);
			// God is a toggle on the server: one send per CHANGE flips it the
			// way we want, because we only ever send from a known state.
			if (nGod != s_nGod)   { s_nGod = nGod;   g_pCheatMgr->VRGod(nGod ? LTTRUE : LTFALSE); }
			if (nClip != s_nClip) { s_nClip = nClip; g_pCheatMgr->VRClip(nClip ? LTTRUE : LTFALSE); }
			// The AI remover cannot be undone and ignores its flag, so it is
			// sent ONCE, on the rising edge, and never sent off. Turning the
			// switch off just stops it firing on the next level.
			if (nAI != s_nNoAI)   { s_nNoAI = nAI;   if (nAI) g_pCheatMgr->VRNoAI(LTTRUE); }
			if (nPos != s_nPos)   { s_nPos = nPos;   g_pCheatMgr->VRShowPos(nPos ? LTTRUE : LTFALSE); }
			if (nMis != s_nMis)   { s_nMis = nMis;   g_pCheatMgr->VRMissions(nMis ? LTTRUE : LTFALSE); }
			// The arsenal is an ACTION, not a state - there is no "take the
			// guns away again". Handed out once per world while it is on, so
			// a skipped-to level still starts armed.
			//
			// AND IT HAS TO BE RETRIED, because one send lands too early. The
			// server's CPlayerAttachments::HandleCheatFullWeapon does nothing
			// at all when the player has no default attachment weapon yet,
			// and returns no error - so the single send at two seconds into
			// the world left the player holding only fists for the whole
			// level. On 11 September a desk harness fired for ten seconds,
			// photographed no muzzle flash, and the weapon it was firing was
			// 'fisty_cuffs'. Counted, retried and checked against what the
			// player actually holds.
			// ALWAYS AT LEAST ONE SEND on the rising edge, whatever the
			// has-a-gun test says. The first version skipped the send
			// entirely when the test read true, and it read true for a
			// pistol with no bullets - so the cheat was never sent at all
			// and the log said nothing either way.
			if (nArs != s_nArs)
			{
				s_nArs = nArs; s_nArsSends = 0;
				// START THE CLOCK WITH THE FIRST SEND, or the retry below
				// fires in the same frame: it compares against s_fArsNext,
				// and a zero there is always in the past. Harmless - the
				// cheat is idempotent - but it spent one of six attempts
				// before the server had a chance to answer the first.
				s_fArsNext = g_pLTClient->GetTime() + 1.5;
				if (nArs) { ++s_nArsSends; g_pCheatMgr->VRArsenal(); }
			}
			// s_nArsSends is set to 90 once a gun is in hand and stays there,
			// so the < 6 test short-circuits VRHasAnyGun - which walks all 130
			// weapons - out of the common case. The == 6 case is parked the
			// same way, or it would re-scan every frame for the whole level.
			if (nArs && s_nArsSends < 6 && !VRHasAnyGun())
			{
				const double fNowArs = g_pLTClient->GetTime();
				if (fNowArs >= s_fArsNext)
				{
					s_fArsNext = fNowArs + 1.5;
					++s_nArsSends;
					g_pCheatMgr->VRArsenal();
					if (s_nArsSends == 6)
					{
						VRLog::Msg("VRDebug: the arsenal cheat has been sent six times"
								   " and the player still holds no gun");
						s_nArsSends = 91;		// given up; stop scanning
					}
				}
			}
			else if (nArs && s_nArsSends && s_nArsSends < 90 && VRHasAnyGun())
			{
				VRLog::Msg("VRDebug: the arsenal arrived after %d send%s",
						   s_nArsSends, (s_nArsSends == 1) ? "" : "s");
				s_nArsSends = 90;			// said once
			}
		}
	}
#endif

	// ---- WHAT SETTINGS IS THE GAME ACTUALLY RUNNING WITH? ---------------
	//
	// Said once per world, because a setting that does not EXIST reads as
	// zero and the feature it gates is silently absent. CGameSettings::
	// GetBoolVar returns LTFALSE when GetConsoleVar finds nothing, so a
	// staged install whose config never visited the Options pages runs with
	// gore off, polygrids off, the sky off - and nothing anywhere says so.
	//
	// the waterfall did not move and there was no
	// blood splatter on walls. PolyGridFX skips its whole wave update when PolyGrids is
	// off; CWeaponFX::CreateSurfaceSpecificFX returns early when Gore is
	// off. Both read as off because both were absent from autoexec.cfg.
	//
	// A MISSING VAR AND A VAR SET TO ZERO ARE DIFFERENT PROBLEMS, so this
	// prints which it is.
	{
		static int s_nSaidFor = -1;
		if (IsInWorld() && s_nSaidFor != g_nVRWorldEntries)
		{
			s_nSaidFor = g_nVRWorldEntries;
			// EVERY NAME HERE IS ONE THE GAME ACTUALLY READS.
			//
			// The first version of this list carried four that it does not.
			// PVWeapons, DynamicLightSetting and SoundChannels are declared in
			// GameSettings.h and never called from anywhere in the client, and
			// ShadowsEnable is not a NOLF console variable at all - the
			// shadow switch is DrawShadows. All four reported ABSENT on every
			// world entry, which reads as four missing features and is four
			// false alarms. Replaced with the ones that gate something real.
			static const char* const kNames[] = {
				"Gore", "PolyGrids", "DrawSky", "ScreenFlash",
				"TextureDetail", "ModelLOD", "DynamicLight",
				"ModelFullbrite", "CloudMapLight", "SpecialFX",
				"PerformanceLevel", "BulletHoles", "MaxModelShadows",
				"LightMap", "EnvMapEnable", "DetailTextures", "SoundEnable",
				"musicenable", "DrawShadows", "MuzzleLight", "Tracers",
				"ShellCasings", "EnableWeatherFX", "ImpactFXLevel",
				"DebrisFXLevel", "Difficulty",
			};
			char szLine[512]; szLine[0] = 0;
			for (int i = 0; i < (int)(sizeof(kNames)/sizeof(kNames[0])); ++i)
			{
				HCONSOLEVAR hV = g_pLTClient->GetConsoleVar((char*)kNames[i]);
				char szOne[64];
				if (hV) sprintf(szOne, "%s=%.4g ", kNames[i],
								g_pLTClient->GetVarValueFloat(hV));
				else    sprintf(szOne, "%s=ABSENT ", kNames[i]);
				if (strlen(szLine) + strlen(szOne) < sizeof(szLine) - 1)
					strcat(szLine, szOne);
			}
			VRLog::Msg("VRSettings: %s", szLine);
			VRLog::Msg("VRSettings: ABSENT means the console var does not exist,"
					   " and every one of those reads as ZERO to the game -"
					   " the feature it gates is off and nothing else says so");

			// HEAD BOB AND WEAPON SWAY START OFF IN A HEADSET. Both move the
			// view or the gun on their own, which feels wrong in VR. Set ONCE
			// per config, with a flag of our own, so a player who turns them
			// back on (Options > Game, or Options > VR) keeps them on.
			if ((VRShared::IsLive() || GetConsoleInt("VRStereo", 0) > 0)
				&& GetConsoleInt("VRComfortDefaults", 0) == 0)
			{
				WriteConsoleFloat("HeadBob", 0.0f);
				WriteConsoleFloat("WeaponSway", 0.0f);
				WriteConsoleInt("VRComfortDefaults", 1);
				g_pLTClient->WriteConfigFile("autoexec.cfg");
				VRLog::Msg("VRComfortDefaults: head bob and weapon sway set to 0 (once per config)");
			}
		}
	}

	// Pull the latest head pose from the host. Cheap, and safe when no host is
	// running - it retries attaching once a second and otherwise does nothing.
	VRShared::Poll();

	// ---- THE EARS FOLLOW THE HEAD --------------------------------------
	//
	// In normal play nobody calls SetListener, so the engine's default holds:
	// "listener in client" - it takes the position AND ORIENTATION from the
	// player object. The player object carries the BODY yaw. The head does
	// not reach it, because the head rotation is applied inside
	// RenderWorldEyes and put back in the __finally.
	//
	// So every 3D sound in the game was panned as though the head were
	// welded facing the body's forward: turn to look at a guard and his
	// footsteps stay off to the side. Everything else about him - the view,
	// the aim, the gun - follows the head, and only the sound does not.
	//
	// The listener is therefore set explicitly, from the same composition
	// the live view uses (rBody * rHead at VRQuatHead 1), at the same moment
	// the pose is freshest. LTFALSE means "these are the values", so the
	// engine stops taking them from the player object.
	//
	// NOT while a cinematic camera owns the listener - that path sets its own
	// above, and fighting it would move the ears to the wrong place during
	// every cutscene. Put back to the client's own listener the moment VR
	// stops driving it, so a flat run is unchanged. +VRHeadListener 0 reverts.
	{
		static bool s_bWeSetIt = false;
		const bool bWant = (g_vtVRHeadListener.GetFloat() > 0.0f)
						&& VRShared::IsLive()
						&& IsInWorld()
						&& !IsUsingExternalCamera()
						&& m_hCamera
						// PLAYING and DIALOGUE both draw a head-tracked world.
						// Gating on PLAYING alone handed the ears back to the
						// body for the length of every conversation, which is
						// a large share of this game.
						&& (m_InterfaceMgr.GetGameState() == GS_PLAYING
						 || m_InterfaceMgr.GetGameState() == GS_DIALOGUE);
		if (bWant)
		{
			const VRSharedState& ls = VRShared::State();
			LTRotation rHead;
			rHead.Init(-ls.fHeadQuatX, -ls.fHeadQuatY,
					   ls.fHeadQuatZ, ls.fHeadQuatW);
			LTRotation rBody;
			g_pLTClient->GetObjectRotation(m_hCamera, &rBody);
			LTRotation rEars = rBody * rHead;
			LTVector vEars;
			g_pLTClient->GetObjectPos(m_hCamera, &vEars);
			g_pLTClient->SetListener(LTFALSE, &vEars, &rEars);
			if (!s_bWeSetIt)
			{
				s_bWeSetIt = true;
				VRLog::Msg("VRHeadListener: the listener now follows the HEAD"
						   " (was the player object's body yaw)");
			}
		}
		else if (s_bWeSetIt)
		{
			s_bWeSetIt = false;
			g_pLTClient->SetListener(LTTRUE, LTNULL, LTNULL);
			// WHICH gate dropped? A desk run showed the listener taken for
			// exactly one frame and then handed back for the rest of the run,
			// which would leave the ears on the body in real play too - so
			// the reason has to be in the log, not inferred.
			VRLog::Msg("VRHeadListener: listener handed back to the client object"
					   "  [cvar %.0f live %d inworld %d extcam %d cam %d state %d]",
					   g_vtVRHeadListener.GetFloat(), (int)VRShared::IsLive(),
					   (int)IsInWorld(), (int)IsUsingExternalCamera(),
					   m_hCamera ? 1 : 0, (int)m_InterfaceMgr.GetGameState());
		}
	}

	// Controllers, straight after the poll that refreshed them.
	VRUpdateControllerInput();
	VRTypeWhenUnfocused();

	// ---- VRDebugQuickSave: MAKE THE QUICK SAVE THE HARNESS WILL LOAD -------
	//
	// Asked for in when something needs testing in
	// a specific level, make the quick save that level, and ideally the specific
	// spot, because it would speed up testing greatly.
	//
	// It is also the cheapest thing on the list. Every desk capture
	// taken without -World loads game\Save\Quick.sav, so whatever that file
	// holds is where every test begins. Today it held the HQ lobby, which is
	// why no capture could find the waterfall and why reaching a level with a
	// gun in it needed the arsenal cheat.
	//
	// A cvar rather than a keystroke, because a bind has to be guessed at,
	// focused and delivered, and this has to work from a script. It fires ONCE:
	// a save that repeated would overwrite the good one with wherever the
	// player drifted to afterwards.
	if (g_vtVRDebugQuickSave.GetFloat() > 0.0f && IsInWorld())
	{
		static bool s_bSavedOnce = false;
		if (!s_bSavedOnce && g_pLTClient->GetTime() >= g_vtVRDebugQuickSave.GetFloat())
		{
			s_bSavedOnce = true;
			const LTBOOL bOk = QuickSave();
			VRLog::Msg("VRDebugQuickSave: QuickSave() at %.1fs returned %s",
				g_pLTClient->GetTime(), bOk ? "TRUE" : "FALSE");
		}
	}
#if VR_DEBUG_TOOLS
	// After, deliberately: the line above may have returned early and released
	// everything, and the desk trigger has to survive that.
	VRDebugFireDrive();
#endif

	// THE PAUSE-LOOK, AFTER THE POLL. It used to run before VRShared::Poll,
	// so the interface camera carried the PREVIOUS tick's head pose while the
	// frame marker painted this tick's - one frame of lag the host then
	// reprojected from the wrong pose. In the headset the world felt as if it
	// was lagging behind head movement behind the pause menu.
	// NOT ON A CARD FOLDER. The briefing, objectives, inventory and loadout
	// cards are presented as a flat panel with no world behind them, and
	// their 3D pieces (the orange sheet, the flower) are drawn through this
	// camera; turning it with the head slid those pieces under the card's
	// text. moving the head moved part of the
	// layers of the image, and nothing should move with headset movement there.
	if (g_vtVRPauseLook.GetFloat() > 0.0f && m_hInterfaceCamera && IsInWorld()
		&& m_InterfaceMgr.GetGameState() == GS_FOLDER
		&& !VRCardScene())
	{
		const VRSharedState& hs = VRShared::State();
		// Same two conditions the live path trusts the quaternion under.
		if (VRShared::IsLive() && g_vtVRQuatHead.GetFloat() > 0.0f)
		{
			LTRotation rHead;
			rHead.Init(-hs.fHeadQuatX, -hs.fHeadQuatY,
					   hs.fHeadQuatZ, hs.fHeadQuatW);
			LTRotation rBodyYaw;
			g_pLTClient->SetupEuler(&rBodyYaw, 0.0f, m_fYaw, 0.0f);
			// THE SAME PRODUCT THE LIVE PATH USES, chosen by the same mode.
			//
			// This was hard-wired to mode 3's order, rHead * rBodyYaw, while
			// the live view runs at VRQuatHead 1 - rSavedRot * rHead, body
			// first. The two orders put the head's pitch and roll about
			// different axes: one is the body's frame, the other the world's,
			// and they differ by the body's yaw. Facing the other way, world
			// frame pitch and roll come out reversed - which is precisely the
			// report: a roll left rolled right, and pitching the head up pitched
			// the view down. The pause menu was never inverted; it was in the wrong
			// frame, by exactly the angle the player happened to be standing at.
			const int nPauseMode = (int)g_vtVRQuatHead.GetFloat();
			LTRotation rUse = (nPauseMode >= 2) ? (rHead * rBodyYaw)
											   : (rBodyYaw * rHead);
			g_pLTClient->SetObjectRotation(m_hInterfaceCamera, &rUse);

			static int s_nSaidPauseLook = 0;
			if (s_nSaidPauseLook < 2)
			{
				++s_nSaidPauseLook;
				VRLog::Msg("VRPauseLook: turning the interface camera with the"
						   " head (yaw %.1f pitch %.1f)",
						   hs.fHeadYawDeg, hs.fHeadPitchDeg);
			}
		}
		s_bVRIfaceCamTurned = true;
	}
	// AND SQUARE IT UP AGAIN WHEN THE PAUSE-LOOK STOPS. The interface camera
	// is shared: the main menu, every full-card folder and the loading screen
	// are drawn through it and their art is placed for a camera facing
	// straight ahead. Nothing put it back, so after a pause menu the next card
	// was drawn through a camera still turned wherever the head had been -
	// 41 degrees in one headset log - and the backdrop no longer covered the
	// view: a black block where the logo was, Cate out of frame, a black
	// Mission Status with only the flower showing. None of it at the desk,
	// where nobody pauses first.
	else if (s_bVRIfaceCamTurned && m_hInterfaceCamera)
	{
		LTRotation rStraight;
		g_pLTClient->SetupEuler(&rStraight, 0.0f, 0.0f, 0.0f);
		g_pLTClient->SetObjectRotation(m_hInterfaceCamera, &rStraight);
		s_bVRIfaceCamTurned = false;
		VRLog::Msg("VRPauseLook: interface camera squared up again (pause-look off)");
	}

	// The model list the renderer cannot see for itself used to be published
	// HERE - before this frame's movement, camera and weapon updates - so the
	// gun went out one frame behind the world it was drawn into. In the headset
	// the gun model was a bit jittery and stuttered on moving left and
	// right. It is published from RenderCamera now, after UpdateWeaponModel,
	// once per Update tick.
	++m_nVRUpdateTick;

	// One pass over everything the engine has near the player.
	if (g_vtVRModelDump.GetFloat() > 0.0f
		&& m_InterfaceMgr.GetGameState() == GS_PLAYING)
	{
		static int s_nDumped = 0;
		if (s_nDumped++ == 120)		// a couple of seconds in, so the world is up
		{
			HLOCALOBJ hPlayer = g_pLTClient->GetClientObject();
			LTVector vPos(0.0f, 0.0f, 0.0f);
			if (hPlayer) g_pLTClient->GetObjectPos(hPlayer, &vPos);

			HLOCALOBJ objs[512];
			uint32 nOut = 0, nFound = 0;
			g_pLTClient->FindObjectsInSphere(&vPos, 5000.0f, objs, 512, &nOut, &nFound);
			VRLog::Msg("VRModelDump: player at (%.0f %.0f %.0f), %u objects returned"
				" of %u within 5000 units", vPos.x, vPos.y, vPos.z, nOut, nFound);

			int nModels = 0;
			for (uint32 i = 0; i < nOut; ++i)
			{
				if (g_pLTClient->GetObjectType(objs[i]) != OT_MODEL) continue;
				++nModels;

				LTVector p; g_pLTClient->GetObjectPos(objs[i], &p);

				// How many skeletal nodes the engine is posing for it, and what the
				// root is called. That count is what the renderer would have to skin
				// against - and the engine computes every one of those transforms
				// for us already, through ILTModel::GetNodeTransform. The hard half
				// of drawing a character is therefore already done by the engine.
				int nNodes = 0;
				char szRoot[64]; szRoot[0] = 0;
				HMODELNODE hNode = INVALID_MODEL_NODE, hNext = INVALID_MODEL_NODE;
				while (g_pLTClient->GetNextModelNode(objs[i], hNode, &hNext) == LT_OK
					   && nNodes < 512)
				{
					if (!nNodes)
						g_pLTClient->GetModelNodeName(objs[i], hNext, szRoot, sizeof szRoot);
					hNode = hNext;
					++nNodes;
				}
				if (nModels <= 40)
					VRLog::Msg("  model %2d: at (%7.0f %7.0f %7.0f)  %3d nodes  root '%s'",
						nModels, p.x, p.y, p.z, nNodes, szRoot);
			}
			VRLog::Msg("VRModelDump: %d of %u objects are OT_MODEL", nModels, nOut);
		}
	}

	// Tell the host our true render surface size. LithTech's window client is
	// 16px larger in each axis, so cropping the client splits at the wrong
	// column and hands each eye a sliver of the other. Published from here
	// rather than the world-render path so it is known at the menu too.
	{
		uint32 nSurfW = 0, nSurfH = 0;
		g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &nSurfW, &nSurfH);
		if (nSurfW > 0 && nSurfH > 0)
		{
			VRShared::PublishScreenSize(nSurfW, nSurfH);
		}

		// Menus, loading screens and cutscenes are drawn once across the whole
		// window rather than in stereo, so the host must not split them.
		// FLAT PANEL WHEN THE WORLD IS NOT DRAWN - not when the state is not
		// GS_PLAYING. Those are different sets, and the difference is every
		// conversation in the game.
		//
		// nInMenu tells the host to present a world-locked quad instead of a
		// stereo projection layer. Keyed off GS_PLAYING it fired for
		// GS_DIALOGUE, which is its own state - so the moment the player spoke to
		// anyone in HQ the picture dropped to a flat screen, while UpdatePlaying
		// went on rendering a perfectly good stereo pair behind it that nothing
		// looked at.
		//
		// UpdatePlaying is what draws the world, and its gate is below: the
		// player being in the world, not the game state. So ask exactly that
		// question. Dialogue, the in-game menu and a pause all keep the world -
		// and now keep the stereo. Loading, the folder menu, the intro movies
		// and a failure screen have no world and still get the flat panel.
		// A FULL-CARD FOLDER IS A MENU EVEN WITH A WORLD LOADED. Mission
		// status, summary, briefing, objectives, inventory, awards: retail
		// draws these through the interface camera as the orange card and
		// never shows the level. Only the pause family (main, options, load,
		// save) keeps the world behind it. The 12 September screenshot batch had
		// "MISSION STATUS" as bare text over the Morocco street.
		const bool bCardFolder = VRCardScene();
		VRShared::PublishInMenu(!(IsPlayerInWorld()
								  && m_InterfaceMgr.GetGameState() != GS_UNDEFINED)
								|| bCardFolder);

		// AND, SEPARATELY, WHETHER A FOLDER IS UP.
		//
		// nInMenu above is about the WORLD - a pause keeps the world and must
		// keep its stereo pair. The 2D layer is a different question: a folder's
		// art is one flat layout, and drawing it per eye puts the same olive
		// card in two places. That is the pause menu's double vision.
		//
		// Sent straight to the renderer rather than through the shared block:
		// the host has no use for it, and the block is shared with another
		// process. Absent under retail d3d.ren, which is correct.
		{
			typedef void (__cdecl *VRFolderFn)(int);
			static VRFolderFn s_pfnFold = NULL;
			static HMODULE s_hRenSeenFold = (HMODULE)(uintptr_t)1;
			{
				HMODULE hR = GetModuleHandleA("d3dstub.ren");
				if (hR != s_hRenSeenFold)		// the one that crashed the intro's end
				{
					s_hRenSeenFold = hR;
					s_pfnFold = hR ? (VRFolderFn)GetProcAddress(hR, "R3D_PublishFolder2D") : NULL;
					VRLog::Msg("VRFolder2D: entry %s", s_pfnFold ? "resolved" : "missing");
				}
			}
			// WHICH folder, not just whether. A menu-to-submenu change is a
			// folder change - the old folder's textures are freed and their
			// addresses reused - but both states are GS_FOLDER, so a boolean
			// cannot see it. That transition is exactly the one reported from the headset:
			// on making a selection, certain textures get removed and load
			// in. Send the folder ID, offset by one so 0 keeps meaning "no
			// folder", and let the renderer notice any change.
			if (s_pfnFold)
			{
				// THE SPLASH AND THE LOADING SCREEN ARE MENUS TOO, for the 2D
				// layer's purposes: a flat full-screen picture that wants the
				// panel's band fit. Before this only GS_FOLDER counted, so the
				// splash - drawn in the first seconds, before the host is live
				// enough for the shared block's in-menu flag to reach the
				// renderer - took the world's fit, stretched and then cropped
				// by the band (headset testing, 13 and 14 September: splash cropped
				// and zoomed). 999 is outside every folder id.
				const int nState = (int)m_InterfaceMgr.GetGameState();
				const int nFold =
					(nState == GS_FOLDER) ? (int)m_InterfaceMgr.GetCurrentFolder() + 1
					: (nState == GS_SPLASHSCREEN || nState == GS_LOADINGLEVEL) ? 999 : 0;
				s_pfnFold(nFold);
			}
			// AND WHETHER IT IS A FULL-CARD ONE. Same resolve pattern; the
			// loading screen counts too - it is drawn through the interface
			// camera by its own thread, and a world may still be loaded.
			{
				static VRFolderFn s_pfnCard = NULL;
				static HMODULE s_hRenSeenCard = (HMODULE)(uintptr_t)1;
				HMODULE hR2 = GetModuleHandleA("d3dstub.ren");
				if (hR2 != s_hRenSeenCard)
				{
					s_hRenSeenCard = hR2;
					s_pfnCard = hR2 ? (VRFolderFn)GetProcAddress(hR2, "R3D_PublishInterfaceOnly") : NULL;
					VRLog::Msg("VRFolder2D: interface-only entry %s", s_pfnCard ? "resolved" : "missing");
				}
				if (s_pfnCard)
					s_pfnCard((bCardFolder
							   || m_InterfaceMgr.GetGameState() == GS_LOADINGLEVEL) ? 1 : 0);
			}
		}

		// When the head goes through the mouse path the rendered camera already
		// carries the head rotation, so the host must submit head-locked and
		// let the runtime reproject nothing. Published from here, not from the
		// render path, because it is a property of how the camera was built and
		// has to be true even on the frames that draw a menu.
		// Either path wants head-locked submission: head-as-mouse because the
		// camera already carries the head, and VRHeadLocked because the point
		// is to stop the compositor reprojecting at all.
		VRShared::PublishHeadLocked(g_vtVRHeadAsMouse.GetFloat() > 0.0f
								 || g_vtVRHeadLocked.GetFloat() > 0.0f);

		// The body yaw, every frame. Cheap, and the host needs it continuously
		// rather than on change - it is the frame the image was rendered in.
		VRShared::PublishBodyYaw(m_fYaw, (int)g_vtVRYawSpace.GetFloat());

		// NOTE: the intro logo sequence is a movie state. Leaving the window
		// alone until the game settles avoids interrupting it.

		// Make the window client EXACTLY the render surface, borderless, at the
		// top-left of the screen.
		//
		// The engine sizes its client 16px larger than the render surface and
		// stretches to fill, and Windows may scale on top of that - measured
		// ratios of 1.008 and 1.507 for the same 1920x1080 render. Every one of
		// those is an interpolation the headset then upscales again. Matching
		// the client to the surface makes the capture pixel-exact, and dropping
		// the border lets the render resolution go all the way to the monitor
		// size instead of stopping short to leave room for a title bar.
		// Deferred until the game reaches the main menu. Doing it on the first
		// frame lands in the middle of the intro splash sequence and stops it
		// advancing - the game keeps rendering, but never gets past the logos.
		const GameState eState = m_InterfaceMgr.GetGameState();
		const bool bSafeToResize = (eState == GS_FOLDER || eState == GS_PLAYING ||
									eState == GS_MENU   || eState == GS_PAUSED);

		static bool s_bSizedWindow = false;
		if (!s_bSizedWindow && bSafeToResize && g_hMainWnd && nSurfW > 0 &&
			g_vtVRBorderless.GetFloat() > 0.0f)
		{
			// Applied unconditionally. Matching sizes does not mean there is no
			// title bar - the client can already be the right size while the
			// frame pushes it partly off-screen, which crops the capture.
			LONG style = GetWindowLong(g_hMainWnd, GWL_STYLE);
			style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
			style |= WS_POPUP;
			SetWindowLong(g_hMainWnd, GWL_STYLE, style);

			// Where to put it. The primary display is the safe default; a
			// faster one is only used when the host is live, so the flat game
			// can never end up on a display the player is not looking at.
			int nWinX = 0, nWinY = 0;
			if (g_vtVRWindowMonitor.GetFloat() > 0.0f && VRShared::IsLive())
			{
				DEVMODEA dmPrim;
				memset(&dmPrim, 0, sizeof(dmPrim));
				dmPrim.dmSize = sizeof(dmPrim);
				EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &dmPrim);

				VRMonPick pick;
				memset(&pick, 0, sizeof(pick));
				pick.nWantH   = (int)nSurfH;
				pick.dwBestHz = dmPrim.dmDisplayFrequency;
				EnumDisplayMonitors(NULL, NULL, VRMonProc, (LPARAM)&pick);

				if (pick.bFound)
				{
					nWinX = pick.nX;
					nWinY = pick.nY;
					VRLog::Msg("window: primary is %lu Hz - moving to %s (%dx%d at %lu Hz, origin %d,%d)",
						dmPrim.dmDisplayFrequency, pick.szName, pick.nW, pick.nH,
						pick.dwBestHz, pick.nX, pick.nY);
					if ((int)nSurfW > pick.nW)
						VRLog::Msg("  WARNING: %u wide does not fit a %d wide display - it will span onto the next one",
							nSurfW, pick.nW);
				}
				else
				{
					VRLog::Msg("window: nothing attached beats the primary's %lu Hz - staying put "
						"(frame rate is capped there)", dmPrim.dmDisplayFrequency);
				}
			}

			SetWindowPos(g_hMainWnd, HWND_TOP, nWinX, nWinY, (int)nSurfW, (int)nSurfH,
				SWP_FRAMECHANGED | SWP_SHOWWINDOW);

			RECT rcNow;
			GetClientRect(g_hMainWnd, &rcNow);
			VRLog::Msg("window: borderless at (%d,%d), client %dx%d for a %ux%u render surface",
				nWinX, nWinY, rcNow.right - rcNow.left, rcNow.bottom - rcNow.top, nSurfW, nSurfH);

			s_bSizedWindow = true;
		}
	}

	static bool s_bWasLive = false;
	const bool bLive = VRShared::IsLive();
	if (bLive != s_bWasLive)
	{
		s_bWasLive = bLive;
		VRLog::Msg("head tracking %s", bLive ? "LIVE" : "lost - camera is flat");
		if (bLive)
		{
			const VRSharedState& s = VRShared::State();
			VRLog::Msg("  host reports IPD %.1f mm, fov L%.1f R%.1f U%.1f D%.1f deg",
				s.fIpdMeters * 1000.0f,
				s.fFovLeftRad * 57.2957795f, s.fFovRightRad * 57.2957795f,
				s.fFovUpRad * 57.2957795f, s.fFovDownRad * 57.2957795f);
		}
	}

	// This will happen when something wanted to disconnect, but wasn't
	// in a valid location to do so.  (e.g. when processing packets..)
	if (m_bForceDisconnect)
	{
		g_pLTClient->Disconnect();
		m_bForceDisconnect = LTFALSE;
		return;
	}

	// Set up the time for this frame...
    m_fFrameTime = g_pLTClient->GetFrameTime();
	if (m_fFrameTime > MAX_FRAME_DELTA)
	{
		m_fFrameTime = MAX_FRAME_DELTA;
	}

	// VR: M2's correctness test depends on this being read once per frame.
	VRLog::NoteFrameTime(m_fFrameTime);

	// Work-around for the timeout bug,
	// if requested try and re-connect to a specific server
	if (m_sRetryAddress.size() > 0)
	{
		// Try to rejoin...
		DoJoinGame((char*)m_sRetryAddress.c_str());
		m_sRetryAddress = "";
	}

	g_pInterfaceMgr->GetPlayerStats()->UpdateFramerate(1 / m_fFrameTime);

	// Update tint if applicable (always do this to make sure tinting
	// gets finished)...

	UpdateScreenFlash();
	m_ScreenTintMgr.Update();


	// Update models (powerups) glowing...

	UpdateModelGlow();


	// Update client-side physics structs...

	SetPhysicsStateTimeStep(&g_normalPhysicsState, m_fFrameTime);
	SetPhysicsStateTimeStep(&g_waterPhysicsState, m_fFrameTime);



	// Update the interface (don't do anything if the interface mgr
	// handles the update...)

	if (m_InterfaceMgr.Update())
	{
		// Actually this is always on top
		g_pConsoleMgr->Draw();

		return;
	}

	if (IsPlayerDead())
	{
		if (!IsMultiplayerGame())
		{
			if (s_fDeadTimer < s_fDeathDelay)
			{
				s_fDeadTimer += m_fFrameTime;
			}
			else
			{
				LTBOOL bHandleMissionFailed = LTTRUE;

				if (m_InterfaceMgr.FadingScreen() && m_InterfaceMgr.FadingScreenIn())
				{
					bHandleMissionFailed = m_InterfaceMgr.ScreenFadeDone();
				}

				if (bHandleMissionFailed)
				{
					HandleMissionFailed();
				}
			}
		}

	}

	// EVERY GAME STATE CHANGE, WITH WHETHER THE WORLD IS STILL BEING DRAWN.
	//
	// Headset testing reports the picture dropping to a flat panel when an HQ dialogue
	// starts. UpdatePlaying - which owns the stereo world render - is gated
	// only on the player being in the world, so on the face of it dialogue
	// should render exactly like play. Something else is stopping it, and a
	// state trace is the cheapest way to see which state it stops in.
	{
		static int s_nLastState = -1;
		const int nState = (int)m_InterfaceMgr.GetGameState();
		const bool bWorld = (IsPlayerInWorld()
							 && m_InterfaceMgr.GetGameState() != GS_UNDEFINED);
		if (nState != s_nLastState)
		{
			s_nLastState = nState;
			VRLog::Msg("VRState: game state -> %d | player in world %d |"
				" world renders %d | stereo %d | this frame the world %s",
				nState, (int)IsPlayerInWorld(),
				(int)((g_vtVRStereo.GetFloat() > 0.0f) ? 2 : 1),
				(int)g_vtVRStereo.GetFloat(),
				bWorld ? "IS drawn" : "is NOT drawn  <- flat from here");
		}
	}

	// At this point we only want to proceed if the player is in the world...
	if (IsPlayerInWorld() && m_InterfaceMgr.GetGameState() != GS_UNDEFINED)
	{
		UpdatePlaying();
	}

	// 
	g_pConsoleMgr->Draw();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdatePlaying()
//
//	PURPOSE:	Handle updating playing (normal) game state
//
// ----------------------------------------------------------------------- //

void CGameClientShell::UpdatePlaying()
{
	// Handle first update...

	if (m_bFirstUpdate)
	{
		FirstUpdate();
	}


	// Update player movement...

	m_MoveMgr.Update();


	// Update our camera offset mgr...

	m_CameraOffsetMgr.Update();



	// Update reverb...

	UpdateSoundReverb();


	// Tell the player to "start" the level...

	if (!m_bStartedLevel)
	{
        m_bStartedLevel = LTTRUE;
		StartLevel();
	}


	// Update Player...

	UpdatePlayer();


	// Update sky-texture panning...

	if (m_bPanSky && !m_bGamePaused)
	{
		m_fCurSkyXOffset += m_fFrameTime * m_fPanSkyOffsetX;
		m_fCurSkyZOffset += m_fFrameTime * m_fPanSkyOffsetZ;

        g_pLTClient->SetGlobalPanInfo(GLOBALPAN_SKYSHADOW, m_fCurSkyXOffset, m_fCurSkyZOffset, m_fPanSkyScaleX, m_fPanSkyScaleZ);
	}


	// Keep track of what the player is doing...

	UpdatePlayerFlags();

	// Update any debugging information...

	UpdateDebugInfo();

#ifndef _FINAL
	// Update cheats...(if a cheat is in effect, just return)...

	if (!IsMultiplayerGame())
	{
		if (UpdateCheats()) return;
	}
#endif

	// Update head-bob/head-cant camera offsets...

	m_HeadBobMgr.Update();


	// Update duck camera offset...

	UpdateDuck();


	// Update the camera's position...

	UpdateCamera();


	// Update any overlays...

	m_InterfaceMgr.UpdateOverlays();


	// Render the camera...

	VRLog::Count(VRLog::CTR_UPDATE_PLAYING);
	RenderCamera();

	// Update container effects...

	UpdateContainerFX();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::PostUpdate()
//
//	PURPOSE:	Handle post updates - after the scene is rendered
//
// ----------------------------------------------------------------------- //

// Publish the model list once per Update tick. RenderCamera calls this after
// the weapon update, which is the right moment in the world; the MAIN MENU
// never runs RenderCamera - its art is 3D models drawn through the folder
// path - so PostUpdate calls it too, and the tick guard makes the second
// call free. The main menu was completely black in the headset the first time the
// publish lived in RenderCamera alone.
void CGameClientShell::VRPublishOnce()
{
	if (m_nVRPublishedTick == m_nVRUpdateTick) return;
	// FROZEN WHILE PAUSED. The server stops, but the client keeps animating
	// its models and this kept publishing them, so behind the pause menu a
	// running guard ran in place and slid to where he was going on resume,
	// eyes blinked, sunglasses went missing (attachments are not processed
	// while paused). Retail shows a frozen screenshot under its pause menu.
	// Not publishing leaves the renderer drawing the last frame it was
	// given, which is the same thing. VRPauseFreeze 0 lets them move.
	// A FULL-CARD FOLDER IS NOT A PAUSE: its card, cubes and flower are
	// published every tick like the main menu's, or nothing is drawn.
	if (g_vtVRPauseFreeze.GetFloat() > 0.0f && IsInWorld()
		&& m_InterfaceMgr.GetGameState() == GS_FOLDER
		&& !VRCardScene())
		return;
	m_nVRPublishedTick = m_nVRUpdateTick;
	LARGE_INTEGER q0, q1, qf;
	QueryPerformanceCounter(&q0);
	VRPublishModels();
	QueryPerformanceCounter(&q1); QueryPerformanceFrequency(&qf);
	if (qf.QuadPart)
		VRLog::AddClientVRMs((double)(q1.QuadPart - q0.QuadPart) * 1000.0
							 / (double)qf.QuadPart);
}

void CGameClientShell::PostUpdate()
{
	// Anything the render path did not publish this tick (menus, cutscene
	// states without a world render) goes now.
	VRPublishOnce();

	if (m_bQuickSave && GetGameType() == SINGLE)
	{
		SaveGame(QUICKSAVE_FILENAME);
        m_bQuickSave = LTFALSE;
	}

	// Conditions where we don't want to flip...

	if (m_ePlayerState == PS_UNKNOWN && m_bInWorld && m_InterfaceMgr.GetGameState() != GS_FOLDER)
	{
		// VR: one of two stock paths that render a frame without presenting it.
		VRLog::Msg("no-flip: PostUpdate early-out (PS_UNKNOWN, in world, GameState=%d)",
			(int)m_InterfaceMgr.GetGameState());
		return;
	}


	m_InterfaceMgr.PostUpdate();

	// Animations are still wonky in multiplayer. Allows you to shoot faster, so let's always limit that in mp.
	// ---
	// Occasionally we'll need to unlock the framerate (like during loading!)
	// But we also want the user to have the option to unlock it,
	// so that's why there's two almost identical lock vars here.
	if ( (m_bLockFramerate && g_vtLockFPS.GetFloat()) || IsMultiplayerGame())
	{
		// Limit our framerate so the game actually runs properly.
		LARGE_INTEGER NewTime;

		// VR needs 90+; the stock limiter is hardcoded to 60.
		LONGLONG lTarget = m_lFrametime;
		const float fVRFps = g_vtVRFramerate.GetFloat();
		if (fVRFps > 1.0f && m_lTimerFrequency.QuadPart > 0)
		{
			lTarget = (LONGLONG)((double)m_lTimerFrequency.QuadPart / (double)fVRFps);
		}

		// Yield rather than spin. The stock loop busy-waits on a tight
		// QueryPerformanceCounter with no sleep, pinning a core for the whole
		// idle portion of every frame. That core is now contended - the VR host
		// is capturing, blitting and submitting on the same machine - and
		// starving it shows up as judder and an unresponsive window.
		while (1) {
			QueryPerformanceCounter(&NewTime);
			const LONGLONG lTime = NewTime.QuadPart - m_lNextUpdate;
			if (lTime > lTarget) {
				// Advance the deadline by exactly one interval rather than
				// resetting it to now. Resetting absorbs every overshoot
				// permanently, so the average period is always longer than the
				// target and never catches up - measured 11.197 ms against an
				// 11.111 ms target, with individual frames ranging from 9 ms
				// upward. In a headset that irregularity reads as micro-judder
				// even when the average looks close enough.
				m_lNextUpdate += lTarget;

				// Cap how much lost time may be reclaimed. Allowing a whole
				// frame of catch-up was measured worse than the drift it was
				// meant to fix: the average barely moved (11.197 -> 11.231 ms)
				// while the shortest frame fell from 9.0 to 6.0 ms, because
				// every overshoot was followed by a frame that ran immediately.
				// Short-term irregularity reads worse in a headset than a
				// slowly drifting average, so only a quarter frame of arrears
				// is carried and anything beyond that is forgiven.
				const LONGLONG lMaxArrears = lTarget / 4;
				if (NewTime.QuadPart - m_lNextUpdate > lMaxArrears)
					m_lNextUpdate = NewTime.QuadPart - lMaxArrears;
				break;
			}

			// Sleep only while there is comfortably more than a millisecond
			// left, then spin the remainder for accuracy.
			const LONGLONG lLeft = lTarget - lTime;
			if (m_lTimerFrequency.QuadPart > 0 &&
				lLeft > (m_lTimerFrequency.QuadPart / 500))		// > 2ms
			{
				Sleep(1);
			}
			else
			{
				Sleep(0);
			}
		}
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::GetDynamicSoundFilter()
//
//	PURPOSE:	Get the dynamic sound filter
//
// ----------------------------------------------------------------------- //

SOUNDFILTER* CGameClientShell::GetDynamicSoundFilter()
{
	// See if we have a current container filter override...

	SOUNDFILTER* pFilter = g_pSoundFilterMgr->GetFilter(m_nSoundFilterId);
	if (!pFilter) return LTNULL;


	if (!g_pSoundFilterMgr->IsDynamic(pFilter))
	{
		// Found it...
		return pFilter;
	}
	else  // Calculate the filter based on the listener...
	{
		// For now just return global default (from WorldProperties)

		return g_pSoundFilterMgr->GetFilter(m_nGlobalSoundFilterId);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateSoundReverb()
//
//	PURPOSE:	Update the sound reverb
//
// ----------------------------------------------------------------------- //

void CGameClientShell::UpdateSoundReverb( )
{
	if (!m_bUseReverb) return;

    HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();
	if (!hPlayerObj) return;

	float fReverbLevel;
    HCONSOLEVAR hVar = g_pLTClient->GetConsoleVar("ReverbLevel");
	if (hVar)
	{
        fReverbLevel = g_pLTClient->GetVarValueFloat(hVar);
	}
	else
	{
		fReverbLevel = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_DEFAULTLEVEL);
	}

	// Check if reverb was off and is still off...

	if (fReverbLevel < 0.001f && m_fReverbLevel < 0.001f) return;

	m_fReverbLevel = fReverbLevel;

	// Check if it's time yet...

    if (g_pLTClient->GetTime() < m_fNextSoundReverbTime) return;

	// Update timer...

	float fUpdatePeriod = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_UPDATEPERIOD);
    m_fNextSoundReverbTime = g_pLTClient->GetTime() + fUpdatePeriod;

    HOBJECT hFilterList[] = {hPlayerObj, m_MoveMgr.GetObject(), LTNULL};

	ClientIntersectInfo info;
	ClientIntersectQuery query;
	query.m_Flags = INTERSECT_OBJECTS | IGNORE_NONSOLID;
	query.m_FilterFn = ObjListFilterFn;
	query.m_pUserData = hFilterList;

    g_pLTClient->GetObjectPos(hPlayerObj, &query.m_From);

	// Make sure the player moved far enough to check reverb again...

	float fPlayerMoveDist = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_PLAYERMOVEDIST);
	if (VEC_DIST(query.m_From, m_vLastReverbPos) < fPlayerMoveDist) return;

	float fReverbSegmentLen = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_INTERSECTSEGMENTLEN);
	VEC_COPY( m_vLastReverbPos, query.m_From );

    LTVector vPos[6], vSegs[6];
	VEC_SET( vSegs[0], query.m_From.x + fReverbSegmentLen, query.m_From.y, query.m_From.z );
	VEC_SET( vSegs[1], query.m_From.x - fReverbSegmentLen, query.m_From.y, query.m_From.z );
	VEC_SET( vSegs[2], query.m_From.x, query.m_From.y + fReverbSegmentLen, query.m_From.z );
	VEC_SET( vSegs[3], query.m_From.x, query.m_From.y - fReverbSegmentLen, query.m_From.z );
	VEC_SET( vSegs[4], query.m_From.x, query.m_From.y, query.m_From.z + fReverbSegmentLen );
	VEC_SET( vSegs[5], query.m_From.x, query.m_From.y, query.m_From.z - fReverbSegmentLen );

    LTBOOL bOpen = LTFALSE;
	for (int i = 0; i < 6; i++)
	{
		VEC_COPY( query.m_To, vSegs[i] );

        if ( g_pLTClient->IntersectSegment( &query, &info ))
		{
			VEC_COPY( vPos[i], info.m_Point );

			SurfaceType eSurfType = GetSurfaceType(info);

			if (eSurfType == ST_AIR || eSurfType == ST_SKY ||
				eSurfType == ST_INVISIBLE)
			{
                bOpen = LTTRUE;
			}
		}
		else
		{
			VEC_COPY( vPos[i], vSegs[i] );
            bOpen = LTTRUE;
		}
	}

	float fVolume = VEC_DIST( vPos[0], vPos[1] );
	fVolume *= VEC_DIST( vPos[2], vPos[3] );
	fVolume *= VEC_DIST( vPos[4], vPos[5] );


	ReverbProperties reverbProperties;

	// Use room types that are not completely enclosed rooms...

	if ( bOpen )
	{
		float fPipeSpace  = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_PIPESPACE);
		float fPlainSpace = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_PLAINSPACE);
		float fArenaSpace = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_ARENASPACE);

		if ( fVolume < fPipeSpace*fPipeSpace*fPipeSpace )
		{
			reverbProperties.m_dwAcoustics  = REVERB_ACOUSTICS_SEWERPIPE;
			reverbProperties.m_fReflectTime = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_PIPEREFLECTTIME);
			reverbProperties.m_fDecayTime	= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_PIPEDECAYTIME);
			reverbProperties.m_fVolume		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_PIPEVOLUME) * m_fReverbLevel;
			reverbProperties.m_fDamping		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_PIPEDAMPING);
		}
		else if ( fVolume < fPlainSpace*fPlainSpace*fPlainSpace )
		{
			reverbProperties.m_dwAcoustics = REVERB_ACOUSTICS_PLAIN;
			reverbProperties.m_fReflectTime = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_PLAINREFLECTTIME);
			reverbProperties.m_fDecayTime	= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_PLAINDECAYTIME);
			reverbProperties.m_fVolume		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_PLAINVOLUME) * m_fReverbLevel;
			reverbProperties.m_fDamping		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_PLAINDAMPING);
		}
		else if ( fVolume < fArenaSpace*fArenaSpace*fArenaSpace )
		{
			reverbProperties.m_dwAcoustics = REVERB_ACOUSTICS_ARENA;
			reverbProperties.m_fReflectTime = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_ARENAREFLECTTIME);
			reverbProperties.m_fDecayTime	= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_ARENADECAYTIME);
			reverbProperties.m_fVolume		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_ARENAVOLUME) * m_fReverbLevel;
			reverbProperties.m_fDamping		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_ARENADAMPING);
		}
		else
		{
			reverbProperties.m_dwAcoustics  = REVERB_ACOUSTICS_MOUNTAINS;
			reverbProperties.m_fReflectTime = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_MOUNTAINSREFLECTTIME);
			reverbProperties.m_fDecayTime	= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_MOUNTAINSDECAYTIME);
			reverbProperties.m_fVolume		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_MOUNTAINSVOLUME) * m_fReverbLevel;
			reverbProperties.m_fDamping		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_MOUNTAINSDAMPING);
		}
	}
	else  // Use room types that are enclosed rooms
	{
		float fStoneRoomSpace	= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_STONEROOMSPACE);
		float fHallwaySpace		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_HALLWAYSPACE);
		float fConcertHallSpace = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_CONCERTHALLSPACE);

		if ( fVolume < fStoneRoomSpace*fStoneRoomSpace*fStoneRoomSpace)
		{
			reverbProperties.m_dwAcoustics  = REVERB_ACOUSTICS_STONEROOM;
			reverbProperties.m_fReflectTime = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_STONEROOMREFLECTTIME);
			reverbProperties.m_fDecayTime	= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_STONEROOMDECAYTIME);
			reverbProperties.m_fVolume		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_STONEROOMVOLUME) * m_fReverbLevel;
			reverbProperties.m_fDamping		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_STONEROOMDAMPING);
		}
		else if ( fVolume < fHallwaySpace*fHallwaySpace*fHallwaySpace )
		{
			reverbProperties.m_dwAcoustics  = REVERB_ACOUSTICS_HALLWAY;
			reverbProperties.m_fReflectTime = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_HALLWAYREFLECTTIME);
			reverbProperties.m_fDecayTime	= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_HALLWAYDECAYTIME);
			reverbProperties.m_fVolume		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_HALLWAYVOLUME) * m_fReverbLevel;
			reverbProperties.m_fDamping		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_HALLWAYDAMPING);
		}
		else if ( fVolume < fConcertHallSpace*fConcertHallSpace*fConcertHallSpace )
		{
			reverbProperties.m_dwAcoustics  = REVERB_ACOUSTICS_CONCERTHALL;
			reverbProperties.m_fReflectTime = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_CONCERTHALLREFLECTTIME);
			reverbProperties.m_fDecayTime	= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_CONCERTHALLDECAYTIME);
			reverbProperties.m_fVolume		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_CONCERTHALLVOLUME) * m_fReverbLevel;
			reverbProperties.m_fDamping		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_CONCERTHALLDAMPING);
		}
		else
		{
			reverbProperties.m_dwAcoustics  = REVERB_ACOUSTICS_AUDITORIUM;
			reverbProperties.m_fReflectTime = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_AUDITORIUMREFLECTTIME);
			reverbProperties.m_fDecayTime	= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_AUDITORIUMDECAYTIME);
			reverbProperties.m_fVolume		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_AUDITORIUMVOLUME) * m_fReverbLevel;
			reverbProperties.m_fDamping		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_AUDITORIUMDAMPING);
		}
	}

	if (m_DamageFXMgr.IsStunned())
	{
		reverbProperties.m_dwAcoustics  = REVERB_ACOUSTICS_DIZZY;
//		reverbProperties.m_fReflectTime = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_STONEROOMREFLECTTIME);
//		reverbProperties.m_fDecayTime	= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_STONEROOMDECAYTIME);
//		reverbProperties.m_fVolume		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_STONEROOMVOLUME) * m_fReverbLevel;
//		reverbProperties.m_fDamping		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_STONEROOMDAMPING);
	}
	else if (m_DamageFXMgr.IsPoisoned())
	{
		reverbProperties.m_dwAcoustics  = REVERB_ACOUSTICS_DRUGGED;
//		reverbProperties.m_fReflectTime = g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_PIPEREFLECTTIME);
//		reverbProperties.m_fDecayTime	= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_PIPEDECAYTIME);
//		reverbProperties.m_fVolume		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_PIPEVOLUME) * m_fReverbLevel;
//		reverbProperties.m_fDamping		= g_pClientButeMgr->GetReverbAttributeFloat(REVERB_BUTE_PIPEDAMPING);
	}

	// Override to water if in it...

	if (IsLiquid(m_eCurContainerCode))
	{
		reverbProperties.m_dwAcoustics = REVERB_ACOUSTICS_UNDERWATER;
	}

	reverbProperties.m_dwParams = REVERBPARAM_ALL;
    g_pLTClient->SetReverbProperties(&reverbProperties);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateCheats()
//
//	PURPOSE:	Update cheats...
//
// ----------------------------------------------------------------------- //

LTBOOL CGameClientShell::UpdateCheats()
{
	if (m_bAdjustLightScale)
	{
		AdjustLightScale();
	}

	if (m_bAdjustLightAdd)
	{
		AdjustLightAdd();
	}

	if (m_bAdjustFOV)
	{
		AdjustFOV();
	}

	if (m_bAdjustWeaponBreach)
	{
		AdjustWeaponBreach();
	}

	if (m_bAdjust1stPersonCamera)
	{
		Adjust1stPersonCamera();
	}

	// If in spectator mode, just do the camera stuff...

	if (m_bSpectatorMode || IsPlayerDead())
	{
		UpdateCamera();
		RenderCamera();
        return LTTRUE;
	}

	// Update weapon position if appropriated...

	if (m_bTweakingWeapon)
	{
		UpdateWeaponPosition();
		UpdateCamera();
		RenderCamera();
        return LTTRUE;
	}
	else if (m_bTweakingWeaponMuzzle)
	{
		UpdateWeaponMuzzlePosition();
		UpdateCamera();
		RenderCamera();
        return LTTRUE;
	}

    return LTFALSE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateCamera()
//
//	PURPOSE:	Update the camera position/rotation
//
// ----------------------------------------------------------------------- //

void CGameClientShell::UpdateCamera()
{
	// Update the sway...

	if (IsZoomed())
	{
		UpdateCameraSway();
	}

	// Update the camera's position and rotation..

	UpdateAlternativeCamera();


	// Update the player camera...

	UpdatePlayerCamera();


	if (!m_bUsingExternalCamera)
	{
		if (IsMultiplayerGame() || m_InterfaceMgr.AllowCameraMovement())
		{
			UpdateCameraPosition();
		}
	}

	if (m_InterfaceMgr.AllowCameraMovement())
	{
		CalculateCameraRotation();
		UpdateCameraRotation();
	}

	if (m_bUsingExternalCamera)
	{
        HandleZoomChange(m_weaponModel.GetWeaponId(), LTTRUE);
	}

	// Update zoom if applicable...

	if (m_bZooming)
	{
		UpdateCameraZoom();
	}

	// Update shake if applicable...

	UpdateCameraShake();


	// Make sure the player gets updated

 	if (IsMultiplayerGame() || m_InterfaceMgr.AllowCameraMovement())
	{
		UpdatePlayerInfo();
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdatePlayerInfo()
//
//	PURPOSE:	Tell the player about the new camera stuff
//
// ----------------------------------------------------------------------- //

void CGameClientShell::UpdatePlayerInfo()
{
	if (m_bAllowPlayerMovement != m_bLastAllowPlayerMovement)
	{
		SetInputState(m_bAllowPlayerMovement);
	}

	if (m_PlayerCamera.IsChaseView() != m_bLastSent3rdPerson)
	{
		m_nPlayerInfoChangeFlags |= CLIENTUPDATE_3RDPERSON;
		m_bLastSent3rdPerson = m_PlayerCamera.IsChaseView();

		if (m_PlayerCamera.IsChaseView())
		{
			m_nPlayerInfoChangeFlags |= CLIENTUPDATE_3RDPERVAL;
		}
	}

	if (m_bAllowPlayerMovement != m_bLastAllowPlayerMovement)
	{
		m_nPlayerInfoChangeFlags |= CLIENTUPDATE_ALLOWINPUT;
	}

	// Always send CLIENTUPDATE_ALLOWINPUT changes guaranteed...

	if (m_nPlayerInfoChangeFlags & CLIENTUPDATE_ALLOWINPUT)
	{
        HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_PLAYER_UPDATE);
        g_pLTClient->WriteToMessageWord(hMessage, CLIENTUPDATE_ALLOWINPUT);
        g_pLTClient->WriteToMessageByte(hMessage, (uint8)m_bAllowPlayerMovement);
        g_pLTClient->EndMessage(hMessage);
		m_nPlayerInfoChangeFlags &= ~CLIENTUPDATE_ALLOWINPUT;
	}

    float fCurTime   = g_pLTClient->GetTime();
	float fSendRate  = 1.0f / g_CV_CSendRate.GetFloat(DEFAULT_CSENDRATE);
	float fSendDelta = (fCurTime - m_fPlayerInfoLastSendTime);

	if (!IsMultiplayerGame() || fSendDelta > fSendRate)
	{
        HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_PLAYER_UPDATE);

		if (g_vtPlayerRotate.GetFloat(1.0) > 0.0)
		{
			m_nPlayerInfoChangeFlags |= CLIENTUPDATE_PLAYERROT;
		}

        g_pLTClient->WriteToMessageWord(hMessage, m_nPlayerInfoChangeFlags);

		if (m_nPlayerInfoChangeFlags & CLIENTUPDATE_PLAYERROT)
		{
			// Set the player's rotation (don't allow model to rotate up/down).

            LTRotation rPlayerRot;
            //g_pLTClient->SetupEuler(&rPlayerRot, 0.0f, m_fYaw, m_fRoll);
            g_pLTClient->SetupEuler(&rPlayerRot, m_fPlayerPitch, m_fPlayerYaw, m_fPlayerRoll);
            g_pLTClient->WriteToMessageByte(hMessage, CompressRotationByte(g_pLTClient->Common(), &rPlayerRot));

			{ // BL 10/05/00 - character waist pitching
//				CGameSettings* pSettings = m_InterfaceMgr.GetSettings();
//				int8 byPitch = (int8)(10.0f*(pSettings->MouseInvertY() ? m_fPitch : -m_fPitch));
				int8 byPitch = (int8)(10.0f * -m_fPitch);
				//g_pLTClient->CPrint("Client pitch = %d", byPitch);
				g_pLTClient->WriteToMessageByte(hMessage, byPitch);
			}
		}

		// Write position info...

		m_MoveMgr.WritePositionInfo(hMessage);

        g_pLTClient->EndMessage2(hMessage, 0);

		m_fPlayerInfoLastSendTime = fCurTime;
		m_nPlayerInfoChangeFlags  = 0;
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::StartLevel()
//
//	PURPOSE:	Tell the player to start the level
//
// ----------------------------------------------------------------------- //

void CGameClientShell::StartLevel()
{
	if (IsMultiplayerGame()) return;

    HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_SINGLEPLAYER_START);
    g_pLTClient->EndMessage(hMessage);

	// Set Initial cheats...

	if (g_pCheatMgr)
	{
        uint8 nNumCheats = g_pClientButeMgr->GetNumCheatAttributes();
		CString strCheat;

        for (uint8 i=0; i < nNumCheats; i++)
		{
			strCheat = g_pClientButeMgr->GetCheat(i);
			if (strCheat.GetLength() > 1)
			{
				g_pCheatMgr->Check(strCheat.GetBuffer(0));
			}
		}
		// The VR level tour's cheats, SET rather than toggled. See VRCheats.
		const int nVRCheats = (int)g_vtVRCheats.GetFloat();
#if VR_DEBUG_TOOLS
		// THE LAUNCHER AND THE MENU MUST AGREE. play-vr -God used to apply the
		// cheats directly, so Options > VR showed every debug switch OFF while
		// god mode was plainly on. It now lights the switches instead, and the
		// applier above does the work.
		if (nVRCheats & 1) g_vtVRDebugGod.SetFloat(1.0f);
		if (nVRCheats & 2) g_vtVRDebugArsenal.SetFloat(1.0f);
		if (nVRCheats & 4) g_vtVRDebugMissions.SetFloat(1.0f);
		// AND NOTHING ELSE. The direct calls below were kept alongside the
		// switches at first, so god mode was sent TWICE on every world entry
		// - and the server toggles, so two sends is OFF. The tester played the
		// whole of the tester's first session with -God and no god mode.
#else
		if (nVRCheats & 1) g_pCheatMgr->VRSetGod();
		if (nVRCheats & 2) g_pCheatMgr->VRSetKFA();
		// "Everything" includes the mods: without them the scope and the silencer
		// never exist on a cheated loadout, and the desk cannot see either.
		if (nVRCheats & 2) g_pCheatMgr->VRSetMods();
		if ((nVRCheats & 4) && !g_pCheatMgr->VRIsActive(CHEAT_ALL_MISSIONS))
			g_pCheatMgr->Check("mpbeenthere");
#endif
		if (nVRCheats)
			VRLog::Msg("VRCheats %d applied on world entry: god %s, everything %s, all missions %s",
				nVRCheats, (nVRCheats & 1) ? "on" : "-", (nVRCheats & 2) ? "on" : "-",
				(nVRCheats & 4) ? "on" : "-");

		// PUT A GUN IN THE PLAYER'S HAND. The arsenal cheat GIVES every weapon and
		// selects none of them, so the player still holds 'fisty_cuffs' - and
		// fists have no muzzle flash. A desk harness that fires for ten
		// seconds and photographs nothing therefore looks exactly like a
		// broken muzzle flash, twice, which is what happened on 11 September
		// before this existed. The weapon KEY does not do it either: the VR
		// control path owns input and the keystroke went nowhere.
		//
		// VRDebugWeapon N sends the Nth weapon command, the same command a
		// number key would send. Sending it HERE does nothing: the arsenal
		// cheat asks the SERVER for the weapons and they have not arrived two
		// frames into the world, so the change is refused and the player keeps
		// the player's fists. The send is retried once a second from the update loop
		// instead - see VRDebugWeapon there.
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdatePlayerCamera()
//
//	PURPOSE:	Update the player camera
//
// ----------------------------------------------------------------------- //

LTBOOL CGameClientShell::UpdatePlayerCamera()
{
	// Make sure our player camera is attached...

	if (m_PlayerCamera.GetAttachedObject() != m_MoveMgr.GetObject())
	{
		m_PlayerCamera.AttachToObject(m_MoveMgr.GetObject());
	}

	if (m_PlayerCamera.IsChaseView())
	{
		// Init the camera so they can be adjusted via the console...

		InitPlayerCamera();

		Update3rdPersonInfo();
	}

	// Every frame, first person included - this is the only crosshair the
	// player has once the gun stops following the head.
	UpdateVRAimMarker();


	// Update our camera position based on the player camera...

	m_PlayerCamera.CameraUpdate(m_fFrameTime);


    return LTTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::InitPlayerCamera()
//
//	PURPOSE:	Update the player camera
//
// ----------------------------------------------------------------------- //

void CGameClientShell::InitPlayerCamera()
{
    LTVector vOffset(0.0f, 0.0f, 0.0f);
	vOffset.y = g_vtChaseCamOffset.GetFloat();

	m_PlayerCamera.SetDistUp(g_vtChaseCamDistUp.GetFloat());
	m_PlayerCamera.SetDistBack(g_vtChaseCamDistBack.GetFloat());
	m_PlayerCamera.SetPointAtOffset(vOffset);
	m_PlayerCamera.SetChaseOffset(vOffset);
	m_PlayerCamera.SetCameraState(CPlayerCamera::SOUTH);

	// Determine the first person offset...

	vOffset = g_vPlayerCameraOffset;

	CCharacterFX* pChar = m_MoveMgr.GetCharacterFX();
	if (pChar)
	{
		vOffset = GetPlayerHeadOffset(g_pModelButeMgr, pChar->GetModelStyle());
	}

	m_PlayerCamera.SetFirstPersonOffset(vOffset);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::SetExternalCamera()
//
//	PURPOSE:	Turn on/off external camera mode
//
// ----------------------------------------------------------------------- //

void CGameClientShell::SetExternalCamera(LTBOOL bExternal)
{
	if (bExternal && m_PlayerCamera.IsFirstPerson())
	{
		m_DamageFXMgr.Clear();
        m_weaponModel.SetVisible(LTFALSE);

        ShowPlayer(LTTRUE);
		m_PlayerCamera.GoChaseMode();
		m_PlayerCamera.CameraUpdate(0.0f);

        m_InterfaceMgr.EnableCrosshair(LTFALSE); // Disable cross hair in 3rd person...
	}
	else if (!bExternal && !m_PlayerCamera.IsFirstPerson()) // Go Internal
	{
        m_weaponModel.SetVisible(LTTRUE);
        ShowPlayer(LTFALSE);

		m_PlayerCamera.GoFirstPerson();
		m_PlayerCamera.CameraUpdate(0.0f);

        m_InterfaceMgr.EnableCrosshair(LTTRUE);

		if (m_h3rdPersonCrosshair)
		{
            g_pLTClient->DeleteObject(m_h3rdPersonCrosshair);
            m_h3rdPersonCrosshair = LTNULL;
		}
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateAlternativeCamera()
//
//	PURPOSE:	Update the camera using an alternative camera
//
// ----------------------------------------------------------------------- //

LTBOOL CGameClientShell::UpdateAlternativeCamera()
{
	m_bLastAllowPlayerMovement = m_bAllowPlayerMovement;
    m_bAllowPlayerMovement     = LTTRUE;

    HOBJECT hObj = LTNULL;

	// See if we should use an alternative camera position...

	CSpecialFXList* pCameraList = m_sfxMgr.GetCameraList();
	if (pCameraList)
	{
		int nNum = pCameraList->GetSize();

		for (int i=0; i < nNum; i++)
		{
			CCameraFX* pCamFX = (CCameraFX*)(*pCameraList)[i];
			if (!pCamFX) continue;

			hObj = pCamFX->GetServerObj();

			if (hObj)
			{
                uint32 dwUsrFlags;
                g_pLTClient->GetObjectUserFlags(hObj, &dwUsrFlags);

#if VR_DEBUG_TOOLS
				// See g_bVRCineEnded. Without this the block below runs again
				// every frame and puts the camera straight back into CHASE
				// with the view weapon disabled, however many times the flag
				// is cleared - which is exactly what VRDebugEndCinematic
				// looked like it was fixing for weeks, and was not.
				if (g_bVRCineEnded) continue;
#endif
				if (dwUsrFlags & USRFLG_CAMERA_LIVE)
				{
                    m_InterfaceMgr.SetDrawInterface(LTFALSE);

                    SetExternalCamera(LTTRUE);

					m_bAllowPlayerMovement = pCamFX->AllowPlayerMovement();

                    LTVector vPos;
                    g_pLTClient->GetObjectPos(hObj, &vPos);
                    g_pLTClient->SetObjectPos(m_hCamera, &vPos);
                    m_bCameraPosInited = LTTRUE;

                    LTRotation rRot;
                    g_pLTClient->GetObjectRotation(hObj, &rRot);
                    g_pLTClient->SetObjectRotation(m_hCamera, &rRot);

					m_bCamIsListener = pCamFX->IsListener();

					// Always set the camera as the listener, the
					// is listener flag tells the dialogue where
					// to play ;)
					g_pLTClient->SetListener(LTFALSE, &vPos, &rRot);

					// Initialize the cinematic camera

					s_nLastCamType = pCamFX->GetType();

					TurnOnAlternativeCamera(s_nLastCamType);

                    return LTTRUE;
				}
			}
		}
	}


	// Okay, we're no longer using an external camera...

	if (m_bUsingExternalCamera)
	{
		TurnOffAlternativeCamera(s_nLastCamType);
	}


    return LTFALSE;
}



// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::TurnOnAlternativeCamera()
//
//	PURPOSE:	Set up using an alternative camera
//
// ----------------------------------------------------------------------- //

void CGameClientShell::TurnOnAlternativeCamera(uint8 nCamType)
{
	if(nCamType == CT_CINEMATIC) {
		m_InterfaceMgr.SetLetterBox(LTTRUE);
		//m_bLockFramerate = LTTRUE;
	}

	if (!m_bUsingExternalCamera)
	{
	    //g_pLTClient->CPrint("TURNING ALTERNATIVE CAMERA: ON");

		// Make sure we clear whatever was on the screen before
		// we switch to this camera...

		m_InterfaceMgr.ClearAllScreenBuffers();

        m_InterfaceMgr.ClosePopup();
		m_weaponModel.Disable(LTTRUE);
	}

    m_bUsingExternalCamera = LTTRUE;

	// Turn off model shadows in cinematics...
	WriteConsoleInt("MaxModelShadows", 0);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::TurnOffAlternativeCamera()
//
//	PURPOSE:	Turn off the alternative camera mode
//
// ----------------------------------------------------------------------- //

void CGameClientShell::TurnOffAlternativeCamera(uint8 nCamType)
{
    //g_pLTClient->CPrint("TURNING ALTERNATIVE CAMERA: OFF");

	m_InterfaceMgr.SetDrawInterface(LTTRUE);
    m_bUsingExternalCamera = LTFALSE;

	// Set the listener back to the client...

    g_pLTClient->SetListener(LTTRUE, LTNULL, LTNULL);

	// Force 1st person...

    SetExternalCamera(LTFALSE);

	m_weaponModel.Disable(LTFALSE);

	m_InterfaceMgr.SetLetterBox(LTFALSE);
	//m_bLockFramerate = LTFALSE;

	// We use DrawShadows as a guide on what MaxModelShadows we're going to use.
	// Projected shadows don't work in this version, so you can really only have one shadow per model!
	int nDrawShadows = GetConsoleInt("DrawShadows", 0);
	WriteConsoleInt("MaxModelShadows", nDrawShadows);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateCameraPosition()
//
//	PURPOSE:	Update the camera position
//
// ----------------------------------------------------------------------- //

void CGameClientShell::UpdateCameraPosition()
{
    LTVector vPos = m_PlayerCamera.GetPos();

	if (m_PlayerCamera.IsFirstPerson())
	{
		// EVERY PIECE OF THE CAMERA'S POSITION, once every 45 frames. The
		// club (M04S02) moved at the desk with a static head and a fixed
		// player position while the HQ held still (21 September); the sway
		// only runs zoomed, so one of these is it, and this says which.
		const LTVector vP0 = vPos;
		m_HeadBobMgr.AdjustCameraPos(vPos);
		const LTVector vBob = vPos - vP0;

		vPos.y	+= m_fCamDuck;
		vPos	+= m_CameraOffsetMgr.GetPosDelta();
		{
			static uint32 s_nSaidCam = 0;
			if ((++s_nSaidCam % 45) == 1)
			{
				const LTVector vCO = m_CameraOffsetMgr.GetPosDelta();
				const LTVector vPYR = m_CameraOffsetMgr.GetPitchYawRollDelta();
				VRLog::Msg("VRCamParts: player cam %.1f %.1f %.1f | bob %+.2f %+.2f %+.2f | duck %+.2f | offset %+.2f %+.2f %+.2f pyr %+.2f %+.2f %+.2f | pitch %+.2f yaw %+.2f | head-attached %d",
					vP0.x, vP0.y, vP0.z, vBob.x, vBob.y, vBob.z, m_fCamDuck, vCO.x, vCO.y, vCO.z,
					vPYR.x, vPYR.y, vPYR.z, m_fPitch, m_fYaw, (int)m_bCameraAttachedToHead);
			}
		}

		// Special case of camera being attached to the player's head
		// (i.e., death)...

		if (m_bCameraAttachedToHead)
		{
			GetPlayerHeadPosRot(vPos, m_rRotation);
		}
	}
	else
	{
        m_rRotation = m_PlayerCamera.GetRotation();
	}

 	g_pLTClient->SetObjectPos(m_hCamera, &vPos);


    m_bCameraPosInited = LTTRUE;
}



// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::LogHeadAxisTable()
//
//	PURPOSE:	Feed each head axis in on its own and print what the camera
//				actually does with it.
//
//				This should have existed on day one. The head composition has
//				been debugged for two days through a headset - lifting the head
//				tilted the view left - when it is a pure function of two
//				rotations and needs no headset, no host and no runtime to
//				evaluate. Every wrong turn in that time would have been visible
//				in this table in one run.
//
//				For each mode and a non-zero body yaw, three synthetic head
//				rotations go in - pitch alone, yaw alone, roll alone - and the
//				resulting camera basis comes out. A correct composition moves
//				the camera on the SAME axis the head moved on. Anything else is
//				the fault, and the table names which axis it landed on.
//
//				Body yaw is deliberately NOT zero: at zero every composition
//				agrees and the table would show nothing.
//
// ----------------------------------------------------------------------- //

void CGameClientShell::LogHeadAxisTable()
{
	const float fR2D = 57.2957795f;
	const float fBodyYawDeg = 45.0f;

	LTRotation rBody;
	g_pLTClient->SetupEuler(&rBody, 0.0f, DEG2RAD(fBodyYawDeg), 0.0f);

	LTVector vBU, vBR, vBF;
	g_pLTClient->GetRotationVectors(&rBody, &vBU, &vBR, &vBF);

	VRLog::Msg("--- head axis table (body yaw %.0f deg) ---", fBodyYawDeg);
	VRLog::Msg("  body alone: fwd(%+.3f %+.3f %+.3f) up(%+.3f %+.3f %+.3f)",
		vBF.x, vBF.y, vBF.z, vBU.x, vBU.y, vBU.z);
	VRLog::Msg("  a correct composition moves the camera on the axis the head moved on.");

	const char* pszAxis[3] = { "head PITCH +20", "head YAW   +20", "head ROLL  +20" };

	for (int nMode = 1; nMode <= 3; ++nMode)
	{
		VRLog::Msg("  VRQuatHead %d:", nMode);

		for (int nAxis = 0; nAxis < 3; ++nAxis)
		{
			// The head rotation as OpenXR would report it: 20 degrees about
			// X (pitch), Y (yaw) or Z (roll), right-handed.
			const float a = DEG2RAD(20.0f) * 0.5f;
			const float sn = (float)sin(a), cs = (float)cos(a);
			float qx = 0.0f, qy = 0.0f, qz = 0.0f;
			if (nAxis == 0) qx = sn;
			else if (nAxis == 1) qy = sn;
			else qz = sn;

			// The client's conversion: OpenXR right-handed -Z forward to
			// LithTech left-handed +Z forward is a Z basis flip.
			LTRotation rHead;
			rHead.Init(-qx, -qy, qz, cs);

			LTRotation rView;
			if (nMode == 1)      rView = rBody * rHead;
			else if (nMode == 2) rView = rHead * rBody;
			else
			{
				LTRotation rYawOnly;
				g_pLTClient->SetupEuler(&rYawOnly, 0.0f, DEG2RAD(fBodyYawDeg), 0.0f);
				rView = rHead * rYawOnly;
			}

			LTVector vU, vR, vF;
			g_pLTClient->GetRotationVectors(&rView, &vU, &vR, &vF);

			// Pitch: how far the forward vector left the horizontal.
			const float fPitch = (float)asin((vF.y > 1.0f) ? 1.0f
										   : ((vF.y < -1.0f) ? -1.0f : vF.y)) * fR2D;

			// Yaw: how far the forward vector swung horizontally, measured
			// against the body's own forward so the body's 45 is not counted.
			const float fYaw = (float)atan2(vF.x, vF.z) * fR2D - fBodyYawDeg;

			// Roll: how far UP leans out of the vertical plane containing
			// FORWARD. Not up.x - that reports the wrong answer after a yaw,
			// which is how a default came to be changed to a broken mode.
			LTVector vHR;
			vHR.x = vF.z; vHR.y = 0.0f; vHR.z = -vF.x;
			const float fLen = (float)sqrt(vHR.x * vHR.x + vHR.z * vHR.z);
			float fRoll = 0.0f;
			if (fLen > 1e-4f)
			{
				vHR.x /= fLen; vHR.z /= fLen;
				float d = vU.x * vHR.x + vU.z * vHR.z;
				if (d >  1.0f) d =  1.0f;
				if (d < -1.0f) d = -1.0f;
				fRoll = (float)asin(d) * fR2D;
			}

			const char* pszLanded = (nAxis == 0) ? "pitch" : ((nAxis == 1) ? "yaw" : "roll");
			const float fWanted = (nAxis == 0) ? fPitch : ((nAxis == 1) ? fYaw : fRoll);
			const bool  bClean  = (fabs(fWanted) > 15.0f)
							   && (fabs((nAxis == 0) ? fYaw   : fPitch) < 5.0f)
							   && (fabs((nAxis == 2) ? fPitch : fRoll)  < 5.0f);

			VRLog::Msg("    %s -> camera pitch %+6.1f  yaw %+6.1f  roll %+6.1f   %s",
				pszAxis[nAxis], fPitch, fYaw, fRoll,
				bClean ? "clean" : "LEAKS onto another axis");
			(void)pszLanded;
		}
	}

	VRLog::Msg("--- end head axis table ---");
}


// The head rotation the previous LIVE sample logged, in LithTech's frame.
// Four floats rather than an LTRotation because the only place it is read is
// inside RenderWorldEyes, which is a __try block.
static float g_fPrevHeadQuat[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static bool  g_bHavePrevHead    = false;

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::FrameResidualDeg()
//
//	PURPOSE:	How far the compositor's correction misses, in degrees.
//
//				The runtime is handed an image, told the view pose it was
//				rendered from, and at display time knows the view pose then. It
//				resamples the image by the rotation between the two, expressed
//				in the rendered view's own frame.
//
//				The image was actually drawn from camera C, and had it been
//				drawn at display time would have been drawn from C2. So the
//				rotation it needs is C^-1 * C2, in the same frame.
//
//				Those two must be the same rotation. This returns the angle
//				between them. It is the amount the compositor turns the picture
//				in a direction the scene never moved, and it is the only
//				quantity in this pipeline that can bend a wide flat image
//				differently every frame while the head moves and do nothing at
//				all while the mouse does.
//
//				NOTE the reference space the layer is submitted in does not
//				appear here, and cannot: both poses are located in it, so
//				(T^-1 P)^-1 (T^-1 P2) = P^-1 P2 for any T. Rotating the space
//				by the body yaw is inert by construction.
//
// ----------------------------------------------------------------------- //

float CGameClientShell::FrameResidualDeg(const LTRotation& rHeadPrev,
										 const LTRotation& rHeadNow,
										 const LTRotation& rBody, int nMode)
{
	LTRotation rBodyYaw;
	{
		LTVector vU, vR, vF;
		g_pLTClient->GetRotationVectors((LTRotation*)&rBody, &vU, &vR, &vF);
		const float fYaw = (float)atan2(vF.x, vF.z);
		g_pLTClient->SetupEuler(&rBodyYaw, 0.0f, fYaw, 0.0f);
	}

	LTRotation rC1, rC2;
	if (nMode >= 3)
	{
		rC1 = rHeadPrev * rBodyYaw;
		rC2 = rHeadNow  * rBodyYaw;
	}
	else if (nMode == 2)
	{
		rC1 = rHeadPrev * rBody;
		rC2 = rHeadNow  * rBody;
	}
	else
	{
		rC1 = rBody * rHeadPrev;
		rC2 = rBody * rHeadNow;
	}

	// What the scene did, in the rendered camera's frame.
	const LTRotation rTrue = rC1.Conjugate() * rC2;

	// What the runtime applies. The head rotations are already in LithTech's
	// frame, and the conversion is a quaternion homomorphism, so the delta of
	// the converted poses is the converted delta - no second conversion here.
	const LTRotation rApp = rHeadPrev.Conjugate() * rHeadNow;

	LTRotation rErr = rApp.Conjugate() * rTrue;
	float w = rErr.m_Quat[QW];
	if (w < 0.0f) w = -w;
	if (w > 1.0f) w = 1.0f;
	return 2.0f * (float)acos(w) * 57.2957795f;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::LogFrameAgreement()
//
//	PURPOSE:	The warping, as a table, with no headset and no host.
//
//				A head that is yawing, pitching and rolling at once moves a few
//				degrees between render and display. For each composition mode
//				and a spread of body angles, print how far the compositor's
//				correction misses. Zero means the picture is turned exactly the
//				way the scene moved and there is nothing left to bend it.
//
// ----------------------------------------------------------------------- //

void CGameClientShell::LogFrameAgreement()
{
	// Two head poses a few degrees apart, all three axes live in both. Every
	// earlier check of this used a pure rotation about ONE axis, which a real
	// headset pose never is - and a single-axis pose is exactly the case in
	// which the wrong answer looks right.
	struct Pose { float fYaw, fPitch, fRoll; };
	const Pose kPoses[3][2] = {
		{ {  20.0f, -15.0f,   6.0f }, {  23.0f, -13.0f,   6.5f } },
		{ { -35.0f,  22.0f,  -8.0f }, { -32.0f,  20.0f,  -8.5f } },
		{ {   5.0f,  30.0f,   0.0f }, {   6.0f,  31.5f,   0.3f } },
	};

	const float kBody[7][2] = {
		{   0.0f,   0.0f }, {  45.0f,   0.0f }, {  90.0f,  0.0f },
		{ 180.0f,   0.0f }, { 270.0f,   0.0f }, { 270.0f, 15.0f },
		{ 135.0f, -20.0f },
	};

	VRLog::Msg("--- frame agreement: how far the compositor's correction misses ---");
	VRLog::Msg("  degrees per frame of ordinary head motion. 0.00 = the picture is");
	VRLog::Msg("  turned exactly the way the scene moved, so nothing can bend it.");
	VRLog::Msg("  body yaw/pitch    mode 1   mode 2   mode 3");

	float fWorst[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	for (int b = 0; b < 7; ++b)
	{
		LTRotation rBody;
		g_pLTClient->SetupEuler(&rBody, DEG2RAD(kBody[b][1]), DEG2RAD(kBody[b][0]), 0.0f);

		float fRow[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		for (int p = 0; p < 3; ++p)
		{
			LTRotation rH[2];
			for (int k = 0; k < 2; ++k)
			{
				// The quaternion a headset reports, then the client's own
				// OpenXR-to-LithTech conversion. Nothing here is a shortcut.
				const float cy = (float)cos(DEG2RAD(kPoses[p][k].fYaw)   * 0.5f);
				const float sy = (float)sin(DEG2RAD(kPoses[p][k].fYaw)   * 0.5f);
				const float cp = (float)cos(DEG2RAD(kPoses[p][k].fPitch) * 0.5f);
				const float sp = (float)sin(DEG2RAD(kPoses[p][k].fPitch) * 0.5f);
				const float cr = (float)cos(DEG2RAD(kPoses[p][k].fRoll)  * 0.5f);
				const float sr = (float)sin(DEG2RAD(kPoses[p][k].fRoll)  * 0.5f);

				LTRotation qY, qP, qR;
				qY.Init(0.0f, sy, 0.0f, cy);
				qP.Init(sp, 0.0f, 0.0f, cp);
				qR.Init(0.0f, 0.0f, sr, cr);
				const LTRotation q = qY * qP * qR;

				rH[k].Init(-q.m_Quat[QX], -q.m_Quat[QY], q.m_Quat[QZ], q.m_Quat[QW]);
			}

			for (int m = 1; m <= 3; ++m)
			{
				const float d = FrameResidualDeg(rH[0], rH[1], rBody, m);
				if (d > fRow[m])   fRow[m]   = d;
				if (d > fWorst[m]) fWorst[m] = d;
			}
		}

		VRLog::Msg("  %6.0f / %-6.0f    %6.2f   %6.2f   %6.2f",
			kBody[b][0], kBody[b][1], fRow[1], fRow[2], fRow[3]);
	}

	VRLog::Msg("  worst:              %6.2f   %6.2f   %6.2f",
		fWorst[1], fWorst[2], fWorst[3]);
	VRLog::Msg("  the running mode is VRQuatHead %d", (int)g_vtVRQuatHead.GetFloat());
	VRLog::Msg("--- end frame agreement ---");
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::LogRotationConvention()
//
//	PURPOSE:	Settle, with numbers, what LTRotation's multiply actually does.
//
//				Three composition orders have now been guessed at and judged in
//				a headset, and two of them were wrong in ways that took a test
//				round each to discover. A wrong order and a wrong handedness
//				look identical through a headset - reversed pitch and a tilt
//				at the same time, reading as a diagonal - and neither can be told
//				from the other by feel.
//
//				The SDK header does not say which convention it uses. So ask
//				it: build a yaw and a pitch, multiply them both ways, and print
//				the basis vectors. Interpretation needs no judgement at all:
//
//				  - Yaw first, then pitch about the BODY's own right axis,
//				    leaves UP in the vertical plane. No roll.
//				  - Pitch first, then yaw about the WORLD's up axis, throws UP
//				    out of that plane. That is roll, and it is the tilt that
//				    has been reported twice.
//
//				Whichever product keeps up.x near zero after a 90 degree yaw is
//				"body then head", which is the one the camera wants.
//
// ----------------------------------------------------------------------- //

void CGameClientShell::LogRotationConvention()
{
	const float fR2D = 57.2957795f;

	LTRotation rYaw, rPitch;
	g_pLTClient->SetupEuler(&rYaw,   0.0f,            DEG2RAD(90.0f), 0.0f);
	g_pLTClient->SetupEuler(&rPitch, DEG2RAD(30.0f),  0.0f,           0.0f);

	LTRotation rYP = rYaw * rPitch;
	LTRotation rPY = rPitch * rYaw;

	VRLog::Msg("--- LTRotation convention (yaw 90, pitch 30) ---");

	struct { const char* pszName; LTRotation* pRot; } tests[] = {
		{ "yaw90 alone   ", &rYaw   },
		{ "pitch30 alone ", &rPitch },
		{ "yaw * pitch   ", &rYP    },
		{ "pitch * yaw   ", &rPY    },
	};

	for (int i = 0; i < 4; ++i)
	{
		LTVector vU, vR, vF;
		g_pLTClient->GetRotationVectors(tests[i].pRot, &vU, &vR, &vF);
		VRLog::Msg("  %s fwd(%+.3f %+.3f %+.3f)  up(%+.3f %+.3f %+.3f)  right(%+.3f %+.3f %+.3f)",
			tests[i].pszName, vF.x, vF.y, vF.z, vU.x, vU.y, vU.z, vR.x, vR.y, vR.z);
	}

	// Roll is how far UP leans out of the vertical plane that contains FORWARD.
	//
	// The first version of this measured |up.x| and called it roll. That is not
	// roll, it is just a component, and after a 90 degree yaw it reports the
	// exact opposite of the truth - which is how the default came to be changed
	// to a mode that swaps pitch and roll.
	{
		for (int i = 2; i < 4; ++i)
		{
			LTVector vU, vR, vF;
			g_pLTClient->GetRotationVectors(tests[i].pRot, &vU, &vR, &vF);

			// horizontal right = worldUp x forward
			LTVector vHR;
			vHR.x = vF.z; vHR.y = 0.0f; vHR.z = -vF.x;
			const float fLen = (float)sqrt(vHR.x * vHR.x + vHR.z * vHR.z);

			float fRoll = 0.0f;
			if (fLen > 1e-4f)
			{
				vHR.x /= fLen; vHR.z /= fLen;
				float d = vU.x * vHR.x + vU.z * vHR.z;
				if (d >  1.0f) d =  1.0f;
				if (d < -1.0f) d = -1.0f;
				fRoll = (float)asin(d) * fR2D;
			}
			const float fPitch = (float)asin((vF.y > 1.0f) ? 1.0f : ((vF.y < -1.0f) ? -1.0f : vF.y)) * fR2D;

			VRLog::Msg("  %s -> forward pitch %+.1f deg, ROLL %+.1f deg%s",
				tests[i].pszName, fPitch, fRoll,
				(fabs(fRoll) < 1.0f) ? "   <- pitch stays pitch: this is body-then-head"
									 : "   <- pitch became ROLL: head is in the wrong frame");
		}
		VRLog::Msg("  the shipped VRQuatHead 1 uses rBody * rHead, which is yaw * pitch above");
	}

	// And what the head quaternion conversion actually produces. A known
	// OpenXR rotation in, Euler angles out, so a mirrored axis is visible as a
	// sign rather than as a feeling.
	{
		// 30 degrees pitch UP in OpenXR (right-handed, -Z forward, +X right):
		// a positive rotation about +X tips -Z toward +Y.
		const float a = DEG2RAD(30.0f) * 0.5f;
		const float qx = (float)sin(a), qw = (float)cos(a);

		LTRotation rHead;
		rHead.Init(-qx, 0.0f, 0.0f, qw);		// the Z-flip the client applies

		LTVector vU, vR, vF;
		g_pLTClient->GetRotationVectors(&rHead, &vU, &vR, &vF);
		VRLog::Msg("  OpenXR pitch +30 (looking UP) -> LithTech fwd(%+.3f %+.3f %+.3f)",
			vF.x, vF.y, vF.z);
		VRLog::Msg("  -> the view is looking %s. Positive fwd.y is up.",
			(vF.y > 0.05f) ? "UP - correct"
						   : ((vF.y < -0.05f) ? "DOWN - the pitch axis is MIRRORED" : "level - no pitch applied"));
	}

	VRLog::Msg("--- end convention test ---");
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateHeadAsMouse()
//
//	PURPOSE:	VR experiment - turn the head with the mouse-look path.
//
//				Instead of composing the head rotation onto the camera at
//				render time, add each frame's head MOVEMENT to m_fYaw and
//				m_fPitch, the same two variables the mouse writes. From there
//				everything downstream is the mouse's path, byte for byte: the
//				camera rotation is built by UpdateCameraRotation, the body
//				turns with the aim, and the weapon points where the game
//				thinks the player is looking.
//
//				The player's most durable observation about the warping is that
//				MOUSE look feels correct and HEAD look does not. Only two
//				things differ between them, so this changes both at once and
//				on purpose:
//
//				  1. the camera path - fixed here, head look now takes the
//				     mouse's;
//				  2. the runtime's reprojection - a still head gives it nothing
//				     to correct, which is removed by the host submitting
//				     head-locked while this is on.
//
//				If the bending survives that, it is in the client - the render
//				itself, the FOV we ask d3d.ren for, or the per-eye geometry.
//				If it goes, it is in what we declare to the runtime. Both
//				answers are worth having; neither has been obtainable by
//				looking at the two halves together, which is what every attempt
//				so far has done.
//
//				DELTAS, not absolutes. Adding the head's absolute angle to the
//				mouse's would be equivalent, but only until the pitch clamp or
//				a cutscene moves m_fPitch underneath us. Deltas compose with
//				anything else that writes those variables, which is what makes
//				this "the mouse path" rather than "something alongside it".
//
// ----------------------------------------------------------------------- //

void CGameClientShell::UpdateHeadAsMouse()
{
	const bool bWant = (g_vtVRHeadAsMouse.GetFloat() > 0.0f)
					&& (g_vtVRHeadTracking.GetFloat() > 0.0f)
					&& VRShared::IsLive();

	if (!bWant)
	{
		// Give back everything the head contributed, so switching the
		// experiment off in-headset returns the aim to where the mouse alone
		// would have left it. Without this, A/B'ing with F1 leaves the head's
		// current offset baked into the body aim and then applies it again on
		// top at render time - the view would jump on every toggle and the
		// comparison would be between two different aims.
		if (m_fVRHeadAccumYaw != 0.0f || m_fVRHeadAccumPitch != 0.0f)
		{
			m_fYaw   -= m_fVRHeadAccumYaw;
			m_fPitch -= m_fVRHeadAccumPitch;
			m_fVRHeadAccumYaw   = 0.0f;
			m_fVRHeadAccumPitch = 0.0f;
		}
		m_bVRHeadRefValid = LTFALSE;
		return;
	}

	const VRSharedState& s = VRShared::State();
	{
		static uint32_t s_nGenHM = 0;
		static int      s_nSettleHM = 0;
		if (VRShared::RecenterGeneration() != s_nGenHM) { s_nGenHM = VRShared::RecenterGeneration(); s_nSettleHM = 6; }
		if (s_nSettleHM > 0) { --s_nSettleHM; m_bVRHeadRefValid = LTFALSE; }
	}
	if (!m_bVRHeadRefValid)
	{
		m_fVRHeadPrevYawDeg   = s.fHeadYawDeg;
		m_fVRHeadPrevPitchDeg = s.fHeadPitchDeg;
		m_bVRHeadRefValid     = LTTRUE;
		return;
	}

	float fDYaw   = s.fHeadYawDeg   - m_fVRHeadPrevYawDeg;
	float fDPitch = s.fHeadPitchDeg - m_fVRHeadPrevPitchDeg;

	// Yaw is reported in (-180, 180], so a turn through the back of the head
	// reads as a 350 degree jump in the wrong direction.
	while (fDYaw >  180.0f) fDYaw -= 360.0f;
	while (fDYaw < -180.0f) fDYaw += 360.0f;

	m_fVRHeadPrevYawDeg   = s.fHeadYawDeg;
	m_fVRHeadPrevPitchDeg = s.fHeadPitchDeg;

	// A step this large in one frame is not a head - it is the reference going
	// stale across a menu, a loading screen or a cutscene, all of which stop
	// this function being called while the player keeps moving. Re-baseline
	// rather than snapping the world round.
	const float kMaxStepDeg = 45.0f;
	if (fabs(fDYaw) > kMaxStepDeg || fabs(fDPitch) > kMaxStepDeg)
	{
		VRLog::Msg("head-as-mouse: ignored a %.0f/%.0f deg step (reference went stale)",
			fDYaw, fDPitch);
		return;
	}

	// Same sign convention as the render path, and the same two console
	// variables, so a wrong axis can still be corrected without a rebuild.
	const float fYawRad   = DEG2RAD(fDYaw   * g_vtVRYawScale.GetFloat());
	const float fPitchRad = DEG2RAD(fDPitch * g_vtVRPitchScale.GetFloat());

	m_fYaw   += fYawRad;
	m_fVRHeadAccumYaw += fYawRad;

	// Pitch is bounded just below straight up, and the caller clamps it a few
	// lines below this. Apply the same bound here and bank only what actually
	// went in, so the accumulator stays a true record of the head's
	// contribution - otherwise looking up past the limit builds a debt that is
	// paid back as a jump the moment F1 is pressed, in the middle of the very
	// comparison this exists to make clean.
	const float fPitchLimit = MATH_HALFPI - 0.1f;
	const float fWanted = m_fPitch + fPitchRad;
	const float fGiven  = (fWanted >  fPitchLimit) ?  fPitchLimit
						: (fWanted < -fPitchLimit) ? -fPitchLimit : fWanted;

	m_fVRHeadAccumPitch += fGiven - m_fPitch;
	m_fPitch = fGiven;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::CalculateCameraRotation()
//
//	PURPOSE:	Calculate the new camera rotation
//
// ----------------------------------------------------------------------- //

void CGameClientShell::CalculateCameraRotation()
{
	CGameSettings* pSettings = m_InterfaceMgr.GetSettings();
	if (!pSettings) return;

    LTBOOL bIsVehicle = m_MoveMgr.GetVehicleMgr()->IsVehiclePhysics();

    LTFLOAT fVal = 1.0f + (LTFLOAT)(3 * m_nZoomView);

	// Get axis offsets...
	float offsets[3] = {0.0, 0.0, 0.0};

	if (!g_vtOldMouseLook.GetFloat())
	{
		int deltaX, deltaY;

		SDL_PumpEvents();

		// Firstly, we need a point of reference.
		// This conditional is here, in case we need to reset the mouse.
		if (m_bGetBaseMouse)
		{
			SDL_GetMouseState(&m_iCurrentMouseX, &m_iCurrentMouseY);
			m_bGetBaseMouse = LTFALSE;
		}

		SDL_GetRelativeMouseState(&deltaX, &deltaY);

		m_iCurrentMouseX += deltaX;
		m_iCurrentMouseY += deltaY;

		// TODO: Clean up, Code is from GameSettings.
		float nMouseSensitivity = GetConsoleFloat("MouseSensitivity", 1.0f);
		float nScale = 0.00125f + ((float)nMouseSensitivity * 0.001125f);
		// Scale this down because lower ends are still too fast!
		nScale *= 0.50f;

		offsets[0] = (float)(m_iCurrentMouseX - m_iPreviousMouseX) * nScale;
		offsets[1] = (float)(m_iCurrentMouseY - m_iPreviousMouseY) * nScale;

		m_iPreviousMouseX = m_iCurrentMouseX;
		m_iPreviousMouseY = m_iCurrentMouseY;
	}
	else
	{
		g_pLTClient->GetAxisOffsets(offsets);
	}

	if (m_bRestoreOrientation)
	{
		memset(offsets, 0, sizeof(float) * 3);
        m_bRestoreOrientation = LTFALSE;
	}

	if (m_bStrafing)
	{
		m_MoveMgr.UpdateMouseStrafeFlags(offsets);
	}

   	LTFLOAT fYawDelta    = offsets[0] / fVal;
    LTFLOAT fPitchDelta  = offsets[1] / fVal;

	m_fYaw += fYawDelta;

	// [kml] 12/26/00 Check varying degrees of strage and look.
	if(!(m_dwPlayerFlags & BC_CFLG_STRAFE))
	{
		if(m_dwPlayerFlags & BC_CFLG_LEFT)
		{
			m_fYaw -= m_fFrameTime * ((m_dwPlayerFlags & BC_CFLG_RUN) ? g_vtFastTurnRate.GetFloat() : g_vtNormalTurnRate.GetFloat());
		}

		if(m_dwPlayerFlags & BC_CFLG_RIGHT)
		{
			m_fYaw += m_fFrameTime * ((m_dwPlayerFlags & BC_CFLG_RUN) ? g_vtFastTurnRate.GetFloat() : g_vtNormalTurnRate.GetFloat());
		}
	}

	if (pSettings->MouseLook() || (m_dwPlayerFlags & BC_CFLG_LOOKUP) || (m_dwPlayerFlags & BC_CFLG_LOOKDOWN)
		|| m_bHoldingMouseLook)
    {
        if (pSettings->MouseLook() || m_bHoldingMouseLook)
		{
			if (pSettings->MouseInvertY())
			{
				m_fPitch -= fPitchDelta;
			}
			else
			{
				m_fPitch += fPitchDelta;
			}
		}

		if(m_dwPlayerFlags & BC_CFLG_LOOKUP)
		{
			m_fPitch -= m_fFrameTime * g_vtLookUpRate.GetFloat();
		}

		if(m_dwPlayerFlags & BC_CFLG_LOOKDOWN)
		{
			m_fPitch += m_fFrameTime * g_vtLookUpRate.GetFloat();
		}

		// Don't allow much movement up/down if 3rd person...

		if (!m_PlayerCamera.IsFirstPerson())
		{
            LTFLOAT fMinY = DEG2RAD(45.0f) - 0.1f;

			if (m_fPitch < -fMinY) m_fPitch = -fMinY;
			if (m_fPitch > fMinY)  m_fPitch = fMinY;
		}
	}
	else if (m_fPitch != 0.0f && pSettings->Lookspring())
	{
        LTFLOAT fPitchDelta = (m_fFrameTime * g_vtLookUpRate.GetFloat());
		if (m_fPitch > 0.0f) m_fPitch -= min(fPitchDelta, m_fPitch);
		if (m_fPitch < 0.0f) m_fPitch += min(fPitchDelta, -m_fPitch);
	}

	// VR experiment: the head turns the player the same way the mouse does.
	//
	// Placed after the mouse block rather than inside it so it does not depend
	// on the MouseLook setting, and before the clamp below so head pitch is
	// bounded exactly as mouse pitch is.
	UpdateHeadAsMouse();

    LTFLOAT fMinY = MATH_HALFPI - 0.1f;

	if (m_fPitch < -fMinY) m_fPitch = -fMinY;
	if (m_fPitch > fMinY)  m_fPitch = fMinY;


	// Set camera and player variables...

	// WHERE THE PLAYER IS, once a second, for the desk. VRDebugPos puts it
	// on the HUD; a harness reads logs. Off unless asked for.
	if (g_vtVRLogPos.GetFloat() > 0.0f)
	{
		static float s_fSaid = -100.0f;
		const float fNow = g_pLTClient->GetTime();
		if (fNow - s_fSaid >= 1.0f)
		{
			s_fSaid = fNow;
			LTVector vMe(0, 0, 0);
			if (g_pLTClient->GetClientObject()) g_pLTClient->GetObjectPos(g_pLTClient->GetClientObject(), &vMe);
			VRLog::Msg("VRPos: player (%.0f %.0f %.0f) body yaw %.1f deg%s",
				vMe.x, vMe.y, vMe.z, RAD2DEG(m_fYaw), bIsVehicle ? " ON VEHICLE" : "");
		}
	}

	// Only use mouse values for yaw if the player isn't on a vehicle...

	if (bIsVehicle)
	{
		// Can't look up/down on vehicles...

		m_fPitch = 0.0f;

		LTVector vPlayerPYR(m_fPlayerPitch, m_fPlayerYaw, m_fPlayerRoll);
		LTVector vPYR(m_fPitch, m_fYaw, m_fRoll);

		m_MoveMgr.GetVehicleMgr()->CalculateVehicleRotation(vPlayerPYR, vPYR, fYawDelta);

		m_fPlayerPitch	= vPlayerPYR.x;
		m_fPlayerYaw	= vPlayerPYR.y;
		m_fPlayerRoll	= vPlayerPYR.z;

		m_fPitch		= vPYR.x;
		m_fYaw			= vPYR.y;
		m_fRoll			= vPYR.z;
	}
	else  // Not vehicle...
	{
		m_fPlayerPitch	= 0.0f;
		m_fPlayerYaw	= m_fYaw;
		m_fPlayerRoll	= m_fRoll;
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateCameraRotation()
//
//	PURPOSE:	Set the new camera rotation
//
// ----------------------------------------------------------------------- //

LTBOOL CGameClientShell::UpdateCameraRotation()
{
    HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();
    if (!hPlayerObj) return LTFALSE;

	// Update camera orientation vars...

    LTVector vPitchYawRollDelta = m_CameraOffsetMgr.GetPitchYawRollDelta();

	if (m_bUsingExternalCamera)
	{
		// Just calculate the correct player rotation...

        g_pLTClient->SetupEuler(&m_rRotation, m_fPitch, m_fYaw, m_fRoll);
	}
	else if (m_PlayerCamera.IsFirstPerson())
	{
		if (!m_bCameraAttachedToHead)
		{
			float fPitch = m_fPitch + vPitchYawRollDelta.x;
			float fYaw   = m_fYaw	+ vPitchYawRollDelta.y;
			float fRoll  = m_fRoll  + vPitchYawRollDelta.z;

			LTRotation rNewRot, rOldRot;
			g_pLTClient->SetupEuler(&rNewRot, fPitch, fYaw, fRoll);
			g_pLTClient->GetObjectRotation(m_hCamera, &rOldRot);

			m_rRotation = rNewRot;
		}

	    g_pLTClient->SetObjectRotation(m_hCamera, &m_rRotation);
	}
	else
	{
		// Set the camera to use the rotation calculated by the player camera,
		// however we still need to calculate the correct rotation to be sent
		// to the player...

        LTFLOAT fAdjust = DEG2RAD(g_vtChaseCamPitchAdjust.GetFloat());
        g_pLTClient->EulerRotateX(&m_rRotation, m_fPitch + fAdjust);
 		g_pLTClient->SetObjectRotation(m_hCamera, &m_rRotation);

		// Okay, now calculate the correct player rotation...

        g_pLTClient->SetupEuler(&m_rRotation, m_fPitch, m_fYaw, m_fRoll);
	}

    return LTTRUE;
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::SetCameraFOV
//
//	PURPOSE:	Set the camera's FOV
//
// --------------------------------------------------------------------------- //

void CGameClientShell::SetCameraFOV(LTFLOAT fFovX, LTFLOAT fFovY)
{
	if (!m_hCamera) return;

    g_pLTClient->SetCameraFOV(m_hCamera, fFovX, fFovY);
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateCameraZoom
//
//	PURPOSE:	Update the camera's field of view
//
// --------------------------------------------------------------------------- //

void CGameClientShell::UpdateCameraZoom()
{
	char strConsole[30];

    uint32 dwWidth = 640, dwHeight = 480;
    g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &dwWidth, &dwHeight);

    LTFLOAT fovX, fovY;
    g_pLTClient->GetCameraFOV(m_hCamera, &fovX, &fovY);

    LTFLOAT fOldFovX = fovX;

	if (!fovX)
	{
		fovX = DEG2RAD(g_vtFOVXNormal.GetFloat());
	}

    m_bZooming = LTTRUE;

	// IN VR THE EYES NEVER ZOOM. The level is kept (the scope's third pass
	// reads it) and the camera's field is left alone: a narrowed camera
	// field would be a magnified world in both eyes, which is the one thing
	// a headset must not do. The lens disc is the zoom.
	if (VRShared::IsLive())
	{
		m_bZooming = LTFALSE;
		m_InterfaceMgr.EndZoom();
		return;
	}

    LTFLOAT fFovXZoomed, fZoomDist;

	if (m_bZoomingIn)
	{
		if (m_nZoomView == 1)
		{
			fFovXZoomed	= DEG2RAD(FOVX_ZOOMED);
			fZoomDist	= DEG2RAD(g_vtFOVXNormal.GetFloat()) - fFovXZoomed;
		}
		else if (m_nZoomView == 2)
		{
			fFovXZoomed	= DEG2RAD(FOVX_ZOOMED1);
			fZoomDist	= DEG2RAD(FOVX_ZOOMED) - fFovXZoomed;
		}
		else if (m_nZoomView == 3)
		{
			fFovXZoomed	= DEG2RAD(FOVX_ZOOMED2);
			fZoomDist	= DEG2RAD(FOVX_ZOOMED1) - fFovXZoomed;
		}
	}
	else
	{
		if (m_nZoomView == 0)
		{
			fFovXZoomed	= DEG2RAD(g_vtFOVXNormal.GetFloat());
			fZoomDist	= DEG2RAD(g_vtFOVXNormal.GetFloat()) - DEG2RAD(FOVX_ZOOMED);
		}
		else if (m_nZoomView == 1)
		{
			fFovXZoomed	= DEG2RAD(FOVX_ZOOMED);
			fZoomDist	= fFovXZoomed - DEG2RAD(FOVX_ZOOMED1);
		}
		else if (m_nZoomView == 2)
		{
			fFovXZoomed	= DEG2RAD(FOVX_ZOOMED1);
			fZoomDist	= fFovXZoomed - DEG2RAD(FOVX_ZOOMED2);
		}
	}

    LTFLOAT fZoomVel = fZoomDist / ZOOM_TIME;
    LTFLOAT fZoomAmount = fZoomVel * m_fFrameTime;

	// Zoom camera in or out...

	if (m_bZoomingIn)
	{
		if (fovX > fFovXZoomed)
		{
			// Zoom camera in...

			fovX -= fZoomAmount;
		}

		if (fovX <= fFovXZoomed)
		{
			fovX = fFovXZoomed;
            m_bZooming = LTFALSE;
			m_InterfaceMgr.EndZoom();
		}
	}
	else  // Zoom camera out...
	{
		if (fovX < fFovXZoomed)
		{
			// Zoom camera out...

			fovX += fZoomAmount;
		}

		if (fovX >= fFovXZoomed)
		{
			fovX = fFovXZoomed;
            m_bZooming = LTFALSE;
			m_InterfaceMgr.EndZoom();
			if (m_nZoomView == 0)
			{
				EndZoom();
			}
		}
	}

	if (fOldFovX != fovX && dwWidth && dwHeight)
	{
		fovY = (fovX * DEG2RAD(g_vtFOVYNormal.GetFloat())) / DEG2RAD(g_vtFOVXNormal.GetFloat());

		SetCameraFOV(fovX, fovY);

		// Update the lod adjustment for models...
		LTFLOAT fZoomAmount = (DEG2RAD(g_vtFOVXNormal.GetFloat()) - fovX) / (DEG2RAD(g_vtFOVXNormal.GetFloat()) - DEG2RAD(FOVX_ZOOMED2));
		LTFLOAT fNewLODOffset = m_fSaveLODScale + (LOD_ZOOMADJUST * fZoomAmount);

		sprintf(strConsole, "+ModelLODOffset %f", fNewLODOffset);
        g_pLTClient->RunConsoleString(strConsole);

        //g_pLTClient->CPrint("Current FOV (%f, %f)", fovX, fovY);
        //g_pLTClient->CPrint("Current Zoom LODOffset: %f", fNewLODOffset);
	}

}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::InCameraGadgetRange
//
//	PURPOSE:	See if the given object is in the camera gadget's range
//
// --------------------------------------------------------------------------- //

LTBOOL CGameClientShell::InCameraGadgetRange(HOBJECT hObj)
{
	if (!hObj || !m_hCamera) return LTFALSE;

	if (m_weaponModel.IsDisabled()) return LTFALSE;

	LTVector vCamPos, vObjPos;
	g_pLTClient->GetObjectPos(m_hCamera, &vCamPos);
	g_pLTClient->GetObjectPos(hObj, &vObjPos);

	LTVector vDist = vCamPos - vObjPos;
	LTFLOAT fDist = vDist.Mag();

	// g_pLTClient->CPrint("fDist = %.2f, Zoom View = %d", fDist, m_nZoomView);

	if (fDist < g_vtSunZoomLevel1MaxDist.GetFloat())
	{
		return LTTRUE;
	}
	else if (fDist < g_vtSunZoomLevel2MaxDist.GetFloat())
	{
		if (m_nZoomView > 0) return LTTRUE;
	}
	else
	{
		if (m_nZoomView > 1) return LTTRUE;
	}

	// Not zoomed in enough...
	return LTFALSE;
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateCameraShake
//
//	PURPOSE:	Update the camera's shake
//
// --------------------------------------------------------------------------- //

void CGameClientShell::UpdateCameraShake()
{
	// Decay...

    LTFLOAT fDecayAmount = 2.0f * m_fFrameTime;

	m_vShakeAmount.x -= fDecayAmount;
	m_vShakeAmount.y -= fDecayAmount;
	m_vShakeAmount.z -= fDecayAmount;

	if (m_vShakeAmount.x < 0.0f) m_vShakeAmount.x = 0.0f;
	if (m_vShakeAmount.y < 0.0f) m_vShakeAmount.y = 0.0f;
	if (m_vShakeAmount.z < 0.0f) m_vShakeAmount.z = 0.0f;


	if (m_vShakeAmount.x <= 0.0f && m_vShakeAmount.y <= 0.0f && m_vShakeAmount.z <= 0.0f) return;


	// Apply...

    LTFLOAT faddX = GetRandom(-1.0f, 1.0f) * m_vShakeAmount.x * 3.0f;
    LTFLOAT faddY = GetRandom(-1.0f, 1.0f) * m_vShakeAmount.y * 3.0f;
    LTFLOAT faddZ = GetRandom(-1.0f, 1.0f) * m_vShakeAmount.z * 3.0f;

	// Don't update the camera's position if the interface
	// doesn't want us to...

 	if (!m_InterfaceMgr.AllowCameraMovement()) return;

    LTVector vPos, vAdd;
	vAdd.Init(faddX, faddY, faddZ);

    g_pLTClient->GetObjectPos(m_hCamera, &vPos);
	vPos += vAdd;

    g_pLTClient->SetObjectPos(m_hCamera, &vPos);

	HLOCALOBJ hWeapon = m_weaponModel.GetHandle();
	if (!hWeapon) return;

    g_pLTClient->GetObjectPos(hWeapon, &vPos);

	vAdd += 0.95f;
	vPos += vAdd;
    g_pLTClient->SetObjectPos(hWeapon, &vPos);
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateScreenFlash
//
//	PURPOSE:	Update the screen flash
//
// --------------------------------------------------------------------------- //

void CGameClientShell::UpdateScreenFlash()
{
	if (!m_bFlashScreen) return;

    LTVector vLightAdd;
	VEC_SET(vLightAdd, 0.0f, 0.0f, 0.0f);

    LTFLOAT fTime  = g_pLTClient->GetTime();
	if ((m_fFlashRampUp > 0.0f) && (fTime < m_fFlashStart + m_fFlashRampUp))
	{
        LTFLOAT fDelta = (fTime - m_fFlashStart);
		vLightAdd.x = fDelta * (m_vFlashColor.x) / m_fFlashRampUp;
		vLightAdd.y = fDelta * (m_vFlashColor.y) / m_fFlashRampUp;
		vLightAdd.z = fDelta * (m_vFlashColor.z) / m_fFlashRampUp;
	}
	else if (fTime < m_fFlashStart + m_fFlashRampUp + m_fFlashTime)
	{
		VEC_COPY(vLightAdd, m_vFlashColor);
	}
	else if ((m_fFlashRampDown > 0.0f) && (fTime < m_fFlashStart + m_fFlashRampUp + m_fFlashTime + m_fFlashRampDown))
	{
        LTFLOAT fDelta = (fTime - (m_fFlashStart + m_fFlashRampUp + m_fFlashTime));

		vLightAdd.x = m_vFlashColor.x - (fDelta * (m_vFlashColor.x) / m_fFlashRampUp);
		vLightAdd.y = m_vFlashColor.y - (fDelta * (m_vFlashColor.y) / m_fFlashRampUp);
		vLightAdd.z = m_vFlashColor.z - (fDelta * (m_vFlashColor.z) / m_fFlashRampUp);
	}
	else
	{
        m_bFlashScreen = LTFALSE;
	}

	// Make sure values are in range...

	vLightAdd.x = (vLightAdd.x < 0.0f ? 0.0f : (vLightAdd.x > 1.0f ? 1.0f : vLightAdd.x));
	vLightAdd.y = (vLightAdd.y < 0.0f ? 0.0f : (vLightAdd.y > 1.0f ? 1.0f : vLightAdd.y));
	vLightAdd.z = (vLightAdd.z < 0.0f ? 0.0f : (vLightAdd.z > 1.0f ? 1.0f : vLightAdd.z));

	m_ScreenTintMgr.Set(TINT_SCREEN_FLASH,&vLightAdd);
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ClearScreenTint
//
//	PURPOSE:	Clear any screen tinting
//
// --------------------------------------------------------------------------- //

void CGameClientShell::ClearScreenTint()
{
	m_ScreenTintMgr.ClearAll();
	if (m_bFlashScreen)
	{
        m_bFlashScreen = LTFALSE;
	}

	m_LightScaleMgr.Term();
	m_LightScaleMgr.Init();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateWeaponModel()
//
//	PURPOSE:	Update the weapon model
//
// ----------------------------------------------------------------------- //

void CGameClientShell::UpdateWeaponModel()
{
    HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();
	if (!hPlayerObj) return;


	// Decay weapon recoil...

	DecayWeaponRecoil();


	// If possible, get these values from the camera, because it
	// is more up-to-date...

    LTRotation rRot;
	rRot.Init();
    LTVector vPos(0, 0, 0);

	// Weapon model pos/rot is relative to camera now...
    g_pLTClient->GetObjectPos(m_hCamera, &vPos);
    g_pLTClient->GetObjectRotation(m_hCamera, &rRot);

	if (!m_PlayerCamera.IsFirstPerson() || m_bUsingExternalCamera)
	{
		// Use the gun's flash orientation...

		GetAttachmentSocketTransform(hPlayerObj, "Flash", vPos, rRot);
	}


	// If we aren't dead, and we aren't in the middle of changing weapons,
	// let us fire.

	FireType eFireType = FT_NORMAL_FIRE;
    LTBOOL bFire = LTFALSE;

	// Only check if we're firing if we aren't choosing ammo/weapons...
	if (!m_InterfaceMgr.IsChoosingAmmo() && ( !m_InterfaceMgr.IsChoosingWeapon() || g_vtQuickSwitch.GetFloat() ))
	{
		if ((m_dwPlayerFlags & BC_CFLG_FIRING) || (m_dwPlayerFlags & BC_CFLG_ALT_FIRING) &&
			!IsPlayerDead() && !m_bSpectatorMode)
		{
            bFire = LTTRUE;
			// Disable alt-fire
			//eFireType = (m_dwPlayerFlags & BC_CFLG_ALT_FIRING) ? FT_ALT_FIRE : FT_NORMAL_FIRE;

			if (m_dwPlayerFlags & BC_CFLG_ALT_FIRING)
			{
                bFire = LTFALSE;
			}
		}
	}


	// Can't fire when asleep...

	//if (m_DamageFXMgr.IsSleeping())
	//{
	//	bFire = LTFALSE;
	//}


	// Update the model position and state...
	WeaponState eWeaponState = m_weaponModel.UpdateWeaponModel(rRot, vPos, bFire, eFireType);


	// Do fire camera jitter...

	if (FiredWeapon(eWeaponState) && m_PlayerCamera.IsFirstPerson())
	{
		StartWeaponRecoil();
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::StartWeaponRecoil()
//
//	PURPOSE:	Start the weapon recoiling...
//
// ----------------------------------------------------------------------- //

void CGameClientShell::StartWeaponRecoil()
{
	// NO CAMERA KICK IN VR. Fifth headset test: the recoil bobbed the view up
	// and down, which a VR game cannot have. The kick is a pitch
	// written into the player's view and a screen shake; the gun is in the
	// hand now, so the recoil belongs to the hand, not the eyes. VRRecoil 1
	// puts it back.
	if (VRShared::IsLive() && g_vtVRRecoil.GetFloat() <= 0.0f) return;

	// Shake the screen if it isn't shaking...

	if (m_vShakeAmount.x < 0.1f &&
		m_vShakeAmount.y < 0.1f &&
		m_vShakeAmount.z < 0.1f)
	{
        LTVector vShake(0.1f, 0.1f, 0.1f);
		ShakeScreen(vShake);
	}

	if (g_vtUseCamRecoil.GetFloat() > 0.0f)
	{
		// Move view up a bit...

        LTFLOAT fPCorrect = 1.0f - (m_fFireJitterPitch / DEG2RAD(g_vtMaxCamRecoilPitch.GetFloat()));
        LTFLOAT fPVal = GetRandom(0.5f,2.0f) * DEG2RAD(g_vtBaseCamRecoilPitch.GetFloat()) * fPCorrect;
		m_fFireJitterPitch += fPVal;
		m_fPitch -= fPVal;

		// Move view left/right a bit...

        LTFLOAT fYawDiff = (LTFLOAT)fabs(m_fFireJitterYaw);
        LTFLOAT fYCorrect = 1.0f - (fYawDiff / DEG2RAD(g_vtMaxCamRecoilYaw.GetFloat()));
        LTFLOAT fYVal = GetRandom(-2.0f,2.0f) * DEG2RAD(g_vtBaseCamRecoilYaw.GetFloat()) * fYCorrect;
		m_fFireJitterYaw += fYVal;
		m_fYaw -= fYVal;
	}
	else
	{
		// Move view up a bit...(based on the current weapon/ammo type)

		WEAPON* pWeaponData = m_weaponModel.GetWeapon();
		AMMO*	pAmmoData	= m_weaponModel.GetAmmo();

        LTFLOAT fMaxPitch = g_vtFireJitterMaxPitchDelta.GetFloat();

		if (pWeaponData && pAmmoData)
		{
			if (fMaxPitch < 0.0f)
			{
				fMaxPitch = pWeaponData->fFireRecoilPitch * pAmmoData->fFireRecoilMult;
			}
		}

        LTFLOAT fVal = (LTFLOAT)DEG2RAD(fMaxPitch) - m_fFireJitterPitch;
		m_fFireJitterPitch += fVal;
		m_fPitch -= fVal;
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::DecayWeaponRecoil()
//
//	PURPOSE:	Decay the weapon's recoil...
//
// ----------------------------------------------------------------------- //

void CGameClientShell::DecayWeaponRecoil()
{
	// Decay firing jitter if necessary...

	if (g_vtUseCamRecoil.GetFloat() > 0.0f)
	{
		if (m_fFireJitterPitch > 0.0f)
		{
            LTFLOAT fCorrect = 1.0f + m_fFireJitterPitch / DEG2RAD(g_vtMaxCamRecoilPitch.GetFloat());
            LTFLOAT fVal = (m_fFrameTime * g_vtCamRecoilRecover.GetFloat()) * fCorrect;

			if (m_fFireJitterPitch < fVal)
			{
				fVal = m_fFireJitterPitch;
			}

			m_fFireJitterPitch -= fVal;
			m_fPitch += fVal;
		}

        LTFLOAT fYawDiff = (LTFLOAT)fabs(m_fFireJitterYaw);
		if (fYawDiff > 0.0f)
		{
            LTFLOAT fCorrect = 1.0f + fYawDiff / DEG2RAD(g_vtMaxCamRecoilYaw.GetFloat());
            LTFLOAT fVal = (m_fFrameTime * g_vtCamRecoilRecover.GetFloat()) * fCorrect;

			if (fYawDiff < fVal)
			{
				fVal = fYawDiff;
			}
			if (m_fFireJitterYaw < 0.0f)
				fVal *= -1.0f;

			m_fFireJitterYaw -= fVal;
			m_fYaw += fVal;
		}
	}
	else
	{
		if (m_fFireJitterPitch > 0.0f)
		{
            LTFLOAT fVal = m_fFireJitterPitch;

			WEAPON* pWeaponData = m_weaponModel.GetWeapon();
			AMMO*	pAmmoData	= m_weaponModel.GetAmmo();

            LTFLOAT fMaxPitch  = g_vtFireJitterMaxPitchDelta.GetFloat();
            LTFLOAT fTotalTime = g_vtFireJitterDecayTime.GetFloat();

			if (pWeaponData && pAmmoData)
			{
				if (fMaxPitch < 0.0f)
				{
					fMaxPitch = pWeaponData->fFireRecoilPitch * pAmmoData->fFireRecoilMult;
				}

				if (fTotalTime < 0.0f)
				{
					fTotalTime = pWeaponData->fFireRecoilDecay;
				}
			}

			if (fTotalTime > 0.01)
			{
				fVal = m_fFrameTime * (DEG2RAD(fMaxPitch) / fTotalTime);
			}

			if (m_fFireJitterPitch - fVal < 0.0f)
			{
				fVal = m_fFireJitterPitch;
			}

			m_fFireJitterPitch -= fVal;
			m_fPitch += fVal;
		}

	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateVRAimMarker()
//
//	PURPOSE:	Put a mark in the world where the GUN is pointing.
//
//				Built from CWeaponModel::GetFireInfo, which is the same choke
//				point the bullet, the impact FX and the server message are all
//				built from. Anything else here would be a second opinion about
//				where the shot goes, and a crosshair that lies is worse than
//				no crosshair - the player trusts it and misses.
//
//				The art is SFX/Flares/Red1.spr -> FLR0032.DTX, measured as a
//				red glow on a BLACK field with no alpha channel at all. That
//				is additive art, so it is created with FLAG2_ADDITIVE: added
//				to the scene it is a laser dot, and blended as alpha it would
//				be a black square. The game ships no Crosshair.spr - the 3rd
//				person routine below names one and it is not in any archive,
//				which is part of why that routine returns early.
//
// ----------------------------------------------------------------------- //

// The aim ray's filter. Rejects the player's own objects, as the plain
// list filter did, AND any dead body.
//
// In the headset, over an enemy lying dead on the ground, the red dot acted
// as if they were standing - it appeared larger and where they died, not
// further back on the wall. The ray tests OBJECT BOUNDING BOXES, and a
// body keeps a standing-sized box lying down, so the dot lands on an
// invisible column of air over the corpse. Bodies are their own objects with
// a CBodyFX behind them, which is how they are told apart here.
static LTBOOL VRAimFilterFn(HOBJECT hTest, void* pUserData)
{
	if (!ObjListFilterFn(hTest, pUserData)) return LTFALSE;
	if (g_pGameClientShell
		&& g_pGameClientShell->GetSFXMgr()->FindSpecialFX(SFX_BODY_ID, hTest))
		return LTFALSE;
	// A WORLD MODEL THE ENGINE HAS HIDDEN IS NOT THERE. At the intro's
	// sniping window the dot stopped 12-42 units out on "an object type 2"
	// with no polygon - a world model's box, at the window, that nothing
	// draws: the pane the level opened. The renderer leaves such a model
	// out; the aim ray now does the same. Three headset runs had the red dot
	// stuck to the window, as if on the glass.
	if (g_pLTClient->GetObjectType(hTest) == OT_WORLDMODEL
		&& !(g_pLTClient->GetObjectFlags(hTest) & FLAG_VISIBLE))
		return LTFALSE;
	return LTTRUE;
}

void CGameClientShell::UpdateVRAimMarker()
{
	m_bVRAimMarkerOn = LTFALSE;

	const LTBOOL bWant = (g_vtVRAimMarker.GetFloat() > 0.0f)
		&& (g_vtVRHandFire.GetFloat() > 0.0f)
		&& VRShared::IsLive()
		&& VRShared::State().Hands[1].nActive
		&& IsFirstPerson()
		&& !IsUsingExternalCamera()
		&& !IsPlayerDead()
		// THE SCOPE KEEPS THE DOT. Zooming turns the flat crosshair off,
		// and the dot went with it - in the sniping section it only showed
		// for a moment after a shot broke the zoom. Headset run 14.
		&& (m_InterfaceMgr.IsCrosshairOn() || IsZoomed())
		&& m_weaponModel.GetHandle()
		// NOTHING IN THE HAND, NO DOT. The HQ start has no weapon, and the
		// dot went on showing - two feet out, on the empty weapon model at
		// the hand rather than on the wall. The tester suggested it should just
		// go away when nothing is equipped.
		&& m_weaponModel.GetWeaponId() != WMGR_INVALID_ID
		&& g_pWeaponMgr->GetWeapon(m_weaponModel.GetWeaponId());

	if (!bWant)
	{
		// Hidden, not destroyed: holstering a weapon or opening a menu is a
		// per-frame condition, and recreating a sprite each time it clears
		// would churn an object the renderer caches by address.
		if (m_hVRAimMarker)
		{
			const uint32 dwOld = g_pLTClient->GetObjectFlags(m_hVRAimMarker);
			g_pLTClient->SetObjectFlags(m_hVRAimMarker, dwOld & ~FLAG_VISIBLE);
		}
		return;
	}

	LTVector vU, vR, vF, vFirePos;
	if (!m_weaponModel.GetFireInfo(vU, vR, vF, vFirePos)) return;
	VEC_NORM(vF);

	// The weapon's own range, so the dot stops where the bullet would.
	WEAPON* pWeapon = g_pWeaponMgr->GetWeapon(m_weaponModel.GetWeaponId());
	// NO DOT FOR A FIST. "Unarmed" is weapon 18, "Fisty cuffs", range 100 -
	// a known weapon, so the gate above passed, and the dot parked at the end
	// of its 100-unit reach: in the headset it stayed really close to the
	// player even when pointing at a very far wall. Nothing that reaches under 300 units is
	// a gun, and a gun's dot is the point of this.
	if (pWeapon && pWeapon->nRange < 300)
	{
		if (m_hVRAimMarker)
		{
			const uint32 dwOld = g_pLTClient->GetObjectFlags(m_hVRAimMarker);
			g_pLTClient->SetObjectFlags(m_hVRAimMarker, dwOld & ~FLAG_VISIBLE);
		}
		return;
	}
	const LTFLOAT fRange = pWeapon ? (LTFLOAT)pWeapon->nRange : 10000.0f;

	HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();
	if (!hPlayerObj) return;
	// The weapon model itself is in the list too: the ray starts at the eye
	// and the model sits at the hand, right in its path.
	// Room for four more: world models hit by their BOX rather than by a
	// polygon are added here and the cast repeated (see below).
	HOBJECT hFilterList[8] = { hPlayerObj, m_MoveMgr.GetObject(), m_weaponModel.GetHandle(), LTNULL,
							   LTNULL, LTNULL, LTNULL, LTNULL };
	int nFilterUsed = 3;

	ClientIntersectQuery query;
	ClientIntersectInfo  info;
	memset(&query, 0, sizeof(query));

	LTVector vEnd;
	VEC_MULSCALAR(vEnd, vF, fRange);
	VEC_ADD(vEnd, vEnd, vFirePos);

	VEC_COPY(query.m_From, vFirePos);
	VEC_COPY(query.m_To, vEnd);
	query.m_Flags     = INTERSECT_OBJECTS | IGNORE_NONSOLID;
	query.m_FilterFn  = VRAimFilterFn;		// the player, and dead bodies
	query.m_pUserData = hFilterList;

	LTVector vPos;
	float fAdvanced = 0.0f;
	// A WORLD MODEL HIT WITH NO POLYGON IS A BOX, NOT A SURFACE. An open
	// window's frame object still sits over the opening, and the client's
	// ray stopped on its box at the sniping window while the leaf itself was
	// swung aside. Such a hit is excluded and the cast run again, up to four
	// times; a hit that names a polygon - a closed door's leaf - still stops
	// the dot where it should.
	LTBOOL bHit = g_pLTClient->IntersectSegment(&query, &info);
	int nRecast = 0;
	while (bHit && info.m_hObject && nFilterUsed < 7
		   && g_pLTClient->GetObjectType(info.m_hObject) == OT_WORLDMODEL
		   && !(info.m_hPoly && info.m_hPoly != INVALID_HPOLY))
	{
		hFilterList[nFilterUsed++] = info.m_hObject;
		hFilterList[nFilterUsed] = LTNULL;
		++nRecast;
		bHit = g_pLTClient->IntersectSegment(&query, &info);
	}
	// THE LEVEL'S OWN POLYGONS, FROM THE RENDERER. The engine's ray reaches
	// a world model by its BOX only: at the intro's window the whole street
	// is one Terrain object and the ray "hit" it at its full range with no
	// polygon, so the dot never landed across the street. The renderer holds
	// every polygon it draws; R3D_RayCast tests them, honouring moved doors
	// and leaving out what the engine hid. The nearer of the two answers wins,
	// and a renderer hit is a world hit (no object to advance into).
	LTBOOL bRenHit = LTFALSE;
	{
		typedef int (__cdecl *VRRayCastFn)(const float*, const float*, float*);
		static VRRayCastFn s_pfnRay = NULL;
		static HMODULE s_hRenSeenRay = (HMODULE)(uintptr_t)1;
		HMODULE hR = GetModuleHandleA("d3dstub.ren");
		if (hR != s_hRenSeenRay)
		{
			s_hRenSeenRay = hR;
			s_pfnRay = hR ? (VRRayCastFn)GetProcAddress(hR, "R3D_RayCast") : NULL;
			VRLog::Msg("VRAim: renderer ray %s", s_pfnRay ? "resolved" : "missing");
		}
		if (s_pfnRay)
		{
			const float fFrom[3] = { vFirePos.x, vFirePos.y, vFirePos.z };
			const float fTo[3]   = { vEnd.x, vEnd.y, vEnd.z };
			float fOut[4] = { 0, 0, 0, 0 };
			if (s_pfnRay(fFrom, fTo, fOut))
			{
				const float fRenDist = fOut[3] * fRange;
				float fEngDist = 1e30f;
				if (bHit) { LTVector vE; VEC_SUB(vE, info.m_Point, vFirePos); fEngDist = VEC_MAG(vE); }
				if (fRenDist < fEngDist)
				{
					bHit = LTTRUE; bRenHit = LTTRUE;
					info.m_hObject = LTNULL;
					info.m_hPoly = INVALID_HPOLY;
					info.m_Point.Init(fOut[0], fOut[1], fOut[2]);
				}
			}
		}
	}
	if (bHit)
	{
		VEC_COPY(vPos, info.m_Point);

		// A CHARACTER'S BOX IS NOT THE CHARACTER. The ray tests bounding
		// boxes, and a standing character's box is a column well wider than
		// the body, so the hit is on the box face - a hand's width or more in
		// front of the chest, and further still when the box is hit near a
		// corner. In the headset, aimed at an enemy, the dot seemed to hover
		// about a foot in front of the player, when it should be touching the
		// surface.
		//
		// There is no mesh to intersect on the client, so the dot goes to the
		// point on the ray nearest the character's own axis, bounded by the
		// box - the body is there, whichever way the box was entered. That
		// point is inside the mesh, which is why the sprite is drawn with no
		// depth test (VRSPRITE_F_NODEPTH). Characters only: a crate's box IS
		// the crate, and its face is the right place.
		const HOBJECT hHitObj = info.m_hObject;
		if (hHitObj && g_pPhysicsLT
			&& GetSFXMgr()->FindSpecialFX(SFX_CHARACTER_ID, hHitObj))
		{
			LTVector vObj, vDims;
			g_pLTClient->GetObjectPos(hHitObj, &vObj);
			g_pPhysicsLT->GetObjectDims(hHitObj, &vDims);
			LTVector vHitRel; VEC_SUB(vHitRel, info.m_Point, vFirePos);
			LTVector vObjRel; VEC_SUB(vObjRel, vObj, vFirePos);
			const float tHit = VEC_DOT(vHitRel, vF);
			const float tAxis = VEC_DOT(vObjRel, vF);
			const float fAcross = (vDims.x > vDims.z ? vDims.x : vDims.z) * 2.0f;
			float t = tAxis;
			if (t < tHit) t = tHit;
			if (t > tHit + fAcross) t = tHit + fAcross;
			fAdvanced = t - tHit;
			VEC_MULSCALAR(vPos, vF, t);
			VEC_ADD(vPos, vPos, vFirePos);
		}
	}
	else
	{
		// Nothing within range: put the dot at the end of the ray so it
		// still says where the gun points across open ground.
		VEC_COPY(vPos, vEnd);
	}

	// ON the surface. It used to sit two units off the wall to escape a
	// z-fight; the sprite is drawn with no depth test now (VRSPRITE_F_NODEPTH)
	// so there is nothing to fight, and up close the gap showed: right up
	// against a wall it looked as if it was not touching the wall, about
	// a unit or two off.

	// WHAT THE DOT LANDED ON, twice a second. In the headset the dot had no
	// distance, staying really close to the player even when pointing at a far
	// wall - and nothing recorded what the ray was hitting.
	{
		static int s_nDotSaid = 0;
		if (s_nDotSaid++ % 150 == 0)
		{
			LTVector vD; VEC_SUB(vD, vPos, vFirePos);
			const HOBJECT hHit = info.m_hObject;
			const char* pszWhat = bRenHit ? "the level (renderer polygon)"
				: !hHit ? "nothing (ray end)"
				: (hHit == m_weaponModel.GetHandle()) ? "THE WEAPON MODEL"
				: (hHit == hPlayerObj) ? "THE PLAYER"
				: (hHit == m_MoveMgr.GetObject()) ? "THE MOVE OBJECT"
				: (hHit == m_hVRAimMarker) ? "THE DOT ITSELF"
				: "an object";
			uint32 nType = hHit ? g_pLTClient->GetObjectType(hHit) : 0;
			// AND WHAT THE OBJECT IS: its flags, its box and its place, so
			// "hit an object type 2" can be matched to a world model by hand.
			uint32 dwHF = 0; LTVector vHP(0, 0, 0), vHD(0, 0, 0);
			if (hHit)
			{
				dwHF = g_pLTClient->GetObjectFlags(hHit);
				g_pLTClient->GetObjectPos(hHit, &vHP);
				if (g_pPhysicsLT) g_pPhysicsLT->GetObjectDims(hHit, &vHD);
			}
			VRLog::Msg("aim dot: weapon id %d (%s) state %d  hit %s type %u"
					   "  at %.0f units  poly %s  advanced to the axis %.1f"
					   "  | object %08X flags %08X (%s%s) at %.0f %.0f %.0f dims %.0f %.0f %.0f",
					   m_weaponModel.GetWeaponId(), pWeapon ? "known" : "none",
					   (int)m_weaponModel.GetState(), pszWhat, nType,
					   VEC_MAG(vD),
					   (info.m_hPoly && info.m_hPoly != INVALID_HPOLY) ? "yes" : "no",
					   fAdvanced,
					   (unsigned)(uintptr_t)hHit, (unsigned)dwHF,
					   (dwHF & FLAG_VISIBLE) ? "visible" : "HIDDEN",
					   (dwHF & FLAG_SOLID) ? " solid" : " nonsolid",
					   vHP.x, vHP.y, vHP.z, vHD.x, vHD.y, vHD.z);
		}
	}

	if (!m_hVRAimMarker)
	{
		ObjectCreateStruct theStruct;
		INIT_OBJECTCREATESTRUCT(theStruct);
		theStruct.m_ObjectType = OT_SPRITE;
		// A DOT, NOT A FLARE. Red1.spr is a soft glow and reads as a blob over
		// the target at any size. Dot1.spr is a hard-edged disc authored for
		// this (ASSETS/SFX/Flares) on the flare's own texture header, so it
		// is the same format the engine already loads. VRAimMarkerStyle 0 is
		// the old flare. The tester asked for the same type as the Quake II port.
		// 2 = NOLF's own yellow cross with a red centre (Cross1.spr, ours),
		// which is what was asked for; 1 = a plain red disc; 0 = the game's
		// soft flare.
		{
			const int nStyle = (int)g_vtVRAimMarkerStyle.GetFloat();
			SAFE_STRCPY(theStruct.m_Filename,
				(nStyle >= 2) ? "SFX\\Flares\\Cross1.spr"
				: (nStyle == 1) ? "SFX\\Flares\\Dot1.spr"
								: "SFX\\Flares\\Red1.spr");
		}
		theStruct.m_Flags  = FLAG_VISIBLE | FLAG_NOLIGHT;
		theStruct.m_Flags2 = FLAG2_ADDITIVE;
		VEC_COPY(theStruct.m_Pos, vPos);
		m_hVRAimMarker = g_pLTClient->CreateObject(&theStruct);
		if (!m_hVRAimMarker) return;
	}

	const uint32 dwOld = g_pLTClient->GetObjectFlags(m_hVRAimMarker);
	g_pLTClient->SetObjectFlags(m_hVRAimMarker, dwOld | FLAG_VISIBLE);

	// A FIXED SIZE IN THE WORLD, the way the Quake II port's dot works and the
	// way a real laser dot works: about a centimetre and a half across, so it
	// is a few pixels on a far wall and a small clear disc on a near one, and
	// never a blob over the target. The tester sent Quake II captures to show
	// exactly this - small, visibly larger when pointing at something close
	// and smaller farther away, but never big.
	//
	// VRAimMarkerWorld 0 restores the constant-on-screen scaling below, which
	// is what had made it large and distracting at every range.
	LTVector vDelta;
	VEC_SUB(vDelta, vPos, vFirePos);
	// A SPRITE AT SCALE 1 IS ONE WORLD UNIT PER TEXEL, and the disc is 32
	// texels across - so "world size 1.0" drew a 32-unit, 54 cm blob and the
	// desk measured it at 40 px on a far wall, the same as before. Divide by
	// the art's width so the cvar really is units, and drop the old floor of
	// 0.05 (1.6 units) that would have kept it above a laser dot regardless.
	LTFLOAT fS;
	if (g_vtVRAimMarkerWorld.GetFloat() > 0.0f)
	{
		fS = g_vtVRAimMarkerWorldSize.GetFloat() / 32.0f;
		// ...BUT NEVER SUB-PIXEL. A 34 mm dot measured ONE PIXEL on the far
		// wall of the Morocco street at 1280 px per eye, which is no sight at
		// all. Quake II's stays a few pixels at any range, so under the world
		// size sits a small angular floor: 0.012 is about three pixels across
		// a headset eye. Near, the world size wins; far, the floor does.
		const LTFLOAT fFloor = g_vtVRAimMarkerMinSize.GetFloat()
							 * (VEC_MAG(vDelta) / 100.0f);
		if (fS < fFloor) fS = fFloor;
	}
	else
		fS = g_vtVRAimMarkerSize.GetFloat() * (VEC_MAG(vDelta) / 100.0f);
	if (fS < 0.005f) fS = 0.005f;
	// CEILING 1.2, NOT 6. Past about this the flare art stops reading as a dot
	// and starts covering the target, which is the failure the size default
	// above is about. A red-dot sight that hides what you are aiming at is
	// worse than no sight.
	if (fS > 1.2f)  fS = 1.2f;

	LTVector vScale;
	VEC_SET(vScale, fS, fS, 1.0f);
	g_pLTClient->SetObjectScale(m_hVRAimMarker, &vScale);
	g_pLTClient->SetObjectPos(m_hVRAimMarker, &vPos);

	m_bVRAimMarkerOn = LTTRUE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::VRHidesGameCrosshair()
//
//	PURPOSE:	Is the world aim marker standing in for the 2D crosshair?
//
//				Deliberately reads m_bVRAimMarkerOn - the marker was actually
//				PLACED this frame - and not the cvar that asks for it. If the
//				marker fails for any reason (no weapon, no fire info, the
//				sprite would not create) the flat crosshair stays, so a fault
//				here cannot leave the player with no aim at all.
//
// ----------------------------------------------------------------------- //

LTBOOL CGameClientShell::VRPersistentFX() const
{
	return (g_vtVRPersistentFX.GetFloat() > 0.0f) ? LTTRUE : LTFALSE;
}

LTBOOL CGameClientShell::VRHidesGameCrosshair() const
{
	if (g_vtVRHideCrosshair.GetFloat() <= 0.0f) return LTFALSE;
	return m_bVRAimMarkerOn;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::Update3rdPersonCrossHair()
//
//	PURPOSE:	Update the 3rd person crosshair pos
//
// ----------------------------------------------------------------------- //

void CGameClientShell::Update3rdPersonCrossHair(LTFLOAT fDistance)
{
	if (!m_PlayerCamera.IsChaseView()) return;

	// Don't do 3rd person cross hair...
	return;


	HLOCALOBJ hPlayerObj = m_MoveMgr.GetObject();
	if (!hPlayerObj) return;

	if (!m_h3rdPersonCrosshair)
	{
		// Create the 3rd person crosshair sprite...

		ObjectCreateStruct theStruct;
		INIT_OBJECTCREATESTRUCT(theStruct);

		theStruct.m_ObjectType = OT_SPRITE;
		SAFE_STRCPY(theStruct.m_Filename, "Spr\\Crosshair.spr");
		theStruct.m_Flags = FLAG_VISIBLE | FLAG_GLOWSPRITE | FLAG_NOLIGHT;
        m_h3rdPersonCrosshair = g_pLTClient->CreateObject(&theStruct);

        LTVector vScale;
		VEC_SET(vScale, .5f, .5f, 1.0f);
        g_pLTClient->SetObjectScale(m_h3rdPersonCrosshair, &vScale);
	}

	if (fDistance < 1.0f || m_bUsingExternalCamera || !m_InterfaceMgr.IsCrosshairOn())
	{
        uint32 dwFlags = g_pLTClient->GetObjectFlags(m_h3rdPersonCrosshair);
        g_pLTClient->SetObjectFlags(m_h3rdPersonCrosshair, dwFlags & ~FLAG_VISIBLE);
		return;
	}
	else
	{
        uint32 dwFlags = g_pLTClient->GetObjectFlags(m_h3rdPersonCrosshair);
        g_pLTClient->SetObjectFlags(m_h3rdPersonCrosshair, dwFlags | FLAG_VISIBLE);
	}

    LTVector vU, vR, vF;
    g_pLTClient->GetRotationVectors(&m_rRotation, &vU, &vR, &vF);
	VEC_NORM(vF);

    LTVector vPos;
    g_pLTClient->GetObjectPos(hPlayerObj, &vPos);

    LTFLOAT fDist = fDistance > 100.0f ? fDistance - 20.0f : fDistance;

    LTVector vTemp;
	VEC_MULSCALAR(vTemp, vF, fDist);
	VEC_ADD(vPos, vPos, vTemp);

	// Set the 3rd person crosshair to the correct position...

    g_pLTClient->SetObjectPos(m_h3rdPersonCrosshair, &vPos);
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateContainerFX
//
//	PURPOSE:	Update any client side container fx
//
// --------------------------------------------------------------------------- //

void CGameClientShell::UpdateContainerFX()
{
    LTVector vPos;
    g_pLTClient->GetObjectPos(m_hCamera, &vPos);

	HLOCALOBJ objList[1];
    uint32 dwNum = g_pLTClient->GetPointContainers(&vPos, objList, 1);

    LTVector vScale, vLightAdd;
	VEC_SET(vScale, 1.0f, 1.0f, 1.0f);
	VEC_SET(vLightAdd, 0.0f, 0.0f, 0.0f);
    LTBOOL bClearCurrentLightScale = LTFALSE;

    char* pCurSound      = LTNULL;
	ContainerCode eCode  = CC_NO_CONTAINER;
	uint8 nSoundFilterId = 0;
    uint32 dwUserFlags   = USRFLG_VISIBLE;
    m_bUseWorldFog       = LTTRUE;

	// Get the user flags associated with the container, and make sure that
	// the container isn't hidden...

	if (dwNum > 0 && objList[0])
	{
        g_pLTClient->GetObjectUserFlags(objList[0], &dwUserFlags);
	}


	if (dwNum > 0 && (dwUserFlags & USRFLG_VISIBLE))
	{
        uint16 code;
        if (g_pLTClient->GetContainerCode(objList[0], &code))
		{
			eCode = (ContainerCode)code;

			// See if we have entered/left a container...

            LTFLOAT fTime = g_pLTClient->GetTime();

			if (m_eCurContainerCode != eCode)
			{
				m_fContainerStartTime = fTime;

				// Check for weather volume brush first (weather volume brushes
				// should never overlap normal volume brushes)

				CVolumeBrushFX* pFX = (CVolumeBrushFX*)m_sfxMgr.FindSpecialFX(SFX_WEATHER_ID, objList[0]);
				if (!pFX)
				{
					pFX = (CVolumeBrushFX*)m_sfxMgr.FindSpecialFX(SFX_VOLUMEBRUSH_ID, objList[0]);
				}

				if (pFX)
				{
					// Set the sound filter override...

					nSoundFilterId = pFX->GetSoundFilterId();

					// See if this container has fog associated with it..

                    LTBOOL bFog = pFX->IsFogEnable();

					if (bFog)
					{
                        m_bUseWorldFog = LTFALSE;

						char buf[30];
						sprintf(buf, "FogEnable %d", (int)bFog);
                        g_pLTClient->RunConsoleString(buf);

						sprintf(buf, "FogNearZ %d", (int)pFX->GetFogNearZ());
                        g_pLTClient->RunConsoleString(buf);

						sprintf(buf, "FogFarZ %d", (int)pFX->GetFogFarZ());
                        g_pLTClient->RunConsoleString(buf);

                        LTVector vFogColor = pFX->GetFogColor();

						sprintf(buf, "FogR %d", (int)vFogColor.x);
                        g_pLTClient->RunConsoleString(buf);

						sprintf(buf, "FogG %d", (int)vFogColor.y);
                        g_pLTClient->RunConsoleString(buf);

						sprintf(buf, "FogB %d", (int)vFogColor.z);
                        g_pLTClient->RunConsoleString(buf);
					}

					// Get the tint color...

					vScale = pFX->GetTintColor();

					if (eCode == CC_WATER)
					{
						vScale = LTVector(0.0f, 127.0f, 178.5f);
					}

					vScale /= 255.0f;

					vLightAdd = pFX->GetLightAdd();
					vLightAdd /= 255.0f;
				}
			}


			switch (eCode)
			{
				case CC_WATER:
				case CC_CORROSIVE_FLUID:
				case CC_FREEZING_WATER:
				{
					pCurSound = "Chars\\Snd\\Player\\unwater.wav";
				}
				break;

				case CC_ENDLESS_FALL:
				{
                    LTFLOAT fFallTime = 1.0f;

					if (fTime > m_fContainerStartTime + fFallTime)
					{
						VEC_SET(vScale, 0.0f, 0.0f, 0.0f);
					}
					else
					{
                        LTFLOAT fScaleStart = .3f;
                        LTFLOAT fTimeLeft = (m_fContainerStartTime + fFallTime) - fTime;
                        LTFLOAT fScalePercent = fTimeLeft/fFallTime;
                        LTFLOAT fScale = fScaleStart * fScalePercent;

						VEC_SET(vScale, fScale, fScale, fScale);
					}

					// special-case the light scale stuff for endless fall
					// if this is our first time in this case, don't clear the effect - otherwise clear it
					if (m_eCurContainerCode == CC_ENDLESS_FALL)
					{
                        bClearCurrentLightScale = LTTRUE;
					}
				}
				break;

				default : break;
			}

		}
	}


	// See if we have entered/left a container...

	if (m_eCurContainerCode != eCode)
	{
		// See if the old container (if any) modified the light scale

        bClearCurrentLightScale = LTTRUE;

		// Adjust Fog as necessary...

		if (m_bUseWorldFog)
		{
			ResetGlobalFog();
		}

		// See if we need to reset the current lightscale
		// If we entered a new container (or modified the light scale in the case of endless fall), then vScale won't be (1,1,1)
        // If we left a container that modified the light scale, then bClearCurrentLightScale will be LTTRUE

		if (bClearCurrentLightScale)
		{
			m_LightScaleMgr.ClearLightScale(&m_vCurContainerLightScale, LightEffectEnvironment);
			VEC_SET(m_vCurContainerLightScale, -1.0f, -1.0f, -1.0f);
		}

		if (vScale.x != 1.0f || vScale.y != 1.0f || vScale.z != 1.0f)
		{
			m_LightScaleMgr.SetLightScale(&vScale, LightEffectEnvironment);
			m_vCurContainerLightScale = vScale;
		}

		// See if we are coming out of water...

		if (IsLiquid(m_eCurContainerCode) && !IsLiquid(eCode))
		{
            UpdateUnderWaterFX(LTFALSE);
            g_pLTClient->RunConsoleString("+ModelWarble 0");
			m_InterfaceMgr.EndUnderwater();
		}

		if (!IsLiquid(m_eCurContainerCode) && IsLiquid(eCode))
		{
            g_pLTClient->RunConsoleString("ModelWarble 1");
			m_InterfaceMgr.BeginUnderwater();
		}

		m_eCurContainerCode = eCode;
		m_nSoundFilterId	= nSoundFilterId;

		if (m_hContainerSound)
		{
            g_pLTClient->KillSound(m_hContainerSound);
            m_hContainerSound = LTNULL;
		}

		if (pCurSound)
		{
            uint32 dwFlags = PLAYSOUND_CLIENT | PLAYSOUND_LOOP | PLAYSOUND_GETHANDLE;
			m_hContainerSound = g_pClientSoundMgr->PlaySoundLocal(pCurSound, SOUNDPRIORITY_PLAYER_MEDIUM, dwFlags);
		}

		m_ScreenTintMgr.Set(TINT_CONTAINER,&vLightAdd);
	}


	// See if we are under water (under any liquid)...

	if (IsLiquid(m_eCurContainerCode))
	{
		UpdateUnderWaterFX();
	}
	else
	{
		UpdateBreathingFX();
	}
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateUnderWaterFX
//
//	PURPOSE:	Update under water fx
//
// --------------------------------------------------------------------------- //

void CGameClientShell::UpdateUnderWaterFX(LTBOOL bUpdate)
{
	if (m_nZoomView) return;

    uint32 dwWidth = 640, dwHeight = 480;
    g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &dwWidth, &dwHeight);

	if (dwWidth < 0 || dwHeight < 0) return;


	// Initialize to default fov x and y...

    LTFLOAT fFovX = g_vtFOVXNormal.GetFloat();
    LTFLOAT fFovY = g_vtFOVYNormal.GetFloat();

	if (bUpdate)
	{
        g_pLTClient->GetCameraFOV(m_hCamera, &fFovX, &fFovY);

		fFovX = RAD2DEG(fFovX);
		fFovY = RAD2DEG(fFovY);

        LTFLOAT fSpeed = g_vtUWFOVRate.GetFloat() * m_fFrameTime;

		if (m_fFovXFXDir > 0)
		{
			fFovX -= fSpeed;
			fFovY += fSpeed;

			if (fFovY > g_vtFOVYMaxUW.GetFloat())
			{
				fFovY = g_vtFOVYMaxUW.GetFloat();
				m_fFovXFXDir = -m_fFovXFXDir;
			}
		}
		else
		{
			fFovX += fSpeed;
			fFovY -= fSpeed;

			if (fFovY < g_vtFOVYMinUW.GetFloat())
			{
				fFovY = g_vtFOVYMinUW.GetFloat();
				m_fFovXFXDir = -m_fFovXFXDir;
			}
		}
	}

	SetCameraFOV(DEG2RAD(fFovX), DEG2RAD(fFovY));
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateBreathingFX
//
//	PURPOSE:	Update breathing fx
//
// --------------------------------------------------------------------------- //

void CGameClientShell::UpdateBreathingFX(LTBOOL bUpdate)
{
	//if (m_nZoomView) return;


}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ChangeWeapon()
//
//	PURPOSE:	Change the weapon model
//
// ----------------------------------------------------------------------- //

void CGameClientShell::ChangeWeapon(HMESSAGEREAD hMessage)
{
	if (!hMessage) return;

    uint8 nCommandId = g_pLTClient->ReadFromMessageByte(hMessage);
    uint8 bAuto      = (LTBOOL) g_pLTClient->ReadFromMessageByte(hMessage);
    LTFLOAT fAmmoId  = g_pLTClient->ReadFromMessageFloat(hMessage);


    uint8 nWeaponId = g_pWeaponMgr->GetWeaponId(nCommandId);
	WEAPON* pWeapon = g_pWeaponMgr->GetWeapon(nWeaponId);
	if (!pWeapon) return;

    LTBOOL bChange = LTTRUE;

	// See what ammo the weapon should start with...

	uint8 nAmmoId = pWeapon->nDefaultAmmoType;
	if (fAmmoId >= 0)
	{
		nAmmoId = (uint8)fAmmoId;
	}

	// If this is an auto weapon change and this is a multiplayer game, see
	// if the user really wants us to switch or not (we'll always switch in
	// single player games)...

	if (bAuto && IsMultiplayerGame())
	{
        bChange = (LTBOOL)GetConsoleInt("AutoWeaponSwitch",1);

		// See if the weapon we're chaning to is really a weapon...

		if (bChange)
		{
			// Don't autoswitch to gadgets in multiplayer...

			AMMO* pAmmo = g_pWeaponMgr->GetAmmo(nAmmoId);
			if (pAmmo && ::IsGadgetType(pAmmo->eInstDamageType))
			{
				bChange = LTFALSE;
			}
		}
	}

	if (bChange)
	{
        // Force a change to the approprite weapon...
		CPlayerStats* pStats = m_InterfaceMgr.GetPlayerStats();
		if (pStats)
		{
			ChangeWeapon(nWeaponId, nAmmoId, pStats->GetAmmoCount(nAmmoId));
		}
    }

}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ChangeWeapon()
//
//	PURPOSE:	Change the weapon model
//
// ----------------------------------------------------------------------- //

void CGameClientShell::ChangeWeapon(uint8 nWeaponId, uint8 nAmmoId, uint32 dwAmmo)
{
    LTBOOL bSameWeapon = (nWeaponId == m_weaponModel.GetWeaponId());
	// Turn off zooming...

    uint8 nOldWeaponId = m_weaponModel.GetWeaponId();
	if (!bSameWeapon && g_pWeaponMgr->IsValidWeapon(nOldWeaponId))
	{
        HandleZoomChange(nOldWeaponId, LTTRUE);
	}

	if (!m_weaponModel.GetHandle() || !bSameWeapon)
	{
        m_weaponModel.Create(g_pLTClient, nWeaponId, nAmmoId, dwAmmo);
	}

	if (m_PlayerCamera.IsChaseView())
	{
        m_weaponModel.SetVisible(LTFALSE);
	}

	// Tell the server to change weapons...

    HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_WEAPON_CHANGE);
    g_pLTClient->WriteToMessageByte(hMessage, nWeaponId);
    g_pLTClient->EndMessage(hMessage);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::BeginInfrared()
//
//	PURPOSE:	setup infrared viewing mode
//
// ----------------------------------------------------------------------- //

void CGameClientShell::BeginInfrared()
{
	m_LightScaleMgr.SetLightScale(&m_vIRLightScale, LightEffectPowerup);
    g_pLTClient->SetModelHook((ModelHookFn)IRModelHook, this);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::EndInfrared()
//
//	PURPOSE:	end infrared viewing mode
//
// ----------------------------------------------------------------------- //

void CGameClientShell::EndInfrared()
{
	m_LightScaleMgr.ClearLightScale(&m_vIRLightScale, LightEffectPowerup);
    g_pLTClient->SetModelHook((ModelHookFn)DefaultModelHook, this);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::BeginMineMode()
//
//	PURPOSE:	setup mine viewing mode
//
// ----------------------------------------------------------------------- //

void CGameClientShell::BeginMineMode()
{
    LTVector vCol = g_pLayoutMgr->GetMineDetectScreenTint();
	m_LightScaleMgr.SetLightScale(&vCol, LightEffectPowerup);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::EndMineMode()
//
//	PURPOSE:	end mine viewing mode
//
// ----------------------------------------------------------------------- //

void CGameClientShell::EndMineMode()
{
    LTVector vCol = g_pLayoutMgr->GetMineDetectScreenTint();
	m_LightScaleMgr.ClearLightScale(&vCol, LightEffectPowerup);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::BeginZoom()
//
//	PURPOSE:	prepare for zooming
//
// ----------------------------------------------------------------------- //

int CGameClientShell::VRScopeZoomLevel() const { return m_nZoomView; }

void CGameClientShell::BeginZoom()
{
    m_bNightVision = LTFALSE;
	CPlayerStats* pStats = m_InterfaceMgr.GetPlayerStats();
    uint8 nScopeId = pStats->GetScope();
	if (nScopeId != WMGR_INVALID_ID)
	{
		MOD* pMod = g_pWeaponMgr->GetMod(nScopeId);
		if (pMod)
		{
			m_bNightVision = pMod->bNightVision;
		}
	}

	// IN VR THE SCOPE IS ON THE GUN, so hiding the gun would hide the scope
	// and its lens with it - which is what a zoom did in the headset on
	// 21 September: the sound played and the picture vanished.
	if (VRShared::IsLive()) return;
	m_InterfaceMgr.BeginScope(m_bNightVision);
	if (m_bNightVision)
	{
		//m_ScreenTintMgr.Set(TINT_NIGHTVISION,&m_vNVScreenTint);
		m_LightScaleMgr.SetLightScale(&m_vNVScreenTint, LightEffectPowerup);
        g_pLTClient->SetModelHook((ModelHookFn)NVModelHook, this);
	}
    m_weaponModel.SetVisible(LTFALSE);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::EndZoom()
//
//	PURPOSE:	done zooming
//
// ----------------------------------------------------------------------- //

void CGameClientShell::EndZoom()
{
	m_InterfaceMgr.EndScope();
	if (m_bNightVision)
	{
		// m_ScreenTintMgr.Clear(TINT_NIGHTVISION);
		m_LightScaleMgr.ClearLightScale(&m_vNVScreenTint, LightEffectPowerup);
        g_pLTClient->SetModelHook((ModelHookFn)DefaultModelHook, this);
        m_bNightVision = LTFALSE;
	}
    m_weaponModel.SetVisible(LTTRUE);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandleZoomChange()
//
//	PURPOSE:	Handle a potential zoom change
//
// ----------------------------------------------------------------------- //

void CGameClientShell::HandleZoomChange(uint8 nWeaponId, LTBOOL bReset)
{
	// Reset to normal FOV...

	if (bReset)
	{
		if (m_nZoomView == 0) return;

		m_nZoomView  = 0;
        m_bZooming   = LTFALSE;
        m_bZoomingIn = LTFALSE;
		m_InterfaceMgr.EndZoom();
		EndZoom();

		SetCameraFOV(DEG2RAD(g_vtFOVXNormal.GetFloat()), DEG2RAD(g_vtFOVYNormal.GetFloat()));

		char strConsole[40];
		sprintf(strConsole, "+ModelLODOffset %f", m_fSaveLODScale);
        g_pLTClient->RunConsoleString(strConsole);
	}


	CPlayerStats* pStats = m_InterfaceMgr.GetPlayerStats();
    uint8 nScopeId = pStats->GetScope();
	if (nScopeId == WMGR_INVALID_ID) return;

	MOD* pMod = g_pWeaponMgr->GetMod(nScopeId);
	if (!pMod) return;

	// Play zoom in/out sounds...

	if (m_bZoomingIn)
	{
		if (pMod->szZoomInSound[0])
		{
			g_pClientSoundMgr->PlaySoundLocal(pMod->szZoomInSound, SOUNDPRIORITY_MISC_MEDIUM);
		}
	}
	else
	{
		if (pMod->szZoomOutSound[0])
		{
			g_pClientSoundMgr->PlaySoundLocal(pMod->szZoomOutSound, SOUNDPRIORITY_MISC_MEDIUM);
		}
	}
}


void CGameClientShell::ProcessHandshake(HMESSAGEREAD hMessage)
{
	int nHandshakeSub = (int)g_pLTClient->ReadFromMessageByte(hMessage);
	switch (nHandshakeSub)
	{
		case MID_HANDSHAKE_HELLO :
		{
			int nHandshakeVer = (int)g_pLTClient->ReadFromMessageWord(hMessage);
			if (nHandshakeVer != GAME_HANDSHAKE_VER)
			{
				// Disconnect
				m_bForceDisconnect = LTTRUE;

				// Show a mis-matched version message to the user
				g_pInterfaceMgr->ChangeState(GS_FOLDER);  // Note : Don't take this out or the cursor goes away!
				HSTRING hString = g_pLTClient->FormatString(IDS_NETERR_NOTSAMEGUID);
				g_pInterfaceMgr->ShowMessageBox(hString,LTMB_OK,LTNULL,LTNULL,g_pInterfaceResMgr->IsEnglish());
				g_pLTClient->FreeString(hString);

				return;
			}

			// Send back a hello response
			HMESSAGEWRITE hResponse = g_pLTClient->StartMessage(MID_HANDSHAKE);
		    g_pLTClient->WriteToMessageByte(hResponse, MID_HANDSHAKE_HELLO);
		    g_pLTClient->WriteToMessageWord(hResponse, GAME_HANDSHAKE_VER);
			// Send them our secret key
			g_pLTClient->WriteToMessageDWord(hResponse, GAME_HANDSHAKE_PASSWORD);
			g_pLTClient->EndMessage(hResponse);
		}
		break;
		case MID_HANDSHAKE_PASSWORD:
		{
			// Read in their key
			uint32 nServerKey = g_pLTClient->ReadFromMessageDWord(hMessage);

			uint32 nPassword = GAME_HANDSHAKE_PASSWORD;
			uint32 nXORMask = GAME_HANDSHAKE_MASK;

			nPassword ^= nXORMask;

			// Get the weapons file CRC
			uint32 nWeaponCRC = g_pWeaponMgr->GetFileCRC();
			// Mask that up too
			nWeaponCRC ^= nXORMask;

			// Get the client shell file CRC
			char aClientShellName[MAX_PATH + 1];
			// Just in case getting the file name fails
			aClientShellName[0] = 0; 
			// Get the client shell handle from the engine
			HMODULE hClientShell;
			g_pLTClient->GetEngineHook("cshell_hinstance", (void**)&hClientShell);
			DWORD nResult = GetModuleFileName(hClientShell, aClientShellName, sizeof(aClientShellName));
			uint32 nClientCRC = CRC32::CalcFileCRC(aClientShellName);
			
			// Mask that up too
			nClientCRC ^= nXORMask;

			// Send it back their direction
			HMESSAGEWRITE hResponse = g_pLTClient->StartMessage(MID_HANDSHAKE);
			g_pLTClient->WriteToMessageByte(hResponse, MID_HANDSHAKE_LETMEIN);
			g_pLTClient->WriteToMessageDWord(hResponse, nPassword);
			g_pLTClient->WriteToMessageDWord(hResponse, nWeaponCRC);
			g_pLTClient->WriteToMessageDWord(hResponse, nClientCRC);
			g_pLTClient->EndMessage(hResponse);
		}
		break;
		case MID_HANDSHAKE_DONE:
		{
			// This just means the server validated us...
		}
		break;
		default :
		{
			// Disconnect
			m_bForceDisconnect = LTTRUE;

			// Show a mis-matched version message to the user
			g_pInterfaceMgr->ChangeState(GS_FOLDER);  // Note : Don't take this out or the cursor goes away!
			HSTRING hString = g_pLTClient->FormatString(IDS_NETERR_NOTSAMEGUID);
			g_pInterfaceMgr->ShowMessageBox(hString,LTMB_OK,LTNULL,LTNULL,g_pInterfaceResMgr->IsEnglish());
			g_pLTClient->FreeString(hString);
		}
		break;
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::OnCommandOn()
//
//	PURPOSE:	Handle client commands
//
// ----------------------------------------------------------------------- //

void CGameClientShell::OnCommandOn(int command)
{
	// If console is active, ignore any other commands
	if (g_pConsoleMgr->IsVisible())
	{
		return;
	}

	// Let the interface handle the command first...
	if (IsPlayerInWorld())
	{
		if (IsMultiplayerGame() || !IsPlayerDead())
		{
			if (m_InterfaceMgr.OnCommandOn(command))
			{
				return;
			}
		}
	}

	// Check for weapon change...

	if (g_pWeaponMgr->GetFirstWeaponCommandId() <= command &&
		command <= g_pWeaponMgr->GetLastWeaponCommandId())
	{
		// NOT WHILE THE FLASH TUNER OWNS THE NUMPAD.
		//
		// In headset testing the numpad did not work because it changes weapons. The numpad
		// digits are the weapon-select bindings, so every nudge swapped the gun
		// out from under the thing being tuned - and swapping the weapon
		// re-creates the muzzle flash, which is the one object the tuner exists
		// to hold still.
		//
		// The tuner reads the keys with GetAsyncKeyState and does not consume
		// them, so the engine sees them too. Rather than move the tuner off the
		// numpad - the tester asked for the numpad, because that is what the Prey port
		// uses and the tester's hands already know it - the weapon change is suppressed
		// while VRFlashTune is on. VRDebugWeapon still chooses the gun, which is
		// how the tuning session is set up in the first place.
		if (g_vtVRFlashTune.GetFloat() > 0.0f)
		{
			static int s_nSaidSwap = 0;
			if (s_nSaidSwap < 3)
			{
				++s_nSaidSwap;
				VRLog::Msg("VRFlashTune: weapon change ignored while tuning");
			}
			return;
		}

		m_weaponModel.ChangeWeapon(command);
		return;
	}

	// AND THE CYCLING COMMANDS, which the guard above does not cover: they are
	// not in the weapon-slot command range. Shift is NextWeapon (autoexec.cfg
	// line 326, scancode 42), and swapping the gun re-creates the muzzle flash -
	// the one object the tuner exists to hold still. This was hit twice, once
	// on the numpad and once on shift.
	// QUICKSAVE IS IN THE SAME HANDFUL OF KEYS, and it is easy to hit blind, sometimes
	// landing on quick save or quick load or something else. A stray
	// quicksave during a tuning session overwrites the save the session was set
	// up from, which is a worse outcome than losing the tuning.
	if (g_vtVRFlashTune.GetFloat() > 0.0f &&
		(command == COMMAND_ID_NEXT_WEAPON || command == COMMAND_ID_PREV_WEAPON
		 || command == COMMAND_ID_HOLSTER || command == COMMAND_ID_QUICKSAVE))
	{
		static int s_nSaidCycle = 0;
		if (s_nSaidCycle < 3)
		{
			++s_nSaidCycle;
			VRLog::Msg("VRFlashTune: weapon cycle ignored while tuning");
		}
		return;
	}


	// Make sure we're in the world...

	if (!IsPlayerInWorld()) return;


	// Take appropriate action

	switch (command)
	{
		case COMMAND_ID_ACTIVATE :
		{
			DoActivate(LTFALSE);
		}
		break;

		case COMMAND_ID_RELOAD :
		{
			m_weaponModel.ReloadClip();
		}
		break;

		case COMMAND_ID_FLASHLIGHT :
		{
			m_FlashLight.Toggle();
		}
		break;

		case COMMAND_ID_ZOOM_IN :
		{
			if (m_weaponModel.IsDisabled()) break;

			CPlayerStats* pStats = m_InterfaceMgr.GetPlayerStats();
            uint8 nScopeId = pStats->GetScope();
			if (nScopeId == WMGR_INVALID_ID) break;

			MOD* pMod = g_pWeaponMgr->GetMod(nScopeId);
			if (!pMod) break;

			int nZoomLevel = pMod->nZoomLevel;

			// Figure out if our current weapon has a scope...
			if (!m_bZooming && nZoomLevel > 0)
			{
				int nOldZoom = m_nZoomView;

				m_nZoomView++;
				m_nZoomView = m_nZoomView > nZoomLevel ? nZoomLevel : m_nZoomView;

				if (m_nZoomView != nOldZoom)
				{
                    m_bZooming   = LTTRUE;
                    m_bZoomingIn = LTTRUE;

					if (nOldZoom == 0)
					{
						BeginZoom();
                    }

					m_InterfaceMgr.BeginZoom(LTTRUE);
					HandleZoomChange(pStats->GetCurWeapon());
				}
			}
		}
		break;

		case COMMAND_ID_ZOOM_OUT :
		{
			if (m_weaponModel.IsDisabled()) break;

			CPlayerStats* pStats = m_InterfaceMgr.GetPlayerStats();
            uint8 nScopeId = pStats->GetScope();
			if (nScopeId == WMGR_INVALID_ID) break;

			MOD* pMod = g_pWeaponMgr->GetMod(nScopeId);
			if (!pMod) break;

			int nZoomLevel = pMod->nZoomLevel;

			if (!m_bZooming && nZoomLevel > 0)
			{
				int nOldZoom = m_nZoomView;
				m_nZoomView = 0;

				if (m_nZoomView != nOldZoom)
				{
                    m_bZooming   = LTTRUE;
                    m_bZoomingIn = LTFALSE;
                    m_InterfaceMgr.BeginZoom(LTFALSE);
					HandleZoomChange(pStats->GetCurWeapon());
				}
			}
		}

		case COMMAND_ID_TOGGLE_ZOOM:
		{
			if (m_weaponModel.IsDisabled()) break;

			CPlayerStats* pStats = m_InterfaceMgr.GetPlayerStats();
			uint8 nScopeId = pStats->GetScope();
			if (nScopeId == WMGR_INVALID_ID) break;

			MOD* pMod = g_pWeaponMgr->GetMod(nScopeId);
			if (!pMod) break;

			int nZoomLevel = pMod->nZoomLevel;

			// Figure out if our current weapon has a scope...
			if (!m_bZooming && nZoomLevel > 0)
			{
				int nOldZoom = m_nZoomView;

				m_nZoomView++;
				m_nZoomView = m_nZoomView > nZoomLevel ? 0 : m_nZoomView;

				if (m_nZoomView != nOldZoom)
				{
					m_bZooming = LTTRUE;
					m_bZoomingIn = LTTRUE;

					if (nOldZoom == 0)
					{
						BeginZoom();
					}

					// If we hit the cap, zoomview will go back to 0. 
					// Good time to zoom out!
					if (m_nZoomView == 0)
					{
						m_bZooming = LTTRUE;
						m_bZoomingIn = LTFALSE;
						m_InterfaceMgr.BeginZoom(LTFALSE);
					}
					else {
						m_InterfaceMgr.BeginZoom(LTTRUE);
					}

					HandleZoomChange(pStats->GetCurWeapon());
				}
			}
		}
		break;

		case COMMAND_ID_TURNAROUND :
		{
			m_fYaw += MATH_PI;
		}
		break;

		case COMMAND_ID_MOUSEAIMTOGGLE :
		{
			CGameSettings* pSettings = m_InterfaceMgr.GetSettings();
			if (!pSettings) return;

			if (m_PlayerCamera.IsFirstPerson())
			{
				if (!pSettings->MouseLook())
				{
                    m_bHoldingMouseLook = LTTRUE;
				}
			}
		}
		break;

		case COMMAND_ID_CENTERVIEW :
		{
			m_fPitch = 0.0f;
		}
		break;

		case COMMAND_ID_FIRING :
		{
			if (IsPlayerDead())
			{
				if (IsMultiplayerGame())
				{
                    if (g_pLTClient->GetTime() > m_fEarliestRespawnTime)
					{
						HandleRespawn();
					}
				}
				else if (g_pLTClient->GetTime() > m_fEarliestRespawnTime)
				{
					HandleMissionFailed();
				}
			}
		}
		break;

		case COMMAND_ID_STRAFE:
		{
            m_bStrafing = LTTRUE;
		}
		break;

		case COMMAND_ID_QUICKSAVE :
		{
			if (!IsUsingExternalCamera() && GetGameType() == SINGLE &&
				!IsPlayerDead() && !m_MoveMgr.IsZipCordOn())
			{
				QuickSave();
			}
			else if (GetGameType() == SINGLE)
			{
				// Can't quicksave now...
			    HSTRING hStr = g_pLTClient->FormatString(IDS_CANTQUICKSAVE);
			    char* pStr = g_pLTClient->GetStringData(hStr);
				CSPrint(pStr);
				g_pLTClient->FreeString(hStr);
			}
		}
		break;

		case COMMAND_ID_QUICKLOAD :
		{
            if (GetGameType() != SINGLE && IsPlayerInWorld())
			{
			    HSTRING hString = g_pLTClient->FormatString(IDS_ENDCURRENTGAME);
			    g_pInterfaceMgr->ShowMessageBox(hString,LTMB_YESNO,QuickLoadCallBack,this);
				g_pLTClient->FreeString(hString);
			}
			else

			QuickLoad();
		}
		break;



		default :
		break;
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::OnCommandOff()
//
//	PURPOSE:	Handle command off notification
//
// ----------------------------------------------------------------------- //

void CGameClientShell::OnCommandOff(int command)
{
	// Let the interface handle the command first...
	if (m_InterfaceMgr.OnCommandOff(command))
	{
		return;
	}

	switch (command)
	{
		case COMMAND_ID_STRAFE :
		{
            m_bStrafing = LTFALSE;
		}
		break;

		case COMMAND_ID_MOUSEAIMTOGGLE :
		{
            m_bHoldingMouseLook = LTFALSE;
		}
		break;

        default : break;
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::OnKeyDown(int key, int rep)
//
//	PURPOSE:	Handle key down notification
//				Try to avoid using OnKeyDown and OnKeyUp as they
//				are not portable functions
//
// ----------------------------------------------------------------------- //

void CGameClientShell::OnKeyDown(int key, int rep)
{
	// VR: a project rule - one toggle key for stereo, so modes can be compared
	// in-game without restarting. Cycles off -> double -> side-by-side.
	//
	// F11, not F9. NOLF binds F9 to QuickLoad and F6 to QuickSave through the
	// engine's action system (defctrls.cfg scancodes 67 and 64), which is
	// processed independently of OnKeyDown - so returning early here does not
	// suppress the bound action and both fire. F10/F11/F12 are unbound.
	if (key == VK_F11)
	{
		const int nNext = ((int)g_vtVRStereo.GetFloat() + 1) % 3;
		g_vtVRStereo.SetFloat((LTFLOAT)nNext);

		const char* pszName = (nNext == 0) ? "OFF (mono)"
							: (nNext == 1) ? "DOUBLE (same camera)"
										   : "SIDE-BY-SIDE STEREO";
		g_pLTClient->CPrint("VRStereo %d - %s", nNext, pszName);
		VRLog::Msg("F9 -> VRStereo %d (%s)", nNext, pszName);
		return;
	}

	// VR: sweep pose lag from the keyboard rather than the console.
	//
	// The console is drawn once across the whole window, so in stereo it lands
	// in one eye and straddles the seam - it is effectively unreadable in the
	// headset. Tuning something that can only be judged by feel should not
	// require reading anything, so F7/F8 step the value and F10 marks the
	// current one as the best so far. The log carries the numbers; the player
	// only has to move the head and press keys.
	// F7/F8 now sweep the horizontal FOV trim rather than pose lag.
	//
	// Pose lag is settled and not worth more keys: any value other than 0 asks
	// the runtime to reproject against a reference we do not actually know, and
	// every attempt at that measured worse. 0 means the reprojection is
	// effectively identity, which is why it won.
	//
	// VRFovXTest is the live suspect for the warping. It is by construction a
	// mismatch between the field we DECLARE to the runtime and the field the
	// renderer actually drew - about 1.25x horizontally. The runtime resamples
	// our image into the headset's lens frustum using the declared field, so a
	// wrong one distorts, worst at the edges. Standing still that reads as a
	// mild static stretch; turning the head sweeps world features through it,
	// which reads as the world bending.
	//
	// 0.8 was chosen by how the world looked while standing still, which is the
	// wrong criterion for an artefact that only appears in motion.
	// F7/F8 are now a straight A/B on the exact-pose correction, not a sweep.
	//
	// Sweeping asked for a ranking of values 10 ms apart, which is below what
	// anyone can judge through a headset - it produced the verdict that they all
	// felt the same, twice, correctly. A binary comparison between two clearly different
	// states is the kind of judgement a headset tester makes reliably.
	if (key == VK_F7 || key == VK_F8)
	{
		const bool bOn = (key == VK_F8);
		g_vtVRExactPose.SetFloat(bOn ? 1.0f : 0.0f);
		g_pLTClient->CPrint("VRExactPose %s", bOn ? "ON" : "OFF");
		VRLog::Msg("%s -> VRExactPose %s", bOn ? "F8" : "F7", bOn ? "ON" : "OFF");
		return;
	}

	// F12 cycles the optical-centre mode, so it can be tested without the
	// console. The console is drawn once across the whole window, so each eye
	// receives half of it and it is unreadable in the headset - the player has
	// been typing commands blind from memory all session. Anything that has to
	// be judged while wearing the headset needs a key, not a command.
	// F5 toggles the render rate between 90 and 60.
	//
	// The game draws ~89 fps but the compositor only publishes the window at
	// the display's 60 Hz, so roughly a third of what is drawn is discarded -
	// and WHICH third is arbitrary, leaving the frames that do reach the
	// headset unevenly spaced in time. Drawing at exactly 60 means every frame
	// drawn is a frame delivered: the same count, evenly spaced. Regular
	// repeats are far less visible than irregular ones, so this may look
	// smoother despite being nominally slower.
	// F4 isolates the two halves of the world bending with head movement.
	//
	// With head tracking OFF the camera no longer rotates with the head at all,
	// but the runtime still reprojects our submitted image against the head
	// pose. So if the world STILL bends while moving the head with this off,
	// the fault is entirely in what we declare to the runtime - field of view
	// or pose - and nothing to do with how the client composes the camera.
	// If the bending stops, it is our camera path.
	//
	// The player's own observation is what this tests: mouse look feels correct
	// and head look does not, and the only difference between them is that a
	// still head means the runtime warps nothing.
	// F2/F3 trim the horizontal field, judged by whether the world STAYS PUT
	// while the head turns.
	//
	// This is an angular gain control, though it was not understood as one.
	// The runtime maps our image onto the field we declare, so if the declared
	// field is wider than what was actually rendered, a 10 degree head turn
	// sweeps world features by more than 10 degrees - the world appears to move
	// around the player rather than the player looking around it. That is the
	// owner's description exactly, and it is why mouse look feels right: with
	// the head still there is nothing for the mismatch to disagree with.
	//
	// Judged as a NULL - does the world stay anchored - rather than by how it
	// looks standing still. The appearance criterion produced a wrong answer
	// twice; people are very good at nulls and poor at absolute judgements.
	// F2 measures the field instead of asking anyone to rank it. The sweep this
	// replaces produced no usable answer twice, which was the test's fault and
	// not the player's - it is below the threshold at which a person can rank a
	// value, and it never needed eyes in the first place.
	if (key == VK_F2)
	{
		// Held for several frames so at least one survives the compositor's
		// 60 Hz sampling of an 85 fps game.
		m_nCalibFrames = 12;
		VRLog::Msg("F2 -> field calibration armed for %d frames", m_nCalibFrames);
		g_pLTClient->CPrint("measuring renderer field...");
		return;
	}

	// F1 runs the head-as-mouse experiment: the head turns the player through
	// the same yaw and pitch the mouse writes, and the host submits head-locked
	// so the runtime reprojects nothing at all.
	//
	// This is the A/B the whole warping question has been missing. Every
	// previous test changed one number and asked whether the bending got
	// better, which needs a ranking judgement; this changes which SIDE of the
	// pipeline is responsible and asks only whether the bending is still there,
	// which is the kind of call a headset tester makes reliably.
	//
	// F1 is free: NOLF binds QuickSave to F6 and QuickLoad to F9 through the
	// engine's action system, which OnKeyDown cannot suppress, but nothing is
	// bound to F1 in defctrls.cfg.
	if (key == VK_F1)
	{
		const bool bOn = (g_vtVRHeadAsMouse.GetFloat() <= 0.0f);
		g_vtVRHeadAsMouse.SetFloat(bOn ? 1.0f : 0.0f);
		g_pLTClient->CPrint("VRHeadAsMouse %s", bOn ? "ON" : "OFF");
		VRLog::Msg("F1 -> VRHeadAsMouse %s (%s)", bOn ? "ON" : "OFF",
			bOn ? "head goes through the mouse path, host submits head-locked"
				: "head composed at render time, host declares the pose");
		return;
	}

	if (key == VK_F4)
	{
		const bool bOn = (g_vtVRHeadTracking.GetFloat() <= 0.0f);
		g_vtVRHeadTracking.SetFloat(bOn ? 1.0f : 0.0f);
		g_pLTClient->CPrint("VRHeadTracking %s", bOn ? "ON" : "OFF");
		VRLog::Msg("F4 -> VRHeadTracking %s", bOn ? "ON" : "OFF");
		return;
	}

	// F5 cycles how the head rotation is composed onto the body's aim.
	//
	// This has never been settled. The code picks rSavedRot * rHead and the
	// comment beside it says mode 2 exists "so it can be settled in the headset
	// instead of guessed at" - and then nobody did, for a month.
	//
	// It matters because the two orders differ ONLY when the body aim and the
	// head rotation are both non-trivial. Look straight ahead and they agree;
	// turn the body with the mouse and then turn your head, and they diverge.
	// A camera pointing somewhere other than the pose we declare to the runtime
	// is exactly what makes the compositor warp the image to reconcile them.
	//
	// It also separates two things the head-as-mouse arm changed together. That
	// arm removed the reprojection AND bypassed this composition entirely, by
	// routing the head through the game's own Euler path. Its clean result was
	// read as "reprojection is the cause"; it is equally consistent with "this
	// composition is wrong".
	//
	//   1 = rSavedRot * rHead   (the shipped guess)
	//   2 = rHead * rSavedRot   (the other order)
	//   0 = the old Euler path, exact for one axis at a time
	//
	// F5 used to toggle the render rate between 90 and 60. That question was
	// closed on 13 August - the rate does not affect the ghosting - so the key
	// was doing nothing anyone needed. VRFramerate is still settable from the
	// console if it is ever wanted again.
	if (key == VK_F5)
	{
		const int nNext = ((int)g_vtVRQuatHead.GetFloat() + 1) % 4;
		g_vtVRQuatHead.SetFloat((LTFLOAT)nNext);

		// Measured, not judged. The startup table prints the numbers these
		// labels stand for: mode 1 misdirects the compositor by 0.07 deg (the
		// noise floor), modes 2 and 3 by up to 5.71, and both turn head pitch
		// into camera roll as a function of body yaw. See docs/FRAMES-AGREE.md.
		const char* pszName = (nNext == 0) ? "EULER (one axis at a time)"
							: (nNext == 1) ? "QUAT body*head - SHIPPED, correct"
							: (nNext == 2) ? "QUAT head*body - BROKEN: pitch becomes roll"
										   : "QUAT head*bodyYAW - BROKEN the same way";
		g_pLTClient->CPrint("VRQuatHead %d - %s", nNext, pszName);
		VRLog::Msg("F5 -> VRQuatHead %d (%s)", nNext, pszName);
		return;
	}

	if (key == VK_F12)
	{
		const int nNext = ((int)g_vtVRAsymFrustum.GetFloat() + 1) % 4;
		g_vtVRAsymFrustum.SetFloat((LTFLOAT)nNext);

		const char* pszName = (nNext == 0) ? "OFF (forward axis)"
							: (nNext == 1) ? "VERTICAL centre only"
							: (nNext == 2) ? "VERTICAL + HORIZONTAL"
										   : "VERTICAL + HORIZONTAL (sign flipped)";
		g_pLTClient->CPrint("VRAsymFrustum %d - %s", nNext, pszName);
		VRLog::Msg("F12 -> VRAsymFrustum %d (%s)", nNext, pszName);
		return;
	}

	// F3 toggles the uniform projection.
	//
	// NOT F10. F10 is a Windows SYSTEM key - it opens the window menu and arrives
	// as WM_SYSKEYDOWN, which this engine's WM_KEYDOWN handler never sees. The
	// old "MARKED BEST" handler sat on F10 and has never once fired: across every
	// client log this project has ever written, F2/F3/F4/F5/F7/F8/F9/F12 all
	// appear and F10 does not. It was dead the day it was written and nobody
	// noticed, because nobody had a reason to press it.
	//
	// F3 is proven: it carried the retired VRFovXTest trim and appears 40 times
	// in the logs. Nothing handles it now.
	//
	// ON  = ask the renderer for exactly the field we declare, both axes.
	// OFF = the shipped behaviour, horizontal request inflated 1.6x.
	//
	// JUDGE IT WHILE MOVING, and on one question only: does the world BEND.
	// It will also look narrower, and that is expected - the appearance
	// criterion has produced a wrong answer on this variable twice.
	if (key == VK_F3)
	{
		const bool bOn = (g_vtVRFovUniform.GetFloat() <= 0.0f);
		g_vtVRFovUniform.SetFloat(bOn ? 1.0f : 0.0f);
		g_pLTClient->CPrint("VRFovUniform %s", bOn ? "ON" : "OFF");
		VRLog::Msg("F3 -> VRFovUniform %s (%s)", bOn ? "ON" : "OFF",
			bOn ? "render == declare, uniform scale"
				: "horizontal request inflated, anisotropic");
		return;
	}


#define __ALLOW_EDIT_MODE__
#ifdef __ALLOW_EDIT_MODE__

	if (key == VK_TOGGLE_SCREENSHOTMODE)
	{
#ifndef _DEMO
#ifndef _FINAL
		g_bScreenShotMode = !g_bScreenShotMode;
		m_InterfaceMgr.DrawPlayerStats((!g_bScreenShotMode && !m_editMgr.IsEditMode()));
        g_pLTClient->CPrint("Screen shot mode: %s", g_bScreenShotMode ? "ON" : "OFF");
#endif
#endif
		return;
	}

	if (key == VK_TOGGLE_EDITMODE)
	{
#ifndef _DEMO
#ifndef _FINAL
		m_editMgr.ToggleEditMode();
#endif
#endif
		return;
	}
	else if (m_editMgr.IsEditMode())
	{
		m_editMgr.OnKeyDown(key);
		return;
	}

#endif

	// Let the interface mgr have a go at it...
	if (m_InterfaceMgr.OnKeyDown(key, rep))
	{
		return;
	}

	if (IsPlayerDead())
	{
		if (g_pLTClient->GetTime() > m_fEarliestRespawnTime)
		{
			if (!IsMultiplayerGame())
			{
				HandleMissionFailed();
			}
		}
	}

	// Are we playing a cinematic...

	if (m_bUsingExternalCamera)
	{
		if (key == VK_SPACE && m_InterfaceMgr.GetGameState() != GS_DIALOGUE)
		{
			// Send an activate message to stop the cinemaitc...

			DoActivate(LTFALSE);
			g_pLTClient->ClearInput();
			return;
		}
		else if	(key == VK_F9)
		{
			// NOT WHILE TUNING. F9 sits between the tuner's F8 and F10 and the player
			// reached it by feel with the headset on; a quickload mid-session
			// throws away everything tuned since the last save.
			if (g_vtVRFlashTune.GetFloat() > 0.0f)
			{
				VRLog::Msg("VRFlashTune: F9 (quickload) ignored while tuning");
				return;
			}
			QuickLoad();
			return;
		}
	}

	if (IsMultiplayerGame())
	{
		uint8 nTauntNum = 255;
		switch (key)
		{
		case VK_F4:
			nTauntNum = 0;
			break;
		case VK_F5:
			nTauntNum = 1;
			break;
		case VK_F6:
			nTauntNum = 2;
			break;
		case VK_F7:
			nTauntNum = 3;
			break;
		}

		if (nTauntNum != 255)
		{
			uint32 nLocalID;
			g_pLTClient->GetLocalClientID(&nLocalID);
			DoTaunt(nLocalID,nTauntNum);
		}
	}

}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::OnKeyUp(int key, int rep)
//
//	PURPOSE:	Handle key up notification
//
// ----------------------------------------------------------------------- //
void CGameClientShell::OnKeyUp(int key)
{
	m_InterfaceMgr.OnKeyUp(key);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdatePlayerFlags
//
//	PURPOSE:	Update our copy of the movement flags
//
// ----------------------------------------------------------------------- //

void CGameClientShell::UpdatePlayerFlags()
{
	// Update flags...

	m_dwPlayerFlags = m_MoveMgr.GetControlFlags();

    if (g_pLTClient->IsCommandOn(COMMAND_ID_LOOKUP))
	{
		m_dwPlayerFlags |= BC_CFLG_LOOKUP;
	}

    if (g_pLTClient->IsCommandOn(COMMAND_ID_LOOKDOWN))
	{
		m_dwPlayerFlags |= BC_CFLG_LOOKDOWN;
	}

}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::OnMessage()
//
//	PURPOSE:	Handle client messages
//
// ----------------------------------------------------------------------- //

void CGameClientShell::OnMessage(uint8 messageID, HMESSAGEREAD hMessage)
{
	// Let interface handle message first...

	if (m_InterfaceMgr.OnMessage(messageID, hMessage)) return;

	switch(messageID)
	{
		case MID_TIMEOFDAYCOLOR:
		{
            LTVector vNewColor;
            vNewColor.x = (float)g_pLTClient->ReadFromMessageByte(hMessage) / MAX_WORLDTIME_COLOR;
            vNewColor.y = (float)g_pLTClient->ReadFromMessageByte(hMessage) / MAX_WORLDTIME_COLOR;
            vNewColor.z = (float)g_pLTClient->ReadFromMessageByte(hMessage) / MAX_WORLDTIME_COLOR;
			m_LightScaleMgr.SetTimeOfDayScale(vNewColor);

            LTVector vNewDir;
            vNewDir.x = (float)(char)g_pLTClient->ReadFromMessageByte(hMessage) / 127.0f;
            vNewDir.y = (float)(char)g_pLTClient->ReadFromMessageByte(hMessage) / 127.0f;
            vNewDir.z = (float)(char)g_pLTClient->ReadFromMessageByte(hMessage) / 127.0f;
            g_pLTClient->SetGlobalLightDir(vNewDir);

            LTFLOAT fTodHours = g_pLTClient->ReadFromMessageFloat(hMessage);
			// Hack: ignores it currently...
            g_pLTClient->SetAmbientLight(0.4f);

			if (m_bUseWorldFog)
			{
				ResetGlobalFog();
			}
		}
		break;

		case MID_SERVERFORCEPOS:
		{
			m_MoveMgr.OnServerForcePos(hMessage);

            m_bPlayerPosSet = LTTRUE;
		}
		break;

		case MID_PHYSICS_UPDATE:
		{
			m_MoveMgr.OnPhysicsUpdate(hMessage);
		}
		break;

		case MID_WEAPON_CHANGE :
		{
			ChangeWeapon(hMessage);
		}
		break;

		case MID_CHANGING_LEVELS :
		{
		}
		break;

		case STC_BPRINT :
		{
			char msg[50];
			hMessage->ReadStringFL(msg, sizeof(msg));
			CSPrint(msg);
		}
		break;

		case MID_SHAKE_SCREEN :
		{
            LTVector vAmount;
            g_pLTClient->ReadFromMessageVector(hMessage, &vAmount);
			ShakeScreen(vAmount);
		}
		break;

		case MID_SFX_MESSAGE :
		{
			m_sfxMgr.OnSFXMessage(hMessage);
		}
		break;

		case MID_PLAYER_EXITLEVEL :
		{
			HandleExitLevel(hMessage);
		}
		break;

		case MID_PLAYER_SUMMARY :
		{
			m_PlayerSummary.ReadClientData(hMessage);
			m_InterfaceMgr.ForceFolderUpdate(m_InterfaceMgr.GetCurrentFolder());
		}
		break;

		case MID_PLAYER_STATE_CHANGE :
		{
			HandlePlayerStateChange(hMessage);
		}
		break;

		case MID_PLAYER_AUTOSAVE :
		{
			AutoSave(hMessage);
		}
		break;

		case MID_PLAYER_DAMAGE :
		{
			HandlePlayerDamage(hMessage);
		}
		break;

		case MID_PLAYER_ORIENTATION :
		{
			// Set our pitch, yaw, and roll according to the players...

            LTVector vVec;
            g_pLTClient->ReadFromMessageVector(hMessage, &vVec);

			SDL_Log("Message GET! %f / %f / %f", vVec.x, vVec.y, vVec.z);

			m_fPitch		= vVec.x;
			m_fYaw			= vVec.y;
			m_fRoll			= vVec.z;
			m_fPlayerPitch	= vVec.x;
			m_fPlayerYaw	= vVec.y;
			m_fPlayerRoll	= vVec.z;
			m_fYawBackup	= m_fYaw;
			m_fPitchBackup	= m_fPitch;
		}
		break;

		case MID_COMMAND_TOGGLE :
		{
            uint8 nId = g_pLTClient->ReadFromMessageByte(hMessage);

			switch(nId)
			{
				case COMMAND_ID_RUNLOCK :
				{
					CGameSettings* pSettings = m_InterfaceMgr.GetSettings();
					if (pSettings)
					{
                        pSettings->SetRunLock((LTBOOL)g_pLTClient->ReadFromMessageByte(hMessage));
					}
				}
				break;

				default : break;
			}
		}
		break;

		case MID_MUSIC:
		{
			if (m_Music.IsInitialized())
			{
				m_Music.ProcessMusicMessage(hMessage);
			}
		}
		break;

		case MID_PLAYER_LOADCLIENT :
		{
			UnpackClientSaveMsg(hMessage);
		}
		break;

		case MID_PLAYER_MULTIPLAYER_INIT :
		{
			m_nMPNameId = (int)g_pLTClient->ReadFromMessageDWord(hMessage);;
			m_nMPBriefingId = (int)g_pLTClient->ReadFromMessageDWord(hMessage);;

			if (m_nMPNameId || m_nMPBriefingId)
			{
				// Hide the loading screen since we're not in the playing state yet
				m_InterfaceMgr.HideLoadScreen();
				m_InterfaceMgr.SwitchToFolder(FOLDER_ID_MP_BRIEFING);
				m_InterfaceMgr.StartScreenFadeIn(0.5f);
			}
			else
			{
				InitMultiPlayer();
			}
		}
		break;

		case MID_HANDSHAKE :
		{
			ProcessHandshake(hMessage);
		}
		break;

		case MID_PLAYER_SINGLEPLAYER_INIT :
		{
			InitSinglePlayer();
		}
		break;

		case MID_SERVER_ERROR :
		{
			HandleServerError(hMessage);
		}
		break;

		case MID_MULTIPLAYER_DATA :
		{
			HandleMultiplayerGameData(hMessage);
		}
		break;

		case MID_UPDATE_OPTIONS :
		{
			HandleServerOptions(hMessage);
		}
		break;

		case MID_EDIT_OBJECTINFO :
		{
			m_editMgr.HandleEditObjectInfo(hMessage);
		}
		break;

		case MID_CHANGE_FOG :
		{
			if (m_bUseWorldFog)
			{
				ResetGlobalFog();
			}
		}
		break;

		default : break;
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ShakeScreen()
//
//	PURPOSE:	Shanke, rattle, and roll
//
// ----------------------------------------------------------------------- //

void CGameClientShell::ShakeScreen(LTVector vShake)
{
	// Add...

	VEC_ADD(m_vShakeAmount, m_vShakeAmount, vShake);

	if (m_vShakeAmount.x > MAX_SHAKE_AMOUNT) m_vShakeAmount.x = MAX_SHAKE_AMOUNT;
	if (m_vShakeAmount.y > MAX_SHAKE_AMOUNT) m_vShakeAmount.y = MAX_SHAKE_AMOUNT;
	if (m_vShakeAmount.z > MAX_SHAKE_AMOUNT) m_vShakeAmount.z = MAX_SHAKE_AMOUNT;
}


// ----------------------------------------------------------------------- //
// Console command handlers for recording and playing demos.
// ----------------------------------------------------------------------- //

void CGameClientShell::HandleRecord(int argc, char **argv)
{
	if(argc < 2)
	{
        g_pLTClient->CPrint("Record <world name> <filename>");
		return;
	}

	if(!DoLoadWorld(argv[0], NULL, NULL, LOAD_NEW_GAME, argv[1], NULL))
	{
        g_pLTClient->CPrint("Error starting world");
	}
}

void CGameClientShell::HandlePlaydemo(int argc, char **argv)
{
	if(argc < 1)
	{
        g_pLTClient->CPrint("Playdemo <filename>");
		return;
	}

	if(!DoLoadWorld("asdf", NULL, NULL, LOAD_NEW_GAME, NULL, argv[0]))
	{
        g_pLTClient->CPrint("Error starting world");
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandleCheat()
//
//	PURPOSE:	Handle cheat console command
//
// ----------------------------------------------------------------------- //

void CGameClientShell::HandleCheat(int argc, char **argv)
{
	if (argc < 1 || !g_pCheatMgr) return;

	if (g_pCheatMgr->Check(argv[0]))
	{
		g_pClientSoundMgr->PlayInterfaceSound("Menu\\Snd\\Cheat.wav");
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::FlashScreen()
//
//	PURPOSE:	Tint screen
//
// ----------------------------------------------------------------------- //

void CGameClientShell::FlashScreen(LTVector vFlashColor, LTVector vPos, LTFLOAT fFlashRange,
                                  LTFLOAT fTime, LTFLOAT fRampUp, LTFLOAT fRampDown, LTBOOL bForce)
{
	CGameSettings* pSettings = m_InterfaceMgr.GetSettings();
	if (!pSettings) return;

	if (!bForce && !pSettings->ScreenFlash()) return;

    LTVector vCamPos;
    g_pLTClient->GetObjectPos(m_hCamera, &vCamPos);

	// Determine if we can see this...

    LTVector vDir;
	vDir =  vPos - vCamPos;
    LTFLOAT fDirMag = vDir.Mag();
	if (fDirMag > fFlashRange) return;

	// Okay, not adjust the tint based on the camera's angle to the tint pos.

    LTRotation rRot;
    LTVector vU, vR, vF;
    g_pLTClient->GetObjectRotation(m_hCamera, &rRot);
    g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);

	VEC_NORM(vDir);
	VEC_NORM(vF);
    LTFLOAT fMul = VEC_DOT(vDir, vF);
	if (fMul <= 0.0f) return;

	// {MD} See if we can even see this point.
	ClientIntersectQuery iQuery;
	ClientIntersectInfo iInfo;
	iQuery.m_From = vPos;
	iQuery.m_To = vCamPos;
    if(g_pLTClient->IntersectSegment(&iQuery, &iInfo))
	{
		// Something is in the way.
		return;
	}

	// Tint less if the pos was far away from the camera...

    LTFLOAT fVal = 1.0f - (fDirMag/fFlashRange);
	fMul *= (fVal <= 1.0f ? fVal : 1.0f);

    m_bFlashScreen  = LTTRUE;
    m_fFlashStart   = g_pLTClient->GetTime();
	m_fFlashTime		= fTime;
	m_fFlashRampUp	= fRampUp;
	m_fFlashRampDown	= fRampDown;
	VEC_COPY(m_vFlashColor, vFlashColor);
	VEC_MULSCALAR(m_vFlashColor, m_vFlashColor, fMul);
}



// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateDuck()
//
//	PURPOSE:	Update ducking camera offset
//
// ----------------------------------------------------------------------- //

void CGameClientShell::UpdateDuck()
{
	// Can't duck when free movement...

    if (m_MoveMgr.IsBodyInLiquid() || m_MoveMgr.IsBodyOnLadder() ||
        IsFreeMovement(m_eCurContainerCode)) return;


    LTFLOAT fTime = g_pLTClient->GetTime();

	if (m_dwPlayerFlags & BC_CFLG_DUCK)
	{
        m_bStartedDuckingUp = LTFALSE;

		// See if the duck just started...

		if (!m_bStartedDuckingDown)
		{
            m_bStartedDuckingDown = LTTRUE;
			m_fStartDuckTime = fTime;
		}

		m_fCamDuck = m_fDuckDownV * (fTime - m_fStartDuckTime);

		if (m_fCamDuck < m_fMaxDuckDistance)
		{
			m_fCamDuck = m_fMaxDuckDistance;
		}

	}
	else if (m_fCamDuck < 0.0) // Raise up
	{
        m_bStartedDuckingDown = LTFALSE;

		if (!m_bStartedDuckingUp)
		{
			m_fStartDuckTime = fTime;
            m_bStartedDuckingUp = LTTRUE;
		}

		m_fCamDuck += m_fDuckUpV * (fTime - m_fStartDuckTime);

		if (m_fCamDuck > 0.0f)
		{
			m_fCamDuck = 0.0f;
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::PauseGame()
//
//	PURPOSE:	Pauses/Unpauses the server
//
// ----------------------------------------------------------------------- //

void CGameClientShell::PauseGame(LTBOOL bPause, LTBOOL bPauseSound)
{
	m_bGamePaused = bPause;

	if (!IsMultiplayerGame())
	{
        HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(bPause ? MID_GAME_PAUSE : MID_GAME_UNPAUSE);
        g_pLTClient->EndMessage(hMessage);
	}

	if (bPause && bPauseSound)
	{
        g_pLTClient->PauseSounds();
	}
	else
	{
        g_pLTClient->ResumeSounds();
	}

	SetInputState(!bPause && m_bAllowPlayerMovement);
	SetMouseInput(!bPause);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::SetInputState()
//
//	PURPOSE:	Allows/disallows input
//
// ----------------------------------------------------------------------- //

void CGameClientShell::SetInputState(LTBOOL bAllowInput)
{
    g_pLTClient->SetInputState(bAllowInput);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::SetMouseInput()
//
//	PURPOSE:	Allows or disallows mouse input on the client
//
// ----------------------------------------------------------------------- //

void CGameClientShell::SetMouseInput(LTBOOL bAllowInput)
{
	if (bAllowInput)
	{
        m_bRestoreOrientation = LTTRUE;
		m_fYaw   = m_fYawBackup;
		m_fPitch = m_fPitchBackup;
	}
	else
	{
		m_fYawBackup = m_fYaw;
		m_fPitchBackup = m_fPitch;
	}
}

void CGameClientShell::AllowPlayerMovement(LTBOOL bAllowPlayerMovement)
{
	m_bAllowPlayerMovement = bAllowPlayerMovement;
	SetMouseInput(bAllowPlayerMovement);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandlePlayerStateChange()
//
//	PURPOSE:	Update player state change
//
// ----------------------------------------------------------------------- //

void CGameClientShell::HandlePlayerStateChange(HMESSAGEREAD hMessage)
{
    m_ePlayerState = (PlayerState) g_pLTClient->ReadFromMessageByte(hMessage);

	switch (m_ePlayerState)
	{
		case PS_DYING:
		{
			AttachCameraToHead(LTTRUE);

			if (IsMultiplayerGame())
			{
				m_InterfaceMgr.GetPlayerStats()->ResetInventory();
                m_fEarliestRespawnTime = g_pLTClient->GetTime() + g_vtMultiplayerRespawnWaitTime.GetFloat();
			}
			else
			{
                m_fEarliestRespawnTime = g_pLTClient->GetTime() + g_vtRespawnWaitTime.GetFloat();
			}

			HandleZoomChange(m_weaponModel.GetWeaponId(), LTTRUE);
			EndZoom();
			m_InterfaceMgr.EndUnderwater();

            m_weaponModel.SetVisible(LTFALSE);
			m_weaponModel.Reset();

            m_InterfaceMgr.SetDrawInterface(LTFALSE);
			m_InterfaceMgr.AddToClearScreenCount();

			if (GetGameType() == SINGLE)
			{
				HSTRING hStr = g_pLTClient->FormatString(IDS_YOUWEREKILLED);
				CSPrint(g_pLTClient->GetStringData(hStr));
			    g_pLTClient->FreeString(hStr);
				m_InterfaceMgr.StartScreenFadeOut(g_vtScreenFadeOutTime.GetFloat());
			}
		}
		break;

		case PS_ALIVE:
		{
			m_DamageFXMgr.Clear();					// Remove all the damage sfx
			m_InterfaceMgr.ForceScreenFadeIn(g_vtScreenFadeInTime.GetFloat());

			AttachCameraToHead(LTFALSE);

            m_InterfaceMgr.SetDrawInterface(LTTRUE);
            m_InterfaceMgr.DrawPlayerStats(LTTRUE);

            SetExternalCamera(LTFALSE);

            m_weaponModel.Disable(LTFALSE);
        }
		break;

		case PS_DEAD:
		{
			m_DamageFXMgr.Clear();					// Remove all the damage sfx
			s_fDeadTimer = 0.0f;
		}
		break;

		case PS_GHOST:
		{
			AttachCameraToHead(LTFALSE);
            m_weaponModel.SetVisible(LTFALSE);

            m_InterfaceMgr.SetDrawInterface(LTFALSE);
            m_InterfaceMgr.DrawPlayerStats(LTFALSE);

			m_InterfaceMgr.ForceScreenFadeIn(g_vtScreenFadeInTime.GetFloat());

			m_InterfaceMgr.GetMessageMgr()->AddLine(IDS_NO_RESPAWN);

/*
            SetExternalCamera(LTTRUE);
*/
            m_weaponModel.Disable(LTTRUE);

        }
		break;

		default : break;
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::AutoSave()
//
//	PURPOSE:	Autosave the game
//
// ----------------------------------------------------------------------- //

void CGameClientShell::AutoSave(HMESSAGEREAD hMessage)
{
	if (m_ePlayerState != PS_ALIVE) return;

	// Save the game for reloading if the player just changed levels
	// or if the player just started a game...
	int missionNum = -1;
	int sceneNum = -1;

	if (!g_pGameClientShell->IsCustomLevel())
	{
		missionNum = g_pGameClientShell->GetCurrentMission();
		sceneNum = g_pGameClientShell->GetCurrentLevel();
	}




	time_t seconds;
	time (&seconds);

	char strSaveGame[256];
	sprintf (strSaveGame, "%s|%d,%d|%ld",m_strCurrentWorldName, missionNum, sceneNum, (long)seconds);


	CWinUtil::WinWritePrivateProfileString(GAME_NAME, "Reload", strSaveGame, SAVEGAMEINI_FILENAME);
	SaveGame(RELOADLEVEL_FILENAME);

}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandlePlayerDamage
//
//	PURPOSE:	Handle the player getting damaged
//
// ----------------------------------------------------------------------- //

void CGameClientShell::HandlePlayerDamage(HMESSAGEREAD hMessage)
{
	if (!hMessage || g_bScreenShotMode) return;

    LTVector vDir;
    g_pLTClient->ReadFromMessageVector(hMessage, &vDir);
    DamageType eType = (DamageType) g_pLTClient->ReadFromMessageByte(hMessage);
    LTBOOL bUsingDamage = (LTBOOL) g_pLTClient->ReadFromMessageByte(hMessage);

    LTFLOAT fPercent = VEC_MAG(vDir);

	LTFLOAT fLowValue = bUsingDamage ? 0.5f : 0.0f;
    LTFLOAT fColor = fLowValue + fPercent;
	fColor = fColor > 1.0f ? 1.0f : fColor;

    LTFLOAT fRampDown = fLowValue + fPercent * 2.0f;

	LTVector vFlashColor(fColor, 0.0f, 0.0f);
 	if (!bUsingDamage)
	{
		if (fColor > 0.7f) fColor = 0.7f;
	    vFlashColor.Init(0.2f, 0.2f, fColor);
    }

	LTFLOAT fRampUp = 0.2f, fFlashTime = 0.1f;

    LTVector vCamPos;
    g_pLTClient->GetObjectPos(m_hCamera, &vCamPos);

    LTRotation rRot;
    g_pLTClient->GetObjectRotation(m_hCamera, &rRot);

    LTVector vU, vR, vF;
    g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);

	VEC_MULSCALAR(vF, vF, 10.0f);
	VEC_ADD(vCamPos, vCamPos, vF);

    FlashScreen(vFlashColor, vCamPos, 1000.0f, fRampUp, fFlashTime, fRampDown, LTTRUE);

	// Tilt the camera based on the direction the damage came from...

	if (bUsingDamage && IsJarCameraType(eType) && m_PlayerCamera.IsFirstPerson() &&
		g_vtCamDamage.GetFloat() > 0.0f && fPercent > 0.0f)
	{
        LTRotation rRot;
        LTVector vU, vR, vF;
		GetPlayerRotation(&rRot);
        g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);

		vDir.Norm();
        LTFLOAT fMul = VEC_DOT(vDir, vF);

		CameraDelta delta;

		// NO CAMERA KICK ON DAMAGE IN VR EITHER. In the headset getting shot had the
		// same head-bob problem. The red flash stays; the pitch nod goes.
		const bool bVRNoKick = VRShared::IsLive() && g_vtVRRecoil.GetFloat() <= 0.0f;
		if (g_vtCamDamagePitch.GetFloat() > 0.0f && !bVRNoKick)
		{
            // g_pLTClient->CPrint("Damage Pitch Val: %.2f", fMul);

			// Hit from the back...

			if (fMul > g_vtCamDamagePitchMin.GetFloat())
			{
				delta.Pitch.fTime1	= g_vtCamDamageTime1.GetFloat();
				delta.Pitch.fTime2	= g_vtCamDamageTime2.GetFloat();
				delta.Pitch.eWave1	= Wave_SlowOff;
				delta.Pitch.eWave2	= Wave_SlowOff;
				delta.Pitch.fVar	= DEG2RAD(g_vtCamDamageVal.GetFloat());
			}
			else if (fMul < - g_vtCamDamagePitchMin.GetFloat()) // Hit from the front...
			{
				delta.Pitch.fTime1	= g_vtCamDamageTime1.GetFloat();
				delta.Pitch.fTime2	= g_vtCamDamageTime2.GetFloat();
				delta.Pitch.eWave1	= Wave_SlowOff;
				delta.Pitch.eWave2	= Wave_SlowOff;
				delta.Pitch.fVar	= -DEG2RAD(g_vtCamDamageVal.GetFloat());
			}
		}

		if (g_vtCamDamageRoll.GetFloat() > 0.0f)
		{
			fMul = VEC_DOT(vDir, vR);
            //g_pLTClient->CPrint("Damage Roll Val: %.2f", fMul);

			// Hit from the left...

			if (fMul > g_vtCamDamageRollMin.GetFloat())
			{
				delta.Roll.fTime1	= g_vtCamDamageTime1.GetFloat();
				delta.Roll.fTime2	= g_vtCamDamageTime2.GetFloat();
				delta.Roll.eWave1	= Wave_SlowOff;
				delta.Roll.eWave2	= Wave_SlowOff;
				delta.Roll.fVar		= DEG2RAD(g_vtCamDamageVal.GetFloat());
			}
			else if (fMul < - g_vtCamDamageRollMin.GetFloat()) // Hit from the right...
			{
				delta.Roll.fTime1	= g_vtCamDamageTime1.GetFloat();
				delta.Roll.fTime2	= g_vtCamDamageTime2.GetFloat();
				delta.Roll.eWave1	= Wave_SlowOff;
				delta.Roll.eWave2	= Wave_SlowOff;
				delta.Roll.fVar		= -DEG2RAD(g_vtCamDamageVal.GetFloat());
			}
		}

		m_CameraOffsetMgr.AddDelta(delta);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandleServerError()
//
//	PURPOSE:	Handle any error messages sent from the server
//
// ----------------------------------------------------------------------- //

void CGameClientShell::HandleServerError(HMESSAGEREAD hMessage)
{
	if (!hMessage) return;

    uint8 nError = g_pLTClient->ReadFromMessageByte(hMessage);
	switch (nError)
	{
		case SERROR_SAVEGAME :
		{
			//DoMessageBox(IDS_SAVEGAMEFAILED, TH_ALIGN_CENTER);
		}
		break;

		case SERROR_LOADGAME :
		{
			//DoMessageBox(IDS_NOLOADLEVEL, TH_ALIGN_CENTER);

			m_InterfaceMgr.ChangeState(GS_FOLDER);
		}
		break;

		default : break;
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandleMultiplayerGameData()
//
//	PURPOSE:	Handle global game data sent from the server
//
// ----------------------------------------------------------------------- //

void CGameClientShell::HandleMultiplayerGameData(HMESSAGEREAD hMessage)
{
	if (!hMessage) return;

    uint8 byGameType = g_pLTClient->ReadFromMessageByte(hMessage);
	m_eGameType = (GameType)byGameType;

	if (m_eGameType != SINGLE)
	{
		hMessage->ReadStringFL(m_szServerAddress, sizeof(m_szServerAddress));
		uint32 tmp = g_pLTClient->ReadFromMessageDWord(hMessage);
		m_nServerPort = (int)tmp;

	}

	if (m_eGameType == COOPERATIVE_ASSAULT)
	{
		m_InterfaceMgr.GetPlayerStats()->SetMultiplayerObjectives(hMessage);

		LTBOOL bFragScore = g_pLTClient->ReadFromMessageByte(hMessage);

	}
}
// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandleServerOptions()
//
//	PURPOSE:	Handle game option data sent from the server
//
// ----------------------------------------------------------------------- //

void CGameClientShell::HandleServerOptions(HMESSAGEREAD hMessage)
{
	if (!hMessage) return;
	if (m_eGameType == SINGLE) return;

	hMessage->ReadStringFL(m_szServerName, sizeof(m_szServerName));

	int	nNumOptions = (int)g_pServerOptionMgr->GetNumOptions();
	if (nNumOptions > MAX_GAME_OPTIONS)
		nNumOptions = MAX_GAME_OPTIONS;
	for (int i = 0; i < nNumOptions; i++)
	{
		OPTION* pOpt = g_pServerOptionMgr->GetOption(i);
		if (GetGameType() == pOpt->eGameType || pOpt->eGameType == SINGLE)
		{
			m_fServerOptions[i] =  g_pLTClient->ReadFromMessageFloat(hMessage);

		}
		else
			m_fServerOptions[i] = 0.0f;
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateWeaponMuzzlePosition()
//
//	PURPOSE:	Update the current weapon muzzle pos
//
// ----------------------------------------------------------------------- //

void CGameClientShell::UpdateWeaponMuzzlePosition()
{
    LTFLOAT fIncValue = WEAPON_MOVE_INC_VALUE_SLOW;
    LTBOOL bChanged = LTFALSE;

    LTVector vOffset;
	VEC_INIT(vOffset);


	// Move weapon faster if running...

	if (m_dwPlayerFlags & BC_CFLG_RUN)
	{
		fIncValue = WEAPON_MOVE_INC_VALUE_FAST;
	}


	vOffset = m_weaponModel.GetMuzzleOffset();


	// Move weapon forward or backwards...

	if ((m_dwPlayerFlags & BC_CFLG_FORWARD) || (m_dwPlayerFlags & BC_CFLG_REVERSE))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_FORWARD ? fIncValue : -fIncValue;
		vOffset.z += fIncValue;
        bChanged = LTTRUE;
	}


	// Move the weapon to the player's right or left...

	if ((m_dwPlayerFlags & BC_CFLG_STRAFE_RIGHT) ||
		(m_dwPlayerFlags & BC_CFLG_STRAFE_LEFT))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_STRAFE_RIGHT ? fIncValue : -fIncValue;
		vOffset.x += fIncValue;
        bChanged = LTTRUE;
	}


	// Move the weapon up or down relative to the player...

	if ((m_dwPlayerFlags & BC_CFLG_JUMP) || (m_dwPlayerFlags & BC_CFLG_DUCK))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_DUCK ? -fIncValue : fIncValue;
		vOffset.y += fIncValue;
        bChanged = LTTRUE;
	}


	// Okay, set the offset...

	if (bChanged)
	{
		m_weaponModel.SetMuzzleOffset(vOffset);
	}

	//if (bChanged)
	//{
	//	CSPrint ("Muzzle offset = %f, %f, %f", vOffset.x, vOffset.y, vOffset.z);
	//}
}



// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateWeaponPosition()
//
//	PURPOSE:	Update the position of the current weapon
//
// ----------------------------------------------------------------------- //

void CGameClientShell::UpdateWeaponPosition()
{
    LTFLOAT fIncValue = WEAPON_MOVE_INC_VALUE_SLOW;
    LTBOOL bChanged = LTFALSE;

    LTVector vOffset;
	VEC_INIT(vOffset);


	// Move weapon faster if running...

	if (m_dwPlayerFlags & BC_CFLG_RUN)
	{
		fIncValue = WEAPON_MOVE_INC_VALUE_FAST;
	}


	vOffset = m_weaponModel.GetWeaponOffset();


	// Move weapon forward or backwards...

	if ((m_dwPlayerFlags & BC_CFLG_FORWARD) || (m_dwPlayerFlags & BC_CFLG_REVERSE))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_FORWARD ? fIncValue : -fIncValue;
		vOffset.z += fIncValue;
        bChanged = LTTRUE;
	}


	// Move the weapon to the player's right or left...

	if ((m_dwPlayerFlags & BC_CFLG_STRAFE_RIGHT) ||
		(m_dwPlayerFlags & BC_CFLG_STRAFE_LEFT))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_STRAFE_RIGHT ? fIncValue : -fIncValue;
		vOffset.x += fIncValue;
        bChanged = LTTRUE;
	}


	// Move the weapon up or down relative to the player...

	if ((m_dwPlayerFlags & BC_CFLG_JUMP) || (m_dwPlayerFlags & BC_CFLG_DUCK))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_DUCK ? -fIncValue : fIncValue;
		vOffset.y += fIncValue;
        bChanged = LTTRUE;
	}


	// Okay, set the offset...

	if (bChanged)
	{
		m_weaponModel.SetWeaponOffset(vOffset);
	}

	//if (bChanged)
	//{
	//	CSPrint ("Weapon offset = %f, %f, %f", vOffset.x, vOffset.y, vOffset.z);
	//}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::AdjustWeaponBreach()
//
//	PURPOSE:	Update the position of the current hand-held weapon breach offset
//
// ----------------------------------------------------------------------- //

void CGameClientShell::AdjustWeaponBreach()
{
    LTFLOAT fIncValue = WEAPON_MOVE_INC_VALUE_SLOW;
    LTBOOL bChanged = LTFALSE;

	// Move breach offset faster if running...

	if (m_dwPlayerFlags & BC_CFLG_RUN)
	{
		fIncValue = WEAPON_MOVE_INC_VALUE_FAST;
	}

	WEAPON* pWeapon = g_pWeaponMgr->GetWeapon(m_weaponModel.GetWeaponId());
	if (!pWeapon) return;

    LTFLOAT fBreach = pWeapon->fHHBreachOffset;


	// Move weapon breach offset forward or backwards...

	if ((m_dwPlayerFlags & BC_CFLG_FORWARD) || (m_dwPlayerFlags & BC_CFLG_REVERSE))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_FORWARD ? fIncValue : -fIncValue;
		fBreach += fIncValue;
        bChanged = LTTRUE;
	}

	if (bChanged)
	{
		// Okay, set the offset...

		WEAPON* pWeaponData = g_pWeaponMgr->GetWeapon(m_weaponModel.GetWeaponId());
		if (pWeaponData)
		{
			pWeaponData->fHHBreachOffset = fBreach;
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::Adjust1stPersonCamera()
//
//	PURPOSE:	Update the 1st-person camera offset
//
// ----------------------------------------------------------------------- //

void CGameClientShell::Adjust1stPersonCamera()
{
    LTFLOAT fIncValue = 0.1f;
    LTBOOL bChanged = LTFALSE;

	// Move breach offset faster if running...

	if (m_dwPlayerFlags & BC_CFLG_RUN)
	{
		fIncValue = fIncValue * 2.0f;
	}

	// Move 1st person offset.x forward or backwards...

	if ((m_dwPlayerFlags & BC_CFLG_FORWARD) || (m_dwPlayerFlags & BC_CFLG_REVERSE))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_FORWARD ? fIncValue : -fIncValue;
		g_vPlayerCameraOffset.x += fIncValue;
        bChanged = LTTRUE;
	}

	// Move 1st person offset.y up or down...

	if ((m_dwPlayerFlags & BC_CFLG_JUMP) || (m_dwPlayerFlags & BC_CFLG_DUCK))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_JUMP ? fIncValue : -fIncValue;
		g_vPlayerCameraOffset.y += fIncValue;
        bChanged = LTTRUE;
	}


	if (bChanged)
	{
		// Okay, set the offset...

		m_PlayerCamera.SetFirstPersonOffset(g_vPlayerCameraOffset);
        g_pLTClient->CPrint("1st person camera offset: %.2f, %.2f, %.2f", g_vPlayerCameraOffset.x, g_vPlayerCameraOffset.y, g_vPlayerCameraOffset.z);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::AdjustLightScale()
//
//	PURPOSE:	Update the current global light scale
//
// ----------------------------------------------------------------------- //

void CGameClientShell::AdjustLightScale()
{
    LTFLOAT fIncValue = 0.01f;
    LTBOOL bChanged = LTFALSE;

    LTVector vScale;
	VEC_INIT(vScale);

    g_pLTClient->GetGlobalLightScale(&vScale);

	// Move faster if running...

	if (m_dwPlayerFlags & BC_CFLG_RUN)
	{
		fIncValue = .5f;
	}


	// Move Red up/down...

	if ((m_dwPlayerFlags & BC_CFLG_FORWARD) || (m_dwPlayerFlags & BC_CFLG_REVERSE))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_FORWARD ? fIncValue : -fIncValue;
		vScale.x += fIncValue;
		vScale.x = vScale.x < 0.0f ? 0.0f : (vScale.x > 1.0f ? 1.0f : vScale.x);

        bChanged = LTTRUE;
	}


	// Move Green up/down...

	if ((m_dwPlayerFlags & BC_CFLG_STRAFE_RIGHT) ||
		(m_dwPlayerFlags & BC_CFLG_STRAFE_LEFT))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_STRAFE_RIGHT ? fIncValue : -fIncValue;
		vScale.y += fIncValue;
		vScale.y = vScale.y < 0.0f ? 0.0f : (vScale.y > 1.0f ? 1.0f : vScale.y);

        bChanged = LTTRUE;
	}


	// Move Blue up/down...

	if ((m_dwPlayerFlags & BC_CFLG_JUMP) || (m_dwPlayerFlags & BC_CFLG_DUCK))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_DUCK ? -fIncValue : fIncValue;
		vScale.z += fIncValue;
		vScale.z = vScale.z < 0.0f ? 0.0f : (vScale.z > 1.0f ? 1.0f : vScale.z);

        bChanged = LTTRUE;
	}


	// Okay, set the light scale.

    g_pLTClient->SetGlobalLightScale(&vScale);

	if (bChanged)
	{
		CSPrint ("Light Scale = %f, %f, %f", vScale.x, vScale.y, vScale.z);
	}

}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::AdjustLightAdd()
//
//	PURPOSE:	Update the current global light add
//
// ----------------------------------------------------------------------- //

void CGameClientShell::AdjustLightAdd()
{
    LTFLOAT fIncValue = 0.01f;
    LTBOOL bChanged = LTFALSE;

    LTVector vScale;
	VEC_INIT(vScale);

    g_pLTClient->GetCameraLightAdd(m_hCamera, &vScale);

	// Move faster if running...

	if (m_dwPlayerFlags & BC_CFLG_RUN)
	{
		fIncValue = .5f;
	}


	// Move Red up/down...

	if ((m_dwPlayerFlags & BC_CFLG_FORWARD) || (m_dwPlayerFlags & BC_CFLG_REVERSE))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_FORWARD ? fIncValue : -fIncValue;
		vScale.x += fIncValue;
		vScale.x = vScale.x < 0.0f ? 0.0f : (vScale.x > 1.0f ? 1.0f : vScale.x);

        bChanged = LTTRUE;
	}


	// Move Green up/down...

	if ((m_dwPlayerFlags & BC_CFLG_STRAFE_RIGHT) ||
		(m_dwPlayerFlags & BC_CFLG_STRAFE_LEFT))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_STRAFE_RIGHT ? fIncValue : -fIncValue;
		vScale.y += fIncValue;
		vScale.y = vScale.y < 0.0f ? 0.0f : (vScale.y > 1.0f ? 1.0f : vScale.y);

        bChanged = LTTRUE;
	}


	// Move Blue up/down...

	if ((m_dwPlayerFlags & BC_CFLG_JUMP) || (m_dwPlayerFlags & BC_CFLG_DUCK))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_DUCK ? -fIncValue : fIncValue;
		vScale.z += fIncValue;
		vScale.z = vScale.z < 0.0f ? 0.0f : (vScale.z > 1.0f ? 1.0f : vScale.z);

        bChanged = LTTRUE;
	}


	// Okay, set the light add.

    g_pLTClient->SetCameraLightAdd(m_hCamera, &vScale);

	if (bChanged)
	{
		CSPrint ("Light Add = %f, %f, %f", vScale.x, vScale.y, vScale.z);
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::AdjustFOV()
//
//	PURPOSE:	Update the current FOV
//
// ----------------------------------------------------------------------- //

void CGameClientShell::AdjustFOV()
{
    LTFLOAT fIncValue = 0.001f;
    LTBOOL bChanged = LTFALSE;

    LTFLOAT fCurFOVx, fCurFOVy;

	// Save the current camera fov...

    g_pLTClient->GetCameraFOV(m_hCamera, &fCurFOVx, &fCurFOVy);


	// Move faster if running...

	if (m_dwPlayerFlags & BC_CFLG_RUN)
	{
		fIncValue = .01f;
	}


	// Adjust X

	if ((m_dwPlayerFlags & BC_CFLG_STRAFE_RIGHT) || (m_dwPlayerFlags & BC_CFLG_STRAFE_LEFT))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_STRAFE_RIGHT ? fIncValue : -fIncValue;
		fCurFOVx += fIncValue;

        bChanged = LTTRUE;
	}


	// Adjust Y

	if ((m_dwPlayerFlags & BC_CFLG_JUMP) || (m_dwPlayerFlags & BC_CFLG_DUCK))
	{
		fIncValue = m_dwPlayerFlags & BC_CFLG_DUCK ? -fIncValue : fIncValue;
		fCurFOVy += fIncValue;

        bChanged = LTTRUE;
	}


	// Okay, set the FOV..

	// Adjust the fov...

	SetCameraFOV(fCurFOVx, fCurFOVy);

	if (bChanged)
	{
		CSPrint ("FOV X = %.2f, FOV Y = %.2f", RAD2DEG(fCurFOVx), RAD2DEG(fCurFOVy));
	}
}



// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::SpecialEffectNotify()
//
//	PURPOSE:	Handle creation of a special fx
//
// ----------------------------------------------------------------------- //

void CGameClientShell::SpecialEffectNotify(HLOCALOBJ hObj, HMESSAGEREAD hMessage)
{
	if (hObj)
	{
        uint32 dwCFlags = g_pLTClient->GetObjectClientFlags(hObj);
        g_pLTClient->SetObjectClientFlags(hObj, dwCFlags | CF_NOTIFYREMOVE);
	}

	m_sfxMgr.HandleSFXMsg(hObj, hMessage);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::OnObjectRemove()
//
//	PURPOSE:	Handle removal of a server created object...
//
// ----------------------------------------------------------------------- //

void CGameClientShell::OnObjectRemove(HLOCALOBJ hObj)
{
	if (!hObj) return;

	// WHO IS TAKING THE SHELL CASINGS. In the headset all shell casings disappeared
	// after walking around and coming back. Everything client-side was
	// measured and cleared - the lifetime is 100000 seconds, the list caps are
	// 3000, the transport carries 4096, and they rest correctly on the floor -
	// so the objects are being removed by the ENGINE and this notification is
	// where the FX list finds out.
	//
	// The question is on what basis: distance would say one thing, a level
	// transition another, an object budget a third. So it logs the distance
	// from the camera and how many are left, capped, and only for objects that
	// are actually in the casing list.
	{
		static long s_nSaidRemove = 0;
		if (s_nSaidRemove < 20)
		{
			CSpecialFXList* pShell = m_sfxMgr.GetFXList(SFX_SHELLCASING_ID);
			bool bWasCasing = false;
			if (pShell)
			{
				for (int z = 0; z < pShell->GetSize(); ++z)
				{
					CSpecialFX* pF = (*pShell)[z];
					if (pF && pF->GetObject() == hObj) { bWasCasing = true; break; }
				}
			}
			if (bWasCasing)
			{
				++s_nSaidRemove;
				LTVector vp(0.0f, 0.0f, 0.0f), vc(0.0f, 0.0f, 0.0f);
				g_pLTClient->GetObjectPos(hObj, &vp);
				if (m_hCamera) g_pLTClient->GetObjectPos(m_hCamera, &vc);
				VRLog::Msg("VRCasingGone: the ENGINE removed a casing %.0f units"
						   " from the camera; %d of %d left in the list",
						   (vp - vc).Mag(), pShell->GetNumItems(),
						   pShell->GetSize());
			}
		}
	}

	m_sfxMgr.RemoveSpecialFX(hObj);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandleExitLevel()
//
//	PURPOSE:	Handle ExitLevel console command
//
// ----------------------------------------------------------------------- //

void CGameClientShell::HandleExitLevel(int argc, char **argv)
{
	// Tell the server to exit the level...
	if (GetGameType() == SINGLE)
	{
	    HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_PLAYER_EXITLEVEL);
		g_pLTClient->EndMessage(hMessage);
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandleExitLevel()
//
//	PURPOSE:	Update player state change
//
// ----------------------------------------------------------------------- //

void CGameClientShell::HandleExitLevel(HMESSAGEREAD hMessage)
{
	if (GetGameType() != SINGLE)
	{
		m_eLevelEnd = (LevelEnd)g_pLTClient->ReadFromMessageByte(hMessage);
		m_nEndString = (int)g_pLTClient->ReadFromMessageDWord(hMessage);
		m_InterfaceMgr.SwitchToFolder(FOLDER_ID_MP_SUMMARY);
		m_InterfaceMgr.StartScreenFadeIn(0.5f);
	}
	else
	{
		// Nothing in the message...currently...
		ExitLevel();
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ExitLevel()
//
//	PURPOSE:	Exit this level and go to the next level
//
// ----------------------------------------------------------------------- //

void CGameClientShell::ExitLevel()
{
    g_pLTClient->ClearInput(); // Start next level with a clean slate

	TurnOffAlternativeCamera(CT_FULLSCREEN);


	// We are officially no longer in a world...

    m_bInWorld = LTFALSE;

	// Go to the the next level...
	SetCurrentLevel(m_nCurrentLevel + 1);

	MISSION* pMission = g_pMissionMgr->GetMission(m_nCurrentMission);
	if (!pMission)
	{
        g_pLTClient->CPrint("ERROR in CGameClientShell::ExitLevel():");
        g_pLTClient->CPrint("      Invalid mission %d!", m_nCurrentMission);
		return;
	}

	// See if we finished a mission...

	if (m_nCurrentLevel >= pMission->nNumLevels)
	{
		//do end of mission clean up
		GetPlayerSummary()->CompleteMission(m_nCurrentMission);

		SetCurrentLevel(0);
		SetCurrentMission(m_nCurrentMission + 1);



		// Check to see if the game is over...

		if (m_nCurrentMission >= g_pMissionMgr->GetNumMissions())
		{
			// TODO: Do game over...

			// For now just start over at the beginning...
			SetCurrentMission(0);

			// Show the mission summary...

			m_InterfaceMgr.SwitchToFolder(FOLDER_ID_MISSION);
		}
		else
		{
			// Show the mission summary...

			m_InterfaceMgr.SwitchToFolder(FOLDER_ID_MISSION);
		}
	}
	else
	{
		// Load the next level...

		if (!LoadCurrentLevel())
		{
            g_pLTClient->CPrint("ERROR in CGameClientShell::ExitLevel():");
            g_pLTClient->CPrint("      Couldn't start level %d!", m_nCurrentLevel);
		}
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::SetSpectatorMode()
//
//	PURPOSE:	Turn spectator mode on/off
//
// ----------------------------------------------------------------------- //

void CGameClientShell::SetSpectatorMode(LTBOOL bOn)
{
	m_bSpectatorMode = bOn;

	// Don't show stats in spectator mode...

	m_InterfaceMgr.DrawPlayerStats(!bOn);

	if (m_PlayerCamera.IsFirstPerson())
	{
        ShowPlayer(LTFALSE);

		m_MoveMgr.SetSpectatorMode(bOn);
	}

	m_weaponModel.SetVisible(!m_bSpectatorMode);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::LoadWorld()
//
//	PURPOSE:	Handles loading a world (with AutoSave)
//
// ----------------------------------------------------------------------- //

LTBOOL CGameClientShell::LoadWorld(char* pWorldFile, char* pCurWorldSaveFile,
                                  char* pRestoreObjectsFile, uint8 nFlags)
{
	// Auto save the newly loaded level...
	return DoLoadWorld(pWorldFile, pCurWorldSaveFile, pRestoreObjectsFile, nFlags);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::DoLoadWorld()
//
//	PURPOSE:	Does actual work of loading a world
//
// ----------------------------------------------------------------------- //

LTBOOL CGameClientShell::DoLoadWorld(char* pWorldFile, char* pCurWorldSaveFile,
                                    char* pRestoreObjectsFile, uint8 nFlags,
									char *pRecordFile, char *pPlaydemoFile)
{
    if (!pWorldFile) return LTFALSE;


	CMissionData* pMissionData = m_InterfaceMgr.GetMissionData();
	_ASSERT(pMissionData);
    if (!pMissionData) return LTFALSE;


	// Make sure the FOV is set correctly...

    uint32 dwWidth = 640;
    uint32 dwHeight = 480;
    g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &dwWidth, &dwHeight);

    g_pLTClient->SetCameraRect(m_hCamera, LTFALSE, 0, 0, dwWidth, dwHeight);
	SetCameraFOV(DEG2RAD(g_vtFOVXNormal.GetFloat()), DEG2RAD(g_vtFOVYNormal.GetFloat()));

	// See if the loaded world is a custom level or not...

	int nMissionId, nLevel;
	if (g_pMissionMgr->IsMissionLevel(pWorldFile, nMissionId, nLevel))
	{
		SetCurrentMission(nMissionId);
		SetCurrentLevel(nLevel);
        m_bIsCustomLevel    = LTFALSE;

		// Set the level in the mission data...
		if (nMissionId != pMissionData->GetMissionNum())
		{
			pMissionData->NewMission(nMissionId);

			CPlayerStats* pStats = m_InterfaceMgr.GetPlayerStats();
			pStats->ClearMissionDamage();
		}

		MISSION* pMission = g_pMissionMgr->GetMission(m_nCurrentMission);
		if (pMission)
		{
			int missionId = pMission->nNameId;
			HSTRING hTxt=g_pLTClient->FormatString(missionId);
			if (pMission->nNumLevels == 1)
			{
				g_pInterfaceMgr->SetLoadLevelString(hTxt);
			}
			else
			{
				char tmp[128];
				HSTRING hTxt2=g_pLTClient->FormatString(IDS_SCENENUMBER,m_nCurrentLevel + 1);
				sprintf(tmp,"%s, %s",g_pLTClient->GetStringData(hTxt),g_pLTClient->GetStringData(hTxt2));
				HSTRING hWorld =g_pLTClient->CreateString(tmp);
				g_pInterfaceMgr->SetLoadLevelString(hWorld);
				g_pLTClient->FreeString(hWorld);
				g_pLTClient->FreeString(hTxt2);
			}
			if (pMission->szPhoto)
			{
				g_pInterfaceMgr->SetLoadLevelPhoto(pMission->szPhoto);
			}
			else
			{
				g_pInterfaceMgr->SetLoadLevelPhoto("interface\\photo\\missions\\default.pcx");
			}
			g_pLTClient->FreeString(hTxt);

		}

		pMissionData->SetLevelNum(m_nCurrentLevel);
	}
	else
	{
        m_bIsCustomLevel = LTTRUE;

		// No mission data in custom levels...

		pMissionData->Clear();
		char *pWorld = strrchr(pWorldFile,'\\');
        if(!pWorld)
        {
            pWorld = strrchr(pWorldFile, '/');
        }
        if(pWorld)
        {
            pWorld++;
            HSTRING hWorld = g_pLTClient->CreateString(pWorld);
            g_pInterfaceMgr->SetLoadLevelString(hWorld);
            g_pLTClient->FreeString(hWorld);
        }

		char szPhoto[512];
		SAFE_STRCPY(szPhoto,pWorldFile);
		strtok(szPhoto,".");
		strcat(szPhoto,".pcx");
		g_pInterfaceMgr->SetLoadLevelPhoto(szPhoto);

	}

	// Change to the loading level state...

	m_InterfaceMgr.ChangeState(GS_LOADINGLEVEL);

	// Check for special case of not being connected to a server or going to
	// single player mode from multiplayer...

	int nGameMode = 0;
    g_pLTClient->GetGameMode(&nGameMode);
    if (pRecordFile || pPlaydemoFile || !g_pLTClient->IsConnected() ||
		(nGameMode != STARTGAME_NORMAL && nGameMode != GAMEMODE_NONE))
	{
		StartGameRequest request;
		memset(&request, 0, sizeof(StartGameRequest));

		// Start with clean slate
		NetGame sNetGame;
		NetClientData sNetClientData;
		memset(&sNetGame, 0, sizeof(NetGame));
		memset(&sNetClientData, 0, sizeof(NetClientData));
		request.m_pGameInfo = &sNetGame;
		request.m_GameInfoLen = sizeof(NetGame_t);
		request.m_pClientData   = &sNetClientData;
		request.m_ClientDataLen = sizeof(NetClientData_t);
		request.m_Type = STARTGAME_NORMAL;

		if(pRecordFile)
		{
			SAFE_STRCPY(request.m_RecordFilename, pRecordFile);
			SAFE_STRCPY(request.m_WorldName, pWorldFile);
		}

		if(pPlaydemoFile)
		{
			SAFE_STRCPY(request.m_PlaybackFilename, pPlaydemoFile);
		}

        LTRESULT dr = g_pLTClient->StartGame(&request);
		if (dr != LT_OK)
		{
            return LTFALSE;
		}

		if(pPlaydemoFile)
		{
			// If StartGameRequest::m_PlaybackFilename is filled in the engine fills in m_WorldName.
			pWorldFile = request.m_WorldName;
		}
	}



	// Send the mission data to the server so the server can update
	// the player as necessary...

	m_InterfaceMgr.SendMissionDataToServer();



	// Send a message to the server shell with the needed info...

    HSTRING hWorldFile          = g_pLTClient->CreateString(pWorldFile);
    HSTRING hCurWorldSaveFile   = g_pLTClient->CreateString(pCurWorldSaveFile ? pCurWorldSaveFile : (char *)" ");
    HSTRING hRestoreObjectsFile = g_pLTClient->CreateString(pRestoreObjectsFile ? pRestoreObjectsFile : (char *)" ");

    HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_LOAD_GAME);
    g_pLTClient->WriteToMessageByte(hMessage, nFlags);
    g_pLTClient->WriteToMessageByte(hMessage, m_eDifficulty);
    g_pLTClient->WriteToMessageByte(hMessage, m_bFadeBodies);
    g_pLTClient->WriteToMessageHString(hMessage, hWorldFile);
    g_pLTClient->WriteToMessageHString(hMessage, hCurWorldSaveFile);
    g_pLTClient->WriteToMessageHString(hMessage, hRestoreObjectsFile);

	BuildClientSaveMsg(hMessage);

    g_pLTClient->EndMessage(hMessage);

    g_pLTClient->FreeString(hWorldFile);
    g_pLTClient->FreeString(hCurWorldSaveFile);
    g_pLTClient->FreeString(hRestoreObjectsFile);


	//if we've loaded a game, we're now in a single player game!
	SetGameType(SINGLE);

    return LTTRUE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::LoadGame()
//
//	PURPOSE:	Handles loading a saved game
//
// ----------------------------------------------------------------------- //

LTBOOL CGameClientShell::LoadGame(char* pWorld, char* pObjectsFile)
{
    if (!pWorld || !pObjectsFile) return LTFALSE;

	TurnOffAlternativeCamera(CT_FULLSCREEN);

	// if we're playing multiplayer, I know I'm about to disconnect so don't warn me
	if (IsInWorld() && GetGameType() != SINGLE)
		g_pInterfaceMgr->StartingNewGame();

    return DoLoadWorld(pWorld, LTNULL, pObjectsFile, LOAD_RESTORE_GAME);
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::SaveGame()
//
//	PURPOSE:	Handles saving a game...
//
// ----------------------------------------------------------------------- //

LTBOOL CGameClientShell::SaveGame(char* pObjectsFile)
{
    if (!pObjectsFile) return LTFALSE;

    uint8 nFlags = 0;

	// Save the level objects...

    HSTRING hSaveObjectsName = g_pLTClient->CreateString(pObjectsFile);

    HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_SAVE_GAME);
    g_pLTClient->WriteToMessageByte(hMessage, nFlags);
    g_pLTClient->WriteToMessageHString(hMessage, hSaveObjectsName);

	BuildClientSaveMsg(hMessage);

    g_pLTClient->EndMessage(hMessage);

    g_pLTClient->FreeString(hSaveObjectsName);

	char strSaveGame[256];
	sprintf (strSaveGame, "%s|%s",m_strCurrentWorldName, pObjectsFile);
	CWinUtil::WinWritePrivateProfileString (GAME_NAME, "Continue", strSaveGame, SAVEGAMEINI_FILENAME);

    return LTTRUE;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::IsJoystickEnabled()
//
//	PURPOSE:	Determines whether or not there is a joystick device
//				enabled
//
// ----------------------------------------------------------------------- //

LTBOOL CGameClientShell::IsJoystickEnabled()
{
	// first attempt to find a joystick device

	char strJoystick[128];
	memset (strJoystick, 0, 128);
    LTRESULT result = g_pLTClient->GetDeviceName (DEVICETYPE_JOYSTICK, strJoystick, 127);
    if (result != LT_OK) return LTFALSE;

	// ok - we found the device and have a name...see if it's enabled

    LTBOOL bEnabled = LTFALSE;
    g_pLTClient->IsDeviceEnabled (strJoystick, &bEnabled);

	return bEnabled;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::EnableJoystick()
//
//	PURPOSE:	Attempts to find and enable a joystick device
//
// ----------------------------------------------------------------------- //

LTBOOL CGameClientShell::EnableJoystick()
{
	// first attempt to find a joystick device

	char strJoystick[128];
	memset(strJoystick, 0, 128);
    LTRESULT result = g_pLTClient->GetDeviceName(DEVICETYPE_JOYSTICK, strJoystick, 127);
    if (result != LT_OK) return LTFALSE;

	// ok, now try to enable the device

	char strConsole[256];
	sprintf(strConsole, "EnableDevice \"%s\"", strJoystick);
    g_pLTClient->RunConsoleString(strConsole);

    LTBOOL bEnabled = LTFALSE;
    g_pLTClient->IsDeviceEnabled(strJoystick, &bEnabled);

	return bEnabled;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::VRFlashTuneUpdate()
//
//	PURPOSE:	Numpad tuning for the muzzle flash, the way the Prey port did it
//
// ----------------------------------------------------------------------- //
//
// Asked for on 19 September: put the ak47's muzzle flash on screen with numpad
// tuning just like the Prey port, and fine tune that one effect to see
// whether the difference can be seen.
//
// That is the way to do it. Every attempt to reason
// the flash into place has cost a headset test and been wrong, most
// recently when the thing at the wall was taken for the impact - the runs
// showed the flash lying on the carpet by the fountain
// in several copies, which is exactly what the screenshots show.
//
// One effect, one gun, live numbers, judged by eye. Nothing here changes the flash's
// behaviour when VRFlashTune is 0.
//
//   NUMPAD 4 / 6      left / right        (across the barrel)
//   NUMPAD 8 / 2      up / down
//   NUMPAD 7 / 9      back / forward      (along the barrel)
//   NUMPAD 5          print the numbers to the log
//   NUMPAD 0          reset all three to zero
//   NUMPAD . (DEL)    toggle VRFlashHold - the continuous flash
//   hold SHIFT        ten times the step
//
// Edge-detected: a held key moves it once, not once per frame. GetAsyncKeyState
// rather than the engine's binding table on purpose - the tuner has to work
// while the game has its own ideas about the keyboard, and this is a debug
// path that should not need a binding to exist.
// EVERY VALUE THE TUNER HAS SET, AND A FILE IT OWNS.
//
// RunConsoleString sets a variable in memory and nothing writes it out. The
// engine's WriteConfigFile does not save these at all - checked: after a
// session of tuning, autoexec.cfg carried not one VRFlashOff. So every muzzle value
// the tester has ever tuned died on quitting, which is most of why this has taken
// days instead of minutes.
//
// So the tuner keeps its own table and writes its own file. vrtune.cfg is a
// plain list of console commands, read at startup right after vrweapons.cfg,
// and safe to delete to reset everything. Nothing in the engine has to
// cooperate.
namespace VRTuneSave
{
	// A TABLE THAT DROPS SILENTLY LOSES THE PLAYER'S WORK. This held 128 entries and
	// Remember() returned without a word when it was full. The file is merged
	// in AFTER the session's own entries, so once the total passed 128 it was
	// the FILE's older values that fell off - and the file was then rewritten
	// without them. 21 September: two weapon sweeps captured an alignment for
	// every gun (34 new keys) and 47 tuned values vanished from vrtune.cfg -
	// the P38 aim, the Luger and Walther alignments, a dozen grips. Restored
	// from the tracked copy. Now 1024, and a full table is logged, loudly.
	struct Entry { char szName[64]; float f; };
	static Entry s_e[1024];
	static int   s_n = 0;

	static void Remember(const char* pszName, float f)
	{
		for (int i = 0; i < s_n; ++i)
			if (stricmp(s_e[i].szName, pszName) == 0) { s_e[i].f = f; return; }
		if (s_n >= (int)(sizeof(s_e) / sizeof(s_e[0])))
		{
			static bool s_bSaidFull = false;
			if (!s_bSaidFull) { s_bSaidFull = true; VRLog::Msg("VRTuneSave: TABLE FULL at %d entries - '%s' NOT kept; raise the table", s_n, pszName); }
			return;
		}
		strncpy(s_e[s_n].szName, pszName, sizeof(s_e[0].szName) - 1);
		s_e[s_n].szName[sizeof(s_e[0].szName) - 1] = '\0';
		s_e[s_n].f = f;
		++s_n;
	}

	// EVERY WEAPON ALREADY ON DISK, NOT JUST THIS SESSION'S.
	//
	// The table lives in memory and starts empty each launch. The tester tuned the
	// ak47, quit, relaunched and tuned the Sterling - and the write replaced the
	// file with the Sterling alone. The ak47's three values were LOADED that
	// session (vrtune.cfg is read at startup) but the writer had never heard of
	// them, so it wrote a complete file from an incomplete table.
	//
	// Merging from disk before every write fixes it for good, and it also
	// survives the file being edited by hand between sessions. Entries already
	// in memory win, because those are the ones just tuned.
	static void MergeFromFile()
	{
		FILE* fp = fopen("vrtune.cfg", "r");
		if (!fp) return;
		char szLine[256];
		while (fgets(szLine, sizeof(szLine), fp))
		{
			if (szLine[0] == '#' || szLine[0] == '\n' || szLine[0] == '\r') continue;
			char  szName[64] = "";
			float f = 0.0f;
			if (sscanf(szLine, "%63s %f", szName, &f) != 2) continue;
			bool bHave = false;
			for (int i = 0; i < s_n; ++i)
				if (stricmp(s_e[i].szName, szName) == 0) { bHave = true; break; }
			if (!bHave) Remember(szName, f);
		}
		fclose(fp);
	}

	static bool Write()
	{
		MergeFromFile();
		if (s_n <= 0) return false;
		FILE* fp = fopen("vrtune.cfg", "w");
		if (!fp) return false;
		fprintf(fp, "# Written by the in-game muzzle tuner. Read at startup.\n");
		fprintf(fp, "# One console command per line. Delete this file to reset.\n");
		for (int i = 0; i < s_n; ++i)
			fprintf(fp, "%s %.4f\n", s_e[i].szName, s_e[i].f);
		fclose(fp);
		return true;
	}
}

void CGameClientShell::VRFlashTuneUpdate()
{
	if (g_vtVRFlashTune.GetFloat() <= 0.0f) return;

	// F-KEYS, BECAUSE NOTHING IN THE GAME IS BOUND TO THEM.
	//
	// In headset testing the numpad did not work because it changes weapons, and
	// tuning time should not go on getting the rig to work rather
	// than fine tuning. So this uses keys that cannot collide.
	//
	// Why the numpad collided, from game/autoexec.cfg:
	//
	//     rangebind "##keyboard" "1" 0.000000 0.000000 "Weapon_1"
	//
	// The weapon slots are bound by CHARACTER, not by scancode. Any key that
	// produces a "1" selects weapon one, and with NumLock on the numpad
	// produces exactly those characters. The numpad's own scancodes (71-83) are
	// bound to nothing at all, which is why this was not obvious.
	//
	// AND THE NUMLOCK-OFF FALLBACK ADDED AN HOUR AGO WAS A MISTAKE, removed
	// here. With NumLock off the numpad sends the navigation codes, and those
	// ARE bound: 201 LookUp, 209 LookDown, 203 Left, 205 Right. Reading them
	// would have retuned the flash every time the head turned.
	//
	// The F-keys are clean. autoexec.cfg binds only 64 and 67 - F6 and F9 - and
	// F1 is spoken for elsewhere in this port, so those three are left alone.
	// The numpad stays live too, with the weapon change suppressed while
	// tuning, so whichever the player reaches for works.
	//
	//   F2 / F3    left / right across the barrel
	//   F4 / F5    up / down
	//   F7 / F8    back / forward along the barrel
	//   F10        print the numbers to the log
	//   F11        reset
	//   F12        toggle the continuous flash
	//   SHIFT      ten times the step
	static bool s_bDown[10] = { false, false, false, false, false,
								false, false, false, false, false };
	// NUMPAD 1 IS THE NUMPAD'S END, and the effect cycle. The tester tuned the AK47's
	// "tracer" for a whole session on 20 September and the log shows Down and
	// Up seen, applied to the FLASH, and no End press ever seen - the readout
	// still said FLASH and the thing that moved was the muzzle flash. The player is in
	// a headset, finding keys blind on the numpad; the main-cluster End was the
	// only key in this table with no numpad twin.
	const int nKeys[10] = { VK_NUMPAD4, VK_NUMPAD6, VK_NUMPAD8, VK_NUMPAD2,
							VK_NUMPAD7, VK_NUMPAD9, VK_NUMPAD5, VK_NUMPAD0,
							VK_DECIMAL, VK_NUMPAD1 };
	// THE ARROW CLUSTER, AND NOT ONE F-KEY. This is the fix for the worst bug
	// of the day.
	//
	// In the headset the view was locked to the head, which was disorienting.
	// VK_F4 is the tuner's UP - and F4 also TOGGLES VRHeadTracking. Every time
	// the flash was nudged up, head tracking switched off, and the
	// symptom was hunted in the compositor, the scales and the frame
	// pacing, all of which measured healthy because they WERE healthy.
	//
	// It was not one unlucky key. Every F-key this tuner used is wired to
	// something that wrecks the view, because the debug toggles and the tuner
	// were assigned the same block by two different people on two different
	// days:
	//
	//     F2  toggle edit mode          F5   cycle VRQuatHead
	//     F3  toggle screenshot mode    F11  cycle VRStereo
	//     F4  HEAD TRACKING ON/OFF      F12  cycle VRAsymFrustum
	//                                   F9   quickload, one key away
	//
	// The tuner reads keys with GetAsyncKeyState and deliberately does not
	// consume them, so every press did both things. So: no F-keys at all.
	//
	// The arrow cluster instead, which the tester asked for and can find by
	// touch with the headset on. Up/Down, Home, Insert and Delete are UNBOUND
	// in autoexec.cfg - checked, not assumed - so only Left/Right (turn) and
	// PageUp/PageDown (look) needed suppressing, and only while tuning.
	//
	//   Up / Down      forward / back along the barrel   (the axis that matters)
	//   Left / Right   left / right across it
	//   PgUp / PgDn    up / down
	//   Home  step     Insert  reset      Delete  hold
	// SLOT 9 IS THE EFFECT CYCLE - End, next to Home in the same block.
	const int nFKey[10] = { VK_LEFT,    VK_RIGHT,   VK_PRIOR,   VK_NEXT,
							VK_DOWN,    VK_UP,      VK_HOME,    VK_INSERT,
							VK_DELETE,  VK_END };
	bool bHit[10];
	for (int k = 0; k < 10; ++k)
	{
		bool bNow = (nKeys[k] != 0) && ((GetAsyncKeyState(nKeys[k]) & 0x8000) != 0);
		if (!bNow && nFKey[k])
			bNow = (GetAsyncKeyState(nFKey[k]) & 0x8000) != 0;
		bHit[k] = bNow && !s_bDown[k];
		s_bDown[k] = bNow;

		// SAY THAT A KEY WAS SEEN AT ALL. A key with no effect has two very
		// different causes - the tuner never saw the key, or it saw it and the
		// game acted on it underneath - and they want different fixes.
		if (bHit[k])
		{
			static int s_nSaidKey = 0;
			if (s_nSaidKey < 12)
			{
				++s_nSaidKey;
				VRLog::Msg("VRFlashTune: key %d seen", k);
			}
		}
	}

	// AND THE SAME CONTROLS ON THE LEFT CONTROLLER, because the player is wearing a
	// headset while the player uses this.
	//
	// the keys
	// were very hard to work out because the headset had to come off to
	// keep finding the right F keys, and sometimes quick save, quick load
	// or something else got hit instead. A tuning tool that needs the headset
	// OFF cannot tune the thing the headset is for - the flash was judged
	// blind and keys were found by touch.
	//
	// HOLD THE LEFT GRIP to tune. Let go and it is ordinary play again, so
	// walking and turning are untouched when the player is not actually adjusting. The
	// right hand keeps holding the gun, which means the player watches the flash with
	// the hand that moves it and adjusts with the other.
	//
	//   left stick left / right    back / forward along the barrel
	//   left stick down / up       down / up
	//   left X                     cycle the step
	//   left Y                     reset this weapon to 0
	//
	// A stick is analogue and would otherwise run away: past half deflection it
	// REPEATS one step every 140 ms, so a tap is one step and a hold is about
	// seven a second. Countable either way, which is the property the F-keys
	// had and a raw axis does not.
	if (VRShared::IsLive())
	{
		const VRHandState& L = VRShared::State().Hands[0];
		static uint32 s_nPrevLBtn   = 0;
		static double s_fNextRepeat = 0.0;
		const uint32  nWentL = L.nButtons & ~s_nPrevLBtn;
		s_nPrevLBtn = L.nButtons;

		if (L.nActive && (L.nButtons & VRBTN_GRIP))
		{
			const double fNow = g_pLTClient->GetTime();
			if (fNow >= s_fNextRepeat)
			{
				bool bAny = false;
				if      (L.fStickX >  0.5f) { bHit[5] = true; bAny = true; }
				else if (L.fStickX < -0.5f) { bHit[4] = true; bAny = true; }
				if      (L.fStickY >  0.5f) { bHit[2] = true; bAny = true; }
				else if (L.fStickY < -0.5f) { bHit[3] = true; bAny = true; }
				if (bAny) s_fNextRepeat = fNow + 0.14;
			}
			if (nWentL & VRBTN_PRIMARY)   bHit[6] = true;	// X - step
			if (nWentL & VRBTN_SECONDARY) bHit[7] = true;	// Y - reset
			// LEFT STICK CLICK - cycle FLASH / TRACER / CASING, so the mode
			// can be changed without leaving the headset. See the numpad note.
			if (nWentL & VRBTN_THUMBCLICK) bHit[9] = true;
		}
	}

	// THE STEP WAS FAR TOO SMALL TO SEE, and that is why the tester reported the flash
	// as never moving whichever way it was pushed.
	//
	// It was 0.5 units a press. A unit is 16.92 mm here, so that is EIGHT
	// MILLIMETRES per press on an object several metres away - invisible.
	// Measured at the desk: 60 units of offset moves the flash's centroid about
	// a hundred pixels, which is the smallest move worth calling a move. At the
	// old step that is a hundred and twenty presses.
	//
	// The tuner was working the whole time; nothing about it could be seen.
	// So: 2 units a press, 20 with SHIFT, and 0.5 with CTRL once it is close
	// and the fine work starts.
	bool bChangedStep = false;

	// NO MODIFIER KEYS AT ALL. Shift cannot be used because it is the
	// change-weapons button - autoexec.cfg line 326, scancode 42 is left shift
	// bound to NextWeapon. Ctrl (29) and Alt (56) are bound as well, so there
	// is no free modifier on this keyboard layout and there is no point hunting
	// for one.
	//
	// F10 cycles the step instead: 0.5 for fine work, 2 for normal, 10 and 50
	// to cross a room. The current value is on screen, so there is never a
	// question of how far a press moves it.
	// AND IT CAN LAND ON AN ODD NUMBER NOW.
	//
	// the ak47 got almost perfect, but the
	// stepping made it impossible to finish - it could be -4.0 or -6.0 and it
	// needed to be about -5. A step of 2 from zero can only ever reach even numbers, so
	// the one value the player wanted was the one value unreachable. The fine step
	// existed all along at 0.5, three presses of F10 away, on a keyboard the player
	// could not see.
	//
	// So the DEFAULT is the fine one and the ladder is denser: 0.5 lands on -5
	// in ten taps, and 1 lands on it in five.
	static float s_fStep = 0.5f;
	if (bHit[6])
	{
		s_fStep = (s_fStep < 0.3f)  ? 0.5f
				: (s_fStep < 0.6f)  ? 1.0f
				: (s_fStep < 1.1f)  ? 2.0f
				: (s_fStep < 2.1f)  ? 5.0f
				: (s_fStep < 5.1f)  ? 20.0f
									: 0.25f;
		bChangedStep = true;
	}
	const float fStep = s_fStep;
	bool bChanged = false;

	// PER WEAPON, NOT GLOBAL. Measured 20 September: with the flash in the right
	// coordinate space at last, the residual along the barrel is different for
	// every gun and follows nothing - the AK47 wants about +10 units and the
	// Sterling wants 0, while their authored MuzzlePos forward figures are 16.54
	// and 19.30. Tuning a global would fix whichever gun is in hand and move all
	// the others off, which is the shape of half of tonight's regressions.
	//
	// So the tuner writes VRFlashOffF@<weapon>, the same "@slug" convention the
	// per-weapon angle trims already use, which the engine persists into
	// autoexec.cfg. Tune a gun once and it stays tuned, and the next gun starts
	// from its own number.
	// WHICH EFFECT THE ARROWS ARE MOVING. End cycles it.
	//
	// The three start from one muzzle - e71f170, and that stays - but a tracer
	// leaving the barrel and a casing leaving the breach are not the same point,
	// so each gets its own nudge on top. Tuning the FLASH still moves all three
	// together, which is the behaviour that stops one drifting away from the
	// others while you work.
	static int s_nTuneMode = 0;			// 0 flash, 1 tracer, 2 casing
	// START IN A MODE FROM THE COMMAND LINE: +VRFlashTuneMode 2 opens in
	// CASING with no presses at all. Headset testing, 20 September, after the stick
	// click took the player to CASING and the session ended a moment later: the request
	// was to start in the casing mode with no button presses at all.
	// Read once, when the tuner first runs; the cycle keys still work after.
	{
		static VarTrack s_vtMode;
		static bool s_bModeInit = false;
		if (!s_vtMode.IsInitted()) s_vtMode.Init(g_pLTClient, "VRFlashTuneMode", LTNULL, 0.0f);
		if (!s_bModeInit)
		{
			s_bModeInit = true;
			const int nWant = (int)s_vtMode.GetFloat();
			if (nWant >= 0 && nWant <= 8) { s_nTuneMode = nWant; bChangedStep = true; }
		}
	}
	if (bHit[9])
	{
		s_nTuneMode = (s_nTuneMode + 1) % 9;
		bChangedStep = true;			// reuse the "say it on screen" path
	}
	// ANGLE: the gun's own orientation in the hand, degrees. Left/Right turn it
	// (yaw), Up/Down tilt the barrel (pitch), PgUp/PgDn roll it. The columns are
	// the key groups, so the middle one is roll here, not "up".
	// SCALE: the weapon's drawn size, one number, Up/Down only. It is not
	// additive and has no zero: a gun with no override draws at the global
	// VRWeaponScale, so the first nudge seeds the per-weapon value from the
	// size actually in use rather than from 0. Steps are the step size x 0.05,
	// so the default 0.5 moves the scale by 0.025.
	// AIM: the shot line relative to the drawn gun, degrees - reticle, flash
	// orientation and burst move; the gun does not. Left/Right yaw, Up/Down pitch.
	// LENS: the eyepiece of a scope that is part of the gun mesh (Contender,
	// Dragunov), moved in the gun's frame like GRIP; and the attached scope's
	// eyepiece and pass on any gun that carries one.
	// LENS SIZE: the lens disc's radius, world units, Up/Down only; absolute,
	// seeded from the mesh's radius on the first nudge; Insert puts the mesh
	// back. LENS moves in fifths of the step and LENS SIZE in tenths - a
	// half-unit step could not land the disc on a 2 cm eyepiece.
	static const char* kModeName[9] = { "FLASH", "TRACER", "CASING", "ANGLE", "SCALE", "GRIP", "AIM", "LENS", "LENS SIZE" };
	static const char* kModeBase[9][3] = {
		{ "VRFlashOffR",  "VRFlashOffU",  "VRFlashOffF"  },
		{ "VRTracerOffR", "VRTracerOffU", "VRTracerOffF" },
		{ "VRShellOffR",  "VRShellOffU",  "VRShellOffF"  },
		{ "VRAngleYaw",   "VRAngleRoll",  "VRAnglePitch" },
		{ "",             "",             "VRWeaponScale" },
		{ "VRGripOffR",   "VRGripOffU",   "VRGripOffF"   },
		{ "VRAimYaw",     "",             "VRAimPitch"   },
		{ "VRLensOffR",   "VRLensOffU",   "VRLensOffF"   },
		{ "",             "",             "VRLensRad"    },
	};
	static const char* kModeLabel[9][3] = {
		{ "R", "U", "F" }, { "R", "U", "F" }, { "R", "U", "F" },
		{ "yaw", "roll", "pitch" },
		{ "-", "-", "scale" },
		{ "R", "U", "F" },
		{ "yaw", "-", "pitch" },
		{ "R", "U", "F" },
		{ "-", "-", "radius" },
	};
	const bool bScaleMode = (s_nTuneMode == 4);
	const bool bLensMode  = (s_nTuneMode == 7);
	const bool bLensSize  = (s_nTuneMode == 8);
	const char* pszR = kModeBase[s_nTuneMode][0];
	const char* pszU = kModeBase[s_nTuneMode][1];
	const char* pszF = kModeBase[s_nTuneMode][2];

	char szSlug[64] = "";
	VRWeaponSlugForId((int)m_weaponModel.GetWeaponId(), szSlug, sizeof(szSlug));

	struct Loc
	{
		static float Get(const char* pszBase, const char* pszSlug)
		{
			if (!pszBase[0]) return 0.0f;
			char szN[128];
			if (pszSlug[0]) sprintf(szN, "%s@%s", pszBase, pszSlug);
			else            sprintf(szN, "%s", pszBase);
			HCONSOLEVAR h = g_pLTClient->GetConsoleVar(szN);
			if (h && !(strcmp(pszBase, "VRLensRad") == 0 && g_pLTClient->GetVarValueFloat(h) <= 0.0f))
				return g_pLTClient->GetVarValueFloat(h);
			// No per-weapon value yet: the size in use is the global's.
			if (strcmp(pszBase, "VRWeaponScale") == 0)
			{
				HCONSOLEVAR hg = g_pLTClient->GetConsoleVar("VRWeaponScale");
				return hg ? g_pLTClient->GetVarValueFloat(hg) : 0.4f;
			}
			// No per-weapon radius yet, or a zero one: the mesh's is in use.
			if (strcmp(pszBase, "VRLensRad") == 0) return s_fVRLensMeshRadius;
			return 0.0f;
		}
		static void Set(const char* pszBase, const char* pszSlug, float f)
		{
			if (!pszBase[0]) return;
			char szCmd[160];
			if (pszSlug[0]) sprintf(szCmd, "%s@%s %f", pszBase, pszSlug, f);
			else            sprintf(szCmd, "%s %f", pszBase, f);
			g_pLTClient->RunConsoleString(szCmd);
			// AND INTO THE TABLE THAT GETS WRITTEN TO DISK.
			char szN2[128];
			if (pszSlug[0]) sprintf(szN2, "%s@%s", pszBase, pszSlug);
			else            sprintf(szN2, "%s", pszBase);
			VRTuneSave::Remember(szN2, f);
			// The reader caches which variable answered, and this may have just
			// brought a new one into existence. Without this the number moves
			// and the flash does not.
			VRFlashOffsetsInvalidate();
		}
	};

	const float fNudge = bScaleMode ? fStep * 0.05f : bLensSize ? fStep * 0.1f : bLensMode ? fStep * 0.2f : fStep;
	if (bHit[0]) { Loc::Set(pszR, szSlug, Loc::Get(pszR, szSlug) - fNudge); bChanged = true; }
	if (bHit[1]) { Loc::Set(pszR, szSlug, Loc::Get(pszR, szSlug) + fNudge); bChanged = true; }
	if (bHit[2]) { Loc::Set(pszU, szSlug, Loc::Get(pszU, szSlug) + fNudge); bChanged = true; }
	if (bHit[3]) { Loc::Set(pszU, szSlug, Loc::Get(pszU, szSlug) - fNudge); bChanged = true; }
	if (bHit[4]) { Loc::Set(pszF, szSlug, Loc::Get(pszF, szSlug) - fNudge); bChanged = true; }
	if (bHit[5]) { Loc::Set(pszF, szSlug, Loc::Get(pszF, szSlug) + fNudge); bChanged = true; }
	if (bHit[7])
	{
		// Only the effect being tuned, so a stray Insert cannot wipe all three.
		if (bScaleMode)
		{
			// Reset = the global size: set the per-weapon value to it.
			HCONSOLEVAR hg = g_pLTClient->GetConsoleVar("VRWeaponScale");
			Loc::Set(pszF, szSlug, hg ? g_pLTClient->GetVarValueFloat(hg) : 0.4f);
		}
		else
		{
			Loc::Set(pszR, szSlug, 0.0f);
			Loc::Set(pszU, szSlug, 0.0f);
			Loc::Set(pszF, szSlug, 0.0f);
		}
		bChanged = true;
	}
	if (bHit[8])
	{
		g_vtVRFlashHold.SetFloat(g_vtVRFlashHold.GetFloat() > 0.0f ? 0.0f : 1.0f);
		bChanged = true;
	}

	// THE NUMBERS GO IN THE LOG AS WELL AS ON SCREEN, so that when the player finds the
	// value that looks right the player can read it back off the log afterwards rather
	// than having to memorise it inside a headset.
	if (bChanged || bChangedStep)
	{
		// The console lines to keep, printed ready to paste.
		// THE MODE'S OWN NAMES AND VALUES. This line printed the FLASH values
		// whatever mode was on, so a session spent in "TRACER" mode that was
		// really in FLASH mode read identically in the log.
		VRLog::Msg("VRFlashTune: %s %s  R %+.1f  U %+.1f  F %+.1f  (step %.1f, hold %s)"
				   "   ->  %s@%s %.1f / %s@%s %.1f / %s@%s %.1f",
				   szSlug[0] ? szSlug : "(no weapon)", kModeName[s_nTuneMode],
				   Loc::Get(pszR, szSlug), Loc::Get(pszU, szSlug),
				   Loc::Get(pszF, szSlug), fStep,
				   g_vtVRFlashHold.GetFloat() > 0.0f ? "on" : "off",
				   pszR, szSlug, Loc::Get(pszR, szSlug),
				   pszU, szSlug, Loc::Get(pszU, szSlug),
				   pszF, szSlug, Loc::Get(pszF, szSlug));
	}

	// ON SCREEN, EVERY FRAME. The player is in a headset; the log is not readable from
	// there and the whole point is to see the number move with the effect.
	// AND WRITE IT TO DISK, OR THE PLAYER TUNES IT TWICE.
	//
	// RunConsoleString sets the variable in memory and nothing more. The ONLY
	// calls to WriteConfigFile in this client are in the options-menu folders
	// and a one-time patch upgrade - there is none on exit. So a perfectly
	// tuned weapon was lost the moment the player quit, which is why autoexec.cfg held
	// 63 per-weapon ANGLE trims and not one VRFlashOffF: the angles got there
	// through a menu save, and the player's muzzle tuning never survived a session.
	//
	// The right question was asked before testing - whether a button had to
	// be pressed to lock in a perfect spot once found - and the honest answer
	// was going to be no, and that the value would already have been lost.
	//
	// Debounced rather than per-press: holding an arrow fires seven changes a
	// second and each write is a whole config file. One write 1.5 s after the
	// last change costs nothing and cannot be forgotten. The readout says SAVED
	// so there is never a question of whether it took.
	{
		static bool   s_bDirty  = false;
		static double s_fSaveAt = 0.0;
		const double  fNowSave  = g_pLTClient->GetTime();
		if (bChanged)
		{
			s_bDirty  = true;
			s_fSaveAt = fNowSave + 1.5;
		}
		else if (s_bDirty && fNowSave >= s_fSaveAt)
		{
			s_bDirty = false;
			if (VRTuneSave::Write())
			{
				s_fVRTuneSavedAt = fNowSave;
				VRLog::Msg("VRFlashTune: saved to game/vrtune.cfg - survives a quit");
			}
			else
			{
				VRLog::Msg("VRFlashTune: COULD NOT WRITE game/vrtune.cfg");
			}
		}
	}
	const bool bJustSaved = (g_pLTClient->GetTime() - s_fVRTuneSavedAt) < 2.0;

	char szT[160];
	sprintf(szT, (bScaleMode || bLensSize) ? "%s %s  %s%+.1f %s%+.1f %s%.3f  step %.3f %s"
	                        : bLensMode ? "%s %s  %s%+.2f %s%+.2f %s%+.2f  step %.2f %s"
	                        : "%s %s  %s%+.1f %s%+.1f %s%+.1f  step %.1f %s",
			szSlug[0] ? szSlug : "-",
			kModeName[s_nTuneMode],
			kModeLabel[s_nTuneMode][0], Loc::Get(pszR, szSlug),
			kModeLabel[s_nTuneMode][1], Loc::Get(pszU, szSlug),
			kModeLabel[s_nTuneMode][2], Loc::Get(pszF, szSlug), fNudge,
			g_vtVRFlashHold.GetFloat() > 0.0f ? "HOLD" : "");
	if (bJustSaved) strncat(szT, "  SAVED", sizeof(szT) - strlen(szT) - 1);
	CreateDebugSurface(szT);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateDebugInfo()
//
//	PURPOSE:	Update debugging info.
//
// ----------------------------------------------------------------------- //

void CGameClientShell::UpdateDebugInfo()
{
	char buf[100];

	if (m_hDebugInfo)
	{
        g_pLTClient->DeleteSurface(m_hDebugInfo);
		m_hDebugInfo = NULL;
	}


	// The muzzle flash tuner owns the debug surface while it is on.
	if (g_vtVRFlashTune.GetFloat() > 0.0f)
	{
		VRFlashTuneUpdate();
		return;
	}

	// Check to see if we should show the player position...

    HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();
	if (m_bShowPlayerPos && hPlayerObj)
	{
        LTVector vPos;
        g_pLTClient->GetObjectPos(hPlayerObj, &vPos);

		sprintf(buf, "Pos(%.0f,%.0f,%.0f)", vPos.x, vPos.y, vPos.z);

		CreateDebugSurface(buf);
	}

	if (m_bShowCamPosRot)
	{
        LTVector vPos;
        g_pLTClient->GetObjectPos(m_hCamera, &vPos);

		// Convert pitch and yaw to the same units used by DEdit...

		LTFLOAT fYawDeg = RAD2DEG(m_fYaw);
		while (fYawDeg < 0.0f)
		{
			fYawDeg += 360.0f;
		}
		while (fYawDeg > 360.0f)
		{
			fYawDeg -= 360.0f;
		}

		LTFLOAT fPitchDeg = RAD2DEG(m_fPitch);
		while (fPitchDeg < 0.0f)
		{
			fPitchDeg += 360.0f;
		}
		while (fPitchDeg > 360.0f)
		{
			fPitchDeg -= 360.0f;
		}

		sprintf(buf, "CamPos(%.0f,%.0f,%.0f)|Pitch(%.0f)|Yaw(%.0f)",
			vPos.x, vPos.y, vPos.z, fPitchDeg, fYawDeg);

		CreateDebugSurface(buf);
	}


	// See if the FOV has changed...

	if (!m_bUsingExternalCamera && !m_bZooming && !m_nZoomView && !IsLiquid(m_eCurContainerCode))
	{
		LTFLOAT fovX, fovY;
		g_pLTClient->GetCameraFOV(m_hCamera, &fovX, &fovY);

		if (fovX != g_vtFOVXNormal.GetFloat() || fovY != g_vtFOVYNormal.GetFloat())
		{
			SetCameraFOV(DEG2RAD(g_vtFOVXNormal.GetFloat()), DEG2RAD(g_vtFOVYNormal.GetFloat()));
		}
	}


	// Check to see if we are in object edit mode...

	if (m_editMgr.IsEditMode() && !g_bScreenShotMode)
	{
		HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();
		if (hPlayerObj)
		{
			LTVector vPos;
			g_pLTClient->GetObjectPos(hPlayerObj, &vPos);
			sprintf(buf, "Pos(%.0f,%.0f,%.0f)", vPos.x, vPos.y, vPos.z);
			CreateDebugSurface(buf);
		}

		sprintf(buf, "EDIT MODE (Object: '%s')", m_editMgr.GetCurObjectName());
		CreateDebugSurface(buf);
	}


    HCONSOLEVAR hVar = g_pLTClient->GetConsoleVar("PlayerDims");
	if (hVar)
	{
        if (g_pLTClient->GetVarValueFloat(hVar) > 0.0f)
		{
			CreateBoundingBox();
			UpdateBoundingBox();
		}
		else if (m_hBoundingBox)
		{
            g_pLTClient->DeleteObject(m_hBoundingBox);
            m_hBoundingBox = LTNULL;
		}
	}

	if (g_vtSpecial.GetFloat())
	{
		g_vtSpecial.SetFloat(0.0f);
		g_pLTClient->CPrint("%d", g_dwSpecial);
	}
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::CreateDebugSurface
//
//	PURPOSE:	Create a surface with debug info on it.
//
// --------------------------------------------------------------------------- //

void CGameClientShell::CreateDebugSurface(char* strMessage)
{
	if (!strMessage || strMessage[0] == '\0' || m_hDebugInfo) return;

	m_hDebugInfo = g_pInterfaceResMgr->CreateSurfaceFromString(
        g_pInterfaceResMgr->GetLargeFont(), strMessage, g_hColorTransparent);
    g_pLTClient->OptimizeSurface(m_hDebugInfo, g_hColorTransparent);

    uint32 cx, cy;
    g_pLTClient->GetSurfaceDims(m_hDebugInfo, &cx, &cy);
	m_rcDebugInfo.left   = 0;
	m_rcDebugInfo.top    = 0;
	m_rcDebugInfo.right  = (int)cx;
	m_rcDebugInfo.bottom = (int)cy;
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ToggleDebugCheat
//
//	PURPOSE:	Handle debug cheat toggles
//
// --------------------------------------------------------------------------- //

void CGameClientShell::ToggleDebugCheat(CheatCode eCheat)
{
	switch (eCheat)
	{
		case CHEAT_POSWEAPON_MUZZLE :
		{
			if (!m_bSpectatorMode)
			{
				m_bTweakingWeaponMuzzle = !m_bTweakingWeaponMuzzle;

				m_MoveMgr.AllowMovement(!m_bTweakingWeaponMuzzle);

				// Save tweaks...

				if (!m_bTweakingWeaponMuzzle)
				{
                    g_pWeaponMgr->WriteFile(g_pLTClient);
				}
			}
		}
		break;

		case CHEAT_POSWEAPON :
		{
			if (!m_bSpectatorMode)
			{
				m_bTweakingWeapon = !m_bTweakingWeapon;

				m_MoveMgr.AllowMovement(!m_bTweakingWeapon);

				// Save tweaks...

				if (!m_bTweakingWeapon)
				{
                    g_pWeaponMgr->WriteFile(g_pLTClient);
				}
			}
		}
		break;

		case CHEAT_POSBREACH :
		{
			if (!m_bSpectatorMode)
			{
				m_bAdjustWeaponBreach	= !m_bAdjustWeaponBreach;

				m_MoveMgr.AllowMovement(!m_bAdjustWeaponBreach);

				// Save tweaks...

				if (!m_bAdjustWeaponBreach)
				{
                    g_pWeaponMgr->WriteFile(g_pLTClient);
				}
			}
		}
		break;

		case CHEAT_POS1STCAM :
		{
			if (!m_bSpectatorMode)
			{
				m_bAdjust1stPersonCamera = !m_bAdjust1stPersonCamera;

				m_MoveMgr.AllowMovement(!m_bAdjust1stPersonCamera);
			}
		}
		break;

		case CHEAT_LIGHTSCALE :
		{
			m_bAdjustLightScale = !m_bAdjustLightScale;

			m_MoveMgr.AllowMovement(!m_bAdjustLightScale);
		}
		break;

		case CHEAT_LIGHTADD :
		{
			m_bAdjustLightAdd = !m_bAdjustLightAdd;

			m_MoveMgr.AllowMovement(!m_bAdjustLightAdd);
		}
		break;

		case CHEAT_FOV :
		{
			m_bAdjustFOV = !m_bAdjustFOV;

			m_MoveMgr.AllowMovement(!m_bAdjustFOV);
		}
		break;

		case CHEAT_INTERFACEADJUST :
		{
//			m_InterfaceMgr.ToggleInterfaceAdjust();
		}
		break;

		case CHEAT_CHASETOGGLE :
		{
			if (m_ePlayerState == PS_ALIVE && !m_nZoomView)
			{
				SetExternalCamera(m_PlayerCamera.IsFirstPerson());
			}
		}
		break;


		default : break;
	}
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::FirstUpdate
//
//	PURPOSE:	Handle first update (each level)
//
// --------------------------------------------------------------------------- //

void CGameClientShell::FirstUpdate()
{
	if (!m_bFirstUpdate) return;

	char buf[200];
    m_bFirstUpdate = LTFALSE;


	// Force the player camera to update...

	UpdatePlayerCamera();


	// Set up the level-start screen fade...

	if (g_varStartLevelScreenFade.GetFloat())
	{
		m_InterfaceMgr.StartScreenFadeIn(g_varStartLevelScreenFadeTime.GetFloat());
	}


	// Initialize model warble sheeyot...

    g_pLTClient->RunConsoleString("+ModelWarble 0");
    g_pLTClient->RunConsoleString("+WarbleSpeed 15");
    g_pLTClient->RunConsoleString("+WarbleScale .95");


	// Set prediction if we are playing multiplayer...We turn this
	// off for single player because projectiles look MUCH better...

	if (IsMultiplayerGame())
	{
	   g_pLTClient->RunConsoleString("Prediction 1");
	}
	else
	{
	   g_pLTClient->RunConsoleString("Prediction 0");
	}


	// Set up the panning sky values

    m_bPanSky = (LTBOOL) g_pLTClient->GetServerConVarValueFloat("PanSky");
    m_fPanSkyOffsetX = g_pLTClient->GetServerConVarValueFloat("PanSkyOffsetX");
    m_fPanSkyOffsetZ = g_pLTClient->GetServerConVarValueFloat("PanSkyOffsetX");
    m_fPanSkyScaleX = g_pLTClient->GetServerConVarValueFloat("PanSkyScaleX");
    m_fPanSkyScaleZ = g_pLTClient->GetServerConVarValueFloat("PanSkyScaleZ");

	char* pTexture = LTNULL;
	if (m_bPanSky)
	{
		pTexture = g_pLTClient->GetServerConVarValueString("PanSkyTexture");
	}

    g_pLTClient->SetGlobalPanTexture(GLOBALPAN_SKYSHADOW, pTexture);


	// Set misc console vars...

	MirrorSConVar("AllSkyPortals", "AllSkyPortals");


	// Set up the environment map (chrome) texture...

    char* pEnvMap = g_pLTClient->GetServerConVarValueString("EnvironmentMap");
    const char* pVal = ((!pEnvMap || !pEnvMap[0]) ? "Tex\\Chrome.dtx" : pEnvMap);
	sprintf(buf, "EnvMap %s", pVal);
    g_pLTClient->RunConsoleString(buf);

 	m_nGlobalSoundFilterId = 0;

	char* pGlobalSoundFilter = g_pLTClient->GetServerConVarValueString("GlobalSoundFilter");
	if (pGlobalSoundFilter && pGlobalSoundFilter[0])
	{
		SOUNDFILTER* pFilter = g_pSoundFilterMgr->GetFilter(pGlobalSoundFilter);
		if (pFilter)
		{
			m_nGlobalSoundFilterId = pFilter->nId;
		}
	}

	// Set up the global (per level) wind values...

    g_vWorldWindVel.x = g_pLTClient->GetServerConVarValueFloat("WindX");
    g_vWorldWindVel.y = g_pLTClient->GetServerConVarValueFloat("WindY");
    g_vWorldWindVel.z = g_pLTClient->GetServerConVarValueFloat("WindZ");


	// Set up the global (per level) fog values...

	ResetGlobalFog();


	// Initialize the music playlists...

	if (m_Music.IsInitialized() && (SINGLE == m_eGameType) )
	{
		// Initialize music for the current level...

		char* pMusicDirectory = g_pLTClient->GetServerConVarValueString("MusicDirectory");
		char* pMusicControlFile = g_pLTClient->GetServerConVarValueString("MusicControlFile");

		if (pMusicDirectory && pMusicControlFile)
		{
			CMusicState MusicState;
			strcpy(MusicState.szDirectory, pMusicDirectory);
			strcpy(MusicState.szControlFile, pMusicControlFile);

			m_Music.RestoreMusicState(MusicState);
		}

		//m_InterfaceMgr.RestoreGameMusic();
		//m_Music.Play();
	}


	// Force us to re-evaluate what container we're in.  We call
	// UpdateContainerFX() first to make sure any container changes
	// have been accounted for, then we clear the container code
	// and force an update (this is done for underwater situations like
	// dying underwater and respawning, and also for picking up intelligence
	// items underwater)...
	UpdateContainerFX();
	ClearCurContainerCode();
	UpdateContainerFX();

	// Ship up BigHeadMode to the server if we're hosting or singleplayer!
	// This mimics the Consoles custom message. It's not great, but it works.
	if (IsHosting() || !IsMultiplayerGame())
	{
		char szBuffer[512];

		sprintf(szBuffer, "%s %f", "BigHeadMode", g_vtBigHeadMode.GetFloat());

		HSTRING hstrCmd = g_pLTClient->CreateString(szBuffer);

		HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_CONSOLE_COMMAND_CLIENT);
		g_pLTClient->WriteToMessageHString(hMessage, hstrCmd);
		g_pLTClient->EndMessage(hMessage);

		g_pLTClient->FreeString(hstrCmd);
	}

	// For some reason shadows can magically turn off on loading a save game,
	// So let's "fix" that.
	int nDrawShadows = GetConsoleInt("DrawShadows", 0);
	WriteConsoleInt("MaxModelShadows", nDrawShadows);
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::MirrorSConVar
//
//	PURPOSE:	Takes the value of the server-side variable specified by
//				pSVarName and sets its value into the client-sdie variable
///				specified by pCVarName.
//
// --------------------------------------------------------------------------- //
void CGameClientShell::MirrorSConVar(char *pSVarName, char *pCVarName)
{
	char buf[512];
	float fVal;

    fVal = g_pLTClient->GetServerConVarValueFloat(pSVarName);

	// Special case, make all farz calls go through this function...
	if (stricmp(pCVarName, "FarZ") == 0)
	{
		SetFarZ((int)fVal);
	}
	else
	{
		sprintf(buf, "%s %f", pCVarName, fVal);
		g_pLTClient->RunConsoleString(buf);
	}
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ResetGlobalFog
//
//	PURPOSE:	Reset the global fog values based on the saved values...
//
// --------------------------------------------------------------------------- //

void CGameClientShell::ResetGlobalFog()
{
	// Set the FarZ for the level...

	MirrorSConVar("FarZ", "FarZ");


	// See if fog should be disabled

    uint32 dwAdvancedOptions = m_InterfaceMgr.GetAdvancedOptions();

	if (!(dwAdvancedOptions & AO_FOG))
	{
        g_pLTClient->RunConsoleString("FogEnable 0");
		return;
	}

	MirrorSConVar("FogEnable", "FogEnable");
	MirrorSConVar("FogNearZ", "FogNearZ");
	MirrorSConVar("FogFarZ", "FogFarZ");
	MirrorSConVar("LMAnimStatic", "LMAnimStatic");

    LTVector todScale = m_LightScaleMgr.GetTimeOfDayScale();

	char buf[255];
    LTFLOAT fVal = g_pLTClient->GetServerConVarValueFloat("FogR") * todScale.x;
	sprintf(buf, "FogR %d", (int)fVal);
    g_pLTClient->RunConsoleString(buf);

    fVal = g_pLTClient->GetServerConVarValueFloat("FogG") * todScale.y;
	sprintf(buf, "FogG %d", (int)fVal);
    g_pLTClient->RunConsoleString(buf);

    fVal = g_pLTClient->GetServerConVarValueFloat("FogB") * todScale.z;
	sprintf(buf, "FogB %d", (int)fVal);
    g_pLTClient->RunConsoleString(buf);

	MirrorSConVar("SkyFogEnable", "SkyFogEnable");
	MirrorSConVar("SkyFogNearZ", "SkyFogNearZ");
	MirrorSConVar("SkyFogFarZ", "SkyFogFarZ");

	// VFog....

	MirrorSConVar("SC_VFog", "VFog");
	MirrorSConVar("SC_VFogMinY", "VFogMinY");
	MirrorSConVar("SC_VFogMaxY", "VFogMaxY");
	MirrorSConVar("SC_VFogDensity", "VFogDensity");
	MirrorSConVar("SC_VFogMax", "VFogMax");
	MirrorSConVar("SC_VFogMaxYVal", "VFogMaxYVal");
	MirrorSConVar("SC_VFogMinYVal", "VFogMinYVal");
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ShowPlayer()
//
//	PURPOSE:	Show/Hide the player object
//
// --------------------------------------------------------------------------- //

void CGameClientShell::ShowPlayer(LTBOOL bShow)
{
    HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();
	if (!hPlayerObj) return;

	// Comment this out to show player model in 1st person view...
#define DO_NORMAL_PLAYER_HIDE_SHOW

    uint32 dwFlags = g_pLTClient->GetObjectFlags(hPlayerObj);
	if (bShow)
	{
		dwFlags |= FLAG_VISIBLE;
        g_pLTClient->SetObjectFlags(hPlayerObj, dwFlags);

#ifndef DO_NORMAL_PLAYER_HIDE_SHOW
		HMODELPIECE hPiece;
        g_pLTClient->GetModelLT()->GetPiece(hPlayerObj, "Torso", hPiece);
        g_pLTClient->GetModelLT()->SetPieceHideStatus(hPlayerObj, hPiece, LTFALSE);
        g_pLTClient->GetModelLT()->GetPiece(hPlayerObj, "Head_zTex1", hPiece);
        g_pLTClient->GetModelLT()->SetPieceHideStatus(hPlayerObj, hPiece, LTFALSE);
#endif

	}
	else if (!bShow)
	{

#ifdef DO_NORMAL_PLAYER_HIDE_SHOW
		dwFlags &= ~FLAG_VISIBLE;
#else
		dwFlags |= FLAG_VISIBLE; // | FLAG_REALLYCLOSE;

		HMODELPIECE hPiece;
        g_pLTClient->GetModelLT()->GetPiece(hPlayerObj, "Torso", hPiece);
        g_pLTClient->GetModelLT()->SetPieceHideStatus(hPlayerObj, hPiece, LTTRUE);
        g_pLTClient->GetModelLT()->GetPiece(hPlayerObj, "Head_zTex1", hPiece);
        g_pLTClient->GetModelLT()->SetPieceHideStatus(hPlayerObj, hPiece, LTTRUE);
#endif

        g_pLTClient->SetObjectFlags(hPlayerObj, dwFlags);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateServerPlayerModel()
//
//	PURPOSE:	Puts the server's player model where our invisible one is
//
// ----------------------------------------------------------------------- //

void CGameClientShell::UpdateServerPlayerModel()
{
	HOBJECT hClientObj, hRealObj;
    LTRotation myRot;
    LTVector myPos;

    if (!(hClientObj = g_pLTClient->GetClientObject())) return;

	if (!(hRealObj = m_MoveMgr.GetObject())) return;

    g_pLTClient->GetObjectPos(hRealObj, &myPos);
    g_pLTClient->SetObjectPos(hClientObj, &myPos);

	if (g_vtPlayerRotate.GetFloat(1.0) > 0.0)
	{
        g_pLTClient->SetupEuler(&myRot, m_fPlayerPitch, m_fPlayerYaw, m_fPlayerRoll);
        g_pLTClient->SetObjectRotation(hClientObj, &myRot);
	}
}


// AN EYE MUST STAY ON THE CENTRE CAMERA'S SIDE OF EVERY WALL.
//
// The intro's opening shot puts its camera right against a hillside - inside
// the brush, in fact, which retail never notices because a polygon's back face
// is not drawn. Offset that camera by half an IPD and one eye lands in the air
// in FRONT of the hill: the engine sees the face from that eye and draws a
// snowy slab across the upper half of the frame, while the other eye, still
// behind the plane, sees the trees. In the headset some things, like the
// trees, showed only in the left eye and not the right. Reproduced at the desk with the tester's
// head pose - the slab is in one column of the sheet and not the other.
//
// So before an eye is rendered, cast from the CENTRE camera to the eye and
// back. A world polygon crossed in either direction means the eye has changed
// sides; put it half a unit short of the surface on the centre's side. Most of
// the separation survives, and both eyes now agree with the camera the level
// designer placed. World geometry only: a door or a character between the two
// eyes is not a side to be on. VREyeClamp 0 switches it off.
static void VRClampEyeToCentre(const LTVector& vCentre, LTVector& vEye, int nEye)
{
	if (g_vtVREyeClamp.GetFloat() <= 0.0f) return;

	const LTVector vD = vEye - vCentre;
	const float    fLen = vD.Mag();
	if (fLen < 0.01f) return;

	for (int nDir = 0; nDir < 2; ++nDir)
	{
		ClientIntersectQuery q;
		ClientIntersectInfo  info;
		memset(&q, 0, sizeof(q));
		q.m_From  = nDir ? vEye : vCentre;
		q.m_To    = nDir ? vCentre : vEye;
		q.m_Flags = IGNORE_NONSOLID;		// the world, not objects

		if (!g_pLTClient->IntersectSegment(&q, &info)) continue;

		const LTVector vNew = info.m_Point - vD * (0.5f / fLen);
		static int s_nSaid[2] = { 0, 0 };
		if (s_nSaid[nEye]++ % 300 == 0)
			VRLog::Msg("eye clamp: eye %d crossed a world polygon (%s), moved %.1f -> %.1f units from the centre",
				nEye, nDir ? "from the eye" : "from the centre",
				fLen, (vNew - vCentre).Mag());
		vEye = vNew;
		return;
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::RenderCamera()
//
//	PURPOSE:	Sets up the client and renders the camera
//
// ----------------------------------------------------------------------- //

void CGameClientShell::RenderCamera(LTBOOL bDrawInterface)
{
	if (!m_bCameraPosInited) return;

	// Make sure the rendered player object is right where it should be.

	UpdateServerPlayerModel();

	// Make sure we process attachments before updating the weapon model
	// and special fx...(some fx are based on attachment positions/rotations)

    g_pLTClient->ProcessAttachments(g_pLTClient->GetClientObject());

	// Make sure the weapon is updated before we render the camera...

	UpdateWeaponModel();

	// Make sure the move-mgr models are updated before we render...

	m_MoveMgr.UpdateModels();

	// PUBLISH THE MODELS NOW, with this frame's camera, movement and weapon
	// pose all applied - once per Update tick, though this runs per eye.
	VRPublishOnce();

	// Update any client-side special effects...

	m_DamageFXMgr.Update();

	// Important to update this after the weapon model has been updated
	// (some fx depend on the position of the weapon model)...

	m_sfxMgr.UpdateSpecialFX();

	// Update the flash light...

	m_FlashLight.Update();

	// VR: the seam. The eye loop wraps the world render and nothing above it -
	// everything from UpdateServerPlayerModel() down to m_FlashLight.Update()
	// advances game state and must run exactly once per frame.
	const int  nStereoMode   = (int)g_vtVRStereo.GetFloat();
	const int  nWorldRenders = (nStereoMode > 0) ? 2 : 1;
	const LTBOOL bSideBySide = (nStereoMode >= 2) ? LTTRUE : LTFALSE;

	// Dump geometry whenever any VR setting changes, not just VRStereo -
	// otherwise the dump describes a configuration that is no longer active.
	const int nSettingsSig = nStereoMode * 100000
		+ (int)(g_vtVRIPD.GetFloat() * 10.0f)
		+ (int)(g_vtVRSwapEyes.GetFloat())  * 100
		+ (int)(g_vtVRFovAdjust.GetFloat()) * 200
		+ (int)(g_vtVRSceneMode.GetFloat()) * 400
		+ (int)(g_vtVROffsetFovScale.GetFloat() * 1000.0f);

	static int s_nLastSettingsSig = -1;
	if (nSettingsSig != s_nLastSettingsSig)
	{
		VRLog::SetExpectedWorldRenders(nWorldRenders);
		VRLog::Msg("world renders per frame -> %d  (VRStereo %d, IPD %.2f, swap %d, fovadjust %d, perEyeScene %d, offsetFovScale %.3f)",
			nWorldRenders, nStereoMode, g_vtVRIPD.GetFloat(),
			(int)g_vtVRSwapEyes.GetFloat(), (int)g_vtVRFovAdjust.GetFloat(),
			(int)g_vtVRSceneMode.GetFloat(), g_vtVROffsetFovScale.GetFloat());
		s_nLastSettingsSig = nSettingsSig;
		g_bLogVRGeometry   = LTTRUE;	// dump one frame of real numbers
	}

	// Arm the consecutive-frame field capture.
	{
		const int nWant = (int)g_vtVRFieldRun.GetFloat();
		if (nWant > 0 && m_nFieldFrame < 0)
		{
			g_vtVRFieldRun.SetFloat(0.0f);
			m_nFieldFrame  = 0;
			m_nFieldFrames = (nWant > 16) ? 16 : nWant;
			VRLog::Msg("field capture armed: %d consecutive frames, %d degree steps",
				m_nFieldFrames, 1);
			VRLog::Msg("  STAND STILL - the frames must differ only by the yaw");
		}
	}

	// The field probe. Once per session automatically, and again whenever
	// VRProbeField is set from the console.
	//
	// Here rather than inside RenderWorldEyes: it moves the camera rect and
	// FOV, and doing that in the middle of the eye passes would be one more
	// thing that has to be unwound correctly on an exception path. Nothing is
	// rendered, so it costs a few function calls.
	{
		static LTBOOL s_bProbedThisRun = LTFALSE;
		const LTBOOL bAsked = (g_vtVRProbeField.GetFloat() > 0.0f) ? LTTRUE : LTFALSE;

		if (bAsked || !s_bProbedThisRun)
		{
			if (bAsked) g_vtVRProbeField.SetFloat(0.0f);
			s_bProbedThisRun = LTTRUE;

			// Two independent measurements of the same quantity, back to back.
			//
			// The probe asks the ENGINE's projection what field a viewport
			// produces. The calibration measures what the RENDERER actually
			// drew, by yawing a known angle and correlating the pixels. They
			// answer the same question through completely different paths, and
			// VRFovXTest is the standing claim that they disagree.
			//
			// Taken together in one frame, with the same camera, they either
			// close that question or measure the gap for the first time.
			ProbeCameraField();
			ProbeVrField();
			if (g_vtVRRenProbe.GetFloat() == 1.0f)
			{
				g_vtVRRenProbe.SetFloat(0.0f);		// once per run
				ProbeRendererSlots();
			}
			CalibrateFovXGuarded();
		}

		// The sweep is separate and always explicit. It runs the calibration
		// four more times, and a measurement that costs four measurements
		// should never happen because something else happened to be due.
		//
		// Gated on actually being in a world. The sweep correlates rendered
		// pixels, and the main menu has none to correlate - armed from the
		// command line it would otherwise fire on the first frame, measure a
		// menu, fail its own control and disarm itself before the level even
		// loaded. This way +VRSweepField 1 at launch ARMS it and it fires when
		// there is something to look at.
		if (g_vtVRSweepField.GetFloat() > 0.0f &&
			m_InterfaceMgr.GetGameState() == GS_PLAYING)
		{
			// And then WAIT. Firing on the first playing frame measured the
			// post-load fade: one field came back "mean luminance 226, sd 0.0
			// - NO TEXTURE", which is a white screen, and the control failed.
			// The fade, the first world tick and any spawn animation all have
			// to be over before there is a stable image to correlate.
			static double s_fPlayingSinceMs = -1.0;
			const double  fPlayNowMs = VRLog::NowMs();
			if (s_fPlayingSinceMs < 0.0) s_fPlayingSinceMs = fPlayNowMs;

			if (fPlayNowMs - s_fPlayingSinceMs >= 6000.0)
			{
				g_vtVRSweepField.SetFloat(0.0f);
				SweepCameraField();
			}
		}
	}

	// The staged renderer probe, if it is armed. Before the world pass, so its
	// bursts are not tangled up in the eye rendering.
	if (g_vtVRRenProbe.GetFloat() >= 2.0f) TickRendererProbe();

	// RenderWorldEyes owns all scene blocks for the world pass. Open a fresh
	// one here for the HUD so the existing EndOptimized2D/End3D below balances.
	const double fRenderStartMs = VRLog::NowMs();
	RenderWorldEyes(nWorldRenders, bSideBySide);
	VRLog::NoteWorldRenderTime(VRLog::NowMs() - fRenderStartMs);

	// M4: time a real back-buffer readback, in-process, on a finished world
	// frame. Called here rather than after the HUD because the transport this
	// measures would copy the world image, and because the number wanted is the
	// cost of the copy, not of whatever happens to be on screen.
	ProbeReadbackTick();

	// Stamp which pose this image was drawn from, before the HUD goes on top.
	//
	// Only in the world. The marker exists to name the pose an image was
	// rendered from, and a menu was not rendered from a pose - the host
	// already ignores the marker there, so drawing it is at best wasted and
	// at worst one more thing to misread.
	// AND WHILE PAUSED. The paused world IS rendered from a pose - VRPauseLook
	// turns it with the head - and without a marker the host could not name
	// that pose, submitted the image against the current one, and the world
	// behind the pause menu fought the head. In the headset the world still
	// juddered, especially on fast head movement.
	if (g_vtVRFrameMarker.GetFloat() > 0.0f && VRShared::IsLive() &&
		(m_InterfaceMgr.GetGameState() == GS_PLAYING
		 || m_InterfaceMgr.GetGameState() == GS_PAUSED))
		DrawFrameMarker(VRShared::State().nFrameCounter);

	// One crosshair per eye, only while actually rendering stereo.
	if (nWorldRenders > 1 && bSideBySide)
	{
		uint32 nCW = 0, nCH = 0;
		g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &nCW, &nCH);
		if (nCW > 1 && nCH > 1) DrawVRCrosshair((int)nCW / 2, (int)nCH);
	}

	g_pLTClient->Start3D();
    g_pLTClient->StartOptimized2D();

	// Alternate Screen Tinting!
	auto hScreen = g_pLTClient->GetScreenSurface();
	LTBOOL bEnableScreenTint = g_vtEnableScreenTint.GetFloat();
	HSURFACE hTintSurface = m_ScreenTintMgr.GetTintSurface();

	if (!bEnableScreenTint && hTintSurface)
	{
		LTRect rDest = { 0, 0, m_ScreenTintMgr.GetTintWidth() , m_ScreenTintMgr.GetTintHeight() };
		g_pLTClient->SetOptimized2DBlend(LTSURFACEBLEND_ADD);
		g_pLTClient->ScaleSurfaceToSurface(hScreen, hTintSurface, &rDest, LTNULL);
		g_pLTClient->SetOptimized2DBlend(LTSURFACEBLEND_ALPHA);
	}

	//

	// VR: the HUD spans the whole window, so in stereo it straddles the seam.
	// Hideable while judging depth; per-eye HUD is M6.
	if (g_vtVRHideHud.GetFloat() <= 0.0f)
	{
		m_InterfaceMgr.Draw();
	}

	// Display any necessary debugging info...

	if (m_hDebugInfo)
	{
		// Get the screen width and height...

        //HSURFACE hScreen = g_pLTClient->GetScreenSurface();
        uint32 nScreenWidth, nScreenHeight;
        g_pLTClient->GetSurfaceDims(hScreen, &nScreenWidth, &nScreenHeight);

		int x = nScreenWidth  - (m_rcDebugInfo.right - m_rcDebugInfo.left);
		int y = nScreenHeight - (m_rcDebugInfo.bottom - m_rcDebugInfo.top);

        g_pLTClient->DrawSurfaceToSurfaceTransparent(hScreen, m_hDebugInfo,
            &m_rcDebugInfo, x, y, g_hColorTransparent);
	}

    g_pLTClient->EndOptimized2D();
    g_pLTClient->End3D();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::GetEyeStashSurface()
//
//	PURPOSE:	VR - lazily created off-screen surface holding one eye while
//				the other is rendered. Recreated if the half-window size
//				changes. Returns LTNULL on failure; the caller degrades to a
//				mono-looking right half rather than crashing (a project rule).
//
// ----------------------------------------------------------------------- //

HSURFACE CGameClientShell::GetEyeStashSurface(int nWidth, int nHeight)
{
	if (nWidth <= 0 || nHeight <= 0) return LTNULL;

	if (m_hEyeStash && (m_nEyeStashW != nWidth || m_nEyeStashH != nHeight))
	{
		g_pLTClient->DeleteSurface(m_hEyeStash);
		m_hEyeStash = LTNULL;
	}

	if (!m_hEyeStash)
	{
		m_hEyeStash = g_pLTClient->CreateSurface((uint32)nWidth, (uint32)nHeight);
		if (m_hEyeStash)
		{
			m_nEyeStashW = nWidth;
			m_nEyeStashH = nHeight;
			VRLog::Msg("eye stash surface created %dx%d", nWidth, nHeight);
		}
		else
		{
			VRLog::Msg("eye stash surface creation FAILED at %dx%d", nWidth, nHeight);
		}
	}

	return m_hEyeStash;
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::LogEyeGeometry()
//
//	PURPOSE:	VR - report what the engine actually stored, never what we
//				assumed it stored. Four measurement mistakes so far came from
//				trusting a set without reading it back.
//
// ----------------------------------------------------------------------- //

void CGameClientShell::LogEyeGeometry(int nEye, const LTVector& vBasePos)
{
	LTBOOL		bFull = LTFALSE;
	int			nL = 0, nT = 0, nR = 0, nB = 0;
	float		fFovX = 0.0f, fFovY = 0.0f;
	LTVector	vPos;

	g_pLTClient->GetCameraRect(m_hCamera, &bFull, &nL, &nT, &nR, &nB);
	g_pLTClient->GetCameraFOV(m_hCamera, &fFovX, &fFovY);
	g_pLTClient->GetObjectPos(m_hCamera, &vPos);

	VRLog::Msg("  pass %d (%s half): rect(%d,%d,%d,%d) fov(%.2f, %.2f)deg  posdelta(%.3f, %.3f, %.3f)",
		nEye, (nEye == 0) ? "right" : "left", nL, nT, nR, nB,
		fFovX * 57.2957795f, fFovY * 57.2957795f,
		vPos.x - vBasePos.x, vPos.y - vBasePos.y, vPos.z - vBasePos.z);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::RenderWorldEyes()
//
//	PURPOSE:	VR - draw the world once or twice, offsetting the camera by
//				half an IPD per eye and splitting the viewport side by side.
//
//				Deliberately free of C++ objects with destructors so that
//				__try/__finally is legal here. Camera position, rotation,
//				viewport and FOV are restored on every exit path including a
//				structured exception (a project rule) - SEH does not unwind C++
//				destructors, so an RAII guard would not be sufficient.
//
// ----------------------------------------------------------------------- //

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ApplyEyeOpticalCentre()
//
//	PURPOSE:	Point this eye's camera at its own optical centre.
//
//				The Quest's lenses are canted, so each eye's frustum centre
//				sits ~7 degrees inward and ~5.5 degrees down from the eye's
//				forward axis. Rendering on the forward axis and declaring that
//				honestly leaves the runtime resampling into a frustum aimed
//				elsewhere, and the error grows toward the edges - a static
//				stretch while still, bending the moment the head turns.
//
//				The host declares the SAME rotation on the submitted pose. The
//				two must move together; a rotated render with an unrotated
//				declaration is worse than neither, which is what ProjMode 1
//				did when it declared the true frustum for a symmetric image.
//
// ----------------------------------------------------------------------- //

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::DrawFrameMarker()
//
//	PURPOSE:	Stamp the frame with which head pose it was rendered from.
//
//				Nothing else can identify a captured image. The host receives
//				pixels with no idea which of the client's frames produced them,
//				so every attempt to correct for staleness has had to GUESS how
//				old the image is - a fixed VRPoseLag, or the compositor's
//				timestamp, which turned out to be a scheduled present time
//				rather than a capture time. Both were wrong, and the guess is
//				worse than the artefact it tries to fix: declaring a pose the
//				image was not rendered from makes the runtime warp it to the
//				wrong place.
//
//				Eight 8x8 blocks encode the low byte of the host frame counter
//				the camera was posed from. The host reads them back and can
//				then look up the exact pose. Drawn at the very top-left, inside
//				the margin the runtime crops away, so it is not visible.
//
// ----------------------------------------------------------------------- //

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::CalibrateFovX()
//
//	PURPOSE:	Measure the horizontal field the renderer ACTUALLY produces.
//
//				VRFovXTest exists because d3d.ren does not give us the field we
//				ask for: it measures FOV against the whole screen surface while
//				each eye renders into half of it. The correction was assumed to
//				be the surface/viewport ratio of 2.0, but 0.8 on top of that -
//				an effective 1.6 - looked better, and nobody knows why.
//
//				That gap is an ANGULAR GAIN error, and it is why head movement
//				feels like the world bending around the player while mouse look
//				feels correct: declaring a wider field than was rendered makes
//				a 10 degree head turn sweep the world by more than 10 degrees.
//
//				It is a geometric fact about a closed binary, so it is measured
//				rather than judged. Render the scene, yaw the camera by a known
//				angle, render again, and find how far the image moved. For a
//				pinhole camera a yaw of d shifts features near the centre by
//				f*tan(d) pixels, so f = shift/tan(d) and the half-field is
//				atan(halfWidth / f).
//
//				Two sweeps of this value produced no usable answer from the
//				headset - correctly, since it is below the threshold at which
//				anyone can rank it. This does not need eyes at all.
//
// ----------------------------------------------------------------------- //

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ProbeReadbackTick()
//
//	PURPOSE:	M4, from the inside: how long does it actually take to get a
//				finished frame out of D3D7 and into system memory?
//
//				The project brief scheduled this measurement at M4 and it has
//				never been taken. Window capture was chosen instead, on the
//				correct observation that the CLIENT SDK cannot read pixels in
//				bulk - GetPixel and SetPixel are all it offers, and GetPixel on
//				the screen surface returns black anyway. That says nothing
//				about reading them one layer down, inside the game process,
//				where the renderer's own DirectDraw surfaces live.
//
//				The proxy DDRAW.dll beside lithtech.exe does the work: it finds
//				the device that owns a primary surface, Blts the finished frame
//				to a system-memory surface, Locks it, and copies every byte -
//				timing all three separately. This just calls it on a rendered
//				frame, because the proxy has no safe way to know when one
//				exists and a background thread touching those surfaces while
//				the renderer uses them would be a different bug.
//
//				Absence of the proxy is reported, not silent. A measurement
//				that quietly does nothing is the failure mode this project has
//				already paid for several times over.
//
// ----------------------------------------------------------------------- //

typedef void (__cdecl *PFN_NolfVrProbeReadback)(void);

void CGameClientShell::ProbeReadbackTick()
{
	static LTBOOL                   s_bResolved = LTFALSE;
	static PFN_NolfVrProbeReadback  s_pfnProbe  = NULL;

	if (!s_bResolved)
	{
		s_bResolved = LTTRUE;

		// Resolve by PATH, not by name.
		//
		// Measured 1 September: GetModuleHandleA("DDRAW.dll") returned the
		// SYSTEM ddraw, not the proxy beside lithtech.exe. Two modules in this
		// process share that base name - the proxy, and the system copy the
		// proxy itself pulled in - and a by-name lookup picked the wrong one.
		// The log line it produced, "loaded but exports no probe", was accurate
		// and pointed straight at it.
		//
		// ".\\DDRAW.dll" is the game directory, which is where the proxy is and
		// where the system copy is not. LoadLibrary on an already-loaded module
		// only takes a reference; it does not load a second instance.
		HMODULE hProxy = LoadLibraryA(".\\DDRAW.dll");
		if (hProxy)
			s_pfnProbe = (PFN_NolfVrProbeReadback)GetProcAddress(hProxy, "NolfVrProbeReadback");

		// Fall back to the by-name lookup, so a differently-staged proxy is
		// still found rather than silently skipped.
		if (!s_pfnProbe)
		{
			HMODULE hByName = GetModuleHandleA("DDRAW.dll");
			if (hByName)
				s_pfnProbe = (PFN_NolfVrProbeReadback)GetProcAddress(hByName, "NolfVrProbeReadback");
		}

		if (s_pfnProbe)
			VRLog::Msg("ddraw readback probe: proxy found, sampling %d frames (VRProbeReadback)",
				(int)g_vtVRProbeReadback.GetFloat());
		else if (hProxy)
			VRLog::Msg("ddraw readback probe: game\\DDRAW.dll loaded but exports no probe - rebuild it with tools/build-ddraw-proxy.ps1");
		else
			VRLog::Msg("ddraw readback probe: no proxy DDRAW.dll in the game folder, skipping");
	}

	if (!s_pfnProbe) return;

	const int nLeft = (int)g_vtVRProbeReadback.GetFloat();
	if (nLeft <= 0) return;

	// The console variable IS the countdown, so it can be re-armed from the
	// console mid-run by setting it again.
	g_vtVRProbeReadback.SetFloat((LTFLOAT)(nLeft - 1));
	s_pfnProbe();

	if (nLeft == 1)
		VRLog::Msg("ddraw readback probe: sampling done - the numbers are in game\\logs\\ddraw-proxy.log");
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::EffectiveIPDUnits()
//
//	PURPOSE:	Eye separation in world units, from the headset when it can be
//				trusted and from VRIPD when it cannot.
//
//				A headset's IPD wheel can be mechanically loose and wander between
//				59 and 62 mm on its own; set to 62, the
//				range cannot be told apart, so this is not about how it looks - it is about
//				not letting the stereo baseline change by 5% between two runs
//				being compared.
//
//				Every rejection falls back to the fixed value rather than to a
//				guess (a project rule).
//
// ----------------------------------------------------------------------- //

float CGameClientShell::EffectiveIPDUnits()
{
	const float fManual = g_vtVRIPD.GetFloat();

	if (g_vtVRIPDAuto.GetFloat() <= 0.0f || !VRShared::IsLive()) return fManual;

	const float fUnitMM = g_vtVRUnitMM.GetFloat();
	if (fUnitMM < 1.0f) return fManual;

	const float fMM = VRShared::State().fIpdMeters * 1000.0f;

	// Human IPD does not live outside this range. Anything else is a runtime
	// that has not measured yet, or a zeroed shared block.
	if (fMM < 45.0f || fMM > 80.0f) return fManual;

	const float fUnits = fMM / fUnitMM;

	// Say so whenever the hardware moves. Without this line a run that felt
	// different has no way of being explained after the fact.
	static float s_fLastMM = 0.0f;
	if (fabs(fMM - s_fLastMM) > 0.25f)
	{
		VRLog::Msg("headset IPD %.1f mm -> eye separation %.2f units (%.2f mm/unit); fixed VRIPD is %.2f",
			fMM, fUnits, fUnitMM, fManual);
		if (s_fLastMM > 0.0f)
		{
			VRLog::Msg("  NOTE: the headset's IPD changed by %+.1f mm mid-session - the wheel has moved",
				fMM - s_fLastMM);
		}
		s_fLastMM = fMM;
	}

	return fUnits;
}




// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::TickRendererProbe()
//
//	PURPOSE:	One renderer operation per five-second window.
//
//				Round 1 fired every operation at once. The surface create and
//				delete slots came out clean because they are otherwise idle,
//				but the pixel and blit work all landed in one interval and
//				could not be separated.
//
//				Staging them means each burst has the neighbouring intervals as
//				its own control. The excess in the window names one slot, and
//				nothing has to be assumed about what the neighbouring slots do.
//
//				Called every frame while VRRenProbe is set; does nothing except
//				in the first frame of each window.
//
// ----------------------------------------------------------------------- //

void CGameClientShell::TickRendererProbe()
{
	static double s_fStartMs = -1.0;
	static int    s_nDone    = 0;

	const double fNow = VRLog::NowMs();
	if (s_fStartMs < 0.0)
	{
		s_fStartMs = fNow;
		VRLog::Msg("--- staged renderer probe: one operation per 5 s window ---");
	}

	// Window 0 is left empty on purpose: it is the baseline the shim's slice
	// logging needs, taken under exactly the conditions the bursts run in.
	const int nWindow = (int)((fNow - s_fStartMs) / 5000.0);
	if (nWindow <= 0 || nWindow > 8 || nWindow == s_nDone) return;
	s_nDone = nWindow;

	HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	HSURFACE hA = g_pLTClient->CreateSurface(32, 32);
	HSURFACE hB = g_pLTClient->CreateSurface(32, 32);
	if (!hA || !hB)
	{
		VRLog::Msg("  window %d: could not create scratch surfaces - skipped", nWindow);
		if (hA) g_pLTClient->DeleteSurface(hA);
		if (hB) g_pLTClient->DeleteSurface(hB);
		return;
	}

	const HLTCOLOR c = g_pLTClient->CreateColor(255.0f, 0.0f, 255.0f, LTFALSE);
	LTRect rAll; rAll.left = 0; rAll.top = 0; rAll.right = 32; rAll.bottom = 32;

	int nCount = 0;
	const char* pszWhat = "";

	switch (nWindow)
	{
		case 1:
			nCount = 601; pszWhat = "SetPixel";
			for (int i = 0; i < nCount; ++i)
				g_pLTClient->SetPixel(hA, (uint32)(i % 32), (uint32)((i / 32) % 32), c);
			break;

		case 2:
			nCount = 701; pszWhat = "OptimizeSurface + UnoptimizeSurface pairs";
			for (int i = 0; i < nCount; ++i)
			{
				g_pLTClient->OptimizeSurface(hA, c);
				g_pLTClient->UnoptimizeSurface(hA);
			}
			break;

		case 3:
			nCount = 809; pszWhat = "DrawSurfaceToSurface onto the SCREEN surface";
			if (hScreen)
			{
				g_pLTClient->Start3D();
				g_pLTClient->StartOptimized2D();
				for (int i = 0; i < nCount; ++i)
					g_pLTClient->DrawSurfaceToSurface(hScreen, hA, &rAll, 0, 0);
				g_pLTClient->EndOptimized2D();
				g_pLTClient->End3D();
			}
			else { nCount = 0; pszWhat = "no screen surface - skipped"; }
			break;

		case 4:
			nCount = 907; pszWhat = "FillRect";
			for (int i = 0; i < nCount; ++i)
				g_pLTClient->FillRect(hA, &rAll, c);
			break;

		case 5:
			nCount = 1009; pszWhat = "DrawSurfaceToSurface OFFSCREEN";
			g_pLTClient->Start3D();
			g_pLTClient->StartOptimized2D();
			for (int i = 0; i < nCount; ++i)
				g_pLTClient->DrawSurfaceToSurface(hB, hA, &rAll, 0, 0);
			g_pLTClient->EndOptimized2D();
			g_pLTClient->End3D();
			break;

		// Round 3: the front of the table. Slot 5 shows no calls where
		// CreateContext should be and slot 6 shows thousands where
		// DeleteContext cannot be, so the 5-6 region is not what AvP2's order
		// says it is and has to be measured like the rest.
		//
		// These are BALANCED pairs of calls the client already makes several
		// times a frame, so driving a few hundred extra is the same kind of
		// work the renderer is doing anyway - not an unbalanced sequence that
		// could leave its 3D state somewhere it cannot recover from.
		case 6:
			nCount = 313; pszWhat = "Start3D + End3D pairs";
			for (int i = 0; i < nCount; ++i)
			{
				g_pLTClient->Start3D();
				g_pLTClient->End3D();
			}
			break;

		case 7:
			nCount = 409; pszWhat = "StartOptimized2D + EndOptimized2D pairs";
			g_pLTClient->Start3D();
			for (int i = 0; i < nCount; ++i)
			{
				g_pLTClient->StartOptimized2D();
				g_pLTClient->EndOptimized2D();
			}
			g_pLTClient->End3D();
			break;

		case 8:
			nCount = 509; pszWhat = "ClearScreen";
			{
				LTRect rClear; rClear.left = 0; rClear.top = 0;
				rClear.right = 16; rClear.bottom = 16;
				for (int i = 0; i < nCount; ++i)
					g_pLTClient->ClearScreen(&rClear, CLEARSCREEN_SCREEN);
			}
			break;
	}

	g_pLTClient->DeleteSurface(hA);
	g_pLTClient->DeleteSurface(hB);

	VRLog::Msg("  window %d: %d x %s", nWindow, nCount, pszWhat);
	if (nWindow == 8) VRLog::Msg("--- end staged renderer probe ---");
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ProbeRendererSlots()
//
//	PURPOSE:	Make the renderer's function table identify itself.
//
//				The pass-through d3d.ren shim counts calls to all 37 renderer
//				entry points but has no way to know which is which. AvP2's
//				header gives an ORDER, and the measured table agrees with it
//				for slots 0-4 and then diverges - so the rest has to be
//				established rather than assumed.
//
//				This drives each operation an odd, distinctive number of times.
//				Whichever slot counter moves by 101 is the create-surface path,
//				by 211 the surface-info path, and so on. Two runs - one with
//				this and one without - and the difference names them.
//
//				Costs nothing and needs no headset. Off unless VRRenProbe is
//				set, and runs once.
//
// ----------------------------------------------------------------------- //

void CGameClientShell::ProbeRendererSlots()
{
	const int kCreates = 101;
	const int kInfos   = 211;
	const int kPixels  = 307;
	const int kBlits   = 401;

	VRLog::Msg("--- renderer slot probe ---");
	VRLog::Msg("  creates %d, surface-info %d, pixels %d, blits %d",
		kCreates, kInfos, kPixels, kBlits);

	// 1. Create and destroy, in pairs. Small, so this is cheap.
	int nMade = 0;
	for (int i = 0; i < kCreates; ++i)
	{
		HSURFACE h = g_pLTClient->CreateSurface(8, 8);
		if (!h) break;
		++nMade;
		g_pLTClient->DeleteSurface(h);
	}
	VRLog::Msg("  created and deleted %d surfaces", nMade);

	// One scratch surface for the rest.
	HSURFACE hScratch = g_pLTClient->CreateSurface(32, 32);
	if (!hScratch)
	{
		VRLog::Msg("  could not create the scratch surface - probe incomplete");
		VRLog::Msg("--- end renderer slot probe ---");
		return;
	}

	// 2. Ask for its dimensions.
	uint32 nW = 0, nH = 0;
	for (int i = 0; i < kInfos; ++i)
		g_pLTClient->GetSurfaceDims(hScratch, &nW, &nH);
	VRLog::Msg("  queried surface dims %d times (reads %ux%u)", kInfos, nW, nH);

	// 3. Write pixels. Whatever lock/unlock the renderer does for this is the
	//    pair that moves.
	const HLTCOLOR c = g_pLTClient->CreateColor(255.0f, 0.0f, 255.0f, LTFALSE);
	for (int i = 0; i < kPixels; ++i)
		g_pLTClient->SetPixel(hScratch, (uint32)(i % 32), (uint32)((i / 32) % 32), c);
	VRLog::Msg("  set %d pixels", kPixels);

	// 4. Blits, inside ONE 3D/2D block rather than hundreds - entering and
	//    leaving the renderer's 3D state repeatedly is not something to do
	//    several hundred times for a measurement.
	HSURFACE hDest = g_pLTClient->CreateSurface(32, 32);
	if (hDest)
	{
		LTRect rSrc;
		rSrc.left = 0; rSrc.top = 0; rSrc.right = 32; rSrc.bottom = 32;

		g_pLTClient->Start3D();
		g_pLTClient->StartOptimized2D();
		for (int i = 0; i < kBlits; ++i)
			g_pLTClient->DrawSurfaceToSurface(hDest, hScratch, &rSrc, 0, 0);
		g_pLTClient->EndOptimized2D();
		g_pLTClient->End3D();

		VRLog::Msg("  blitted %d times inside one 3D block", kBlits);
		g_pLTClient->DeleteSurface(hDest);
	}
	else
	{
		VRLog::Msg("  no destination surface - blits skipped");
	}

	g_pLTClient->DeleteSurface(hScratch);
	VRLog::Msg("--- end renderer slot probe ---");
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ProbeVrField()
//
//	PURPOSE:	Measure the ENGINE's projection at the field VR asks for.
//
//				ProbeCameraField() asks for 90 degrees and varies the viewport
//				WIDTH, which settled the horizontal question. It says nothing
//				about the vertical, and it never asks for anything near the
//				121-degree vertical the VR path uses.
//
//				This sweeps the vertical ask, in the eye's own viewport, and
//				prints what comes back. Three outcomes and they are not alike:
//
//				  - measured tracks asked        the engine is faithful; the
//				                                 fault is further down, in what
//				                                 d3d.ren rasterises
//				  - measured saturates           the engine CLAMPS, and we have
//				                                 been declaring a field that was
//				                                 never rendered
//				  - measured follows the aspect  the engine ignores fovY and
//				                                 derives it from fovX and the
//				                                 viewport shape
//
//				Get3DCameraPt only. Nothing is rendered, so this cannot be
//				fooled by a blank wall, by the weapon, or by the surface reads
//				that produced four confident wrong answers before it.
//
// ----------------------------------------------------------------------- //

void CGameClientShell::ProbeVrField()
{
	if (!m_hCamera) return;

	HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	if (!hScreen) return;

	uint32 nSurfW = 0, nSurfH = 0;
	g_pLTClient->GetSurfaceDims(hScreen, &nSurfW, &nSurfH);
	if (nSurfW < 64 || nSurfH < 64) return;

	// Save everything touched (a project rule).
	LTVector   vSavedPos;
	LTRotation rSavedRot;
	float      fSavedFovX = 0.0f, fSavedFovY = 0.0f;
	LTBOOL     bSavedFull = LTFALSE;
	int        nL = 0, nT = 0, nR = 0, nB = 0;

	g_pLTClient->GetObjectPos(m_hCamera, &vSavedPos);
	g_pLTClient->GetObjectRotation(m_hCamera, &rSavedRot);
	g_pLTClient->GetCameraFOV(m_hCamera, &fSavedFovX, &fSavedFovY);
	g_pLTClient->GetCameraRect(m_hCamera, &bSavedFull, &nL, &nT, &nR, &nB);

	LTVector vUp, vRight, vForward;
	g_pLTClient->GetRotationVectors(&rSavedRot, &vUp, &vRight, &vForward);

	const float fR2D = 57.2957795f;

	// The eye viewport, exactly as the VR path sets it.
	const int nW = (int)nSurfW / 2;
	const int nH = (int)nSurfH;

	VRLog::Msg("--- VR field probe: does the engine honour a wide VERTICAL fov? ---");
	VRLog::Msg("  eye viewport %dx%d (aspect %.3f). Horizontal held at 119.18 deg.",
		nW, nH, (float)nW / (float)nH);
	VRLog::Msg("  askedY   measuredY   measuredX   verdict");

	// Held horizontal, swept vertical. 121 is what VR asks for today.
	const float kAskY[6] = { 60.0f, 90.0f, 110.0f, 121.0f, 140.0f, 160.0f };
	const float fAskX    = 119.18f;

	for (int i = 0; i < 6; ++i)
	{
		g_pLTClient->SetCameraRect(m_hCamera, LTFALSE, 0, 0, nW, nH);
		g_pLTClient->SetCameraFOV(m_hCamera, fAskX / fR2D, kAskY[i] / fR2D);

		// What the engine says it stored, before asking what it projects.
		float fGotX = 0.0f, fGotY = 0.0f;
		g_pLTClient->GetCameraFOV(m_hCamera, &fGotX, &fGotY);

		// One pixel inside the rect; the call reports LT_OUTSIDE on the edge.
		LTVector vC, vRt, vTp;
		const LTRESULT rC  = g_pLTClient->Get3DCameraPt(m_hCamera, nW / 2, nH / 2, &vC);
		const LTRESULT rRt = g_pLTClient->Get3DCameraPt(m_hCamera, nW - 2,  nH / 2, &vRt);
		const LTRESULT rTp = g_pLTClient->Get3DCameraPt(m_hCamera, nW / 2,  1,      &vTp);

		if (rC != LT_OK || rRt != LT_OK || rTp != LT_OK)
		{
			VRLog::Msg("  %6.1f   Get3DCameraPt refused (%d/%d/%d)",
				kAskY[i], (int)rC, (int)rRt, (int)rTp);
			continue;
		}

		// Same world/camera-space decision ProbeCameraField makes, from the
		// data rather than from an assumption about the header.
		LTVector vCamRel = vC - vSavedPos;
		const float fMagAbs = vC.Mag();
		const float fMagRel = vCamRel.Mag();
		const bool  bWorld  = (fMagRel > 0.25f && fMagRel < 4.0f)
						   && !(fMagAbs > 0.25f && fMagAbs < 4.0f);

		LTVector vDirRt = bWorld ? (vRt - vSavedPos) : vRt;
		LTVector vDirTp = bWorld ? (vTp - vSavedPos) : vTp;

		const float fRtX = vDirRt.Dot(vRight), fRtZ = vDirRt.Dot(vForward);
		const float fTpY = vDirTp.Dot(vUp),    fTpZ = vDirTp.Dot(vForward);

		if (fabs(fRtZ) < 0.0001f || fabs(fTpZ) < 0.0001f)
		{
			VRLog::Msg("  %6.1f   degenerate forward component - skipped", kAskY[i]);
			continue;
		}

		const float fHalfX = (float)atan(fabs(fRtX / fRtZ)) * fR2D;
		const float fHalfY = (float)atan(fabs(fTpY / fTpZ)) * fR2D;

		// The sampling inset is two pixels, which covers a fixed small angle -
		// so a fraction of a degree of shortfall is the instrument and not the
		// engine. Anything larger is not.
		const float fErr = fHalfY * 2.0f - kAskY[i];

		const char* pszVerdict;
		if (fabs(fGotY - kAskY[i] / fR2D) > 0.001f)   pszVerdict = "ENGINE CHANGED THE STORED VALUE";
		else if (fabs(fErr) < 0.5f)                   pszVerdict = "honoured";
		else if (fErr < -0.5f)                        pszVerdict = "SHORT - clamped or derived";
		else                                          pszVerdict = "LONGER than asked";

		VRLog::Msg("  %6.1f   %9.2f   %9.2f   %s (err %+.2f)",
			kAskY[i], fHalfY * 2.0f, fHalfX * 2.0f, pszVerdict, fErr);
	}

	// Restore (a project rule).
	g_pLTClient->SetCameraRect(m_hCamera, bSavedFull, nL, nT, nR, nB);
	g_pLTClient->SetCameraFOV(m_hCamera, fSavedFovX, fSavedFovY);

	VRLog::Msg("  a faithful engine gives measuredY == askedY on every row.");
	VRLog::Msg("--- end VR field probe ---");
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ProbeCameraField()
//
//	PURPOSE:	Ask the engine what field it actually produces, analytically.
//
//				Get3DCameraPt maps a SCREEN PIXEL to a point one unit out along
//				the camera's forward vector. That is the projection itself, so
//				the field falls straight out of it:
//
//				    tan(half-fovX) = sideways component at the viewport edge
//
//				docs/GHOSTING.md listed this route and dismissed it with
//				"iltclient.h has no such call". It does - it is right above
//				GetCameraFOV in the header, and it has been there since 2001.
//
//				This costs no render, no image correlation, no headset and no
//				judgement from anyone. It is the opposite of every VRFovXTest
//				sweep, all of which failed because they asked a person to rank
//				values a person cannot rank.
//
//				WHAT IT CANNOT TELL US, said plainly: this is the ENGINE's
//				projection. If the engine and d3d.ren disagree about how FOV
//				maps to a viewport, this reports the engine's answer and the
//				renderer may still draw something else - and that disagreement
//				is precisely the VRFovXTest mystery. So it is evidence, not
//				proof, and it wants checking against the F2 image correlation.
//
//				The probe sweeps viewport widths on purpose. The standing
//				theory is that the renderer measures FOV against the whole
//				SCREEN SURFACE rather than the camera rect, which is where the
//				2.0 in the FOV maths comes from. If that is true, the measured
//				half-angle stays put as the viewport narrows. If it is false,
//				it tracks the viewport. One run separates those.
//
// ----------------------------------------------------------------------- //

void CGameClientShell::ProbeCameraField()
{
	if (!m_hCamera) return;

	HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	if (!hScreen) return;

	uint32 nSurfW = 0, nSurfH = 0;
	g_pLTClient->GetSurfaceDims(hScreen, &nSurfW, &nSurfH);
	if (nSurfW < 64 || nSurfH < 64) return;

	// Save everything we touch (a project rule).
	LTVector   vSavedPos;
	LTRotation rSavedRot;
	float      fSavedFovX = 0.0f, fSavedFovY = 0.0f;
	LTBOOL     bSavedFull = LTFALSE;
	int        nL = 0, nT = 0, nR = 0, nB = 0;

	g_pLTClient->GetObjectPos(m_hCamera, &vSavedPos);
	g_pLTClient->GetObjectRotation(m_hCamera, &rSavedRot);
	g_pLTClient->GetCameraFOV(m_hCamera, &fSavedFovX, &fSavedFovY);
	g_pLTClient->GetCameraRect(m_hCamera, &bSavedFull, &nL, &nT, &nR, &nB);

	LTVector vUp, vRight, vForward;
	g_pLTClient->GetRotationVectors(&rSavedRot, &vUp, &vRight, &vForward);

	const float fR2D = 57.2957795f;

	VRLog::Msg("--- camera field probe (Get3DCameraPt) ---");
	VRLog::Msg("  screen surface %ux%u, camera rect was (%d,%d,%d,%d) full=%d, fov was (%.2f, %.2f) deg",
		nSurfW, nSurfH, nL, nT, nR, nB, (int)bSavedFull, fSavedFovX * fR2D, fSavedFovY * fR2D);

	// Ask for a round number, then vary only the viewport width.
	const float fAskDeg  = 90.0f;
	const float fAskRad  = fAskDeg / fR2D;
	const int   nWidths[3] = { (int)nSurfW, (int)nSurfW / 2, (int)nSurfW / 4 };

	for (int i = 0; i < 3; ++i)
	{
		const int nW = nWidths[i];
		const int nH = (int)nSurfH;
		if (nW < 32) continue;

		g_pLTClient->SetCameraRect(m_hCamera, LTFALSE, 0, 0, nW, nH);
		g_pLTClient->SetCameraFOV(m_hCamera, fAskRad, fAskRad);

		// Centre, right edge, top edge. One pixel inside the rect, because the
		// call reports LT_OUTSIDE for anything on or past the boundary.
		LTVector vC, vRt, vTp;
		const LTRESULT rC  = g_pLTClient->Get3DCameraPt(m_hCamera, nW / 2, nH / 2, &vC);
		const LTRESULT rRt = g_pLTClient->Get3DCameraPt(m_hCamera, nW - 2,  nH / 2, &vRt);
		const LTRESULT rTp = g_pLTClient->Get3DCameraPt(m_hCamera, nW / 2,  1,      &vTp);

		if (rC != LT_OK || rRt != LT_OK || rTp != LT_OK)
		{
			VRLog::Msg("  viewport %dx%d: Get3DCameraPt refused (%d/%d/%d) - skipped",
				nW, nH, (int)rC, (int)rRt, (int)rTp);
			continue;
		}

		// The header says "one unit out along the forward vector" without
		// saying in whose space. Decide from the data rather than assuming:
		// a camera-space point sits about a unit from the origin, a world-space
		// one about a unit from the camera. Whichever is near unit length wins.
		LTVector vCamRel = vC - vSavedPos;
		const float fMagAbs = vC.Mag();
		const float fMagRel = vCamRel.Mag();
		const bool  bWorld  = (fMagRel > 0.25f && fMagRel < 4.0f)
						   && !(fMagAbs > 0.25f && fMagAbs < 4.0f);

		if (i == 0)
		{
			VRLog::Msg("  centre point returns (%.3f, %.3f, %.3f); |p|=%.2f, |p-cam|=%.2f -> reading as %s space",
				vC.x, vC.y, vC.z, fMagAbs, fMagRel, bWorld ? "WORLD" : "CAMERA");
		}

		LTVector vDirRt = bWorld ? (vRt - vSavedPos) : vRt;
		LTVector vDirTp = bWorld ? (vTp - vSavedPos) : vTp;

		// Into the camera's own basis, so the answer does not depend on where
		// the player happens to be standing or facing.
		const float fRtX = vDirRt.Dot(vRight),   fRtZ = vDirRt.Dot(vForward);
		const float fTpY = vDirTp.Dot(vUp),      fTpZ = vDirTp.Dot(vForward);

		if (fabs(fRtZ) < 0.0001f || fabs(fTpZ) < 0.0001f)
		{
			VRLog::Msg("  viewport %dx%d: degenerate forward component - skipped", nW, nH);
			continue;
		}

		const float fHalfX = (float)atan(fabs(fRtX / fRtZ));
		const float fHalfY = (float)atan(fabs(fTpY / fTpZ));

		// Two competing predictions, printed side by side so the answer needs
		// no arithmetic from the reader.
		const float fIfViewport = fAskRad * 0.5f;
		const float fIfSurface  = (float)atan(tan(fAskRad * 0.5f) * (float)nW / (float)nSurfW);

		VRLog::Msg("  viewport %4dx%d (%.2f of surface): asked half-fovX %.2f, MEASURED %.2f deg  [half-fovY %.2f]",
			nW, nH, (float)nW / (float)nSurfW,
			fIfViewport * fR2D, fHalfX * fR2D, fHalfY * fR2D);
		VRLog::Msg("      if FOV is measured against the VIEWPORT -> %.2f deg   (error %+.2f)",
			fIfViewport * fR2D, (fHalfX - fIfViewport) * fR2D);
		VRLog::Msg("      if FOV is measured against the SURFACE  -> %.2f deg   (error %+.2f)",
			fIfSurface * fR2D, (fHalfX - fIfSurface) * fR2D);

		if (i == 1)
		{
			// The half-width case is the one VR actually renders into, so say
			// what VRFovXTest would have to be for our request to come out
			// right. tan-space, because that is where projection lives.
			const float fWant = tan(fAskRad * 0.5f);
			const float fGot  = tan(fHalfX);
			if (fGot > 0.0001f)
			{
				VRLog::Msg("      -> at half width, the renderer gives %.3f of the tangent we asked for",
					fGot / fWant);
			}
		}
	}

	// Restore before anything else can observe the camera.
	g_pLTClient->SetObjectRotation(m_hCamera, &rSavedRot);
	g_pLTClient->SetObjectPos(m_hCamera, &vSavedPos);
	g_pLTClient->SetCameraFOV(m_hCamera, fSavedFovX, fSavedFovY);
	g_pLTClient->SetCameraRect(m_hCamera, bSavedFull, nL, nT, nR, nB);

	VRLog::Msg("--- end camera field probe ---");
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::CalibrateFovXGuarded()
//
//	PURPOSE:	Run the renderer-side calibration without letting it take the
//				game down.
//
//				CalibrateFovX has been in the tree since 13 August and has
//				NEVER BEEN CALLED - nothing referenced it. So it is effectively
//				untested code that renders twice out of band and reads the
//				screen surface a pixel at a time.
//				
//
//				The wrapper exists because SEH cannot live in a function with
//				C++ objects that have destructors, and CalibrateFovX is full of
//				them. A fault here costs one log line, not the session
//				(a project rule).
//
// ----------------------------------------------------------------------- //

void CGameClientShell::CalibrateFovXGuarded()
{
	__try
	{
		CalibrateFovX();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		VRLog::Msg("renderer field calibration FAULTED - skipped, game continues");
	}
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::DumpEyeImage()
//
//	PURPOSE:	Write the eye viewport to a BMP next to this run's log.
//
//				GetPixel is the only surface read this SDK offers and it costs
//				a call per pixel, so this subsamples hard. It is a diagnostic
//				that runs once when asked, not something in a frame path.
//
// --------------------------------------------------------------------------- //

void CGameClientShell::DumpEyeImage(const char* pszTag)
{
	const char* pszDir = VRLog::Dir();
	if (!pszDir || !pszDir[0]) return;

	HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	if (!hScreen || !m_hCamera) return;

	uint32 nSurfW = 0, nSurfH = 0;
	g_pLTClient->GetSurfaceDims(hScreen, &nSurfW, &nSurfH);
	if (nSurfW < 64 || nSurfH < 64) return;

	const int nHalfW = (int)nSurfW / 2;
	const int nViewH = (int)nSurfH;

	HSURFACE hStash = GetEyeStashSurface(nHalfW, nViewH);
	if (!hStash) return;

	// RENDER, then copy.
	//
	// The first version of this omitted the render, on the reasoning that the
	// screen surface holds the last frame anyway. It read back entirely empty -
	// 0 of 134280 samples above threshold - which is itself the finding: on
	// this engine the screen surface is only readable straight after a render
	// in the same call sequence. Anything that reads it at some other moment
	// gets nothing, silently, and that is indistinguishable from a dark scene.
	LTRect rHalf;
	rHalf.left = 0; rHalf.top = 0; rHalf.right = nHalfW; rHalf.bottom = nViewH;

	g_pLTClient->SetCameraRect(m_hCamera, LTFALSE, 0, 0, nHalfW, nViewH);
	g_pLTClient->Start3D();
	g_pLTClient->RenderCamera(m_hCamera);
	g_pLTClient->End3D();

	g_pLTClient->Start3D();
	g_pLTClient->StartOptimized2D();
	g_pLTClient->DrawSurfaceToSurface(hStash, hScreen, &rHalf, 0, 0);
	g_pLTClient->EndOptimized2D();
	g_pLTClient->End3D();

	DumpStashImage(pszTag, hStash, nHalfW, nViewH);
}

void CGameClientShell::DumpStashImage(const char* pszTag, HSURFACE hStash,
									  int nHalfW, int nViewH, int nStepIn,
									  int nY0In, int nY1In)
{
	const char* pszDir = VRLog::Dir();
	if (!pszDir || !pszDir[0] || !hStash) return;

	// Downsample. 4 is right for "is there anything on this buffer at all";
	// the field measurement needs 1, because it is measuring a shift of a
	// few pixels and a 4x reduction throws three quarters of the precision
	// away before the correlation ever sees it.
	const int kStep = (nStepIn > 0) ? nStepIn : 4;
	// A ROW BAND, not the whole frame.
	//
	// This function reads the surface one pixel at a time through GetPixel,
	// which is the only bulk-read this SDK offers and is very slow. At the
	// default step of 4 over a full frame that is 134k calls and takes a few
	// milliseconds. At step 1 it is 2.15 MILLION calls per frame, which hangs
	// the game outright - measured: no frames rendered at all, and the control
	// with the capture disarmed ran normally.
	//
	// A horizontal shift only needs full resolution in X. Taking a band keeps
	// the call count near the figure that is known to work while giving every
	// horizontal pixel, and it is also how the player's weapon is kept out of
	// the sample: the view model sits at the bottom centre, has zero parallax,
	// and correlates perfectly with itself at zero shift.
	const int nY0 = (nY0In > 0) ? nY0In : 0;
	const int nY1 = (nY1In > 0 && nY1In <= nViewH) ? nY1In : nViewH;
	if (nY1 - nY0 < kStep * 8) return;

	const int nOutW = nHalfW / kStep;
	const int nOutH = (nY1 - nY0) / kStep;
	if (nOutW < 8 || nOutH < 8) return;

	const int nRowBytes = (nOutW * 3 + 3) & ~3;

	char szPath[MAX_PATH];
	sprintf(szPath, "%s\\eye-%s.bmp", pszDir, pszTag ? pszTag : "dump");

	FILE* f = fopen(szPath, "wb");
	if (!f)
	{
		VRLog::Msg("eye dump: could not open %s", szPath);
		return;
	}

	const uint32 nPixBytes = (uint32)nRowBytes * (uint32)nOutH;
	const uint32 nOffBits  = 14 + 40;

	// BMP header, written byte by byte: the structs are packed differently
	// under this project's pragmas and a struct write here has no way to be
	// checked short of opening the file.
	unsigned char h[54];
	memset(h, 0, sizeof(h));
	h[0] = 'B'; h[1] = 'M';
	const uint32 nFileSize = nOffBits + nPixBytes;
	memcpy(h + 2,  &nFileSize, 4);
	memcpy(h + 10, &nOffBits,  4);
	const uint32 nHdrSize = 40;
	memcpy(h + 14, &nHdrSize, 4);
	const int32 nW32 = nOutW, nH32 = nOutH;
	memcpy(h + 18, &nW32, 4);
	memcpy(h + 22, &nH32, 4);
	const uint16 nPlanes = 1, nBits = 24;
	memcpy(h + 26, &nPlanes, 2);
	memcpy(h + 28, &nBits,   2);
	memcpy(h + 34, &nPixBytes, 4);
	fwrite(h, 1, 54, f);

	unsigned char* pRow = (unsigned char*)malloc(nRowBytes);
	if (!pRow) { fclose(f); return; }

	// Counted while reading, so the log can say whether the image is empty
	// without anyone having to open it.
	double fSum = 0.0, fSumSq = 0.0;
	int    nNonZero = 0, nTotal = 0;
	int    nMinX = nOutW, nMaxX = -1, nMinY = nOutH, nMaxY = -1;

	// BMP rows run bottom-up.
	for (int oy = nOutH - 1; oy >= 0; --oy)
	{
		memset(pRow, 0, nRowBytes);
		for (int ox = 0; ox < nOutW; ++ox)
		{
			HLTCOLOR c = 0;
			g_pLTClient->GetPixel(hStash, (uint32)(ox * kStep), (uint32)(nY0 + oy * kStep), &c);
			const int r = (int)((c >> 16) & 0xFF);
			const int g = (int)((c >>  8) & 0xFF);
			const int b = (int)( c        & 0xFF);

			pRow[ox * 3 + 0] = (unsigned char)b;
			pRow[ox * 3 + 1] = (unsigned char)g;
			pRow[ox * 3 + 2] = (unsigned char)r;

			const int nLum = (r * 77 + g * 151 + b * 28) >> 8;
			fSum   += nLum;
			fSumSq += (double)nLum * nLum;
			++nTotal;
			if (nLum > 8)
			{
				++nNonZero;
				if (ox < nMinX) nMinX = ox;
				if (ox > nMaxX) nMaxX = ox;
				if (oy < nMinY) nMinY = oy;
				if (oy > nMaxY) nMaxY = oy;
			}
		}
		fwrite(pRow, 1, nRowBytes, f);
	}

	free(pRow);
	fclose(f);

	const double fMean = (nTotal > 0) ? (fSum / nTotal) : 0.0;
	const double fVar  = (nTotal > 0) ? ((fSumSq / nTotal) - fMean * fMean) : 0.0;

	VRLog::Msg("eye dump -> %s  (%dx%d, every %d px)", szPath, nOutW, nOutH, kStep);
	VRLog::Msg("  mean luminance %.1f, sd %.1f, %d of %d samples above 8 (%.0f%%)",
		fMean, sqrt(fVar > 0.0 ? fVar : 0.0), nNonZero, nTotal,
		100.0 * nNonZero / (nTotal > 0 ? nTotal : 1));

	if (nMaxX >= 0)
	{
		// The bounding box separates "nothing was drawn" from "content is
		// confined to a rectangle" - a viewport or canvas fact - from "the
		// scene is simply dark". Those need different fixes and look the same
		// in any single average.
		VRLog::Msg("  content bounding box x %d..%d of %d, y %d..%d of %d",
			nMinX, nMaxX, nOutW - 1, nMinY, nMaxY, nOutH - 1);
	}
	else
	{
		VRLog::Msg("  NOTHING ABOVE THRESHOLD - the eye read back empty");
	}
}

void CGameClientShell::CalibrateFovYGuarded()
{
	__try
	{
		CalibrateFovY();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		VRLog::Msg("renderer VERTICAL field calibration FAULTED - skipped, game continues");
	}
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::SweepCameraField()
//
//	PURPOSE:	Measure the renderer's tangent ratio at several asked fields,
//				and say whether it is a constant.
//
//				docs/FIELD-MEASURED.md measured 0.733 at ONE asked field - 45
//				degrees half, the game's normal flat FOV - and said plainly
//				that this was not settled:
//
//				  "Whether d3d.ren applies a constant tangent scale, or derives
//				   the horizontal from the vertical and the viewport aspect -
//				   which would make the ratio vary - is not decided by a single
//				   point."
//
//				It matters because the VR path asks for about 70 degrees half,
//				not 45. If the ratio varies, a VRFovXTest derived at 45 is the
//				wrong number at 70, and the headset round that tested it would
//				be spent proving nothing.
//
// --------------------------------------------------------------------------- //

void CGameClientShell::SweepCameraField()
{
	if (!m_hCamera) return;

	float  fSavedFovX = 0.0f, fSavedFovY = 0.0f;
	g_pLTClient->GetCameraFOV(m_hCamera, &fSavedFovX, &fSavedFovY);

	// Half-angles, degrees. 45 is included deliberately and FIRST: it is the
	// field 0.733 was measured at, so it is the CONTROL. If this run does not
	// reproduce roughly 0.733 there, the sweep is broken and nothing else it
	// prints should be believed.
	const int   kFields = 4;
	const float fHalfDeg[kFields] = { 45.0f, 30.0f, 60.0f, 70.0f };

	float fRatio[kFields];
	float fFocal[kFields];
	int   nGood = 0;

	VRLog::Msg("");
	VRLog::Msg("=== field sweep: is the renderer's tangent ratio constant? ===");
	VRLog::Msg("  45 deg is the control - it must reproduce the 0.733 already on record");

	// Before any measurement: an image of what the instrument is looking at.
	//
	// Cheap, and it pre-empts the failure this sweep has had four times - a
	// confident number derived from a frame nobody ever saw. It also settles
	// the read itself: if the BMP shows the scene, GetPixel through the stash
	// works and any later "no texture" verdict is about the view; if it is
	// blank or banded, the correlation was never the problem.
	DumpEyeImage("sweep-start");

	// Images from inside the first calibration only. Every calibration in the
	// sweep would be 16 more files and the question is answered by the first.
	m_bDumpCalibPasses = LTTRUE;

	// A heading scan was tried here on 27 August and REMOVED. It rendered the
	// scene at eight headings and picked the one with the most band contrast,
	// so that the measurement would not depend on someone aiming well.
	//
	// It worked - it correctly found sd 63.2 at +45 against 17.1 straight
	// ahead - and it broke the calibration that followed. Every reference read
	// after it came back "mean luminance 226, sd 0.0", uniformly blank, where
	// the same reads without it returned real content. Eight extra renders and
	// surface copies immediately before the calibration leave the read in a
	// state it cannot use, and the mechanism is not understood.
	//
	// Not reinstated on a guess. The calibration has been got wrong four times
	// already by changes that looked reasonable, and every one of them produced
	// a confident number. Aim the player at something with depth and detail
	// before running this.

	for (int k = 0; k < kFields; ++k)
	{
		fRatio[k] = 0.0f;
		fFocal[k] = 0.0f;

		const float fFovX = fHalfDeg[k] * 2.0f * 0.01745329f;
		g_pLTClient->SetCameraFOV(m_hCamera, fFovX, fSavedFovY);

		VRLog::Msg("");
		VRLog::Msg("-- asked half-fovX %.0f deg --", fHalfDeg[k]);

		// The calibration reads the camera's CURRENT fov as the asked field and
		// restores it when it finishes, so setting it here is all that is
		// needed. Guarded, because it is still the function that had never run
		// at all before 23 August.
		CalibrateFovXGuarded();

		m_bDumpCalibPasses = LTFALSE;

		fRatio[k] = m_fLastFieldRatio;
		fFocal[k] = m_fLastFieldFocal;
		if (fRatio[k] > 0.0f) ++nGood;
	}

	g_pLTClient->SetCameraFOV(m_hCamera, fSavedFovX, fSavedFovY);

	// ------------------------------------------------------------------ //
	// Does the renderer even LOOK at the horizontal FOV it is given?
	//
	// This is the hypothesis the project has carried for six weeks without
	// testing, and it is decisive either way. The measured focal at 45 degrees
	// half was 982 px. A renderer honouring fovX against the eye viewport
	// would give 720; against the full surface, 1440. Neither is 982. But a
	// renderer that IGNORES fovX and derives the horizontal from the vertical
	// and the viewport aspect - the ordinary thing for a renderer of this era
	// to do - gives a focal set by fovY alone, which at the game's fovY of 78
	// degrees is about 922.
	//
	// So: hold fovX fixed and move fovY. If the HORIZONTAL focal moves with
	// it, the renderer derives X from Y, VRFovXTest is a knob on the wrong
	// quantity, and the correct fix is to set the vertical and let the
	// horizontal follow. If it does not move, fovX is honoured and the tangent
	// scale is real.
	//
	// Neither answer needs a headset, and one of them retires a variable that
	// has cost three test rounds.
	// ------------------------------------------------------------------ //
	VRLog::Msg("");
	VRLog::Msg("=== does fovY move the HORIZONTAL image? ===");
	VRLog::Msg("  fovX held at 90 deg full; if the horizontal focal tracks fovY,");
	VRLog::Msg("  the renderer derives the horizontal from the vertical.");

	const int   kVFields = 3;
	const float fVFullDeg[kVFields] = { 60.0f, 78.0f, 100.0f };
	float       fVFocal[kVFields];
	int         nVGood = 0;

	for (int v = 0; v < kVFields; ++v)
	{
		fVFocal[v] = 0.0f;
		g_pLTClient->SetCameraFOV(m_hCamera, 90.0f * 0.01745329f,
										   fVFullDeg[v] * 0.01745329f);
		VRLog::Msg("");
		VRLog::Msg("-- fovY %.0f deg full, fovX 90 deg full --", fVFullDeg[v]);
		CalibrateFovXGuarded();
		fVFocal[v] = m_fLastFieldFocal;
		if (fVFocal[v] > 0.0f) ++nVGood;
	}

	g_pLTClient->SetCameraFOV(m_hCamera, fSavedFovX, fSavedFovY);

	// Verdict on the fovY probe, printed HERE rather than with the rest.
	//
	// The sweep's own result section has four early returns in it, all of them
	// about the horizontal control. This answer does not depend on that control
	// and must not be thrown away with it - a run that fails the horizontal can
	// still settle whether fovX is honoured at all, which is the more
	// structural of the two questions.
	VRLog::Msg("");
	VRLog::Msg("--- fovY probe verdict ---");
	if (nVGood < 2)
	{
		VRLog::Msg("  only %d of %d vertical fields measured - no verdict.", nVGood, kVFields);
	}
	else
	{
		float fVMin = 0.0f, fVMax = 0.0f;
		int   nv = 0;
		for (int v = 0; v < kVFields; ++v)
		{
			if (fVFocal[v] <= 0.0f) continue;
			VRLog::Msg("  fovY %3.0f deg -> horizontal focal %.0f px", fVFullDeg[v], fVFocal[v]);
			if (nv == 0 || fVFocal[v] < fVMin) fVMin = fVFocal[v];
			if (nv == 0 || fVFocal[v] > fVMax) fVMax = fVFocal[v];
			++nv;
		}
		const float fVSpread = (fVMax > 0.0f) ? ((fVMax - fVMin) / fVMax) : 0.0f;
		VRLog::Msg("  spread %.1f percent across a %.0f-to-%.0f degree change in fovY",
			fVSpread * 100.0f, fVFullDeg[0], fVFullDeg[kVFields - 1]);

		if (fVSpread > 0.15f)
		{
			VRLog::Msg("");
			VRLog::Msg("  THE RENDERER DERIVES THE HORIZONTAL FROM THE VERTICAL.");
			VRLog::Msg("  fovX was held constant and the horizontal image moved anyway, so the");
			VRLog::Msg("  horizontal FOV we ask for is being ignored. VRFovXTest is a knob on a");
			VRLog::Msg("  quantity the renderer does not read, which is why sweeping it changed");
			VRLog::Msg("  the picture only through side effects and never fixed the geometry.");
			VRLog::Msg("  The fix is to set the VERTICAL correctly and let the horizontal follow");
			VRLog::Msg("  from the viewport aspect, which is what the eye buffer's shape already");
			VRLog::Msg("  encodes.");
			g_pLTClient->CPrint("fovY probe: renderer derives horizontal from vertical");
		}
		else
		{
			VRLog::Msg("");
			VRLog::Msg("  fovX IS honoured: moving fovY by %.0f degrees left the horizontal focal",
				fVFullDeg[kVFields - 1] - fVFullDeg[0]);
			VRLog::Msg("  within %.1f percent. The two axes are set independently, so the tangent",
				fVSpread * 100.0f);
			VRLog::Msg("  scale on each is real and each needs its own correction.");
			g_pLTClient->CPrint("fovY probe: fovX is honoured");
		}
	}

	// ------------------------------------------------------------------ //
	// The VERTICAL field, which has never been measured at all.
	// ------------------------------------------------------------------ //
	VRLog::Msg("");
	VRLog::Msg("=== the VERTICAL field, at the game's own FOV ===");
	CalibrateFovYGuarded();
	const float fVertRatio = m_fLastFieldRatio;
	const float fVertFocal = m_fLastFieldFocal;

	g_pLTClient->SetCameraFOV(m_hCamera, fSavedFovX, fSavedFovY);

	// The anisotropy, which is the number this whole sweep exists to produce.
	//
	// A projection is only rigid if both axes are magnified by the same
	// factor. Horizontal and vertical ratios that differ mean the picture
	// handed to the runtime is stretched on one axis, and because that stretch
	// is locked to the head rather than to the world, head rotation drags
	// world features through it. That is the observed bending and warping, and
	// no horizontal-only knob can reach it.
	VRLog::Msg("");
	VRLog::Msg("--- anisotropy: the warping number ---");
	if (fRatio[0] <= 0.0f || fVertRatio <= 0.0f)
	{
		VRLog::Msg("  cannot compute: horizontal ratio %.3f, vertical ratio %.3f",
			fRatio[0], fVertRatio);
		VRLog::Msg("  (a ratio of 0 means that axis produced no usable lock this run)");
	}
	else
	{
		// What the eye actually sees against what the host declares, per axis,
		// at the settings this build ships with.
		const float fSeenX = 2.0f * g_vtVRFovXTest.GetFloat() * fRatio[0];
		const float fSeenY = g_vtVRFovYScale.GetFloat() * fVertRatio;
		const float fAniso = (fSeenY > 0.0f) ? (fSeenX / fSeenY) : 0.0f;

		VRLog::Msg("  horizontal focal %.0f px, tan ratio %.3f", fFocal[0], fRatio[0]);
		VRLog::Msg("  vertical   focal %.0f px, tan ratio %.3f", fVertFocal, fVertRatio);
		VRLog::Msg("  at VRFovXTest %.3f and VRFovYScale %.3f the eye sees %+.0f%% of the",
			g_vtVRFovXTest.GetFloat(), g_vtVRFovYScale.GetFloat(),
			100.0f * (fSeenX - 1.0f));
		VRLog::Msg("  declared HORIZONTAL tangent and %+.0f%% of the declared VERTICAL,",
			100.0f * (fSeenY - 1.0f));
		VRLog::Msg("  an anisotropy of %.3f (1.000 is a rigid projection).", fAniso);

		if (fAniso > 1.02f || fAniso < 0.98f)
		{
			VRLog::Msg("");
			VRLog::Msg("  THE PROJECTION IS NOT RIGID. The world is stretched by %.0f%% on one",
				100.0f * ((fAniso > 1.0f ? fAniso : 1.0f / fAniso) - 1.0f));
			VRLog::Msg("  axis relative to the other, and that stretch follows the head. This is");
			VRLog::Msg("  the bending, and it is arithmetic rather than a matter of taste.");
			VRLog::Msg("  Set VRFovXTest %.3f and VRFovYScale %.3f to remove it.",
				1.0f / (2.0f * fRatio[0]), 1.0f / fVertRatio);
			g_pLTClient->CPrint("anisotropy %.3f - set VRFovXTest %.3f, VRFovYScale %.3f",
				fAniso, 1.0f / (2.0f * fRatio[0]), 1.0f / fVertRatio);
		}
		else
		{
			VRLog::Msg("  The projection is rigid to within 2 percent. The warping is NOT an");
			VRLog::Msg("  axis-scale error, and the next suspect is elsewhere.");
			g_pLTClient->CPrint("anisotropy %.3f - projection is rigid", fAniso);
		}
	}

	VRLog::Msg("");
	VRLog::Msg("=== sweep result: is the horizontal tangent ratio constant? ===");

	if (fRatio[0] <= 0.0f)
	{
		QuitAfterSweepIfAsked();
		VRLog::Msg("  THE CONTROL FAILED: no usable measurement at 45 deg, where one is known");
		VRLog::Msg("  to be obtainable. Aim at a scene with depth and varied detail and retry.");
		VRLog::Msg("  Nothing else in this sweep means anything.");
		g_pLTClient->CPrint("field sweep: control failed, no verdict");
		return;
	}

	if (fRatio[0] < 0.66f || fRatio[0] > 0.81f)
	{
		QuitAfterSweepIfAsked();
		VRLog::Msg("  THE CONTROL DISAGREES: %.3f at 45 deg against the 0.733 on record.", fRatio[0]);
		VRLog::Msg("  Either the scene is unsuitable or this sweep is wrong. No verdict.");
		g_pLTClient->CPrint("field sweep: control disagrees, no verdict");
		return;
	}

	VRLog::Msg("  control at 45 deg: %.3f - agrees with the recorded 0.733", fRatio[0]);

	if (nGood < 3)
	{
		QuitAfterSweepIfAsked();
		VRLog::Msg("  only %d of %d fields measured. Not enough to call it.", nGood, kFields);
		g_pLTClient->CPrint("field sweep: too few fields, no verdict");
		return;
	}

	float fMin = 0.0f, fMax = 0.0f, fSum = 0.0f;
	int   n = 0;
	for (int k = 0; k < kFields; ++k)
	{
		if (fRatio[k] <= 0.0f) continue;
		VRLog::Msg("  half-fovX %2.0f deg -> tan ratio %.3f", fHalfDeg[k], fRatio[k]);
		if (n == 0 || fRatio[k] < fMin) fMin = fRatio[k];
		if (n == 0 || fRatio[k] > fMax) fMax = fRatio[k];
		fSum += fRatio[k];
		++n;
	}

	const float fMean   = fSum / (float)n;
	const float fSpread = (fMean > 0.0f) ? ((fMax - fMin) / fMean) : 0.0f;

	VRLog::Msg("  mean %.3f, range %.3f to %.3f, spread %.1f percent of the mean",
		fMean, fMin, fMax, fSpread * 100.0f);

	// 10 percent is chosen against the instrument, not against taste: the
	// single-field run that produced 0.733 had a 0.8 percent spread across its
	// four yaws, and the run before it was thrown out at 21.4 percent. A ratio
	// that moves by more than 10 percent across the asked field is moving by far
	// more than the measurement's own noise.
	if (fSpread <= 0.10f)
	{
		VRLog::Msg("");
		VRLog::Msg("  CONSTANT. The renderer applies a fixed tangent scale of about %.3f,", fMean);
		VRLog::Msg("  independent of the asked field. So a correction derived at 45 deg is");
		VRLog::Msg("  valid at the 70 deg the VR path asks for, and VRFovXTest %.3f holds.",
			1.0f / (2.0f * fMean));
	}
	else
	{
		VRLog::Msg("");
		VRLog::Msg("  NOT CONSTANT - the ratio varies with the asked field by %.1f percent.",
			fSpread * 100.0f);
		VRLog::Msg("  A single VRFovXTest cannot be right at every field, and the 0.682");
		VRLog::Msg("  derived at 45 deg is NOT the right number for the VR path's 70 deg.");
		VRLog::Msg("  The correction has to be a function of the asked field, fitted to");
		VRLog::Msg("  the table above, rather than one constant.");
	}

	g_pLTClient->CPrint("field sweep: done, see the client log");

	// Quit the moment the measurement is finished, if asked to.
	//
	// VRQuitAfter is a wall-clock timer set before the run, so it has to be
	// guessed generously - and every second of that guess is a game window
	// sitting on the player's screen while the player is trying to work. The sweep
	// knows exactly when it is done; nothing else has to.
	//
	// Set on its own variable rather than folded into VRQuitAfter, because a
	// timer that sometimes means "after N seconds" and sometimes means "when
	// some other thing finishes" is the kind of overloaded knob this project
	// has already been bitten by.
	QuitAfterSweepIfAsked();
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::QuitAfterSweepIfAsked()
//
//	PURPOSE:	End the run the moment the sweep has written its verdict.
//
//				A function rather than a line at the end of the sweep, because
//				the sweep has four early returns in it and the first version
//				of this sat below all of them - so the one case that actually
//				happened, the control failing, left the game window on screen
//				for the whole VRQuitAfter margin. Which is the interruption
//				this was added to remove.
//
// --------------------------------------------------------------------------- //

void CGameClientShell::QuitAfterSweepIfAsked()
{
	if (g_vtVRQuitAfterSweep.GetFloat() <= 0.0f) return;

	VRLog::Msg("VRQuitAfterSweep - measurement complete, shutting down cleanly");
	g_vtVRQuitAfterSweep.SetFloat(0.0f);
	g_pLTClient->Shutdown();
}

void CGameClientShell::CalibrateFovX()
{
	CalibrateAxis(LTFALSE);
}

void CGameClientShell::CalibrateFovY()
{
	CalibrateAxis(LTTRUE);
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::CalibrateAxis()
//
//	PURPOSE:	Measure the field d3d.ren ACTUALLY produces on one axis, by
//				rotating a known angle and finding how far the image moved.
//
//				Horizontal (bVertical FALSE) yaws and correlates along rows.
//				Vertical pitches and correlates along columns. The vertical has
//				never been measured, and it is the prime suspect for the
//				warping: if the two axes are magnified by different factors the
//				runtime is handed a picture that is stretched on one axis, and
//				head rotation sweeps world features through that anisotropy.
//				That is the observed bending, and it is exactly the
//				failure a horizontal-only knob like VRFovXTest cannot touch -
//				which is why sweeping it only ever changed the squish.
//
// --------------------------------------------------------------------------- //

void CGameClientShell::CalibrateAxis(LTBOOL bVertical)
{
	// Cleared first, so every early return below leaves them at 0 and the
	// sweep cannot read a stale result from a previous call as though it were
	// this one's. There are six ways out of this function before a number
	// exists.
	m_fLastFieldRatio = 0.0f;
	m_fLastFieldFocal = 0.0f;

	const char* const pszAxis = bVertical ? "VERTICAL" : "horizontal";

	HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	if (!hScreen || !m_hCamera) return;

	uint32 nSurfW = 0, nSurfH = 0;
	g_pLTClient->GetSurfaceDims(hScreen, &nSurfW, &nSurfH);
	if (nSurfW < 64 || nSurfH < 64) return;

	const int nHalfW = (int)nSurfW / 2;
	const int nViewH = (int)nSurfH;

	// The axis being measured, and the axis the sample lines run across.
	const int nAlong  = bVertical ? nViewH : nHalfW;	// samples run along this
	const int nAcross = bVertical ? nHalfW : nViewH;	// lines are spread across this

	// A narrow band at the viewport CENTRE, at full pixel resolution, on
	// several lines.
	//
	// A rotation does not translate the image, it reprojects it: for a 5 degree
	// yaw the displacement runs from 63 px at the centre to 134 px at the edge.
	// No single shift fits that, which is why correlating the full width found
	// nothing at all.
	const int   kBand     = 128;
	const int   kSamples  = kBand * 2;
	const int   kRows     = 3;
	const int   kMaxShift = 110;
	static int  nRef[kRows][kSamples];
	static int  nCmp[kRows][kSamples];

	// SEVERAL SMALL ROTATIONS, not one big one.
	//
	// A 5 degree yaw put the honest answer at 63 px and the rival at 170 -
	// which is exactly where a 256 px band runs out of overlap. The 23 August
	// run duly "locked" at +170 with the correlation still climbing, i.e. on
	// the wall of the search rather than on a feature.
	//
	// Small angles keep every candidate comfortably inside the range, and
	// sweeping several turns one number into a LINE: shift must be
	// proportional to tan(angle). If it is not, this is not measuring a
	// projection and no focal length should be reported. That check is worth
	// more than any single reading.
	const int   kYaws = 4;
	const float fYawDeg[kYaws] = { 1.0f, 2.0f, 3.0f, 4.0f };

	LTVector   vSavedPos;
	LTRotation rSavedRot;
	float      fSavedFovX = 0.0f, fSavedFovY = 0.0f;
	LTBOOL     bSavedFull = LTFALSE;
	int        nL = 0, nT = 0, nR = 0, nB = 0;

	g_pLTClient->GetObjectPos(m_hCamera, &vSavedPos);
	g_pLTClient->GetObjectRotation(m_hCamera, &rSavedRot);
	g_pLTClient->GetCameraFOV(m_hCamera, &fSavedFovX, &fSavedFovY);
	g_pLTClient->GetCameraRect(m_hCamera, &bSavedFull, &nL, &nT, &nR, &nB);

	g_pLTClient->SetCameraRect(m_hCamera, LTFALSE, 0, 0, nHalfW, nViewH);

	// HIDE THE PLAYER'S WEAPON. This is the bug that made every previous run
	// of this measurement lie.
	//
	// The view model is attached to the camera, so it has ZERO parallax: yaw
	// the camera and the gun occupies exactly the same pixels. It is also the
	// highest-contrast object in a typical frame - dark metal against a lit
	// wall or a sand floor - so any line-selection rule that looks for
	// contrast walks straight into it, and so did the original fixed rows at
	// 35/50/65 percent of the height.
	//
	// The correlation then matches the gun against itself and reports a
	// perfect lock at zero shift. That is why the log has been full of
	// "shift +0 px, correlation 1.000" and why the verdict has read "aim at a
	// scene with depth and varied detail" while pointed at a courtyard full
	// of it. The scene was never the problem.
	//
	// Restored on every exit path below, including the early ones.
	m_weaponModel.SetVisible(LTFALSE);

	HSURFACE hStash = GetEyeStashSurface(nHalfW, nViewH);
	if (!hStash)
	{
		m_weaponModel.SetVisible(LTTRUE);
		g_pLTClient->SetCameraRect(m_hCamera, bSavedFull, nL, nT, nR, nB);
		VRLog::Msg("fov calibration: no stash surface - cannot read the frame back");
		return;
	}

	const int nCentreAlong = nAlong / 2;
	LTRect    rHalf;
	rHalf.left = 0; rHalf.top = 0; rHalf.right = nHalfW; rHalf.bottom = nViewH;

	// Which lines to sample, chosen from the picture rather than fixed at
	// 35/50/65 percent of the height.
	//
	// Every run of this measurement since 23 August has died on "reference
	// band: sd 0.0 - NO TEXTURE", because the quick save happens to leave the
	// player facing a blank wall at those three rows. The instrument was fine;
	// it was looking at the only part of the frame with nothing in it.
	//
	// A heading scan was tried for this on 27 August and REMOVED: it rendered
	// the scene eight times to find a good direction, and eight extra renders
	// and surface copies immediately before the calibration left every
	// subsequent read blank. Choosing LINES costs no extra render at all - it
	// reads different pixels out of the reference frame that has already been
	// copied - so it cannot reproduce that failure.
	const int  kCand = 24;
	int        nLine[kRows];
	for (int i = 0; i < kRows; ++i) nLine[i] = nAcross * (35 + i * 15) / 100;

	int    nShiftFor[kYaws];
	double fCorrFor[kYaws];
	int    nBoundaryFor[kYaws];

	for (int k = 0; k < kYaws; ++k) { nShiftFor[k] = 0; fCorrFor[k] = -2.0; nBoundaryFor[k] = 0; }

	// A warm-up render, discarded.
	//
	// The camera rect and FOV have just changed, and reads taken immediately
	// after such a change have come back uniformly blank ("mean 226, sd 0.0")
	// in a way that looked like an absent scene and was not. One throwaway
	// render costs a millisecond and removes that whole class of wrong answer.
	g_pLTClient->SetObjectRotation(m_hCamera, &rSavedRot);
	g_pLTClient->Start3D();
	g_pLTClient->RenderCamera(m_hCamera);
	g_pLTClient->End3D();

	for (int pass = 0; pass <= kYaws; ++pass)
	{
		LTRotation rUse = rSavedRot;
		if (pass > 0)
		{
			const float fRad = fYawDeg[pass - 1] * 0.01745329f;
			if (bVertical) g_pLTClient->EulerRotateX(&rUse, fRad);
			else           g_pLTClient->EulerRotateY(&rUse, fRad);
		}
		g_pLTClient->SetObjectRotation(m_hCamera, &rUse);

		g_pLTClient->Start3D();
		g_pLTClient->RenderCamera(m_hCamera);
		g_pLTClient->End3D();

		// GetPixel on the screen surface returns black on D3D7 - measured, 512
		// samples, twice. Surface-to-surface copy works, so read the copy.
		g_pLTClient->Start3D();
		g_pLTClient->StartOptimized2D();
		g_pLTClient->DrawSurfaceToSurface(hStash, hScreen, &rHalf, 0, 0);
		g_pLTClient->EndOptimized2D();
		g_pLTClient->End3D();

		// The reference and the widest rotation, as images.
		//
		// The correlation reported 1.000 at zero shift for a 1 and 2 degree
		// yaw, which is not a weak match - it is the same bytes twice. Either
		// the camera rotation is not reaching the render, or the read is not
		// returning this render. Those need opposite fixes and no amount of
		// correlation output separates them. Two BMPs do, immediately.
		if (m_bDumpCalibPasses && (pass == 0 || pass == kYaws))
		{
			char szTag[64];
			sprintf(szTag, "%s-pass%d", bVertical ? "v" : "h", pass);
			DumpStashImage(szTag, hStash, nHalfW, nViewH);
		}

		// On the reference pass, pick the lines with the most structure. The
		// correlation can only find a shift where the picture varies, so this
		// is choosing the instrument's own working range, not tuning a result:
		// it happens before any rotation, and the SAME lines are then used for
		// every comparison pass.
		if (pass == 0)
		{
			double fBestSd = -1.0, fSumSd = 0.0, fWorstSd = 1e9;
			int    nBestOf[kCand];
			double fSdOf[kCand];

			for (int c = 0; c < kCand; ++c)
			{
				const int nAt = nAcross * (12 + (c * 76) / (kCand - 1)) / 100;
				double fMean = 0.0, fVar = 0.0;
				int    nGot = 0;

				for (int i = 0; i < kSamples; ++i)
				{
					const int nPos = nCentreAlong - kBand + i;
					if (nPos < 0 || nPos >= nAlong) continue;

					const uint32 x = (uint32)(bVertical ? nAt  : nPos);
					const uint32 y = (uint32)(bVertical ? nPos : nAt);
					HLTCOLOR c2 = 0;
					g_pLTClient->GetPixel(hStash, x, y, &c2);

					const int r = (int)((c2 >> 16) & 0xFF);
					const int g = (int)((c2 >>  8) & 0xFF);
					const int b = (int)( c2        & 0xFF);
					const int nLum = (r * 77 + g * 151 + b * 28) >> 8;
					fMean += nLum;
					fVar  += (double)nLum * nLum;
					++nGot;
				}

				if (nGot < 8) { fSdOf[c] = -1.0; nBestOf[c] = nAt; continue; }
				fMean /= nGot;
				const double fSd = sqrt((fVar / nGot) - fMean * fMean > 0.0
										? (fVar / nGot) - fMean * fMean : 0.0);
				fSdOf[c]   = fSd;
				nBestOf[c] = nAt;
				fSumSd    += fSd;
				if (fSd > fBestSd)  fBestSd  = fSd;
				if (fSd < fWorstSd) fWorstSd = fSd;
			}

			// Take the kRows candidates with the highest contrast, spread out
			// rather than adjacent: adjacent lines see almost the same
			// features, so three of them is one measurement counted three
			// times rather than three measurements.
			for (int slot = 0; slot < kRows; ++slot)
			{
				int    nPick = -1;
				double fPick = -1.0;
				for (int c = 0; c < kCand; ++c)
				{
					if (fSdOf[c] < 0.0) continue;
					int bTooClose = 0;
					for (int s2 = 0; s2 < slot; ++s2)
					{
						const int d = nBestOf[c] - nLine[s2];
						if ((d < 0 ? -d : d) < nAcross / 12) bTooClose = 1;
					}
					if (bTooClose) continue;
					if (fSdOf[c] > fPick) { fPick = fSdOf[c]; nPick = c; }
				}
				if (nPick >= 0) { nLine[slot] = nBestOf[nPick]; fSdOf[nPick] = -1.0; }
			}

			VRLog::Msg("  reference: best %s of %d candidate lines has sd %.1f, mean sd %.1f, worst %.1f",
				bVertical ? "column" : "row", kCand, fBestSd, fSumSd / kCand, fWorstSd);
			VRLog::Msg("  sampling %s %d, %d, %d",
				bVertical ? "columns" : "rows", nLine[0], nLine[1], nLine[2]);

			if (fBestSd < 4.0)
			{
				// Distinguishes two states that have looked identical in every
				// previous run. If the BEST of 24 lines across the frame is
				// flat, the frame itself carries nothing - which is a broken
				// read far more often than it is a real blank wall.
				VRLog::Msg("  NOTHING IN THE FRAME: no line anywhere has contrast.");
				VRLog::Msg("  That is a failed surface read, not a badly aimed camera -");
				VRLog::Msg("  a real view of a wall still varies more than this.");
			}
		}

		// Read the chosen lines for this pass.
		for (int row = 0; row < kRows; ++row)
		{
			for (int i = 0; i < kSamples; ++i)
			{
				const int nPos = nCentreAlong - kBand + i;
				HLTCOLOR c = 0;
				if (nPos >= 0 && nPos < nAlong)
				{
					const uint32 x = (uint32)(bVertical ? nLine[row] : nPos);
					const uint32 y = (uint32)(bVertical ? nPos       : nLine[row]);
					g_pLTClient->GetPixel(hStash, x, y, &c);
				}

				const int r = (int)((c >> 16) & 0xFF);
				const int g = (int)((c >>  8) & 0xFF);
				const int b = (int)( c        & 0xFF);
				const int nLum = (r * 77 + g * 151 + b * 28) >> 8;

				if (pass == 0) nRef[row][i] = nLum;
				else           nCmp[row][i] = nLum;
			}
		}

		if (pass == 0) continue;

		// Normalised cross-correlation. Absolute difference fails here: a few
		// degrees of yaw moved the mean luminance from 97 to 111 in a measured
		// run, which swamped the structure and flattened the entire curve. NCC
		// divides brightness and contrast out, and gives a number with an
		// absolute meaning so the function can say it does not know.
		int    nBest = 0;
		double fBest = -2.0;

		for (int shift = -kMaxShift; shift <= kMaxShift; ++shift)
		{
			double fSA = 0.0, fSB = 0.0, fSAA = 0.0, fSBB = 0.0, fSAB = 0.0;
			int    nCount = 0;

			for (int row = 0; row < kRows; ++row)
			{
				for (int i = 0; i < kSamples; ++i)
				{
					const int j = i + shift;
					if (j < 0 || j >= kSamples) continue;
					const double a = (double)nRef[row][j];
					const double b = (double)nCmp[row][i];
					fSA += a; fSB += b; fSAA += a * a; fSBB += b * b; fSAB += a * b;
					++nCount;
				}
			}
			if (nCount < kSamples) continue;

			const double n  = (double)nCount;
			const double mA = fSA / n, mB = fSB / n;
			const double vA = fSAA / n - mA * mA;
			const double vB = fSBB / n - mB * mB;
			const double cv = fSAB / n - mA * mB;
			const double den = sqrt((vA > 0.0 ? vA : 0.0) * (vB > 0.0 ? vB : 0.0));
			const double ncc = (den > 1e-6) ? (cv / den) : -2.0;

			if (ncc > fBest) { fBest = ncc; nBest = shift; }
		}

		nShiftFor[pass - 1] = nBest;
		fCorrFor[pass - 1]  = fBest;

		// A peak at zero shift is not a measurement of the world.
		//
		// The camera has demonstrably rotated - the rendered images differ in
		// 83 percent of their bytes - so anything that did NOT move is
		// attached to the camera rather than standing in the world. With the
		// weapon hidden this should no longer fire; it stays as the guard that
		// would have caught the weapon in the first place, and will catch the
		// next piece of camera-locked geometry someone adds.
		if (nBest == 0)
		{
			fCorrFor[pass - 1] = -2.0;
			VRLog::Msg("  rot %.0f deg: peak at ZERO shift - correlating something "
					   "attached to the camera, not the world", fYawDeg[pass - 1]);
		}

		// A peak on the edge of the search is not a peak. This is exactly how
		// +170 was reported as a confident 0.93 lock.
		nBoundaryFor[pass - 1] = ((nBest <= -kMaxShift + 1) || (nBest >= kMaxShift - 1)) ? 1 : 0;
	}

	// Restore before any arithmetic, so an early return cannot leave the camera
	// modified (a project rule).
	m_weaponModel.SetVisible(LTTRUE);
	g_pLTClient->SetObjectRotation(m_hCamera, &rSavedRot);
	g_pLTClient->SetObjectPos(m_hCamera, &vSavedPos);
	g_pLTClient->SetCameraFOV(m_hCamera, fSavedFovX, fSavedFovY);
	g_pLTClient->SetCameraRect(m_hCamera, bSavedFull, nL, nT, nR, nB);

	const float fHalfAsked     = (bVertical ? fSavedFovY : fSavedFovX) * 0.5f;
	const float fFocalIfHonest = (float)(nAlong / 2) / (float)tan(fHalfAsked);

	VRLog::Msg("fov calibration [%s]: asked half-fov %.2f deg into a %d px viewport axis",
		pszAxis, fHalfAsked * 57.2957795f, nAlong);
	VRLog::Msg("  a renderer honouring that has focal %.0f px, so shift = focal * tan(angle)",
		fFocalIfHonest);

	int    nGood = 0;
	double fFocalSum = 0.0;

	for (int k = 0; k < kYaws; ++k)
	{
		const double fTan    = tan(fYawDeg[k] * 0.01745329);
		const int    nExpect = (int)(fFocalIfHonest * fTan + 0.5);
		const int    nAbs    = (nShiftFor[k] < 0) ? -nShiftFor[k] : nShiftFor[k];

		VRLog::Msg("  rot %.0f deg: shift %+4d px (a faithful renderer gives %+d), correlation %.3f%s%s",
			fYawDeg[k], nShiftFor[k], nExpect, fCorrFor[k],
			nBoundaryFor[k] ? "   <- ON THE SEARCH BOUNDARY, not a peak" : "",
			(!nBoundaryFor[k] && fCorrFor[k] <= 0.5) ? "   <- too weak to be a match" : "");

		if (fCorrFor[k] > 0.5 && !nBoundaryFor[k] && nAbs > 2)
		{
			++nGood;
			fFocalSum += (nAbs / fTan);
		}
	}

	if (nGood < 2)
	{
		VRLog::Msg("  NO USABLE MEASUREMENT - fewer than two rotations produced an interior peak.");
		g_pLTClient->CPrint("fov calibration [%s]: no lock", pszAxis);
		return;
	}

	// The check that makes this trustworthy: every rotation must imply the SAME
	// focal length. Scatter means the correlation is matching something other
	// than the same features moving, and the number is worthless.
	const double fFocalMean = fFocalSum / nGood;
	double fSpread = 0.0;
	for (int k = 0; k < kYaws; ++k)
	{
		const double fTan = tan(fYawDeg[k] * 0.01745329);
		const int    nAbs = (nShiftFor[k] < 0) ? -nShiftFor[k] : nShiftFor[k];
		if (fCorrFor[k] > 0.5 && !nBoundaryFor[k] && nAbs > 2)
		{
			const double d = (nAbs / fTan) - fFocalMean;
			fSpread += d * d;
		}
	}
	fSpread = sqrt(fSpread / nGood);

	VRLog::Msg("  focal from %d rotations: %.0f px, spread %.0f px (%.1f percent)",
		nGood, fFocalMean, fSpread, 100.0 * fSpread / (fFocalMean > 0.0 ? fFocalMean : 1.0));

	if (fSpread > fFocalMean * 0.15)
	{
		VRLog::Msg("  INCONSISTENT - the rotations disagree by more than 15 percent, so this is not a projection.");
		VRLog::Msg("  Not reporting a field from it.");
		return;
	}

	const float fHalfMeas = (float)atan((double)(nAlong / 2) / fFocalMean);
	const float fRatio    = (fHalfAsked > 0.0f)
		? (float)(tan(fHalfMeas) / tan(fHalfAsked)) : 0.0f;

	VRLog::Msg("  renderer produced half-fov %.2f deg against %.2f asked (tan ratio %.3f) [%s]",
		fHalfMeas * 57.2957795f, fHalfAsked * 57.2957795f, fRatio, pszAxis);

	m_fLastFieldRatio = fRatio;
	m_fLastFieldFocal = (float)fFocalMean;

	if (fRatio <= 0.25f || fRatio >= 4.0f)
	{
		VRLog::Msg("  tan ratio %.3f is not physical - rejected.", fRatio);
		m_fLastFieldRatio = 0.0f;
		return;
	}

	if (bVertical)
	{
		// The VR path asks for the vertical field with NO correction factor at
		// all - fFovY = 2*atan(fTanY), straight through. So whatever the
		// renderer does to it lands on the eye unopposed, and the knob that
		// would cancel it has to inflate the TANGENT by 1/ratio.
		const float fWant = 1.0f / fRatio;
		VRLog::Msg("  the VR path requests the vertical tangent unmodified, and the renderer draws %.3f of it",
			fRatio);
		VRLog::Msg("  -> VRFovYScale should be %.3f (currently %.2f); at the current value the eye",
			fWant, g_vtVRFovYScale.GetFloat());
		VRLog::Msg("     sees %+.0f%% of the vertical tangent we declare to the runtime",
			100.0f * (g_vtVRFovYScale.GetFloat() * fRatio - 1.0f));
		g_pLTClient->CPrint("VRFovYScale should be %.3f", fWant);
	}
	else
	{
		// The client asks for   T_req = T_declared * (surfaceW/viewportW) * VRFovXTest
		// and the renderer draws T_prod = fRatio * T_req.
		// For the eye to see what we declare to the runtime, T_prod == T_declared:
		//
		//     VRFovXTest = 1 / ((surfaceW/viewportW) * fRatio)
		//
		// An earlier version reported VRFovXTest/fRatio, which forgets that the
		// surface/viewport factor is ALREADY in the request. It recommended
		// 1.091 where the answer is 0.682 - a value that would have made the
		// error worse than leaving it alone.
		const float fClientFactor = (float)nSurfW / (float)nHalfW;
		const float fWant = 1.0f / (fClientFactor * fRatio);

		VRLog::Msg("  the client requests T * %.1f * VRFovXTest, and the renderer draws %.3f of it",
			fClientFactor, fRatio);
		VRLog::Msg("  -> VRFovXTest should be %.3f (currently %.2f); at the current value the eye",
			fWant, g_vtVRFovXTest.GetFloat());
		VRLog::Msg("     sees %+.0f%% of the tangent we declare to the runtime",
			100.0f * (fClientFactor * g_vtVRFovXTest.GetFloat() * fRatio - 1.0f));
		g_pLTClient->CPrint("VRFovXTest should be %.3f", fWant);
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	ForwardNdcForEye()
//
//	PURPOSE:	Where the forward ray lands in one eye's image.
//
//				The centre of the image is the forward direction only when the
//				frustum is symmetric. A headset's is not: the Quest's optical
//				axis sits about 7 degrees inward and 5.5 down, so the eye sees
//				L54 R40 and U44 D55 and "ahead" is off centre.
//
//				For a frustum spanning tanL..tanR, ndc maps tanL to -1 and tanR
//				to +1, so a ray at tan 0 lands at -(tanL+tanR)/(tanR-tanL).
//
//				Leaves the outputs untouched and returns false if the four
//				angles are not a usable frustum - a host that never wrote them
//				leaves all four zero, and dividing by that would put the
//				crosshair nowhere at all, which is the hardest symptom in this
//				project to attribute.
//
// ----------------------------------------------------------------------- //

// -1 so the first call always logs, whatever the state turns out to be.
static int s_nCrosshairLoggedNative = -1;

static LTBOOL ForwardNdcForEye(const VRSharedState& s, int nEye,
							   float* pNdcX, float* pNdcY)
{
	if (nEye < 0 || nEye > 1) return LTFALSE;

	const float tL = (float)tan(s.fEyeFovLeftRad[nEye]);
	const float tR = (float)tan(s.fEyeFovRightRad[nEye]);
	const float tU = (float)tan(s.fEyeFovUpRad[nEye]);
	const float tD = (float)tan(s.fEyeFovDownRad[nEye]);

	if (!(tR > tL) || !(tU > tD)) return LTFALSE;

	*pNdcX = -(tL + tR) / (tR - tL);
	*pNdcY = -(tD + tU) / (tU - tD);
	return LTTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::DrawVRCrosshair()
//
//	PURPOSE:	A crosshair in EACH eye.
//
//				The game's own crosshair is drawn once across the whole window,
//				so in side-by-side stereo it lands on the seam and each eye
//				gets half of it - which is why there appears to be no crosshair
//				at all. Anything the player must aim with has to be drawn twice,
//				once per half, at the centre of that half.
//
//				Centre of the eye, not the weapon's muzzle direction. That is
//				correct while the view and the weapon share an axis, which is
//				the case until motion controls drive the gun independently -
//				at which point this needs the aim ray from VRShared::Hands.
//
// ----------------------------------------------------------------------- //

void CGameClientShell::DrawVRCrosshair(int nHalfWidth, int nHeight)
{
	if (g_vtVRCrosshair.GetFloat() <= 0.0f || !m_hMarkerOn) return;
	// TWO CROSSHAIRS IS WORSE THAN THE WRONG ONE. When the world-space aim
	// marker is up it is the truth - it sits on the gun's own ray - and
	// this one would sit somewhere else entirely and be believed.
	if (m_bVRAimMarkerOn) return;

	HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	if (!hScreen) return;

	LTRect rBlock;
	rBlock.left = 0; rBlock.top = 0;
	rBlock.right = kMarkerBlock; rBlock.bottom = kMarkerBlock;

	// Four ticks around a gap. The first version listed offsets by hand and got
	// them wrong - three blocks horizontally, two vertically, and nothing at
	// +2 arms - which drew a lopsided shape rather than a crosshair.
	//
	// VRCrosshair is also the size: 1 draws single blocks, 2 doubles the reach,
	// and so on, so it can be sized without a rebuild.
	const int nScale = (int)g_vtVRCrosshair.GetFloat();
	const int kArm   = kMarkerBlock * ((nScale > 0) ? nScale : 1);
	const int nOff[5][2] = { { -kArm, 0 }, { kArm, 0 }, { 0, -kArm }, { 0, kArm }, { 0, 0 } };

	// Which experiment arm is running, visible without reading anything: white
	// four-tick crosshair on the normal path, GREEN crosshair with a filled
	// centre while the head is going through the mouse path.
	//
	// A whole run was already judged and reported on the assumption that F1 had
	// been pressed when the log shows it had not. Colour AND shape both change,
	// so this survives whatever the headset does to colour.
	const bool bAlt = (g_vtVRHeadAsMouse.GetFloat() > 0.0f) && m_hMarkerAlt;
	HSURFACE   hTick = bAlt ? m_hMarkerAlt : m_hMarkerOn;
	const int  nTicks = bAlt ? 5 : 4;

	// Only offset when the RENDERER says it used the native frustum. That
	// flag is written by the renderer about what it did, not read from a
	// console variable on this side, so the two cannot disagree.
	const VRSharedState& s = VRShared::State();
	const LTBOOL bNative = (VRShared::IsLive() && s.nNativeFrustum != 0)
						 ? LTTRUE : LTFALSE;
	const LTBOOL bSwapEyes = (g_vtVRSwapEyes.GetFloat() > 0.0f) ? LTTRUE : LTFALSE;

	g_pLTClient->Start3D();
	g_pLTClient->StartOptimized2D();
	for (int half = 0; half < 2; ++half)
	{
		// Pass 1 renders the eye destined for the RIGHT half and pass 2 the
		// LEFT, so without a swap the left half holds eye 0. That is this
		// file's own composition order, not something read from elsewhere.
		const int nEye = bSwapEyes ? (1 - half) : half;

		float fNdcX = 0.0f, fNdcY = 0.0f;
		if (bNative) ForwardNdcForEye(s, nEye, &fNdcX, &fNdcY);

		const int px = (int)((fNdcX * 0.5f + 0.5f) * (float)nHalfWidth);
		const int py = (int)((0.5f - fNdcY * 0.5f) * (float)nHeight);

		const int cx = half * nHalfWidth + px - kMarkerBlock / 2;
		const int cy = py - kMarkerBlock / 2;

		// On CHANGE, not once. The renderer publishes nNativeFrustum when it
		// draws its first world scene, which is after the crosshair's first
		// call, so a one-shot log records the state before the thing it is
		// meant to report has happened.
		if (s_nCrosshairLoggedNative != (int)bNative)
		{
			VRLog::Msg("  crosshair: half %d = eye %d, native %d, ndc %+.4f %+.4f"
					   " -> %d,%d in a %dx%d half",
					   half, nEye, (int)bNative, fNdcX, fNdcY, px, py,
					   nHalfWidth, nHeight);
			if (half == 1) s_nCrosshairLoggedNative = (int)bNative;
		}
		for (int i = 0; i < nTicks; ++i)
		{
			g_pLTClient->DrawSurfaceToSurface(hScreen, hTick, &rBlock,
				cx + nOff[i][0], cy + nOff[i][1]);
		}
	}
	g_pLTClient->EndOptimized2D();
	g_pLTClient->End3D();
}

void CGameClientShell::DrawFrameMarker(uint32 nHostFrame)
{
	HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	if (!hScreen) return;
	// The same number, in memory, for a host that reads the block instead
	// of the pixels. See VRShared.h, v16.
	VRShared::PublishMarker(nHostFrame);

	// Two 8x8 swatches, made once. There is no fill-rectangle in the client
	// SDK - only GetPixel/SetPixel - so they are painted a pixel at a time at
	// creation and blitted thereafter.
	if (!m_hMarkerOn || !m_hMarkerOff)
	{
		m_hMarkerOn  = g_pLTClient->CreateSurface(kMarkerBlock, kMarkerBlock);
		m_hMarkerOff = g_pLTClient->CreateSurface(kMarkerBlock, kMarkerBlock);
		if (!m_hMarkerOn || !m_hMarkerOff) return;

		// A third swatch, green, used only to say which experiment arm is
		// running. The console is drawn once across the window and is
		// unreadable in the headset, so a mode that can only be confirmed by
		// reading something cannot be confirmed at all - and a run was already
		// lost to exactly that.
		m_hMarkerAlt = g_pLTClient->CreateSurface(kMarkerBlock, kMarkerBlock);

		const HLTCOLOR cOn  = g_pLTClient->CreateColor(255.0f, 255.0f, 255.0f, LTFALSE);
		const HLTCOLOR cOff = g_pLTClient->CreateColor(0.0f, 0.0f, 0.0f, LTFALSE);
		const HLTCOLOR cAlt = g_pLTClient->CreateColor(0.0f, 255.0f, 0.0f, LTFALSE);
		for (uint32 y = 0; y < kMarkerBlock; ++y)
			for (uint32 x = 0; x < kMarkerBlock; ++x)
			{
				g_pLTClient->SetPixel(m_hMarkerOn,  x, y, cOn);
				g_pLTClient->SetPixel(m_hMarkerOff, x, y, cOff);
				if (m_hMarkerAlt) g_pLTClient->SetPixel(m_hMarkerAlt, x, y, cAlt);
			}
		VRLog::Msg("frame marker: %dx%d swatches created", kMarkerBlock, kMarkerBlock);
	}

	LTRect rBlock;
	rBlock.left = 0; rBlock.top = 0;
	rBlock.right = kMarkerBlock; rBlock.bottom = kMarkerBlock;

	// Eight data bits, then a fixed 1 0 1 0 sync. The sync is not
	// decoration: without it the host cannot tell the marker from whatever
	// else is in the corner, and a byte read off a menu or a wall still
	// looks like a perfectly good frame number. It matched a pose about
	// half the time, and every one of those matches was a pose picked at
	// random out of the last second and a half.
	//
	// Also gives the host a black and a white sample in every read, so it
	// can threshold against the marker's own levels instead of a constant.
	// The white swatch measures 170, not 255, on the desktop.
	g_pLTClient->Start3D();
	g_pLTClient->StartOptimized2D();
	for (int bit = 0; bit < 12; ++bit)
	{
		bool bOn;
		if (bit < 8) bOn = ((nHostFrame >> bit) & 1) != 0;
		else         bOn = ((bit - 8) % 2) == 0;		// 1 0 1 0

		HSURFACE hSrc = bOn ? m_hMarkerOn : m_hMarkerOff;
		g_pLTClient->DrawSurfaceToSurface(hScreen, hSrc, &rBlock,
			bit * kMarkerBlock, 0);
	}
	g_pLTClient->EndOptimized2D();
	g_pLTClient->End3D();
}

void CGameClientShell::ApplyEyeOpticalCentre(const LTRotation& rBase, int nEye)
{
	// Calibration overrides everything: left half at the normal aim, right half
	// yawed by a known angle, so one captured frame carries the same scene from
	// two angles and the host can measure the shift between them.
	if (m_nCalibFrames > 0)
	{
		LTRotation rUse = rBase;
		if (nEye == 1) g_pLTClient->EulerRotateY(&rUse, kCalibYawRad);
		g_pLTClient->SetObjectRotation(m_hCamera, &rUse);
		return;
	}

	if (g_vtVRAsymFrustum.GetFloat() <= 0.0f || !VRShared::IsLive())
	{
		g_pLTClient->SetObjectRotation(m_hCamera, (LTRotation*)&rBase);
		VRShared::PublishAppliedCentre(nEye, 0.0f, 0.0f);
		return;
	}

	const VRSharedState& s = VRShared::State();
	const int nMode = (int)g_vtVRAsymFrustum.GetFloat();

	// The two axes behave completely differently and must be controlled
	// separately.
	//
	// The VERTICAL centre is the SAME for both eyes (-5.5 degrees), so it can
	// only move both images up or down together. It cannot cause double
	// vision, and it is the axis the player reported warping worst.
	//
	// The HORIZONTAL centre is MIRRORED (-7 and +7), so it is the only one that
	// can cross the eyes. Applying it produced exactly that - a double image,
	// as if the viewer had gone cross-eyed - which is the signature Forsaken recorded
	// as "eyes being asked to diverge".
	//
	//   1 = vertical only        - safe, cannot diverge
	//   2 = vertical + horizontal
	//   3 = vertical + horizontal, sign flipped
	float fPitch = s.fEyeCentrePitchRad[nEye];
	float fYaw   = (nMode >= 2) ? s.fEyeCentreYawRad[nEye] : 0.0f;
	if (nMode >= 3) fYaw = -fYaw;

	// Built in OpenXR's frame, then flipped into LithTech's exactly as the
	// head rotation is: right-handed -Z forward to left-handed +Z forward is a
	// Z basis flip, which for a quaternion is (x,y,z,w) -> (-x,-y,z,w).
	//
	// Turning -Z toward +X is a NEGATIVE rotation about +Y, hence the sign on
	// yaw; turning it toward +Y is a positive rotation about +X.
	const float sy = (float)sin(-fYaw   * 0.5f), cy = (float)cos(-fYaw   * 0.5f);
	const float sx = (float)sin( fPitch * 0.5f), cx = (float)cos( fPitch * 0.5f);

	LTRotation rCentre;											// q_yaw * q_pitch
	rCentre.Init(-(cy * sx), -(sy * cx), -(sy * sx), cy * cx);	// with the Z flip

	LTRotation rEye = rBase * rCentre;
	g_pLTClient->SetObjectRotation(m_hCamera, &rEye);

	// Tell the host exactly what was applied, so it declares the same thing
	// rather than deriving it again and possibly differently.
	VRShared::PublishAppliedCentre(nEye, fYaw, fPitch);
}

void CGameClientShell::RenderWorldEyes(int nWorldRenders, LTBOOL bSideBySide)
{
	LTVector	vSavedPos;
	LTRotation	rSavedRot;
	LTBOOL		bSavedFull	= LTFALSE;
	int			nSavedL = 0, nSavedT = 0, nSavedR = 0, nSavedB = 0;
	float		fSavedFovX	= 0.0f;
	float		fSavedFovY	= 0.0f;

	g_pLTClient->GetObjectPos(m_hCamera, &vSavedPos);
	g_pLTClient->GetObjectRotation(m_hCamera, &rSavedRot);
	g_pLTClient->GetCameraRect(m_hCamera, &bSavedFull, &nSavedL, &nSavedT, &nSavedR, &nSavedB);
	g_pLTClient->GetCameraFOV(m_hCamera, &fSavedFovX, &fSavedFovY);

	const float fR2D = 57.2957795f;

	// VR: compose head rotation onto the camera's existing aim. Mouse still
	// turns the body; the head is a free look offset on top of it.
	//
	// Rotation only. Positional head movement is deliberately not applied -
	// physical lean is post-v0.1 per the brief, and it would let the camera
	// pass through geometry with no collision handling.
	//
	// ...unless the head is being fed through the mouse-look path, in which
	// case rSavedRot ALREADY contains it: UpdateHeadAsMouse added the head's
	// movement to m_fYaw and m_fPitch, and UpdateCameraRotation built the
	// camera from those before we were called. Composing it again here would
	// double every head movement.
	const bool bHeadAsMouse = (g_vtVRHeadAsMouse.GetFloat() > 0.0f);

	LTRotation rViewRot = rSavedRot;

	// Field capture: yaw the whole view by a known angle for this frame. The
	// eye offset is taken from rViewRot below, so both eyes move together and
	// the pair differs from the previous frame only by this rotation.
	if (m_nFieldFrame >= 0)
	{
		const float fYaw = (float)m_nFieldFrame * 0.01745329f;   // 1 degree per frame
		g_pLTClient->EulerRotateY(&rViewRot, fYaw);
		g_pLTClient->SetObjectRotation(m_hCamera, &rViewRot);
	}

	// Synthetic head pitch: same composition path as a real head, so whatever
	// it shows is what a real head of the same angle would show.
	const float fFakePitch = g_vtVRFakeHeadPitch.GetFloat();
	if (!bHeadAsMouse && fabs(fFakePitch) > 0.01f)
	{
		// The quaternion OpenXR would report for that much pitch, put through
		// the client's own conversion - nothing here is a shortcut.
		const float a = DEG2RAD(fFakePitch) * 0.5f;
		const float qx = (float)sin(a), qw = (float)cos(a);

		LTRotation rHead;
		rHead.Init(-qx, 0.0f, 0.0f, qw);

		rViewRot = (g_vtVRQuatHead.GetFloat() >= 2.0f) ? (rHead * rSavedRot)
													   : (rSavedRot * rHead);
		g_pLTClient->SetObjectRotation(m_hCamera, &rViewRot);

		static float s_fLast = -999.0f;
		if (fabs(fFakePitch - s_fLast) > 0.01f)
		{
			s_fLast = fFakePitch;
			LTVector vU, vR, vF;
			g_pLTClient->GetRotationVectors(&rViewRot, &vU, &vR, &vF);
			VRLog::Msg("fake head pitch %+.1f deg -> camera fwd(%+.3f %+.3f %+.3f) up(%+.3f %+.3f %+.3f)",
				fFakePitch, vF.x, vF.y, vF.z, vU.x, vU.y, vU.z);
			VRLog::Msg("  forward y %+.3f means the camera is looking %s",
				vF.y, (vF.y > 0.1f) ? "UP" : ((vF.y < -0.1f) ? "DOWN" : "level"));
		}
	}
	else if (!bHeadAsMouse && g_vtVRHeadTracking.GetFloat() > 0.0f && VRShared::IsLive())
	{
		const VRSharedState& s = VRShared::State();

		if (g_vtVRQuatHead.GetFloat() > 0.0f)
		{
			// Take the rotation whole.
			//
			// OpenXR is right-handed with -Z forward; LithTech is left-handed
			// with +Z forward. That is exactly a Z-axis basis flip, which for a
			// quaternion is (x,y,z,w) -> (-x,-y,z,w): the axis mirrors in Z and
			// the handedness change negates the angle. It agrees with the three
			// signs measured in the headset - yaw and pitch inverted, roll not -
			// which is why those were right for single-axis motion.
			//
			// Rebuilding from Euler angles was not. Three sequential axis
			// rotations only reproduce the original rotation when the motion is
			// about one axis; pitch combined with roll left the camera pointing
			// somewhere other than the pose declared to the runtime, and the
			// runtime warped the image to reconcile the two.
			LTRotation rHead;
			rHead.Init(-s.fHeadQuatX, -s.fHeadQuatY, s.fHeadQuatZ, s.fHeadQuatW);

			// Head is a local offset on top of the body's aim, matching what
			// the Euler path did - mouse still turns the body.
			//
			// Which product does that depends on whether LithTech composes
			// rotations in the body frame or the world frame, and the SDK
			// header does not say. 2 selects the other order so it can be
			// settled in the headset instead of guessed at.
			const int nMode = (int)g_vtVRQuatHead.GetFloat();

			if (nMode >= 3)
			{
				// Head first, against the body's YAW ONLY.
				//
				// Mode 2 stopped the world bending, and the reason is frame
				// agreement: it applies the head rotation first, in the world
				// frame - which is the frame the host declares the pose in. The
				// host declares the raw head pose in a LOCAL space with an
				// identity offset and has never been told about the body yaw at
				// all, so the runtime computes its reprojection correction in
				// that frame. An image rendered in a different frame gets the
				// correction applied about the wrong axes.
				//
				// What mode 2 gets wrong is the other half: it post-multiplies
				// the FULL body rotation, which carries the mouse's pitch, so
				// looking up also rolls the view. Reported exactly that way from the
				// headset: looking up also tilted the view.
				//
				// Yaw only, so the body can turn without ever tilting anything.
				LTRotation rBodyYaw;
				g_pLTClient->SetupEuler(&rBodyYaw, 0.0f, m_fYaw, 0.0f);
				rViewRot = rHead * rBodyYaw;
			}
			else
			{
				rViewRot = (nMode >= 2) ? (rHead * rSavedRot)
										: (rSavedRot * rHead);
			}
		}
		else
		{
			g_pLTClient->EulerRotateY(&rViewRot, DEG2RAD(s.fHeadYawDeg   * g_vtVRYawScale.GetFloat()));
			g_pLTClient->EulerRotateX(&rViewRot, DEG2RAD(s.fHeadPitchDeg * g_vtVRPitchScale.GetFloat()));
			g_pLTClient->EulerRotateZ(&rViewRot, DEG2RAD(s.fHeadRollDeg  * g_vtVRRollScale.GetFloat()));
		}

		g_pLTClient->SetObjectRotation(m_hCamera, &rViewRot);

		// THE REAL HEAD POSE, four times a second, with what the camera did
		// with it.
		//
		// Every previous check of this used a head rotation built here rather
		// than one from the headset - a clean pitch about X, which a real pose
		// never is: it carries the yaw the player is standing at as well. So
		// "the camera is fine" was only ever shown for an idealised input.
		//
		// This logs the pose as received and the camera basis that came out, so
		// a report of looking up and the view rolling becomes two numbers instead.
		{
			static DWORD s_dwNext = 0;
			const DWORD dwNow = GetTickCount();
			if (dwNow >= s_dwNext)
			{
				s_dwNext = dwNow + 250;

				const float fR2D = 57.2957795f;

				// The head as the runtime reported it, decomposed for reading.
				const float qx = s.fHeadQuatX, qy = s.fHeadQuatY;
				const float qz = s.fHeadQuatZ, qw = s.fHeadQuatW;

				const float sinp = 2.0f * (qw * qx - qy * qz);
				const float fHeadPitch = (fabs(sinp) >= 1.0f)
					? ((sinp > 0) ? 90.0f : -90.0f) : (float)asin(sinp) * fR2D;
				const float fHeadYaw = (float)atan2(2.0f * (qw * qy + qz * qx),
					1.0f - 2.0f * (qx * qx + qy * qy)) * fR2D;
				const float fHeadRoll = (float)atan2(2.0f * (qw * qz + qx * qy),
					1.0f - 2.0f * (qx * qx + qz * qz)) * fR2D;

				// What the camera actually ended up doing.
				LTVector vU, vR, vF;
				g_pLTClient->GetRotationVectors(&rViewRot, &vU, &vR, &vF);

				const float fCamPitch = (float)asin((vF.y > 1.0f) ? 1.0f
					: ((vF.y < -1.0f) ? -1.0f : vF.y)) * fR2D;

				LTVector vHR;
				vHR.x = vF.z; vHR.y = 0.0f; vHR.z = -vF.x;
				const float fLen = (float)sqrt(vHR.x * vHR.x + vHR.z * vHR.z);
				float fCamRoll = 0.0f;
				if (fLen > 1e-4f)
				{
					vHR.x /= fLen; vHR.z /= fLen;
					float d = vU.x * vHR.x + vU.z * vHR.z;
					if (d >  1.0f) d =  1.0f;
					if (d < -1.0f) d = -1.0f;
					fCamRoll = (float)asin(d) * fR2D;
				}

				// The residual against the PREVIOUS logged pose: how far the
				// compositor's correction would miss for the head motion that
				// actually happened. This is the warping, as a number, in the
				// live path rather than in a table of made-up poses.
				//
				// The pose is rebuilt here from the same four floats and the same
				// conversion the composition used - rHead belongs to the branch
				// above and this block runs after it. Carried across frames as
				// plain floats because this function is wrapped in __try and MSVC
				// will not emit a static local of class type inside one.
				LTRotation rHeadNow;
				rHeadNow.Init(-qx, -qy, qz, qw);

				float fResidual = -1.0f;
				if (g_bHavePrevHead)
				{
					LTRotation rPrev;
					rPrev.Init(g_fPrevHeadQuat[0], g_fPrevHeadQuat[1],
							   g_fPrevHeadQuat[2], g_fPrevHeadQuat[3]);
					fResidual = FrameResidualDeg(rPrev, rHeadNow, rSavedRot,
						(int)g_vtVRQuatHead.GetFloat());
				}
				g_fPrevHeadQuat[0] = rHeadNow.m_Quat[QX];
				g_fPrevHeadQuat[1] = rHeadNow.m_Quat[QY];
				g_fPrevHeadQuat[2] = rHeadNow.m_Quat[QZ];
				g_fPrevHeadQuat[3] = rHeadNow.m_Quat[QW];
				g_bHavePrevHead = true;

				VRLog::Msg("LIVE head yaw %+6.1f pitch %+6.1f roll %+6.1f  ->  camera pitch %+6.1f roll %+6.1f   (bodyYaw %+6.1f)",
					fHeadYaw, fHeadPitch, fHeadRoll, fCamPitch, fCamRoll, m_fYaw * fR2D);
				// The noise floor is about 0.07 deg - single-precision
				// quaternions, measured in the startup table where the answer
				// is known to be exactly zero. A wrong composition measures
				// 2.2 to 5.7 for the same motion, so half a degree sits in an
				// empty gap and the flag never fires on a correct run.
				if (fResidual >= 0.0f)
					VRLog::Msg("  compositor correction misses by %.2f deg over that motion%s",
						fResidual, (fResidual > 0.5f)
							? "   <- THIS IS THE BENDING: the picture is turned a way the scene did not move"
							: "   (noise floor 0.07)");
			}
		}

		if (g_bLogVRGeometry)
		{
			VRLog::Msg("head tracking applied: yaw %+.2f pitch %+.2f roll %+.2f (scales %+.1f %+.1f %+.1f)",
				s.fHeadYawDeg, s.fHeadPitchDeg, s.fHeadRollDeg,
				g_vtVRYawScale.GetFloat(), g_vtVRPitchScale.GetFloat(), g_vtVRRollScale.GetFloat());
		}
	}

	if (g_bLogVRGeometry)
	{
		uint32 dwSurfW = 0, dwSurfH = 0;
		g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &dwSurfW, &dwSurfH);
		VRLog::Msg("geometry: screen %ux%u  saved rect(%d,%d,%d,%d) full=%d  saved fov(%.2f, %.2f)deg",
			dwSurfW, dwSurfH, nSavedL, nSavedT, nSavedR, nSavedB, (int)bSavedFull,
			fSavedFovX * fR2D, fSavedFovY * fR2D);

		// WORLD SCALE, measured rather than assumed. Cast down to the floor and
		// up to the ceiling from the camera. Eye height for a standing adult is
		// about 1.6m and interior ceilings about 2.5m, so these two numbers
		// give the size of a world unit directly - and with it the correct
		// VRIPD, which no amount of tuning by eye can establish.
		ClientIntersectQuery IQuery;
		ClientIntersectInfo  IInfo;
		memset(&IQuery, 0, sizeof(IQuery));

		IQuery.m_From  = vSavedPos;
		IQuery.m_To    = vSavedPos - LTVector(0.0f, 400.0f, 0.0f);
		IQuery.m_Flags = INTERSECT_HPOLY | IGNORE_NONSOLID;

		if (g_pLTClient->IntersectSegment(&IQuery, &IInfo))
		{
			const float fEyeUnits = vSavedPos.y - IInfo.m_Point.y;
			VRLog::Msg("world scale: camera is %.1f units above the floor", fEyeUnits);
			if (fEyeUnits > 1.0f)
			{
				VRLog::Msg("  if that is a 1.6m eye height, 1 unit = %.2f mm, so VRIPD should be %.2f for a 62.1mm IPD",
					1600.0f / fEyeUnits, 62.1f * fEyeUnits / 1600.0f);
			}
		}
		else
		{
			VRLog::Msg("world scale: no floor found below the camera");
		}

		memset(&IQuery, 0, sizeof(IQuery));
		IQuery.m_From  = vSavedPos;
		IQuery.m_To    = vSavedPos + LTVector(0.0f, 400.0f, 0.0f);
		IQuery.m_Flags = INTERSECT_HPOLY | IGNORE_NONSOLID;

		if (g_pLTClient->IntersectSegment(&IQuery, &IInfo))
		{
			VRLog::Msg("  ceiling is %.1f units above the camera", IInfo.m_Point.y - vSavedPos.y);
		}
	}

	__try
	{
		if (!bSideBySide)
		{
			// Mono, or M2's double render with an unchanged camera.
			g_pLTClient->Start3D();
			for (int nEye = 0; nEye < nWorldRenders; ++nEye)
			{
				VRLog::Count(VRLog::CTR_WORLD_RENDER);
				g_pLTClient->RenderCamera(m_hCamera);
			}
			g_pLTClient->End3D();
		}
		else
		{
			// Both eyes render into a viewport at the ORIGIN, which is the only
			// geometry d3d.ren gets right (see docs/M3-RENDERER-VIEWPORT-BUG.md).
			// The right eye is drawn first, copied aside, then the left eye is
			// drawn over it and the copy is placed back on the right. No offset
			// viewport is ever used, so the renderer bug cannot apply.
			const int nHalfWidth = (nSavedR - nSavedL) / 2;
			const int nHeight    = nSavedB - nSavedT;

			// Eye offset follows the head-adjusted view, not the body aim, so
			// the stereo baseline stays level with where you are looking.
			LTVector vUp, vRight, vForward;
			g_pLTClient->GetRotationVectors(&rViewRot, &vUp, &vRight, &vForward);

			// No eye separation while calibrating: the two halves must differ
			// only by the known yaw, or the parallax is measured as part of
			// the shift. At 200 units that is about half a degree against a
			// 5 degree signal - a 10% error in the answer.
			const float fHalfIPD = (m_nCalibFrames > 0)
				? 0.0f : (EffectiveIPDUnits() * 0.5f);
			const LTBOOL bSwap = (g_vtVRSwapEyes.GetFloat() > 0.0f) ? LTTRUE : LTFALSE;

			g_pLTClient->SetCameraRect(m_hCamera, LTFALSE, 0, 0, nHalfWidth, nHeight);

			if (g_vtVRUseHeadsetFov.GetFloat() > 0.0f && VRShared::IsLive())
			{
				// Render the headset's own field of view. The Quest 3's per-eye
				// frustum is ASYMMETRIC (L54 R40, U44 D55) because the panels
				// are canted outward, but SetCameraFOV only builds symmetric
				// frustums and the renderer is a closed binary. So render a
				// symmetric frustum that CONTAINS the asymmetric one and let
				// the runtime crop. Costs ~28% extra pixels; rendering is cheap
				// enough (M3: 1.2ms) that this is the right trade.
				const VRSharedState& s = VRShared::State();

				const float fL = (float)fabs(s.fFovLeftRad);
				const float fR = (float)fabs(s.fFovRightRad);
				const float fU = (float)fabs(s.fFovUpRad);
				const float fD = (float)fabs(s.fFovDownRad);

				// Margin so reprojection has image beyond what the headset
				// shows. Applied to the angle, since that is what a head
				// rotation between render and display consumes.
				const float fMargin = (g_vtVRFovMargin.GetFloat() > 0.1f)
					? g_vtVRFovMargin.GetFloat() : 1.0f;

				// With the optical centres applied, each eye is rendered about
				// its OWN axis, so the frustum it needs is the half-SPAN -
				// (54+40)/2 = 47 horizontally, (44+55)/2 = 49.5 vertically -
				// not the maximum edge. The maximum is only needed when
				// rendering about the forward axis, where the symmetric
				// frustum has to reach the furthest edge to contain the
				// asymmetric one.
				//
				// These are already absolute values, so the mean of the pair
				// is the half-span.
				// Per axis, because the two are enabled separately: an axis is
				// only entitled to the narrower half-span if the camera is
				// actually being rotated onto its centre. Claim the half-span
				// without the rotation and the frustum no longer contains the
				// eye's, which shows as black at the edges.
				const int   nAsym  = (int)g_vtVRAsymFrustum.GetFloat();
				const bool  bAsym  = (nAsym > 0);

				const float fHalfX = ((nAsym >= 2) ? ((fL + fR) * 0.5f)
												   : ((fL > fR) ? fL : fR)) * fMargin;
				const float fHalfY = ((nAsym >= 1) ? ((fU + fD) * 0.5f)
												   : ((fU > fD) ? fU : fD)) * fMargin;

				if (fHalfX > 0.0f && fHalfY > 0.0f && nHeight > 0)
				{
					// The renderer maps horizontal FOV across the SCREEN
					// SURFACE width, not the eye viewport width. Measured: the
					// owner found a 1.27x multiplier made doors correct, and
					// in tangent space that is tan(68.7)/tan(54) = 1.86, i.e.
					// screenWidth/viewportWidth = 1920/960 = 2 to within the player's
					// ability to judge. Vertical is unaffected because the eye
					// viewport is the full screen height.
					//
					// So, for square pixels:
					//   tan(wanted) = tan(fovY/2) * viewportW / viewportH
					// and to make the renderer produce that:
					//   tan(fovXset/2) = tan(wanted) * screenW / viewportW
					//                  = tan(fovY/2) * screenW / viewportH
					const float fScreenW  = (float)(nSavedR - nSavedL);
					const float fViewW    = (float)nHalfWidth;
					const float fViewH    = (float)nHeight;

					// Cover BOTH axes, then expand the slack one to keep square
					// pixels.
					//
					// The horizontal used to be derived from the vertical
					// alone. That was safe only while both axes wanted the same
					// thing. Once the vertical could be narrowed independently
					// - rotating onto its centre earns the smaller half-span -
					// the horizontal silently shrank with it, even though
					// nothing was rotating it and it still had to reach the
					// eye's full extent. It fell from 59.6 to 53.4 degrees and
					// the uncovered edge showed as vertical black bars in the
					// periphery, which felt boxed in.
					//
					// Sizing each axis from what it actually needs, and then
					// growing whichever is short, makes coverage structural
					// rather than something VRFovMargin has to be tuned to
					// paper over.
					const float fAspect = fViewW / fViewH;

					// Shout if the render resolution has the wrong shape.
					//
					// The headset's frustum wants a per-eye aspect of 0.964.
					// At anything narrower the horizontal cannot be covered
					// without inflating the VERTICAL to compensate - measured
					// at 2560x1600 (aspect 0.80): 129 degrees of vertical field
					// where the headset shows 99. The runtime then crops to
					// what it needs, magnifying the middle and distorting the
					// edges, which reads as the world being short and wide and
					// warping when the head tilts.
					//
					// The in-game display menu cannot know any of this, so
					// choosing a resolution there silently breaks the geometry.
					// It has now done so three times, and looked like a
					// rendering bug each time.
					{
						static float s_fLastAspect = -1.0f;
						if (fabs(fAspect - s_fLastAspect) > 0.001f)
						{
							s_fLastAspect = fAspect;
							VRLog::Msg("per-eye aspect %.3f (headset wants 0.964)%s",
								fAspect,
								(fabs(fAspect - 0.964f) > 0.02f)
									? "  <- WRONG SHAPE: use tools/set-res.ps1, not the display menu"
									: "");
						}
					}

					float fTanX = (float)tan(fHalfX);
					float fTanY = (float)tan(fHalfY * g_vtVRFovYTest.GetFloat());

					if (fTanX > fTanY * fAspect) fTanY = fTanX / fAspect;
					else                         fTanX = fTanY * fAspect;

					// What the eye should actually see, on both axes.
					const float fWantTanX = fTanX;
					const float fWantTanY = fTanY;

					// What to ask for so the renderer produces it.
					//
					// Both axes are now corrected the same way, in the same
					// space. The vertical previously went through untouched -
					// fFovY was 2*atan(fWantTanY) - so any scale the renderer
					// applies to it reached the eye unopposed while the
					// horizontal was being trimmed. Two axes magnified by
					// different factors is a non-uniform scale locked to the
					// head, which bends straight lines as you look around.
					// VRFovYScale defaults to 1.0, so this is inert until the
					// vertical measurement names a value.
					// The factor between what we ASK the renderer for and what
					// we DECLARE to the runtime, per axis. The vertical's is 1.0
					// by construction, so anything other than 1.0 here is a
					// head-locked anisotropic squash.
					const float fXFactor = (g_vtVRFovUniform.GetFloat() > 0.0f)
						? 1.0f
						: ((fScreenW / fViewW) * g_vtVRFovXTest.GetFloat());

					const float fFovX = 2.0f * (float)atan(fWantTanX * fXFactor);

					// Say it out loud whenever it changes. "Work out what the
					// value has to be for the scale to come out exactly 1, and
					// assert it" - the lesson the Descent port paid for.
					{
						static float s_fLastFactor = -1.0f;
						if (fabs(fXFactor - s_fLastFactor) > 0.001f)
						{
							s_fLastFactor = fXFactor;
							VRLog::Msg("render/declare tangent ratio: x %.3f, y 1.000%s",
								fXFactor,
								(fabs(fXFactor - 1.0f) > 0.01f)
									? "   <- ANISOTROPIC: the squash axis is locked to the head"
									: "   - uniform, the mapping is a rigid scale");
						}
					}
					const float fFovY = 2.0f * (float)atan(
						fWantTanY * g_vtVRFovYScale.GetFloat());

					g_pLTClient->SetCameraFOV(m_hCamera, fFovX, fFovY);

					// Read it straight back. "Is this quantity even in the
					// output path" is the first question to ask of any value
					// that seems not to matter, and it has never been asked of
					// fovY. A 121-degree vertical is an extreme thing to hand a
					// 2000-era engine; if it clamps, everything downstream is
					// declared against a field that was never rendered.
					{
						static float s_fLastAskX = -1.0f, s_fLastAskY = -1.0f;
						if (fabs(fFovX - s_fLastAskX) > 0.001f ||
							fabs(fFovY - s_fLastAskY) > 0.001f)
						{
							s_fLastAskX = fFovX;
							s_fLastAskY = fFovY;

							float fGotX = 0.0f, fGotY = 0.0f;
							g_pLTClient->GetCameraFOV(m_hCamera, &fGotX, &fGotY);

							const bool bKept = (fabs(fGotX - fFovX) < 0.001f)
											&& (fabs(fGotY - fFovY) < 0.001f);
							VRLog::Msg("camera fov: asked (%.2f, %.2f) deg, engine returned"
								" (%.2f, %.2f) deg   %s",
								fFovX * fR2D, fFovY * fR2D,
								fGotX * fR2D, fGotY * fR2D,
								bKept ? "- kept verbatim"
									  : "<- THE ENGINE CHANGED IT");
						}
					}

					// Declare the EFFECTIVE field of view - what the eye sees,
					// not the inflated values used to coax it out of the
					// renderer. Declaring the asked vertical instead would put
					// the correction back into the runtime's frustum, which is
					// where it started.
					VRShared::PublishFov(2.0f * (float)atan(fWantTanX),
										 2.0f * (float)atan(fWantTanY));
					VRShared::PublishPoseLag(g_vtVRPoseLag.GetFloat());
					VRShared::PublishAsymActive(bAsym);
					VRShared::PublishExactPose(g_vtVRExactPose.GetFloat() > 0.0f);
					VRShared::PublishCalib(m_nCalibFrames > 0, kCalibYawRad);
					if (m_nCalibFrames > 0) --m_nCalibFrames;
				}
			}
			else if (nHeight > 0 && g_vtVRFovAdjust.GetFloat() > 0.0f)
			{
				// No headset: half-SBS renders a full-window field of view into
				// a half-width viewport, so the image is squeezed and a 3D
				// viewer's stretch restores it. Full-SBS keeps each half
				// geometrically correct as it stands.
				const float fAspect = (g_vtVRHalfSbs.GetFloat() > 0.0f)
					? ((float)(nSavedR - nSavedL) / (float)nHeight)
					: ((float)nHalfWidth / (float)nHeight);

				const float fEyeFovX = 2.0f * (float)atan(
					tan(fSavedFovY * 0.5f) * fAspect * g_vtVROffsetFovScale.GetFloat());
				g_pLTClient->SetCameraFOV(m_hCamera, fEyeFovX,
					fSavedFovY * g_vtVRFovYTest.GetFloat());
			}

			// The head's translation since the level was entered, in the
			// camera's own frame. See the note on VRHeadPos.
			LTVector vHeadOfs(0.0f, 0.0f, 0.0f);
			if (g_vtVRHeadPos.GetFloat() > 0.0f && VRShared::IsLive())
			{
				const VRSharedState& hs = VRShared::State();

				// Re-reference on a JUMP - a teleport, a level change or a
				// recentre - measured frame to frame. Half a metre between two
				// consecutive frames is faster than a head moves; half a metre
				// from the reference is just a player who leaned or took a
				// step, and re-centring on that would snap the world sideways
				// exactly when they moved to look at something.
				const float dx = hs.fHeadPosX - s_fHeadPrevX;
				const float dy = hs.fHeadPosY - s_fHeadPrevY;
				const float dz = hs.fHeadPosZ - s_fHeadPrevZ;
				// A RECENTER re-references whatever the distance: sitting down
				// from standing is about 0.4 m, under the jump threshold.
				{
					// ...and keep re-referencing for a few frames after: the number
					// and the first pose in the new space can arrive a frame apart,
					// and referencing to the old space's last pose would drop the
					// camera by the whole head height.
					static uint32_t s_nRecenterSeen = 0;
					static int      s_nRecenterSettle = 0;
					const uint32_t nGen = VRShared::RecenterGeneration();
					if (nGen != s_nRecenterSeen)
					{
						s_nRecenterSeen = nGen;
						s_nRecenterSettle = 6;
						VRLog::Msg("VRRecenter: generation %u seen - head at %.2f %.2f %.2f m, settling",
								   (unsigned)nGen, hs.fHeadPosX, hs.fHeadPosY, hs.fHeadPosZ);
					}
					if (s_nRecenterSettle > 0)
					{
						s_bHeadRefValid = LTFALSE;
						if (--s_nRecenterSettle == 0)
							VRLog::Msg("VRRecenter: settled - head reference %.2f %.2f %.2f m",
									   hs.fHeadPosX, hs.fHeadPosY, hs.fHeadPosZ);
					}
				}
				if (!s_bHeadRefValid || (dx*dx + dy*dy + dz*dz) > 0.25f)
				{
					s_bHeadRefValid = LTTRUE;
					s_fHeadRefX = hs.fHeadPosX;
					s_fHeadRefY = hs.fHeadPosY;
					s_fHeadRefZ = hs.fHeadPosZ;
				}
				s_fHeadPrevX = hs.fHeadPosX;
				s_fHeadPrevY = hs.fHeadPosY;
				s_fHeadPrevZ = hs.fHeadPosZ;

				const float fUnits = (g_vtVRUnitMM.GetFloat() > 0.01f)
					? (1000.0f / g_vtVRUnitMM.GetFloat()) : 58.75f;

				const float px = (hs.fHeadPosX - s_fHeadRefX) * fUnits;
				const float py = (hs.fHeadPosY - s_fHeadRefY) * fUnits;
				// OpenXR is -Z forward and right-handed; the game is
				// left-handed. Same flip WeaponModel.cpp applies to the hands.
				const float pz = -(hs.fHeadPosZ - s_fHeadRefZ) * fUnits;

				// Room space maps into the world by the BODY's yaw, and by
				// nothing else. Not by rViewRot: that already contains the
				// head rotation, so using it would turn the translation frame
				// as the head turns and couple rotation into position. Not by
				// the body's full rotation either: room-up must stay
				// world-up however far the mouse is aiming up or down.
				// THE HEAD'S TRANSLATION GOES INTO THE CAMERA'S FRAME, NOT THE
				// BODY'S. This used m_fYaw, the player's yaw - the same thing as
				// the camera's in first person and NOT in a cinematic, where the
				// level's camera faces wherever it was placed. Turning the head
				// then moved the eye in the wrong direction. In the HQ dialogue the
				// camera swayed instead of tracking the normal VR way, and Cate
				// vanishing as the eye was pushed into her. The yaw comes from
				// the camera object's own rotation now.
				LTRotation rYawOnly;
				{
					LTVector cU, cR, cF;
					g_pLTClient->GetRotationVectors(&rSavedRot, &cU, &cR, &cF);
					const float fCamYaw = (float)atan2(cF.x, cF.z);
					g_pLTClient->SetupEuler(&rYawOnly, 0.0f, fCamYaw, 0.0f);
				}
				LTVector vYU, vYR, vYF;
				g_pLTClient->GetRotationVectors(&rYawOnly, &vYU, &vYR, &vYF);

				vHeadOfs = vYR * px + LTVector(0.0f, 1.0f, 0.0f) * py + vYF * pz;

				static int s_nHeadPosLogged = 0;
				if (s_nHeadPosLogged++ % 500 == 0)
					VRLog::Msg("  head translation %+.3f %+.3f %+.3f m -> "
							   "%+.1f %+.1f %+.1f units in the camera frame",
							   hs.fHeadPosX - s_fHeadRefX,
							   hs.fHeadPosY - s_fHeadRefY,
							   hs.fHeadPosZ - s_fHeadRefZ,
							   vHeadOfs.x, vHeadOfs.y, vHeadOfs.z);
			}

			HSURFACE hScreen = g_pLTClient->GetScreenSurface();
			LTRect   rHalf   = { 0, 0, nHalfWidth, nHeight };

			// Copying screen->screen moves the first eye across in ONE copy.
			// Routing it through an off-screen surface costs two. Both are kept
			// because a same-surface blit is not guaranteed to be supported.
			const LTBOOL bDirect = (g_vtVRDirectBlit.GetFloat() > 0.0f) ? LTTRUE : LTFALSE;
			HSURFACE hStash = bDirect ? LTNULL : GetEyeStashSurface(nHalfWidth, nHeight);

			// --- pass 1: the eye destined for the RIGHT half ---
			LTVector vPosB = vSavedPos + vHeadOfs
						   + vRight * (bSwap ? -fHalfIPD : fHalfIPD);
			VRClampEyeToCentre(vSavedPos + vHeadOfs, vPosB, bSwap ? 0 : 1);
			g_pLTClient->SetObjectPos(m_hCamera, &vPosB);
			ApplyEyeOpticalCentre(rViewRot, bSwap ? 0 : 1);

			if (g_bLogVRGeometry) LogEyeGeometry(0, vSavedPos);
			VRLog::Count(VRLog::CTR_WORLD_RENDER);
			g_pLTClient->ClearScreen(&rHalf, CLEARSCREEN_SCREEN | CLEARSCREEN_RENDER);
			g_pLTClient->Start3D();
			g_pLTClient->RenderCamera(m_hCamera);
			g_pLTClient->End3D();

			// Move it aside before the left eye overdraws the same region.
			g_pLTClient->Start3D();
			g_pLTClient->StartOptimized2D();
			const LTRESULT nBlitRes = bDirect
				? g_pLTClient->DrawSurfaceToSurface(hScreen, hScreen, &rHalf, nHalfWidth, 0)
				: (hStash ? g_pLTClient->DrawSurfaceToSurface(hStash, hScreen, &rHalf, 0, 0) : LT_ERROR);
			g_pLTClient->EndOptimized2D();
			g_pLTClient->End3D();

			if (g_bLogVRGeometry)
			{
				VRLog::Msg("  eye copy: %s, result %d (%s)",
					bDirect ? "screen->screen, 1 copy" : "via stash surface, 2 copies",
					(int)nBlitRes, (nBlitRes == LT_OK) ? "OK" : "FAILED - set VRDirectBlit 0");
			}

			// --- pass 2: the eye destined for the LEFT half ---
			LTVector vPosA = vSavedPos + vHeadOfs
						   + vRight * (bSwap ? fHalfIPD : -fHalfIPD);
			VRClampEyeToCentre(vSavedPos + vHeadOfs, vPosA, bSwap ? 1 : 0);
			ApplyEyeOpticalCentre(rViewRot, bSwap ? 1 : 0);
			g_pLTClient->SetObjectPos(m_hCamera, &vPosA);

			if (g_bLogVRGeometry) LogEyeGeometry(1, vSavedPos);
			VRLog::Count(VRLog::CTR_WORLD_RENDER);

			// The engine clears the depth buffer once per frame. Both eyes now
			// draw into the SAME region, so without a clear here the second
			// eye depth-tests against the first eye's values - near-identical,
			// producing the stipple z-fighting seen against bright sky.
			g_pLTClient->ClearScreen(&rHalf, CLEARSCREEN_SCREEN | CLEARSCREEN_RENDER);

			g_pLTClient->Start3D();
			g_pLTClient->RenderCamera(m_hCamera);
			g_pLTClient->End3D();

			// The stash route needs a second copy to place the eye on the right.
			if (!bDirect && hStash)
			{
				g_pLTClient->Start3D();
				g_pLTClient->StartOptimized2D();
				g_pLTClient->DrawSurfaceToSurface(hScreen, hStash, &rHalf, nHalfWidth, 0);
				g_pLTClient->EndOptimized2D();
				g_pLTClient->End3D();
			}

			// Field capture: write this frame's LEFT eye out, full resolution.
			//
			// Taken here, at the end of an ordinary world render, with no extra
			// RenderCamera of any kind - the left half of the screen holds the
			// left eye exactly as the engine drew it. The stash has already
			// done its job for this frame and is free to reuse.
			if (m_nFieldFrame >= 0 && hStash)
			{
				g_pLTClient->Start3D();
				g_pLTClient->StartOptimized2D();
				g_pLTClient->DrawSurfaceToSurface(hStash, hScreen, &rHalf, 0, 0);
				g_pLTClient->EndOptimized2D();
				g_pLTClient->End3D();

				char szTag[32];
				sprintf(szTag, "field-%02ddeg", m_nFieldFrame);
				// Full horizontal resolution, rows 30%-38% of the frame:
				// above the weapon, below the sky, and about the same number
				// of GetPixel calls as the full-frame step-4 dump that is
				// known to run without stalling.
				const int nBandY0 = nHeight * 30 / 100;
				const int nBandY1 = nHeight * 38 / 100;
				DumpStashImage(szTag, hStash, nHalfWidth, nHeight, 1, nBandY0, nBandY1);

				VRLog::Msg("field capture: frame %d of %d, yaw %d deg, band rows %d-%d of %d, fov set %.2f deg",
					m_nFieldFrame + 1, m_nFieldFrames, m_nFieldFrame,
					nBandY0, nBandY1, nHeight, fSavedFovX * fR2D);

				if (++m_nFieldFrame >= m_nFieldFrames)
				{
					m_nFieldFrame = -1;
					VRLog::Msg("field capture: done - images are in this log directory");
				}
			}
		}

		g_bLogVRGeometry = LTFALSE;
	}
	__finally
	{
		g_pLTClient->SetObjectPos(m_hCamera, &vSavedPos);
		g_pLTClient->SetObjectRotation(m_hCamera, &rSavedRot);
		g_pLTClient->SetCameraRect(m_hCamera, bSavedFull, nSavedL, nSavedT, nSavedR, nSavedB);
		g_pLTClient->SetCameraFOV(m_hCamera, fSavedFovX, fSavedFovY);
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdatePlayer()
//
//	PURPOSE:	Update the player
//
// ----------------------------------------------------------------------- //

void CGameClientShell::UpdatePlayer()
{
    HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();
	if (!hPlayerObj || IsPlayerDead()) return;


	// This is pretty much a complete kludge, but I can't really think of
	// a better way to handle this...Okay, since the server can update the
	// player's flags at any time (and override anything that we set), we'll
	// make sure that the player's flags are always what we want them to be :)

    uint32 dwPlayerFlags = g_pLTClient->GetObjectFlags(hPlayerObj);
	if (m_PlayerCamera.IsFirstPerson())
	{
		if (dwPlayerFlags & FLAG_VISIBLE)
		{
            ShowPlayer(LTFALSE);
		}
	}
	else  // Third person
	{
		if (!(dwPlayerFlags & FLAG_VISIBLE))
		{
            ShowPlayer(LTTRUE);
		}
	}


	// Hide/Show our attachments...

	HideShowAttachments(hPlayerObj);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HideShowAttachments()
//
//	PURPOSE:	Recursively hide/show attachments...
//
// ----------------------------------------------------------------------- //

void CGameClientShell::HideShowAttachments(HOBJECT hObj)
{
	if (!hObj) return;

	HLOCALOBJ attachList[20];
    uint32 dwListSize = 0;
    uint32 dwNumAttach = 0;

    g_pLTClient->GetAttachments(hObj, attachList, 20, &dwListSize, &dwNumAttach);
	int nNum = dwNumAttach <= dwListSize ? dwNumAttach : dwListSize;

	for (int i=0; i < nNum; i++)
	{
        uint32 dwUsrFlags;
        g_pLTClient->GetObjectUserFlags(attachList[i], &dwUsrFlags);

        if (dwUsrFlags & USRFLG_ATTACH_HIDE1SHOW3)
		{
            uint32 dwFlags = g_pLTClient->GetObjectFlags(attachList[i]);

			if (m_PlayerCamera.IsFirstPerson())
			{
				if (dwFlags & FLAG_VISIBLE)
				{
					dwFlags &= ~FLAG_VISIBLE;
                    g_pLTClient->SetObjectFlags(attachList[i], dwFlags);
				}

				if (!(dwFlags & FLAG_PORTALVISIBLE))
				{
					dwFlags |= FLAG_PORTALVISIBLE;
					g_pLTClient->SetObjectFlags(attachList[i], dwFlags);
				}
			}
			else
			{
				if (!(dwFlags & FLAG_VISIBLE))
				{
					dwFlags |= FLAG_VISIBLE;
                    g_pLTClient->SetObjectFlags(attachList[i], dwFlags);
				}
			}
		}
		else if (dwUsrFlags & USRFLG_ATTACH_HIDE1)
		{
            uint32 dwFlags = g_pLTClient->GetObjectFlags(attachList[i]);

			if (m_PlayerCamera.IsFirstPerson())
			{
				if (dwFlags & FLAG_VISIBLE)
				{
					dwFlags &= ~FLAG_VISIBLE;
                    g_pLTClient->SetObjectFlags(attachList[i], dwFlags);
				}
			}
		}

		// Hide/Show this attachment's attachments...
		HideShowAttachments(attachList[i]);
	}
}



// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::Update3rdPersonInfo
//
//	PURPOSE:	Update the 3rd person cross hair / camera info
//
// ----------------------------------------------------------------------- //

void CGameClientShell::Update3rdPersonInfo()
{
    HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();
	if (!hPlayerObj || IsPlayerDead()) return;

    HOBJECT hFilterList[] = {hPlayerObj, m_MoveMgr.GetObject(), LTNULL};


	ClientIntersectInfo info;
	ClientIntersectQuery query;
    LTVector vPlayerPos, vUp, vRight, vForward;

    g_pLTClient->GetObjectPos(hPlayerObj, &vPlayerPos);

    LTFLOAT fCrosshairDist = -1.0f;
    LTFLOAT fCameraOptZ = g_vtChaseCamDistBack.GetFloat();

	WEAPON* pWeapon = g_pWeaponMgr->GetWeapon(m_weaponModel.GetWeaponId());
	if (!pWeapon) return;

	// Figure out crosshair distance...

	if (m_InterfaceMgr.IsCrosshairOn() && m_weaponModel.GetHandle())
	{
        fCrosshairDist = (LTFLOAT) pWeapon->nRange;

        g_pLTClient->GetRotationVectors(&m_rRotation, &vUp, &vRight, &vForward);

		// Determine where the cross hair should be...

        LTVector vStart, vEnd, vPos;
		VEC_COPY(vStart, vPlayerPos);
		VEC_MULSCALAR(vEnd, vForward, fCrosshairDist);
		VEC_ADD(vEnd, vEnd, vStart);

		VEC_COPY(query.m_From, vStart);
		VEC_COPY(query.m_To, vEnd);

		query.m_Flags = INTERSECT_OBJECTS | IGNORE_NONSOLID;
		query.m_FilterFn = ObjListFilterFn;
		query.m_pUserData = hFilterList;

        if (g_pLTClient->IntersectSegment (&query, &info))
		{
			VEC_COPY(vPos, info.m_Point);
		}
		else
		{
			VEC_COPY(vPos, vEnd);
		}

        LTVector vTemp;
		VEC_SUB(vTemp, vPos, vStart);

		fCrosshairDist = VEC_MAG(vTemp);
	}


	// Figure out optinal camera distance...

    LTRotation rRot;
    g_pLTClient->GetObjectRotation(hPlayerObj, &rRot);
    g_pLTClient->GetRotationVectors(&rRot, &vUp, &vRight, &vForward);
	VEC_NORM(vForward);

	// Determine how far behind the player the camera can go...

    LTVector vEnd;
	VEC_MULSCALAR(vEnd, vForward, -fCameraOptZ);
	VEC_ADD(vEnd, vEnd, vPlayerPos);

	VEC_COPY(query.m_From, vPlayerPos);
	VEC_COPY(query.m_To, vEnd);

	query.m_Flags = INTERSECT_OBJECTS | IGNORE_NONSOLID;
	query.m_FilterFn = ObjListFilterFn;
	query.m_pUserData = hFilterList;

    if (g_pLTClient->IntersectSegment (&query, &info))
	{
        LTVector vTemp;
		VEC_SUB(vTemp, info.m_Point, vPlayerPos);
        LTFLOAT fDist = VEC_MAG(vTemp);

		fCameraOptZ = fDist < fCameraOptZ ? -(fDist - 5.0f) : -fCameraOptZ;
	}
	else
	{
		fCameraOptZ = -fCameraOptZ;
	}


	Update3rdPersonCrossHair(fCrosshairDist);
	m_PlayerCamera.SetOptZ(fCameraOptZ);
}



// --------------------------------------------------------------------------- //
//
//	ROUTINE:	UpdateModelGlow
//
//	PURPOSE:	Update the current model glow color
//
// --------------------------------------------------------------------------- //

void CGameClientShell::UpdateModelGlow()
{
    LTFLOAT fColor      = 0.0f;
	LTFLOAT	fMin		= g_vtModelGlowMin.GetFloat();
	LTFLOAT	fMax		= g_vtModelGlowMax.GetFloat();
    LTFLOAT fColorRange = fMax - fMin;
	LTFLOAT fTimeRange  = g_vtModelGlowTime.GetFloat();

	if (m_bModelGlowCycleUp)
	{
		if (m_fModelGlowCycleTime < fTimeRange)
		{
			fColor = fMin + (fColorRange * (m_fModelGlowCycleTime / fTimeRange));
			m_vCurModelGlow.Init(fColor, fColor, fColor);
		}
		else
		{
			m_fModelGlowCycleTime = 0.0f;
			m_vCurModelGlow.Init(fMax, fMax, fMax);
			m_bModelGlowCycleUp = LTFALSE;
			return;
		}
	}
	else
	{
		if (m_fModelGlowCycleTime < fTimeRange)
		{
			fColor = fMax - (fColorRange * (m_fModelGlowCycleTime / fTimeRange));
			m_vCurModelGlow.Init(fColor, fColor, fColor);
		}
		else
		{
			m_fModelGlowCycleTime = 0.0f;
			m_vCurModelGlow.Init(fMin, fMin, fMin);
            m_bModelGlowCycleUp = LTTRUE;
			return;
		}
	}

	m_fModelGlowCycleTime += m_fFrameTime;
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::InitSinglePlayer
//
//	PURPOSE:	Send the server the initial single player info
//
// --------------------------------------------------------------------------- //

void CGameClientShell::InitSinglePlayer()
{
	CGameSettings* pSettings = m_InterfaceMgr.GetSettings();
	if (!pSettings) return;

	m_eGameType = SINGLE;

	// Init player variables on server...

    HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_PLAYER_INITVARS);
    g_pLTClient->WriteToMessageByte(hMessage, (uint8)pSettings->RunLock());
    g_pLTClient->EndMessage(hMessage);
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::InitMultiPlayer
//
//	PURPOSE:	Send the server the initial multiplayer info
//
// --------------------------------------------------------------------------- //
void CGameClientShell::InitMultiPlayer()
{
	if (!IsMultiplayerGame()) return;

	char	sName[255];
	char	sMod[255];
	char	sSkin[255];
	char	sHead[255];
    int		nTeam;

	SAFE_STRCPY(sName,g_vtPlayerName.GetStr());
	GetConsoleString("NetPlayerModel",sMod,"Hero,action");
	GetConsoleString("NetPlayerSkin",sSkin,"");
	GetConsoleString("NetPlayerHead",sHead,"");
	nTeam = GetConsoleInt("NetPlayerTeam",0);

	if ( !sSkin[0] || !sHead[0] )
	{
		char szTemp[512];
		strcpy(szTemp, sMod);
		if ( strchr(szTemp, ',') )
		{
			*strchr(szTemp, ',') = '_';
			_strlwr(szTemp);
			strcpy(sSkin, g_pModelButeMgr->GetButeMgr()->GetString(szTemp, "Skin0"));
			strcpy(sHead, g_pModelButeMgr->GetButeMgr()->GetString(szTemp, "Head0"));
		} 
	}

    HSTRING hstrName = g_pLTClient->CreateString(sName);
	if (!hstrName) return;
    HSTRING hstrMod = g_pLTClient->CreateString(sMod);
	if (!hstrMod) return;
    HSTRING hstrSkin = g_pLTClient->CreateString(strchr(sSkin, ',')+1);
	if (!hstrSkin) return;
    HSTRING hstrHead = g_pLTClient->CreateString(strchr(sHead, ',')+1);
	if (!hstrHead) return;

	// Init multiplayer info on server...

    HMESSAGEWRITE hWrite = g_pLTClient->StartMessage(MID_PLAYER_MULTIPLAYER_INIT);
    g_pLTClient->WriteToMessageHString(hWrite, hstrName);
    g_pLTClient->WriteToMessageHString(hWrite, hstrMod);
    g_pLTClient->WriteToMessageHString(hWrite, hstrSkin);
    g_pLTClient->WriteToMessageHString(hWrite, hstrHead);
    g_pLTClient->EndMessage(hWrite);

	g_pLTClient->FreeString(hstrName);
	g_pLTClient->FreeString(hstrMod);
	g_pLTClient->FreeString(hstrSkin);
	g_pLTClient->FreeString(hstrHead);

	// Init player settings...

	CGameSettings* pSettings = m_InterfaceMgr.GetSettings();
	if (!pSettings) return;

    hWrite = g_pLTClient->StartMessage(MID_PLAYER_INITVARS);
    g_pLTClient->WriteToMessageByte(hWrite, (uint8)pSettings->RunLock());
    g_pLTClient->EndMessage(hWrite);

	if (IsHosting()) {
		// Send a signal to not lock server framerate. 
		// Dedicated servers don't send this, so it's a nice hack without backporting the dedicated server...
		hWrite = g_pLTClient->StartMessage(MID_DONT_LOCK_SERVER_FPS);
		g_pLTClient->WriteToMessageByte(hWrite, (uint8)LTTRUE);
		g_pLTClient->EndMessage(hWrite);
	}
}






// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::BuildClientSaveMsg
//
//	PURPOSE:	Save all the necessary client-side info
//
// --------------------------------------------------------------------------- //

void CGameClientShell::BuildClientSaveMsg(HMESSAGEWRITE hMessage)
{
	if (!hMessage) return;

	{ // BL 09/30/00 - HACK to save ammo type
		g_pLTClient->WriteToMessageFloat(hMessage, m_weaponModel.GetAmmo() ? m_weaponModel.GetAmmo()->nId : -1.0f);
		g_pLTClient->WriteToMessageDWord(hMessage, m_Music.GetMusicState()->nIntensity);
	}

    HMESSAGEWRITE hData = g_pLTClient->StartHMessageWrite();

	// Save complex data members...

	m_InterfaceMgr.Save(hData);
	m_MoveMgr.Save(hData);


	// Save all necessary data members...

    g_pLTClient->WriteToMessageRotation(hData, &m_rRotation);
    g_pLTClient->WriteToMessageVector(hData, &m_vFlashColor);

    g_pLTClient->WriteToMessageByte(hData, m_eDifficulty);
    g_pLTClient->WriteToMessageByte(hData, m_bFlashScreen);
    g_pLTClient->WriteToMessageByte(hData, m_bSpectatorMode);
    g_pLTClient->WriteToMessageByte(hData, m_bLastSent3rdPerson);
//    g_pLTClient->WriteToMessageByte(hData, m_bStartedDuckingDown);
//    g_pLTClient->WriteToMessageByte(hData, m_bStartedDuckingUp);
    g_pLTClient->WriteToMessageByte(hData, m_bAllowPlayerMovement);
    g_pLTClient->WriteToMessageByte(hData, m_bLastAllowPlayerMovement);
    g_pLTClient->WriteToMessageByte(hData, m_bWasUsingExternalCamera);
    g_pLTClient->WriteToMessageByte(hData, m_bUsingExternalCamera);
    g_pLTClient->WriteToMessageByte(hData, m_ePlayerState);
    g_pLTClient->WriteToMessageByte(hData, m_nCurrentLevel);
    g_pLTClient->WriteToMessageByte(hData, m_nCurrentMission);
    g_pLTClient->WriteToMessageByte(hData, m_nSoundFilterId);
    g_pLTClient->WriteToMessageByte(hData, m_nGlobalSoundFilterId);
    g_pLTClient->WriteToMessageByte(hData, m_weaponModel.GetHolster());
    g_pLTClient->WriteToMessageByte(hData, m_FlashLight.IsOn());

    g_pLTClient->WriteToMessageFloat(hData, m_fPitch);
    g_pLTClient->WriteToMessageFloat(hData, m_fYaw);
    g_pLTClient->WriteToMessageFloat(hData, m_fRoll);
    g_pLTClient->WriteToMessageFloat(hData, m_fPlayerPitch);
    g_pLTClient->WriteToMessageFloat(hData, m_fPlayerYaw);
    g_pLTClient->WriteToMessageFloat(hData, m_fPlayerRoll);

	m_fPitchBackup = m_fPitch;
	m_fYawBackup = m_fYaw;

    g_pLTClient->WriteToMessageFloat(hData, m_fPitchBackup);
    g_pLTClient->WriteToMessageFloat(hData, m_fYawBackup);

    g_pLTClient->WriteToMessageFloat(hData, m_fFlashTime);
    g_pLTClient->WriteToMessageFloat(hData, m_fFlashStart);
    g_pLTClient->WriteToMessageFloat(hData, m_fFlashRampUp);
    g_pLTClient->WriteToMessageFloat(hData, m_fFlashRampDown);
    g_pLTClient->WriteToMessageFloat(hData, m_fFireJitterPitch);
    g_pLTClient->WriteToMessageFloat(hData, m_fFireJitterYaw);
    g_pLTClient->WriteToMessageFloat(hData, m_fContainerStartTime);
    g_pLTClient->WriteToMessageFloat(hData, m_fFovXFXDir);
    g_pLTClient->WriteToMessageFloat(hData, m_fSaveLODScale);
//    g_pLTClient->WriteToMessageFloat(hData, m_fCamDuck);
//    g_pLTClient->WriteToMessageFloat(hData, m_fDuckDownV);
//    g_pLTClient->WriteToMessageFloat(hData, m_fDuckUpV);
//    g_pLTClient->WriteToMessageFloat(hData, m_fMaxDuckDistance);
//    g_pLTClient->WriteToMessageFloat(hData, m_fStartDuckTime);

    g_pLTClient->WriteToMessageHMessageWrite(hMessage, hData);
    g_pLTClient->EndHMessageWrite(hData);
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UnpackClientSaveMsg
//
//	PURPOSE:	Load all the necessary client-side info
//
// --------------------------------------------------------------------------- //

void CGameClientShell::UnpackClientSaveMsg(HMESSAGEREAD hMessage)
{
	if (!hMessage) return;

    m_bRestoringGame = LTTRUE;

    HMESSAGEREAD hData = g_pLTClient->ReadFromMessageHMessageRead(hMessage);


	// Load complex data members...

	m_InterfaceMgr.Load(hData);
	m_MoveMgr.Load(hData);

	// THE SAVE'S FLAGS LAND ON TOP OF THE CHEAT'S. The server restores the
	// player before the client is in the world, so its weapons and mods are
	// right; the client's copy of those flags arrives in THIS message, and
	// when it lands after the level-entry cheats it puts back whatever the
	// save held - a loadout with no scope, on a gun the server says has one.
	// The log says which came first; the re-mark makes the order not matter.
	VRLog::Msg("VRSave: client save data restored - the weapon, mod and gear"
			   " flags now come from the save");
	if (g_vtVRCheats.IsInitted() && (((int)g_vtVRCheats.GetFloat()) & 2))
	{
		int nMods = 0;
		const int nW = m_InterfaceMgr.GetPlayerStats()->VRHaveEverything(nMods);
		VRLog::Msg("VRSave: VRCheats everything - the client's flags re-marked"
				   " after the restore: %d player weapons, %d mods", nW, nMods);
	}

	// Load data members...

    g_pLTClient->ReadFromMessageRotation(hData, &m_rRotation);
    g_pLTClient->ReadFromMessageVector(hData, &m_vFlashColor);

    m_eDifficulty               = (GameDifficulty) g_pLTClient->ReadFromMessageByte(hData);
    m_bFlashScreen              = (LTBOOL) g_pLTClient->ReadFromMessageByte(hData);
    m_bSpectatorMode            = (LTBOOL) g_pLTClient->ReadFromMessageByte(hData);
    m_bLastSent3rdPerson        = (LTBOOL) g_pLTClient->ReadFromMessageByte(hData);
//    m_bStartedDuckingDown       = (LTBOOL) g_pLTClient->ReadFromMessageByte(hData);
//    m_bStartedDuckingUp         = (LTBOOL) g_pLTClient->ReadFromMessageByte(hData);
    m_bAllowPlayerMovement      = (LTBOOL) g_pLTClient->ReadFromMessageByte(hData);
    m_bLastAllowPlayerMovement  = (LTBOOL) g_pLTClient->ReadFromMessageByte(hData);
    m_bWasUsingExternalCamera   = (LTBOOL) g_pLTClient->ReadFromMessageByte(hData);
    m_bUsingExternalCamera      = (LTBOOL) g_pLTClient->ReadFromMessageByte(hData);
    m_ePlayerState              = (PlayerState) g_pLTClient->ReadFromMessageByte(hData);
    /*m_nCurrentLevel             = */SetCurrentLevel(g_pLTClient->ReadFromMessageByte(hData));
    /*m_nCurrentMission           = */SetCurrentMission(g_pLTClient->ReadFromMessageByte(hData));
    m_nSoundFilterId            = g_pLTClient->ReadFromMessageByte(hData);
    m_nGlobalSoundFilterId      = g_pLTClient->ReadFromMessageByte(hData);

	m_weaponModel.SetHolster(g_pLTClient->ReadFromMessageByte(hData));

	if (g_pLTClient->ReadFromMessageByte(hData))
	{
		m_FlashLight.TurnOn();
	}

    m_fPitch                    = g_pLTClient->ReadFromMessageFloat(hData);
    m_fYaw                      = g_pLTClient->ReadFromMessageFloat(hData);
    m_fRoll                     = g_pLTClient->ReadFromMessageFloat(hData);
    m_fPlayerPitch              = g_pLTClient->ReadFromMessageFloat(hData);
    m_fPlayerYaw                = g_pLTClient->ReadFromMessageFloat(hData);
    m_fPlayerRoll               = g_pLTClient->ReadFromMessageFloat(hData);
    m_fPitchBackup              = g_pLTClient->ReadFromMessageFloat(hData);
    m_fYawBackup                = g_pLTClient->ReadFromMessageFloat(hData);
    m_fFlashTime                = g_pLTClient->ReadFromMessageFloat(hData);
    m_fFlashStart               = g_pLTClient->ReadFromMessageFloat(hData);
    m_fFlashRampUp              = g_pLTClient->ReadFromMessageFloat(hData);
    m_fFlashRampDown            = g_pLTClient->ReadFromMessageFloat(hData);
    m_fFireJitterPitch          = g_pLTClient->ReadFromMessageFloat(hData);
    m_fFireJitterYaw	        = g_pLTClient->ReadFromMessageFloat(hData);
    m_fContainerStartTime       = g_pLTClient->ReadFromMessageFloat(hData);
    m_fFovXFXDir                = g_pLTClient->ReadFromMessageFloat(hData);
    m_fSaveLODScale             = g_pLTClient->ReadFromMessageFloat(hData);
//    m_fCamDuck                  = g_pLTClient->ReadFromMessageFloat(hData);
//    m_fDuckDownV                = g_pLTClient->ReadFromMessageFloat(hData);
//    m_fDuckUpV                  = g_pLTClient->ReadFromMessageFloat(hData);
//    m_fMaxDuckDistance          = g_pLTClient->ReadFromMessageFloat(hData);
//    m_fStartDuckTime            = g_pLTClient->ReadFromMessageFloat(hData);

    g_pLTClient->EndHMessageRead(hData);
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ProcessCheat
//
//	PURPOSE:	process a cheat.
//
// --------------------------------------------------------------------------- //

void CGameClientShell::ProcessCheat(CheatCode nCode)
{
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandleRespawn
//
//	PURPOSE:	Handle player respawn
//
// --------------------------------------------------------------------------- //

void CGameClientShell::HandleRespawn()
{
	if (!IsPlayerDead()) return;

	// if we're in multiplayer send the respawn command...

	if (IsMultiplayerGame())
	{
		// Don't send the respawn command if we are clicking around in
		// a menu...
		if (m_InterfaceMgr.GetGameState() != GS_MENU)
		{
			// send a message to the server telling it that it's ok to respawn us now...
	        HMESSAGEWRITE hMsg = g_pLTClient->StartMessage(MID_PLAYER_RESPAWN);
		    g_pLTClient->EndMessage(hMsg);
		}
	}
	else  // Bring up load game menu...
	{
		m_InterfaceMgr.SetLoadGameMenu();
	}
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::QuickSave
//
//	PURPOSE:	Quick save the game
//
// --------------------------------------------------------------------------- //

LTBOOL CGameClientShell::QuickSave()
{
	if (IsPlayerDead() || m_bUsingExternalCamera || GetGameType() != SINGLE ||
        m_InterfaceMgr.GetGameState() != GS_PLAYING) return LTFALSE;

	// Do quick save...

    HSTRING hStr = g_pLTClient->FormatString(IDS_QUICKSAVING);
    char* pStr = g_pLTClient->GetStringData(hStr);
    CSPrint(pStr);
    g_pLTClient->FreeString(hStr);

	time_t seconds;
	time (&seconds);
	struct tm* timedate = localtime (&seconds);
	if (!timedate) return 0;

	int missionNum = -1;
	int sceneNum = -1;

	if (!g_pGameClientShell->IsCustomLevel())
	{
		missionNum = g_pGameClientShell->GetCurrentMission();
		sceneNum = g_pGameClientShell->GetCurrentLevel();
	}

	char strSaveGame[256];
	sprintf (strSaveGame, "%s|%d,%d|%ld",m_strCurrentWorldName, missionNum, sceneNum, (long)seconds);

	CWinUtil::WinWritePrivateProfileString(GAME_NAME, "SaveGame00", strSaveGame, SAVEGAMEINI_FILENAME);

    m_bQuickSave = LTTRUE;

    return LTTRUE;
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::QuickLoad
//
//	PURPOSE:	Quick load the game
//
// --------------------------------------------------------------------------- //

LTBOOL CGameClientShell::QuickLoad()
{
    if (m_InterfaceMgr.GetGameState() != GS_PLAYING &&
		m_InterfaceMgr.GetGameState() != GS_FAILURE &&
		m_InterfaceMgr.GetCurrentFolder() != FOLDER_ID_FAILURE &&
		m_InterfaceMgr.GetCurrentFolder() != FOLDER_ID_SUMMARY) return LTFALSE;

	ClearScreenTint();

	char strSaveGameSetting[256];
	memset(strSaveGameSetting, 0, 256);
	CWinUtil::WinGetPrivateProfileString(GAME_NAME, "SaveGame00", "", strSaveGameSetting, 256, SAVEGAMEINI_FILENAME);

	if (!*strSaveGameSetting)
	{
        HSTRING hString = g_pLTClient->FormatString(IDS_NOQUICKSAVEGAME);
        m_InterfaceMgr.ShowMessageBox(hString,LTMB_OK,LTNULL);
		g_pLTClient->FreeString(hString);

        return LTFALSE;
	}

	char* strWorldName = strtok(strSaveGameSetting,"|");

	if (!LoadGame(strWorldName, QUICKSAVE_FILENAME))
	{
        HSTRING hString = g_pLTClient->FormatString(IDS_LOADGAMEFAILED);
        m_InterfaceMgr.ShowMessageBox(hString,LTMB_OK,LTNULL);
		g_pLTClient->FreeString(hString);

        return LTFALSE;
	}

    return LTTRUE;
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::IsMultiplayerGame()
//
//	PURPOSE:	See if we are playing a multiplayer game
//
// --------------------------------------------------------------------------- //

LTBOOL CGameClientShell::IsMultiplayerGame()
{
	int nGameMode = 0;
    g_pLTClient->GetGameMode(&nGameMode);
    if (nGameMode == STARTGAME_NORMAL || nGameMode == GAMEMODE_NONE) return LTFALSE;

    return LTTRUE;
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::IsHosting()
//
//	PURPOSE:	See if we are playing a multiplayer game
//
// --------------------------------------------------------------------------- //

LTBOOL CGameClientShell::IsHosting()
{
	int nGameMode = 0;
    g_pLTClient->GetGameMode(&nGameMode);
    if (nGameMode == STARTGAME_HOST || nGameMode == STARTGAME_HOSTTCP) return LTTRUE;

    return LTFALSE;
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::IsPlayerInWorld()
//
//	PURPOSE:	See if the player is in the world
//
// --------------------------------------------------------------------------- //

LTBOOL CGameClientShell::IsPlayerInWorld()
{
    HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();

    if (!m_bPlayerPosSet || !m_bInWorld || m_ePlayerState == PS_UNKNOWN || !hPlayerObj) return LTFALSE;

    return LTTRUE;
}


void CGameClientShell::GetCameraRotation(LTRotation *pRot)
{
    g_pLTClient->SetupEuler(pRot, m_fPitch, m_fYaw, m_fRoll);
}

void CGameClientShell::GetPlayerRotation(LTRotation *pRot)
{
    g_pLTClient->SetupEuler(pRot, m_fPlayerPitch, m_fPlayerYaw, m_fPlayerRoll);
}



// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::StartGame
//
//	PURPOSE:	Start a new game
//
// --------------------------------------------------------------------------- //

LTBOOL CGameClientShell::StartGame(GameDifficulty eDifficulty)
{
	SetDifficulty(eDifficulty);
	SetFadeBodies(m_bFadeBodies);
	DoStartGame();
    return LTTRUE;
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::DoStartGame
//
//	PURPOSE:	*Really* Start a new game
//
// --------------------------------------------------------------------------- //

void CGameClientShell::DoStartGame()
{
	SetCurrentMission(0);
	SetCurrentLevel(0);

	if (!LoadCurrentLevel())
	{
        g_pLTClient->CPrint("ERROR in CGameClientShell::DoStartGame():");
        g_pLTClient->CPrint("      Couldn't load the first mission!");
	}
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::StartMission
//
//	PURPOSE:	Start a new mission
//
// --------------------------------------------------------------------------- //

LTBOOL CGameClientShell::StartMission(int nMissionId)
{

	// if we're playing multiplayer, I know I'm about to disconnect so don't warn me
	if (IsInWorld() && GetGameType() != SINGLE)
		g_pInterfaceMgr->StartingNewGame();

	MISSION* pMission = g_pMissionMgr->GetMission(nMissionId);
    if (!pMission) return LTFALSE;

	SetCurrentMission(nMissionId);
	SetCurrentLevel(0);

	CPlayerStats* pStats = m_InterfaceMgr.GetPlayerStats();

	pStats->ClearMissionDamage();

	ObjectivesList* pObjList = pStats->GetObjectives();
	pObjList->Clear();

	pObjList = pStats->GetCompletedObjectives();
	pObjList->Clear();

	m_eGameType = SINGLE;
	m_InterfaceMgr.ChangeState(GS_LOADINGLEVEL);

	if (!LoadCurrentLevel())
	{
        g_pLTClient->CPrint("ERROR in CGameClientShell::StartMission():");
        g_pLTClient->CPrint("      Couldn't start mission %d!", m_nCurrentMission);
        m_InterfaceMgr.SwitchToFolder(g_pInterfaceMgr->GetMainFolder());

        return LTFALSE;
	}

    return LTTRUE;
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::LoadCurrentLevel
//
//	PURPOSE:	Handle loading the current level
//
// --------------------------------------------------------------------------- //

LTBOOL CGameClientShell::LoadCurrentLevel()
{
	MISSION* pMission = g_pMissionMgr->GetMission(m_nCurrentMission);
    if (!pMission) return LTFALSE;

    if (m_nCurrentLevel < 0 || m_nCurrentLevel > pMission->nNumLevels) return LTFALSE;

    uint8 nFlags = m_nCurrentLevel == 0 ? LOAD_NEW_GAME : LOAD_NEW_LEVEL;

    return LoadWorld(pMission->aLevels[m_nCurrentLevel].szLevel, LTNULL, LTNULL, nFlags);
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::GetNiceWorldName
//
//	PURPOSE:	Get the nice (level designer set) world name...
//
// --------------------------------------------------------------------------- //

LTBOOL CGameClientShell::GetNiceWorldName(char* pWorldFile, char* pRetName, int nRetLen)
{
    if (!pWorldFile || !pRetName || nRetLen < 2) return LTFALSE;

	char buf[_MAX_PATH];
	buf[0] = '\0';
	uint32 len;

	char buf2[_MAX_PATH];
	sprintf(buf2, "%s.dat", pWorldFile);

    LTRESULT dRes = g_pLTClient->GetWorldInfoString(buf2, buf, _MAX_PATH, &len);

	if (dRes != LT_OK || !buf[0] || len < 1)
	{
		// try pre-pending "worlds\" to the filename to see if it will find it then...
		// sprintf (buf2, "worlds\\%s.dat", pWorldFile);
        // dRes = g_pLTClient->GetWorldInfoString(buf2, buf, _MAX_PATH, &len);

		if (dRes != LT_OK || !buf[0] || len < 1)
		{
            return LTFALSE;
		}
	}


	char tokenSpace[5*(PARSE_MAXTOKENSIZE + 1)];
	char *pTokens[5];
	int nArgs;

	char* pCurPos = buf;
	char* pNextPos;

    LTBOOL bMore = LTTRUE;
	while (bMore)
	{
        bMore = g_pLTClient->Parse(pCurPos, &pNextPos, tokenSpace, pTokens, &nArgs);
		if (nArgs < 2) break;

		if (_stricmp(pTokens[0], "WORLDNAME") == 0)
		{
			strncpy(pRetName, pTokens[1], nRetLen);
            return LTTRUE;
		}

		pCurPos = pNextPos;
	}

    return LTFALSE;
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::OnModelKey
//
//	PURPOSE:	Handle weapon model keys
//
// --------------------------------------------------------------------------- //

void CGameClientShell::OnModelKey(HLOCALOBJ hObj, ArgList *pArgs)
{
	if (m_weaponModel.GetHandle() == hObj)
	{
		m_weaponModel.OnModelKey(hObj, pArgs);
	}
	else
	{
		m_sfxMgr.OnModelKey(hObj, pArgs);
	}
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::OnPlaySound
//
//	PURPOSE:	Handle a sound being played...
//
// --------------------------------------------------------------------------- //

void CGameClientShell::OnPlaySound(PlaySoundInfo* pPSI)
{
	if (!pPSI || !g_vtUseSoundFilters.GetFloat()) return;

	SOUNDFILTER* pFilter = g_pSoundFilterMgr->GetFilter((uint8)pPSI->m_UserData);
	if (!pFilter)
	{
		g_pLTClient->CPrint("ERROR in CGameClientShell::OnPlaySound()!");
		g_pLTClient->CPrint("  couldn't find filter (%d)", pPSI->m_UserData);
		return;
	}

	if (pFilter && pPSI->m_hSound)
	{
		// See if we need to calculate the filter dynamically...

		if (g_pSoundFilterMgr->IsDynamic(pFilter))
		{
			pFilter = GetDynamicSoundFilter();

			if (!pFilter)
			{
				g_pLTClient->CPrint("ERROR in CGameClientShell::OnPlaySound()!");
				g_pLTClient->CPrint("  couldn't find a dynamic filter (%s)", pFilter->szName);
				return;
			}
		}

		// Some sounds are unfiltered...

		if (g_pSoundFilterMgr->IsUnFiltered(pFilter)) return;


		// Set up the filter

		ILTClientSoundMgr *pSoundMgr = (ILTClientSoundMgr *)g_pLTClient->SoundMgr();

		pSoundMgr->SetSoundFilter(pPSI->m_hSound, pFilter->szFilterName);
		for (int i=0; i < pFilter->nNumVars; i++)
		{
			pSoundMgr->SetSoundFilterParam(pPSI->m_hSound, pFilter->szVars[i], pFilter->fValues[i]);
		}

		// TEMP, let us test what filter is being used...
		// g_pLTClient->CPrint("Using Filter: %s", pFilter->szName);
	}
	else
	{
		g_pLTClient->CPrint("ERROR in CGameClientShell::OnPlaySound()!");
		if (pFilter)
		{
			g_pLTClient->CPrint("  Invalid sound associated with Filter: %s", pFilter->szName);
		}
		else
		{
			g_pLTClient->CPrint("  Invalid filter associated with FilterId: %d", pPSI->m_UserData);
		}
	}
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::DoActivate
//
//	PURPOSE:	Tell the server to do Activate
//
// --------------------------------------------------------------------------- //

void CGameClientShell::DoActivate(LTBOOL bEditMode)
{
	// Don't allow activation when the zip cord is on...
	if (m_MoveMgr.IsZipCordOn()) return;

	char* pActivateOverride = g_vtActivateOverride.GetStr();
	if (pActivateOverride && pActivateOverride[0] != ' ')
	{
        g_pLTClient->RunConsoleString(pActivateOverride);
		return;
	}

    LTRotation rRot;
    LTVector vU, vR, vF, vPos;

	// ACTIVATE WHERE THE DOT IS. Outside the eye render the camera object
	// carries the BODY's rotation only - the head is applied inside
	// RenderWorldEyes and put back - so the retail ray below went along the
	// stick's facing, not where the player looks or points. In a headset the
	// head and the hand turn and the body does not, so a door looked at and
	// pointed at was missed whenever the body faced elsewhere. The fire ray
	// (hand origin, hand direction) is where the aim dot sits, and "put the
	// dot on it and press X" is a rule a player can see.
	bool bVRRay = false;
	if (m_PlayerCamera.IsFirstPerson() && VRShared::IsLive()
		&& VRShared::State().Hands[1].nActive && g_vtVRHandFire.GetFloat() > 0.0f)
	{
		LTVector vFU, vFR, vFF, vFPos;
		if (m_weaponModel.GetFireInfo(vFU, vFR, vFF, vFPos))
		{
			vPos = vFPos; vF = vFF; vU = vFU; vR = vFR;
			VEC_NORM(vF);
			bVRRay = true;
		}
	}

	if (bVRRay)
	{
		// A LOW TARGET WITHOUT A PRECISE AIM. A parked motorcycle's hit box
		// is 60 units tall and sits below the hand, and the game's reach is
		// 100 plus half the player's width. The desk log of a headset session
		// shows a press whose ray passed just over the bike and met an
		// invisible brush 336 units off, and a second at 30 degrees down that
		// hit it. When the hand's own ray meets no object within reach, the
		// same press tries steeper rays from the same origin, 10 degrees at a
		// time, and takes the first that meets a MODEL (vehicles, bodies,
		// things on the floor) within reach. A ray that already meets an
		// object is sent as it is. VRActivateAssist 0 turns this off.
		if (GetConsoleInt("VRActivateAssist", 1))
		{
			const float fReach = 124.0f;
			HOBJECT hMe = g_pLTClient->GetClientObject();
			HOBJECT hMove = m_MoveMgr.GetObject();
			// per ray: 0 nothing or the world, 1 an object that is not a model, 2 a model
			for (int nStep = 0; nStep <= 6; ++nStep)
			{
				LTVector vTry = vF;
				if (nStep > 0)
				{
					LTVector vH(vF.x, 0.0f, vF.z);
					const float fH = vH.Mag();
					if (fH < 0.01f) break;			// already straight down
					vH.x /= fH; vH.z /= fH;
					float fPitch = (float)asin(vF.y < -1.0f ? -1.0f : (vF.y > 1.0f ? 1.0f : vF.y))
								 - (float)nStep * (MATH_PI / 18.0f);
					if (fPitch < -1.4f) fPitch = -1.4f;
					vTry.x = vH.x * (float)cos(fPitch);
					vTry.z = vH.z * (float)cos(fPitch);
					vTry.y = (float)sin(fPitch);
				}
				IntersectQuery q; IntersectInfo ii;
				q.m_From  = vPos;
				q.m_To    = vPos + (vTry * fReach);
				q.m_Flags = INTERSECT_OBJECTS | INTERSECT_HPOLY;
				int nHit = 0;
				if (g_pLTClient->IntersectSegment(&q, &ii) && ii.m_hObject
					&& ii.m_hObject != hMe && ii.m_hObject != hMove)
				{
					// Only a model or a world model (a door, a switch) counts as
					// "something to use". A sprite does not: the aim dot sits
					// exactly where the ray lands and was found first, which
					// stopped the assist dead whenever the dot was within reach
					// (desk, 24 September, reaching for a letter on a desk).
					const uint32 nType = g_pLTClient->GetObjectType(ii.m_hObject);
					nHit = IsMainWorld(ii.m_hObject) ? 0
						 : (nType == OT_MODEL) ? 2
						 : (nType == OT_WORLDMODEL) ? 1 : 0;
				}
				if (nStep == 0)
				{
					if (nHit) break;				// the hand already meets something
					continue;
				}
				if (nHit == 2)
				{
					VRLog::Msg("Activate: assist - the hand ray met nothing within %.0f;"
						" %d degrees lower meets a model at %.0f", fReach, nStep * 10,
						(ii.m_Point - vPos).Mag());
					vF = vTry;
					break;
				}
			}
		}
	}
	else if (m_PlayerCamera.IsFirstPerson())
	{
		g_pLTClient->GetObjectPos(m_hCamera, &vPos);
		g_pLTClient->GetObjectRotation(m_hCamera, &rRot);
		g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);
	}
	else  // Use player pos/rot
	{
		HLOCALOBJ hPlayerObj = g_pLTClient->GetClientObject();
		if (!hPlayerObj) return;

		g_pLTClient->GetObjectPos(hPlayerObj, &vPos);
		g_pLTClient->GetObjectRotation(hPlayerObj, &rRot);
		g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);
	}

    HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_PLAYER_ACTIVATE);
    g_pLTClient->WriteToMessageVector(hMessage, &vPos);
    g_pLTClient->WriteToMessageVector(hMessage, &vF);
    g_pLTClient->WriteToMessageByte(hMessage, bEditMode);
    g_pLTClient->EndMessage(hMessage);

	// WHAT THE ACTIVATE RAY MET, as the client sees it. The server casts its
	// own ray from these two vectors and says nothing back when it misses,
	// so a desk run pressing X in front of a vehicle could not tell "missed"
	// from "not sent". A client cast along the same line, objects included
	// and non-solids NOT ignored (a vehicle is FLAG_CLIENTNONSOLID), names
	// the nearest thing on it.
	{
		IntersectQuery q; IntersectInfo ii;
		q.m_From  = vPos;
		q.m_To    = vPos + (vF * 512.0f);
		q.m_Flags = INTERSECT_OBJECTS | INTERSECT_HPOLY;
		const char* pHit = "nothing within 512";
		float fDist = 0.0f;
		char szModel[128]; szModel[0] = 0;
		if (g_pLTClient->IntersectSegment(&q, &ii))
		{
			fDist = (ii.m_Point - vPos).Mag();
			if (IsMainWorld(ii.m_hObject)) pHit = "the world";
			else
			{
				pHit = "an object";
				LTVector vO, vD;
				g_pLTClient->GetObjectPos(ii.m_hObject, &vO);
				g_pLTClient->Physics()->GetObjectDims(ii.m_hObject, &vD);
				sprintf(szModel, "type %d at (%.0f %.0f %.0f) dims (%.0f %.0f %.0f)",
					(int)g_pLTClient->GetObjectType(ii.m_hObject), vO.x, vO.y, vO.z, vD.x, vD.y, vD.z);
			}
		}
		LTVector vMe; g_pLTClient->GetObjectPos(g_pLTClient->GetClientObject(), &vMe);
		VRLog::Msg("Activate: from (%.0f %.0f %.0f) along (%+.2f %+.2f %+.2f) %s; player at (%.0f %.0f %.0f); hit %s at %.0f %s",
			vPos.x, vPos.y, vPos.z, vF.x, vF.y, vF.z, bVRRay ? "(hand ray)" : "(camera ray)",
			vMe.x, vMe.y, vMe.z, pHit, fDist, szModel);
	}
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::CreateBoundingBox
//
//	PURPOSE:	Create a box around the MoveMgr object
//
// --------------------------------------------------------------------------- //

void CGameClientShell::CreateBoundingBox()
{
	if (m_hBoundingBox) return;

	HLOCALOBJ hMoveMgrObj = m_MoveMgr.GetObject();
	if (!hMoveMgrObj) return;

	ObjectCreateStruct theStruct;
	INIT_OBJECTCREATESTRUCT(theStruct);

    LTVector vPos;
    g_pLTClient->GetObjectPos(hMoveMgrObj, &vPos);
	VEC_COPY(theStruct.m_Pos, vPos);

	SAFE_STRCPY(theStruct.m_Filename, "Models\\1x1_square.abc");
	SAFE_STRCPY(theStruct.m_SkinName, "SpecialFX\\smoke.dtx");
	theStruct.m_ObjectType = OT_MODEL;
	theStruct.m_Flags = FLAG_VISIBLE | FLAG_MODELWIREFRAME;

    m_hBoundingBox = g_pLTClient->CreateObject(&theStruct);

	UpdateBoundingBox();
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateBoundingBox()
//
//	PURPOSE:	Update the bounding box
//
// ----------------------------------------------------------------------- //

void CGameClientShell::UpdateBoundingBox()
{
	if (!m_hBoundingBox) return;

	HLOCALOBJ hMoveMgrObj = m_MoveMgr.GetObject();
	if (!hMoveMgrObj) return;

    LTVector vPos;
    g_pLTClient->GetObjectPos(hMoveMgrObj, &vPos);
    g_pLTClient->SetObjectPos(m_hBoundingBox, &vPos);

    LTVector vDims;
    g_pLTClient->Physics()->GetObjectDims(hMoveMgrObj, &vDims);

    LTVector vScale;
	VEC_DIVSCALAR(vScale, vDims, 0.5f);
    g_pLTClient->SetObjectScale(m_hBoundingBox, &vScale);
}

// --------------------------------------------------------------------------- //
// Called by the engine, saves all variables (console and member variables)
// related to demo playback.
// --------------------------------------------------------------------------- //
void LoadConVar(ILTClient *g_pLTClient, ILTStream *pStream, char *pVarName)
{
	float val;
	char cString[512];

	(*pStream) >> val;
	sprintf(cString, "%s %f", pVarName, val);
    g_pLTClient->RunConsoleString(cString);
}

void SaveConVar(ILTClient *g_pLTClient, ILTStream *pStream, char *pVarName, float defaultVal)
{
	HCONSOLEVAR hVar;
	float val;

	val = defaultVal;
    if(hVar = g_pLTClient->GetConsoleVar (pVarName))
	{
        val = g_pLTClient->GetVarValueFloat (hVar);
	}

	(*pStream) << val;
}

void CGameClientShell::DemoSerialize(ILTStream *pStream, LTBOOL bLoad)
{
	CGameSettings* pSettings = m_InterfaceMgr.GetSettings();
	if (!pSettings) return;

	if (bLoad)
	{
		g_vtNormalTurnRate.Load(pStream);
		g_vtFastTurnRate.Load(pStream);
		g_vtLookUpRate.Load(pStream);

		m_HeadBobMgr.DemoLoad(pStream);

		pSettings->LoadDemoSettings(pStream);
	}
	else
	{
		g_vtNormalTurnRate.Save(pStream);
		g_vtFastTurnRate.Save(pStream);
		g_vtLookUpRate.Save(pStream);

		m_HeadBobMgr.DemoSave(pStream);

		pSettings->SaveDemoSettings(pStream);
	}
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	LoadLeakFile
//
//	PURPOSE:	Loads a leak file and creates a line system for it.
//
// --------------------------------------------------------------------------- //

LTBOOL LoadLeakFile(ILTClient *g_pLTClient, char *pFilename)
{
	FILE *fp;
	char line[256];
	HLOCALOBJ hObj;
	ObjectCreateStruct cStruct;
    LTLine theLine;
	int nRead;

	fp = fopen(pFilename, "rt");
	if(fp)
	{
		INIT_OBJECTCREATESTRUCT(cStruct);
		cStruct.m_ObjectType = OT_LINESYSTEM;
		cStruct.m_Flags = FLAG_VISIBLE;
        hObj = g_pLTClient->CreateObject(&cStruct);
		if(!hObj)
		{
			fclose(fp);
            return LTFALSE;
		}

		while(fgets(line, 256, fp))
		{
			nRead = sscanf(line, "%f %f %f %f %f %f",
				&theLine.m_Points[0].m_Pos.x, &theLine.m_Points[0].m_Pos.y, &theLine.m_Points[0].m_Pos.z,
				&theLine.m_Points[1].m_Pos.x, &theLine.m_Points[1].m_Pos.y, &theLine.m_Points[1].m_Pos.z);

			// White
			theLine.m_Points[0].r = theLine.m_Points[0].g = theLine.m_Points[0].b = 1;
			theLine.m_Points[0].a = 1;

			// Read
			theLine.m_Points[1].r = 1;
			theLine.m_Points[1].g = theLine.m_Points[1].b = 0;
			theLine.m_Points[1].a = 1;

            g_pLTClient->AddLine(hObj, &theLine);
		}

		fclose(fp);
        return LTTRUE;
	}
	else
	{
        return LTFALSE;
	}
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	ConnectToTcpIpAddress
//
//	PURPOSE:	Connects (joins) to the given tcp/ip address
//
// --------------------------------------------------------------------------- //

LTBOOL ConnectToTcpIpAddress(ILTClient* pClientDE, char* sAddress)
{
	// Sanity checks...

    if (!g_pLTClient) return(LTFALSE);
    if (!sAddress) return(LTFALSE);

/*
	// Try to connect to the given address...

    LTBOOL db = NetStart_DoConsoleConnect(g_pLTClient, sAddress);

	if (!db)
	{
        if (strlen(sAddress) <= 0) g_pLTClient->CPrint("Unable to connect");
        else g_pLTClient->CPrint("Unable to connect to %s", sAddress);
        return(LTFALSE);
	}


	// All done...

    if (strlen(sAddress) > 0) g_pLTClient->CPrint("Connected to %s", sAddress);
    else g_pLTClient->CPrint("Connected");

    return(LTTRUE);
	*/
    return LTFALSE;
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	NVModelHook
//
//	PURPOSE:	Special Rendering Code for NightVision Powerup
//
// --------------------------------------------------------------------------- //

void NVModelHook (ModelHookData *pData, void *pUser)
{
	CGameClientShell* pShell = (CGameClientShell*) pUser;
	if (!pShell) return;

    uint32 nUserFlags = 0;
    g_pLTClient->GetObjectUserFlags (pData->m_hObject, &nUserFlags);
	if (nUserFlags & USRFLG_NIGHT_INFRARED)
	{
		pData->m_Flags &= ~MHF_USETEXTURE;
		if (pData->m_LightAdd)
		{
			VEC_COPY(*pData->m_LightAdd , g_vNVModelColor);
		}
	}
	else
	{

		DefaultModelHook(pData, pUser);
	}
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	IRModelHook
//
//	PURPOSE:	Special Rendering Code for Infrared Powerup
//
// --------------------------------------------------------------------------- //

void IRModelHook (ModelHookData *pData, void *pUser)
{
	CGameClientShell* pShell = (CGameClientShell*) pUser;
	if (!pShell) return;

    uint32 nUserFlags = 0;
    g_pLTClient->GetObjectUserFlags (pData->m_hObject, &nUserFlags);
	if (nUserFlags & USRFLG_NIGHT_INFRARED)
	{
		pData->m_Flags &= ~MHF_USETEXTURE;
		if (pData->m_LightAdd)
		{
			// *(pData->m_LightAdd) = g_vIRModelColor;
			*(pData->m_ObjectColor) = g_vIRModelColor;
			pData->m_ObjectFlags |= FLAG_NOLIGHT;
		}
	}
	else
	{
		DefaultModelHook(pData, pUser);
	}
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	DefaultModelHook
//
//	PURPOSE:	Default model hook function
//
// --------------------------------------------------------------------------- //

void DefaultModelHook (ModelHookData *pData, void *pUser)
{

	CGameClientShell* pShell = (CGameClientShell*) pUser;
	if (!pShell) return;

    uint32 nUserFlags = 0;
    g_pLTClient->GetObjectUserFlags (pData->m_hObject, &nUserFlags);

	if (nUserFlags & USRFLG_GLOW)
	{
		if (pData->m_LightAdd)
		{
			*pData->m_LightAdd = pShell->GetModelGlow();
			VEC_CLAMP((*pData->m_LightAdd), 0.0f, 255.0f);
		}
	}
	else if (nUserFlags & USRFLG_MODELADD)
	{
		// Get the new color out of the upper 3 bytes of the
		// user flags...

        LTFLOAT r = (LTFLOAT)(nUserFlags>>24);
        LTFLOAT g = (LTFLOAT)(nUserFlags>>16);
        LTFLOAT b = (LTFLOAT)(nUserFlags>>8);

		if (pData->m_LightAdd)
		{
			VEC_SET (*pData->m_LightAdd, r, g, b);
			VEC_CLAMP((*pData->m_LightAdd), 0.0f, 255.0f);
		}
	}
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	HookedWindowProc
//
//	PURPOSE:	Hook it real good
//
// --------------------------------------------------------------------------- //

LRESULT CALLBACK HookedWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!g_pfnMainWndProc)
	{
		SDL_Log("Invalid window proc call <%d>, ignoring", uMsg);
		return 0;
	}

	// IN VR THE DESKTOP WINDOW'S FOCUS IS NOT THE PLAYER'S. The engine answers
	// every focus change by unloading and reloading its renderer and releasing
	// and reacquiring its sound device - legacy of 2000's exclusive fullscreen.
	// In a headset that meant a blank view for a second or two on every
	// alt-tab or notification, and the sound library's mixer thread racing
	// the release: mss32.dll faulted reading freed memory, reproduced at the
	// desk on the first or second alt-tab (24 September). The four focus
	// messages go to Windows' own handling instead of the engine's while the
	// headset is live, so focus still moves (typing still reaches the window)
	// and the engine never hears about it. VRKeepFocus 0 restores the stock
	// behaviour.
	// IN VR MODE, NOT ONLY ONCE THE HEADSET IS LIVE: before the host has a
	// session - the player starting the game first, or the host's "waiting
	// for your headset" notice taking the foreground - the same race is there,
	// and our renderer never needs the engine's reload. Only the retail
	// d3d.ren (VRStereo 0, a flat control run) keeps the stock behaviour.
	if (uMsg == WM_ACTIVATEAPP || uMsg == WM_ACTIVATE || uMsg == WM_KILLFOCUS || uMsg == WM_SETFOCUS)
	{
		static VarTrack s_vtKeepFocus;
		if (!s_vtKeepFocus.IsInitted() && g_pLTClient)
			s_vtKeepFocus.Init(g_pLTClient, "VRKeepFocus", LTNULL, 1.0f);
		if (s_vtKeepFocus.IsInitted() && s_vtKeepFocus.GetFloat() > 0.0f
			&& (VRShared::IsLive() || GetConsoleInt("VRStereo", 0) > 0))
		{
			static int s_nSaid = 0;
			if (s_nSaid < 40)
			{
				++s_nSaid;
				const char* pszMsg = (uMsg == WM_ACTIVATEAPP) ? "WM_ACTIVATEAPP"
								   : (uMsg == WM_ACTIVATE)    ? "WM_ACTIVATE"
								   : (uMsg == WM_KILLFOCUS)   ? "WM_KILLFOCUS" : "WM_SETFOCUS";
				VRLog::Msg("VRFocus: %s (wParam %u) kept from the engine (VR mode%s)",
						   pszMsg, (unsigned)wParam, VRShared::IsLive() ? ", headset live" : "");
			}
			if (uMsg == WM_ACTIVATEAPP) return 0;
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
		}
	}

	switch(uMsg)
	{
		HANDLE_MSG(hWnd, WM_LBUTTONUP, CGameClientShell::OnLButtonUp);
		HANDLE_MSG(hWnd, WM_LBUTTONDOWN, CGameClientShell::OnLButtonDown);
		HANDLE_MSG(hWnd, WM_LBUTTONDBLCLK, CGameClientShell::OnLButtonDblClick);
		HANDLE_MSG(hWnd, WM_RBUTTONUP, CGameClientShell::OnRButtonUp);
		HANDLE_MSG(hWnd, WM_RBUTTONDOWN, CGameClientShell::OnRButtonDown);
		HANDLE_MSG(hWnd, WM_RBUTTONDBLCLK, CGameClientShell::OnRButtonDblClick);
		HANDLE_MSG(hWnd, WM_MOUSEMOVE, CGameClientShell::OnMouseMove);
		HANDLE_MSG(hWnd, WM_CHAR, CGameClientShell::OnChar);
		HANDLE_MSG(hWnd, WM_SETCURSOR, OnSetCursor);
	}
	_ASSERT(g_pfnMainWndProc);
	return(CallWindowProc(g_pfnMainWndProc,hWnd,uMsg,wParam,lParam));
}

void CGameClientShell::OnChar(HWND hWnd, char c, int rep)
{
	// DID A CHARACTER EVER ARRIVE? with a headset on, T
	// opens "Say:" and nothing can be typed into it. Those are two different
	// roads and only one was fixed - the bind rides DirectInput, which the
	// proxy now acquires in BACKGROUND, while the TEXT rides WM_CHAR, and a
	// window message only reaches a window holding keyboard FOCUS.
	//
	// This line is the far end of that road. If the host's MIRROR KEY lines say
	// a character was posted and this one never appears, the message is being
	// posted to the wrong window; if neither appears, focus is on some third
	// window and nothing is reaching either of us. Those want opposite fixes,
	// and without this line they look identical from the outside.
	{
		static int s_nSaidChar = 0;
		if (s_nSaidChar < 16)
		{
			++s_nSaidChar;
			VRLog::Msg("VRChar: WM_CHAR reached the game - '%c' (0x%02X)",
				(c >= 32 && c < 127) ? c : '.', (unsigned char)c);
		}
	}
	g_pInterfaceMgr->OnChar(c);
}


void CGameClientShell::OnLButtonUp(HWND hWnd, int x, int y, UINT keyFlags)
{
	g_pInterfaceMgr->OnLButtonUp(x,y);
}

void CGameClientShell::OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
	/*if (!g_tmrDblClick.Stopped() &&
		(g_mouseMgr.GetClickPosX() == x) && (g_mouseMgr.GetClickPosY() == y))
	{
		g_tmrDblClick.Stop();
		OnLButtonDblClick(hwnd,fDoubleClick,x,y,keyFlags);
	}
	else
	{
		g_mouseMgr.SetClickPos(x,y);
		g_tmrDblClick.Start(.5);
	}*/

	g_pInterfaceMgr->OnLButtonDown(x,y);
}

void CGameClientShell::OnLButtonDblClick(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
	g_pInterfaceMgr->OnLButtonDblClick(x,y);
}

void CGameClientShell::OnRButtonUp(HWND hwnd, int x, int y, UINT keyFlags)
{
	g_pInterfaceMgr->OnRButtonUp(x,y);
}

void CGameClientShell::OnRButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
	g_pInterfaceMgr->OnRButtonDown(x,y);
}

void CGameClientShell::OnRButtonDblClick(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
	g_pInterfaceMgr->OnRButtonDblClick(x,y);
}

void CGameClientShell::OnMouseMove(HWND hwnd, int x, int y, UINT keyFlags)
{
	//g_mouseMgr.SetMousePos(x,y);

	g_pInterfaceMgr->OnMouseMove(x,y);
}

BOOL OnSetCursor(HWND hwnd, HWND hwndCursor, UINT codeHitTest, UINT msg)
{
	return TRUE;
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	HookWindow
//
//	PURPOSE:	HOOK IT!
//
// --------------------------------------------------------------------------- //

BOOL HookWindow()
{
	// Get the screen dimz
    HSURFACE hScreen = g_pLTClient->GetScreenSurface();
	if(!hScreen)
		return FALSE;

	// Hook the window
    if(g_pLTClient->GetEngineHook("HWND",(void **)&g_hMainWnd) != LT_OK)
	{
		TRACE("HookWindow - ERROR - could not get the engine window!\n");
		return FALSE;
	}

	// Get the window procedure
#ifdef STRICT
	g_pfnMainWndProc = (WNDPROC)GetWindowLong(g_hMainWnd,GWL_WNDPROC);
#else
	g_pfnMainWndProc = (FARPROC)GetWindowLong(g_hMainWnd,GWL_WNDPROC);
#endif

	if(!g_pfnMainWndProc)
	{
		TRACE("HookWindow - ERROR - could not get the window procedure from the engine window!\n");
		return FALSE;
	}

	// Replace it with ours
	if(!SetWindowLong(g_hMainWnd,GWL_WNDPROC,(LONG)HookedWindowProc))
	{
		TRACE("HookWindow - ERROR - could not set the window procedure!\n");
		return FALSE;
	}

	// Clip the cursor if we're NOT in a window
    HCONSOLEVAR hVar = g_pLTClient->GetConsoleVar("Windowed");
	BOOL bClip = TRUE;
	if(hVar)
	{
        float fVal = g_pLTClient->GetVarValueFloat(hVar);
		if(fVal == 1.0f)
			bClip = FALSE;
	}

	if(bClip)
	{
        uint32 dwScreenWidth = g_pGameClientShell->GetScreenWidth();
        uint32 dwScreenHeight = g_pGameClientShell->GetScreenHeight();

		SetWindowLong(g_hMainWnd,GWL_STYLE,WS_VISIBLE);
		SetWindowPos(g_hMainWnd,HWND_TOPMOST,0,0,dwScreenWidth,dwScreenHeight,SWP_FRAMECHANGED);
		RECT wndRect;
		GetWindowRect(g_hMainWnd, &wndRect);
		ClipCursor(&wndRect);
	}

	g_SDLWindow = SDL_CreateWindowFrom(g_hMainWnd);

	if(g_SDLWindow) {
		SDL_Log("Hooked window!");

		// If they request it, don't use raw input!
		if (g_vtNoRawInput.GetFloat()) 
		{
			SDL_Log("No Raw Input requested.");
			SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
		}

		// Centre the window please.
		SDL_SetWindowPosition(g_SDLWindow, SDL_WINDOWPOS_CENTERED , SDL_WINDOWPOS_CENTERED);	
	} else {
		SDL_Log("Error hooking window: %s", SDL_GetError());
	}

	return TRUE;
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	UnhookWindow
//
//	PURPOSE:	Unhook the window
//
// --------------------------------------------------------------------------- //

void UnhookWindow()
{
	// ORDER MATTERS, and it used to be the wrong way round.
	//
	// HookWindow subclasses the window TWICE: it installs HookedWindowProc, and
	// then SDL_CreateWindowFrom subclasses it again, saving HookedWindowProc as
	// the proc IT will restore. Undoing those in the same order they were made
	// meant SetWindowLong put the engine's proc back, and SDL_DestroyWindow then
	// overwrote it with the HookedWindowProc it had saved - so the window ended
	// teardown pointing into this DLL. The next message after CShell.dll
	// unloaded jumped into freed memory, faulting at HookedWindowProc+0x0 in
	// cshell.dll_unloaded. See docs/EXIT-CRASH.md.
	//
	// Unwind in reverse: let SDL put HookedWindowProc back first, then restore
	// the engine's proc over the top of it.
	if(g_SDLWindow) {
		SDL_DestroyWindow(g_SDLWindow);
		g_SDLWindow = NULL;
	}

	if(g_pfnMainWndProc && g_hMainWnd)
	{
		SetWindowLong(g_hMainWnd, GWL_WNDPROC, (LONG)g_pfnMainWndProc);
		g_hMainWnd = 0;
		g_pfnMainWndProc = NULL;
	}
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::SetDifficulty
//
//	PURPOSE:	Dynamically change our difficulty level
//
// --------------------------------------------------------------------------- //

void CGameClientShell::SetDifficulty(GameDifficulty e)
{
	m_eDifficulty = e;
	WriteConsoleInt("Difficulty",(int)e);

    HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_DIFFICULTY);
    g_pLTClient->WriteToMessageByte(hMessage, m_eDifficulty);
    g_pLTClient->EndMessage(hMessage);
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::SetFadeBodies
//
//	PURPOSE:	Dynamically change our difficulty level
//
// --------------------------------------------------------------------------- //

void CGameClientShell::SetFadeBodies(LTBOOL bFade)
{
	m_bFadeBodies = bFade;

    HMESSAGEWRITE hMessage = g_pLTClient->StartMessage(MID_FADEBODIES);
    g_pLTClient->WriteToMessageByte(hMessage, (uint8)m_bFadeBodies);
    g_pLTClient->EndMessage(hMessage);
}



// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandleWeaponPickup()
//
//	PURPOSE:	Handle picking up weapon
//
// ----------------------------------------------------------------------- //

void CGameClientShell::HandleWeaponPickup(uint8 nWeaponID)
{
	WEAPON* pWeapon = g_pWeaponMgr->GetWeapon(nWeaponID);
	if (!pWeapon) return;

	// Don't display information about picking up melee weapons...

	AMMO* pAmmo = g_pWeaponMgr->GetAmmo(pWeapon->nDefaultAmmoType);
	if (!pAmmo) return;

	if (pAmmo->eInstDamageType == DT_MELEE) return;


	int nNameId = pWeapon->nNameId;
	if (!nNameId) return;

    HSTRING hStr = g_pLTClient->FormatString(nNameId);
	if (!hStr) return;

    char* pStr = g_pLTClient->GetStringData(hStr);
	if (!pStr)
	{
        g_pLTClient->FreeString(hStr);
		return;
	}

    HSURFACE hSurf = LTNULL;
	if (strlen(pWeapon->szSmallIcon))
		hSurf = g_pLTClient->CreateSurfaceFromBitmap(pWeapon->szSmallIcon);
	else
		hSurf = g_pLTClient->CreateSurfaceFromBitmap(pWeapon->szIcon);
	if (!hSurf)
        hSurf = g_pLTClient->CreateSurfaceFromBitmap("interface\\missingslot.pcx");

    HSTRING hMsg = g_pLTClient->FormatString(IDS_GUNPICKUP,pStr);
	m_InterfaceMgr.GetMessageMgr()->AddLine(hMsg,MMGR_PICKUP,hSurf);
    g_pLTClient->FreeString(hStr);

    LTVector vTintColor = g_pLayoutMgr->GetWeaponPickupColor();
    LTFLOAT  fTotalTime = g_pLayoutMgr->GetTintTime();

    LTFLOAT fRampDown = fTotalTime * 0.85f;
    LTFLOAT fRampUp   = fTotalTime * 0.10f;
    LTFLOAT fTintTime = fTotalTime * 0.05f;

    LTVector vCamPos;
    g_pLTClient->GetObjectPos(m_hCamera, &vCamPos);

    LTRotation rRot;
    g_pLTClient->GetObjectRotation(m_hCamera, &rRot);

    LTVector vU, vR, vF;
    g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);

	VEC_MULSCALAR(vF, vF, 10.0f);
	VEC_ADD(vCamPos, vCamPos, vF);

    g_pGameClientShell->FlashScreen(vTintColor, vCamPos, 1000.0f, fRampUp, fTintTime, fRampDown, LTTRUE);

}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandleGearPickup()
//
//	PURPOSE:	Handle picking up gear
//
// ----------------------------------------------------------------------- //

void CGameClientShell::HandleGearPickup(uint8 nGearId)
{
	GEAR* pGear = g_pWeaponMgr->GetGear(nGearId);
	if (!pGear) return;

	int nNameId = pGear->nNameId;
	if (!nNameId) return;

    HSTRING hStr = g_pLTClient->FormatString(nNameId);
	if (!hStr) return;

    char* pStr = g_pLTClient->GetStringData(hStr);
	if (!pStr)
	{
        g_pLTClient->FreeString(hStr);
		return;
	}

    HSURFACE hSurf = LTNULL;
	if (strlen(pGear->szSmallIcon))
		hSurf = g_pLTClient->CreateSurfaceFromBitmap(pGear->szSmallIcon);
	else
		hSurf = g_pLTClient->CreateSurfaceFromBitmap(pGear->szIcon);

	if (!hSurf)
        hSurf = g_pLTClient->CreateSurfaceFromBitmap("interface\\missingslot.pcx");


    HSTRING hMsg = g_pLTClient->FormatString(IDS_GEARPICKUP,pStr);
	m_InterfaceMgr.GetMessageMgr()->AddLine(hMsg,MMGR_PICKUP, hSurf);
    g_pLTClient->FreeString(hStr);

    LTVector vTintColor = pGear->vScreenTintColor;
    LTFLOAT  fTotalTime = pGear->fScreenTintTime;

    LTFLOAT fRampDown = fTotalTime * 0.85f;
    LTFLOAT fRampUp   = fTotalTime * 0.10f;
    LTFLOAT fTintTime = fTotalTime * 0.05f;


    LTVector vCamPos;
    g_pLTClient->GetObjectPos(m_hCamera, &vCamPos);

    LTRotation rRot;
    g_pLTClient->GetObjectRotation(m_hCamera, &rRot);

    LTVector vU, vR, vF;
    g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);

	VEC_MULSCALAR(vF, vF, 10.0f);
	VEC_ADD(vCamPos, vCamPos, vF);

    g_pGameClientShell->FlashScreen(vTintColor, vCamPos, 1000.0f, fRampUp, fTintTime, fRampDown, LTTRUE);

}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandleModPickup()
//
//	PURPOSE:	Handle picking up mod
//
// ----------------------------------------------------------------------- //

void CGameClientShell::HandleModPickup(uint8 nModId)
{
	MOD* pMod = g_pWeaponMgr->GetMod(nModId);
	if (!pMod) return;

	int nNameId = pMod->nNameId;
	if (!nNameId) return;

    HSTRING hStr = g_pLTClient->FormatString(nNameId);
	if (!hStr) return;

    char* pStr = g_pLTClient->GetStringData(hStr);
	if (!pStr)
	{
        g_pLTClient->FreeString(hStr);
		return;
	}

    HSURFACE hSurf = LTNULL;
	if (strlen(pMod->szSmallIcon))
		hSurf = g_pLTClient->CreateSurfaceFromBitmap(pMod->szSmallIcon);
    else
		hSurf = g_pLTClient->CreateSurfaceFromBitmap(pMod->szIcon);
	if (!hSurf)
        hSurf = g_pLTClient->CreateSurfaceFromBitmap("interface\\missingslot.pcx");

    HSTRING hMsg = g_pLTClient->FormatString(IDS_MODPICKUP,pStr);
	m_InterfaceMgr.GetMessageMgr()->AddLine(hMsg, MMGR_PICKUP, hSurf);
    g_pLTClient->FreeString(hStr);

    LTVector vTintColor = pMod->vScreenTintColor;
    LTFLOAT  fTotalTime = pMod->fScreenTintTime;

    LTFLOAT fRampDown = fTotalTime * 0.85f;
    LTFLOAT fRampUp   = fTotalTime * 0.10f;
    LTFLOAT fTintTime = fTotalTime * 0.05f;


    LTVector vCamPos;
    g_pLTClient->GetObjectPos(m_hCamera, &vCamPos);

    LTRotation rRot;
    g_pLTClient->GetObjectRotation(m_hCamera, &rRot);

    LTVector vU, vR, vF;
    g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);

	VEC_MULSCALAR(vF, vF, 10.0f);
	VEC_ADD(vCamPos, vCamPos, vF);

    g_pGameClientShell->FlashScreen(vTintColor, vCamPos, 1000.0f, fRampUp, fTintTime, fRampDown, LTTRUE);

}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandleAmmoPickup()
//
//	PURPOSE:	Handle picking up ammo
//
// ----------------------------------------------------------------------- //

void CGameClientShell::HandleAmmoPickup(uint8 nAmmoId, int nAmmoCount)
{
	AMMO* pAmmo = g_pWeaponMgr->GetAmmo(nAmmoId);
	if (!pAmmo) return;

	int nNameId = pAmmo->nNameId;
	if (!nNameId) return;

    HSTRING hStr = g_pLTClient->FormatString(nNameId);
	if (!hStr) return;

    char* pStr = g_pLTClient->GetStringData(hStr);
	if (!pStr)
	{
        g_pLTClient->FreeString(hStr);
		return;
	}

    HSURFACE hSurf = g_pLTClient->CreateSurfaceFromBitmap(pAmmo->szSmallIcon);
	if (!hSurf)
        hSurf = g_pLTClient->CreateSurfaceFromBitmap("interface\\missingslot.pcx");

    HSTRING hMsg = g_pLTClient->FormatString(IDS_AMMOPICKUP, nAmmoCount, pStr);
	m_InterfaceMgr.GetMessageMgr()->AddLine(hMsg,MMGR_PICKUP, hSurf);
    g_pLTClient->FreeString(hStr);

    LTVector vTintColor = g_pLayoutMgr->GetAmmoPickupColor();
    LTFLOAT  fTotalTime = g_pLayoutMgr->GetTintTime();

    LTFLOAT fRampDown = fTotalTime * 0.85f;
    LTFLOAT fRampUp   = fTotalTime * 0.10f;
    LTFLOAT fTintTime = fTotalTime * 0.05f;


    LTVector vCamPos;
    g_pLTClient->GetObjectPos(m_hCamera, &vCamPos);

    LTRotation rRot;
    g_pLTClient->GetObjectRotation(m_hCamera, &rRot);

    LTVector vU, vR, vF;
    g_pLTClient->GetRotationVectors(&rRot, &vU, &vR, &vF);

	VEC_MULSCALAR(vF, vF, 10.0f);
	VEC_ADD(vCamPos, vCamPos, vF);

    g_pGameClientShell->FlashScreen(vTintColor, vCamPos, 1000.0f, fRampUp, fTintTime, fRampDown, LTTRUE);
}




// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::UpdateCameraSway
//
//	PURPOSE:	Update the camera's sway
//
// --------------------------------------------------------------------------- //

void CGameClientShell::UpdateCameraSway()
{
	if (g_pInterfaceMgr->GetSunglassMode() != SUN_NONE) return;

	// Apply...
    LTFLOAT swayAmount = m_fFrameTime / 1000.0f;

    LTFLOAT tm  = g_pLTClient->GetTime()/10.0f;

	// Adjust if ducking...
	LTFLOAT fMult = (m_dwPlayerFlags & BC_CFLG_DUCK) ? g_vtCameraSwayDuckMult.GetFloat() : 1.0f;

    LTFLOAT faddP = fMult * g_vtCameraSwayYSpeed.GetFloat() * (float)sin(tm*g_vtCameraSwayYFreq.GetFloat()) * swayAmount;
    LTFLOAT faddY = fMult * g_vtCameraSwayXSpeed.GetFloat() * (float)sin(tm*g_vtCameraSwayXFreq.GetFloat()) * swayAmount;

	m_fPitch += faddP;
	m_fYaw	 += faddY;
}




// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandleWeaponDisable
//
//	PURPOSE:	Handle the weapon being disabled...
//
// --------------------------------------------------------------------------- //

void CGameClientShell::HandleWeaponDisable(LTBOOL bDisabled)
{
	if (bDisabled)
	{
		ClearScreenTint();
        HandleZoomChange(m_weaponModel.GetWeaponId(), LTTRUE);
		EndZoom();
	}
	else
	{
		// Force us to re-evaluate what container we're in.  We call
		// UpdateContainerFX() first to make sure any container changes
		// have been accounted for, then we clear the container code
		// and force an update (this is done for underwater situations like
		// dying underwater and respawning, and also for picking up intelligence
		// items underwater)...

		if (IsPlayerInWorld())
		{
			UpdateContainerFX();
			ClearCurContainerCode();
			UpdateContainerFX();
		}

        m_weaponModel.SetVisible(LTTRUE);
	}
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::HandleMissionFailed
//
//	PURPOSE:	Handle mission failure
//
// --------------------------------------------------------------------------- //

void CGameClientShell::HandleMissionFailed()
{
	ClearScreenTint();
    HandleZoomChange(m_weaponModel.GetWeaponId(), LTTRUE);
	EndZoom();
	m_InterfaceMgr.EndUnderwater();

	m_InterfaceMgr.ForceScreenFadeIn(g_vtScreenFadeInTime.GetFloat());

	m_InterfaceMgr.MissionFailed(IDS_YOUWEREKILLED);
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::AttachCameraToHead
//
//	PURPOSE:	Attach the camera to a socket in the player's head
//
// --------------------------------------------------------------------------- //

void CGameClientShell::AttachCameraToHead(LTBOOL bAttach)
{
	m_bCameraAttachedToHead = bAttach;
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::GetPlayerHeadPos
//
//	PURPOSE:	Get the player's head position
//
// --------------------------------------------------------------------------- //

void CGameClientShell::GetPlayerHeadPosRot(LTVector & vPos, LTRotation & rRot)
{
	HMODELSOCKET hSocket = INVALID_MODEL_SOCKET;
	HOBJECT hBody = LTNULL; // g_pLTClient->GetClientObject();

	// We actually want to use the body prop, so...

	CSpecialFXList* pList = m_sfxMgr.GetFXList(SFX_BODY_ID);
	if (!pList) return;

	int nNumBodies = pList->GetSize();

    uint32 dwId;
    g_pLTClient->GetLocalClientID(&dwId);

	for (int i=0; i < nNumBodies; i++)
	{
		if ((*pList)[i])
		{
			CBodyFX* pBody = (CBodyFX*)(*pList)[i];

			if (pBody->GetClientId() == dwId)
			{
				hBody = (*pList)[i]->GetServerObj();
				break;
			}
		}
	}

	if (hBody)
	{
		if (g_pModelLT->GetSocket(hBody, "Eyes", hSocket) == LT_OK)
		{
			LTransform transform;
            if (g_pModelLT->GetSocketTransform(hBody, hSocket, transform, LTTRUE) == LT_OK)
			{
				g_pTransLT->Get(transform, vPos, rRot);
				CalcNonClipPos(vPos, rRot);
			}
		}
	}
}


// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::SetFarZ
//
//	PURPOSE:	Localize setting the far z (cause it can screw things up
//				when set to 0)
//
// --------------------------------------------------------------------------- //

void CGameClientShell::SetFarZ(int nFarZ)
{
	// Don't EVER set the farZ really close!
	if (nFarZ > 50)
	{
		WriteConsoleInt("FarZ", nFarZ);
	}
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ClearCurContainerCode
//
//	PURPOSE:	Clear our current container info.
//
// --------------------------------------------------------------------------- //

void CGameClientShell::ClearCurContainerCode()
{
	m_eCurContainerCode = CC_NO_CONTAINER;
	m_nSoundFilterId = 0;
}

// returns LTTRUE if the passed in address matches the current server address
LTBOOL CGameClientShell::CheckServerAddress(char *pszTestAddress, int nPort)
{
	if (!pszTestAddress) return LTFALSE;
	if (nPort != m_nServerPort) return LTFALSE;
	return (stricmp(pszTestAddress,m_szServerAddress) == 0);
}


void QuickLoadCallBack(LTBOOL bReturn, void *pData)
{
	if (bReturn)
	{
		g_pGameClientShell->QuickLoad();
	}
}


void CGameClientShell::DoTaunt(uint32 nClientID,uint8 nTaunt)
{
	if (m_ePlayerState != PS_ALIVE) return;

	//if you're not listening to taunts, you're not allowed to send them
	if (GetConsoleInt("IgnoreTaunts",0) > 0) return;

	CClientInfoMgr *pCIMgr = m_InterfaceMgr.GetClientInfoMgr();
	if (!pCIMgr) return;

	CLIENT_INFO *pInfo = pCIMgr->GetLocalClient();
	if (!pInfo) return;

	char szVar[16] = "";

	switch (pInfo->team)
	{
	case 0:
		sprintf(szVar,"TauntDM%d",nTaunt);
		break;
	case 1:
		sprintf(szVar,"TauntUnity%d",nTaunt);
		break;
	case 2:
		sprintf(szVar,"TauntHARM%d",nTaunt);
		break;
	}

	uint32 nTauntID = (uint32)GetConsoleInt(szVar,0);

	if (!nTauntID) return;

	// Don't allow the client to flood the server with taunts...

	CCharacterFX *pFX = m_MoveMgr.GetCharacterFX();
	if (pFX && !pFX->IsPlayingTaunt())
	{
		pFX->PlayTaunt(nTauntID);

		HMESSAGEWRITE hWrite = g_pLTClient->StartMessage(MID_PLAYER_TAUNT);
		g_pLTClient->WriteToMessageDWord(hWrite, nTauntID);
		g_pLTClient->EndMessage(hWrite);
	}
}

LTBOOL CGameClientShell::DoJoinGame(char* sIp)
{
	// Sanity checks...

	if (!sIp) return(LTFALSE);
	if (!g_pLTClient) return(LTFALSE);

	g_pLTClient->CPrint("Joining multi-player game");

	// Start the game...

	StartGameRequest req;
	NetClientData clientData;

	memset( &req, 0, sizeof( req ));


	// Setup our client...

	clientData.m_dwTeam = (uint32)GetConsoleInt("NetPlayerTeam",0);
	SAFE_STRCPY(clientData.m_sName,g_vtPlayerName.GetStr());

	req.m_pClientData = &clientData;
	req.m_ClientDataLen = sizeof( clientData );
	req.m_Type = STARTGAME_CLIENTTCP;
	strncpy(req.m_TCPAddress, sIp, MAX_SGR_STRINGLEN);


	// Try to join a game...

    LTRESULT dr = g_pLTClient->InitNetworking(NULL, 0);
	if (dr != LT_OK)
	{
		g_pLTClient->CPrint("InitNetworking() : error %d", dr);
        return(LTFALSE);
	}


	// [blg] we don't know it yet. g_pGameClientShell->SetGameType((GameType)nType);
	g_pInterfaceMgr->DrawFragCount(LTFALSE);
	g_pInterfaceMgr->ChangeState(GS_LOADINGLEVEL);

	int nRetries = GetConsoleInt("NetJoinRetry", 0);
	int nMaxRetries = nRetries + 1;
	while (nRetries >= 0)
	{
		g_pLTClient->CPrint("[Attempt %d] Trying to start game!", nMaxRetries - nRetries);

		// If successful, then we're done.
        if( g_pLTClient->StartGame( &req ) == LT_OK )
		{
			g_pLTClient->CPrint("Multiplayer game started!");

            return(LTTRUE);
		}

		// Wait a sec and try again.
		Sleep(1000);
		nRetries--;
	}

	// All done...
    return(LTFALSE);
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::SetDisconnectCode
//
//	PURPOSE:	Sets the disconnection code and message
//
// --------------------------------------------------------------------------- //

void CGameClientShell::SetDisconnectCode(uint32 nCode, const char *pMsg, uint32 nSubCode)
{
	// Don't override what someone already told us
	if (m_nDisconnectCode)
		return;

	m_nDisconnectCode = nCode;
	m_nDisconnectSubCode = nSubCode;
	if (m_pDisconnectMsg)
		debug_deletea(m_pDisconnectMsg);
	m_pDisconnectMsg = debug_newa(char, strlen(pMsg) + 1);
	strcpy(m_pDisconnectMsg, pMsg);
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::ClearDisconnectCode
//
//	PURPOSE:	Clears the disconnection code and message
//
// --------------------------------------------------------------------------- //

void CGameClientShell::ClearDisconnectCode()
{
	m_nDisconnectCode = 0;
	m_nDisconnectSubCode = 0;
	if (m_pDisconnectMsg)
		debug_deletea(m_pDisconnectMsg);
	m_pDisconnectMsg = LTNULL;
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::GetDisconnectCode
//
//	PURPOSE:	Retrieves the disconnection code
//
// --------------------------------------------------------------------------- //

uint32 CGameClientShell::GetDisconnectCode()
{
	return m_nDisconnectCode;
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::GetDisconnectSubCode
//
//	PURPOSE:	Retrieves the disconnection sub-code
//
// --------------------------------------------------------------------------- //

uint32 CGameClientShell::GetDisconnectSubCode()
{
	return m_nDisconnectSubCode;
}

// --------------------------------------------------------------------------- //
//
//	ROUTINE:	CGameClientShell::GetDisconnectMsg
//
//	PURPOSE:	Retrieves the disconnection message
//
// --------------------------------------------------------------------------- //

const char *CGameClientShell::GetDisconnectMsg()
{
	if (m_pDisconnectMsg)
		return m_pDisconnectMsg;
	else
		return LTNULL;
}



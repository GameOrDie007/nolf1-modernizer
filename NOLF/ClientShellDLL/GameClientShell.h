// ----------------------------------------------------------------------- //
//
// MODULE  : GameClientShell.h
//
// PURPOSE : Game Client Shell - Definition
//
// CREATED : 9/18/97
//
// (c) 1997-2000 Monolith Productions, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //

#ifndef __GAME_CLIENT_SHELL_H__
#define __GAME_CLIENT_SHELL_H__

#include "iclientshell.h"
#include "PlayerCamera.h"
#include "ClientServerShared.h"
#include "SFXMgr.h"
#include "Music.h"
#include "ContainerCodes.h"
#include "WeaponModel.h"
#include "LightScaleMgr.h"
#include "GlobalClientMgr.h"
#include "CMoveMgr.h"
#include "DamageFXMgr.h"
#include "ScreenTintMgr.h"
#include "ObjEditMgr.h"
#include "InterfaceMgr.h"
#include "AttachButeMgr.h"
#include "FlashLight.h"
#include "SharedMovement.h"
#include "PlayerSummary.h"
#include "IntelItemMgr.h"
#include "CameraOffsetMgr.h"
#include "HeadBobMgr.h"
#include "NetDefs.h"
#include "OptimizedRenderer.h"
#include "JukeboxButeMgr.h"

#define DEG2RAD(x)		(((x)*MATH_PI)/180.0f)
#define RAD2DEG(x)		(((x)*180.0f)/MATH_PI)

class CSpecialFX;
class CCameraFX;
class CGameTexMgr;


// For speedrunners
static uint8 g_nCurrentMission = 255;
static uint8 g_nCurrentLevel = 255;

class CGameClientShell : public IClientShell
{
	public:

		CGameClientShell();
		~CGameClientShell();

		int	  GetScreenWidth();
		int	  GetScreenHeight();

        HLTSOUND PlaySoundLocal(char *szSound, BOOL bLoop = FALSE, BOOL bStream = FALSE, BOOL bGetHandle = FALSE);

        void  ProcessCheat(CheatCode nCode);
        void  ShakeScreen(LTVector vAmount);
        void  PauseGame(LTBOOL bPause, LTBOOL bPauseSound=LTFALSE);

		void  HandleRecord(int argc, char **argv);
		void  HandlePlaydemo(int argc, char **argv);
		void  HandleCheat(int argc, char **argv);
		void  HandleExitLevel(int argc, char **argv);

		void  ExitLevel();

        LTBOOL LoadGame(char* pWorld, char* pObjectsFile);
        LTBOOL SaveGame(char* pObjectsFile);

        LTBOOL QuickSave();
        LTBOOL QuickLoad();

        LTBOOL GetNiceWorldName(char* pWorldFile, char* pRetName, int nRetLen);
        LTBOOL StartGame(GameDifficulty eDifficulty);

        LTBOOL LoadWorld(char* pWorldFile, char* pCurWorldSaveFile=LTNULL,
                        char* pRestoreWorldFile=LTNULL, uint8 nFlags=LOAD_NEW_GAME);
        LTBOOL StartMission(int nMissionId);
		void   InitMultiPlayer();

		int	GetMPMissionName() const {return m_nMPNameId;}
		int	GetMPMissionBriefing() const {return m_nMPBriefingId;}

        LTBOOL IsCustomLevel()   const {return m_bIsCustomLevel;}
		int GetCurrentMission() const {return m_nCurrentMission;}
		int GetCurrentLevel()	const {return m_nCurrentLevel;}

		void SetCurrentMission(int mission) {
			m_nCurrentMission = mission;
			g_nCurrentMission = mission;
		}

		void SetCurrentLevel(int level) {
			m_nCurrentLevel = level;
			g_nCurrentLevel = level;
		}

		LTBOOL DoJoinGame(char* sIpAddress);

		//returns LTTRUE if the passed in address matches the current server address
		LTBOOL CheckServerAddress(char *pszTestAddress, int nPort);

		void			SetDifficulty(GameDifficulty e);
		GameDifficulty  GetDifficulty()					{return m_eDifficulty;}
		void			SetFadeBodies(LTBOOL bFade);
		LTBOOL			GetFadeBodies()					{return m_bFadeBodies;}
		GameType		GetGameType()					{return m_eGameType;}
		void			SetGameType(GameType eGameType)	{m_eGameType = eGameType;}
		LevelEnd		GetLevelEnd()					{return m_eLevelEnd;}
		int				GetLevelEndString()				{return m_nEndString;}

        void  DoActivate(LTBOOL bEditMode);

		void  SetFarZ(int nFarZ);

        LTFLOAT             GetFrameTime()              { return m_fFrameTime; }
        CPlayerSummaryMgr*  GetPlayerSummary()          { return &m_PlayerSummary; }
        CIntelItemMgr*		GetIntelItemMgr()			{ return &m_IntelItemMgr; }
		CWeaponModel*		GetWeaponModel()			{ return &m_weaponModel; }
		CMoveMgr*			GetMoveMgr()				{ return &m_MoveMgr; }
		CDamageFXMgr*		GetDamageFXMgr()			{ return &m_DamageFXMgr; }
		ContainerCode		GetCurContainerCode() const { return m_eCurContainerCode; }
		CPlayerStats*		GetPlayerStats()			{ return m_InterfaceMgr.GetPlayerStats(); }

        CScreenTintMgr*     GetScreenTintMgr()          { return &m_ScreenTintMgr; }
		CLightScaleMgr*		GetLightScaleMgr()			{ return &m_LightScaleMgr; }

        CAttachButeMgr*     GetAttachButeMgr()          { return &m_AttachButeMgr; }
		CCameraOffsetMgr*	GetCameraOffsetMgr()		{ return &m_CameraOffsetMgr; }

		CJukeboxButeMgr* GetJukeboxButeMgr()			{ return &m_JukeBoxButeMgr; }

		HLOCALOBJ		GetCamera()				const	{ return m_hCamera; }
		HLOCALOBJ		GetInterfaceCamera()	const	{ return m_hInterfaceCamera; }
		// Square the interface camera up before a card folder places its 3D
		// pieces (CBaseFolder::CreateInterfaceSFX). See the definition.
		void			VRSquareInterfaceCameraFor(int nFolderId);
        LTBOOL			IsUsingExternalCamera()	const	{ return m_bUsingExternalCamera; }
        LTBOOL          CanSaveGame()			const   { return (m_bInWorld && !m_bUsingExternalCamera); }
        LTBOOL          IsInWorld()				const   { return m_bInWorld; }
        LTBOOL          IsGamePaused()			const   { return m_bGamePaused; }
        LTBOOL          IsFirstUpdate()			const   { return m_bFirstUpdate; }
		char*			GetCurrentWorldName()			{ return m_strCurrentWorldName; }
        LTBOOL          SoundInited()			const   { return ( m_resSoundInit == LT_OK ) ? LTTRUE : LTFALSE; }
		LTBOOL			IsCameraListener()		const	{ return m_bCamIsListener; }

        void		MinimizeMainWindow()            { m_bMainWindowMinimized = LTTRUE; }
        LTBOOL		IsMainWindowMinimized()         { return m_bMainWindowMinimized; }
        void		RestoreMainWindow()             { m_bMainWindowMinimized = LTFALSE; }
        LTBOOL		IsSpectatorMode()               { return m_bSpectatorMode; }

		LTBOOL		InCameraGadgetRange(HOBJECT hObj);

		void		AllowPlayerMovement(LTBOOL bAllowPlayerMovement);

        LTBOOL      IsPlayerMovementAllowed()       { return m_bAllowPlayerMovement;}
        int         VRScopeZoomLevel() const;   // the game's zoom level, for the scope pass
        LTBOOL      IsZoomed()          const       { return (m_nZoomView > 0 || m_bZooming); }
        LTBOOL      IsUnderwater()      const       { return IsLiquid(m_eCurContainerCode); }
        LTBOOL      UsingNightVision()  const       { return m_bNightVision; }

        LTBOOL      IsFirstPerson()                 { return m_PlayerCamera.IsFirstPerson(); }

		CPlayerCamera* GetPlayerCamera() { return &m_PlayerCamera; }

		CMusic*		GetMusic()		{ return &m_Music; }
		void		InitSound();

		void		ToggleDebugCheat(CheatCode eCheat);
        void        ShowPlayerPos(LTBOOL bShow=LTTRUE)	{ m_bShowPlayerPos = bShow; }
        void        ShowCamPosRot(LTBOOL bShow=LTTRUE)  { m_bShowCamPosRot = bShow; }
        void        SetSpectatorMode(LTBOOL bOn=LTTRUE);
        void        SetPlayerNotInWorld()				{ m_bInWorld = LTFALSE; }

		void		ClearAllScreenBuffers();
		void		ClearCurContainerCode();

        LTBOOL      IsJoystickEnabled();
        LTBOOL      EnableJoystick();

		CSFXMgr*	GetSFXMgr() { return &m_sfxMgr; }

        void        FlashScreen(LTVector vFlashColor, LTVector vPos, LTFLOAT fFlashRange,
                               LTFLOAT fTime, LTFLOAT fRampUp, LTFLOAT fRampDown,
                               LTBOOL bForce=LTFALSE);

		void		BeginMineMode();
		void		EndMineMode();

		void		BeginInfrared();
		void		EndInfrared();

		void		UpdateModelGlow();
        LTVector&   GetModelGlow() {return m_vCurModelGlow;}

        LTBOOL      IsMultiplayerGame();
		LTBOOL		IsHosting();
        LTBOOL      IsPlayerInWorld();
		// The loading screen is drawn by its own thread while this one loads the
		// level, so the per-tick publish never runs for it. CLoadingScreen::Show
		// calls this once, on the main thread, before that thread starts.
		void		VRPublishLoadingScreen();

		PlayerState	GetPlayerState()	const { return m_ePlayerState; }
        LTBOOL      IsPlayerDead()      const { return (m_ePlayerState == PS_DEAD || m_ePlayerState == PS_DYING); }

        // THE 2D CROSSHAIR AND THE WORLD MARKER ARE THE SAME ANSWER TWICE.
        //
        // The game's own crosshair is blitted at the centre of the screen,
        // which is the VIEW axis; once the controller aims the weapon that is
        // not where the shot goes. This says the world marker is up and the
        // flat one should stand down. It does NOT touch IsCrosshairOn():
        // that is a game STATE (the player's option, the 3rd-person logic,
        // and one of the marker's own enable conditions), and clearing it
        // would switch the marker off along with the cross.
        LTBOOL      VRHidesGameCrosshair() const;

        // Blood, bullet holes and shell casings stay instead of fading. One
        // decision, so one switch: VRPersistentFX, default on.
        LTBOOL      VRPersistentFX() const;

        uint32      GetPlayerFlags()    const { return m_dwPlayerFlags; }

        void        GetCameraRotation(LTRotation *pRot);
        void        GetPlayerRotation(LTRotation *pRot);
        void        GetPlayerPitchYawRoll(LTVector & vVec) const { vVec.x = m_fPlayerPitch; vVec.y = m_fPlayerYaw; vVec.z = m_fPlayerRoll; }

        void        HandleWeaponPickup(uint8 nWeaponId);
        void        HandleGearPickup(uint8 nGearId);
        void        HandleModPickup(uint8 nModId);
        void        HandleAmmoPickup(uint8 nAmmoId, int nAmmoCount);

		void		HandleMissionFailed();

        void        HandleWeaponDisable(LTBOOL bDisabled);

        void        ChangeWeapon(uint8 nWeaponId, uint8 nAmmoId, uint32 dwAmmo);
        void        DemoSerialize(ILTStream *pStream, LTBOOL bLoad);
		void		CSPrint(char* msg, ...);

        void        SetInputState(LTBOOL bAllowInput);

        LTBOOL      PreChangeGameState(GameState eNewState);
        LTBOOL      PostChangeGameState(GameState eOldState);

		void		UpdatePlayerFlags();

		// Mouse Messages
        static void OnChar(HWND hWnd, char c, int rep);
		static void OnLButtonUp(HWND hwnd, int x, int y, UINT keyFlags);
		static void OnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
		static void OnLButtonDblClick(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
		static void OnRButtonUp(HWND hwnd, int x, int y, UINT keyFlags);
		static void OnRButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
		static void OnRButtonDblClick(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
		static void OnMouseMove(HWND hwnd, int x, int y, UINT keyFlags);

        LTFLOAT     GetPitch()  const { return m_fPitch; }
        LTFLOAT     GetYaw()    const { return m_fYaw; }
        LTFLOAT     GetRoll()   const { return m_fRoll; }

        LTFLOAT     GetPlayerPitch()    const { return m_fPlayerPitch; }
        LTFLOAT     GetPlayerYaw()      const { return m_fPlayerYaw; }
        LTFLOAT     GetPlayerRoll()     const { return m_fPlayerRoll; }

		// These should only be called after first getting the value
		// from the GetXXX() functions above...

        void        SetPitch(LTFLOAT fPitch)     { m_fPitch  = fPitch; }
        void        SetYaw(LTFLOAT fYaw)         { m_fYaw    = fYaw; }
        void        SetRoll(LTFLOAT fRoll)       { m_fRoll   = fRoll; }

        void        SetPlayerPitch(LTFLOAT fPitch)   { m_fPlayerPitch = fPitch; }
        void        SetPlayerYaw(LTFLOAT fYaw)       { m_fPlayerYaw   = fYaw; }
        void        SetPlayerRoll(LTFLOAT fRoll)     { m_fPlayerRoll  = fRoll; }

		// Called when the engine wants to tell the game a disconnection code (a.k.a. hack)
		virtual void SetDisconnectCode(uint32 nCode, const char *pMsg, uint32 nSubCode = 0);
		// Internal game-side support for the disconnection code
		void		ClearDisconnectCode();
		uint32		GetDisconnectCode();
		uint32		GetDisconnectSubCode();
		const char *GetDisconnectMsg();

		const char *GetServerAddress() const {return m_szServerAddress;}
		const char *GetServerName() const {return m_szServerName;}
		const LTFLOAT *GetServerOptions() const {return m_fServerOptions;}

		void SetFramerateLock(LTBOOL bLock) { m_bLockFramerate = bLock; }
		void UpdateConfigSettings();

		void ClearBindings();

	protected :

        uint32      OnEngineInitialized(RMode *pMode, LTGUID *pAppGuid);
		void		OnEngineTerm();
        void        OnEvent(uint32 dwEventID, uint32 dwParam);
        LTRESULT    OnObjectMove(HOBJECT hObj, LTBOOL bTeleport, LTVector *pPos);
        LTRESULT    OnObjectRotate(HOBJECT hObj, LTBOOL bTeleport, LTRotation *pNewRot);
        LTRESULT    OnTouchNotify(HOBJECT hMain, CollisionInfo *pInfo, float forceMag);
		void		PreLoadWorld(char *pWorldName);
		void		OnEnterWorld();
		void		OnExitWorld();
		void		PreUpdate();
		void		Update();
		void		PostUpdate();
		void		UpdatePlaying();
		void		OnCommandOn(int command);
		void		OnCommandOff(int command);

		// Turns the headset's controllers into the commands and keys the game
		// already understands. A member because it needs the game state, and
		// m_InterfaceMgr has no public accessor.
		void		VRUpdateControllerInput();
		// Typing into the chat line while the game is not the foreground
		// window - which is its normal state with a headset on. See the note
		// at the definition.
		void		VRTypeWhenUnfocused();
		// Defined only when VR_DEBUG_TOOLS is 1 (GameClientShell.cpp), and
		// called only from there - the macro lives in the .cpp, after this
		// header is included, so guarding the declaration with it would
		// compile the declaration out and the definition in.
		void			VRDebugFireDrive();
		// Test only: select a weapon on world entry, retried until the server
		// has actually handed it over. See VRDebugWeapon.
		void		VRDebugSelectWeapon();
		// True when the player's own stats say the player carries a firearm - the only
		// way to tell whether the arsenal cheat actually landed.
		bool		VRHasAnyGun();

		// Hand the renderer the engine's model list once a frame. A member for
		// the same reason as above: it needs the game state and the camera.
		void		VRUpdateVehicleBody();
		void		VRPublishModels();
		// True when the interface scene is the whole picture and no level may
		// show behind it: a full-card folder, any folder once the player has
		// left the world (a failed mission), or the loading screen while it is
		// being published. See VRIsCardFolder.
		bool		VRCardScene();
		void		OnKeyDown(int key, int rep);
		void		OnKeyUp(int key);
        void        SpecialEffectNotify(HLOCALOBJ hObj, HMESSAGEREAD hMessage);
		void		OnObjectRemove(HLOCALOBJ hObj);
        void        OnMessage(uint8 messageID, HMESSAGEREAD hMessage);
		void		OnModelKey(HLOCALOBJ hObj, ArgList *pArgs);
		void		OnPlaySound(PlaySoundInfo* pPlaySoundInfo);
        void        SetMouseInput(LTBOOL bAllowInput);
        void        ShowPlayer(LTBOOL bShow=LTTRUE);

		void		UpdateServerPlayerModel();
        void        RenderCamera(LTBOOL bDrawInterface = LTTRUE);

        // VR: draws the world once or twice, optionally offset per eye into
        // side-by-side viewports. Separate function so it can use
        // __try/__finally - camera state must be restored even on a structured
        // exception (a project rule), and SEH will not run C++ destructors.
        void        RenderWorldEyes(int nWorldRenders, LTBOOL bSideBySide);

        // Points one eye's camera at its own optical centre. The Quest's
        // lenses are canted, so each eye's frustum centre is offset from its
        // forward axis; the host declares the matching rotation.
        void        ApplyEyeOpticalCentre(const LTRotation& rBase, int nEye);

        // Stamps the frame with which host pose it was rendered from, so the
        // host can stop guessing how stale each captured image is.
        enum { kMarkerBlock = 8 };
    public:
        // Public: the folder path (InterfaceMgr) paints it for the paused world.
        void        DrawFrameMarker(uint32 nHostFrame);
    protected:

        // A crosshair per eye. The game's own is drawn once across the window
        // and lands on the stereo seam, so neither eye gets a usable one.
        void        DrawVRCrosshair(int nHalfWidth, int nHeight);

        // Measures the horizontal field the renderer actually produces, by
        // yawing a known angle and finding how far the image moved. Replaces
        // guessing at VRFovXTest.
        void        CalibrateFovX();

        // The same measurement on the VERTICAL axis: pitch a known angle and
        // correlate along columns. Never measured before 31 August, and the
        // prime suspect for the warping - if the renderer scales the two axes
        // differently the picture handed to the runtime is anisotropic, which
        // bends the world as the head turns and is invisible to VRFovXTest.
        void        CalibrateFovY();

        // Both of the above. bVertical picks the axis; everything else -
        // the band, the four rotations, the linearity check - is shared, so
        // the two axes cannot drift apart as instruments.
        void        CalibrateAxis(LTBOOL bVertical);

        // CalibrateFovX behind a structured-exception guard. It had never once
        // been called before 23 August, so it is treated as untested code.
        void        CalibrateFovXGuarded();

        // CalibrateFovY behind the same guard.
        void        CalibrateFovYGuarded();

        // Writes the eye viewport to a BMP beside the run log, so what the
        // calibration reads can be LOOKED AT instead of inferred from a
        // standard deviation.
        //
        // Every failure of this measurement since 23 August has been reported
        // as one number - "reference band: sd 0.0" - which cannot separate a
        // blank wall from a failed surface read from a correlation looking at
        // the wrong rows. Those need opposite fixes and have each been acted
        // on wrongly at least once. One image settles all three at a glance.
        // Subsampled, because this reads a pixel at a time.
        void        DumpEyeImage(const char* pszTag);

        // Dumps whatever is ALREADY in the stash surface, with no render and
        // no copy of its own.
        //
        // Separate from the above because the two answer different questions.
        // DumpEyeImage asks "can this engine read a frame back at all"; this
        // one asks "what did the calibration actually correlate", and it must
        // not disturb the surface to do it. A dump that re-renders would
        // answer its own question rather than the calibration's.
        void        DumpStashImage(const char* pszTag, HSURFACE hStash,
                                   int nWidth, int nHeight, int nStepIn = 4,
                                   int nY0In = 0, int nY1In = -1);

        // Runs the calibration at several asked fields and reports whether the
        // renderer's tangent ratio is CONSTANT.
        //
        // 0.733 was measured at one asked field, 45 degrees half, because that
        // is the game's normal FOV flat. The VR path asks for about 70. If the
        // renderer derives the horizontal from the vertical and the viewport
        // aspect rather than applying a fixed tangent scale, the ratio varies
        // and a VRFovXTest derived at 45 is wrong at 70 - which would waste a
        // headset round. Neither this nor the vertical needs a headset.
        void        SweepCameraField();

        // Called from every exit path of the sweep, including the early ones.
        void        QuitAfterSweepIfAsked();

        // The heading the sweep turned the camera away from, so it can be put
        // back on every exit path including the early ones (a project rule).
        LTRotation  m_rSweepSavedRot;
        LTBOOL      m_bSweepRotSaved;

        // The tangent ratio the last calibration produced, or 0 if it did not
        // produce a usable one. Written by CalibrateAxis, read by the sweep.
        float       m_fLastFieldRatio;

        // The focal length in pixels the last calibration measured, or 0.
        //
        // Reported alongside the ratio because the two hypotheses that have
        // never been separated are distinguished by it directly: a renderer
        // applying a fixed tangent scale gives a focal that tracks 1/tan(asked),
        // while one that ignores the asked horizontal and derives it from the
        // vertical gives the SAME focal at every asked field.
        float       m_fLastFieldFocal;

        // Set for one calibration at a time, so the sweep writes two images
        // rather than thirty-two.
        LTBOOL      m_bDumpCalibPasses;

        // Asks the engine's own projection what field a given FOV and viewport
        // produce, via Get3DCameraPt. No render, no headset, no judgement -
        // and it reports the engine's belief, which may not be what d3d.ren
        // draws. That difference is the whole VRFovXTest question.
        void        ProbeCameraField();

        // Times a real back-buffer readback through the proxy DDRAW.dll, on a
        // finished world frame. This is the M4 transport number the brief
        // scheduled and nobody has ever taken.
        void        ProbeReadbackTick();

        // Prints what LTRotation's multiply actually does, so composition
        // order stops being something judged through a headset.
        void        LogRotationConvention();

        // Feeds each head axis in alone and prints which camera axis it
        // actually moved. A pure function of two rotations - no headset needed.
        void        LogHeadAxisTable();

        // The engine's projection measured at the field the VR path actually
        // uses, swept on the vertical. Answers whether a 121-degree vertical
        // request survives at all - which nothing has ever checked.
        void        ProbeVrField();

        // Drives each renderer-visible operation a distinctive number of
        // times, so the d3d.ren shim's per-slot counters name themselves.
        void        ProbeRendererSlots();

        // The same idea, staged: one operation per five-second window, so each
        // burst lands in its own slice of the shim's per-interval counts and
        // names exactly one slot.
        void        TickRendererProbe();

        // The warping question as a number: how far the rotation the
        // compositor applies to our image differs from the rotation the
        // scene actually made between render and display. Zero is correct.
        void        LogFrameAgreement();

        // The same residual for the pose actually being rendered right
        // now, so a live run says whether the frames still agree.
        float       FrameResidualDeg(const LTRotation& rHeadPrev,
                                     const LTRotation& rHeadNow,
                                     const LTRotation& rBody, int nMode);

        // Eye separation in world units: from the headset's reported IPD when
        // VRIPDAuto is on and the value is plausible, otherwise the fixed
        // VRIPD. Guards against a loose headset IPD wheel silently changing
        // the stereo baseline between two runs being compared.
        float       EffectiveIPDUnits();

        // While non-zero, the right half is yawed by a known angle so the host
        // can measure the renderer's field from a single captured frame.
        int         m_nCalibFrames;

        // Consecutive-frame field capture: index of the frame being captured,
        // -1 when idle, and how many to take. See VRFieldRun.
        int         m_nFieldFrame;
        int         m_nFieldFrames;
        static const float kCalibYawRad;
        HSURFACE    m_hMarkerOn;
        HSURFACE    m_hMarkerOff;

        // Green. Only used to colour the crosshair when the head-as-mouse
        // experiment is running, so which arm is live can be seen in the
        // headset rather than read off a console nobody can read in there.
        HSURFACE    m_hMarkerAlt;
        HSURFACE    GetEyeStashSurface(int nWidth, int nHeight);
        void        LogEyeGeometry(int nEye, const LTVector& vBasePos);

        HSURFACE    m_hEyeStash;
        int         m_nEyeStashW;
        int         m_nEyeStashH;

        // VR experiment: add the head's movement to the same yaw and pitch the
        // mouse writes, so head look and mouse look travel one identical path.
        // Called from CalculateCameraRotation.
        void        UpdateHeadAsMouse();

        float       m_fVRHeadPrevYawDeg;    // last head angles seen, degrees
        float       m_fVRHeadPrevPitchDeg;
        float       m_fVRHeadAccumYaw;      // total injected, radians, so it
        float       m_fVRHeadAccumPitch;    // can be handed back on toggle-off
        LTBOOL      m_bVRHeadRefValid;

		// Process the networking handshake message
		void		ProcessHandshake(HMESSAGEREAD hMessage);

		void		DoTaunt(uint32 nClientID,uint8 nTaunt);

	private :

		CHeadBobMgr				m_HeadBobMgr;		// Handle head/weapon bob/cant
		CCameraOffsetMgr		m_CameraOffsetMgr;	// Adjust camera orientation
		CPlayerSummaryMgr		m_PlayerSummary;	// Player stats data
		CIntelItemMgr			m_IntelItemMgr;		// intelligence item data
		CInterfaceMgr			m_InterfaceMgr;		// Interface manager
		uint32					m_nVRModelFrame;
		uint32		m_nVRUpdateTick;
		uint32		m_nVRPublishedTick;	// the tick the list was last published for
		bool		m_bVRLoadPublish;	// VRPublishLoadingScreen is running
		void		VRPublishOnce();		// bumped per Update; the publish runs once per tick from RenderCamera	// published model list serial
		CGlobalClientMgr		m_GlobalMgr;		// Contains global mgrs
		CMoveMgr				m_MoveMgr;			// Always around...
		CDamageFXMgr			m_DamageFXMgr;		// handle player damage
		CScreenTintMgr			m_ScreenTintMgr;	// handle screen tinting

		// Helper class
		COptimizedRenderer		m_OptimizedRenderer;

        LTBOOL           m_bUseWorldFog;     // Tells if we should use global fog settings or
											// let the container handling do it.
        LTBOOL           m_bFlashScreen;     // Are we tinting the screen for a screen flash
        LTFLOAT          m_fFlashTime;       // Time screen stays at tint color
        LTFLOAT          m_fFlashStart;      // When did the flash start
        LTFLOAT          m_fFlashRampUp;     // Ramp up time
        LTFLOAT          m_fFlashRampDown;   // Ramp down time
        LTVector         m_vFlashColor;      // Tint color

        LTFLOAT          m_fYawBackup;
        LTFLOAT          m_fPitchBackup;

		// Player movement variables...

        uint32          m_dwPlayerFlags;    // What is the player doing
        LTBOOL          m_bSpectatorMode;   // Are we in spectator mode
		PlayerState		m_ePlayerState;		// What is the state of the player

		// Player update stuff...

        LTBOOL          m_bLastSent3rdPerson;

		LTRotation      m_rRotation;                // Player view rotation
        LTFLOAT         m_fPitch;                   // Pitch of camera
        LTFLOAT         m_fYaw;                     // Yaw of camera
        LTFLOAT         m_fRoll;                    // Roll of camera
        LTFLOAT         m_fFireJitterPitch;         // Weapon firing jitter pitch adjust
        LTFLOAT         m_fFireJitterYaw;           // Weapon firing jitter yaw adjust

        LTFLOAT         m_fPlayerPitch;             // Pitch of player object
        LTFLOAT         m_fPlayerYaw;               // Yaw of player object
        LTFLOAT         m_fPlayerRoll;              // Roll of player object

        LTBOOL          m_bAllowPlayerMovement;     // External camera stuff
        LTBOOL          m_bLastAllowPlayerMovement;
        LTBOOL          m_bWasUsingExternalCamera;  // so we can detect when we start using it
		LTBOOL          m_bUsingExternalCamera;
		LTBOOL			m_bCamIsListener;


		// Container FX helpers...

		ContainerCode	m_eCurContainerCode;	// Code of container currently in
        LTFLOAT         m_fContainerStartTime;  // Time we entered current container
        LTFLOAT         m_fFovXFXDir;           // Variable use in UpdateUnderWaterFX()
		uint8			m_nSoundFilterId;		// SoundFilterId for our current container
		uint8			m_nGlobalSoundFilterId;	// Global (i.e., whole level) sound filter

		// Camera zoom related variables...

		int				m_nZoomView;		// Are we in zoom mode (m_nZoomView > 0)
        LTBOOL          m_bZooming;         // Are we zooming
        LTBOOL          m_bZoomingIn;       // Are we zooming in
        LTFLOAT         m_fSaveLODScale;    // LOD Scale value before zooming


		// Camera ducking variables...

        LTBOOL   m_bStartedDuckingDown;      // Have we started ducking down
        LTBOOL   m_bStartedDuckingUp;        // Have we started back up
        LTFLOAT  m_fCamDuck;                 // How far to move camera
        LTFLOAT  m_fDuckDownV;               // Ducking down velocity
        LTFLOAT  m_fDuckUpV;                 // Ducking up velocity
        LTFLOAT  m_fMaxDuckDistance;         // Max distance we can duck
        LTFLOAT  m_fStartDuckTime;           // When duck up/down started


		GameDifficulty	m_eDifficulty;		// Difficulty of this game
		LTBOOL			m_bFadeBodies;
		GameType		m_eGameType;
		LevelEnd		m_eLevelEnd;
		int				m_nEndString;

	// NOTE:  The following data members do not need to be saved / loaded
	// when saving games.  Any data members that don't need to be saved
	// should be added here (to keep them together)...

		//these refer to the server we are currently connected to
		char		m_szServerAddress[32];
		int			m_nServerPort;
		char		m_szServerName[MAX_SESSION_NAME];
		LTFLOAT		m_fServerOptions[MAX_GAME_OPTIONS];

        LTBOOL      m_bIsCustomLevel;           // Is the current level a custom level
 		LTFLOAT     m_fFrameTime;               // Current frame delta

        LTRESULT    m_resSoundInit;             // Was sound initialized ok?

        LTBOOL      m_bStartedLevel;
        uint16      m_nPlayerInfoChangeFlags;
        LTFLOAT     m_fPlayerInfoLastSendTime;

        LTBOOL      m_bCameraPosInited;     // Make sure the position is valid
        LTBOOL      m_bRestoringGame;       // Are we restoring a saved game

        LTBOOL      m_bMainWindowMinimized; // Is the main window minimized?

        LTBOOL      m_bStrafing;            // Are we strafing?  This used to implement mouse strafing.
        LTBOOL      m_bHoldingMouseLook;    // Is the user holding down the mouselook key?

		// Glowing models...

        LTVector    m_vCurModelGlow;        // Current glowing model light color
        LTVector    m_vMaxModelGlow;        // Max glowing model light color
        LTVector    m_vMinModelGlow;        // Min glowing model light color
        LTFLOAT     m_fModelGlowCycleTime;  // Current type throught 1/2 cycle
        LTBOOL      m_bModelGlowCycleUp;    // Cycle color up or down?

		// Panning sky...

        LTBOOL      m_bPanSky;              // Should we pan the sky
        LTFLOAT     m_fPanSkyOffsetX;       // How much do we pan in X/frame
        LTFLOAT     m_fPanSkyOffsetZ;       // How much do we pan in Z/frame
        LTFLOAT     m_fPanSkyScaleX;        // How much do we scale the texture in X
        LTFLOAT     m_fPanSkyScaleZ;        // How much do we scale the texutre in Z
        LTFLOAT     m_fCurSkyXOffset;       // What is the current x offset
        LTFLOAT     m_fCurSkyZOffset;       // What is the current z offset

		CFlashLightPlayer	m_FlashLight;	// flash light for the player
		CWeaponModel	m_weaponModel;			// Current weapon model
        LTBOOL          m_bTweakingWeapon;      // Helper, move weapon around
        LTBOOL          m_bTweakingWeaponMuzzle;// Helper, move weapon muzzle around

		CMusic			m_Music;					// Music helper variable
        LTVector        m_vShakeAmount;         // Amount to shake screen
        LTVector        m_vDefaultLightScale;       // Level default light scale
		char			m_strCurrentWorldName[256];	// Current world that's running
        LTBOOL          m_bGamePaused;              // Is the game paused?
        LTBOOL          m_bMainWindowFocus;         // Focus


		// Interface stuff...
		CCheatMgr		m_cheatMgr;				// Same as g_pCheatMgr
		CLightScaleMgr	m_LightScaleMgr;		// Class to handle light scale changes

		CAttachButeMgr	m_AttachButeMgr;

        LTVector        m_vCurContainerLightScale;  // light scale values of current container

        LTBOOL          m_bRestoreOrientation;
		HLOCALOBJ		m_h3rdPersonCrosshair;
		// THE AIM MARKER: a sprite sitting where the gun is pointing.
		// A screen-centre crosshair is meaningless once the weapon aims
		// independently of the head, which is what motion controls did.
		HLOCALOBJ		m_hVRAimMarker;
		LTBOOL			m_bVRAimMarkerOn;
		HLOCALOBJ		m_hBoundingBox;

        LTBOOL          m_bNightVision;         // does this player currently use NightVision
        LTVector        m_vNVScreenTint;        // screen tint for night vision
        LTVector        m_vIRLightScale;        // default light scale for infrared powerup

        LTBOOL          m_bQuickSave;

        LTFLOAT         m_fEarliestRespawnTime;
        uint8           m_nCurrentMission;
        uint8           m_nCurrentLevel;

		int				m_nMPNameId;
		int				m_nMPBriefingId;


		// Camera variables...

		HLOCALOBJ		m_hCamera;			// The camera
		HLOCALOBJ		m_hInterfaceCamera;	// The camera used in the interface
		CPlayerCamera	m_PlayerCamera;		// Handle 3rd person view

		LTBOOL			m_bCameraAttachedToHead;

        LTBOOL          m_bFirstUpdate;     // Is this the first update
        LTBOOL          m_bInWorld;         // Are we in a world

        LTBOOL          m_bPlayerPosSet;    // Has the server sent us the player pos?

		// Container FX helpers...

        HLTSOUND        m_hContainerSound;  // Container sound...


		// Reverb parameters...

        LTBOOL          m_bUseReverb;
		float			m_fReverbLevel;
		float			m_fNextSoundReverbTime;
        LTVector        m_vLastReverbPos;


		// Special FX management...

		CSFXMgr		m_sfxMgr;

		// Connection handling
		LTBOOL			m_bForceDisconnect;	// Set this flag to disconnect on the next update

		// Debugging variables...

		HSURFACE	m_hDebugInfo;			// The degug info surface
        LTRect      m_rcDebugInfo;          // Debug info rect. (0,0,width,height)

        LTBOOL      m_bShowPlayerPos;       // Display player's position.
        LTBOOL      m_bShowCamPosRot;       // Display camera's position/rotation.
        LTBOOL      m_bAdjustLightScale;    // Adjusting the global light scale
        LTBOOL      m_bAdjustLightAdd;      // Adjusting the camera light add
        LTBOOL      m_bAdjustFOV;           // Adjusting the FOV
        LTBOOL      m_bAdjustWeaponBreach;  // Adjust the hand-held weapon breach
        LTBOOL      m_bAdjust1stPersonCamera; // Adjust the 1st person camera offset

		CObjEditMgr	m_editMgr;				// Mgr for editing objects

		// Disconnection code/msg storage
		uint32		m_nDisconnectCode;
		uint32		m_nDisconnectSubCode;
		char *		m_pDisconnectMsg;

		// Jake's additionals
		LTBOOL 		m_bLockFramerate; // Locks framerate for ...everything...
		LTBOOL		m_bUserWantsFramerateLock; // If the user wants to override it, let them!
		LTBOOL		m_bOldMouseLook; // If the user wants to use the old mouselook.
		LONGLONG    m_lNextUpdate;
		LONGLONG    m_lFrametime;
		int 		m_iPreviousMouseX;
		int 		m_iPreviousMouseY;
		int			m_iCurrentMouseX;
		int			m_iCurrentMouseY;
		LTBOOL		m_bGetBaseMouse;
		LARGE_INTEGER m_lTimerFrequency;

		CJukeboxButeMgr m_JukeBoxButeMgr;

		int			m_nTimeoutBugRetriesLeft;
		std::string m_sRetryAddress;

		// Private helper functions...

		void	FirstUpdate();
		void	ChangeWeapon(HMESSAGEREAD hMessage);
		void	CreateDebugSurface(char* strMessage);
		void	UpdateContainerFX();
		void	MirrorSConVar(char *pSVarName, char *pCVarName);
		void	ResetGlobalFog();
        void    UpdateUnderWaterFX(LTBOOL bUpdate=LTTRUE);
        void    UpdateBreathingFX(LTBOOL bUpdate=LTTRUE);
		void	UpdateWeaponModel();
		void	StartWeaponRecoil();
		void	DecayWeaponRecoil();
		void	UpdateHeadBob();
		void	UpdateHeadCant();
		void	UpdateDuck();
		void	UpdatePlayer();
		void	AdjustWeaponBreach();
		void	Adjust1stPersonCamera();
		void	UpdateWeaponPosition();
		void	UpdateWeaponMuzzlePosition();
        void    Update3rdPersonCrossHair(LTFLOAT fDistance);
        void    UpdateVRAimMarker();
		void	UpdateSoundReverb();

		SOUNDFILTER* GetDynamicSoundFilter();

		void	AdjustLightScale();
		void	AdjustLightAdd();
		void	AdjustFOV();
		void	AdjustMenuPolygrid();
		void	AdjustHeadBob();
		void	UpdateDebugInfo();
		// Numpad tuning for the muzzle flash - see the .cpp.
		void	VRFlashTuneUpdate();
		void	HandlePlayerStateChange(HMESSAGEREAD hMessage);
		void	HandlePlayerDamage(HMESSAGEREAD hMessage);
		void	HandleExitLevel(HMESSAGEREAD hMessage);
		void	HandleServerError(HMESSAGEREAD hMessage);
		void	HandleMultiplayerGameData(HMESSAGEREAD hMessage);
		void	HandleServerOptions(HMESSAGEREAD hMessage);

		void	HandleRespawn();

		void	InitSinglePlayer();

        LTBOOL  LoadCurrentLevel();
		void	DoStartGame();
        LTBOOL  DoLoadWorld(char* pWorldFile, char* pCurWorldSaveFile=LTNULL,
                            char* pRestoreWorldFile=LTNULL, uint8 nFlags=LOAD_NEW_GAME,
                            char *pRecordFile=LTNULL, char *pPlaydemoFile=NULL);

		void	AutoSave(HMESSAGEREAD hMessage);

		void	StartLevel();

		// Load Save functionality...

		void	BuildClientSaveMsg(HMESSAGEWRITE hMessage);
		void	UnpackClientSaveMsg(HMESSAGEREAD hRead);


		// Camera helper functions...

		void	UpdateCamera();
		void	UpdateCameraZoom();
		void	UpdateCameraShake();
		void	UpdateCameraSway();
		void	UpdateScreenFlash();
		void	ClearScreenTint();
		void	InitPlayerCamera();
        LTBOOL  UpdatePlayerCamera();
		void	UpdateCameraPosition();
		void	CalculateCameraRotation();
        LTBOOL  UpdateCameraRotation();
		void	UpdatePlayerInfo();
        LTBOOL  UpdateAlternativeCamera();
        void    TurnOffAlternativeCamera(uint8 nCamType);
        void    TurnOnAlternativeCamera(uint8 nCamType);
        void    SetExternalCamera(LTBOOL bExternal=LTTRUE);

		void	BeginZoom();
        void    HandleZoomChange(uint8 nWeaponId, LTBOOL bReset=LTFALSE);
		void	EndZoom();

		void	Update3rdPersonInfo();
        void    SetCameraFOV(LTFLOAT fFovX, LTFLOAT fFovY);
		void	HideShowAttachments(HOBJECT hObj);

		void	GetPlayerHeadPosRot(LTVector & vPos, LTRotation & rRot);
		void	AttachCameraToHead(LTBOOL bAttach=LTTRUE);

		void	CreateBoundingBox();
		void	UpdateBoundingBox();

        LTBOOL  UpdateCheats();
};

inline int CGameClientShell::GetScreenWidth()
{
    uint32 dwWidth = 640;
    uint32 dwHeight = 480;
    g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &dwWidth, &dwHeight);

	return (int)dwWidth;
}

inline int CGameClientShell::GetScreenHeight()
{
    uint32 dwWidth = 640;
    uint32 dwHeight = 480;
    g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &dwWidth, &dwHeight);

	return (int)dwHeight;
}


#endif  // __GAME_CLIENT_SHELL_H__
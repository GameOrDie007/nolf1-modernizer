// ----------------------------------------------------------------------- //
//
// MODULE  : VRLog.h
//
// PURPOSE : Structured per-run logging for the NOLF1 VR project.
//
//           A project rule: logging exists before features. Every hook reports
//           install, activation and failure through here.
//
//           Fails open. If the log file cannot be opened the game runs
//           normally and every call below becomes a no-op.
//
// ----------------------------------------------------------------------- //

#ifndef __VRLOG_H__
#define __VRLOG_H__

namespace VRLog
{
	// Per-frame call counters. The M1 gate is proving each of these lands
	// exactly once per rendered frame.
	enum Counter
	{
		CTR_FRAME_TIME = 0,		// GameClientShell.cpp:1899  m_fFrameTime read
		CTR_UPDATE_PLAYING,		// GameClientShell.cpp:2081  RenderCamera() call site
		CTR_WORLD_RENDER,		// GameClientShell.cpp:7339  g_pLTClient->RenderCamera()
		CTR_FLIP_SCREEN,		// InterfaceMgr.cpp:709      FlipScreen()
		CTR_COUNT
	};

	// Opens logs/<timestamp>/client.log next to the game executable.
	void Init();
	void Shutdown();

	// Called once at the top of CGameClientShell::Update(). Emits the previous
	// frame's counter line, then resets for the new frame.
	void BeginFrame();

	void Count(Counter c);
	void NoteFrameTime(float fSeconds);

	// How many world renders a frame should contain. The anomaly test is an
	// UPPER bound on anything that steps the simulation - the world may be
	// drawn more than once, the frame time and present may not. Kept in sync
	// with the render loop so the log cannot contradict the analysis tool.
	void SetExpectedWorldRenders(int n);

	// Millisecond clock for timing call sites. Frame time alone cannot measure
	// render cost on a vsynced display - it reports the refresh rate.
	double NowMs();

	// Cost of the world-render loop (CPU-side submission) and of FlipScreen
	// (where the driver blocks on vsync and pending GPU work). The pair is more
	// informative than either alone: work moving from one to the other tells us
	// whether we are CPU or GPU bound. Feeds the M4 transport decision.
	void NoteWorldRenderTime(double ms);
	void NoteFlipTime(double ms);

	// Free-form message, timestamped and frame-stamped.
	void Msg(const char* pFmt, ...);
	// Time this frame spent in the client's VR publish; printed on long frames.
	void AddClientVRMs(double fMs);

	bool IsActive();

	// This run's log directory ("logs\\<timestamp>"), or "" before Init.
	// Empty means no directory exists - callers must write nothing.
	const char* Dir();
}

#endif // __VRLOG_H__

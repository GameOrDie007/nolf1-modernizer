// ----------------------------------------------------------------------- //
//
// MODULE  : VRLog.cpp
//
// PURPOSE : Structured per-run logging. See VRLog.h.
//
//           Single-threaded by design. Every call site lives on the engine's
//           client-shell callback thread, so no locking is used.
//
// ----------------------------------------------------------------------- //

#include "stdafx.h"
#include "VRLog.h"

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

namespace
{
	FILE*			g_pFile			= NULL;
	unsigned int	g_nFrame		= 0;
	unsigned int	g_nAnomalies	= 0;
	int				g_Counters[VRLog::CTR_COUNT] = { 0 };
	float			g_fFrameTime	= 0.0f;

	// Frame-time statistics, accumulated for the shutdown summary.
	double			g_fTimeMin		= 1e9;
	double			g_fTimeMax		= 0.0;
	double			g_fTimeSum		= 0.0;
	unsigned int	g_nTimeSamples	= 0;

	int				g_nExpectedWR	= 1;

	// Per-frame render timings, and their run-long accumulators.
	double			g_fWorldMs		= 0.0;
	double			g_fFlipMs		= 0.0;
	double			g_fWorldSum		= 0.0;
	double			g_fFlipSum		= 0.0;
	double			g_fWorldMax		= 0.0;
	unsigned int	g_nRenderSamples = 0;

	LARGE_INTEGER	g_Freq			= { 0 };
	LARGE_INTEGER	g_Start			= { 0 };

	double Elapsed()
	{
		if (!g_Freq.QuadPart) return 0.0;
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		return (double)(now.QuadPart - g_Start.QuadPart) / (double)g_Freq.QuadPart;
	}

	void Line(const char* pFmt, ...)
	{
		if (!g_pFile) return;
		fprintf(g_pFile, "[%10.3f] ", Elapsed());
		va_list args;
		va_start(args, pFmt);
		vfprintf(g_pFile, pFmt, args);
		va_end(args);
		fputc('\n', g_pFile);
		fflush(g_pFile);	// a crash must not take the last lines with it - never buffer
	}

	// Where did CShell.dll actually load from? It ships inside Modernizer.rez,
	// so this answers whether the engine extracts it or maps it in place.
	void ReportModulePath()
	{
		HMODULE hSelf = NULL;
		if (GetModuleHandleExA(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR)&ReportModulePath, &hSelf) && hSelf)
		{
			char szPath[MAX_PATH] = { 0 };
			if (GetModuleFileNameA(hSelf, szPath, MAX_PATH))
			{
				Line("module     : %s", szPath);
				return;
			}
		}
		Line("module     : <unavailable>");
	}
}

namespace VRLog
{
	double g_fClientVRMs = 0.0;
	void AddClientVRMs(double fMs) { g_fClientVRMs += fMs; }
	// The publish's cost on EVERY frame, summarised every 900: the STALL tag
	// only shows it on frames over 25 ms, so a desk run that stays fast never
	// said what the publish cost at all.
	double g_fPubSum = 0.0, g_fPubMax = 0.0; long g_nPubFrames = 0;

// This run's log directory, so anything that wants to drop a file beside
// the log - an image dump, a capture - lands in the folder belonging to the
// run that produced it, rather than in the game root where consecutive runs
// overwrite each other and a stale file reads as a fresh result.
static char g_szDir[MAX_PATH] = { 0 };

const char* Dir()
{
	return g_szDir;
}

void Init()
{
	if (g_pFile) return;

	QueryPerformanceFrequency(&g_Freq);
	QueryPerformanceCounter(&g_Start);

	SYSTEMTIME st;
	GetLocalTime(&st);

	char szDir[MAX_PATH];
	CreateDirectoryA("logs", NULL);
	sprintf(szDir, "logs\\%04d%02d%02d-%02d%02d%02d",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	CreateDirectoryA(szDir, NULL);
	strcpy(g_szDir, szDir);

	char szPath[MAX_PATH];
	sprintf(szPath, "%s\\client.log", szDir);

	g_pFile = fopen(szPath, "w");
	if (!g_pFile) return;		// fail open - the game runs regardless

	char szCwd[MAX_PATH] = { 0 };
	GetCurrentDirectoryA(MAX_PATH, szCwd);

	Line("=== NOLF1 VR client log ===");
	Line("build      : %s %s", __DATE__, __TIME__);
#ifdef _FINAL
	Line("config     : _FINAL defined  (UpdateCheats compiled out - one world-render call site)");
#else
	Line("config     : _FINAL NOT defined  (UpdateCheats live - four world-render call sites)");
#endif
	ReportModulePath();
	Line("cwd        : %s", szCwd);
	Line("log        : %s", szPath);
	Line("");
	Line("Per-frame columns: ftr=frame-time reads  up=UpdatePlaying  wr=world renders  fs=FlipScreen");
	Line("All four should read 1 on every rendered frame. Deviations are marked ANOMALY.");
	Line("");
}

void BeginFrame()
{
	if (!g_pFile) return;

	// Emit the frame that just finished before resetting.
	if (g_nFrame > 0)
	{
		const int ftr = g_Counters[CTR_FRAME_TIME];
		const int up  = g_Counters[CTR_UPDATE_PLAYING];
		const int wr  = g_Counters[CTR_WORLD_RENDER];
		const int fs  = g_Counters[CTR_FLIP_SCREEN];

		// Upper bounds only. Anything that steps the simulation must run at
		// most once; the world itself may be drawn g_nExpectedWR times. A
		// missing present is stock behaviour at state transitions and is
		// reported separately by the engine, so it is not flagged here.
		bool bAnomaly = (ftr > 1 || up > 1 || fs > 1);
		if (!bAnomaly)
		{
			bAnomaly = (up > 0) ? (wr != g_nExpectedWR) : (wr != 0);
		}

		if (bAnomaly) ++g_nAnomalies;

		// ON A LONG FRAME, SAY HOW MUCH OF IT WAS OURS. The renderer's stall
		// line attributes a 100 ms frame to "engine+client" once the world is
		// up, and that bucket cannot be split from the renderer's side. This
		// is the client's half: the time spent in the VR publish this frame,
		// beside the frame time, on the frames that matter.
		char szVR[48] = "";
		if (g_fFrameTime * 1000.0f > 25.0f)
			sprintf(szVR, "  STALL vr-publish=%.1fms", g_fClientVRMs);
		fprintf(g_pFile, "F %06u  ft=%7.3fms  ftr=%d up=%d wr=%d fs=%d  wrms=%6.3f flipms=%7.3f%s%s\n",
			g_nFrame, g_fFrameTime * 1000.0f, ftr, up, wr, fs,
			g_fWorldMs, g_fFlipMs,
			bAnomaly ? "   <<< ANOMALY" : "", szVR);
		g_fPubSum += g_fClientVRMs; ++g_nPubFrames;
		if (g_fClientVRMs > g_fPubMax) g_fPubMax = g_fClientVRMs;
		if (g_nPubFrames >= 900)
		{
			fprintf(g_pFile, "[%10.3f] F%06u  VRPublishCost: avg %.2f ms, worst %.2f ms over %ld frames\n",
				Elapsed(), g_nFrame, g_fPubSum / (double)g_nPubFrames, g_fPubMax, g_nPubFrames);
			g_fPubSum = 0.0; g_fPubMax = 0.0; g_nPubFrames = 0;
		}
		g_fClientVRMs = 0.0;
		fflush(g_pFile);
	}

	for (int i = 0; i < CTR_COUNT; ++i) g_Counters[i] = 0;
	g_fFrameTime = 0.0f;
	g_fWorldMs   = 0.0;
	g_fFlipMs    = 0.0;
	++g_nFrame;
}

void SetExpectedWorldRenders(int n)
{
	g_nExpectedWR = n;
}

double NowMs()
{
	return Elapsed() * 1000.0;
}

void NoteWorldRenderTime(double ms)
{
	if (!g_pFile) return;
	g_fWorldMs = ms;
	g_fWorldSum += ms;
	if (ms > g_fWorldMax) g_fWorldMax = ms;
	++g_nRenderSamples;
}

void NoteFlipTime(double ms)
{
	if (!g_pFile) return;
	g_fFlipMs = ms;
	g_fFlipSum += ms;
}

void Count(Counter c)
{
	if (!g_pFile) return;
	if (c < 0 || c >= CTR_COUNT) return;
	++g_Counters[c];
}

void NoteFrameTime(float fSeconds)
{
	if (!g_pFile) return;
	g_fFrameTime = fSeconds;
	++g_Counters[CTR_FRAME_TIME];

	const double ms = fSeconds * 1000.0;
	if (ms < g_fTimeMin) g_fTimeMin = ms;
	if (ms > g_fTimeMax) g_fTimeMax = ms;
	g_fTimeSum += ms;
	++g_nTimeSamples;
}

void Msg(const char* pFmt, ...)
{
	if (!g_pFile) return;
	fprintf(g_pFile, "[%10.3f] F%06u  ", Elapsed(), g_nFrame);
	va_list args;
	va_start(args, pFmt);
	vfprintf(g_pFile, pFmt, args);
	va_end(args);
	fputc('\n', g_pFile);
	fflush(g_pFile);
}

bool IsActive()
{
	return g_pFile != NULL;
}

void Shutdown()
{
	if (!g_pFile) return;

	Line("");
	Line("=== summary ===");
	Line("frames     : %u", g_nFrame);
	Line("anomalies  : %u", g_nAnomalies);
	if (g_nTimeSamples)
	{
		Line("frame time : min %.3fms  max %.3fms  avg %.3fms  over %u samples",
			g_fTimeMin, g_fTimeMax, g_fTimeSum / (double)g_nTimeSamples, g_nTimeSamples);
	}
	if (g_nRenderSamples)
	{
		Line("world render: avg %.3fms  max %.3fms  over %u frames at %d render(s)/frame",
			g_fWorldSum / (double)g_nRenderSamples, g_fWorldMax, g_nRenderSamples, g_nExpectedWR);
		Line("flipscreen  : avg %.3fms  (absorbs vsync wait - high here means GPU/display bound)",
			g_fFlipSum / (double)g_nRenderSamples);
	}
	Line("run time   : %.1f seconds", Elapsed());
	Line("=== end ===");

	fclose(g_pFile);
	g_pFile = NULL;
}

} // namespace VRLog

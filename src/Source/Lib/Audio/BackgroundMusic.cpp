// Looping background music player.
// OGG decoded via stb_vorbis, played via waveOut (no size limit).
// WAV files played directly.
//
// Does NOT interact with the game's AudioDaemon / TPA system.

#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#include "BackgroundMusic.hpp"

#pragma comment(lib, "winmm.lib")

static HWAVEOUT  g_hWaveOut  = NULL;
static WAVEHDR   g_WaveHdr;
static BYTE*     g_pWaveData = NULL;  // WAV header + PCM
static bool      g_bLooping  = false;
static CRITICAL_SECTION g_cs;

static void BgmLog(const char* fmt, ...)
{
	FILE* f = fopen("bgmusic.log", "a");
	if (f)
	{
		va_list args;
		va_start(args, fmt);
		vfprintf(f, fmt, args);
		va_end(args);
		fprintf(f, "\n");
		fclose(f);
	}
}

// WAV header (44 bytes)
#pragma pack(push, 1)
struct WavHeader
{
	char    riff[4];
	DWORD   fileSize;
	char    wave[4];
	char    fmt[4];
	DWORD   fmtSize;
	WORD    formatTag;
	WORD    channels;
	DWORD   sampleRate;
	DWORD   bytesPerSec;
	WORD    blockAlign;
	WORD    bitsPerSample;
	char    data[4];
	DWORD   dataSize;
};
#pragma pack(pop)

// waveOut callback — re-queue buffer for looping
static void CALLBACK WaveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance,
                                 DWORD dwParam1, DWORD dwParam2)
{
	if (uMsg == WOM_DONE && g_bLooping)
	{
		EnterCriticalSection(&g_cs);
		if (g_bLooping && g_hWaveOut)
			waveOutWrite(g_hWaveOut, &g_WaveHdr, sizeof(g_WaveHdr));
		LeaveCriticalSection(&g_cs);
	}
}

//-----------------------------------------------------------------------------
CBackgroundMusic::CBackgroundMusic()
{
	m_bPlaying = false;
	InitializeCriticalSection(&g_cs);
}

CBackgroundMusic::~CBackgroundMusic()
{
	Stop();
	DeleteCriticalSection(&g_cs);
}

//-----------------------------------------------------------------------------
bool CBackgroundMusic::Play(const char* pszFilename)
{
	BgmLog("Play(%s)", pszFilename);
	Stop();

	const char* pszExt = strrchr(pszFilename, '.');
	if (!pszExt || (lstrcmpi(pszExt, ".ogg") != 0 && lstrcmpi(pszExt, ".wav") != 0))
	{
		BgmLog("  FAIL: bad extension");
		return false;
	}

	if (GetFileAttributes(pszFilename) == 0xFFFFFFFF)
	{
		BgmLog("  FAIL: file not found");
		return false;
	}
	BgmLog("  file found");

	// --- WAV: read file into memory for waveOut ---
	if (lstrcmpi(pszExt, ".wav") == 0)
	{
		HANDLE hFile = CreateFile(pszFilename, GENERIC_READ, FILE_SHARE_READ,
		                          NULL, OPEN_EXISTING, 0, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
			return false;

		DWORD dwSize = GetFileSize(hFile, NULL);
		g_pWaveData = (BYTE*)malloc(dwSize);
		if (!g_pWaveData) { CloseHandle(hFile); return false; }

		ReadFile(hFile, g_pWaveData, dwSize, &dwSize, NULL);
		CloseHandle(hFile);
	}
	else
	{
		// --- OGG: decode to PCM, wrap in WAV header ---
		int channels, sample_rate;
		short* pcm;
		int nSamples = stb_vorbis_decode_filename(pszFilename, &channels, &sample_rate, &pcm);
		BgmLog("  OGG decode: samples=%d ch=%d rate=%d", nSamples, channels, sample_rate);
		if (nSamples <= 0) return false;

		DWORD dataSize = nSamples * sizeof(short);
		DWORD totalSize = sizeof(WavHeader) + dataSize;
		g_pWaveData = (BYTE*)malloc(totalSize);
		if (!g_pWaveData) { free(pcm); return false; }

		WavHeader* pHdr = (WavHeader*)g_pWaveData;
		ZeroMemory(pHdr, sizeof(WavHeader));
		memcpy(pHdr->riff,  "RIFF", 4);
		pHdr->fileSize     = totalSize - 8;
		memcpy(pHdr->wave,  "WAVE", 4);
		memcpy(pHdr->fmt,   "fmt ", 4);
		pHdr->fmtSize      = 16;
		pHdr->formatTag    = 1;
		pHdr->channels     = (WORD)channels;
		pHdr->sampleRate   = sample_rate;
		pHdr->bitsPerSample = 16;
		pHdr->blockAlign   = (WORD)(channels * 2);
		pHdr->bytesPerSec  = sample_rate * channels * 2;
		memcpy(pHdr->data,  "data", 4);
		pHdr->dataSize     = dataSize;
		memcpy(g_pWaveData + sizeof(WavHeader), pcm, dataSize);
		free(pcm);
		BgmLog("  wrote %d bytes in-memory WAV", totalSize);
	}

	// --- open waveOut device ---
	WavHeader* pHdr = (WavHeader*)g_pWaveData;
	WAVEFORMATEX wfx;
	wfx.wFormatTag      = WAVE_FORMAT_PCM;
	wfx.nChannels       = pHdr->channels;
	wfx.nSamplesPerSec  = pHdr->sampleRate;
	wfx.wBitsPerSample  = pHdr->bitsPerSample;
	wfx.nBlockAlign     = pHdr->blockAlign;
	wfx.nAvgBytesPerSec = pHdr->bytesPerSec;
	wfx.cbSize          = 0;

	MMRESULT mmr = waveOutOpen(&g_hWaveOut, WAVE_MAPPER, &wfx,
	                           (DWORD)WaveOutProc, 0, CALLBACK_FUNCTION);
	if (mmr != MMSYSERR_NOERROR)
	{
		BgmLog("  FAIL: waveOutOpen error %d", mmr);
		free(g_pWaveData); g_pWaveData = NULL;
		return false;
	}

	// --- prepare and queue buffer ---
	DWORD dwTotalSize = sizeof(WavHeader) + pHdr->dataSize;
	ZeroMemory(&g_WaveHdr, sizeof(g_WaveHdr));
	g_WaveHdr.lpData         = (LPSTR)g_pWaveData;
	g_WaveHdr.dwBufferLength = dwTotalSize;
	g_WaveHdr.dwFlags        = 0;

	mmr = waveOutPrepareHeader(g_hWaveOut, &g_WaveHdr, sizeof(g_WaveHdr));
	if (mmr != MMSYSERR_NOERROR)
	{
		BgmLog("  FAIL: waveOutPrepareHeader error %d", mmr);
		waveOutClose(g_hWaveOut); g_hWaveOut = NULL;
		free(g_pWaveData); g_pWaveData = NULL;
		return false;
	}

	g_bLooping = true;
	mmr = waveOutWrite(g_hWaveOut, &g_WaveHdr, sizeof(g_WaveHdr));
	if (mmr != MMSYSERR_NOERROR)
	{
		BgmLog("  FAIL: waveOutWrite error %d", mmr);
		waveOutUnprepareHeader(g_hWaveOut, &g_WaveHdr, sizeof(g_WaveHdr));
		waveOutClose(g_hWaveOut); g_hWaveOut = NULL;
		free(g_pWaveData); g_pWaveData = NULL;
		return false;
	}

	m_bPlaying = true;
	BgmLog("  SUCCESS: playing (waveOut)");
	return true;
}

//-----------------------------------------------------------------------------
void CBackgroundMusic::Stop()
{
	if (!m_bPlaying && !g_hWaveOut) return;
	BgmLog("Stop()");

	EnterCriticalSection(&g_cs);
	g_bLooping = false;
	if (g_hWaveOut)
	{
		waveOutReset(g_hWaveOut);
		waveOutUnprepareHeader(g_hWaveOut, &g_WaveHdr, sizeof(g_WaveHdr));
		waveOutClose(g_hWaveOut);
		g_hWaveOut = NULL;
	}
	LeaveCriticalSection(&g_cs);

	if (g_pWaveData)
	{
		free(g_pWaveData);
		g_pWaveData = NULL;
	}
	m_bPlaying = false;
}

//-----------------------------------------------------------------------------
CBackgroundMusic g_BackgroundMusic;

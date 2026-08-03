// Looping background music player.
// OGG decoded via stb_vorbis, played via waveOut with streaming buffers.
// Does NOT interact with the game's AudioDaemon / TPA system.

#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#include "BackgroundMusic.hpp"

#pragma comment(lib, "winmm.lib")

#define WAVCHUNK_SIZE  16384
#define MAX_QUEUED      8       // queue only a few at a time

struct BgmState
{
	HWAVEOUT  hWaveOut;
	BYTE*     pData;
	DWORD     dwTotalSize;
	int       nChunks;
	WAVEHDR*  pHeaders;
	int       iNextChunk;
	LONG      lPlaying;
	CRITICAL_SECTION cs;
};
static BgmState g;

static void BgmLog(const char* fmt, ...)
{
	FILE* f = fopen("bgmusic.log", "a");
	if (f) {
		va_list args; va_start(args, fmt);
		vfprintf(f, fmt, args); va_end(args);
		fprintf(f, "\n"); fclose(f);
	}
}

#pragma pack(push, 1)
struct WavHeader
{
	char    riff[4]; DWORD fileSize; char wave[4];
	char    fmt[4];  DWORD fmtSize;  WORD formatTag;
	WORD    channels;   DWORD sampleRate;
	DWORD   bytesPerSec; WORD blockAlign; WORD bitsPerSample;
	char    data[4]; DWORD dataSize;
};
#pragma pack(pop)

static void CALLBACK WaveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance,
                                 DWORD dwParam1, DWORD dwParam2)
{
	if (uMsg != WOM_DONE) return;
	EnterCriticalSection(&g.cs);
	if (g.lPlaying && g.hWaveOut)
	{
		// Queue the next chunk in sequence (or re-queue for loop)
		waveOutWrite(g.hWaveOut, &g.pHeaders[g.iNextChunk], sizeof(WAVEHDR));
		g.iNextChunk++;
		if (g.iNextChunk >= g.nChunks) g.iNextChunk = 0;
	}
	LeaveCriticalSection(&g.cs);
}

CBackgroundMusic::CBackgroundMusic()
{
	m_bPlaying = false;
	ZeroMemory(&g, sizeof(g));
	InitializeCriticalSection(&g.cs);
}

CBackgroundMusic::~CBackgroundMusic()
{
	Stop();
	DeleteCriticalSection(&g.cs);
}

static void FreeBgmState()
{
	EnterCriticalSection(&g.cs);
	if (g.hWaveOut)
	{
		waveOutReset(g.hWaveOut);
		for (int i = 0; i < g.nChunks; i++)
			waveOutUnprepareHeader(g.hWaveOut, &g.pHeaders[i], sizeof(WAVEHDR));
		waveOutClose(g.hWaveOut);
		g.hWaveOut = NULL;
	}
	delete[] g.pHeaders; g.pHeaders = NULL;
	g.nChunks = 0;
	free(g.pData); g.pData = NULL;
	g.dwTotalSize = 0;
	LeaveCriticalSection(&g.cs);
}

bool CBackgroundMusic::Play(const char* pszFilename)
{
	BgmLog("Play(%s)", pszFilename);
	Stop();

	const char* pszExt = strrchr(pszFilename, '.');
	if (!pszExt || (lstrcmpi(pszExt, ".ogg") != 0 && lstrcmpi(pszExt, ".wav") != 0))
		return false;
	if (GetFileAttributes(pszFilename) == 0xFFFFFFFF)
		return false;

	// --- Load/decode ---
	DWORD dwTotalSize;
	if (lstrcmpi(pszExt, ".wav") == 0)
	{
		HANDLE hFile = CreateFile(pszFilename, GENERIC_READ, FILE_SHARE_READ,
		                          NULL, OPEN_EXISTING, 0, NULL);
		if (hFile == INVALID_HANDLE_VALUE) return false;
		dwTotalSize = GetFileSize(hFile, NULL);
		g.pData = (BYTE*)malloc(dwTotalSize);
		if (!g.pData) { CloseHandle(hFile); return false; }
		ReadFile(hFile, g.pData, dwTotalSize, &dwTotalSize, NULL);
		CloseHandle(hFile);
	}
	else
	{
		int channels, sample_rate;
		short* pcm;
		int nSamples = stb_vorbis_decode_filename(pszFilename, &channels, &sample_rate, &pcm);
		BgmLog("  OGG: %d samples %dch %dHz", nSamples, channels, sample_rate);
		if (nSamples <= 0) return false;
		DWORD dataSize = nSamples * sizeof(short);
		dwTotalSize = sizeof(WavHeader) + dataSize;
		g.pData = (BYTE*)malloc(dwTotalSize);
		if (!g.pData) { free(pcm); return false; }
		WavHeader* pHdr = (WavHeader*)g.pData;
		ZeroMemory(pHdr, sizeof(WavHeader));
		memcpy(pHdr->riff, "RIFF", 4);  pHdr->fileSize = dwTotalSize - 8;
		memcpy(pHdr->wave, "WAVE", 4);  memcpy(pHdr->fmt, "fmt ", 4);
		pHdr->fmtSize = 16;  pHdr->formatTag = 1;
		pHdr->channels = (WORD)channels;  pHdr->sampleRate = sample_rate;
		pHdr->bitsPerSample = 16;  pHdr->blockAlign = (WORD)(channels * 2);
		pHdr->bytesPerSec = sample_rate * channels * 2;
		memcpy(pHdr->data, "data", 4);  pHdr->dataSize = dataSize;
		memcpy(g.pData + sizeof(WavHeader), pcm, dataSize);
		free(pcm);
	}
	g.dwTotalSize = dwTotalSize;
	BgmLog("  total bytes: %d", dwTotalSize);

	// --- Open waveOut ---
	WavHeader* pHdr = (WavHeader*)g.pData;
	WAVEFORMATEX wfx;
	wfx.wFormatTag      = WAVE_FORMAT_PCM;
	wfx.nChannels       = pHdr->channels;
	wfx.nSamplesPerSec  = pHdr->sampleRate;
	wfx.wBitsPerSample  = pHdr->bitsPerSample;
	wfx.nBlockAlign     = pHdr->blockAlign;
	wfx.nAvgBytesPerSec = pHdr->bytesPerSec;
	wfx.cbSize          = 0;

	if (waveOutOpen(&g.hWaveOut, WAVE_MAPPER, &wfx,
	                (DWORD)WaveOutProc, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
	{
		BgmLog("  FAIL: waveOutOpen");
		free(g.pData); g.pData = NULL;
		return false;
	}

	// --- Prepare all headers ---
	g.nChunks = (dwTotalSize + WAVCHUNK_SIZE - 1) / WAVCHUNK_SIZE;
	g.pHeaders = new WAVEHDR[g.nChunks];
	ZeroMemory(g.pHeaders, sizeof(WAVEHDR) * g.nChunks);
	BYTE* pData = g.pData;
	DWORD dwRem = dwTotalSize;
	for (int i = 0; i < g.nChunks; i++)
	{
		DWORD dwChunk = (dwRem < WAVCHUNK_SIZE) ? dwRem : WAVCHUNK_SIZE;
		g.pHeaders[i].lpData = (LPSTR)pData;
		g.pHeaders[i].dwBufferLength = dwChunk;
		g.pHeaders[i].dwFlags = 0;
		if (waveOutPrepareHeader(g.hWaveOut, &g.pHeaders[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			BgmLog("  FAIL: PrepareHeader[%d]", i);
			FreeBgmState(); return false;
		}
		pData += dwChunk; dwRem -= dwChunk;
	}
	BgmLog("  %d chunks prepared", g.nChunks);

	// --- Queue only MAX_QUEUED chunks initially, callback queues the rest ---
	InterlockedExchange(&g.lPlaying, 1);
	int nQueued = 0;
	int j;
	for (j = 0; j < MAX_QUEUED && j < g.nChunks; j++)
	{
		if (waveOutWrite(g.hWaveOut, &g.pHeaders[j], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
			break;
		nQueued++;
	}
	g.iNextChunk = j;
	if (g.iNextChunk >= g.nChunks) g.iNextChunk = 0;

	m_bPlaying = true;
	BgmLog("  SUCCESS: %d/%d chunks queued, next=%d", nQueued, g.nChunks, g.iNextChunk);
	return true;
}

void CBackgroundMusic::Stop()
{
	if (!m_bPlaying && !g.hWaveOut) return;
	BgmLog("Stop()");
	InterlockedExchange(&g.lPlaying, 0);
	FreeBgmState();
	m_bPlaying = false;
}

CBackgroundMusic g_BackgroundMusic;

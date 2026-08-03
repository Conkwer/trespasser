// Looping background music via DirectSound.
// WAV played directly. OGG decoded via stb_vorbis, wrapped in WAV header.
// DirectSound secondary buffer has no practical size limit for our use.

#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"

#include <windows.h>
#include <dsound.h>
#include <stdio.h>
#include <stdarg.h>

#include "BackgroundMusic.hpp"

static LPDIRECTSOUND       g_pDS     = NULL;
static LPDIRECTSOUNDBUFFER g_pDSBuf  = NULL;

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

CBackgroundMusic::CBackgroundMusic()  { m_bPlaying = false; }
CBackgroundMusic::~CBackgroundMusic() { Stop(); }

bool CBackgroundMusic::Play(const char* pszFilename)
{
	BgmLog("Play(%s)", pszFilename);
	Stop();

	const char* pszExt = strrchr(pszFilename, '.');
	if (!pszExt || (lstrcmpi(pszExt, ".ogg") != 0 && lstrcmpi(pszExt, ".wav") != 0))
		return false;
	if (GetFileAttributes(pszFilename) == 0xFFFFFFFF)
		return false;

	// --- Get PCM data and format ---
	BYTE*  pPCM     = NULL;
	DWORD  dwPCMSize = 0;
	int    channels = 0, sampleRate = 0;

	if (lstrcmpi(pszExt, ".wav") == 0)
	{
		HANDLE hFile = CreateFile(pszFilename, GENERIC_READ, FILE_SHARE_READ,
		                          NULL, OPEN_EXISTING, 0, NULL);
		if (hFile == INVALID_HANDLE_VALUE) return false;
		DWORD dwFileSize = GetFileSize(hFile, NULL);
		BYTE* pFileData = (BYTE*)malloc(dwFileSize);
		if (!pFileData) { CloseHandle(hFile); return false; }
		ReadFile(hFile, pFileData, dwFileSize, &dwFileSize, NULL);
		CloseHandle(hFile);

		WavHeader* pHdr = (WavHeader*)pFileData;
		if (memcmp(pHdr->riff, "RIFF", 4) != 0 || pHdr->formatTag != 1)
		{
			BgmLog("  FAIL: not PCM WAV");
			free(pFileData); return false;
		}
		channels   = pHdr->channels;
		sampleRate = pHdr->sampleRate;
		dwPCMSize  = pHdr->dataSize;
		pPCM = (BYTE*)malloc(dwPCMSize);
		if (pPCM)
			memcpy(pPCM, pFileData + sizeof(WavHeader), dwPCMSize);
		free(pFileData);
	}
	else
	{
		short* pcm;
		int nSamples = stb_vorbis_decode_filename(pszFilename, &channels, &sampleRate, &pcm);
		BgmLog("  OGG: %d samples %dch %dHz", nSamples, channels, sampleRate);
		if (nSamples <= 0) return false;
		dwPCMSize = nSamples * sizeof(short);
		pPCM = (BYTE*)pcm;  // take ownership
	}

	if (!pPCM || dwPCMSize == 0) return false;

	// --- Create DirectSound ---
	if (FAILED(DirectSoundCreate(NULL, &g_pDS, NULL)))
	{
		BgmLog("  FAIL: DirectSoundCreate");
		free(pPCM); return false;
	}
	HWND hWnd = GetForegroundWindow();
	if (FAILED(g_pDS->SetCooperativeLevel(hWnd, DSSCL_NORMAL)))
	{
		BgmLog("  FAIL: SetCooperativeLevel");
		g_pDS->Release(); g_pDS = NULL;
		free(pPCM); return false;
	}

	// --- Create secondary buffer ---
	WAVEFORMATEX wfx;
	ZeroMemory(&wfx, sizeof(wfx));
	wfx.wFormatTag      = WAVE_FORMAT_PCM;
	wfx.nChannels       = (WORD)channels;
	wfx.nSamplesPerSec  = sampleRate;
	wfx.wBitsPerSample  = 16;
	wfx.nBlockAlign     = (WORD)(channels * 2);
	wfx.nAvgBytesPerSec = sampleRate * channels * 2;

	DSBUFFERDESC dsbd;
	ZeroMemory(&dsbd, sizeof(dsbd));
	dsbd.dwSize         = sizeof(dsbd);
	dsbd.dwFlags        = DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME;
	dsbd.dwBufferBytes  = dwPCMSize;
	dsbd.lpwfxFormat    = &wfx;

	if (FAILED(g_pDS->CreateSoundBuffer(&dsbd, &g_pDSBuf, NULL)))
	{
		BgmLog("  FAIL: CreateSoundBuffer(%d bytes)", dwPCMSize);
		g_pDS->Release(); g_pDS = NULL;
		free(pPCM); return false;
	}

	// --- Fill buffer ---
	void* pLocked;
	DWORD dwLocked;
	if (SUCCEEDED(g_pDSBuf->Lock(0, dwPCMSize, &pLocked, &dwLocked, NULL, NULL, 0)))
	{
		memcpy(pLocked, pPCM, dwPCMSize);
		g_pDSBuf->Unlock(pLocked, dwLocked, NULL, 0);
	}
	free(pPCM);

	// --- Play looping ---
	g_pDSBuf->SetCurrentPosition(0);
	g_pDSBuf->Play(0, 0, DSBPLAY_LOOPING);

	m_bPlaying = true;
	BgmLog("  SUCCESS: DirectSound looping, %d bytes", dwPCMSize);
	return true;
}

void CBackgroundMusic::Stop()
{
	if (g_pDSBuf)
	{
		g_pDSBuf->Stop();
		g_pDSBuf->Release();
		g_pDSBuf = NULL;
	}
	if (g_pDS)
	{
		g_pDS->Release();
		g_pDS = NULL;
	}
	if (m_bPlaying) BgmLog("Stop()");
	m_bPlaying = false;
}

CBackgroundMusic g_BackgroundMusic;

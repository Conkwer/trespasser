// Simple looping background music player.
// Decodes OGG via stb_vorbis, wraps in a WAV header in memory,
// plays via PlaySound (winmm).  No DirectSound dependency.
//
// Does NOT interact with the game's AudioDaemon / TPA system.

#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"

#include <windows.h>
#include <stdio.h>

#include "BackgroundMusic.hpp"

#pragma comment(lib, "winmm.lib")

// WAV file header structure (44 bytes before PCM data)
#pragma pack(push, 1)
struct WavHeader
{
	char    riff[4];        // "RIFF"
	DWORD   fileSize;       // 36 + dataSize
	char    wave[4];        // "WAVE"
	char    fmt[4];         // "fmt "
	DWORD   fmtSize;        // 16 for PCM
	WORD    formatTag;      // 1 = PCM
	WORD    channels;
	DWORD   sampleRate;
	DWORD   bytesPerSec;
	WORD    blockAlign;
	WORD    bitsPerSample;
	char    data[4];        // "data"
	DWORD   dataSize;
};
#pragma pack(pop)

static BYTE*  g_pWavMem  = NULL;  // WAV header + PCM, for PlaySound
static bool   g_bActive  = false;

//-----------------------------------------------------------------------------
CBackgroundMusic::CBackgroundMusic()
{
	m_bPlaying = false;
}

CBackgroundMusic::~CBackgroundMusic()
{
	Stop();
}

//-----------------------------------------------------------------------------
bool CBackgroundMusic::Play(const char* pszFilename)
{
	Stop();

	// Check extension
	const char* pszExt = strrchr(pszFilename, '.');
	if (!pszExt || (lstrcmpi(pszExt, ".ogg") != 0 && lstrcmpi(pszExt, ".wav") != 0))
		return false;

	if (GetFileAttributes(pszFilename) == 0xFFFFFFFF)
		return false;

	if (lstrcmpi(pszExt, ".wav") == 0)
	{
		// Play WAV directly via PlaySound from file
		if (!PlaySound(pszFilename, NULL, SND_FILENAME | SND_LOOP | SND_ASYNC))
			return false;
		m_bPlaying = true;
		g_bActive = true;
		return true;
	}

	// --- OGG: decode to PCM ---
	int channels, sample_rate;
	short* pcm;
	int nSamples = stb_vorbis_decode_filename(pszFilename, &channels, &sample_rate, &pcm);
	if (nSamples <= 0)
		return false;

	// --- build WAV header in memory ---
	DWORD dataSize = nSamples * sizeof(short);
	DWORD totalSize = sizeof(WavHeader) + dataSize;

	g_pWavMem = (BYTE*)malloc(totalSize);
	if (!g_pWavMem)
	{
		free(pcm);
		return false;
	}

	WavHeader* pHdr = (WavHeader*)g_pWavMem;
	ZeroMemory(pHdr, sizeof(WavHeader));
	memcpy(pHdr->riff,  "RIFF", 4);
	pHdr->fileSize     = totalSize - 8;
	memcpy(pHdr->wave,  "WAVE", 4);
	memcpy(pHdr->fmt,   "fmt ", 4);
	pHdr->fmtSize      = 16;
	pHdr->formatTag    = 1;  // PCM
	pHdr->channels     = (WORD)channels;
	pHdr->sampleRate   = sample_rate;
	pHdr->bitsPerSample = 16;
	pHdr->blockAlign   = (WORD)(channels * 2);
	pHdr->bytesPerSec  = sample_rate * channels * 2;
	memcpy(pHdr->data,  "data", 4);
	pHdr->dataSize     = dataSize;

	// Copy PCM after header
	memcpy(g_pWavMem + sizeof(WavHeader), pcm, dataSize);
	free(pcm);

	// Play from memory, looping, async
	if (!PlaySound((LPCSTR)g_pWavMem, NULL, SND_MEMORY | SND_LOOP | SND_ASYNC))
	{
		free(g_pWavMem);
		g_pWavMem = NULL;
		return false;
	}

	m_bPlaying = true;
	g_bActive = true;
	return true;
}

//-----------------------------------------------------------------------------
void CBackgroundMusic::Stop()
{
	PlaySound(NULL, NULL, 0);  // Stop any playing sound

	if (g_pWavMem)
	{
		free(g_pWavMem);
		g_pWavMem = NULL;
	}

	m_bPlaying = false;
	g_bActive = false;
}

//-----------------------------------------------------------------------------
CBackgroundMusic g_BackgroundMusic;

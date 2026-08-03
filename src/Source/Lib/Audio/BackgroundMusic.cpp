// Simple looping background music player.
// WAV: plays directly via PlaySound from file.
// OGG: decodes via stb_vorbis to a temp WAV, plays from file.
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

static char g_szTempWav[MAX_PATH] = "";

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

	const char* pszExt = strrchr(pszFilename, '.');
	if (!pszExt || (lstrcmpi(pszExt, ".ogg") != 0 && lstrcmpi(pszExt, ".wav") != 0))
		return false;

	if (GetFileAttributes(pszFilename) == 0xFFFFFFFF)
		return false;

	// WAV: play directly from file
	if (lstrcmpi(pszExt, ".wav") == 0)
	{
		if (!PlaySound(pszFilename, NULL, SND_FILENAME | SND_LOOP | SND_ASYNC))
			return false;
		m_bPlaying = true;
		return true;
	}

	// OGG: decode to PCM, write temp WAV, play from file
	int channels, sample_rate;
	short* pcm;
	int nSamples = stb_vorbis_decode_filename(pszFilename, &channels, &sample_rate, &pcm);
	if (nSamples <= 0)
		return false;

	// Build temp WAV path in the game directory
	GetTempPath(MAX_PATH, g_szTempWav);
	lstrcat(g_szTempWav, "tp_bgm.wav");

	DWORD dataSize = nSamples * sizeof(short);
	DWORD totalSize = sizeof(WavHeader) + dataSize;

	HANDLE hFile = CreateFile(g_szTempWav, GENERIC_WRITE, 0, NULL,
	                          CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		free(pcm);
		return false;
	}

	WavHeader hdr;
	ZeroMemory(&hdr, sizeof(hdr));
	memcpy(hdr.riff,  "RIFF", 4);
	hdr.fileSize     = totalSize - 8;
	memcpy(hdr.wave,  "WAVE", 4);
	memcpy(hdr.fmt,   "fmt ", 4);
	hdr.fmtSize      = 16;
	hdr.formatTag    = 1;
	hdr.channels     = (WORD)channels;
	hdr.sampleRate   = sample_rate;
	hdr.bitsPerSample = 16;
	hdr.blockAlign   = (WORD)(channels * 2);
	hdr.bytesPerSec  = sample_rate * channels * 2;
	memcpy(hdr.data,  "data", 4);
	hdr.dataSize     = dataSize;

	DWORD dwWritten;
	WriteFile(hFile, &hdr, sizeof(hdr), &dwWritten, NULL);
	WriteFile(hFile, pcm, dataSize, &dwWritten, NULL);
	CloseHandle(hFile);
	free(pcm);

	if (!PlaySound(g_szTempWav, NULL, SND_FILENAME | SND_LOOP | SND_ASYNC))
	{
		DeleteFile(g_szTempWav);
		g_szTempWav[0] = '\0';
		return false;
	}

	m_bPlaying = true;
	return true;
}

//-----------------------------------------------------------------------------
void CBackgroundMusic::Stop()
{
	PlaySound(NULL, NULL, 0);

	if (g_szTempWav[0])
	{
		DeleteFile(g_szTempWav);
		g_szTempWav[0] = '\0';
	}

	m_bPlaying = false;
}

//-----------------------------------------------------------------------------
CBackgroundMusic g_BackgroundMusic;

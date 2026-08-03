// Looping background music via DirectSound with CTRLFREQUENCY lock.
// Supports .ogg (stb_vorbis), .wav (PCM), .cau (IMA ADPCM — own decoder).

#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"

#include <windows.h>
#include <dsound.h>
#include <stdio.h>
#include <stdarg.h>

#include "BackgroundMusic.hpp"

extern HWND g_hwnd;

static LPDIRECTSOUND       g_pDS     = NULL;
static LPDIRECTSOUNDBUFFER g_pDSBuf  = NULL;
static DWORD               g_dwRate  = 0;
static bool                g_bHaveDS = false;

static void BgmLog(const char* fmt, ...)
{
	FILE* f = fopen("bgmusic.log", "a");
	if (f) {
		va_list a; va_start(a, fmt);
		vfprintf(f, fmt, a); va_end(a);
		fprintf(f, "\n"); fclose(f);
	}
}

// --- IMA ADPCM decoder tables (same as AudioADPCM.cpp) ---
static const int i4Step[89] = {
	7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,
	55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,
	279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,
	1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,
	3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,
	11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,
	29794,32767
};
static const int i4NextStep[256] = {
	-1,-1,-1,-1,2,4,6,8, -1,-1,-1,-1,2,4,6,8,
	-1,-1,-1,-1,2,4,6,8, -1,-1,-1,-1,2,4,6,8,
	-1,-1,-1,-1,2,4,6,8, -1,-1,-1,-1,2,4,6,8,
	-1,-1,-1,-1,2,4,6,8, -1,-1,-1,-1,2,4,6,8,
	-1,-1,-1,-1,2,4,6,8, -1,-1,-1,-1,2,4,6,8,
	-1,-1,-1,-1,2,4,6,8, -1,-1,-1,-1,2,4,6,8,
	-1,-1,-1,-1,2,4,6,8, -1,-1,-1,-1,2,4,6,8,
	-1,-1,-1,-1,2,4,6,8, -1,-1,-1,-1,2,4,6,8
};

// CAU file header (36 bytes, little-endian)
struct CAUHeader
{
	DWORD magic;       // 'ROBW'
	DWORD version;
	DWORD offset;      // offset to sample data
	DWORD blockAlign;  // ADPCM block alignment
	DWORD dataSize;    // compressed data size
	DWORD decompSize;  // decompressed size in bytes
	DWORD frequency;
	BYTE  bits;
	BYTE  channels;
	BYTE  compression; // 0=PCM, 1=ADPCM, 2=voice
	BYTE  flags;
	DWORD subtitleOff; // subtitle data offset
};

// Decode a single IMA ADPCM nibble
static int DecodeNibble(int nib, int* pPred, int* pIdx)
{
	int step = i4Step[*pIdx];
	int diff = step >> 3;
	if (nib & 4) diff += step;
	if (nib & 2) diff += step >> 1;
	if (nib & 1) diff += step >> 2;
	if (nib & 8) diff = -diff;
	int samp = *pPred + diff;
	if (samp > 32767) samp = 32767;
	else if (samp < -32768) samp = -32768;
	*pPred = samp;
	*pIdx += i4NextStep[nib & 0xF];
	if (*pIdx < 0) *pIdx = 0;
	else if (*pIdx > 88) *pIdx = 88;
	return samp;
}

// Decode ADX frame (18 bytes → 32 samples per channel)
// coeff[2] = {c1*2^14, c2*2^14} pre-scaled from standard formula
static void DecodeADXFrame(short* dst, const BYTE* src, int* pS1, int* pS2,
                           int c1, int c2)
{
	int scale = ((src[0] << 8) | src[1]) & 0x1FFF;
	int s1 = *pS1, s2 = *pS2;
	for (int i = 2; i < 18; i++)
	{
		int b = src[i];
		// High nibble first
		int d = b >> 4;
		if (d & 8) d -= 16;
		int s0 = d * scale + ((c1 * s1 + c2 * s2) >> 14);
		if (s0 > 32767) s0 = 32767; else if (s0 < -32768) s0 = -32768;
		*dst++ = (short)s0;
		s2 = s1; s1 = s0;
		// Low nibble
		d = b & 15;
		if (d & 8) d -= 16;
		s0 = d * scale + ((c1 * s1 + c2 * s2) >> 14);
		if (s0 > 32767) s0 = 32767; else if (s0 < -32768) s0 = -32768;
		*dst++ = (short)s0;
		s2 = s1; s1 = s0;
	}
	*pS1 = s1; *pS2 = s2;
}

// Decode ADX file to PCM
static DWORD DecodeADX(BYTE* pSrc, DWORD srcSize, int channels,
                        BYTE* pDst, DWORD dstMax)
{
	if (srcSize < 24 || pSrc[0] != 0x80) return 0;

	WORD copyOff   = (pSrc[2] << 8) | pSrc[3];   // BE
	BYTE encType   = pSrc[4];
	BYTE blockSize = pSrc[5];  // usually 18
	BYTE bitDepth  = pSrc[6];  // usually 4
	// channel count at [7], sample rate at [8-11] BE, total samples [12-15] BE
	int highpass   = (pSrc[0x10] << 8) | pSrc[0x11];  // BE

	if ((DWORD)copyOff + 6 > srcSize) return 0;
	if (memcmp(pSrc + copyOff - 2, "(c)CRI", 6) != 0) return 0;
	DWORD pos = copyOff + 4;  // audio data starts here

	// Compute prediction coefficients
	int c1, c2;
	if (encType == 3) // standard ADX: compute from highpass
	{
		double a = 1.41421356237 - cos(6.28318530718 * (double)highpass / 44100.0);
		double b = 0.41421356237;
		double cc = (a - sqrt((a + b) * (a - b))) / b;
		c1 = (int)(cc * 2.0 * 16384.0);
		c2 = (int)(-(cc * cc) * 16384.0);
	}
	else // type 2: fixed coefficients (use set 0 = all zero → no prediction)
	{
		c1 = 0; c2 = 0;
	}

	BYTE* pDstStart = pDst;
	int s1[2] = {0, 0}, s2[2] = {0, 0};

	while (pos + (DWORD)(channels * blockSize) <= srcSize)
	{
		short* dst = (short*)pDst;
		if ((DWORD)((BYTE*)dst - pDstStart) + channels * 32 * 4 > dstMax)
			break;

		for (int ch = 0; ch < channels; ch++)
		{
			short tmp[32];
			DecodeADXFrame(tmp, pSrc + pos, &s1[ch], &s2[ch], c1, c2);
			pos += blockSize;
			for (int i = 0; i < 32; i++)
				dst[i * channels + ch] = tmp[i];
		}
		pDst += channels * 32 * 2;
	}
	return (DWORD)(pDst - pDstStart);
}

// Decode stereo 16-bit IMA ADPCM to interleaved PCM
static DWORD DecodeADPCM(BYTE* pSrc, DWORD srcSize, DWORD blockAlign,
                          BYTE* pDst, DWORD dstMax)
{
	BYTE* pDstStart = pDst;
	while (srcSize > 0 && (DWORD)(pDst - pDstStart) < dstMax)
	{
		DWORD block = blockAlign;
		if (block > srcSize) block = srcSize;
		srcSize -= block;
		block -= 8; // 2 headers * 4 bytes

		// Left channel header
		int predL = (short)(*(WORD*)pSrc);
		int idxL  = *(BYTE*)(pSrc + 2);
		if (idxL > 88) idxL = 88;
		// Right channel header
		int predR = (short)(*(WORD*)(pSrc + 4));
		int idxR  = *(BYTE*)(pSrc + 6);
		if (idxR > 88) idxR = 88;
		pSrc += 8;

		// Write first samples
		*(short*)pDst = (short)predL;
		*(short*)(pDst+2) = (short)predR;
		pDst += 4;

		// Decode 8 bytes at a time (4 left nibbles + 4 right nibbles)
		while (block >= 8)
		{
			block -= 8;
			DWORD left  = *(DWORD*)pSrc; pSrc += 4;
			DWORD right = *(DWORD*)pSrc; pSrc += 4;

			for (int i = 0; i < 8; i++)
			{
				int nibL = left & 0xF; left >>= 4;
				int nibR = right & 0xF; right >>= 4;
				int sL = DecodeNibble(nibL, &predL, &idxL);
				int sR = DecodeNibble(nibR, &predR, &idxR);
				*(short*)pDst = (short)sL;
				*(short*)(pDst+2) = (short)sR;
				pDst += 4;
			}
		}
	}
	return (DWORD)(pDst - pDstStart);
}

CBackgroundMusic::CBackgroundMusic()  { m_bPlaying = false; }
CBackgroundMusic::~CBackgroundMusic() { Stop(); }

bool CBackgroundMusic::Play(const char* pszFilename)
{
	BgmLog("Play(%s)", pszFilename);
	Stop();

	const char* pszExt = strrchr(pszFilename, '.');
	if (!pszExt) return false;
	if (GetFileAttributes(pszFilename) == 0xFFFFFFFF)
		return false;

	BYTE* pPCM = NULL; DWORD sz = 0; int ch = 0, sr = 0;

	if (lstrcmpi(pszExt, ".adx") == 0)
	{
		HANDLE hf = CreateFile(pszFilename, GENERIC_READ, FILE_SHARE_READ,
		                       NULL, OPEN_EXISTING, 0, NULL);
		if (hf == INVALID_HANDLE_VALUE) return false;
		DWORD fs = GetFileSize(hf, NULL);
		BYTE* pd = (BYTE*)malloc(fs);
		if (!pd) { CloseHandle(hf); return false; }
		ReadFile(hf, pd, fs, &fs, NULL); CloseHandle(hf);
		if (fs < 24 || pd[0] != 0x80) { BgmLog("  FAIL: not ADX"); free(pd); return false; }
			ch = pd[7];
			sr = (pd[8] << 24) | (pd[9] << 16) | (pd[10] << 8) | pd[11];
			if (ch < 1 || ch > 2) ch = 2;
			if (sr < 4000 || sr > 96000) sr = 44100;
			BgmLog("  ADX: %dch %dHz type=%d", ch, sr, pd[4]);
		pPCM = (BYTE*)malloc(fs * 4 + 4096);
		DWORD dwDec = DecodeADX(pd, fs, ch, pPCM, fs * 4);
		if (dwDec == 0) { BgmLog("  FAIL: ADX decode"); free(pd); free(pPCM); return false; }
		sz = dwDec; free(pd);
		BgmLog("  ADX decoded: %d bytes PCM", sz);
	}
	else if (lstrcmpi(pszExt, ".cau") == 0)
	{
		HANDLE hf = CreateFile(pszFilename, GENERIC_READ, FILE_SHARE_READ,
		                       NULL, OPEN_EXISTING, 0, NULL);
		if (hf == INVALID_HANDLE_VALUE) return false;
		CAUHeader cau; DWORD br;
		ReadFile(hf, &cau, sizeof(cau), &br, NULL);
		if (cau.magic != 'ROBW')
			{ BgmLog("  FAIL: bad CAU magic"); CloseHandle(hf); return false; }
		ch = cau.channels; sr = cau.frequency;
		BgmLog("  CAU: %dHz %dch comp=%d data=%d decomp=%d align=%d",
		       sr, ch, cau.compression, cau.dataSize, cau.decompSize, cau.blockAlign);

		if (cau.compression == 0)
		{
			// Raw PCM — read directly
			sz = cau.dataSize;
			pPCM = (BYTE*)malloc(sz);
			SetFilePointer(hf, cau.offset, NULL, FILE_BEGIN);
			ReadFile(hf, pPCM, sz, &br, NULL);
		}
		else if (cau.compression == 1)
		{
			// IMA ADPCM — decode
			BYTE* pComp = (BYTE*)malloc(cau.dataSize);
			SetFilePointer(hf, cau.offset, NULL, FILE_BEGIN);
			ReadFile(hf, pComp, cau.dataSize, &br, NULL);
			sz = cau.decompSize;
			pPCM = (BYTE*)malloc(sz + 4096);
			DWORD dwDec = 0;
			if (ch == 2)
				dwDec = DecodeADPCM(pComp, cau.dataSize, cau.blockAlign, pPCM, sz);
			if (dwDec == 0)
				{ BgmLog("  FAIL: ADPCM decode"); free(pComp); free(pPCM); CloseHandle(hf); return false; }
			sz = dwDec;
			free(pComp);
			BgmLog("  ADPCM decoded: %d bytes PCM", sz);
		}
		else
			{ BgmLog("  FAIL: unknown compression %d", cau.compression); CloseHandle(hf); return false; }
		CloseHandle(hf);
	}
	else if (lstrcmpi(pszExt, ".wav") == 0)
	{
		HANDLE hf = CreateFile(pszFilename, GENERIC_READ, FILE_SHARE_READ,
		                       NULL, OPEN_EXISTING, 0, NULL);
		if (hf == INVALID_HANDLE_VALUE) return false;
		DWORD fs = GetFileSize(hf, NULL);
		BYTE* pd = (BYTE*)malloc(fs);
		if (!pd) { CloseHandle(hf); return false; }
		ReadFile(hf, pd, fs, &fs, NULL); CloseHandle(hf);
		if (memcmp(pd,"RIFF",4) || memcmp(pd+8,"WAVE",4))
			{ BgmLog("  FAIL: not WAV"); free(pd); return false; }

		WORD fmtTag = *(WORD*)(pd + 20);
		ch  = *(WORD*)(pd + 22);
		sr  = *(DWORD*)(pd + 24);
		DWORD blockAlign = *(WORD*)(pd + 32);
		// Find the 'data' chunk (skip fmt + optional fact chunks)
		DWORD pos = 36; // after standard fmt header
		// If PCM, pos is 36. If ADPCM, fmt chunk may be larger (extra bytes).
		if (fmtTag == 0x11) // IMA ADPCM
		{
			WORD cbSize = *(WORD*)(pd + 34); // extra format bytes
			pos = 36 + cbSize; // skip past fmt chunk
		}
		// Skip any intermediate chunks (fact, etc.) to find 'data'
		while (pos < fs - 8 && memcmp(pd + pos, "data", 4) != 0)
		{
			DWORD chunkSize = *(DWORD*)(pd + pos + 4);
			pos += 8 + chunkSize;
		}
		if (pos >= fs - 8 || memcmp(pd + pos, "data", 4) != 0)
			{ BgmLog("  FAIL: no data chunk"); free(pd); return false; }
		DWORD dataSize = *(DWORD*)(pd + pos + 4);
		BYTE* pData = pd + pos + 8;

		if (fmtTag == 1) // PCM
		{
			sz = dataSize;
			pPCM = (BYTE*)malloc(sz);
			if (pPCM) memcpy(pPCM, pData, sz);
		}
		else if (fmtTag == 0x11) // IMA ADPCM
		{
			BgmLog("  WAV ADPCM: align=%d data=%d", blockAlign, dataSize);
			sz = 0; // will be set by decoder
			pPCM = (BYTE*)malloc(dataSize * 4 + 4096); // worst case: 4x expansion
			DWORD dwDec = DecodeADPCM(pData, dataSize, blockAlign, pPCM, dataSize * 4);
			if (dwDec == 0)
				{ BgmLog("  FAIL: ADPCM decode"); free(pd); free(pPCM); return false; }
			sz = dwDec;
			BgmLog("  ADPCM decoded: %d bytes PCM", sz);
		}
		else { BgmLog("  FAIL: unsupported WAV tag %d", fmtTag); free(pd); return false; }
		free(pd);
	}
	else if (lstrcmpi(pszExt, ".ogg") == 0)
	{
		short* pcm; int n;
		n = stb_vorbis_decode_filename(pszFilename, &ch, &sr, &pcm);
		BgmLog("  OGG: %d samples %dch %dHz", n, ch, sr);
		if (n <= 0) return false;
		sz = n * sizeof(short);
		pPCM = (BYTE*)pcm;
	}
	else
		return false;

	if (!pPCM) return false;
	g_dwRate = sr;

	if (!g_bHaveDS)
	{
		if (FAILED(DirectSoundCreate(NULL, &g_pDS, NULL)))
			{ BgmLog("  FAIL: DS"); free(pPCM); return false; }
		if (FAILED(g_pDS->SetCooperativeLevel(g_hwnd, DSSCL_NORMAL)))
			{ BgmLog("  FAIL: Coop"); g_pDS->Release(); g_pDS=NULL; free(pPCM); return false; }
		g_bHaveDS = true;
	}

	WAVEFORMATEX wfx; ZeroMemory(&wfx, sizeof(wfx));
	wfx.wFormatTag=1; wfx.nChannels=(WORD)ch; wfx.nSamplesPerSec=sr;
	wfx.wBitsPerSample=16; wfx.nBlockAlign=(WORD)(ch*2);
	wfx.nAvgBytesPerSec=sr*ch*2;

	DSBUFFERDESC d; ZeroMemory(&d, sizeof(d)); d.dwSize=sizeof(d);
	d.dwFlags=DSBCAPS_GLOBALFOCUS|DSBCAPS_CTRLVOLUME|DSBCAPS_LOCSOFTWARE|DSBCAPS_CTRLFREQUENCY;
	d.dwBufferBytes=sz; d.lpwfxFormat=&wfx;

	if (FAILED(g_pDS->CreateSoundBuffer(&d, &g_pDSBuf, NULL)))
		{ BgmLog("  FAIL: CreateBuf"); free(pPCM); return false; }

	void* pL; DWORD dwL;
	if (SUCCEEDED(g_pDSBuf->Lock(0, sz, &pL, &dwL, NULL, NULL, 0)))
		{ memcpy(pL, pPCM, sz); g_pDSBuf->Unlock(pL, dwL, NULL, 0); }
	free(pPCM);

	g_pDSBuf->SetFrequency(sr);
	g_pDSBuf->SetCurrentPosition(0);
	g_pDSBuf->Play(0, 0, DSBPLAY_LOOPING);

	m_bPlaying = true;
	BgmLog("  SUCCESS: %d bytes @ %dHz", sz, sr);
	return true;
}

void CBackgroundMusic::Stop()
{
	if (g_pDSBuf) { g_pDSBuf->Stop(); g_pDSBuf->Release(); g_pDSBuf = NULL; }
	m_bPlaying = false;
}

void CBackgroundMusic::InnerLoopCall()
{
	if (!m_bPlaying || !g_pDSBuf) return;
	if (g_dwRate)
		g_pDSBuf->SetFrequency(g_dwRate);
}

CBackgroundMusic g_BackgroundMusic;

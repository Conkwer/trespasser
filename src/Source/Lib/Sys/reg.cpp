
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "Reg.h"
#include "RegInit.hpp"

#define REGKEYPARENT HKEY_LOCAL_MACHINE
#ifndef DEMO_BUILD
#define REGLOCATION "Software\\DreamWorks Interactive\\Trespasser"
#else
#define REGLOCATION "Software\\DreamWorks Interactive\\Trespasser Demo"
#endif

#define INI_FILENAME    ".\\trespass.ini"
#define INI_SECTION     "Settings"

//
// Module specific variables.
//

BOOL bSafeModeReg = FALSE;
HKEY  g_hKey = NULL;   // kept as dummy for code that references it

void DisableSafemode()
{
	bSafeModeReg = FALSE;
}

//
// Write default INI values on first run.
//
static void WriteDefaults()
{
	SetRegValue(REG_KEY_INSTALLED,            1);
	SetRegValue(REG_KEY_SAFEMODE,             0);
	SetRegValue(strAUTOSETTINGS,               TRUE);
	SetRegValue(REG_KEY_NOVIDEO,              DEFAULT_REG_NOVIDEO);
	SetRegValue(strSIZE_WIDTH,                DEFAULT_SIZE_WIDTH);
	SetRegValue(strSIZE_HEIGHT,               DEFAULT_SIZE_HEIGHT);
	SetRegValue(strFLAG_FULLSCREEN,           DEFAULT_FULLSCREEN);
	SetRegValue(strFLAG_D3D,                  DEFAULT_D3D);
	SetRegValue(strFLAG_SYSTEMMEM,            DEFAULT_SYSTEMMEM);
	SetRegValue(REG_KEY_AUDIO_EFFECT,         DEFAULT_AUDIO_EFFECT);
	SetRegValue(REG_KEY_AUDIO_AMBIENT,        DEFAULT_AUDIO_AMBIENT);
	SetRegValue(REG_KEY_AUDIO_VOICEOVER,      DEFAULT_AUDIO_VOICEOVER);
	SetRegValue(REG_KEY_AUDIO_MUSIC,          DEFAULT_AUDIO_MUSIC);
	SetRegValue(REG_KEY_AUDIO_SUBTITLES,      DEFAULT_AUDIO_SUBTITLES);
	SetRegValue(REG_KEY_AUDIO_ENABLE3D,       DEFAULT_AUDIO_ENABLE3D);
	SetRegValue(REG_KEY_RENDERING_QUALITY,    DEFAULT_RENDERING_QUALITY);
	SetRegValue(REG_KEY_GAMMA,                DEFAULT_GAMMA);
	SetRegValue(REG_KEY_GORE,                 DEFAULT_GORE);
	SetRegValue(REG_KEY_INVERTMOUSE,          DEFAULT_INVERTMOUSE);
	SetRegValue(REG_KEY_VIEWPORT_X,           DEFAULT_VIEWPORT_X);
	SetRegValue(REG_KEY_VIEWPORT_Y,           DEFAULT_VIEWPORT_Y);
	SetRegValue(REG_KEY_AUTOLOAD,             FALSE);
	SetRegValue(REG_KEY_AUTOSAVE,             FALSE);
	SetRegValue(strFLAG_REGINIT,              1);
	SetRegValue(strD3D_DITHER,                1);
	SetRegValue(strTRIPLEBUFFER,              0);
	SetRegValue(strPAGEMANAGED,               1);
	SetRegValue(strHARDWARE_WATER,            0);
	SetRegValue(strZBUFFER_BITDEPTH,          16);
	SetRegValue(strRECOMMENDEDTEXMAX,         256);
	SetRegValue(strD3D_FILTERCACHES,          0);

	SetRegString(REG_KEY_DATA_DRIVE,        ".\\");
	SetRegString(REG_KEY_INSTALLED_DIR,     ".\\");
}

void OpenKey()
{
	// Dummy handle — some code checks g_hKey before proceeding.
	g_hKey = (HKEY)1;

	// Check if INI exists — write defaults if not.
	if (GetFileAttributes(INI_FILENAME) == 0xFFFFFFFF)
	{
		WriteDefaults();
	}

	// Get ready for safe mode.
	bSafeModeReg = GetRegValue(REG_KEY_SAFEMODE, FALSE) ? true : false;
	SetRegValue(REG_KEY_SAFEMODE, TRUE);
}

void CloseKey(BOOL b_change_safemode)
{
	if (b_change_safemode)
	{
		SetRegValue(REG_KEY_SAFEMODE, FALSE);
	}

	// Flush INI to disk
	WritePrivateProfileString(NULL, NULL, NULL, INI_FILENAME);
}

// --- Integer values ---

void SetRegValue(LPCSTR lpszVal, int nVal)
{
	char szVal[32];
	wsprintf(szVal, "%d", nVal);
	WritePrivateProfileString(INI_SECTION, lpszVal, szVal, INI_FILENAME);
}

int GetRegValue(LPCSTR lpszVal, int nDefault)
{
	char szDefault[32];
	wsprintf(szDefault, "%d", nDefault);
	return GetPrivateProfileInt(INI_SECTION, lpszVal, nDefault, INI_FILENAME);
}

// --- String values ---

void SetRegString(LPCSTR lpszVal, LPCSTR lpszString)
{
	WritePrivateProfileString(INI_SECTION, lpszVal, lpszString, INI_FILENAME);
}

int GetRegString(LPCSTR lpszVal, LPSTR lpszString, int nSize, LPCSTR lpszDefault)
{
	GetPrivateProfileString(INI_SECTION, lpszVal, lpszDefault, lpszString, nSize, INI_FILENAME);
	return lstrlen(lpszString);
}

// --- Binary data (stored as hex string in INI) ---

static void BinToHex(LPBYTE pbData, int nSize, LPSTR pszOut)
{
	int i;
	for (i = 0; i < nSize; i++)
	{
		wsprintf(pszOut + i * 2, "%02X", pbData[i]);
	}
	pszOut[nSize * 2] = 0;
}

static int HexToBin(LPCSTR pszHex, LPBYTE pbData, int nMaxSize)
{
	int nLen, nBytes, i;
	unsigned int val;
	char szByte[3];

	nLen = lstrlen(pszHex);
	if (nLen % 2) nLen--;
	nBytes = nLen / 2;
	if (nBytes > nMaxSize) nBytes = nMaxSize;

	for (i = 0; i < nBytes; i++)
	{
		szByte[0] = pszHex[i * 2];
		szByte[1] = pszHex[i * 2 + 1];
		szByte[2] = 0;
		sscanf(szByte, "%02X", &val);
		pbData[i] = (BYTE)val;
	}
	return nBytes;
}

void SetRegData(LPCSTR lpszVal, LPBYTE lpszData, int nSize)
{
	char* pszHex;
	pszHex = (char*)malloc(nSize * 2 + 1);
	if (pszHex)
	{
		BinToHex(lpszData, nSize, pszHex);
		SetRegString(lpszVal, pszHex);
		free(pszHex);
	}
}

int GetRegData(LPCSTR lpszVal, LPBYTE lpszData, int nSize)
{
	char szHex[4096];
	int nResult = GetRegString(lpszVal, szHex, sizeof(szHex), "");
	if (nResult == 0)
		return 0;
	return HexToBin(szHex, lpszData, nSize);
}

// --- Float values ---

void SetRegFloat(LPCSTR lpszVal, float fVal)
{
	char szVal[64];
	sprintf(szVal, "%f", fVal);
	SetRegString(lpszVal, szVal);
}

float GetRegFloat(LPCSTR lpszVal, float fDefault)
{
	char szDefault[64];
	sprintf(szDefault, "%f", fDefault);

	char szVal[64];
	GetRegString(lpszVal, szVal, sizeof(szVal), szDefault);

	float fResult;
	if (sscanf(szVal, "%f", &fResult) != 1)
		return fDefault;
	return fResult;
}

void DeleteValue(LPCSTR lpszVal)
{
	WritePrivateProfileString(INI_SECTION, lpszVal, NULL, INI_FILENAME);
}

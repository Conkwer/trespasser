
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "Reg.h"
#include "RegInit.hpp"
#include "IniFile.hpp"

#define REGKEYPARENT HKEY_LOCAL_MACHINE
#ifndef DEMO_BUILD
#define REGLOCATION "Software\\DreamWorks Interactive\\Trespasser"
#else
#define REGLOCATION "Software\\DreamWorks Interactive\\Trespasser Demo"
#endif

static const char* g_pszIniSection = "Settings";

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
	// KeyMap bytes — original default: W=run, X=walk, S=backup, A=left, D=right
	static BYTE defaultKeyMap[] = {
		0x01,0x00,0x00,0x00,0x40,0x00,0x00,0x00,
		0x02,0x00,0x00,0x00,0x80,0x00,0x00,0x00,
		0x10,0x00,0x00,0x00,0x10,0x00,0x00,0x00,
		0x11,0x00,0x00,0x00,0x08,0x00,0x00,0x00,
		0x20,0x00,0x00,0x00,0x20,0x00,0x00,0x00,
		0x51,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
		0x5A,0x00,0x00,0x00,0x02,0x00,0x00,0x00,
		0x45,0x00,0x00,0x00,0x00,0x01,0x00,0x00,
		0x46,0x00,0x00,0x00,0x00,0x04,0x00,0x00,
		0x52,0x00,0x00,0x00,0x00,0x02,0x00,0x00,
		0x57,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
		0x58,0x00,0x00,0x00,0x00,0x00,0x00,0x02,
		0x53,0x00,0x00,0x00,0x00,0x00,0x00,0x04,
		0x41,0x00,0x00,0x00,0x00,0x00,0x00,0x08,
		0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x10
	};

	SetRegString(REG_KEY_DATA_DRIVE,        ".\\");
	SetRegString(REG_KEY_INSTALLED_DIR,     ".\\");

	SetRegValue(REG_KEY_INSTALLED,            1);
	SetRegValue(REG_KEY_SAFEMODE,             0);
	SetRegValue(strAUTOSETTINGS,              1);
	SetRegValue(strFLAG_REGINIT,              1);

	// Display defaults: 640x480 software
	SetRegValue(strFLAG_FULLSCREEN,           1);
	SetRegValue(strFLAG_D3D,                  0);
	SetRegValue(strFLAG_SYSTEMMEM,            0);
	SetRegValue(strSIZE_WIDTH,                640);
	SetRegValue(strSIZE_HEIGHT,               480);
	SetRegValue(strD3D_DITHER,                1);
	SetRegValue(strTRIPLEBUFFER,              0);
	SetRegValue(strPAGEMANAGED,               1);
	SetRegValue(strHARDWARE_WATER,            0);
	SetRegValue(strZBUFFER_BITDEPTH,          37);
	SetRegValue(strRECOMMENDEDTEXMAX,         256);
	SetRegValue(strD3D_FILTERCACHES,          0);
	SetRegValue(REG_KEY_NOVIDEO,              0);
	SetRegValue(REG_KEY_RENDERING_QUALITY,    2);
	SetRegValue(REG_KEY_GAMMA,                3);

	// Audio: all on by default
	SetRegValue(REG_KEY_AUDIO_EFFECT,         -1);
	SetRegValue(REG_KEY_AUDIO_AMBIENT,        -1);
	SetRegValue(REG_KEY_AUDIO_VOICEOVER,      -1);
	SetRegValue(REG_KEY_AUDIO_MUSIC,          -1);
	SetRegValue(REG_KEY_AUDIO_SUBTITLES,      0);
	SetRegValue(REG_KEY_AUDIO_ENABLE3D,       -1);

	// Background music
	SetRegValue("EnableBackgroundMusic",       1);

	// Gameplay
	SetRegValue(REG_KEY_GORE,                 3);
	SetRegValue(REG_KEY_INVERTMOUSE,          0);
	SetRegValue(REG_KEY_VIEWPORT_X,           0);
	SetRegValue(REG_KEY_VIEWPORT_Y,           0);

	SetRegData(REG_KEY_KEYMAP, defaultKeyMap, sizeof(defaultKeyMap));

	// Misc
	SetRegValue(REG_KEY_AUTOLOAD,             0);
	SetRegValue(REG_KEY_AUTOSAVE,             0);
	SetRegValue(REG_KEY_DDRAW_CERT_IGNORE,    1);
}

void OpenKey()
{
	// Dummy handle — some code checks g_hKey before proceeding.
	g_hKey = (HKEY)1;

	// If no config path set by CLI, use default
	if (g_szConfigPath[0] == '\0')
		InitDefaultConfigPath();

	// Check if INI exists — write defaults if not.
	if (GetFileAttributes(g_szConfigPath) == 0xFFFFFFFF)
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
	WritePrivateProfileString(NULL, NULL, NULL, g_szConfigPath);
}

// --- Integer values ---

void SetRegValue(LPCSTR lpszVal, int nVal)
{
	char szVal[32];
	wsprintf(szVal, "%d", nVal);
	WritePrivateProfileString(g_pszIniSection, lpszVal, szVal, g_szConfigPath);
}

int GetRegValue(LPCSTR lpszVal, int nDefault)
{
	char szDefault[32];
	wsprintf(szDefault, "%d", nDefault);
	return GetPrivateProfileInt(g_pszIniSection, lpszVal, nDefault, g_szConfigPath);
}

// --- String values ---

void SetRegString(LPCSTR lpszVal, LPCSTR lpszString)
{
	WritePrivateProfileString(g_pszIniSection, lpszVal, lpszString, g_szConfigPath);
}

int GetRegString(LPCSTR lpszVal, LPSTR lpszString, int nSize, LPCSTR lpszDefault)
{
	GetPrivateProfileString(g_pszIniSection, lpszVal, lpszDefault, lpszString, nSize, g_szConfigPath);
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
	WritePrivateProfileString(g_pszIniSection, lpszVal, NULL, g_szConfigPath);
}

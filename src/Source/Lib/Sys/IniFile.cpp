#include "IniFile.hpp"
#include <windows.h>
#include <stdio.h>

void InitDefaultConfigPath()
{
	// Prefer .cfg, fall back to .ini for backward compat
	if (GetFileAttributes(".\\trespass.cfg") != 0xFFFFFFFF)
		lstrcpy(g_szConfigPath, ".\\trespass.cfg");
	else if (GetFileAttributes(".\\trespass.ini") != 0xFFFFFFFF)
		lstrcpy(g_szConfigPath, ".\\trespass.ini");
	else
		lstrcpy(g_szConfigPath, ".\\trespass.cfg");
}

char g_szConfigPath[MAX_PATH] = "";

IniFile::IniFile()
{
	// Use the global config path with "Settings" section
	lstrcpy(m_szFilepath, g_szConfigPath);
	lstrcpy(m_szSection, "Settings");
}

IniFile::IniFile(const char* filename, const char* section)
{
	// Resolve relative path to absolute
	if (filename[0] == '.' && filename[1] == '\\')
	{
		GetCurrentDirectory(MAX_PATH, m_szFilepath);
		lstrcat(m_szFilepath, "\\");
		lstrcat(m_szFilepath, filename + 2);
	}
	else if (filename[0] != '\\' && filename[1] != ':')
	{
		// Relative path without .\ prefix
		GetCurrentDirectory(MAX_PATH, m_szFilepath);
		lstrcat(m_szFilepath, "\\");
		lstrcat(m_szFilepath, filename);
	}
	else
	{
		lstrcpy(m_szFilepath, filename);
	}

	lstrcpy(m_szSection, section);
}

IniFile::~IniFile()
{
}

void IniFile::deleteValue(const char* key)
{
	WritePrivateProfileString(m_szSection, key, NULL, m_szFilepath);
}

int IniFile::getInt(const char* key, int defaultvalue) const
{
	return GetPrivateProfileInt(m_szSection, key, defaultvalue, m_szFilepath);
}

void IniFile::setInt(const char* key, int value)
{
	char buf[32];
	wsprintf(buf, "%d", value);
	WritePrivateProfileString(m_szSection, key, buf, m_szFilepath);
}

int IniFile::getBinary(const char* key, void* buffer, size_t buffersize) const
{
	if (!GetPrivateProfileStruct(m_szSection, key, buffer, buffersize, m_szFilepath))
		return 0;
	return (int)buffersize;
}

void IniFile::setBinary(const char* key, void* buffer, size_t buffersize)
{
	WritePrivateProfileStruct(m_szSection, key, buffer, buffersize, m_szFilepath);
}

int IniFile::getString(const char* key, char* buffer, size_t buffersize, const char* defaultvalue) const
{
	return GetPrivateProfileString(m_szSection, key, defaultvalue, buffer, buffersize, m_szFilepath);
}

void IniFile::setString(const char* key, const char* string)
{
	WritePrivateProfileString(m_szSection, key, string, m_szFilepath);
}

float IniFile::getFloat(const char* key, float fDefault) const
{
	float fVal = fDefault;
	int iSize = getBinary(key, &fVal, sizeof(fVal));
	if (iSize != sizeof(fVal))
		return fDefault;
	return fVal;
}

void IniFile::setFloat(const char* key, float fVal)
{
	setBinary(key, &fVal, sizeof(fVal));
}

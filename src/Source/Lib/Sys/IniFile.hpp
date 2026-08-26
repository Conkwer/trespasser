#ifndef HEADER_LIB_SYS_INIFILE_HPP
#define HEADER_LIB_SYS_INIFILE_HPP

#include <windows.h>

// Simple INI/CFG file reader using Win32 PrivateProfile API.
// VC6 C++98 compatible.

class IniFile
{
public:
	IniFile();
	IniFile(const char* filename, const char* section);
	~IniFile();

	void deleteValue(const char* key);
	int  getInt(const char* key, int defaultvalue) const;
	void setInt(const char* key, int value);
	int  getBinary(const char* key, void* buffer, size_t buffersize) const;
	void setBinary(const char* key, void* buffer, size_t buffersize);
	int  getString(const char* key, char* buffer, size_t buffersize, const char* defaultvalue) const;
	void setString(const char* key, const char* string);
	float getFloat(const char* key, float fDefault) const;
	void setFloat(const char* key, float fVal);

private:
	char m_szFilepath[MAX_PATH];
	char m_szSection[128];
};

// Global config file path, set by CLI -config or defaults to <exe dir>\trespass.cfg
extern char g_szConfigPath[MAX_PATH];

// Initialize g_szConfigPath with default file (trespass.cfg or trespass.ini)
void InitDefaultConfigPath();

#endif

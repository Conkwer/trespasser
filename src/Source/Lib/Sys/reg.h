

void  OpenKey ();
void  CloseKey(BOOL b_change_safemode = TRUE);
bool  IsHardcore();
void  DeleteValue(LPCSTR lpszVal);
void  SetRegValue (LPCSTR lpszVal, int nVal);
int   GetRegValue (LPCSTR lpszVal, int nDefault);
void  SetRegString (LPCSTR lpszVal, LPCSTR lpszString);
int   GetRegString (LPCSTR lpszVal, LPSTR lpszString, int nSize, LPCSTR lpszDefault);
void  SetRegData (LPCSTR lpszVal, LPBYTE lpszData, int nSize);
int   GetRegData (LPCSTR lpszVal, LPBYTE lpszData, int nSize);
void  SetRegFloat(LPCSTR lpszVal, float fVal);
float GetRegFloat(LPCSTR lpszVal, float fDefault);
void  SetModValue(LPCSTR lpszVal, int nVal);
int   GetModValue(LPCSTR lpszVal, int nDefault);
float GetModFloat(LPCSTR lpszVal, float fDefault);
void  SetModString(LPCSTR lpszVal, LPCSTR lpszString);
int   GetModString(LPCSTR lpszVal, LPSTR lpszString, int nSize, LPCSTR lpszDefault);
extern BOOL bSafeModeReg;

void DisableSafemode();
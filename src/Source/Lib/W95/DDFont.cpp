/***********************************************************************************************
 *
 * Copyright � DreamWorks Interactive. 1998
 *
 * Contents:
 *		Implementation of CDDFont
 *
 * Bugs:
 *
 * To do:
 *
 ***********************************************************************************************
 *
 * $Log:: /JP2_PC/Source/Lib/W95/DDFont.cpp                                                    $
 * 
 * 4     9/30/98 2:11a Rwyatt
 * Added 3D effect to text to help contrast
 * 
 * 3     8/21/98 5:00p Rwyatt
 * Now centers subtitles properly on a reduced screen size
 * 
 * 2     2/11/98 4:36p Rwyatt
 * Subtitles now use GDI so it works on all video and 3D cards.
 * 
 * 1     2/06/98 8:22p Rwyatt
 * Initial implementation
 * 
 ***********************************************************************************************/

#include "Common.hpp"
#include "Lib/W95/WinInclude.hpp"
#include "Lib/View/RasterVid.hpp"
#include "Lib/Sys/Profile.hpp"
#include "DDFont.hpp"


extern rptr<CRasterWin> prasMainScreen;


// AddFontResourceExW was introduced in the Win2000 SDK so it is not declared in the
// VC6 headers; resolve it at run time (the game runs on XP or later where it exists).
#define FR_PRIVATE		0x10

typedef int (WINAPI * PFNADDFONTRESOURCEEXW)(LPCWSTR, DWORD, PVOID);

static PFNADDFONTRESOURCEEXW pfnAddFontResourceExW()
{
	static PFNADDFONTRESOURCEEXW pfn = NULL;

	if (pfn == NULL)
	{
		HMODULE h_gdi = GetModuleHandle("gdi32.dll");

		if (h_gdi)
		{
			pfn = (PFNADDFONTRESOURCEEXW)GetProcAddress(h_gdi, "AddFontResourceExW");
		}
	}

	return pfn;
}


//**********************************************************************************************
//
CDDFont::CDDFont
(
	int32	i4_size
)
//*************************************
{
	uint8	u1;

	u4Colour = 0x00ffffff;
	bFill = false;

	// create the Win32 Font
	hfont = CreateFont(i4_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
						ANSI_CHARSET,
						OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,
						DEFAULT_QUALITY,   // NONANTIALIASED_QUALITY - Undocumented
						VARIABLE_PITCH,
						"Arial" );

	// load the multilingual TTF font for UTF-8 text (subtitles etc.)
	hfontTTF = LoadTTFFont(i4_size);

	// get a DC for the screen
	HDC	hdc_temp = GetDC(NULL);
	Assert(hdc_temp);
	
	// select our selected font into the screen DC
	void* hfont_temp_old = SelectObject(hdc_temp,hfont);

	SIZE	size_ch;

	i4YHeight = 0;

	for (uint32 u4_font_char = 32; u4_font_char<255; u4_font_char++)
	{
		u1 = (uint8)u4_font_char;

		// get the size of the current character
		GetTextExtentPoint(hdc_temp, (char*)&u1, 1, &size_ch);

		u4FontWidth[u4_font_char] = size_ch.cx;

		// get the height of the biggest character...
		if (size_ch.cy>i4YHeight)
		{
			i4YHeight = size_ch.cy;
		}
	}

	// tidy up the DC for the screen
	SelectObject(hdc_temp, hfont_temp_old);

	// release the DC for the screen
	ReleaseDC(NULL,hdc_temp);
}


//**********************************************************************************************
//
CDDFont::~CDDFont
(
)
//*************************************
{
	DeleteObject(hfont);

	if (hfontTTF)
	{
		DeleteObject(hfontTTF);
	}
}


//**********************************************************************************************
// Load HackGenConsoleNF-Regular.ttf from the game, data or override directory, register it
// with GDI and create a font from it. Returns the font handle or NULL if the file is not
// present, in which case UTF-8 text falls back to the regular GDI font.
//
void* CDDFont::LoadTTFFont
(
	int32	i4_size
)
//*************************************
{
	static const char* asz_paths[] =
	{
		"HackGenConsoleNF-Regular.ttf",
		"data\\HackGenConsoleNF-Regular.ttf",
		"override\\HackGenConsoleNF-Regular.ttf",
	};

	uint32	u4;
	char	asz_found[_MAX_PATH] = "";

	for (u4 = 0; u4 < 3; u4++)
	{
		if (GetFileAttributes(asz_paths[u4]) != 0xFFFFFFFF)
		{
			lstrcpy(asz_found, asz_paths[u4]);
			break;
		}
	}

	if (asz_found[0] == 0)
	{
		return NULL;
	}

	// register the font with GDI (private to this process)
	wchar_t	awz_path[_MAX_PATH];
	MultiByteToWideChar(CP_ACP, 0, asz_found, -1, awz_path, _MAX_PATH);

	PFNADDFONTRESOURCEEXW pfn_add_font = pfnAddFontResourceExW();

	if (pfn_add_font == NULL || pfn_add_font(awz_path, FR_PRIVATE, 0) == 0)
	{
		return NULL;
	}

	//
	// Read the family name from the TTF name table so we can create a font from it.
	//
	HANDLE	h_file = CreateFile(asz_found, GENERIC_READ, FILE_SHARE_READ, NULL,
								OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);

	if (h_file == INVALID_HANDLE_VALUE)
	{
		return NULL;
	}

	wchar_t	awz_family[128] = L"";
	DWORD	u4_bytes;

	// sfnt header: version (4), number of tables (2)
	uint16	au1_sfnt[3];

	if (ReadFile(h_file, au1_sfnt, 6, &u4_bytes, NULL) && u4_bytes == 6)
	{
		uint16	u1_num_tables = au1_sfnt[2];

		// table directory: 16 bytes per table
		struct STableDir
		{
			uint32	u4Tag;
			uint32	u4Checksum;
			uint32	u4Offset;
			uint32	u4Length;
		};

		uint32	u4_name_off = 0;
		uint16	u1;

		for (u1 = 0; u1 < u1_num_tables; u1++)
		{
			STableDir	td;

			if (!ReadFile(h_file, &td, sizeof(td), &u4_bytes, NULL) || u4_bytes != sizeof(td))
			{
				break;
			}

			if (td.u4Tag == 'name')
			{
				u4_name_off = td.u4Offset;
				break;
			}
		}

		if (u4_name_off != 0)
		{
			SetFilePointer(h_file, u4_name_off, NULL, FILE_BEGIN);

			uint16	au1_name_hdr[3];	// format, count, stringOffset

			if (ReadFile(h_file, au1_name_hdr, 6, &u4_bytes, NULL) && u4_bytes == 6)
			{
				uint16	u1_count = au1_name_hdr[1];
				uint16	u1_str_off = au1_name_hdr[2];

				uint16	u1_family_rec = 0xFFFF;		// index of the family name record
				uint16	u1_family_len = 0;
				uint16	u1_family_off = 0;
				bool	b_typed = false;			// found a typographic family (id 16)

				for (u1 = 0; u1 < u1_count; u1++)
				{
					// record: platform, encoding, language, nameID, length, offset
					uint16	au1_rec[6];

					if (!ReadFile(h_file, au1_rec, 12, &u4_bytes, NULL) || u4_bytes != 12)
					{
						break;
					}

					// Windows platform only, family (1) or typographic family (16)
					if (au1_rec[0] != 3 || (au1_rec[3] != 1 && au1_rec[3] != 16))
					{
						continue;
					}

					bool	b_is_typed	= (au1_rec[3] == 16);
					bool	b_us		= (au1_rec[2] == 0x409);

					// prefer the typographic family and the US english variant
					if (u1_family_rec == 0xFFFF || (b_is_typed && !b_typed) ||
						(b_is_typed == b_typed && b_us))
					{
						u1_family_rec	= u1;
						u1_family_len	= au1_rec[4];
						u1_family_off	= au1_rec[5];
						b_typed			= b_is_typed;
					}
				}

				if (u1_family_rec != 0xFFFF && u1_family_len < sizeof(awz_family))
				{
					// the string is UTF-16 big endian, convert it
					SetFilePointer(h_file, u4_name_off + u1_str_off + u1_family_off, NULL, FILE_BEGIN);

					uint16	au1_str[64];

					if (ReadFile(h_file, au1_str, u1_family_len, &u4_bytes, NULL) && u4_bytes == u1_family_len)
					{
						uint16	u1_chars = u1_family_len / 2;

						for (u1 = 0; u1 < u1_chars; u1++)
						{
							awz_family[u1] = (wchar_t)((au1_str[u1] << 8) | (au1_str[u1] >> 8));
						}

						awz_family[u1_chars] = 0;
					}
				}
			}
		}
	}

	CloseHandle(h_file);

	if (awz_family[0] == 0)
	{
		return NULL;
	}

	return CreateFontW(i4_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
						DEFAULT_CHARSET,
						OUT_DEFAULT_PRECIS,
						CLIP_DEFAULT_PRECIS,
						DEFAULT_QUALITY,
						VARIABLE_PITCH,
						awz_family);
}


//**********************************************************************************************
// Print a UTF-8 string with the TTF font. The text is word wrapped, centered and anchored
// to the bottom or top of the screen depending on the formatting flags.
//
void CDDFont::PrintUTF8String
(
	const char*	str_utf8,
	uint32		u4_flags,
	uint32		u4_row
)
//*************************************
{
	if (prasMainScreen == NULL)
	{
		return;
	}

	void*	hfont_use = (hfontTTF != NULL) ? hfontTTF : hfont;

	// convert the UTF-8 string to UTF-16
	int32	i4_len = MultiByteToWideChar(CP_UTF8, 0, str_utf8, -1, NULL, 0);

	if (i4_len <= 1)
	{
		return;
	}

	wchar_t*	pwz_str = new wchar_t[i4_len];
	MultiByteToWideChar(CP_UTF8, 0, str_utf8, -1, pwz_str, i4_len);

	prasMainScreen->Unlock();

	HDC	hdc = prasMainScreen->hdcGet();

	SetTextColor(hdc, u4Colour);

	if (bFill)
	{
		SetBkColor(hdc, 0);
		SetBkMode(hdc, OPAQUE);
	}
	else
	{
		SetBkMode(hdc, TRANSPARENT);
	}

	void* hfont_old = SelectObject(hdc, hfont_use);

	int32	i4_scr_width	= prasMainScreen->iWidth - (TEXT_BORDER_X * 2);
	int32	i4_left			= TEXT_BORDER_X;
	int32	i4_right		= TEXT_BORDER_X + i4_scr_width;
	int32	i4_top;
	int32	i4_bottom;

	// shift the line up by u4_row line-heights so stacked subtitles don't overlap
	int32	i4_yoff		= 0;

	if (u4_row)
	{
		TEXTMETRIC	tm;

		if (GetTextMetrics(hdc, &tm))
			i4_yoff = (int32)u4_row * (tm.tmHeight + tm.tmExternalLeading);
	}

	uint32	u4_dt_flags = DT_CENTER | DT_WORDBREAK | DT_NOPREFIX;

	if (u4_flags & TEXT_FORMAT_TOP)
	{
		i4_top		= TEXT_BORDER_Y;
		i4_bottom	= prasMainScreen->iHeight;
	}
	else
	{
		// subtitles are anchored to the bottom of the screen by default.
		// DT_BOTTOM requires DT_SINGLELINE, so the text height is measured
		// first with a scratch rect and the draw rect is positioned after.
		// (DT_CALCRECT modifies the rect in place, it must not be reused.)
		RECT	rc_measure = { i4_left, 0, i4_right, prasMainScreen->iHeight };

		int32	i4_h = DrawTextW(hdc, pwz_str, -1, &rc_measure, u4_dt_flags | DT_CALCRECT);

		i4_bottom	= prasMainScreen->iHeight - TEXT_BORDER_Y - i4_yoff;
		i4_top		= i4_bottom - i4_h;
	}

	RECT	rc = { i4_left, i4_top, i4_right, i4_bottom };

	DrawTextW(hdc, pwz_str, -1, &rc, u4_dt_flags);

	delete pwz_str;

	if (hfont_old)
	{
		SelectObject(hdc, hfont_old);
	}

	prasMainScreen->ReleaseDC(hdc);
}


//**********************************************************************************************
//
void CDDFont::PrintOverlayString
(
	int32 i4_xp, 
	int32 i4_yp, 
	const char* str
)
//*************************************
{
	CCycleTimer ctmr;

	prasMainScreen->Unlock();

	HDC	hdc = prasMainScreen->hdcGet();

	SetTextColor(hdc, u4Colour );
	if (bFill)
	{
		SetBkColor(hdc,0);		
		//
		// We must set the background back to opaque as a DD surface has a unique DC whcih is
		// returned every time you ask for a DC (similar to the OWNDC flag in windows). If
		// you cange the state of the DC it remains across DC calls.
		//
		SetBkMode(hdc, OPAQUE);
	}
	else
	{
		SetBkMode(hdc, TRANSPARENT);
	}

	void* hfont_old = SelectObject(hdc,hfont);
	Assert(hfont_old);

	TextOut(hdc,i4_xp,i4_yp,str,strlen(str));

	SelectObject(hdc,hfont_old);

	prasMainScreen->ReleaseDC(hdc);

	proProfile.psOverlayText.Add(ctmr());
}




//**********************************************************************************************
//
uint8* CDDFont::pu1FormatOverlayString
(
	const char*	str_string,
	uint8*		str_buff,
	uint32		u4_flags
)
//*************************************
{
	char*	str = (char*)str_buff;
	int32*	pi4_lines;
	int32*	pi4_bytes;

	SFormattedLine*	apfmtl_lines[32];
	SFormattedLine* pfmtl;
	int32	i4_count;

	// width and height of the screen area we are going to draw to...
	int32	i4_scr_width	= prasMainScreen->iWidth - (TEXT_BORDER_X*2);
	int32	i4_x_size;
	int32	i4_y_pos = TEXT_BORDER_Y;// - prasMainScreen->iOffsetY;
	char	ch;
	int32	i4_lines = 0;

	char*	str_dst_break;
	char*	str_src_break;
	bool	b_break;
	int32	i4_break_size;
	int32	i4_break_count;

	pi4_bytes = (int32*)(str);
	str += sizeof(int32);

	pi4_lines = (int32*)(str);
	str += sizeof(int32);

	while (*str_string)
	{
		pfmtl = (SFormattedLine*)str;
		str+=sizeof(SFormattedLine);

		i4_x_size = 0;

		b_break		  = false;
		str_dst_break = NULL;
		str_src_break = NULL;
		i4_break_size = 0;
		i4_count = 0;
		i4_break_count = 0;

		while (*str_string)
		{
			ch = *str_string;

			if (ch==13)
			{
				str_string++;
				break;
			}

			if (i4_x_size+u4FontWidth[ch]>=i4_scr_width)
			{
				break;
			}

			i4_x_size += u4FontWidth[ch];

			*str = ch;
			i4_count ++;

			if (*str == ' ' || *str == ',' || *str == ';' || *str == ':' || *str == '-')
			{
				str_dst_break = (char*)str+1;
				str_src_break = (char*)str_string+1;
				i4_break_size = i4_x_size;
				i4_break_count = i4_count;
				b_break = true;
			}
			else
			{
				b_break = false;
			}

			str++;
			str_string++;
		}

		if (b_break || (str_dst_break == NULL) || (*str_string == NULL))
		{
			// the last charatcer we emmited was a character suitable for a break character,
			// terminate the current string
			*str = 0;
			*str++;
		}
		else
		{
			// we must go back to the last break position.
			*str_dst_break = 0;
			str = str_dst_break+1;
			str_string = str_src_break;
			i4_x_size = i4_break_size;
			i4_count = i4_break_count;
		}

		//
		// fill in the position formatting the string in the correct way
		//
		if (u4_flags & TEXT_FORMAT_LEFT)
		{
			pfmtl->u4XPos = TEXT_BORDER_X + prasMainScreen->iOffsetX;
		}
		else if (u4_flags & TEXT_FORMAT_CENTER)
		{
			pfmtl->u4XPos = TEXT_BORDER_X + prasMainScreen->iOffsetX + (i4_scr_width - i4_x_size)/2;
		}
		else if (u4_flags & TEXT_FORMAT_RIGHT)
		{
			pfmtl->u4XPos = prasMainScreen->iWidth - i4_x_size - TEXT_BORDER_X + prasMainScreen->iOffsetX;
		}
		else
		{
			Assert(0);
		}

		pfmtl->u4TextLen = (uint32)i4_count;
		pfmtl->u4YPos = i4_y_pos + prasMainScreen->iOffsetY;
		apfmtl_lines[i4_lines] = pfmtl;
		i4_lines++;
		i4_y_pos+=i4YHeight;
	}

	// if we are formatting at the bottom of the screen, move all the subtitles down..
	if (u4_flags & TEXT_FORMAT_BOTTOM)
	{
		// text_y_pos is the bottom of the last scan line drawn....
		// adjust all the Y positions
		for (int i = 0; i<i4_lines; i++)
		{
			apfmtl_lines[i]->u4YPos += (prasMainScreen->iHeight-i4_y_pos - TEXT_BORDER_Y);
		}
	}

	// fill in the number of lines
	*pi4_lines = i4_lines;

	// number of bytes taken is the difference between the start and end pointers
	*pi4_bytes = (int32)((uint32)str-(uint32)str_buff);

	// return the current address as the next address that can be used...
	return (uint8*)str;
}




//**********************************************************************************************
//
void CDDFont::PrintFormattedOverlayString
(
	uint8*	pu1_data
)
//*************************************
{
	int32			i4_lines;
	SFormattedLine* pfmtl;

	CCycleTimer		ctmr;

	//skip the byte count dword - we do not need it.
	pu1_data+=sizeof(int32);

	// lines is the second dword
	i4_lines = *(int32*)(pu1_data);
	pu1_data+=sizeof(int32);


	//
	// Setup the device context for drawing to
	//
	prasMainScreen->Unlock();
	HDC	hdc = prasMainScreen->hdcGet();

	if (bFill)
	{
		SetBkColor(hdc,0);		
		SetBkMode(hdc, OPAQUE);
	}
	else
	{
		SetBkMode(hdc, TRANSPARENT);
	}

	void* hfont_old = SelectObject(hdc,hfont);
	Assert(hfont_old);

	//
	// After the string header is a formatted section..
	//
	while (i4_lines)
	{
		pfmtl = (SFormattedLine*)pu1_data;
		pu1_data+=sizeof(SFormattedLine);

		SetTextColor(hdc, 0);
		TextOut(hdc,pfmtl->u4XPos-1,pfmtl->u4YPos-1,(char*)pu1_data,pfmtl->u4TextLen);

		SetTextColor(hdc, u4Colour );
		TextOut(hdc,pfmtl->u4XPos,pfmtl->u4YPos,(char*)pu1_data,pfmtl->u4TextLen);

		// point to the next line header, skip the text len + 1 for the 0
		pu1_data+=pfmtl->u4TextLen+1;
		i4_lines--;
	}


	//
	// Put the DD device context back to how it was
	//
	SelectObject(hdc,hfont_old);
	prasMainScreen->ReleaseDC(hdc);

	proProfile.psOverlayText.Add(ctmr());
}

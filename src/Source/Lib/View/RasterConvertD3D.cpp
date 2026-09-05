/***********************************************************************************************
 *
 * Copyright (C) DreamWorks Interactive, 1998.
 *
 * Contents:
 *		Implementation of 'RasterConvertD3D.hpp.'
 *
 ***********************************************************************************************
 *
 * $Log:: /JP2_PC/Source/Lib/View/RasterConvertD3D.cpp                                         $
 * 
 * 8     9/27/98 10:02p Pkeet
 * Added a global function to reset conversion tables.
 * 
 * 7     8/28/98 9:22p Pkeet
 * Added code to expand the width or heigh automatically if the d3d raster has a dimension
 * exactly twice as large as the memory raster. This is for the Permedia 2 chip-based cards
 * which have a minimum dimension of 16, but must store Trespasser rasters which can have a
 * minimum dimension of 8.
 * 
 * 6     8/10/98 5:51p Pkeet
 * Added the 'ed3dcom565_RGB' common format.
 * 
 * 5     8/05/98 10:29p Pkeet
 * Put in a safety valve for render caches that are wrongly being converted.
 * 
 * 4     8/01/98 4:44p Pkeet
 * Added a background colour for transparent textures.
 * 
 * 3     7/27/98 10:51p Pkeet
 * Added timers for raster conversions.
 * 
 * 2     7/23/98 9:52p Pkeet
 * Made conversions faster.
 * 
 * 1     7/23/98 6:18p Pkeet
 * Initial implementation.
 * 
 **********************************************************************************************/

//
// Necessary includes.
//
#include "Set.h"
#include "Memory.h"
#include "Common.hpp"
#include "RasterConvertD3D.hpp"

#include "Raster.hpp"
#include "RasterD3D.hpp"
#include "Lib/Sys/Profile.hpp"

#include <stdio.h>
#include <string.h>


//
// Module specific variables.
//
static CProfileStat psUploadConversion ("Conversion", &proHardware.psUploads);


//
// Class definitions.
//

//**********************************************************************************************
//
class CRasterConvert
//
// A raster conversion object.
//
// Prefix: rc
//
//**************************************
{
private:

	uint32          u4HashValue;
	uint16*         pu2ConversionTable;
	CPal*           ppalPalette;
	ED3DTextureType ed3dtexType;

public:

	//******************************************************************************************
	//
	// Constructors.
	//

	// Default constructor.
	CRasterConvert()
		: u4HashValue(0), pu2ConversionTable(0), ppalPalette(0)
	{
	}

	// Copy constructor.
	CRasterConvert(const CRasterConvert& rc)
	{
		memcpy(this, &rc, sizeof(rc));
	}

	// Reference constructor.
	CRasterConvert(CRasterD3D* pras_d3d, rptr<CRaster> pras_mem)
		: pu2ConversionTable(0)
	{
		Assert(pras_d3d);
		Assert(pras_mem);

		// Extract the palette.
		ppalPalette = pras_mem->pxf.ppalAttached;
		AlwaysAssert(ppalPalette);

		// Get the texture type.
		ed3dtexType = pras_d3d->ed3dtexGet();

		// Construct the hash value from the palette's CURRENT content. Do NOT use the
		// lazy-cached u4GetHashValue(): a shared/reused palette object is reloaded with
		// new content per texture, but its cached hash never updates, so the SetConv
		// cache would hand back the wrong (stale) conversion table -> colour garble.
		u4HashValue  = uint32(ed3dtexType);
		for (uint u = 0; u < ppalPalette->aclrPalette.uLen; ++u)
			u4HashValue ^= ppalPalette->aclrPalette[u].u4Value << (u4HashValue & 2);
		u4HashValue ^= (uint32)(unsigned)ppalPalette;
	}

	// Destructor.
	~CRasterConvert()
	{
		delete[] pu2ConversionTable;
	}

	//******************************************************************************************
	//
	// Member functions.
	//

	//******************************************************************************************
	//
	uint32 u4GetHashValue
	(
	) const
	//
	// Returns the hash identifier for this object.
	//
	//**************************************
	{
		return u4HashValue;
	}

	//******************************************************************************************
	//
	uint16* pu2GetConversionTable
	(
		CColour clr
	)
	//
	// Returns the hash identifier for this object.
	//
	//**************************************
	{
		Assert(ppalPalette);

		// Get the common conversion type.
		ED3DCommonFormat d3dcom = d3dDriver.d3dcomGetCommonFormat(ed3dtexType);

		// CONV-DIAG: log the common format + first palette entries (color-corruption hunt).
		static int s_convDiag = 0;
		if (s_convDiag++ < 6)
		{
			CColour c0 = ppalPalette->aclrPalette.uLen > 0 ? ppalPalette->aclrPalette[0] : CColour(0,0,0);
			CColour c1 = ppalPalette->aclrPalette.uLen > 1 ? ppalPalette->aclrPalette[1] : CColour(0,0,0);
			CColour c2 = ppalPalette->aclrPalette.uLen > 2 ? ppalPalette->aclrPalette[2] : CColour(0,0,0);
			dprintf("D3D9 CONV[%d]: d3dcom=%d palLen=%d c0=(%d,%d,%d) c1=(%d,%d,%d) c2=(%d,%d,%d)\n",
				s_convDiag, (int)d3dcom, (int)ppalPalette->aclrPalette.uLen,
				c0.u1Red, c0.u1Green, c0.u1Blue, c1.u1Red, c1.u1Green, c1.u1Blue,
				c2.u1Red, c2.u1Green, c2.u1Blue);
		}


		// If the conversion table does not already exist, create one.
		if (!pu2ConversionTable)
		{
			// Allocate memory for the lookup table.
			pu2ConversionTable = new uint16[256];

			// For all entries in the palette, convert.
			switch (d3dcom)
			{
				case ed3dcom565_RGB:
					BuildOpaque565_RGB();
					break;

				case ed3dcom565_BGR:
					BuildOpaque565_BGR();
					break;

				case ed3dcom555_RGB:
					BuildOpaque555_RGB();
					break;

				case ed3dcom5551_BRGA:
					BuildTransparent5551_RGBA();
					break;

				case ed3dcom1555_ARGB:
					BuildTransparent1555_ARGB();
					break;

				case ed3dcomUnknown:
					BuildOpaque565_RGB();
					break;

				case ed3dcom4444_ARGB:
				case ed3dcom4444_BRGA:
				default:
					AlwaysAssert(0);
			};
		}

		// Set colour for the conversion tables that require it.
		switch (d3dcom)
		{
			case ed3dcom5551_BRGA:
				{
					uint16 u2_blue  = clr.u1Blue  >> 3;
					uint16 u2_green = clr.u1Green >> 3;
					uint16 u2_red   = clr.u1Red   >> 3;
					pu2ConversionTable[0] = (u2_blue << 1) | (u2_green << 6) | (u2_red << 11) | 0x0000;
				}
				break;

			case ed3dcom1555_ARGB:
				{
					uint16 u2_blue  = clr.u1Blue  >> 3;
					uint16 u2_green = clr.u1Green >> 3;
					uint16 u2_red   = clr.u1Red   >> 3;
					pu2ConversionTable[0] = u2_blue | (u2_green << 5) | (u2_red << 10) | 0x0000;
				}
				break;
		};

		return pu2ConversionTable;
	}

	//******************************************************************************************
	void BuildOpaque565_RGB()
	{
		for (int i = 0; i < ppalPalette->aclrPalette.uLen; ++i)
		{
			CColour clr = ppalPalette->aclrPalette[i];
			uint16 u2_blue  = (clr.u1Blue >> 3);
			uint16 u2_green = (clr.u1Green >> 2) << 5;
			uint16 u2_red   = (clr.u1Red >> 3) << 11;
			pu2ConversionTable[i] = u2_blue | u2_green | u2_red;
		}
	}

	//******************************************************************************************
	void BuildOpaque565_BGR()
	{
		for (int i = 0; i < ppalPalette->aclrPalette.uLen; ++i)
		{
			CColour clr = ppalPalette->aclrPalette[i];
			uint16 u2_blue  = (clr.u1Blue >> 3) << 11;
			uint16 u2_green = (clr.u1Green >> 2) << 5;
			uint16 u2_red   = (clr.u1Red >> 3);
			pu2ConversionTable[i] = u2_blue | u2_green | u2_red;
		}
	}

	//******************************************************************************************
	void BuildOpaque555_RGB()
	{
		for (int i = 0; i < ppalPalette->aclrPalette.uLen; ++i)
		{
			CColour clr = ppalPalette->aclrPalette[i];
			uint16 u2_blue  = (clr.u1Blue >> 3);
			uint16 u2_green = (clr.u1Green >> 3) << 5;
			uint16 u2_red   = (clr.u1Red >> 3) << 10;
			pu2ConversionTable[i] = u2_blue | u2_green | u2_red;
		}
	}

	//******************************************************************************************
	void BuildTransparent5551_RGBA()
	{
		pu2ConversionTable[0] = 0x0000;
		for (int i = 1; i < ppalPalette->aclrPalette.uLen; ++i)
		{
			CColour clr = ppalPalette->aclrPalette[i];
			uint16 u2_blue  = (clr.u1Blue >> 3) << 1;
			uint16 u2_green = (clr.u1Green >> 3) << 6;
			uint16 u2_red   = (clr.u1Red >> 3) << 11;
			pu2ConversionTable[i] = u2_blue | u2_green | u2_red | 0x0001;
		}
	}

	//******************************************************************************************
	void BuildTransparent1555_ARGB()
	{
		pu2ConversionTable[0] = 0x0000;
		for (int i = 1; i < ppalPalette->aclrPalette.uLen; ++i)
		{
			CColour clr = ppalPalette->aclrPalette[i];
			uint16 u2_blue  = (clr.u1Blue >> 3);
			uint16 u2_green = (clr.u1Green >> 3) << 5;
			uint16 u2_red   = (clr.u1Red >> 3) << 10;
			pu2ConversionTable[i] = u2_blue | u2_green | u2_red | 0x8000;
		}
	}

};


//**********************************************************************************************
//
class CRasterConvertLess
//
// A comparison class for 'CRasterConvert.'
//
//**************************************
{
public:

	//**************************************************************************************
	//
	bool operator()(const CRasterConvert& rc_0, const CRasterConvert& rc_1) const
	//
	// Returns 'true' if rc_0 has a lower hash value than rc_1.
	//
	//**************************************
	{
		// Return the results of the comparision.
		return rc_0.u4GetHashValue() < rc_1.u4GetHashValue();
	}

};


//
// Internal type definitions.
//

// Type of set used to store conversion table pointers.
typedef set<CRasterConvert, CRasterConvertLess> TSetConv;


//
// Local variable declarations.
//

// Conversion set.
TSetConv SetConv;


//
// Local functions.
//
 
//**********************************************************************************************
//
const uint16* u2ConversionGet
(
	CRasterD3D*   pras_d3d,
	rptr<CRaster> pras_mem,
	CColour       clr
)
//
// Returns a reference to the conversion object for a given pair of rasters.
//
//**************************************
{
	//
	// Make a conversion object of the correct type. Memory for the conversion table will not
	// be created until it is requireed.
	//
	CRasterConvert rc(pras_d3d, pras_mem);

	// See if a suitable conversion object already exists.
	TSetConv::iterator it = SetConv.find(rc);
	if (it != SetConv.end())
		return ((CRasterConvert&)(*it)).pu2GetConversionTable(clr);

	// Add the conversion object.
	SetConv.insert(rc);
	AlwaysAssert(SetConv.find(rc) != SetConv.end());

	// Return the newly added conversion object.
	return ((CRasterConvert&)(*SetConv.find(rc))).pu2GetConversionTable(clr);
}

//**********************************************************************************************
void ConvertRasterAlpha
(
	CRasterD3D*   pras_d3d,
	rptr<CRaster> pras_mem
)
{
	Assert(pras_d3d);
	Assert(pras_mem);

	// Lock surfaces.
	pras_d3d->Lock();

	uint16* pu2_d3d = (uint16*)pras_d3d->pSurface;
	uint16* pu2_mem = (uint16*)pras_mem->pSurface;

	int i_d3d_stride = pras_d3d->iLinePixels;
	int i_mem_stride = pras_mem->iLinePixels;

	int i_width  = min(pras_d3d->iWidth,  pras_mem->iWidth);
	int i_height = min(pras_d3d->iHeight, pras_mem->iHeight);

	// Dumb conversion for now.
	for (int j = 0; j < i_height; ++j)
	{
		for (int i = i_width - 1; i >= 0; --i)
		{
			pu2_d3d[i] = pu2_mem[i];
		}

		// Increment by strides.
		pu2_d3d += i_d3d_stride;
		pu2_mem += i_mem_stride;
	}

	// Unlock surfaces.
	pras_d3d->Unlock();
}

//**********************************************************************************************
void ConvertRaster(CRasterD3D* pras_d3d, rptr<CRaster> pras_mem, CColour clr)
{
	CCycleTimer ctmr;

	Assert(pras_d3d);
	Assert(pras_mem);

	if (pras_d3d->ed3dtexGet() == ed3dtexSCREEN_ALPHA || pras_mem->iPixelBits > 8)
	{
		ConvertRasterAlpha(pras_d3d, pras_mem);
		psUploadConversion.Add(ctmr(), pras_d3d->iSurfaceMemBytes());
		return;
	}

	// Get a suitable conversion object.
	const uint16* pu2 = u2ConversionGet(pras_d3d, pras_mem, clr);

	// Lock surfaces.
	pras_d3d->Lock();

	uint16* pu2_d3d = (uint16*)pras_d3d->pSurface;
	uint8*  pu1_mem = (uint8*)pras_mem->pSurface;

	int i_d3d_stride = pras_d3d->iLinePixels;
	int i_mem_stride = pras_mem->iLinePixels;

	int i_width      = min(pras_d3d->iWidth,  pras_mem->iWidth);
	int i_height     = min(pras_d3d->iHeight, pras_mem->iHeight);

	// CONVTEST: capture source indices + pu2 results for the first poly (is the
	// 8-bit->565 conversion table wrong, or is the source read wrong?)
	{
		static int s_cdiag = 0;
		if (s_cdiag < 20)
		{
			int i = s_cdiag++;
			int i_pal = (pras_mem->pxf.ppalAttached) ? (int)pras_mem->pxf.ppalAttached->aclrPalette.uLen : -1;
			unsigned u_palptr = (unsigned)pras_mem->pxf.ppalAttached;
			unsigned u_palhash = pras_mem->pxf.ppalAttached ? pras_mem->pxf.ppalAttached->u4GetHashValue() : 0;
			dprintf("CONVTEST[%d]: src=%p ed3dtex=%d d3dcom=%d palPtr=%08x palHash=%08x palLen=%d src_w=%d src_h=%d src_line=%d d3d_w=%d d3d_h=%d d3d_line=%d bits=%d/%d\n",
				i, pras_mem.ptGet(), (int)pras_d3d->ed3dtexGet(),
				(int)d3dDriver.d3dcomGetCommonFormat(pras_d3d->ed3dtexGet()),
				u_palptr, u_palhash, i_pal, pras_mem->iWidth, pras_mem->iHeight, pras_mem->iLinePixels,
				pras_d3d->iWidth, pras_d3d->iHeight, pras_d3d->iLinePixels, pras_d3d->iPixelBits, pras_mem->iPixelBits);
			for (int k = 0; k < 6 && k < i_width; ++k)
			{
				int sv = pu1_mem[k];
				CColour c = pras_mem->pxf.ppalAttached->aclrPalette.atArray[sv];
				dprintf("  src[%d]=%d pal=(%d,%d,%d) pu2=%04x\n", k, sv, c.u1Red, c.u1Green, c.u1Blue, (unsigned)pu2[sv]);
			}
		}
	}


	// SRC-DIAG: log geometry + whether source indices exceed the palette length (->garbage).
	{
		static const void* s_srcdiag[40];
		static int         s_srcdiag_n = 0;
		int   b_new = 1;
		for (int k = 0; k < s_srcdiag_n; ++k) if (s_srcdiag[k] == pras_mem.ptGet()) b_new = 0;
		if (b_new && s_srcdiag_n < 40)
		{
			s_srcdiag[s_srcdiag_n++] = pras_mem.ptGet();
			int i_pal = (pras_mem->pxf.ppalAttached) ? (int)pras_mem->pxf.ppalAttached->aclrPalette.uLen : -1;
			int i_oor = 0, i_max = 0;
			const uint8* p = pu1_mem;
			for (int x = 0; x < i_width; ++x) { int v = p[x]; if (v > i_max) i_max = v; if (v >= i_pal) ++i_oor; }
			dprintf("D3D9 SRC[%d]: src=%p w=%d h=%d line=%d bits=%d palLen=%d width=%d hgt=%d maxIdx=%d overPal=%d\n",
				s_srcdiag_n-1, pras_mem.ptGet(), pras_mem->iWidth, pras_mem->iHeight,
				pras_mem->iLinePixels, pras_mem->iPixelBits, i_pal,
				pras_d3d->iWidth, pras_d3d->iHeight, i_max, i_oor);

			// TDMP: write the 8-bit source indices + palette to files for visual inspection.
			if (s_srcdiag_n <= 30)
			{
				char sz_idx[64], sz_pal[64];
				sprintf(sz_idx, "/tmp/tdump_%d.idx", s_srcdiag_n-1);
				sprintf(sz_pal, "/tmp/tdump_%d.pal", s_srcdiag_n-1);
				FILE* f = fopen(sz_idx, "wb");
				if (f) {
					const uint8* ps = pu1_mem;
					for (int y = 0; y < i_height; ++y) { fwrite(ps, 1, i_width, f); ps += i_mem_stride; }
					fclose(f);
				}
				FILE* fp = fopen(sz_pal, "wb");
				if (fp && pras_mem->pxf.ppalAttached) {
					int n = pras_mem->pxf.ppalAttached->aclrPalette.uLen;
					unsigned char rgb[256*3];
					for (int k = 0; k < 256; ++k) {
						if (k < n) { CColour c = pras_mem->pxf.ppalAttached->aclrPalette.atArray[k];
							rgb[k*3]=c.u1Red; rgb[k*3+1]=c.u1Green; rgb[k*3+2]=c.u1Blue; }
						else { rgb[k*3]=0; rgb[k*3+1]=0; rgb[k*3+2]=0; }
					}
					fwrite(rgb, 1, sizeof(rgb), fp);
					fclose(fp);
				}
			}
		}
	}


	// Dumb conversion for now.
	for (int j = 0; j < i_height; ++j)
	{
		for (int i = i_width - 1; i >= 0; --i)
		{
			pu2_d3d[i] = pu2[pu1_mem[i]];
		}

		// Increment by strides.
		pu2_d3d += i_d3d_stride;
		pu1_mem += i_mem_stride;
	}

	// Expand the width if required.
	if (pras_d3d->iWidth == pras_mem->iWidth * 2)
	{
		pu2_d3d = (uint16*)pras_d3d->pSurface;

		for (int j = 0; j < i_height; ++j)
		{
			int i   = pras_d3d->iWidth / 2 - 1;
			int i_2 = pras_d3d->iWidth - 2;

			for (; i_2 >= 0; i_2 -= 2, --i)
			{
				uint16 u2      = pu2_d3d[i];
				pu2_d3d[i]     = u2;
				pu2_d3d[i + 1] = u2;
			}

			// Increment by strides.
			pu2_d3d += i_d3d_stride;
		}
	}

	// Expand the height if required.
	if (pras_d3d->iHeight == pras_mem->iHeight * 2)
	{
		pu2_d3d  = (uint16*)pras_d3d->pSurface;

		for (int j = pras_mem->iHeight - 1; j >= 0; --j)
		{
			int i;
			uint16* pu2_d3d2a = (uint16*)pras_d3d->pSurface;
			pu2_d3d2a += i_d3d_stride * j * 2;
			uint16* pu2_d3d2b = pu2_d3d2a + i_d3d_stride;

			for (i = 0; i < pras_d3d->iWidth - 1; ++i)
			{
				uint16 u2 = pu2_d3d[i];
				pu2_d3d2a[i] = u2;
				pu2_d3d2b[i] = u2;
			}

			// Increment by strides.
			pu2_d3d += i_d3d_stride;
		}
	}

	// Unlock surfaces.
	pras_d3d->Unlock();

	psUploadConversion.Add(ctmr(), pras_d3d->iSurfaceMemBytes());
}
 
//**********************************************************************************************
void ResetD3DConversions()
{
	SetConv.erase(SetConv.begin(), SetConv.end());
}
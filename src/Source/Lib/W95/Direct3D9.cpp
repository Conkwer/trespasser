/***********************************************************************************************
 *
 * Trespasser-Plus D3D9 driver (Track A of the D3D9 backend plan — see directx9_ideas.md).
 *
 * IMPORTANT: this TU must NOT include the game's DX6 headers (d3d.h, d3dtypes.h, ...) —
 * the vendored DirectX/d3d9.h defines colliding enum names. Under /YX automatic PCH the
 * FIRST #include of a .cpp becomes its own PCH, so keeping <DirectX/d3d9.h> first isolates
 * this file from the game's precomp.h/d3d.h entirely.
 *
 * d3d9.dll is loaded dynamically (LoadLibrary/GetProcAddress) — same pattern the game
 * already uses for DirectSound (Audio.cpp:1039). No DX9 SDK needed; the headers are
 * vendored from mingw-w64 (LGPL 2.1+, ABI-exact with the real d3d9.dll).
 *
 * Game-side code talks to this driver through Direct3D9API.h (no D3D9 types).
 *
 ***********************************************************************************************/

#include <DirectX/d3d9.h>
#include <windows.h>
#include <stdio.h>

#include "Direct3D9API.h"

extern void __cdecl dprintf(char* str, ...);

static HMODULE             s_hD3D9   = NULL;
static IDirect3D9*         s_pD3D9   = NULL;
static IDirect3DDevice9*   s_pDevice = NULL;
static D3DPRESENT_PARAMETERS s_pp;
static BOOL                s_bInited = FALSE;

// 2D present resources (opaque / additive overlay).
static IDirect3DTexture9*  s_pTex2D        = NULL;
static int                 s_iTex2DWidth   = 0;
static int                 s_iTex2DHeight  = 0;
static BOOL                s_bHardwareFrame = FALSE;

// Texture is stored pow2 (1024x512) so it works on any D3D9 device / wrapper;
// the visible area is scaled by these UVs.
#define D3D9_TEX_W   1024
#define D3D9_TEX_H   512

static void D3D9SetViewportFull(void);

// Log through the game's dprintf (dout -> DebugLog.txt in the game folder, flushed).
// NOTE: fopen-append to DebugLog.txt races with the game's buffered dout stream and
// writes to the process CWD (not the game folder) — never use it for diagnostics here.
static void D3D9Log(const char* psz)
{
	dprintf("D3D9: %s\n", (char*)psz);
}

BOOL D3D9Init(HWND hwnd, int iWidth, int iHeight, BOOL bFullScreen)
{
	typedef IDirect3D9* (WINAPI *pfnDirect3DCreate9)(UINT);

	if (s_bInited)
	{
		// Track the screen size: if the raster was resized (e.g. the config dialog
		// rewrote Width/Height and bCreateScreen re-ran), resize the windowed
		// backbuffer with a cheap windowed Reset. The upload texture is keyed to
		// the old size — drop it (it is MANAGED, so it would otherwise survive
		// the Reset and hold stale UVs).
		if ((int)s_pp.BackBufferWidth != iWidth || (int)s_pp.BackBufferHeight != iHeight)
		{
			s_pp.BackBufferWidth  = (UINT)iWidth;
			s_pp.BackBufferHeight = (UINT)iHeight;
			if (s_pTex2D)
			{
				s_pTex2D->Release();
				s_pTex2D = NULL;
				s_iTex2DWidth = 0;
				s_iTex2DHeight = 0;
			}
			if (s_pDevice)
			{
				s_pDevice->Reset(&s_pp);
				D3D9SetViewportFull();
			}
			D3D9Log("Init: device resized (windowed Reset)");
		}
		return TRUE;
	}

	if (!hwnd)
		return FALSE;

	s_hD3D9 = LoadLibraryA("d3d9.dll");
	if (!s_hD3D9)
	{
		D3D9Log("Init: d3d9.dll not found");
		return FALSE;
	}

	pfnDirect3DCreate9 pCreate =
		(pfnDirect3DCreate9)GetProcAddress(s_hD3D9, "Direct3DCreate9");
	if (!pCreate)
	{
		D3D9Log("Init: Direct3DCreate9 not exported");
		FreeLibrary(s_hD3D9);
		s_hD3D9 = NULL;
		return FALSE;
	}

	s_pD3D9 = pCreate(D3D_SDK_VERSION);
	if (!s_pD3D9)
	{
		D3D9Log("Init: Direct3DCreate9 failed");
		FreeLibrary(s_hD3D9);
		s_hD3D9 = NULL;
		return FALSE;
	}

	ZeroMemory(&s_pp, sizeof(s_pp));
	s_pp.BackBufferWidth  = (UINT)iWidth;
	s_pp.BackBufferHeight = (UINT)iHeight;
	s_pp.BackBufferFormat = D3DFMT_X8R8G8B8;
	s_pp.BackBufferCount  = 1;
	s_pp.MultiSampleType  = D3DMULTISAMPLE_NONE;
	s_pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
	s_pp.hDeviceWindow    = hwnd;
	s_pp.Windowed         = !bFullScreen;
	s_pp.EnableAutoDepthStencil = TRUE;
	s_pp.AutoDepthStencilFormat = D3DFMT_D16;
	s_pp.Flags = 0;
	s_pp.FullScreen_RefreshRateInHz = 0;
	s_pp.PresentationInterval      = D3DPRESENT_INTERVAL_DEFAULT;

	// Software vertex processing + FPU preserve: the game does all transform/light on the
	// CPU (RHW vertices) and uses the FPU heavily for physics — keep the FPU untouched.
	HRESULT hr = s_pD3D9->CreateDevice(
		D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
		&s_pp, &s_pDevice);

	if (FAILED(hr))
	{
		// Fallback: let the driver pick vertex processing.
		hr = s_pD3D9->CreateDevice(
			D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
			D3DCREATE_FPU_PRESERVE, &s_pp, &s_pDevice);
	}

	if (FAILED(hr))
	{
		char sz[96];
		wsprintf(sz, "Init: CreateDevice failed (hr=0x%08x)", hr);
		D3D9Log(sz);
		s_pD3D9->Release();
		s_pD3D9 = NULL;
		FreeLibrary(s_hD3D9);
		s_hD3D9 = NULL;
		return FALSE;
	}

	s_bInited = TRUE;
	{
		char szInit[96];
		wsprintf(szInit, "Init: D3D9 device created (HAL, sw VP, D16, %ux%u)",
			(unsigned)s_pp.BackBufferWidth, (unsigned)s_pp.BackBufferHeight);
		D3D9Log(szInit);
	}

	// RHW fog: the game carries the per-vertex fog factor in specular ALPHA
	// (0xFF = no fog). D3D9 must not apply table/vertex fog math on top.
	s_pDevice->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
	s_pDevice->SetRenderState(D3DRS_FOGTABLEMODE,  D3DFOG_NONE);
	return TRUE;
}

void D3D9Shutdown(void)
{
	if (s_pDevice)
	{
		s_pDevice->Release();
		s_pDevice = NULL;
	}
	if (s_pD3D9)
	{
		s_pD3D9->Release();
		s_pD3D9 = NULL;
	}
	if (s_hD3D9)
	{
		FreeLibrary(s_hD3D9);
		s_hD3D9 = NULL;
	}
	s_bInited = FALSE;
	D3D9Log("Shutdown");
}

BOOL D3D9BeginScene(void)
{
	if (!s_pDevice)
		return FALSE;
	return SUCCEEDED(s_pDevice->BeginScene());
}

void D3D9EndScene(void)
{
	if (s_pDevice)
		s_pDevice->EndScene();
}

void D3D9Clear(DWORD dwColor)
{
	// Z-clear to 0.0 — the game's convention (Direct3D.cpp flip-clear uses Clear2 z=0.0
	// with ZFUNC=GREATEREQUAL, i.e. 0=far, greatest=nearest). A 1.0 clear rejects
	// every poly (0.5>=1.0 fails) → black world. The sky still drew because it
	// disables the Z-buffer.
	if (s_pDevice)
		s_pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, dwColor, 0.0f, 0);
}

BOOL D3D9Present(void)
{
	if (!s_pDevice)
		return FALSE;
	return SUCCEEDED(s_pDevice->Present(NULL, NULL, NULL, NULL));
}

BOOL D3D9CheckDevice(void)
{
	if (!s_pDevice)
		return FALSE;
	HRESULT hr = s_pDevice->TestCooperativeLevel();
	if (hr == D3DERR_DEVICELOST)
		return FALSE;
	if (hr == D3DERR_DEVICENOTRESET)
		return FALSE;   // caller should D3D9Reset()
	return TRUE;
}

BOOL D3D9Reset(void)
{
	if (!s_pDevice)
		return FALSE;
	HRESULT hr = s_pDevice->Reset(&s_pp);
	if (SUCCEEDED(hr))
		D3D9SetViewportFull();
	return SUCCEEDED(hr);
}

// Set the viewport to the full backbuffer (windowed Reset restores this, but the
// 3D path may change it — Present2D must always cover the whole backbuffer).
static void D3D9SetViewportFull(void)
{
	if (!s_pDevice)
		return;
	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = s_pp.BackBufferWidth;
	vp.Height = s_pp.BackBufferHeight;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	s_pDevice->SetViewport(&vp);
}

void* D3D9GetDevice(void)
{
	return s_pDevice;
}

void* D3D9GetD3D(void)
{
	return s_pD3D9;
}

// The current render target (backbuffer surface) — used by the readback. The returned
// pointer is borrowed (do not release); GetRenderTargetData takes it as its first arg.
static void* s_pRenderTarget = NULL;

void* D3D9GetRenderTarget(void)
{
	if (!s_pDevice)
		return NULL;
	if (s_pRenderTarget)
	{
		((IDirect3DSurface9*)s_pRenderTarget)->Release();
		s_pRenderTarget = NULL;
	}
	if (FAILED(s_pDevice->GetRenderTarget(0, (IDirect3DSurface9**)&s_pRenderTarget)))
		return NULL;
	return s_pRenderTarget;
}

//*******************************************************************************************
// Track C: raw D3D9 device calls for the CD3D9Device façade.
//*******************************************************************************************

void D3D9SetRenderState(DWORD dwState, DWORD dwValue)
{
	if (s_pDevice)
		s_pDevice->SetRenderState((D3DRENDERSTATETYPE)dwState, dwValue);
}

void D3D9SetTextureStageState(DWORD dwStage, DWORD dwTss, DWORD dwValue)
{
	if (s_pDevice)
		s_pDevice->SetTextureStageState(dwStage, (D3DTEXTURESTAGESTATETYPE)dwTss, dwValue);
}

void D3D9SetSamplerState(DWORD dwStage, DWORD dwSamp, DWORD dwValue)
{
	if (s_pDevice)
		s_pDevice->SetSamplerState(dwStage, (D3DSAMPLERSTATETYPE)dwSamp, dwValue);
}

void D3D9SetTexture(DWORD dwStage, void* pTex)
{
	if (s_pDevice)
		s_pDevice->SetTexture(dwStage, (IDirect3DTexture9*)pTex);
}

static DWORD s_dwFVF = 0;

void D3D9SetFVF(DWORD dwFvf)
{
	s_dwFVF = dwFvf;
}

BOOL D3D9DrawPrimitiveUP(DWORD dwPrimType, DWORD dwVertexCount, const void* pVerts,
                         DWORD dwStride)
{
	if (!s_pDevice || !pVerts)
		return FALSE;

	// D3D6 DrawPrimitive takes a VERTEX count; D3D9 DrawPrimitiveUP needs a PRIMITIVE
	// count. Passing the vertex count reads past the buffer → garbage vertices that
	// explode across the screen (the stretched-spike tearing seen on the host).
	s_dwPrimsPerSec += dwVertexCount;
	DWORD dwPrimCount = dwVertexCount;
	switch (dwPrimType)
	{
		case D3DPT_TRIANGLELIST: dwPrimCount = dwVertexCount / 3;    break;
		case D3DPT_TRIANGLESTRIP:
		case D3DPT_TRIANGLEFAN:  dwPrimCount = dwVertexCount - 2;   break;
		case D3DPT_LINELIST:     dwPrimCount = dwVertexCount / 2;   break;
		case D3DPT_LINESTRIP:    dwPrimCount = dwVertexCount - 1;   break;
		default:                 dwPrimCount = dwVertexCount;       break;
	}

	if (s_dwFVF)
		s_pDevice->SetFVF(s_dwFVF);
	return SUCCEEDED(s_pDevice->DrawPrimitiveUP((D3DPRIMITIVETYPE)dwPrimType,
		dwPrimCount, pVerts, dwStride));
}

void* D3D9CreateTexture(int iWidth, int iHeight, DWORD dwFormat)
{
	if (!s_pDevice)
		return NULL;
	IDirect3DTexture9* pt = NULL;
	// MANAGED: survives device resets; CPU LockRect is cheap and direct.
	if (FAILED(s_pDevice->CreateTexture(iWidth, iHeight, 1, 0,
		(D3DFORMAT)dwFormat, D3DPOOL_MANAGED, &pt, NULL)))
		return NULL;
	return pt;
}

BOOL D3D9LockTexture(void* pTex, void** ppBits, int* piPitch)
{
	if (!pTex)
		return FALSE;
	D3DLOCKED_RECT lr;
	if (FAILED(((IDirect3DTexture9*)pTex)->LockRect(0, &lr, NULL, 0)))
		return FALSE;
	*ppBits  = lr.pBits;
	*piPitch = (int)lr.Pitch;
	return TRUE;
}

void D3D9UnlockTexture(void* pTex)
{
	if (pTex)
		((IDirect3DTexture9*)pTex)->UnlockRect(0);
}

void D3D9ReleaseTexture(void* pTex)
{
	if (pTex)
		((IDirect3DTexture9*)pTex)->Release();
}

// Readback staging surface (sysmem offscreen-plain, same size as the backbuffer).
static IDirect3DSurface9* s_pReadback = NULL;
static int s_iReadbackW = 0;
static int s_iReadbackH = 0;

// X8R8G8B8 -> RGB565 (inverse of Convert565To8888).
static void Convert8888To565(const void* pSrc, void* pDst, int iWidth, int iHeight,
                             int iSrcPitch, int iDstPitch)
{
	const char* ps = (const char*)pSrc;
	char*       pd = (char*)pDst;
	for (int y = 0; y < iHeight; y++)
	{
		const DWORD* psRow = (const DWORD*)(ps + y * iSrcPitch);
		WORD*        pdRow = (WORD*)(pd + y * iDstPitch);
		for (int x = 0; x < iWidth; x++)
		{
			DWORD px = psRow[x];
			int   r  = (px >> 16) & 0xFF;
			int   g  = (px >> 8)  & 0xFF;
			int   b  = px & 0xFF;
			pdRow[x] = (WORD)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
		}
	}
}

BOOL D3D9ReadbackToDIB(void* pDibBits, int iWidth, int iHeight, int iDibPitch)
{
	if (!s_pDevice || !pDibBits)
		return FALSE;

	// Track C diagnostics: measure the readback cost.
	LARGE_INTEGER liFreq, liStart, liEnd;
	QueryPerformanceFrequency(&liFreq);
	QueryPerformanceCounter(&liStart);
	s_dwReadbacksPerFrame++;

	// Lazy-create the staging surface at the backbuffer size.
	IDirect3DSurface9* pb = NULL;
	if (FAILED(s_pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pb)))
		return FALSE;
	D3DSURFACE_DESC d;
	pb->GetDesc(&d);
	pb->Release();

	if (!s_pReadback || s_iReadbackW != (int)d.Width || s_iReadbackH != (int)d.Height)
	{
		if (s_pReadback)
		{
			s_pReadback->Release();
			s_pReadback = NULL;
		}
		s_pDevice->CreateOffscreenPlainSurface(d.Width, d.Height, D3DFMT_X8R8G8B8,
			D3DPOOL_SYSTEMMEM, &s_pReadback, NULL);
		s_iReadbackW = (int)d.Width;
		s_iReadbackH = (int)d.Height;
	}
	if (!s_pReadback)
		return FALSE;

	if (FAILED(s_pDevice->GetRenderTargetData(
			(IDirect3DSurface9*)D3D9GetRenderTarget(), s_pReadback)))
		return FALSE;

	D3DLOCKED_RECT lr;
	if (FAILED(s_pReadback->LockRect(&lr, NULL, D3DLOCK_READONLY)))
		return FALSE;
	Convert8888To565(lr.pBits, pDibBits, iWidth, iHeight, (int)lr.Pitch, iDibPitch);
	s_pReadback->UnlockRect();

	QueryPerformanceCounter(&liEnd);
	s_dwReadbacksPerSec++;
	s_dwReadbackMsPerSec += DWORD((liEnd.QuadPart - liStart.QuadPart) * 1000 / liFreq.QuadPart);
	return TRUE;
}

// Per-frame clear flag: the façade clears target+Z on the first hardware BeginScene
// of each frame (D3D6 clears after each flip); reset by D3D9FramePresented.
static BOOL s_bClearedThisFrame = FALSE;

// Track C diagnostics: per-frame readback count + total cost, logged once per second.
static DWORD   s_dwReadbacksPerSec = 0;
static DWORD   s_dwReadbackMsPerSec = 0;
static DWORD   s_dwFramesPerSec = 0;
static DWORD   s_dwReadbacksPerFrame = 0;
static DWORD   s_dwReadbackMsPerFrame = 0;
static DWORD   s_dwSeconds = 0;
static DWORD   s_dwPrimsPerSec = 0;

void D3D9FramePresented(void)
{
	if (s_pDevice)
	{
		s_dwFramesPerSec++;
		if (++s_dwSeconds >= 60)
		{
			s_dwSeconds = 0;
			dprintf("D3D9: perf frames=%u prims=%u readbacks=%u (%.2f/frame) readbackMs=%u (%.2f/frame)\n",
				s_dwFramesPerSec, s_dwPrimsPerSec, s_dwReadbacksPerSec,
				s_dwFramesPerSec ? double(s_dwReadbacksPerSec) / s_dwFramesPerSec : 0.0,
				s_dwReadbackMsPerSec,
				s_dwFramesPerSec ? double(s_dwReadbackMsPerSec) / s_dwFramesPerSec : 0.0);
			s_dwFramesPerSec = 0;
			s_dwReadbacksPerSec = 0;
			s_dwReadbackMsPerSec = 0;
			s_dwPrimsPerSec = 0;
		}
		s_dwReadbacksPerFrame = 0;
	}
	s_bClearedThisFrame = FALSE;
}

BOOL D3D9ClearTargetZ(DWORD dwColor)
{
	if (!s_pDevice)
		return FALSE;
	if (s_bClearedThisFrame)
		return TRUE;   // already cleared this frame
	s_bClearedThisFrame = TRUE;
	return SUCCEEDED(s_pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
		dwColor, 0.0f, 0));
}

//*******************************************************************************************
// 2D present (menu/overlay) implementation.
//*******************************************************************************************

// 16bpp RGB565 -> X8R8G8B8.
static void Convert565To8888(const void* pSrc, void* pDst, int iWidth, int iHeight,
                             int iSrcPitch, int iDstPitch)
{
	const char* ps = (const char*)pSrc;
	char*       pd = (char*)pDst;
	for (int y = 0; y < iHeight; y++)
	{
		const WORD* psRow = (const WORD*)(ps + y * iSrcPitch);
		DWORD*      pdRow = (DWORD*)(pd + y * iDstPitch);
		for (int x = 0; x < iWidth; x++)
		{
			WORD px = psRow[x];
			int  r  = (px >> 11) & 0x1F;
			int  g  = (px >> 5)  & 0x3F;
			int  b  = px & 0x1F;
			pdRow[x] = 0xFF000000
				| ((r << 3) | (r >> 2)) << 16
				| ((g << 2) | (g >> 4)) << 8
				| ((b << 3) | (b >> 2));
		}
	}
}

// Ensure the device is usable; attempt a reset when D3DERR_DEVICENOTRESET. Returns
// TRUE when the device can be used for this frame.
static BOOL D3D9CheckDeviceReset(void)
{
	if (!s_pDevice)
		return FALSE;
	HRESULT hr = s_pDevice->TestCooperativeLevel();
	if (hr == D3DERR_DEVICENOTRESET)
	{
		// Pool-default resources died with the device; managed ones survived.
		if (s_pTex2D)
		{
			s_pTex2D->Release();
			s_pTex2D = NULL;
		}
		hr = s_pDevice->Reset(&s_pp);
		if (SUCCEEDED(hr))
			D3D9SetViewportFull();
	}
	return hr == D3D_OK;
}

void D3D9SetHardwareFrame(BOOL bHardwareFrame)
{
	s_bHardwareFrame = bHardwareFrame;
}

// Throttled diagnostic: log a message at most every ~5 seconds (the present runs
// every frame — a failing present would otherwise spam the log).
static void D3D9Diag(const char* psz)
{
	static DWORD s_dwLastLog = 0;
	DWORD dwNow = GetTickCount();
	if (dwNow - s_dwLastLog < 5000)
		return;
	s_dwLastLog = dwNow;
	D3D9Log(psz);
}

void D3D9Present2D(const void* pSrc, int iWidth, int iHeight, int iSrcPitch)
{
	if (!s_pDevice || !pSrc)
	{
		D3D9Diag("Present2D: no device/src");
		return;
	}
	if (!D3D9CheckDeviceReset())
	{
		D3D9Diag("Present2D: device check failed");
		return;
	}

	// Lazy-create the upload texture (pow2, managed so it survives device reset).
	if (!s_pTex2D || s_iTex2DWidth != iWidth || s_iTex2DHeight != iHeight)
	{
		if (s_pTex2D)
		{
			s_pTex2D->Release();
			s_pTex2D = NULL;
		}
		s_pDevice->CreateTexture(D3D9_TEX_W, D3D9_TEX_H, 1, 0,
			D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &s_pTex2D, NULL);
		s_iTex2DWidth  = iWidth;
		s_iTex2DHeight = iHeight;
	}
	if (!s_pTex2D)
	{
		D3D9Diag("Present2D: no texture (CreateTexture failed)");
		return;
	}

	// One-time + throttled diagnostic: the average brightness of the content we
	// upload (separates a broken present from black content).
	{
		static DWORD s_dwLastBright = 0;
		DWORD dwNow = GetTickCount();
		if (dwNow - s_dwLastBright > 5000)
		{
			s_dwLastBright = dwNow;
			const char* ps = (const char*)pSrc;
			int iH = iHeight > 32 ? 32 : iHeight;
			int iW = iWidth > 32 ? 32 : iWidth;
			DWORD dwSum = 0;
			for (int yy = 0; yy < iH; yy++)
			{
				const WORD* pRow = (const WORD*)(ps + yy * iSrcPitch);
				for (int xx = 0; xx < iW; xx++)
				{
					WORD px = pRow[xx];
					int  r  = (px >> 11) & 0x1F;
					int  g  = (px >> 5)  & 0x3F;
					int  b  = px & 0x1F;
					dwSum += (r * 299 + g * 587 + b * 114) / 1000;
				}
			}
			char szBr[80];
			wsprintf(szBr, "Present2D: content brightness avg=%lu (0-31)",
				dwSum / (DWORD)(iH * iW));
			D3D9Log(szBr);
		}
	}

	// Upload: convert 565 -> X8R8G8B8 into the locked texture.
	D3DLOCKED_RECT lr;
	if (FAILED(s_pTex2D->LockRect(0, &lr, NULL, 0)))
	{
		D3D9Diag("Present2D: LockRect failed");
		return;
	}
	Convert565To8888(pSrc, lr.pBits, iWidth, iHeight, iSrcPitch, (int)lr.Pitch);
	s_pTex2D->UnlockRect(0);

	// Size the quad to the ACTUAL backbuffer (the window may have been resized or
	// the device Reset at a different size) — never clip to a stale 640x480.
	D3DSURFACE_DESC d;
	d.Width  = s_pp.BackBufferWidth;
	d.Height = s_pp.BackBufferHeight;
	IDirect3DSurface9* pb = NULL;
	if (SUCCEEDED(s_pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pb)))
	{
		pb->GetDesc(&d);
		pb->Release();
	}
	const float fW = (float)d.Width;
	const float fH = (float)d.Height;

	// One-time diagnostic: what the backbuffer reports (windowed dgVoodoo2 may
	// report the fullscreen size or a stale size).
	{
		static BOOL bLoggedBB = FALSE;
		if (!bLoggedBB)
		{
			bLoggedBB = TRUE;
			char szBB[96];
			wsprintf(szBB, "Present2D backbuffer: %ux%u (raster %dx%d)",
				(unsigned)d.Width, (unsigned)d.Height, iWidth, iHeight);
			D3D9Log(szBB);
		}
	}

	// Fullscreen textured quad (backbuffer coords).
	const float fU1 = (float)iWidth  / D3D9_TEX_W;
	const float fV1 = (float)iHeight / D3D9_TEX_H;
	struct QuadVert { float x, y, z, rhw; float u, v; };
	QuadVert aVerts[6] =
	{
		{ 0.0f,  0.0f, 0.5f, 1.0f, 0.0f, 0.0f },
		{ fW,    0.0f, 0.5f, 1.0f, fU1,  0.0f },
		{ fW,    fH,   0.5f, 1.0f, fU1,  fV1 },
		{ 0.0f,  0.0f, 0.5f, 1.0f, 0.0f, 0.0f },
		{ fW,    fH,   0.5f, 1.0f, fU1,  fV1 },
		{ 0.0f,  fH,   0.5f, 1.0f, 0.0f, fV1 },
	};

	s_pDevice->BeginScene();
	D3D9SetViewportFull();
	s_pDevice->SetRenderState(D3DRS_ZENABLE,       FALSE);
	s_pDevice->SetRenderState(D3DRS_CULLMODE,      D3DCULL_NONE);
	s_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,
		s_bHardwareFrame ? TRUE : FALSE);
	s_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
	s_pDevice->SetRenderState(D3DRS_DESTBLEND,
		s_bHardwareFrame ? D3DBLEND_ONE : D3DBLEND_ZERO);
	s_pDevice->SetTexture(0, s_pTex2D);
	s_pDevice->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
	s_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	s_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE);
	s_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
	s_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	s_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
	s_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, aVerts, sizeof(QuadVert));
	s_pDevice->EndScene();
	{
		HRESULT hrPresent = s_pDevice->Present(NULL, NULL, NULL, NULL);
		if (FAILED(hrPresent))
		{
			char szP[96];
			wsprintf(szP, "Present failed (hr=0x%08x)", (unsigned)hrPresent);
			D3D9Log(szP);
		}
	}

	// A frame was presented: reset the per-frame clear flag so the next hardware
	// BeginScene clears target+Z again (Track C, the D3D6 flip-clear equivalent).
	s_bClearedThisFrame = FALSE;

	// The hardware-frame flag is consumed by this present.
	s_bHardwareFrame = FALSE;
}

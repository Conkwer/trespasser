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

static HMODULE             s_hD3D9   = NULL;
static IDirect3D9*         s_pD3D9   = NULL;
static IDirect3DDevice9*   s_pDevice = NULL;
static D3DPRESENT_PARAMETERS s_pp;
static BOOL                s_bInited = FALSE;

// Log to the game's DebugLog.txt (the game pins the CWD to its own folder on startup).
static void D3D9Log(const char* psz)
{
	FILE* f = fopen("DebugLog.txt", "a");
	if (f)
	{
		fprintf(f, "D3D9: %s\n", psz);
		fclose(f);
	}
}

BOOL D3D9Init(HWND hwnd, int iWidth, int iHeight, BOOL bFullScreen)
{
	typedef IDirect3D9* (WINAPI *pfnDirect3DCreate9)(UINT);

	if (s_bInited)
		return TRUE;

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
		D3D9Log("Init: CreateDevice failed");
		s_pD3D9->Release();
		s_pD3D9 = NULL;
		FreeLibrary(s_hD3D9);
		s_hD3D9 = NULL;
		return FALSE;
	}

	s_bInited = TRUE;
	D3D9Log("Init: D3D9 device created (HAL, sw VP, D16 depth)");
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
	if (s_pDevice)
		s_pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, dwColor, 1.0f, 0);
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
	return SUCCEEDED(hr);
}

void* D3D9GetDevice(void)
{
	return s_pDevice;
}

void* D3D9GetD3D(void)
{
	return s_pD3D9;
}

/***********************************************************************************************
 *
 * Trespasser-Plus D3D9 driver — C-style interface.
 *
 * This header deliberately exposes NO D3D9 types (only HWND/BOOL/DWORD/void*) so that any
 * game-side file can include it without pulling d3d9.h into a TU that already includes the
 * game's DX6 d3d.h (the two headers define colliding enum names).
 *
 * Implementation: Lib/W95/Direct3D9.cpp (a PCH-isolated TU whose first include is the
 * vendored DirectX/d3d9.h).
 *
 ***********************************************************************************************/

#ifndef D3D9API_H
#define D3D9API_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// Create the D3D9 device (dynamic-loads d3d9.dll). Returns TRUE on success.
// Width/Height are used in fullscreen mode; in windowed mode the client rect is used.
BOOL  D3D9Init(HWND hwnd, int iWidth, int iHeight, BOOL bFullScreen);

// Release the device and unload d3d9.dll.
void  D3D9Shutdown(void);

// Frame control. D3D9Clear clears target + depth; call between Begin/EndScene.
BOOL  D3D9BeginScene(void);
void  D3D9EndScene(void);
void  D3D9Clear(DWORD dwColor);
BOOL  D3D9Present(void);

// 2D/overlay present: uploads a 16bpp RGB565 raster as a fullscreen textured quad and
// presents. When the 3D renderer called D3D9SetHardwareFrame(TRUE) this frame, the upload
// is an ADDITIVE overlay (captions/HUD on top of the 3D image); otherwise it is an opaque
// full-frame present (menus, loading, movies, pause). The hardware-frame flag is consumed
// (reset) by this call.
void  D3D9SetHardwareFrame(BOOL bHardwareFrame);
void  D3D9Present2D(const void* pSrc, int iWidth, int iHeight, int iSrcPitch);

// Device-lost handling for real d3d9 (alt-tab/UAC). D3D9CheckDevice returns TRUE when
// the device is usable; FALSE when lost. D3D9Reset recreates the device (call when
// TestCooperativeLevel reports D3DERR_DEVICENOTRESET).
BOOL  D3D9CheckDevice(void);
BOOL  D3D9Reset(void);

// Raw interface access for the renderer TU (ScreenRenderAuxD3D9.cpp casts these).
void* D3D9GetDevice(void);
void* D3D9GetD3D(void);

#ifdef __cplusplus
}
#endif

#endif // D3D9API_H

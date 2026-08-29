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
BOOL  D3D9IsHardwareFrame(void);
void  D3D9Present2D(const void* pSrc, int iWidth, int iHeight, int iSrcPitch);

// Device-lost handling for real d3d9 (alt-tab/UAC). D3D9CheckDevice returns TRUE when
// the device is usable; FALSE when lost. D3D9Reset recreates the device (call when
// TestCooperativeLevel reports D3DERR_DEVICENOTRESET).
BOOL  D3D9CheckDevice(void);
BOOL  D3D9Reset(void);

// Raw interface access for the renderer TU (ScreenRenderAuxD3D9.cpp casts these).
void* D3D9GetDevice(void);
void* D3D9GetD3D(void);

// Borrowed pointer to the current render target surface (used by the readback).
void* D3D9GetRenderTarget(void);

//-------------------------------------------------------------------------------------------
// Track C: raw D3D9 device calls for the CD3D9Device façade (Lib/W95/D3D9Device.cpp).
// All state values are D3D9's own (the façade translates the D3D6 enums before calling).
//-------------------------------------------------------------------------------------------

// Render state (D3DRS_* value, D3D9 numeric value).
void  D3D9SetRenderState(DWORD dwState, DWORD dwValue);

// Texture stage state (D3DTSS_* value — D3D9 kept the D3D6 numbers for the stages it has).
void  D3D9SetTextureStageState(DWORD dwStage, DWORD dwTss, DWORD dwValue);

// Sampler state (D3DSAMP_* value).
void  D3D9SetSamplerState(DWORD dwStage, DWORD dwSamp, DWORD dwValue);

// Texture binding. pTex is an IDirect3DTexture9* (the D3D6-side callers pass their
// pd3dtexGet() pointer, which is a void* in both modes; the façade reinterprets it).
void  D3D9SetTexture(DWORD dwStage, void* pTex);

// Vertex format cache + DrawPrimitiveUP (D3D9 primitive type = D3D6 type value).
void  D3D9SetFVF(DWORD dwFvf);
BOOL  D3D9DrawPrimitiveUP(DWORD dwPrimType, DWORD dwVertexCount, const void* pVerts,
                          DWORD dwStride);

// State snapshot (used by D3D9Present2D to save/restore the states its 2D quad
// clobbers — see R0, the unifying state-leak fix). D3D9GetTexture returns the
// borrowed stage-0 texture pointer.
DWORD  D3D9GetRenderState(DWORD dwState);
DWORD  D3D9GetTextureStageState(DWORD dwStage, DWORD dwTss);
DWORD  D3D9GetSamplerState(DWORD dwStage, DWORD dwSamp);
void*  D3D9GetTexture(DWORD dwStage);
DWORD  D3D9GetFVF(void);

// Hardware-drew latch (R2 readback suppression): set TRUE by every DrawPrimitiveUP,
// peeked by D3D9RasterReadback (early-out unless TRUE) and cleared on a successful
// readback. Dedups all hw->sw transitions in a frame to exactly 1 readback.
BOOL   D3D9HwDrawnPeek(void);
void   D3D9HwDrawnClear(void);

// Texture creation/locking for the CRasterD3D texture twin. fmt is a D3DFMT_* value.
void* D3D9CreateTexture(int iWidth, int iHeight, DWORD dwFormat);
BOOL  D3D9LockTexture(void* pTex, void** ppBits, int* piPitch);
void  D3D9UnlockTexture(void* pTex);
void  D3D9ReleaseTexture(void* pTex);

// Backbuffer readback: copies the current backbuffer into a 16bpp 565 destination
// (the DIB raster). Used at hardware->software transitions in the mixed frame.
BOOL  D3D9ReadbackToDIB(void* pDibBits, int iWidth, int iHeight, int iDibPitch);

// Tell the driver a frame was presented (resets the per-frame clear flag and the
// scene machine so the next hardware BeginScene clears target+Z again).
void  D3D9FramePresented(void);

// Clear target+Z once per frame (the D3D6 flip-clear equivalent). The flag is reset
// by D3D9FramePresented. Returns TRUE when the clear ran (or was already done).
BOOL  D3D9ClearTargetZ(DWORD dwColor);

#ifdef __cplusplus
}
#endif

#endif // D3D9API_H

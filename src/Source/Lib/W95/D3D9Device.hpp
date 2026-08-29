/***********************************************************************************************
 *
 * Trespasser-Plus CD3D9Device — the D3D6-signature device façade (Track C, work item A).
 *
 * The game's hardware pipeline calls the D3D6 device exclusively through
 * d3dDriver.pGetDevice() with a small method surface:
 *
 *     BeginScene / EndScene / SetRenderState / SetTextureStageState /
 *     SetTexture / DrawPrimitive
 *
 * This class presents that exact surface with D3D6 signatures, forwarding 1:1 to the
 * real IDirect3DDevice3 in dx6 mode, and translating to the D3D9 device (via the
 * Direct3D9API.h C interface) in dx9 mode. Drawers, batching, fog and vertex data are
 * untouched — D3DTLVERTEX is byte-identical to D3D9's XYZRHW|DIFFUSE|SPECULAR|TEX1.
 *
 * Render-state translation is a numeric pass-through: D3D9 preserved the D3D6 enum
 * values for every state the game uses (verified against d3dtypes.h/d3d9types.h).
 * D3D6-only states (color-key, subpixel, texturemapblend, ...) are no-ops. The texture
 * stage ADDRESS/filter states move to sampler states in D3D9.
 *
 * Fog: D3D9 RHW fog reads the per-vertex factor from specular ALPHA; the driver sets
 * FOGVERTEXMODE=FOGTABLEMODE=NONE at device init (see D3D9Init).
 *
 ***********************************************************************************************/

#ifndef D3D9DEVICE_H
#define D3D9DEVICE_H

#include "Direct3D9API.h"

// D3D6 render-state values with no D3D9 equivalent (from the game's d3dtypes.h) — no-op.
#define D3D9DEV_NOOP_STATE(s) \
	((s) == 1  /* TEXTUREHANDLE   */ || (s) == 2  /* ANTIALIAS        */ || \
	 (s) == 4  /* TEXTUREPERSPECT */ || (s) == 11 /* MONOENABLE       */ || \
	 (s) == 17 /* TEXTUREMAG      */ || (s) == 18 /* TEXTUREMIN       */ || \
	 (s) == 21 /* TEXTUREMAPBLEND */ || (s) == 31 /* SUBPIXEL         */ || \
	 (s) == 32 /* SUBPIXELX       */ || (s) == 36 /* FOGTABLESTART    */ || \
	 (s) == 37 /* FOGTABLEEND     */ || (s) == 41 /* COLORKEYENABLE   */ || \
	 (s) == 47 /* ZBIAS           */)

class CD3D9Device
{
private:

	// The real D3D6 device (dx6 mode). NULL in dx9 mode.
	LPDIRECT3DDEVICE3 pDevice6;

	// dx9 scene state: D3D9 requires Begin/EndScene pairing — a guarded 1:1 map keeps
	// the D3D6 pattern legal (nested/sky BeginScene collapses to the open scene).
	bool bInScene;

public:

	CD3D9Device() : pDevice6(0), bInScene(false) {}

	// dx6 mode: bind the real D3D6 device (called by bInitializeD3D / ReleaseD3D).
	void BindD3D6(LPDIRECT3DDEVICE3 pDevice)
	{
		pDevice6 = pDevice;
		if (!pDevice)
			bInScene = false;
	}

	LPDIRECT3DDEVICE3 Device6() const
	{
		return pDevice6;
	}

	// dx6: device present? dx9: the D3D9 device exists (checked lazily at BeginScene).
	bool bIsValid() const
	{
		return pDevice6 != 0;
	}

	//*****************************************************************************************
	HRESULT BeginScene()
	{
		if (pDevice6)
			return pDevice6->BeginScene();

		// dx9: guard against nested BeginScene (the sky opens its own raw scene).
		if (bInScene)
			return DD_OK;

		bInScene = D3D9BeginScene() ? true : false;
		if (!bInScene)
			return DDERR_GENERIC;

		// Clear target+Z once per frame (the D3D6 flip-clear equivalent) — inside
		// the open scene, which is guaranteed legal in D3D9.
		D3D9ClearTargetZ(0x00000000);
		return DD_OK;
	}

	//*****************************************************************************************
	HRESULT EndScene()
	{
		if (pDevice6)
			return pDevice6->EndScene();

		if (bInScene)
		{
			D3D9EndScene();
			bInScene = false;
		}
		return DD_OK;
	}

	//*****************************************************************************************
	HRESULT SetRenderState(D3DRENDERSTATETYPE d3dRenderStateType, DWORD dwValue)
	{
		if (pDevice6)
			return pDevice6->SetRenderState(d3dRenderStateType, dwValue);

		const DWORD s = (DWORD)d3dRenderStateType;
		if (D3D9DEV_NOOP_STATE(s))
			return DD_OK;
		D3D9SetRenderState(s, dwValue);
		return DD_OK;
	}

	//*****************************************************************************************
	HRESULT SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexState,
	                            DWORD dwValue)
	{
		if (pDevice6)
			return pDevice6->SetTextureStageState(dwStage, d3dTexState, dwValue);

		switch ((DWORD)d3dTexState)
		{
			case 12:  // D3DTSS_ADDRESS  -> both sampler address modes
				D3D9SetSamplerState(dwStage, 1, dwValue);
				D3D9SetSamplerState(dwStage, 2, dwValue);
				break;
			case 13:  // D3DTSS_ADDRESSU -> D3DSAMP_ADDRESSU
				D3D9SetSamplerState(dwStage, 1, dwValue);
				break;
			case 14:  // D3DTSS_ADDRESSV -> D3DSAMP_ADDRESSV
				D3D9SetSamplerState(dwStage, 2, dwValue);
				break;
			case 16:  // D3DTSS_MAGFILTER -> D3DSAMP_MAGFILTER
				D3D9SetSamplerState(dwStage, 5, dwValue);
				break;
			case 17:  // D3DTSS_MINFILTER -> D3DSAMP_MINFILTER
				D3D9SetSamplerState(dwStage, 6, dwValue);
				break;
			default:  // COLOROP/COLORARG*/ALPHAOP/... — D3D9 kept the D3D6 numbers.
				D3D9SetTextureStageState(dwStage, (DWORD)d3dTexState, dwValue);
				break;
		}
		return DD_OK;
	}

	//*****************************************************************************************
	HRESULT SetTexture(DWORD dwStage, LPDIRECT3DTEXTURE2 pd3dTexture)
	{
		if (pDevice6)
			return pDevice6->SetTexture(dwStage, pd3dTexture);

		// dx9: the pointer is really an IDirect3DTexture9* (the CRasterD3D texture twin
		// returns it through the same void*/LPDIRECT3DTEXTURE2 slot).
		D3D9SetTexture(dwStage, (void*)pd3dTexture);
		return DD_OK;
	}

	//*****************************************************************************************
	HRESULT DrawPrimitive(D3DPRIMITIVETYPE d3dPrimitiveType, DWORD dwVertexTypeDesc,
	                      LPVOID lpvVertices, DWORD dwVertexCount, DWORD dwFlags)
	{
		if (pDevice6)
			return pDevice6->DrawPrimitive(d3dPrimitiveType, dwVertexTypeDesc,
				lpvVertices, dwVertexCount, dwFlags);

		// D3DTLVERTEX is 32 bytes; the FVF value is identical in D3D9.
		D3D9SetFVF(dwVertexTypeDesc);
		D3D9DrawPrimitiveUP((DWORD)d3dPrimitiveType, dwVertexCount, lpvVertices, 32);
		return DD_OK;
	}
};

// Global façade instance (Direct3D.hpp's pGetDevice() returns this).
inline CD3D9Device& D3D9RenderDevice()
{
	static CD3D9Device s_device;
	return s_device;
}

#endif // D3D9DEVICE_H

#include <windows.h>
#include <fstream>
#include <ostream>
#include <iostream>
#include <list>
#include "main.h"
#include "d3d9.h"
#include "d3dutil.h"
#include "RenderstateManager.h"
#include "WindowManager.h"
#include "Settings.h"
#include "KeyActions.h"

using namespace std;

hkIDirect3DDevice9::hkIDirect3DDevice9(IDirect3DDevice9 **ppReturnedDeviceInterface, D3DPRESENT_PARAMETERS *pPresentParam, IDirect3D9 *pIDirect3D9) {
	m_pD3Ddev = *ppReturnedDeviceInterface;
	*ppReturnedDeviceInterface = this;
	m_PresentParam = *pPresentParam;
	m_pD3Dint = pIDirect3D9;
	RSManager::get().setD3DDevice(m_pD3Ddev);
	RSManager::get().initResources();
}

HRESULT APIENTRY hkIDirect3DDevice9::Present(CONST RECT *pSourceRect, CONST RECT *pDestRect, HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion) {
	SDLOG(3, "!!!!!!!!!!!!!!!!!!!!!!! Present !!!!!!!!!!!!!!!!!!\n");
	KeyActions::get().processIO();
	//WindowManager::get().applyCursorCapture();
	if(Settings::get().getBorderlessFullscreen()) WindowManager::get().maintainBorderlessFullscreen();
	if(Settings::get().getForceWindowed()) WindowManager::get().maintainWindowSize();
	return RSManager::get().redirectPresent(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetVertexShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) {
	SDLOG(4, "SetVertexShaderConstantF: start: %u, count: %u\n", StartRegister, Vector4fCount);
	if(Settings::get().getLogLevel() > 13) {
		for(size_t i=0; i<Vector4fCount; ++i) {
			SDLOG(0, " - %16.10f %16.10f %16.10f %16.10f\n", pConstantData[i*4+0], pConstantData[i*4+1], pConstantData[i*4+2], pConstantData[i*4+3]);
		}
	}
	//if(StartRegister == 8 && Vector4fCount == 8) {
	//	SDLOG(0, "!!8ball\n");
	//	memcpy(replacement, pConstantData, sizeof(float)*4*Vector4fCount);
	//	D3DXMATRIX projMatrix, viewMatrix, cameraMatrix, viewInv;
	//	memcpy(&projMatrix, &(replacement[0]), sizeof(float)*16);
	//	memcpy(&viewMatrix, &(replacement[16]), sizeof(float)*16);
	//	if(viewMatrix._14 != 0.0 && viewMatrix._24 != 0.0 && viewMatrix._34 != 0.0 && viewMatrix._44 != 0.0) {
	//		SDLOG(0, "!!8ball zero\n");
	//		D3DXMatrixInverse(&viewInv, NULL, &viewMatrix);
	//		projMatrix *= viewInv;
	//		viewMatrix._11 *= 2.0;
	//		viewMatrix._22 *= 2.0;
	//		projMatrix *= viewMatrix;
	//		memcpy(&(replacement[0]), &projMatrix, sizeof(float)*16);
	//		memcpy(&(replacement[16]), &viewMatrix, sizeof(float)*16);
	//		memset(replacement, 0, sizeof(float)*32);
	//		for(size_t i=0; i<Vector4fCount; ++i) {
	//			SDLOG(0, " + %16.10f %16.10f %16.10f %16.10f\n", replacement[i*4+0], replacement[i*4+1], replacement[i*4+2], replacement[i*4+3]);
	//		}
	//		return m_pD3Ddev->SetVertexShaderConstantF(StartRegister, replacement, Vector4fCount);
	//	}
	//} /*else if(StartRegister == 8 && Vector4fCount == 4) {
	//	SDLOG(0, "!!4ball\n");
	//	return m_pD3Ddev->SetVertexShaderConstantF(StartRegister, replacement, Vector4fCount);
	//}*/ 
	return RSManager::get().redirectSetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) {
	//if(RenderTargetIndex != 0) return D3D_OK; // rendertargets > 0 are not actually used by the game - this makes the log shorter 
	SDLOG(3, "SetRenderTarget %5d, %p\n", RenderTargetIndex, pRenderTarget);
	return RSManager::get().redirectSetRenderTarget(RenderTargetIndex, pRenderTarget);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetVertexShader(IDirect3DVertexShader9* pvShader) {
	SDLOG(3, "SetVertexShader: %p\n", pvShader);	
	return RSManager::get().redirectSetVertexShader(pvShader);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetViewport(CONST D3DVIEWPORT9 *pViewport) {
	Settings::get().init();
	SDLOG(6, "SetViewport X / Y - W x H : %4lu / %4lu  -  %4lu x %4lu ; Z: %f - %f\n", pViewport->X, pViewport->Y, pViewport->Width, pViewport->Height, pViewport->MinZ, pViewport->MaxZ);
	
	D3DVIEWPORT9 vp;
	memcpy(&vp, pViewport, sizeof(D3DVIEWPORT9));

	// fix the various maps
	if(    (pViewport->X == 76 && pViewport->Y == 368 && pViewport->Width == 232 && pViewport->Height == 200) // minimap
		|| (pViewport->X == 252 && pViewport->Y == 60 && pViewport->Width == 776 && pViewport->Height == 520) // main menu map
		|| (pViewport->X == 64 && pViewport->Y == 164 && pViewport->Width == 512 && pViewport->Height == 512) // large overlay map
	  ) { 
		vp.X = pViewport->X * Settings::get().getPresentWidth() / 1280;
		vp.Y = pViewport->Y * Settings::get().getPresentHeight() / 720;
		vp.Width = pViewport->Width * Settings::get().getPresentWidth() / 1280;
		vp.Height = pViewport->Height * Settings::get().getPresentHeight() / 720;
		SDLOG(3, "MAP VIEWPORT OVERRIDE to %4lu / %4lu  -  %4lu x %4lu\n", vp.X, vp.Y, vp.Width, vp.Height);
		
	}

	// fix shadowmaps
	else if((pViewport->Width == pViewport->Height) && pViewport->X == 0 && pViewport->Y == 0) {
		vp.Width = pViewport->Width * Settings::get().getShadowMapScale();
		vp.Height = pViewport->Height * Settings::get().getShadowMapScale();
		SDLOG(3, "SHADOWMAP VIEWPORT OVERRIDE to %4lu / %4lu  -  %4lu x %4lu\n", vp.X, vp.Y, vp.Width, vp.Height);
	}
	
	// fix reflections
	else if((pViewport->Width == 640 && pViewport->Height == 360) || (pViewport->Width == 320 && pViewport->Height == 180)) {
		vp.Width = pViewport->Width * Settings::get().getReflectionScale();
		vp.Height = pViewport->Height * Settings::get().getReflectionScale();
		SDLOG(3, "REFLECTION VIEWPORT OVERRIDE to %4lu / %4lu  -  %4lu x %4lu\n", vp.X, vp.Y, vp.Width, vp.Height);
	}

	// fix DoF
	else if(Settings::get().getImproveDOF() && pViewport->Width == 448 && pViewport->Height == 252) {
		vp.Width = (int)(Settings::get().getRenderWidth()*0.35f);
		vp.Height = (int)(Settings::get().getRenderHeight()*0.35f);
		SDLOG(3, "DOF VIEWPORT OVERRIDE to %4lu / %4lu  -  %4lu x %4lu\n", vp.X, vp.Y, vp.Width, vp.Height);
	}

	// fix main
	else if(pViewport->Width == 1280 && pViewport->Height == 720) {
		IDirect3DSurface9 *rendertarget, *bb0, *bb1;
		m_pD3Ddev->GetRenderTarget(0,&rendertarget);
		m_pD3Ddev->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&bb0);
		m_pD3Ddev->GetBackBuffer(0,1,D3DBACKBUFFER_TYPE_MONO,&bb1);
		if(rendertarget==bb0 || rendertarget==bb1) {
			vp.Width = Settings::get().getPresentWidth();
			vp.Height = Settings::get().getPresentHeight();
		} else {
			vp.Width = Settings::get().getRenderWidth();
			vp.Height = Settings::get().getRenderHeight();
		}
		SAFERELEASE(rendertarget);
		SAFERELEASE(bb0);
		SAFERELEASE(bb1);
		SDLOG(3, "MAIN VIEWPORT OVERRIDE to %dx%d\n", vp.Width, vp.Height);
	}

	return RSManager::get().redirectSetViewport(&vp); 
}

HRESULT APIENTRY hkIDirect3DDevice9::DrawIndexedPrimitive(D3DPRIMITIVETYPE Type, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) {
	SDLOG(9, "DrawIndexedPrimitive\n");
	return RSManager::get().redirectDrawIndexedPrimitive(Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
}

HRESULT APIENTRY hkIDirect3DDevice9::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinIndex, UINT NumVertices, UINT PrimitiveCount, CONST void *pIndexData, D3DFORMAT IndexDataFormat, CONST void *pVertexStreamZeroData, UINT VertexStreamZeroStride) {
	SDLOG(9, "DrawIndexedPrimitiveUP(%d, %u, %u, %u, %u, %p, %d, %p, %d)\n", PrimitiveType, MinIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
	return RSManager::get().redirectDrawIndexedPrimitiveUP(PrimitiveType, MinIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT APIENTRY hkIDirect3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) {
	SDLOG(9, "DrawPrimitive\n");
	return RSManager::get().redirectDrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
}

HRESULT APIENTRY hkIDirect3DDevice9::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void *pVertexStreamZeroData, UINT VertexStreamZeroStride) {
	SDLOG(9, "DrawPrimitiveUP(%d, %u, %u, %u, %u, %p, %d, %p, %d)\n", PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
	return RSManager::get().redirectDrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT APIENTRY hkIDirect3DDevice9::DrawRectPatch(UINT Handle, CONST float *pNumSegs, CONST D3DRECTPATCH_INFO *pRectPatchInfo) {
	SDLOG(9, "DrawRectPatch\n");
	return m_pD3Ddev->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
}

HRESULT APIENTRY hkIDirect3DDevice9::DrawTriPatch(UINT Handle, CONST float *pNumSegs, CONST D3DTRIPATCH_INFO *pTriPatchInfo) {
	SDLOG(9, "DrawTriPatch\n");
	return m_pD3Ddev->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetBackBuffer(UINT iSwapChain,UINT iBackBuffer,D3DBACKBUFFER_TYPE Type,IDirect3DSurface9** ppBackBuffer) {
	return m_pD3Ddev->GetBackBuffer(iSwapChain, iBackBuffer, Type, ppBackBuffer);
}

HRESULT APIENTRY hkIDirect3DDevice9::EndScene(){
	SDLOG(7, "EndScene\n");
	return m_pD3Ddev->EndScene();
}

HRESULT APIENTRY hkIDirect3DDevice9::QueryInterface(REFIID riid, LPVOID *ppvObj) {
	return m_pD3Ddev->QueryInterface(riid, ppvObj);
}

ULONG APIENTRY hkIDirect3DDevice9::AddRef() {
	return m_pD3Ddev->AddRef();
}

HRESULT APIENTRY hkIDirect3DDevice9::BeginScene() {
	SDLOG(7, "BeginScene\n");
	return m_pD3Ddev->BeginScene();
}

HRESULT APIENTRY hkIDirect3DDevice9::BeginStateBlock() {
	return m_pD3Ddev->BeginStateBlock();
}

HRESULT APIENTRY hkIDirect3DDevice9::Clear(DWORD Count, CONST D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) {
	return m_pD3Ddev->Clear(Count, pRects, Flags, Color, Z, Stencil);
}

HRESULT APIENTRY hkIDirect3DDevice9::ColorFill(IDirect3DSurface9* pSurface,CONST RECT* pRect, D3DCOLOR color) {	
	return m_pD3Ddev->ColorFill(pSurface,pRect,color);
}

HRESULT APIENTRY hkIDirect3DDevice9::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DSwapChain9 **ppSwapChain) {
	return m_pD3Ddev->CreateAdditionalSwapChain(pPresentationParameters, ppSwapChain);
}

HRESULT APIENTRY hkIDirect3DDevice9::CreateCubeTexture(UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture9** ppCubeTexture,HANDLE* pSharedHandle) {
	return m_pD3Ddev->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture,pSharedHandle);
}

HRESULT APIENTRY hkIDirect3DDevice9::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
	SDLOG(4, "CreateDepthStencilSurface w/h: %4u/%4u  format: %s\n", Width, Height, D3DFormatToString(Format));
	return m_pD3Ddev->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);
}

HRESULT APIENTRY hkIDirect3DDevice9::CreateIndexBuffer(UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer9** ppIndexBuffer,HANDLE* pSharedHandle) {
	return m_pD3Ddev->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer,pSharedHandle);
}

HRESULT APIENTRY hkIDirect3DDevice9::CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
	SDLOG(4, "CreateOffscreenPlainSurface w/h: %4u/%4u  format: %s\n", Width, Height, D3DFormatToString(Format));
	return m_pD3Ddev->CreateOffscreenPlainSurface(Width,Height,Format,Pool,ppSurface,pSharedHandle);
}

HRESULT APIENTRY hkIDirect3DDevice9::CreatePixelShader(CONST DWORD* pFunction,IDirect3DPixelShader9** ppShader) {
	return RSManager::get().redirectCreatePixelShader(pFunction, ppShader);
}

HRESULT APIENTRY hkIDirect3DDevice9::CreateQuery(D3DQUERYTYPE Type,IDirect3DQuery9** ppQuery) {
	return m_pD3Ddev->CreateQuery(Type, ppQuery);
}

HRESULT APIENTRY hkIDirect3DDevice9::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
	SDLOG(1, "CreateRenderTarget w/h: %4u/%4u  format: %s\n", Width, Height, D3DFormatToString(Format));
	if(Width == 1024 && Height == 720) {
		SDLOG(1, " - OVERRIDE to %4u/%4u!\n", Settings::get().getRenderWidth(), Settings::get().getRenderHeight());
		HRESULT hr = m_pD3Ddev->CreateRenderTarget(Settings::get().getRenderWidth(), Settings::get().getRenderHeight(), Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);
		RSManager::get().registerMainRenderSurface(*ppSurface);
		return hr;
	}
	return m_pD3Ddev->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);
}

HRESULT APIENTRY hkIDirect3DDevice9::CreateStateBlock(D3DSTATEBLOCKTYPE Type,IDirect3DStateBlock9** ppSB) {
	return m_pD3Ddev->CreateStateBlock(Type, ppSB);
}

UINT scaleW(UINT in) {
	return in * Settings::get().getRenderWidth() / 1280;
}
UINT scaleH(UINT in) {
	return in * Settings::get().getRenderHeight() / 720;
}

HRESULT APIENTRY hkIDirect3DDevice9::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) {
	SDLOG(1, "CreateTexture w/h: %4u/%4u    format: %s    RENDERTARGET=%d\n", Width, Height, D3DFormatToString(Format), Usage & D3DUSAGE_RENDERTARGET);
	
	// used to store buffers for dual-scene rendering
	if((Width == 896 && Height == 504) || (Width == 448 && Height == 252)  && (Usage & D3DUSAGE_RENDERTARGET && Format == D3DFMT_A8R8G8B8)) {
		Width = scaleW(Width);
		Height = scaleH(Height);
		SDLOG(1, " - storage RT OVERRIDE to %4u/%4u!\n", Width, Height);
	}

	// shadowmap RTs
	else if((Width == Height) && ((Usage & D3DUSAGE_RENDERTARGET && Format == D3DFMT_A8R8G8B8) || Format == D3DFMT_D16)) {
		Width *= Settings::get().getShadowMapScale();
		Height *= Settings::get().getShadowMapScale();
		if(Format == D3DFMT_D16 && Settings::get().getImproveShadowPrecision()) Format = D3DFMT_D32F_LOCKABLE;
		SDLOG(1, " - shadowmap OVERRIDE to %4u/%4u, %s!\n", Width, Height, D3DFormatToString(Format));
	}

	// main render buffers
	else if(Width == 1280 && Height == 720 && (Usage & D3DUSAGE_RENDERTARGET || Format == D3DFMT_D24S8)) {
		Width = Settings::get().getRenderWidth();
		Height = Settings::get().getRenderHeight();
		// SSAO requires higher precision depth RT
		if(Settings::get().getSsaoStrength() != 0 && Format == D3DFMT_A8R8G8B8) Format = D3DFMT_A16B16G16R16F;
		SDLOG(1, " - main RT OVERRIDE to %4u/%4u!\n", Width, Height);
	}

	// mirror buffers -- be careful not to hit normal textures by mistake!
	else if(((Width == 640 && Height == 360) || (Width == 320 && Height == 180))
			&& (Format == D3DFMT_D24S8 || Usage & D3DUSAGE_RENDERTARGET)) {
		Width *= Settings::get().getReflectionScale();
		Height *= Settings::get().getReflectionScale();
	}

	// DoF buffers
	else if(Settings::get().getImproveDOF() && Width == 448 && Height == 252) {
		Width = (int)(Settings::get().getRenderWidth()*0.35f);
		Height = (int)(Settings::get().getRenderHeight()*0.35f);
		SDLOG(1, " - DoF OVERRIDE to %4u/%4u!\n", Width, Height);
	}

	HRESULT res = m_pD3Ddev->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
	return res;
}

HRESULT APIENTRY hkIDirect3DDevice9::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** VERTexBuffer, HANDLE* pSharedHandle) {
	SDLOG(4, "CreateVertexBuffer: length %d, usage: %p, fvf: %p, pool: %d\n", Length, Usage, FVF, Pool);
	HRESULT retval = m_pD3Ddev->CreateVertexBuffer(Length, Usage, FVF, Pool, VERTexBuffer, pSharedHandle);
	SDLOG(4, " --> %p\n", *VERTexBuffer);
	return retval;
}

HRESULT APIENTRY hkIDirect3DDevice9::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements,IDirect3DVertexDeclaration9** ppDecl) {
	return m_pD3Ddev->CreateVertexDeclaration(pVertexElements,ppDecl);
}

HRESULT APIENTRY hkIDirect3DDevice9::CreateVertexShader(CONST DWORD* pFunction,IDirect3DVertexShader9** ppShader) {
	return RSManager::get().redirectCreateVertexShader(pFunction, ppShader);
}

HRESULT APIENTRY hkIDirect3DDevice9::CreateVolumeTexture(UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture9** ppVolumeTexture,HANDLE* pSharedHandle) {
	return m_pD3Ddev->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture,pSharedHandle);
}

HRESULT APIENTRY hkIDirect3DDevice9::DeletePatch(UINT Handle) {
	return m_pD3Ddev->DeletePatch(Handle);
}

HRESULT APIENTRY hkIDirect3DDevice9::EndStateBlock(IDirect3DStateBlock9** ppSB) {
	return m_pD3Ddev->EndStateBlock(ppSB);
}

HRESULT APIENTRY hkIDirect3DDevice9::EvictManagedResources() {
	return m_pD3Ddev->EvictManagedResources();
}

UINT APIENTRY hkIDirect3DDevice9::GetAvailableTextureMem() {
	return m_pD3Ddev->GetAvailableTextureMem();
}

HRESULT APIENTRY hkIDirect3DDevice9::GetClipPlane(DWORD Index, float *pPlane) {
	return m_pD3Ddev->GetClipPlane(Index, pPlane);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetClipStatus(D3DCLIPSTATUS9 *pClipStatus) {
	return m_pD3Ddev->GetClipStatus(pClipStatus);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) {
	return m_pD3Ddev->GetCreationParameters(pParameters);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetCurrentTexturePalette(UINT *pPaletteNumber){
	return m_pD3Ddev->GetCurrentTexturePalette(pPaletteNumber);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetDepthStencilSurface(IDirect3DSurface9 **ppZStencilSurface) {
	return m_pD3Ddev->GetDepthStencilSurface(ppZStencilSurface);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetDeviceCaps(D3DCAPS9 *pCaps) {
	return m_pD3Ddev->GetDeviceCaps(pCaps);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetDirect3D(IDirect3D9 **ppD3D9) {
	HRESULT hRet = m_pD3Ddev->GetDirect3D(ppD3D9);
	if( SUCCEEDED(hRet) )
		*ppD3D9 = m_pD3Dint;
	return hRet;
}

HRESULT APIENTRY hkIDirect3DDevice9::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode) {
	SDLOG(15, "GetDisplayMode %u\n", iSwapChain);
	//pMode = &displayMode;
	//return S_OK;
	return m_pD3Ddev->GetDisplayMode(iSwapChain, pMode);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) {
	return m_pD3Ddev->GetFrontBufferData(iSwapChain, pDestSurface);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetFVF(DWORD* pFVF) {
	return m_pD3Ddev->GetFVF(pFVF);
}

void APIENTRY hkIDirect3DDevice9::GetGammaRamp(UINT iSwapChain,D3DGAMMARAMP* pRamp) {
	m_pD3Ddev->GetGammaRamp(iSwapChain,pRamp);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetIndices(IDirect3DIndexBuffer9** ppIndexData) {
	return m_pD3Ddev->GetIndices(ppIndexData);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetLight(DWORD Index, D3DLIGHT9 *pLight) {
	return m_pD3Ddev->GetLight(Index, pLight);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetLightEnable(DWORD Index, BOOL *pEnable) {
	return m_pD3Ddev->GetLightEnable(Index, pEnable);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetMaterial(D3DMATERIAL9 *pMaterial) {
	return m_pD3Ddev->GetMaterial(pMaterial);
}

float APIENTRY hkIDirect3DDevice9::GetNPatchMode() {
	return m_pD3Ddev->GetNPatchMode();
}

unsigned int APIENTRY hkIDirect3DDevice9::GetNumberOfSwapChains() {
	return m_pD3Ddev->GetNumberOfSwapChains();
}

HRESULT APIENTRY hkIDirect3DDevice9::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY *pEntries){
	return m_pD3Ddev->GetPaletteEntries(PaletteNumber, pEntries);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetPixelShader(IDirect3DPixelShader9** ppShader) {
	return m_pD3Ddev->GetPixelShader(ppShader);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetPixelShaderConstantB(UINT StartRegister,BOOL* pConstantData,UINT BoolCount) {
	return m_pD3Ddev->GetPixelShaderConstantB(StartRegister,pConstantData,BoolCount);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetPixelShaderConstantF(UINT StartRegister,float* pConstantData,UINT Vector4fCount) {
	return m_pD3Ddev->GetPixelShaderConstantF(StartRegister,pConstantData,Vector4fCount);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetPixelShaderConstantI(UINT StartRegister,int* pConstantData,UINT Vector4iCount){
	return m_pD3Ddev->GetPixelShaderConstantI(StartRegister,pConstantData,Vector4iCount);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetRasterStatus(UINT iSwapChain,D3DRASTER_STATUS* pRasterStatus) {
	return m_pD3Ddev->GetRasterStatus(iSwapChain,pRasterStatus);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue) {
	return m_pD3Ddev->GetRenderState(State, pValue);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetRenderTarget(DWORD renderTargetIndex,IDirect3DSurface9** ppRenderTarget) {
	return m_pD3Ddev->GetRenderTarget(renderTargetIndex,ppRenderTarget);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetRenderTargetData(IDirect3DSurface9* renderTarget,IDirect3DSurface9* pDestSurface) {
	return m_pD3Ddev->GetRenderTargetData(renderTarget,pDestSurface);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetSamplerState(DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD* pValue) {
	return m_pD3Ddev->GetSamplerState(Sampler,Type,pValue);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetScissorRect(RECT* pRect) {
	return m_pD3Ddev->GetScissorRect(pRect);
}

BOOL APIENTRY hkIDirect3DDevice9::GetSoftwareVertexProcessing() {
	return m_pD3Ddev->GetSoftwareVertexProcessing();
}

HRESULT APIENTRY hkIDirect3DDevice9::GetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9** ppStreamData,UINT* OffsetInBytes,UINT* pStride) {
	return m_pD3Ddev->GetStreamSource(StreamNumber, ppStreamData,OffsetInBytes, pStride);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetStreamSourceFreq(UINT StreamNumber,UINT* Divider) {
	return m_pD3Ddev->GetStreamSourceFreq(StreamNumber,Divider);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetSwapChain(UINT iSwapChain,IDirect3DSwapChain9** pSwapChain){
	return m_pD3Ddev->GetSwapChain(iSwapChain,pSwapChain);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture) {
	return m_pD3Ddev->GetTexture(Stage, ppTexture);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue) {
	return m_pD3Ddev->GetTextureStageState(Stage, Type, pValue);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix) {
	return m_pD3Ddev->GetTransform(State, pMatrix);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) {
	return m_pD3Ddev->GetVertexDeclaration(ppDecl);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetVertexShader(IDirect3DVertexShader9** ppShader) {
	return m_pD3Ddev->GetVertexShader(ppShader);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetVertexShaderConstantB(UINT StartRegister,BOOL* pConstantData,UINT BoolCount){
	return m_pD3Ddev->GetVertexShaderConstantB(StartRegister,pConstantData,BoolCount);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetVertexShaderConstantF(UINT StartRegister,float* pConstantData,UINT Vector4fCount) {
	return m_pD3Ddev->GetVertexShaderConstantF(StartRegister,pConstantData,Vector4fCount);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetVertexShaderConstantI(UINT StartRegister,int* pConstantData,UINT Vector4iCount){
	return m_pD3Ddev->GetVertexShaderConstantI(StartRegister,pConstantData,Vector4iCount);
}

HRESULT APIENTRY hkIDirect3DDevice9::GetViewport(D3DVIEWPORT9 *pViewport) {
	return m_pD3Ddev->GetViewport(pViewport);
}

HRESULT APIENTRY hkIDirect3DDevice9::LightEnable(DWORD LightIndex, BOOL bEnable) {
	return m_pD3Ddev->LightEnable(LightIndex, bEnable);
}

HRESULT APIENTRY hkIDirect3DDevice9::MultiplyTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX *pMatrix) {
	SDLOG(5, "MultiplyTransform state: %u matrix: \n%s\n", State, D3DMatrixToString(pMatrix));
	return m_pD3Ddev->MultiplyTransform(State, pMatrix);
}

void APIENTRY WINAPI D3DPERF_SetOptions(DWORD options){
	//MessageBox(NULL, "D3DPERF_SetOptions", "D3D9Wrapper", MB_OK);
}

HRESULT APIENTRY hkIDirect3DDevice9::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) {
	return m_pD3Ddev->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer,pVertexDecl, Flags);
}

ULONG APIENTRY hkIDirect3DDevice9::Release() {
	return m_pD3Ddev->Release();
}

HRESULT APIENTRY hkIDirect3DDevice9::Reset(D3DPRESENT_PARAMETERS *pPresentationParameters) {
	RSManager::get().releaseResources();
	SDLOG(0, "Reset ------\n");
	displayMode.Format = pPresentationParameters->BackBufferFormat;
	displayMode.Width = pPresentationParameters->BackBufferWidth;
	displayMode.Height = pPresentationParameters->BackBufferHeight;
	displayMode.RefreshRate = 60;

	RSManager::get().adjustPresentationParameters(pPresentationParameters);
	
	HRESULT hRet = m_pD3Ddev->Reset(pPresentationParameters);

	if(SUCCEEDED(hRet)) {
		SDLOG(0, " - succeeded\n");
		m_PresentParam = *pPresentationParameters;
		RSManager::get().initResources();
	} else {
		SDLOG(0, "ERROR: Reset Failed!\n");
		SDLOG(0, "Error code: %s\n", DXGetErrorString(hRet));
	}
	
	return hRet;
}

HRESULT APIENTRY hkIDirect3DDevice9::SetClipPlane(DWORD Index, CONST float *pPlane) {
	SDLOG(7, "SetClipPlane %d: %f / %f / %f / %f\n", 0, pPlane[0], pPlane[1], pPlane[2], pPlane[3]);
	return m_pD3Ddev->SetClipPlane(Index, pPlane);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetClipStatus(CONST D3DCLIPSTATUS9 *pClipStatus) {
	return m_pD3Ddev->SetClipStatus(pClipStatus);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetCurrentTexturePalette(UINT PaletteNumber) {
	return m_pD3Ddev->SetCurrentTexturePalette(PaletteNumber);
}

void APIENTRY hkIDirect3DDevice9::SetCursorPosition(int X, int Y, DWORD Flags) {
	m_pD3Ddev->SetCursorPosition(X, Y, Flags);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9 *pCursorBitmap) {
	return m_pD3Ddev->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) {
	SDLOG(5, "SetDepthStencilSurface %p\n", pNewZStencil);
	//return m_pD3Ddev->SetDepthStencilSurface(pNewZStencil);
	return RSManager::get().redirectSetDepthStencilSurface(pNewZStencil);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetDialogBoxMode(BOOL bEnableDialogs) {
	return m_pD3Ddev->SetDialogBoxMode(bEnableDialogs);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetFVF(DWORD FVF) {
	return m_pD3Ddev->SetFVF(FVF);
}

void APIENTRY hkIDirect3DDevice9::SetGammaRamp(UINT iSwapChain,DWORD Flags,CONST D3DGAMMARAMP* pRamp){
	m_pD3Ddev->SetGammaRamp(iSwapChain,Flags, pRamp);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetIndices(IDirect3DIndexBuffer9* pIndexData) {
	return m_pD3Ddev->SetIndices(pIndexData);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetLight(DWORD Index, CONST D3DLIGHT9 *pLight) {
	return m_pD3Ddev->SetLight(Index, pLight);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetMaterial(CONST D3DMATERIAL9 *pMaterial) {	
	return m_pD3Ddev->SetMaterial(pMaterial);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetNPatchMode(float nSegments) {	
	return m_pD3Ddev->SetNPatchMode(nSegments);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetPaletteEntries(UINT PaletteNumber, CONST PALETTEENTRY *pEntries) {
	return m_pD3Ddev->SetPaletteEntries(PaletteNumber, pEntries);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetPixelShader(IDirect3DPixelShader9* pShader) {
	SDLOG(3, "SetPixelShader %p\n", pShader);
	return m_pD3Ddev->SetPixelShader(pShader);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetPixelShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount) {
	SDLOG(6, "SetPixelShaderConstantB: start: %u, count: %u\n", StartRegister, BoolCount);
	if(Settings::get().getLogLevel() > 13) {
		for(size_t i=0; i<BoolCount; ++i) {
			SDLOG(0, " - % 5s\n", pConstantData[i] ? "true" : "false");
		}
	}
	return m_pD3Ddev->SetPixelShaderConstantB(StartRegister,pConstantData,BoolCount);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetPixelShaderConstantF(UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount) {
	SDLOG(6, "SetPixelShaderConstantF: start: %u, count: %u\n", StartRegister, Vector4fCount);
	if(Settings::get().getLogLevel() > 13) {
		for(size_t i=0; i<Vector4fCount; ++i) {
			SDLOG(0, " - %16.10f %16.10f %16.10f %16.10f\n", pConstantData[i*4+0], pConstantData[i*4+1], pConstantData[i*4+2], pConstantData[i*4+3]);
		}
	}
	return RSManager::get().redirectSetPixelShaderConstantF(StartRegister,pConstantData,Vector4fCount);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetPixelShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount) {
	SDLOG(6, "SetPixelShaderConstantI: start: %u, count: %u\n", StartRegister, Vector4iCount);
	if(Settings::get().getLogLevel() > 13) {
		for(size_t i=0; i<Vector4iCount; ++i) {
			SDLOG(0, " - % 16d % 16d % 16d % 16d\n", pConstantData[i*4+0], pConstantData[i*4+1], pConstantData[i*4+2], pConstantData[i*4+3]);
		}
	}
	return m_pD3Ddev->SetPixelShaderConstantI(StartRegister,pConstantData,Vector4iCount);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
	return RSManager::get().redirectSetRenderState(State, Value);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) {
	SDLOG(14, "SetSamplerState sampler %lu:   state type: %s   value: %lu\n", Sampler, D3DSamplerStateTypeToString(Type), Value);
	if(Settings::get().getFilteringOverride() == 2) {
		SDLOG(10, " - aniso sampling activated!\n");
		if(Type == D3DSAMP_MAXANISOTROPY) {
			return m_pD3Ddev->SetSamplerState(Sampler, Type, 16);
		} else if(Type != D3DSAMP_MINFILTER && Type != D3DSAMP_MAGFILTER) {
			return m_pD3Ddev->SetSamplerState(Sampler, Type, Value);
		} else {
			return m_pD3Ddev->SetSamplerState(Sampler, Type, D3DTEXF_ANISOTROPIC);
		}
	} else if(Settings::get().getFilteringOverride() == 1) {
		if((Type == D3DSAMP_MINFILTER || Type == D3DSAMP_MIPFILTER) && (Value == D3DTEXF_POINT || Value == D3DTEXF_NONE)) {
			SDLOG(10, " - linear override activated!\n");
			return m_pD3Ddev->SetSamplerState(Sampler, Type, D3DTEXF_LINEAR);
		}
	}
	return m_pD3Ddev->SetSamplerState(Sampler, Type, Value);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetScissorRect(CONST RECT* pRect) {
	SDLOG(5, "SetScissorRect %s\n", RectToString(pRect));
	return m_pD3Ddev->SetScissorRect(pRect);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetSoftwareVertexProcessing(BOOL bSoftware) {
	return m_pD3Ddev->SetSoftwareVertexProcessing(bSoftware);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride) {
	SDLOG(4, "SetStreamSource %d: %p (%u, %u)\n", StreamNumber, pStreamData, OffsetInBytes, Stride);
	return RSManager::get().redirectSetStreamSource(StreamNumber, pStreamData,OffsetInBytes, Stride);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetStreamSourceFreq(UINT StreamNumber,UINT Divider){
	SDLOG(4, "SetStreamSourceFreq %d: %u\n", StreamNumber, Divider);
	return m_pD3Ddev->SetStreamSourceFreq(StreamNumber, Divider);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture) {
	unsigned index = RSManager::get().getTextureIndex((IDirect3DTexture9*)pTexture);
	SDLOG(6, "setTexture %d, %p (index %u)\n", Stage, pTexture, index);
	if(Settings::get().getLogLevel() > 10 && pTexture) {
		IDirect3DTexture9 *tex;
		if(pTexture->QueryInterface(IID_IDirect3DTexture9, (void**)&tex) == S_OK) {
			D3DSURFACE_DESC desc;
			tex->GetLevelDesc(0, &desc);
			SDLOG(10, " -- size: %dx%d RT? %s\n", desc.Width, desc.Height, (desc.Usage & D3DUSAGE_RENDERTARGET) ? "true" : "false");
		}
	}
	return RSManager::get().redirectSetTexture(Stage, pTexture);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) {
	return RSManager::get().redirectSetTextureStageState(Stage, Type, Value);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX *pMatrix) {
	SDLOG(0, "SetTransform state: %u matrix: \n%s\n", State, D3DMatrixToString(pMatrix));
	return m_pD3Ddev->SetTransform(State, pMatrix);	
}

HRESULT APIENTRY hkIDirect3DDevice9::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) {
	return m_pD3Ddev->SetVertexDeclaration(pDecl);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetVertexShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount) {
	SDLOG(4, "SetVertexShaderConstantB: start: %u, count: %u\n", StartRegister, BoolCount);
	if(Settings::get().getLogLevel() > 13) {
		for(size_t i=0; i<BoolCount; ++i) {
			SDLOG(0, " - %s\n", pConstantData[i] ? "true" : "false");
		}
	}
	return m_pD3Ddev->SetVertexShaderConstantB(StartRegister,pConstantData,BoolCount);
}

HRESULT APIENTRY hkIDirect3DDevice9::SetVertexShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount) {
	SDLOG(4, "SetVertexShaderConstantI: start: %u, count: %u\n", StartRegister, Vector4iCount);
	if(Settings::get().getLogLevel() > 13) {
		for(size_t i=0; i<Vector4iCount; ++i) {
			SDLOG(0, " - % 16d % 16d % 16d % 16d\n", pConstantData[i*4+0], pConstantData[i*4+1], pConstantData[i*4+2], pConstantData[i*4+3]);
		}
	}
	return m_pD3Ddev->SetVertexShaderConstantI(StartRegister,pConstantData,Vector4iCount);
}

BOOL APIENTRY hkIDirect3DDevice9::ShowCursor(BOOL bShow) {
	return m_pD3Ddev->ShowCursor(bShow);
}

HRESULT APIENTRY hkIDirect3DDevice9::StretchRect(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST RECT* pDestRect,D3DTEXTUREFILTERTYPE Filter) {
	SDLOG(5, "StretchRect src -> dest, sR -> dR : %p -> %p,  %s -> %s\n", pSourceSurface, pDestSurface, RectToString(pSourceRect), RectToString(pDestRect));
	return RSManager::get().redirectStretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter);
}

HRESULT APIENTRY hkIDirect3DDevice9::TestCooperativeLevel() {
	SDLOG(8, "TestCooperativeLevel");
	HRESULT hr = m_pD3Ddev->TestCooperativeLevel();
	SDLOG(8, " - returned %s\n", (hr ==  D3D_OK ? "OK" : (D3DERR_DEVICELOST ? "DEVICELOST" : (hr == D3DERR_DEVICENOTRESET ? "DEVICENOTRESET" : "DRIVERINTERNALERROR"))));
	return hr;
}

HRESULT APIENTRY hkIDirect3DDevice9::UpdateSurface(IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestinationSurface,CONST POINT* pDestPoint) {
	SDLOG(6, "UpdateSurface source: %p - %s   dest: %p - %d/%d\n", pSourceSurface, RectToString(pSourceRect), pDestinationSurface, pDestPoint->x, pDestPoint->y);
	return m_pD3Ddev->UpdateSurface(pSourceSurface,pSourceRect,pDestinationSurface,pDestPoint);
}

HRESULT APIENTRY hkIDirect3DDevice9::UpdateTexture(IDirect3DBaseTexture9 *pSourceTexture, IDirect3DBaseTexture9 *pDestinationTexture) {
	SDLOG(6, "UpdateTexture source: %p    dest: %p\n", pSourceTexture, pDestinationTexture);
	return m_pD3Ddev->UpdateTexture(pSourceTexture, pDestinationTexture);
}

HRESULT APIENTRY hkIDirect3DDevice9::ValidateDevice(DWORD *pNumPasses) {
	return m_pD3Ddev->ValidateDevice(pNumPasses);
}

#pragma once

#include <map>
#include <set>
#include <vector>

#include "d3d9.h"
#include "SMAA.h"
#include "FXAA.h"
#include "SSAO.h"
#include "GAUSS.h"

class RSManager {
	static RSManager instance;

	bool inited;
	D3DVIEWPORT9 viewport;
	IDirect3DDevice9 *d3ddev;

	bool doAA;
	SMAA* smaa;
	FXAA* fxaa;
	
	bool doSsao;
	SSAO* ssao;

	bool doDofGauss;
	GAUSS* gauss;
	
	IDirect3DTexture9* rgbaBuffer1Tex;
	IDirect3DSurface9* rgbaBuffer1Surf;
	IDirect3DSurface9* depthStencilSurf;

	bool hideHud;
	
	std::set<int> dumpedTextures;

	unsigned texIndex, mainRenderTexIndex, mainRenderSurfIndex;
	typedef std::map<IDirect3DTexture9*, int> TexIntMap;
	TexIntMap texIndices, mainRenderTexIndices;
	typedef std::map<IDirect3DSurface9*, int> SurfIntMap;
	SurfIntMap mainRenderSurfIndices;

	bool captureNextFrame, capturing, hudStarted, takeScreenshot, takeHudlessScreenshot;
	unsigned dumpCaptureIndex;

	void dumpSurface(const char* name, IDirect3DSurface9* surface);

	#define TEXTURE(_name, _hash) \
	private: \
	static const UINT32 texture##_name##Hash = _hash; \
	IDirect3DTexture9* texture##_name; \
	bool isTexture##_name(IDirect3DBaseTexture9* pTexture) { return texture##_name && ((IDirect3DTexture9*)pTexture) == texture##_name; };
	#include "Textures.def"
	#undef TEXTURE
	const char* getTextureName(IDirect3DBaseTexture9* pTexture);

	unsigned numKnownTextures, foundKnownTextures, nrts, mainRTCount, vsswitch;
	
	bool onBackbuffer, firstStreamSource, lastT1024;

	D3DVIEWPORT9 lastVp;

	// was the last SetVertexShaderConstantF at (0,1) a pixel size?
	bool lastVSC0;

	void registerKnowTexture(LPCVOID pSrcData, UINT SrcDataSize, LPDIRECT3DTEXTURE9 pTexture);
	IDirect3DTexture9* getSurfTexture(IDirect3DSurface9* pSurface);

	// Render state store/restore
	void storeRenderState();
	void restoreRenderState();
	IDirect3DVertexDeclaration9* prevVDecl;
	IDirect3DSurface9* prevDepthStencilSurf;
	IDirect3DSurface9* prevRenderTarget;
	IDirect3DTexture9* prevRenderTex;
	IDirect3DStateBlock9* prevStateBlock;

	IDirect3DSurface9 *normalSurface, *depthSurface, *mainSurface, *lastRTSurface;
	void captureRTScreen();

public:
	static RSManager& get() {
		return instance;
	}

	RSManager() : smaa(NULL), fxaa(NULL), ssao(NULL), gauss(NULL), rgbaBuffer1Surf(NULL), rgbaBuffer1Tex(NULL), lastRTSurface(NULL),
			inited(false), doAA(true), doSsao(true), doDofGauss(true), captureNextFrame(false), capturing(false), hudStarted(false), takeScreenshot(false), takeHudlessScreenshot(false), hideHud(false),
			mainRenderTexIndex(0), mainRenderSurfIndex(0), dumpCaptureIndex(0), numKnownTextures(0), foundKnownTextures(0), onBackbuffer(false) {
		#define TEXTURE(_name, _hash) ++numKnownTextures;
		#include "Textures.def"
		#undef TEXTURE
	}
	
	void setD3DDevice(IDirect3DDevice9 *pD3Ddev) { d3ddev = pD3Ddev; }

	void initResources();
	void releaseResources();

	void adjustPresentationParameters(D3DPRESENT_PARAMETERS *pPresentationParameters);
	void enableSingleFrameCapture();
	void enableTakeScreenshot();
	void enableTakeHudlessScreenshot();
	bool takingScreenshot() { return takeScreenshot; }

	void toggleAA() { doAA = !doAA; }
	void toggleVssao() { doSsao = !doSsao; }
	void toggleHideHud() { hideHud = !hideHud; }
	void toggleDofGauss() { doDofGauss = !doDofGauss; }

	void reloadVssao();
	void reloadVssao2();
	void reloadHbao();
	void reloadScao();
	void reloadGauss();
	void reloadAA();
	
	void registerMainRenderTexture(IDirect3DTexture9* pTexture);
	void registerMainRenderSurface(IDirect3DSurface9* pSurface);
	unsigned getTextureIndex(IDirect3DTexture9* ppTexture);
	void registerD3DXCreateTextureFromFileInMemory(LPCVOID pSrcData, UINT SrcDataSize, LPDIRECT3DTEXTURE9 pTexture);
	void registerD3DXCompileShader(LPCSTR pSrcData, UINT srcDataLen, const D3DXMACRO *pDefines, LPD3DXINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, DWORD Flags, LPD3DXBUFFER * ppShader, LPD3DXBUFFER * ppErrorMsgs, LPD3DXCONSTANTTABLE * ppConstantTable);
	
	HRESULT redirectSetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget);
	HRESULT redirectStretchRect(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter);
	HRESULT redirectSetTexture(DWORD Stage, IDirect3DBaseTexture9 * pTexture);
	HRESULT redirectSetDepthStencilSurface(IDirect3DSurface9* pNewZStencil);
	HRESULT redirectPresent(CONST RECT * pSourceRect, CONST RECT * pDestRect, HWND hDestWindowOverride, CONST RGNDATA * pDirtyRegion);
	HRESULT redirectDrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinIndex, UINT NumVertices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride);
	HRESULT redirectDrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride);
	HRESULT redirectD3DXCreateTextureFromFileInMemoryEx(LPDIRECT3DDEVICE9 pDevice, LPCVOID pSrcData, UINT SrcDataSize, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO* pSrcInfo, PALETTEENTRY* pPalette, LPDIRECT3DTEXTURE9* ppTexture);
	HRESULT redirectSetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value);
	HRESULT redirectSetRenderState(D3DRENDERSTATETYPE State, DWORD Value);
	HRESULT redirectSetVertexShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount);
	HRESULT redirectSetVertexShader(IDirect3DVertexShader9* pvShader);
	HRESULT redirectCreateVertexShader(CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader);
	HRESULT redirectCreatePixelShader(CONST DWORD* pFunction, IDirect3DPixelShader9** ppShader);
	HRESULT redirectSetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride);
	HRESULT redirectDrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount);
	HRESULT redirectDrawIndexedPrimitive(D3DPRIMITIVETYPE Type, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount);
	HRESULT redirectSetPixelShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount);
	HRESULT redirectSetViewport(D3DVIEWPORT9* vp);
};

#include "RenderstateManager.h"

#include <time.h>
#include <intsafe.h>
#include <io.h>
#include <fstream>
#include <string>

#include "d3dutil.h"
#include "Settings.h"
#include "Hash.h"
#include "Detouring.h"
#include "WindowManager.h"

RSManager RSManager::instance;

void RSManager::initResources() {
	SDLOG(0, "RenderstateManager resource initialization started\n");
	unsigned rw = Settings::get().getRenderWidth(), rh = Settings::get().getRenderHeight();
	if(Settings::get().getAAQuality()) {
		if(Settings::get().getAAType() == "SMAA") {
			smaa = new SMAA(d3ddev, rw, rh, (SMAA::Preset)(Settings::get().getAAQuality()-1));
		} else {
			fxaa = new FXAA(d3ddev, rw, rh, (FXAA::Quality)(Settings::get().getAAQuality()-1));
		}
	}
	if(Settings::get().getSsaoStrength()) ssao = new SSAO(d3ddev, rw, rh, Settings::get().getSsaoStrength()-1, 
		(Settings::get().getSsaoType() == "VSSAO") ? SSAO::VSSAO : SSAO::VSSAO2);
	if(Settings::get().getAddDOFBlur() > 0) gauss = new GAUSS(d3ddev, (int)(rw*0.35f), (int)(rh*0.35f));
	d3ddev->CreateTexture(rw, rh, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &rgbaBuffer1Tex, NULL);
	rgbaBuffer1Tex->GetSurfaceLevel(0, &rgbaBuffer1Surf);
	d3ddev->CreateDepthStencilSurface(rw, rh, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, false, &depthStencilSurf, NULL);
	d3ddev->CreateStateBlock(D3DSBT_ALL, &prevStateBlock);
	SDLOG(0, "RenderstateManager resource initialization completed\n");
	if(!inited) {
		startDetour(); // on first init only
		if(Settings::get().getDisableJoystick()) startJoyDetour();
	} 
	inited = true;
}

void RSManager::releaseResources() {
	SDLOG(0, "RenderstateManager releasing resources\n");
	SAFERELEASE(rgbaBuffer1Surf);
	SAFERELEASE(rgbaBuffer1Tex);
	SAFERELEASE(depthStencilSurf);
	SAFERELEASE(prevStateBlock);
	SAFEDELETE(smaa);
	SAFEDELETE(fxaa);
	SAFEDELETE(ssao);
	SAFEDELETE(gauss);
	SDLOG(0, "RenderstateManager resource release completed\n");
}

HRESULT RSManager::redirectPresent(CONST RECT *pSourceRect, CONST RECT *pDestRect, HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion) {
	if(takeScreenshot) {
		takeScreenshot = false;
		captureRTScreen();
	}

	if(capturing) {
		capturing = false;
		Settings::get().restoreLogLevel();
	}
	if(captureNextFrame) {
		capturing = true;
		captureNextFrame = false;
		Settings::get().elevateLogLevel(30);
		SDLOG(0, "== CAPTURING FRAME ==\n")
	}
	hudStarted = false;
	nrts = 0;
	normalSurface = NULL;
	SAFERELEASE(depthSurface);
	SAFERELEASE(mainSurface);
	mainRTCount = 0;
	vsswitch = 0;
	
	//if(Settings::get().getEnableTripleBuffering()) return ((IDirect3DDevice9Ex*)d3ddev)->PresentEx(NULL, NULL, NULL, NULL, D3DPRESENT_FORCEIMMEDIATE);
	return d3ddev->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

void RSManager::adjustPresentationParameters(D3DPRESENT_PARAMETERS *pPresentationParameters) {
	SDLOG(0, " - requested mode:\n");
	SDLOG(0, " - - Backbuffer(s): %4u x %4u %16s *%d \n", pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight, D3DFormatToString(pPresentationParameters->BackBufferFormat), pPresentationParameters->BackBufferCount);
	SDLOG(0, " - - PresentationInterval: %2u   Windowed: %5s    Refresh: %3u Hz\n", pPresentationParameters->PresentationInterval, pPresentationParameters->Windowed ? "true" : "false", pPresentationParameters->FullScreen_RefreshRateInHz);

	if(pPresentationParameters->BackBufferCount > 0) {
		if(Settings::get().getBorderlessFullscreen() || Settings::get().getForceWindowed()) {
			pPresentationParameters->Windowed = TRUE;
			pPresentationParameters->BackBufferFormat = D3DFMT_UNKNOWN;
			pPresentationParameters->hDeviceWindow = GetActiveWindow();
			//pPresentationParameters->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		} else {
			pPresentationParameters->Windowed = FALSE;
			pPresentationParameters->FullScreen_RefreshRateInHz = Settings::get().getFullscreenHz();
			pPresentationParameters->BackBufferFormat = D3DFMT_A8R8G8B8;
		} 
		pPresentationParameters->BackBufferWidth = Settings::get().getPresentWidth();
		pPresentationParameters->BackBufferHeight = Settings::get().getPresentHeight();
	} 
}

void RSManager::captureRTScreen() {
	SDLOG(0, "Capturing screenshot\n");
	char timebuf[128], buffer[512];
	time_t ltime;
	time(&ltime);
	struct tm *timeinfo;
	timeinfo = localtime(&ltime);
	strftime(timebuf, 128, "screenshot_%Y-%m-%d_%H-%M-%S.png", timeinfo);
	sprintf(buffer, "%s\\%s", Settings::get().getScreenshotDir().c_str(), timebuf);
	SDLOG(0, " - to %s\n", buffer);
		
	IDirect3DSurface9 *render = NULL;
	d3ddev->GetRenderTarget(0, &render);
	if(render) {
		D3DXSaveSurfaceToFile(buffer, D3DXIFF_PNG, render, NULL, NULL);
	}
	SAFERELEASE(render);
}

void RSManager::dumpSurface(const char* name, IDirect3DSurface9* surface) {
	char fullname[128];
	sprintf_s(fullname, 128, "dump%03d_%s.tga", dumpCaptureIndex++, name);
	D3DXSaveSurfaceToFile(fullname, D3DXIFF_TGA, surface, NULL, NULL);
}

void RSManager::registerMainRenderTexture(IDirect3DTexture9* pTexture) {
	if(pTexture) {
		mainRenderTexIndices.insert(std::make_pair(pTexture, mainRenderTexIndex));
		SDLOG(4, "Registering main render tex: %p as #%d\n", pTexture, mainRenderTexIndex);
		mainRenderTexIndex++;
	}
}

void RSManager::registerMainRenderSurface(IDirect3DSurface9* pSurface) {
	if(pSurface) {
		mainRenderSurfIndices.insert(std::make_pair(pSurface, mainRenderSurfIndex));
		SDLOG(4, "Registering main render surface: %p as #%d\n", pSurface, mainRenderSurfIndex);
		mainRenderSurfIndex++;
	}
}


HRESULT RSManager::redirectSetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) {
	nrts++;
	firstStreamSource = true;

	if(RenderTargetIndex == 1 && pRenderTarget) { // we'll now be rendering the depth and normal buffers (in renderpath A)
		normalSurface = pRenderTarget;
		SDLOG(3, "~~ found normalSurface : %p\n", normalSurface);
		// depth is bound to 0
		d3ddev->GetRenderTarget(0, &depthSurface);
		SDLOG(3, "~~ found depthSurface : %p\n", depthSurface);
	}

	// if we don't have the z Surface but have the main surface it has to be the previously bound RT
	if(mainSurface && !depthSurface) depthSurface = lastRTSurface;

	// if we know the main surface, and are on the backbuffer, apply AA
	if(mainSurface && onBackbuffer) { 
		SDLOG(3, "~~ applying AA now\n");
		// final renderbuffer has to be from texture, just making sure here
		if(IDirect3DTexture9* tex = getSurfTexture(mainSurface)) {
			// check size just to make even more sure
			D3DSURFACE_DESC desc;
			mainSurface->GetDesc(&desc);
			if(desc.Width == Settings::get().getRenderWidth() && desc.Height == Settings::get().getRenderHeight()) {
				storeRenderState();
				d3ddev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
				d3ddev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
				d3ddev->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
				// perform AA processing
				if(doAA && (smaa || fxaa)) {
					if(smaa) smaa->go(tex, tex, rgbaBuffer1Surf, SMAA::INPUT_COLOR);
					else fxaa->go(tex, rgbaBuffer1Surf);
					d3ddev->StretchRect(rgbaBuffer1Surf, NULL, mainSurface, NULL, D3DTEXF_NONE);
				}
				// perform SSAO
				if(ssao && doSsao) {
					IDirect3DTexture9 *zTex = getSurfTexture(depthSurface);
					ssao->go(tex, zTex, rgbaBuffer1Surf);
					d3ddev->StretchRect(rgbaBuffer1Surf, NULL, mainSurface, NULL, D3DTEXF_NONE);
					zTex->Release();
				}
				restoreRenderState();
			}
			tex->Release();				
		}
		normalSurface = NULL;
		SAFERELEASE(depthSurface);
		SAFERELEASE(mainSurface);
		mainRTCount = 0;
	}

	onBackbuffer = false;
	if(RenderTargetIndex == 0) {
		IDirect3DSurface9 *bb0, *bb1;
		d3ddev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb0);
		d3ddev->GetBackBuffer(0, 1, D3DBACKBUFFER_TYPE_MONO, &bb1);
		if(pRenderTarget == bb0 || pRenderTarget == bb1) {
			onBackbuffer = true;
		}
		bb0->Release();
		bb1->Release();
	}

	// Perform DoF blur after a viable DoF target has been selected twice in direct succession
	if(gauss && doDofGauss) {
		static unsigned dofTargeted = 0;
		IDirect3DSurface9 *oldRenderTarget;
		d3ddev->GetRenderTarget(0, &oldRenderTarget);
		D3DSURFACE_DESC desc;
		oldRenderTarget->GetDesc(&desc);

		if(gauss->isDoFTarget(desc)) dofTargeted++;
		else dofTargeted = 0;

		if(dofTargeted == 2) {
			SDLOG(3,"~~ applying additional DoF blur\n");
			IDirect3DTexture9 *oldRTtex = getSurfTexture(oldRenderTarget);
			if(oldRTtex) {
				storeRenderState();
				for(size_t i=0; i<Settings::get().getAddDOFBlur(); ++i) gauss->go(oldRTtex, oldRenderTarget);
				restoreRenderState();
			}
			SAFERELEASE(oldRTtex);
		}
		SAFERELEASE(oldRenderTarget);
	}

	if(capturing) {
		IDirect3DSurface9 *oldRenderTarget, *depthStencilSurface;
		d3ddev->GetRenderTarget(RenderTargetIndex, &oldRenderTarget);
		d3ddev->GetDepthStencilSurface(&depthStencilSurface);
		char buffer[64];
		sprintf(buffer, "%03d_i%03d_oldRenderTarget_%p_.tga", nrts, RenderTargetIndex, oldRenderTarget);
		SDLOG(0, "Capturing surface %p as %s\n", oldRenderTarget, buffer);
		D3DXSaveSurfaceToFile(buffer, D3DXIFF_TGA, oldRenderTarget, NULL, NULL);
		if(depthStencilSurface) {
			sprintf(buffer, "%03d_oldRenderTargetDepth_%p_.tga", nrts, oldRenderTarget);
			SDLOG(0, "Capturing depth surface %p as %s\n", depthStencilSurface, buffer);
			D3DXSaveSurfaceToFile(buffer, D3DXIFF_TGA, depthStencilSurface, NULL, NULL);
		}
		SAFERELEASE(oldRenderTarget);
		SAFERELEASE(depthStencilSurface);
	}

	// store previous RT 0
	SAFERELEASE(lastRTSurface);
	d3ddev->GetRenderTarget(0, &lastRTSurface);

	return d3ddev->SetRenderTarget(RenderTargetIndex, pRenderTarget);
}

HRESULT RSManager::redirectStretchRect(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter) {
	//SurfSurfMap::iterator it = renderTexSurfTargets.find(pSourceSurface);
	//if(it != renderTexSurfTargets.end()) {
	//	SDLOG(1, "Redirected StretchRect %p -> %p to %p -> %p\n", pSourceSurface, pDestSurface, it->second, pDestSurface);
	//	if(capturing) dumpSurface("redirectStretchRect", it->second);
	//	return d3ddev->StretchRect(it->second, pSourceRect, pDestSurface, pDestRect, Filter);
	//}
	return d3ddev->StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, D3DTEXF_LINEAR);
}

HRESULT RSManager::redirectSetTexture(DWORD Stage, IDirect3DBaseTexture9 * pTexture) {
	if(pTexture == NULL) return d3ddev->SetTexture(Stage, pTexture);

	// If we set a renderwidth x renderheight rendertaget texture to stage 8, we are rendering the pre-final image
	if(depthSurface && normalSurface && Stage == 8) {
		IDirect3DTexture9 *tex;
		if(pTexture->QueryInterface(IID_IDirect3DTexture9, (void**)&tex) == S_OK) {
			D3DSURFACE_DESC desc;
			tex->GetLevelDesc(0, &desc);
			if(desc.Width == Settings::get().getRenderWidth() && desc.Height == Settings::get().getRenderHeight() && (desc.Usage & D3DUSAGE_RENDERTARGET)) {
				d3ddev->GetRenderTarget(0, &mainSurface);
				// only if its not a surface we already know!
				if(mainSurface && mainSurface != normalSurface && mainSurface != depthSurface) {
					SDLOG(3, "~~ found mainSurface : %p\n", mainSurface);
				} else {
					SAFERELEASE(mainSurface);
				}
			}
		}
	}

	// alternative surface detection if we don't have depth/normal surfaces
	// stage 5 -> 6 -> 7 set consecutively to RTs
	static int numRenderTargetTexs = 0;
	if(!mainSurface) {
		IDirect3DTexture9 *tex;
		if(pTexture->QueryInterface(IID_IDirect3DTexture9, (void**)&tex) == S_OK) {
			D3DSURFACE_DESC desc;
			tex->GetLevelDesc(0, &desc);
			
			if(numRenderTargetTexs == 0 && Stage == 5 && desc.Width == Settings::get().getRenderWidth() && desc.Height == Settings::get().getRenderHeight()) {
				numRenderTargetTexs++;
			} 
			else if(numRenderTargetTexs == 1 && Stage == 6 && desc.Width == Settings::get().getRenderWidth() && desc.Height == Settings::get().getRenderHeight()) {
				numRenderTargetTexs++;
			}
			else if(numRenderTargetTexs == 2 && Stage == 7 && desc.Width == Settings::get().getRenderWidth() && desc.Height == Settings::get().getRenderHeight()) {
				numRenderTargetTexs = 0;
				d3ddev->GetRenderTarget(0, &mainSurface);
				SDLOG(3, "~~ found mainSurface (alternative) : %p\n", mainSurface);
			} else {
				numRenderTargetTexs = 0;
			}
		}
	}

	// help with dual screen rendering detection
	lastT1024 = false;
	if(onBackbuffer && Stage == 0) {
		IDirect3DTexture9 *tex;
		if(pTexture->QueryInterface(IID_IDirect3DTexture9, (void**)&tex) == S_OK) {
			D3DSURFACE_DESC desc;
			tex->GetLevelDesc(0, &desc);
			if(desc.Width == 1024 && desc.Height == 1024 && !(desc.Usage & D3DUSAGE_RENDERTARGET)) {
				lastT1024 = true;
			}
		}
	}
	
	// if we set a renderwidth x renderheight RT to texture 1, we'll render the lightmap 
	// --> fix pixel offset in lightmap
	// None of this is actually necessary if you just get the vertex shader parameters right!
	//if(Stage==1) {
	//	IDirect3DTexture9 *tex;
	//	if(pTexture->QueryInterface(IID_IDirect3DTexture9, (void**)&tex) == S_OK) {
	//		D3DSURFACE_DESC desc;
	//		tex->GetLevelDesc(0, &desc);
	//		
	//		if(desc.Width == Settings::get().getRenderWidth() && desc.Height == Settings::get().getRenderHeight() && (desc.Usage & D3DUSAGE_RENDERTARGET)) {
	//			static float vdata[16] = {
	//				-1.0, -1.0,  0.0,  1.0,
	//				 1.0, -1.0,  1.0,  1.0,
	//				 1.0,  1.0,  1.0,  0.0,
	//				-1.0,  1.0,  0.0,  0.0 };
	//			static IDirect3DVertexBuffer9 *vbuff = NULL;
	//			const size_t memSize = 16*sizeof(float);
	//
	//			// create our vertex buffer on first call
	//			if(!vbuff) {
	//				for(unsigned i=0; i<16; i+=4) {
	//					vdata[i+2] -= 0.000390625f;//((Settings::get().getRenderWidth()-1280.0f)/1280.0f)/Settings::get().getRenderWidth();
	//					vdata[i+3] -= 0.00069444444f;//((Settings::get().getRenderHeight()-720.0f)/720.0f)/Settings::get().getRenderHeight();
	//				}
	//				d3ddev->CreateVertexBuffer(memSize, 0, 0, D3DPOOL_DEFAULT, &vbuff, NULL);
	//				void *bdata;
	//				vbuff->Lock(0, memSize, &bdata, 0);
	//				memcpy(bdata, vdata, memSize);
	//				vbuff->Unlock();
	//			}
	//			SDLOG(3, "~~ pixel offset fix: redirect to own vertex buffer\n");
	//			d3ddev->SetStreamSource(0, vbuff, 0, 16);
	//		} 
	//	}
	//}
	
	return d3ddev->SetTexture(Stage, pTexture);
}

HRESULT RSManager::redirectSetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) {
	//if(lastReplacement >= 0) {
	//	SDLOG(1, "Redirected SetDepthStencilSurface(%p) to %p\n", pNewZStencil, renderTexDSBuffers[lastReplacement]);
	//	d3ddev->SetDepthStencilSurface(renderTexDSBuffers[lastReplacement]);
	//}
	//lastReplacement = -1;
	return d3ddev->SetDepthStencilSurface(pNewZStencil);
}

unsigned RSManager::getTextureIndex(IDirect3DTexture9* ppTexture) {
	TexIntMap::iterator it = texIndices.find(ppTexture);
	if(it != texIndices.end()) return it->second;
	return UINT_MAX;
}

void RSManager::registerD3DXCreateTextureFromFileInMemory(LPCVOID pSrcData, UINT SrcDataSize, LPDIRECT3DTEXTURE9 pTexture) {
	SDLOG(1, "RenderstateManager: registerD3DXCreateTextureFromFileInMemory %p | %p (size %d)\n", pTexture, pSrcData, SrcDataSize);
	if(Settings::get().getEnableTextureDumping()) {
		UINT32 hash = SuperFastHash((char*)const_cast<void*>(pSrcData), SrcDataSize);
		SDLOG(1, " - size: %8u, hash: %8x\n", SrcDataSize, hash);

		IDirect3DSurface9* surf;
		((IDirect3DTexture9*)pTexture)->GetSurfaceLevel(0, &surf);
		char buffer[128];
		sprintf_s(buffer, "dpfix/tex_dump/%08x.tga", hash);
		D3DXSaveSurfaceToFile(GetDirectoryFile(buffer), D3DXIFF_TGA, surf, NULL, NULL);
		surf->Release();
	}
	registerKnowTexture(pSrcData, SrcDataSize, pTexture);
}

void RSManager::registerKnowTexture(LPCVOID pSrcData, UINT SrcDataSize, LPDIRECT3DTEXTURE9 pTexture) {
	if(foundKnownTextures < numKnownTextures) {
		UINT32 hash = SuperFastHash((char*)const_cast<void*>(pSrcData), SrcDataSize);
		#define TEXTURE(_name, _hash) \
		if(hash == _hash) { \
			texture##_name = pTexture; \
			++foundKnownTextures; \
			SDLOG(1, "RenderstateManager: recognized known texture %s at %u\n", #_name, pTexture); \
		}
		#include "Textures.def"
		#undef TEXTURE
		if(foundKnownTextures == numKnownTextures) {
			SDLOG(1, "RenderstateManager: all known textures found!\n");
		}
	}
}

void RSManager::registerD3DXCompileShader(LPCSTR pSrcData, UINT srcDataLen, const D3DXMACRO* pDefines, LPD3DXINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, DWORD Flags, LPD3DXBUFFER * ppShader, LPD3DXBUFFER * ppErrorMsgs, LPD3DXCONSTANTTABLE * ppConstantTable) {
	SDLOG(0, "RenderstateManager: registerD3DXCompileShader %p, fun: %s, profile: %s", *ppShader, pFunctionName, pProfile);
	SDLOG(0, "============= source:\n%s\n====================", pSrcData);
}

IDirect3DTexture9* RSManager::getSurfTexture(IDirect3DSurface9* pSurface) {
	IUnknown *pContainer = NULL;
	HRESULT hr = pSurface->GetContainer(IID_IDirect3DTexture9, (void**)&pContainer);
	if(D3D_OK == hr) return (IDirect3DTexture9*)pContainer;
	SAFERELEASE(pContainer);
	return NULL;
}

void RSManager::enableSingleFrameCapture() {
	captureNextFrame = true;
}

void RSManager::enableTakeScreenshot() {
	takeScreenshot = true; 
	SDLOG(0, "takeScreenshot: %s\n", takeScreenshot?"true":"false");
}
void RSManager::enableTakeHudlessScreenshot() {
	takeHudlessScreenshot = true; 
	SDLOG(0, "takeHudlessScreenshot: %s\n", takeHudlessScreenshot?"true":"false");
}

void RSManager::reloadVssao() {
	SAFEDELETE(ssao); 
	ssao = new SSAO(d3ddev, Settings::get().getRenderWidth(), Settings::get().getRenderHeight(), Settings::get().getSsaoStrength()-1, SSAO::VSSAO);
	SDLOG(0, "Reloaded VSSAO\n");
}
void RSManager::reloadVssao2() {
	SAFEDELETE(ssao); 
	ssao = new SSAO(d3ddev, Settings::get().getRenderWidth(), Settings::get().getRenderHeight(), Settings::get().getSsaoStrength()-1, SSAO::VSSAO2);
	SDLOG(0, "Reloaded VSSAO2\n");
}
void RSManager::reloadHbao() {
	SAFEDELETE(ssao); 
	ssao = new SSAO(d3ddev, Settings::get().getRenderWidth(), Settings::get().getRenderHeight(), Settings::get().getSsaoStrength()-1, SSAO::HBAO);
	SDLOG(0, "Reloaded HBAO\n");
}
void RSManager::reloadScao() {
	SAFEDELETE(ssao); 
	ssao = new SSAO(d3ddev, Settings::get().getRenderWidth(), Settings::get().getRenderHeight(), Settings::get().getSsaoStrength()-1, SSAO::SCAO);
	SDLOG(0, "Reloaded SCAO\n");
}

void RSManager::reloadGauss() {
	SAFEDELETE(gauss); 
	//gauss = new GAUSS(d3ddev, Settings::get().getDOFOverrideResolution()*16/9, Settings::get().getDOFOverrideResolution());
	SDLOG(0, "Reloaded GAUSS\n");
}

void RSManager::reloadAA() {
	SAFEDELETE(smaa); 
	SAFEDELETE(fxaa); 
	if(Settings::get().getAAType() == "SMAA") {
		smaa = new SMAA(d3ddev, Settings::get().getRenderWidth(), Settings::get().getRenderHeight(), (SMAA::Preset)(Settings::get().getAAQuality()-1));
	} else {
		fxaa = new FXAA(d3ddev, Settings::get().getRenderWidth(), Settings::get().getRenderHeight(), (FXAA::Quality)(Settings::get().getAAQuality()-1));
	}
	SDLOG(0, "Reloaded AA\n");
}


HRESULT RSManager::redirectDrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinIndex, UINT NumVertices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
	HRESULT hr = d3ddev->DrawIndexedPrimitiveUP(PrimitiveType, MinIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
	return hr;
}

HRESULT RSManager::redirectDrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
	return d3ddev->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT RSManager::redirectD3DXCreateTextureFromFileInMemoryEx(LPDIRECT3DDEVICE9 pDevice, LPCVOID pSrcData, UINT SrcDataSize, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO* pSrcInfo, PALETTEENTRY* pPalette, LPDIRECT3DTEXTURE9* ppTexture) {
	if(Settings::get().getEnableTextureOverride()) {
		UINT ssize = (SrcDataSize == 2147483647u) ? (Width*Height/2) : SrcDataSize;
		UINT32 hash = SuperFastHash((char*)const_cast<void*>(pSrcData), ssize);
		SDLOG(4, "Trying texture override size: %8u, hash: %8x\n", ssize, hash);
		
		char buffer[128];
		sprintf_s(buffer, "dpfix/tex_override/%08x.dds", hash);
		if(fileExists(buffer)) {
			SDLOG(3, "Texture override (dds)! hash: %8x\n", hash);
			return D3DXCreateTextureFromFileEx(pDevice, buffer, D3DX_DEFAULT, D3DX_DEFAULT, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture);
		}
		sprintf_s(buffer, "dpfix/tex_override/%08x.png", hash);
		if(fileExists(buffer)) {
			SDLOG(3, "Texture override (png)! hash: %8x\n", hash);
			return D3DXCreateTextureFromFileEx(pDevice, buffer, D3DX_DEFAULT, D3DX_DEFAULT, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture);
		}
	}
	return TrueD3DXCreateTextureFromFileInMemoryEx(pDevice, pSrcData, SrcDataSize, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture);
}

void RSManager::storeRenderState() {
	prevStateBlock->Capture();
	prevVDecl = NULL;
	prevDepthStencilSurf = NULL;
	d3ddev->GetVertexDeclaration(&prevVDecl);
	d3ddev->GetDepthStencilSurface(&prevDepthStencilSurf);
	d3ddev->SetDepthStencilSurface(depthStencilSurf);
}

void RSManager::restoreRenderState() {
	if(prevVDecl) {
		d3ddev->SetVertexDeclaration(prevVDecl);
		prevVDecl->Release();
	}
	d3ddev->SetDepthStencilSurface(prevDepthStencilSurf); // also restore NULL!
	if(prevDepthStencilSurf) {
		prevDepthStencilSurf->Release();
	}
	prevStateBlock->Apply();
}

const char* RSManager::getTextureName(IDirect3DBaseTexture9* pTexture) {
	#define TEXTURE(_name, _hash) \
	if(texture##_name == pTexture) return #_name;
	#include "Textures.def"
	#undef TEXTURE
	return "Unknown";
}

HRESULT RSManager::redirectSetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) {
	//if(allowStateChanges()) {
		return d3ddev->SetTextureStageState(Stage, Type, Value);
	//} else {
	//	SDLOG(3, "SetTextureStageState suppressed: %u  -  %u  -  %u\n", Stage, Type, Value);
	//}
	//return D3D_OK;
}

HRESULT RSManager::redirectSetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
	//if(State == D3DRS_COLORWRITEENABLE && !allowStateChanges()) return D3D_OK;
	//if(allowStateChanges()) {
		return d3ddev->SetRenderState(State, Value);
	//} else {
	//	SDLOG(3, "SetRenderState suppressed: %u  -  %u\n", State, Value);
	//}
	//return D3D_OK;
}

HRESULT RSManager::redirectSetVertexShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) {

	// fix pixel size
	if(StartRegister == 0 && Vector4fCount == 1) {
		if(pConstantData[2] == 0.0f && pConstantData[3] == 0.0f) {
			lastVSC0 = true;
			float replacement[4] = { 0.5f/lastVp.Width, 0.5f/lastVp.Height, 0.0f, 0.0f };
			return d3ddev->SetVertexShaderConstantF(StartRegister, replacement, Vector4fCount);
		}
		else lastVSC0 = false;
	}

	return d3ddev->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT RSManager::redirectSetVertexShader(IDirect3DVertexShader9* pvShader) {
	if(onBackbuffer) {
		vsswitch++;
		if(capturing) {
			IDirect3DSurface9 *rt;
			d3ddev->GetRenderTarget(0, &rt);
			char buffer[64];
			sprintf(buffer, "%03d_PV_%03d_curRT_%p_.tga", nrts, vsswitch, rt);
			SDLOG(0, "PV Capturing surface %p as %s\n", rt, buffer);
			D3DXSaveSurfaceToFile(buffer, D3DXIFF_TGA, rt, NULL, NULL);
			SAFERELEASE(rt);
		}
		if(takeHudlessScreenshot && vsswitch >= 3) {
			takeHudlessScreenshot = false;
			captureRTScreen();
		}
	}

	return d3ddev->SetVertexShader(pvShader);
}

HRESULT RSManager::redirectCreateVertexShader(CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader) {
	HRESULT res = d3ddev->CreateVertexShader(pFunction, ppShader);
#ifndef RELEASE_VER
	if(res == D3D_OK) {
		SDLOG(3, "CreateVertexShader data: %p : shader: %p\n", pFunction, *ppShader);
		LPD3DXBUFFER buffer;
		D3DXDisassembleShader(pFunction, false, NULL, &buffer);
		SDLOG(3, "===== disassembly:\n%s\n==============\n", buffer->GetBufferPointer());
		UINT32 hash = SuperFastHash((const char*)buffer->GetBufferPointer(), buffer->GetBufferSize());
		SDLOG(3, "== Vertex Shader hash: %08p : H_shader: %p\n", hash, *ppShader);
		buffer->Release();
	}
#endif // !RELEASE
	return res;
}

HRESULT RSManager::redirectCreatePixelShader(CONST DWORD* pFunction, IDirect3DPixelShader9** ppShader) {
	HRESULT res = d3ddev->CreatePixelShader(pFunction, ppShader);
#ifndef RELEASE_VER
	if(res == D3D_OK) {
		SDLOG(3, "CreatePixelShader data: %p : shader: %p\n", pFunction, *ppShader);
		LPD3DXBUFFER buffer;
		D3DXDisassembleShader(pFunction, false, NULL, &buffer);
		SDLOG(3, "===== disassembly:\n%s\n==============\n", buffer->GetBufferPointer());
		UINT32 hash = SuperFastHash((const char*)buffer->GetBufferPointer(), buffer->GetBufferSize());
		SDLOG(3, "== Pixel Shader hash: %08p : H_shader: %p\n", hash, *ppShader);
		buffer->Release();
	}
#endif // !RELEASE
	return res;
}

HRESULT RSManager::redirectSetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride) {
	
	// fix enemy shadow trails
	if(   !onBackbuffer && (
		  (OffsetInBytes ==  96 && Stride == 24)       // inside
	   || (OffsetInBytes == 192 && Stride == 24) ) ) { // outside
		float vConst[4] = { 640.0f, 360.0f, 640.0f, 360.0f };
		d3ddev->SetVertexShaderConstantF(254, vConst, 1);
		SDLOG(4, "~~~ Shadow trail fix activated!\n");
	}

	// fix dual screen rendering
	if(lastT1024 && firstStreamSource & onBackbuffer && (OffsetInBytes == 192 && Stride == 24)) {
		IDirect3DBaseTexture9 *t = NULL;
		IDirect3DTexture9 *tt = NULL;
		d3ddev->GetTexture(0, &t);
		if(t && t->QueryInterface(IID_IDirect3DTexture9, (void**)&tt) == S_OK) {
			D3DSURFACE_DESC desc;
			tt->GetLevelDesc(0, &desc);
			if(desc.Width == 1024 && desc.Height == 1024 && ((desc.Usage & D3DUSAGE_RENDERTARGET) == 0)) {
				float replacement[4] = { 640.0f, 360.0f, 640.0f, 360.0f };
				SDLOG(0, "DualScreenEffect SetVertexShaderConstantF OVERRIDE\n");
				d3ddev->SetVertexShaderConstantF(254, replacement, 1);
			}
		}
		SAFERELEASE(t);
	}

	// fix pixel offset
	// not necessary, breaks stuff
	//SetStreamSource 0: 1B5EE4C0 (0, 16)
	//~      -1.00000000      -1.00000000       0.00000000       1.00000000
	//~       1.00000000      -1.00000000       1.00000000       1.00000000
	//~       1.00000000       1.00000000       1.00000000       0.00000000
	//~      -1.00000000       1.00000000       0.00000000       0.00000000
	//if(StreamNumber == 0 && OffsetInBytes == 0 && Stride == 16
	//	/*	&& lastVp.Width == Settings::get().getRenderWidth() && lastVp.Height == Settings::get().getRenderHeight()*/) {
	//	D3DVERTEXBUFFER_DESC desc;
	//	pStreamData->GetDesc(&desc);
	//	float *data;
	//	pStreamData->Lock(0, desc.Size, (void**)&data, 0);
	//	for(unsigned i=0; i<desc.Size/4; i+=4) {
	//		SDLOG(11, "~ %16.8f %16.8f %16.8f %16.8f\n", data[i+0], data[i+1], data[i+2], data[i+3]);
	//	}
	//	if(FLT_EQ(data[3], 1.0f)) {
	//		SDLOG(3, "~~~ Changing stream source data!\n");
	//		for(unsigned i=0; i<desc.Size/4; i+=4) {
	//			data[i+2] -= ((lastVp.Width-1280.0f)/1280.0f)/1280.0f;
	//			data[i+3] -= ((lastVp.Height-720.0f)/720.0f)/720.0f;
	//		}
	//	}
	//	pStreamData->Unlock();
	//}

	firstStreamSource = false;
	return d3ddev->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
}

HRESULT RSManager::redirectDrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) {
	//static unsigned ii = 0;
	//if(nrts == 12 && capturing) {
	//	IDirect3DSurface9 *oldRenderTarget;
	//	d3ddev->GetRenderTarget(0, &oldRenderTarget);
	//	char buffer[64];
	//	sprintf(buffer, "%03d_dp%03d_oldRenderTarget_%p_.tga", nrts, ii++, oldRenderTarget);
	//	SDLOG(0, "Capturing surface %p as %s\n", oldRenderTarget, buffer);
	//	D3DXSaveSurfaceToFile(buffer, D3DXIFF_TGA, oldRenderTarget, NULL, NULL);
	//	SAFERELEASE(oldRenderTarget);
	//}
	return d3ddev->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
}

HRESULT RSManager::redirectDrawIndexedPrimitive(D3DPRIMITIVETYPE Type, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) {
	//static unsigned ii = 0;
	//if(nrts == 12 && capturing) {
	//	IDirect3DSurface9 *oldRenderTarget;
	//	d3ddev->GetRenderTarget(0, &oldRenderTarget); 
	//	char buffer[64];
	//	sprintf(buffer, "%03d_dip%03d_oldRenderTarget_%p_.tga", nrts, ii++, oldRenderTarget);
	//	SDLOG(0, "Capturing surface %p as %s\n", oldRenderTarget, buffer);
	//	D3DXSaveSurfaceToFile(buffer, D3DXIFF_TGA, oldRenderTarget, NULL, NULL);
	//	SAFERELEASE(oldRenderTarget);
	//}
	return d3ddev->DrawIndexedPrimitive(Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
}

HRESULT RSManager::redirectSetPixelShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) {

	// adjust pixel size
	if(StartRegister == 17 && Vector4fCount == 1) {
		if(FLT_EQ(pConstantData[0], 0.5f/1280) && FLT_EQ(pConstantData[1], 0.5f/720) && FLT_EQ(pConstantData[2], 0.0f)) {
			SDLOG(3, "~~ Setting PS pixel size\n");
			float replacement[4] = { 0.5f / Settings::get().getRenderWidth(), 0.5f / Settings::get().getRenderHeight(), 0.0f, 0.0f };
			return d3ddev->SetPixelShaderConstantF(StartRegister, replacement, Vector4fCount);
		}
	}
	if(StartRegister == 4 && Vector4fCount == 5) {
		if(FLT_EQ(pConstantData[4], 0.0007812500f) && FLT_EQ(pConstantData[17], 0.0013888889f)) {
			SDLOG(3, "~~ Setting PS2 pixel size\n");
			float replacement[4*5];
			memcpy(replacement, pConstantData, sizeof(float)*4*5);
			replacement[4] = 1.0f/Settings::get().getRenderWidth();
			replacement[8] = 2.0f/Settings::get().getRenderWidth();
			replacement[17] = 1.0f/Settings::get().getRenderHeight();
			return d3ddev->SetPixelShaderConstantF(StartRegister, replacement, Vector4fCount);
		}
	}

	// adjust shadow map resolution shadow rendering pixel shaders
	if(StartRegister == 0 && Vector4fCount == 1) {
		if(FLT_EQ(pConstantData[1], 0.0f) && FLT_EQ(pConstantData[2], 0.0f) && FLT_EQ(pConstantData[3], 0.0f) 
			 && (FLT_EQ(pConstantData[0], 1024.0f) || FLT_EQ(pConstantData[0], 512.0f) ) ) {
			float replacement[4] = { pConstantData[0] * Settings::get().getShadowMapScale(), 0.0f, 0.0f, 0.0f };
			return d3ddev->SetPixelShaderConstantF(StartRegister, replacement, Vector4fCount);
		}
	}

	// adjust shadow cascade distances
	//if(StartRegister == 2 && Vector4fCount == 1) {
		// if(pConstantData[0] == 50.0f && pConstantData[1] == 200.0f && pConstantData[2] == 1000.0f) {
		//	float replacement[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		//	return d3ddev->SetPixelShaderConstantF(StartRegister, replacement, Vector4fCount);
		// }
	//}
	
	return d3ddev->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT RSManager::redirectSetViewport(D3DVIEWPORT9* vp) {
	memcpy(&lastVp, vp, sizeof(D3DVIEWPORT9));

	// fix pixel size
	if(lastVSC0) {
		SDLOG(3, "~~ Setting VS pixel size\n");
		float replacement[4] = { 0.5f / vp->Width, 0.5f / vp->Height, 0.0f, 0.0f};
		d3ddev->SetVertexShaderConstantF(0, replacement, 1);
	}

	return d3ddev->SetViewport(vp);
}

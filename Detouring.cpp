#include "Detouring.h"

#include <Windows.h>

#include "Settings.h"
#include "RenderstateManager.h"
#include "d3dutil.h"

bool timingIntroMode = false;

DWORD WINAPI DetouredSleepEx(DWORD dwMilliseconds, BOOL bAlertable) {
	//SDLOG(12, "T %6lu: Detouring: Sleep for %lu ms\n", GetCurrentThreadId(), dwMilliseconds);
	//return TrueSleepEx(dwMilliseconds, bAlertable);
	return 0;
}

#include <mmsystem.h>

static DWORD timeIncrease;
DWORD (WINAPI * TrueTimeGetTime)(void) = timeGetTime;
DWORD WINAPI DetouredTimeGetTime() {
	SDLOG(13, "T %6lu: Detouring: TimeGetTime - real: %10lu, returned: %10lu\n", GetCurrentThreadId(), TrueTimeGetTime(), TrueTimeGetTime() + timeIncrease);
	//timeIncrease += 16;
	return TrueTimeGetTime() + timeIncrease;
}

static LARGE_INTEGER perfCountIncrease, countsPerSec;
BOOL (WINAPI * TrueQueryPerformanceCounter)(_Out_ LARGE_INTEGER *lpPerformanceCount) = QueryPerformanceCounter;
BOOL WINAPI DetouredQueryPerformanceCounter(_Out_ LARGE_INTEGER *lpPerformanceCount) {
	void *traces[128];
	DWORD hash;
	int captured = CaptureStackBackTrace(0, 128, traces, &hash);
	SDLOG(14, "T %6lu: Detouring: QueryPerformanceCounter, stack depth %3d, hash %20ul\n", GetCurrentThreadId(), captured, hash);
	BOOL ret = TrueQueryPerformanceCounter(lpPerformanceCount);
	if(timingIntroMode && captured == 1) {
		perfCountIncrease.QuadPart += countsPerSec.QuadPart/50;
	}
	lpPerformanceCount->QuadPart += perfCountIncrease.QuadPart;
	return ret;
}

typedef HRESULT (WINAPI * D3DXCreateTexture_FNType)(_In_ LPDIRECT3DDEVICE9 pDevice, _In_ UINT Width, _In_ UINT Height, _In_ UINT MipLevels, _In_ DWORD Usage, _In_ D3DFORMAT Format, _In_ D3DPOOL Pool, _Out_ LPDIRECT3DTEXTURE9 *ppTexture);
D3DXCreateTexture_FNType TrueD3DXCreateTexture = D3DXCreateTexture;
HRESULT WINAPI DetouredD3DXCreateTexture(_In_ LPDIRECT3DDEVICE9 pDevice, _In_ UINT Width, _In_ UINT Height, _In_ UINT MipLevels, _In_ DWORD Usage, _In_ D3DFORMAT Format, _In_ D3DPOOL Pool, _Out_ LPDIRECT3DTEXTURE9 *ppTexture) {
	SDLOG(4, "DetouredD3DXCreateTexture\n");
	HRESULT res = TrueD3DXCreateTexture(pDevice, Width, Height, MipLevels, Usage, Format, Pool, ppTexture);
	return res;
}

typedef HRESULT (WINAPI * D3DXCreateTextureFromFileInMemory_FNType)(_In_ LPDIRECT3DDEVICE9 pDevice, _In_ LPCVOID pSrcData, _In_ UINT SrcDataSize, _Out_ LPDIRECT3DTEXTURE9 *ppTexture);
D3DXCreateTextureFromFileInMemory_FNType TrueD3DXCreateTextureFromFileInMemory = D3DXCreateTextureFromFileInMemory;
HRESULT WINAPI DetouredD3DXCreateTextureFromFileInMemory(_In_ LPDIRECT3DDEVICE9 pDevice, _In_ LPCVOID pSrcData, _In_ UINT SrcDataSize, _Out_ LPDIRECT3DTEXTURE9 *ppTexture) {
	SDLOG(4, "DetouredD3DXCreateTextureFromFileInMemory\n");
	HRESULT res = TrueD3DXCreateTextureFromFileInMemory(pDevice, pSrcData, SrcDataSize, ppTexture);
	RSManager::get().registerD3DXCreateTextureFromFileInMemory(pSrcData, (SrcDataSize == 2147483647u) ? 256 : SrcDataSize, *ppTexture);
	return res;
}

typedef HRESULT (WINAPI * D3DXCreateTextureFromFileInMemoryEx_FNType)(LPDIRECT3DDEVICE9 pDevice, LPCVOID pSrcData, UINT SrcDataSize, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO *pSrcInfo, PALETTEENTRY *pPalette, LPDIRECT3DTEXTURE9 *ppTexture);
D3DXCreateTextureFromFileInMemoryEx_FNType TrueD3DXCreateTextureFromFileInMemoryEx = D3DXCreateTextureFromFileInMemoryEx;
HRESULT WINAPI DetouredD3DXCreateTextureFromFileInMemoryEx(LPDIRECT3DDEVICE9 pDevice, LPCVOID pSrcData, UINT SrcDataSize, UINT Width, UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, DWORD Filter, 
														   DWORD MipFilter, D3DCOLOR ColorKey, D3DXIMAGE_INFO *pSrcInfo, PALETTEENTRY *pPalette, LPDIRECT3DTEXTURE9 *ppTexture) {
	SDLOG(4, "DetouredD3DXCreateTextureFromFileInMemoryEx: \n -- pDevice %p, pSrcData %p, SrcDataSize %u, Width %u, Height %u, MipLevels %u, Usage %d, Format %s\n",
		pDevice, pSrcData, SrcDataSize, Width, Height, MipLevels, Usage, D3DFormatToString(Format));
	HRESULT res = RSManager::get().redirectD3DXCreateTextureFromFileInMemoryEx(pDevice, pSrcData, SrcDataSize, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture);
	if(SrcDataSize == 2147483647u) SrcDataSize = Width*Height/2;
	RSManager::get().registerD3DXCreateTextureFromFileInMemory(pSrcData, SrcDataSize, *ppTexture); 
	return res;
}

typedef HRESULT (WINAPI * D3DXCompileShader_FNType)(_In_ LPCSTR pSrcData, _In_ UINT srcDataLen, _In_ const D3DXMACRO *pDefines, _In_ LPD3DXINCLUDE pInclude, _In_ LPCSTR pFunctionName, _In_ LPCSTR pProfile, _In_ DWORD Flags, _Out_ LPD3DXBUFFER *ppShader, _Out_ LPD3DXBUFFER *ppErrorMsgs, _Out_ LPD3DXCONSTANTTABLE *ppConstantTable);
D3DXCompileShader_FNType TrueD3DXCompileShader = D3DXCompileShader;
HRESULT WINAPI DetouredD3DXCompileShader(_In_ LPCSTR pSrcData, _In_ UINT srcDataLen, _In_ const D3DXMACRO *pDefines, _In_ LPD3DXINCLUDE pInclude, _In_ LPCSTR pFunctionName, _In_ LPCSTR pProfile, 
										 _In_ DWORD Flags, _Out_ LPD3DXBUFFER *ppShader, _Out_ LPD3DXBUFFER *ppErrorMsgs, _Out_ LPD3DXCONSTANTTABLE *ppConstantTable) {
	HRESULT res = TrueD3DXCompileShader(pSrcData, srcDataLen, pDefines, pInclude, pFunctionName, pProfile, Flags, ppShader, ppErrorMsgs, ppConstantTable);
	RSManager::get().registerD3DXCompileShader(pSrcData, srcDataLen, pDefines, pInclude, pFunctionName, pProfile, Flags, ppShader, ppErrorMsgs, ppConstantTable);
	return res;
}

void earlyDetour() {
	QueryPerformanceFrequency(&countsPerSec);
	//DetourTransactionBegin();
	//DetourUpdateThread(GetCurrentThread());
	//DetourAttach(&(PVOID&)oDirect3DCreate9, hkDirect3DCreate9);
	//DetourTransactionCommit();
}

void startDetour() {		
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	//DetourAttach(&(PVOID&)TrueSleepEx, DetouredSleepEx);
	//DetourAttach(&(PVOID&)TrueTimeGetTime, DetouredTimeGetTime);
	//DetourAttach(&(PVOID&)TrueQueryPerformanceCounter, DetouredQueryPerformanceCounter);
	//TrueD3DXCreateTexture = (D3DXCreateTexture_FNType)DetourFindFunction("d3dx9_43.dll", "D3DXCreateTexture");
	TrueD3DXCreateTextureFromFileInMemory = (D3DXCreateTextureFromFileInMemory_FNType)DetourFindFunction("d3dx9_43.dll", "D3DXCreateTextureFromFileInMemory");
	TrueD3DXCreateTextureFromFileInMemoryEx = (D3DXCreateTextureFromFileInMemoryEx_FNType)DetourFindFunction("d3dx9_43.dll", "D3DXCreateTextureFromFileInMemoryEx");
	//DetourAttach(&(PVOID&)TrueD3DXCreateTexture, DetouredD3DXCreateTexture);
	DetourAttach(&(PVOID&)TrueD3DXCreateTextureFromFileInMemory, DetouredD3DXCreateTextureFromFileInMemory);
	DetourAttach(&(PVOID&)TrueD3DXCreateTextureFromFileInMemoryEx, DetouredD3DXCreateTextureFromFileInMemoryEx);
	//TrueD3DXCompileShader = (D3DXCompileShader_FNType)DetourFindFunction("d3dx9_43.dll", "D3DXCompileShader");
	//SDLOG(0, "Detouring: compile shader: %p\n", TrueD3DXCompileShader);
	//DetourAttach(&(PVOID&)TrueD3DXCompileShader, DetouredD3DXCompileShader);

	if(DetourTransactionCommit() == NO_ERROR) {
		SDLOG(0, "Detouring: Detoured successfully\n");
	} else {
		SDLOG(0, "Detouring: Error detouring\n");
	}
}

void endDetour() {
	//if(Settings::get().getSkipIntro()) {
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		//DetourDetach(&(PVOID&)TrueSleepEx, DetouredSleepEx);
		//DetourDetach(&(PVOID&)TrueTimeGetTime, DetouredTimeGetTime);
		//DetourDetach(&(PVOID&)TrueQueryPerformanceCounter, DetouredQueryPerformanceCounter);
		//DetourDetach(&(PVOID&)TrueD3DXCreateTexture, DetouredD3DXCreateTexture);
		DetourDetach(&(PVOID&)TrueD3DXCreateTextureFromFileInMemory, DetouredD3DXCreateTextureFromFileInMemory);
		DetourDetach(&(PVOID&)TrueD3DXCreateTextureFromFileInMemoryEx, DetouredD3DXCreateTextureFromFileInMemoryEx);
		//DetourDetach(&(PVOID&)oDirect3DCreate9, hkDirect3DCreate9);
		//DetourDetach(&(PVOID&)TrueD3DXCompileShader, DetouredD3DXCompileShader);
		DetourTransactionCommit();
	//}
}

// Window stuff

typedef BOOL (WINAPI * AdjustWindowRect_FNType)(__inout LPRECT lpRect, __in DWORD dwStyle, __in BOOL bMenu);
AdjustWindowRect_FNType TrueAdjustWindowRect = AdjustWindowRect;
BOOL WINAPI DetouredAdjustWindowRect(__inout LPRECT lpRect, __in DWORD dwStyle, __in BOOL bMenu) {
	return true;
}
typedef BOOL (WINAPI * AdjustWindowRectEx_FNType)(_Inout_ LPRECT lpRect, _In_ DWORD dwStyle, _In_ BOOL bMenu, _In_ DWORD dwExStyle);
AdjustWindowRectEx_FNType TrueAdjustWindowRectEx = AdjustWindowRectEx;
BOOL WINAPI DetouredAdjustWindowRectEx(_Inout_ LPRECT lpRect, _In_ DWORD dwStyle, _In_ BOOL bMenu, _In_ DWORD dwExStyle) {
	return true;
}

SetWindowPos_FNType TrueSetWindowPos = SetWindowPos;
BOOL WINAPI DetouredSetWindowPos(_In_ HWND hWnd, _In_opt_ HWND hWndInsertAfter, _In_ int X, _In_ int Y, _In_ int cx, _In_ int cy, _In_ UINT uFlags) {
	return true;
}

void startWindowDetour() {
	static bool detoured = false;
	if(!detoured) {
		detoured = true;
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		TrueAdjustWindowRect = (AdjustWindowRect_FNType)DetourFindFunction("User32.dll", "AdjustWindowRect");
		DetourAttach(&(PVOID&)TrueAdjustWindowRect, DetouredAdjustWindowRect);
		TrueAdjustWindowRectEx = (AdjustWindowRectEx_FNType)DetourFindFunction("User32.dll", "AdjustWindowRectEx");
		DetourAttach(&(PVOID&)TrueAdjustWindowRectEx, DetouredAdjustWindowRectEx);
		TrueSetWindowPos = (SetWindowPos_FNType)DetourFindFunction("User32.dll", "SetWindowPos");
		DetourAttach(&(PVOID&)TrueSetWindowPos, DetouredSetWindowPos);
	
		if(DetourTransactionCommit() == NO_ERROR) {
			SDLOG(0, "Window Detouring: Detoured successfully\n");
		} else {
			SDLOG(0, "Window Detouring: Error detouring\n");
		}
	}
}

void endWindowDetour() {
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)TrueAdjustWindowRect, DetouredAdjustWindowRect);
	DetourDetach(&(PVOID&)TrueAdjustWindowRectEx, DetouredAdjustWindowRectEx);
	DetourDetach(&(PVOID&)TrueSetWindowPos, DetouredSetWindowPos);
	DetourTransactionCommit();
}

// Joystick stuff

typedef MMRESULT (WINAPI * joyGetPosEx_FNType)(UINT uJoyID, LPJOYINFOEX pji);
joyGetPosEx_FNType TruejoyGetPosEx = joyGetPosEx;
MMRESULT DetouredjoyGetPosEx(UINT uJoyID, LPJOYINFOEX pji) {
	return 0;
}

void startJoyDetour() {
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	TruejoyGetPosEx = (joyGetPosEx_FNType)DetourFindFunction("Winmm.dll", "joyGetPosEx");
	DetourAttach(&(PVOID&)TruejoyGetPosEx, DetouredjoyGetPosEx);
	
	if(DetourTransactionCommit() == NO_ERROR) {
		SDLOG(0, "Joystick Detouring: Detoured successfully\n");
	} else {
		SDLOG(0, "Joystick Detouring: Error detouring\n");
	}
}

void endJoyDetour() {
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)TruejoyGetPosEx, DetouredjoyGetPosEx);
	DetourTransactionCommit();
}
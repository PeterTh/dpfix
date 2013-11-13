#include <windows.h>

#include "d3d9.h"
#include "main.h"
#include "Settings.h"

IDirect3D9 *APIENTRY Direct3DCreate9(UINT SDKVersion) {
	SDLOG(0, "=== Direct3DCreate9\n");
	IDirect3D9 *d3dint = NULL;
	//if(Settings::get().getEnableTripleBuffering()) Direct3DCreate9Ex(SDKVersion, (IDirect3D9Ex**)&d3dint);
	//else d3dint = oDirect3DCreate9(SDKVersion);
	d3dint = oDirect3DCreate9(SDKVersion);

	if(d3dint != NULL) {
		hkIDirect3D9 *ret = new hkIDirect3D9(&d3dint);
	}
	return d3dint;
}

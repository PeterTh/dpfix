// DPfix Copyright 2013 Peter Thoman (Durante)
// based heavily on DSfix

//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>

#pragma once

#include <cmath>
#include <limits>

#define VERSION "0.9"

#define INTERCEPTOR_NAME "DPfix"
#define SETTINGS_FILE_NAME (INTERCEPTOR_NAME".ini")
#define KEY_FILE_NAME (INTERCEPTOR_NAME"Keys.ini")
#define LOG_FILE_NAME (INTERCEPTOR_NAME".log")

#define RELEASE_VER

#ifndef RELEASE_VER
#define SDLOG(_level, _str, ...) if(Settings::get().getLogLevel() > _level) { sdlog(_str, __VA_ARGS__); }
#else
#define SDLOG(_level, _str, ...) {}
#endif
#define SAFERELEASE(_p) { if(_p) { (_p)->Release(); (_p) = NULL; } }
#define SAFEDELETE(_p) { if(_p) { delete (_p); (_p) = NULL; } }

#define EPSILON (std::numeric_limits<float>::epsilon()*10)
#define FLT_EQ(__a, __b) (std::abs((__a) - (__b)) <= EPSILON * (std::max)(1.0f, (std::max)(std::abs(__a), std::abs(__b))))

#include "d3d9.h"

char *GetDirectoryFile(char *filename);
bool fileExists(const char *filename);
void __cdecl sdlogtime();
void __cdecl sdlog(const char * fmt, ...);
void errorExit(LPTSTR lpszFunction);

typedef IDirect3D9 *(APIENTRY *tDirect3DCreate9)(UINT);
extern tDirect3DCreate9 oDirect3DCreate9;

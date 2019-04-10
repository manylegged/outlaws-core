//
//  PlatformIncludes.h
//  Outlaws
//
//  Created by Arthur Danskin on 10/21/12.
//  Copyright (c) 2012 Tiny Chocolate Games. All rights reserved.
//

#ifndef Outlaws_PlatformIncludes_h
#define Outlaws_PlatformIncludes_h

#ifdef _DEBUG
#define DEBUG 1
#endif

// disable slow stl
#ifndef DEBUG
#define _SECURE_SCL 0
#define _HAS_ITERATOR_DEBUGGING 0
#endif

#define _CRT_SECURE_NO_WARNINGS 1

#ifndef NOMINMAX
#define NOMINMAX
#endif

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define _WIN32_WINNT 0x501              // xp compatibility
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// OpenGL headers
#include "../win32/include/GL/glew.h"
#include "SDL.h"
#include "SDL_opengl.h"

#define HAS_SOUND 1

#pragma warning(disable: 4996) // depricated
#pragma warning(disable: 4018) // signed/unsigned mismatch
#pragma warning(disable: 4244) // possible loss of data
#pragma warning(disable: 4305) // truncation from double to float
#pragma warning(disable: 4800) // forcing value to bool true or false
#pragma warning(disable: 4351) 
#pragma warning(disable: 4091) // 'typedef ': ignored on left of '' when no variable is declared

#define __func__ __FUNCTION__

#ifdef MOUSE_MOVED
#undef MOUSE_MOVED
#endif

typedef unsigned int uint;
typedef unsigned short ushort;

#define __builtin_trap __debugbreak
#define __printflike(X, Y)
#define __builtin_expect(X, Y) (X)

#ifndef __has_feature
#define __has_feature(X) (0)
#endif

#if defined(NULL)
#undef NULL
#define NULL nullptr
#endif

#ifdef MOD_ALT
#undef MOD_ALT
#endif

#ifdef MOD_CONTROL
#undef MOD_CONTROL
#endif

#ifdef MOD_SHIFT
#undef MOD_SHIFT
#endif

#ifdef ABSOLUTE
#undef ABSOLUTE
#endif

#ifdef RELATIVE
#undef RELATIVE
#endif

std::wstring s2ws(const std::string& s);
std::string ws2s(const std::wstring& wstr);
void ReportWin32Err1(const char *msg, DWORD dwLastError, const char* file, int line);

#endif

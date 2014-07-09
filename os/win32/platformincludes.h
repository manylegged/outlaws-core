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

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// OpenGL headers
#include "../../glew-1.9.0/include/GL/glew.h"
#include "SDL.h"
#include "SDL_opengl.h"

#define HAS_SOUND 1

#pragma warning(disable: 4996) // depricated
#pragma warning(disable: 4018) // signed/unsigned mismatch
#pragma warning(disable: 4244) // possible loss of data
#pragma warning(disable: 4305) // truncation from double to float
#pragma warning(disable: 4800) // forcing value to bool true or false

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

#endif

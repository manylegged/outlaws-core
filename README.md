outlaws-core
============

General purpose c++11 code from my game Reassembly: http://www.anisopteragames.com

This repository contains all of the general purpose code from the game - input handling, platform
layers, GUI/text systems, graphics abstractions, etc.

Highlights:
* polygon intersections, interpolation, vector helpers, ternary digits, and more in Geometry.h
* fast 2d spatial hash in SpacialHash.h
* string/utf8 utilities in Str.h and Unicode.h
* 'copy_ptr' and 'watch_ptr' smart pointers for managing sparse structs and automatically nulling object references on deletion, respectively, in stl_ext.h
* C++ wrappers for OpenGL buffers and polygon drawing code in Graphics.h
* Fast shader powered particle system in Particles.h
* Color transformation utilities in RGB.h
* mac/win/linux platform layer in Outlaws.h (implementations in OS directory)
* zlib wrapper in ZipFile.h

supported compilers
-------------------
* Xcode 6.3 (clang)
* Visual Studio 2013
* GCC 4.8

libraries
--------------
* OpenGL: http://www.opengl.org
* OpenGL Extension Wrangler Library (GLEW): http://glew.sourceforge.net
* OpenGL Mathematics Library: http://glm.g-truc.net
* Zlib: http://www.zlib.net/
* Simple DirectMedia Layer: http://www.libsdl.org
* cAudio 3D audio engine based on OpenAL (with my modifications): https://github.com/manylegged/cAudio
* Steamworks: https://partner.steamgames.com

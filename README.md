outlaws-core
============

General purpose c++11 code from my upcoming game Reassembly: http://www.anisopteragames.com

Not cleaned up enough to be #included directly by other projects without changes, but contains some useful snippets.

Highlights:
* polygon intersections, interpolation, vector helpers, ternary digits, and more in Geometry.h
* fast 2d spatial hash in SpacialHash.h
* a class 'lstring' for fast symbol manipulation and easy to use string utils in Str.h
* 'copy_ptr' and 'watch_ptr' smart pointers for managing sparse structs and automatically nulling object references on deletion, respectively, in stl_ext.h
* C++ wrappers for OpenGL buffers and polygon drawing code in Graphics.h
* Fast shader powered particle system in Particles.h
* Color transformation utilities in RGB.h
* mac/win/linux platform layer in Outlaws.h (implementations in OS directory)

supported compilers
-------------------
* Xcode 6.1 (clang)
* Visual Studio 2013
* GCC 4.8

libraries
--------------
* OpenGL: http://www.opengl.org
* OpenGL Mathematics Library: http://glm.g-truc.net
* Simple DirectMedia Layer: http://www.libsdl.org
* OpenGL Extension Wrangler Library (GLEW): http://glew.sourceforge.net
* cAudio 3D audio engine based on OpenAL (with my modifications): https://github.com/manylegged/cAudio

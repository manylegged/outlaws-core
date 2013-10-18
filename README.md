outlaws-core
============

General purpose c++11 code from my game.

Not intended to be #included directly by other projects, but contains some useful snippets.

Highlights:
* polygon intersection functions in Geometry.h
* fast 2d spacial hash in SpacialHash.h
* a class 'lstring' for fast symbol manipulation in Str.h
* 'copy_ptr' and 'watch_ptr' smart pointers for managing sparse structs and automatically nulling object references on deletion, respectively, in stl_ext.h
* C++ wrappers for OpenGL index buffers and vertex buffers and polygon drawing code in Graphics.h


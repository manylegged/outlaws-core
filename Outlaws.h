// -*- mode: c -*-
//  Outlaws.h
//  Outlaws
//
//  Created by Arthur Danskin on 10/21/12.
//  Copyright (c) 2012-2015 Arthur Danskin. All rights reserved.
//
// This file defines the interface between the platform independant game code and the platform
// specific parts.
// Game functions are prefixed OLG_ for Outlaws Game
// OS functions are prefixed OL_
//
// const char* is always UTF-8


#ifndef __Outlaws__Outlaws__
#define __Outlaws__Outlaws__

#ifdef __cplusplus
extern "C"
{
#endif

#if !defined(CLANG_ANALYZER_NORETURN)
#if __has_feature(attribute_analyzer_noreturn)
#define CLANG_ANALYZER_NORETURN __attribute__((analyzer_noreturn))
#else
#define CLANG_ANALYZER_NORETURN
#endif
#endif

#ifndef NOINLINE
#if _MSC_VER
#define NOINLINE __declspec(noinline)
#elif __clang__ || __GNUC__
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif
#endif
////////////////////////////// OS layer calls into Game ///////////////////////////////

// main game function - called once per frame
void OLG_Draw(void);
    
enum OLModKeys {
    OShiftKey = 0xF610,
    OControlKey = 0xF611,
    OAltKey = 0xF612,
};

struct OLEvent {
    enum EventType { KEY_DOWN=0, KEY_UP, MOUSE_DOWN, MOUSE_UP, MOUSE_DRAGGED, 
                     MOUSE_MOVED, SCROLL_WHEEL, LOST_FOCUS, GAINED_FOCUS,
                     TOUCH_BEGIN, TOUCH_MOVED, TOUCH_STATIONARY, TOUCH_ENDED, TOUCH_CANCELLED,
                     GAMEPAD_AXIS, GAMEPAD_ADDED, GAMEPAD_REMOVED,
                     INVALID };
    enum EventType type;
    long key;
    float x, y;
    float dx, dy;
};

// handle an input event
void OLG_OnEvent(const struct OLEvent* event);

// called before program terminates
void OLG_OnQuit(void);

// called when the application window is closed - like OnQuit but more gracefull
void OLG_OnClose(void);

// init, process args. Return 1 if create window and interactive, 0 if headless mode
int OLG_Init(int argc, const char** argv);

// init opengl, return NULL if supported or error message if not
const char* OLG_InitGL(void);

// called when window manager changes full screen state
void OLG_SetFullscreenPref(int enabled);

// handle assertions. return 1
NOINLINE int OLG_OnAssertFailed(const char* file, int line, const char* func,
                                const char* x, const char* format, ...)
    __printflike(5, 6) CLANG_ANALYZER_NORETURN;

NOINLINE int OLG_vOnAssertFailed(const char* file, int line, const char* func,
                                 const char* x, const char* format, va_list v)
    __printflike(5, 0) CLANG_ANALYZER_NORETURN;

// return target frame rate. e.g. 60 fps
float OLG_GetTargetFPS(void);

// get name of game (for save path)
const char* OLG_GetName(void);

// true if we should load / save data from the game directory instead of system save path
int OLG_UseDevSavePath(void);

// return name of log file to open
const char* OLG_GetLogFileName(void);

// upload logfile to server
int OLG_UploadLog(const char* logdata, int loglen);

 //////////////////////////////// Game calls into OS layer //////////////////////////////////

// call around code inside the main loop of helper threads
// these allocate and drain autorelease pools on Apple platforms
void OL_ThreadBeginIteration(int i);
void OL_ThreadEndIteration(int i);

// return number of cpu cores
int OL_GetCpuCount(void);

// print a debugging message message
void OL_ReportMessage(const char *str);

// time since start of game in seconds
double OL_GetCurrentTime(void);

// get logged in username
const char* OL_GetUserName(void);

// return string describing runtime platform and current time, for log
const char* OL_GetPlatformDateInfo(void);

// open default web browser to selected url
int OL_OpenWebBrowser(const char* url);

// quit gracefully
void OL_DoQuit(void);

// request that log be uploaded when game is shutdown
void OL_ScheduleUploadLog(void);

// read string from clipboard (may return null)
const char* OL_ReadClipboard(void);

// move cursor
void OL_WarpCursorPosition(float x, float y);
    
////////// Graphics

// swap the OpenGL buffers and display the frame
void OL_Present(void);

// get window size is pixels and points (for retina displays)
void OL_GetWindowSize(float *pixelWidth, float *pixelHeight, float *pointWidth, float *pointHeight);

// get scale factor of windowing system
float OL_GetBackingScaleFactor(void);

// get scale factor of game window (may be different than GetBackingScaleFactor if game window is
// on a non-retina display attached to a retina laptop, for example)
float OL_GetCurrentBackingScaleFactor(void);

// toggle fullscreeen mode
void OL_SetFullscreen(int fullscreen);
int OL_GetFullscreen(void);

void OL_SetWindowSizePoints(int w, int h);

// change swap interval (0 is immediate flip, 1 is vsync 60fps, 2 is vsync 30fps...)
void OL_SetSwapInterval(int interval);

// return true if driver supports tear control, aka adaptive vsync
int OL_HasTearControl(void);

typedef struct OutlawImage {
    int width, height;
    char *data;                 /* release with free() */
} OutlawImage;

// load an image
OutlawImage OL_LoadImage(const char*fname);

typedef struct OutlawTexture {
    int width, height;
    int texwidth, texheight;
    unsigned texnum;
} OutlawTexture;

// load a texture from file into OpenGL
OutlawTexture OL_LoadTexture(const char* fname);

// save a texture to file
int OL_SaveTexture(const OutlawTexture *tex, const char* fname);

struct OLSize {
    float x, y;
};
// load a font file, may be referred to later using INDEX
void OL_SetFont(int index, const char* file, const char* name);

// render a string into an OpenGL texture, using a previously loaded font
int OL_StringTexture(OutlawTexture *tex, const char* string, float size, int font, float maxw, float maxh);

// get a table of character sizes for font
void OL_FontAdvancements(int font, float size, struct OLSize* advancements); // advancements must be at least size 127

// get height from one line to the next
float OL_FontHeight(int fontName, float size);

// print stacktrace to log, upload log, quit program, etc
void OL_OnTerminate(const char* message);

/////////// File IO
// All functions take paths relative to main game directory

// load text file into memory. pointer does not need to be freed, but is reused across calls
const char *OL_LoadFile(const char *fname);

// write text file to disk, atomically. Creates directories as needed.
int OL_SaveFile(const char* fname, const char* data, int size);

int OL_CopyFile(const char* source, const char *dest);

// Return list of files in a directory (base name only - no path)
const char** OL_ListDirectory(const char* path);

// get complete path for data file in utf8, searching through save directory and application resource directory
// mode should be "w" or "r"
const char *OL_PathForFile(const char *fname, const char *mode);

// recursively delete a file or directory
int OL_RemoveFileOrDirectory(const char* dirname);

// return true if path is a file or directory
int OL_FileDirectoryPathExists(const char* fname);

#ifdef __cplusplus
}
#endif


#endif /* defined(__Outlaws__Outlaws__) */

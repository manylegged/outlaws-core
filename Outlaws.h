// -*- mode: c -*-
//  Outlaws.h
//  Outlaws
//
//  Created by Arthur Danskin on 10/21/12.
//  Copyright (c) 2012-2016 Arthur Danskin. All rights reserved.
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
    
#ifndef DLLFACE
#define DLLFACE
#endif

/////////////////////////////// Toggles ///////////////////////////////////////////////

#define OL_HAS_SDL 1

////////////////////////////// OS layer calls into Game ///////////////////////////////

// main game function - called once per frame
void OLG_Draw(void);
    
enum OLModKeys {
    OShiftKey = 0xF610,
    OControlKey = 0xF611,
    OAltKey = 0xF612,
};

enum EventType {
    OL_KEY_DOWN=0, OL_KEY_UP, OL_MOUSE_DOWN, OL_MOUSE_UP, OL_MOUSE_DRAGGED,
    OL_MOUSE_MOVED, OL_SCROLL_WHEEL, OL_LOST_FOCUS, OL_GAINED_FOCUS,
    OL_TOUCH_BEGIN, OL_TOUCH_MOVED, OL_TOUCH_STATIONARY, OL_TOUCH_ENDED, OL_TOUCH_CANCELLED,
    OL_GAMEPAD_AXIS, OL_GAMEPAD_ADDED, OL_GAMEPAD_REMOVED,
    OL_INVALID
};

struct OLEvent {
    enum EventType type;
    long key;
    int which;                  /* which device (gamepads) */
    float x, y;
    float dx, dy;               /* delta x, y */
};

// handle an input event
void OLG_OnEvent(const struct OLEvent* event);

// called before program terminates
void OLG_OnQuit(void);

// called when an unhandled exception is caught and game is about to crash
// also called by OL_Terminate. May be called several times
// return 1 if we should continue handling, or 0 to abort (i.e. a crash is already in progress)
int OLG_OnTerminate(const char* message);

// called when the application window is closed
// game should clean up and call OLG_DoQuit()
// return 1 if already closing, 0 if just started
int OLG_DoClose(void);

// init, process args. Return 1 if create window and interactive, 0 if headless mode
int OLG_Init(int argc, const char** argv);

// init opengl, return 1 if initialized, 0 if failed.
int OLG_InitGL(const char **error);

// called when window manager changes full screen state
void OLG_SetFullscreenPref(int enabled);

// handle assertions. return 1
DLLFACE NOINLINE int OLG_OnAssertFailed(const char* file, int line, const char* func,
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

// true to catch signals / print stack trace, etc
int OLG_EnableCrashHandler(void);

// return name of log file to open
const char* OLG_GetLogFileName(void);

// upload logfile to server
int OLG_UploadLog(const char* logdata, int loglen);

// return 0xRRGGBB indexed color code 
int OLG_GetQuake3Color(int val);

// return game language (e.g. ru, en)
const char* OLG_GetLanguage(void);

 //////////////////////////////// Game calls into OS layer //////////////////////////////////

// call around code inside the main loop of helper threads
// these allocate and drain autorelease pools on Apple platforms
void OL_ThreadBeginIteration(void);
void OL_ThreadEndIteration(void);

// return number of cpu cores
int OL_GetCpuCount(void);

// print a debugging message message
void OL_ReportMessage(const char *str);

// return 1 if log has already been opened
int OL_IsLogOpen(void);
void OL_OpenLog(void);

// time since start of game in seconds
double OL_GetCurrentTime(void);

// Sleep
void OL_Sleep(double seconds);

// get logged in username
const char* OL_GetUserName(void);

// return string describing runtime platform and current time, for log
const char* OL_GetPlatformDateInfo(void);

// open default web browser to selected url
int OL_OpenWebBrowser(const char* url);

int OL_OpenFolder(const char* url);

// quit gracefully, return 1 if already trying to quit
int OL_DoQuit(void);
int OL_IsQuitting(void);

// request that log be uploaded when game is shutdown
void OL_ScheduleUploadLog(const char* reason);

// read string from clipboard (may return null)
const char* OL_ReadClipboard(void);

// write string to clipboard
void OL_WriteClipboard(const char* txt);

// move cursor
void OL_WarpCursorPosition(float x, float y);

// get cursor
void OL_GetCursorPosition(float *x, float *y);

// disable gamepad
void OL_SetGamepadEnabled(int enabled);

// get name of gamepad
const char* OL_GetGamepadName(int instance_id);

// get ordered list of preferred languages from OS
// en, kr format
const char** OL_GetOSLanguages(void);

////////// Graphics

// swap the OpenGL buffers and display the frame
void OL_Present(void);

// get window size is pixels and points (for retina displays)
void OL_GetWindowSize(float *pixelWidth, float *pixelHeight, float *pointWidth, float *pointHeight);

// get scale factor of game window
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
    unsigned internal_format;   /* glTexImage2D */
    unsigned format;            /* glTexImage2D */
    unsigned type;              /* glTexImage2D */
    char *data;                 /* pixel data */
    void *handle;
} OutlawImage;

// load an image
struct OutlawImage OL_LoadImage(const char *fname);
void OL_FreeImage(struct OutlawImage *img);

// save an image to file (png format)
int OL_SaveImage(const OutlawImage *tex, const char* fname);

struct OLSize {
    float x, y;
};
#define OL_MAX_FONTS 10

// load a ttf font file, may be referred to later using INDEX
void OL_SetFont(int index, const char* file);

// render a string into an OpenGL texture, using a previously loaded font
int OL_StringImage(struct OutlawImage *img, const char* string, float size, int font, float maxw, float maxh);

// get a table of character sizes for font
void OL_FontAdvancements(int font, float size, struct OLSize* advancements); // advancements must be at least size 127

// get height from one line to the next
float OL_FontHeight(int fontName, float size);

// print stacktrace to log, upload log, quit program, etc
void OL_Terminate(const char* message);

/////////// File IO
// All functions take paths relative to main game directory

// create directories as needed for fname
// fname must be an absolute path - PathForFile is NOT called
int OL_CreateParentDirs(const char* fname);

// copy file from source to dest. create parent dirs for dest automatically
int OL_CopyFile(const char* source, const char *dest);

// Return list of files in a directory (base name only - no path)
const char** OL_ListDirectory(const char* path);

// get complete path for data file in utf8, searching through save directory and application resource directory
// mode should be "w" (local), "r" (read), "p" (package)
const char *OL_PathForFile(const char *fname, const char *mode);

// recursively delete a file or directory
int OL_RemoveFileOrDirectory(const char* dirname);

// delete just a file
int OL_RemoveFile(const char* fname);

// return true if path is a file or directory
int OL_FileDirectoryPathExists(const char* fname);
int OL_DirectoryExists(const char* path);

#if __APPLE__
const int OL_IsSandboxed(void);
const char* OL_SandboxSaveFile(const char* filename, const char* ext);
const char* OL_SandboxOpenFile(void); /* always .lua or .lua.gz! */
#endif

#ifdef __cplusplus
}

// convenience for c++ strings
inline const char** OL_ListDirectory(const std::string &path) { return OL_ListDirectory(path.c_str()); }
inline int OL_RemoveFileOrDirectory(const std::string &dirname) { return OL_RemoveFileOrDirectory(dirname.c_str()); }
inline const char *OL_PathForFile(const std::string &fname, const char *mode) { return OL_PathForFile(fname.c_str(), mode); }
inline struct OutlawImage OL_LoadImage(const std::string &fname) { return OL_LoadImage(fname.c_str()); }
#endif


#endif /* defined(__Outlaws__Outlaws__) */

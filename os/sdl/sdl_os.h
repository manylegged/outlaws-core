
// sdl_os.h - SDL Outlaws.h implementation

#ifndef SDL_OS
#define SDL_OS

#include <string>

//////////////// implemented in sdl.cpp

// run the main loop
int sdl_os_main(int argc, const char **argv);

// store a string for one frame, then autorelease
const char* sdl_os_autorelease(std::string &val);

// call from crash handler. flush log, etc.
void sdl_os_oncrash(const char* message);

void sdl_set_scaling_factor(float factor);

// return log file as a string
string sdl_get_logdata();

// utf8 <-> utf16
std::wstring s2ws(const std::string& s);


/////////////// implemented in per-os file

struct SDL_SysWMinfo;

// display a message box to the user on unrecoverable errors
void os_errormessage1(const char* msg, const string& data, SDL_SysWMinfo *info);

int os_create_parent_dirs(const char* path);

string os_get_platform_info();

int os_init();

#endif

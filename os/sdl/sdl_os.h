
// sdl_os.h - SDL Outlaws.h implementation

#ifndef SDL_OS
#define SDL_OS

//////////////// implemented in sdl.cpp

// run the main loop
int sdl_os_main(int argc, const char **argv);

// store a string for one frame, then autorelease
const char* sdl_os_autorelease(std::string &val);

// call from crash handler. flush log, etc.
void sdl_os_oncrash();

void sdl_set_scaling_factor(float factor);


/////////////// implemented in per-os file

// display a message box to the user on unrecoverable errors
void os_errormessage(const char* msg);

int os_create_parent_dirs(const char* path);

#endif

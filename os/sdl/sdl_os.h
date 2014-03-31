
// sdl_os.h - SDL Outlaws.h implementation

#ifndef SDL_OS
#define SDL_OS

// run the main loop
int sdl_os_main(int argc, char **argv);

// store a string for one frame, then autorelease
const char* sdl_os_autorelease(std::string &val);

// call from crash handler. flush log, etc.
void sdl_os_oncrash();

#endif

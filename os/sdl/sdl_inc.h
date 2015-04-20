
#ifndef SDL_INC_H
#define SDL_INC_H

#ifdef _MSC_VER
#  include "../../SDL2_ttf-2.0.12/include/SDL_ttf.h"
#  include "../../SDL2_image-2.0.0/include/SDL_image.h"
#  include "SDL.h"
#  include "SDL_syswm.h"
#  define OL_WINDOWS 1
#elif __APPLE__
#  include <SDL.h>
// #  include <SDL_ttf.h>
#  define OL_MAC 1
#else
#  include <SDL2/SDL.h>
#  include <SDL2/SDL_ttf.h>
#  include <SDL2/SDL_image.h>
#  include <SDL2/SDL_syswm.h>
#  include <sys/utsname.h>
#  include <dlfcn.h>
#  include <unistd.h>
#  define OL_LINUX 1
#endif

/////////////////// sdl_gamepad.cpp

#ifdef __cplusplus
extern "C"
{
#endif

int Controller_Init(void);
int Controller_HandleEvent(const SDL_Event *evt);

#ifdef __cplusplus
}
#endif


#endif // SDL_INC_H

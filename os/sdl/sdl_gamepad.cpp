

#include "StdAfx.h"
#include "sdl_inc.h"
#include "sdl_os.h"

static SDL_GameController *s_controller     = NULL;
static bool                s_gamepad_enabled = true;
static std::mutex          s_mutex;


// don't go through ReportMessagef/ReportMessage!
static void ReportGP(const char *format, ...)
{
    va_list vl;
    va_start(vl, format);
    const string buf = "\n[SDL] " + str_vformat(format, vl);
    OL_ReportMessage(buf.c_str());
    va_end(vl);
}

static bool initGamepad()
{
    static bool loadedMappings = true;
    if (!loadedMappings)
    {
        SDL_version linked;
        SDL_GetVersion(&linked);
        if (linked.minor > 0 || linked.patch > 1)
        {
            const int mappings = SDL_GameControllerAddMappingsFromFile(OL_PathForFile("data/gamecontrollerdb.txt", "r"));
            ReportGP("Loaded %d game controller mappings: %s",
                             max(0, mappings), mappings < 0 ? SDL_GetError() : "OK");
        }
        loadedMappings = true;
    }

    if (s_controller && !SDL_GameControllerGetAttached(s_controller))
    {
        SDL_GameControllerClose(s_controller);
        s_controller = NULL;
    }

    if (!s_controller)
    {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (SDL_IsGameController(i)) {
                s_controller = SDL_GameControllerOpen(i);
                if (s_controller) {
                    break;
                } else {
                    ReportGP("Could not open gamecontroller %i: %s", i, SDL_GetError());
                }
            }
        }
    }

    if (!s_controller)
        return false;

    ReportGP("Found a valid controller, named: %s", SDL_GameControllerName(s_controller));
    
    return true;
}

void OL_SetGamepadEnabled(int enabled)
{
    std::lock_guard<std::mutex> l(s_mutex);
    
    s_gamepad_enabled = enabled;
    if (s_controller && !enabled)
    {
        SDL_GameControllerClose(s_controller);
        s_controller = NULL;
    }
    else if (!s_controller && enabled)
    {
        initGamepad();
    }
}

const char* OL_GetGamepadName()
{
    std::lock_guard<std::mutex> l(s_mutex);
    
    return s_controller ? SDL_GameControllerName(s_controller) : NULL;
}


int Controller_HandleEvent(const SDL_Event *evt)
{
    std::lock_guard<std::mutex> l(s_mutex);
    
    OLEvent e;
    memset(&e, 0, sizeof(e));
    
    switch (evt->type) {
    case SDL_CONTROLLERDEVICEADDED:
    {
        ReportGP("SDL_CONTROLLERDEVICEADDED");
        initGamepad();
        e.type = OLEvent::GAMEPAD_ADDED;
        OLG_OnEvent(&e);
        return 1;
    }
    case SDL_CONTROLLERDEVICEREMOVED:
    {
        ReportGP("SDL_CONTROLLERDEVICEREMOVED");
        initGamepad();
        e.type = OLEvent::GAMEPAD_REMOVED;
        OLG_OnEvent(&e);
        return 1;
    }
    case SDL_CONTROLLERAXISMOTION: 
    {
        const SDL_ControllerAxisEvent &caxis = evt->caxis;
        e.type = OLEvent::GAMEPAD_AXIS;
        e.y = caxis.value / 32767.f;
        switch (caxis.axis)
        {
        case SDL_CONTROLLER_AXIS_LEFTX: e.key = GamepadAxisLeftX; break;
        case SDL_CONTROLLER_AXIS_LEFTY: e.key = GamepadAxisLeftY; break;
        case SDL_CONTROLLER_AXIS_RIGHTX: e.key = GamepadAxisRightX; break;
        case SDL_CONTROLLER_AXIS_RIGHTY: e.key = GamepadAxisRightY; break;
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT: e.key = GamepadAxisTriggerLeftY; break;
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: e.key = GamepadAxisTriggerRightY; break;
        }
        OLG_OnEvent(&e);
        return 1;
    }
    case SDL_CONTROLLERBUTTONDOWN: // fallthrough
    case SDL_CONTROLLERBUTTONUP:
    {
        const SDL_ControllerButtonEvent &cbutton = evt->cbutton;
        e.key = (int)cbutton.button + GamepadA;
        e.type = cbutton.state == SDL_PRESSED ? OLEvent::KEY_DOWN : OLEvent::KEY_UP;
        OLG_OnEvent(&e);
        return 1;
    }   
    }
    return 0;
}


int Controller_Init()
{
    std::lock_guard<std::mutex> l(s_mutex);
    
    SDL_Init(SDL_INIT_GAMECONTROLLER);
    return 1;
    
}

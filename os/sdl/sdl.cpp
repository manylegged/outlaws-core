
// Outlaws.h platform implementation for SDL

#include "StdAfx.h"

#ifdef _MSC_VER
#include "../../SDL2_ttf-2.0.12/include/SDL_ttf.h"
#include "../../SDL2_image-2.0.0/include/SDL_image.h"
#include "../../SDL2-2.0.1/include/SDL_keyboard.h"
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#endif

static uint g_screenWidth, g_screenHeight;
static uint g_sdl_flags;

static SDL_Window*   g_displayWindow;

static bool g_quitting;

GLenum glReportError (void)
{
    GLenum err = glGetError();
    if (GL_NO_ERROR != err)
        ReportMessagef("%s\n", (char *) gluErrorString (err));
    return err;
}

static SDL_RWops *g_logfile = NULL;


void sdl_os_oncrash()
{
    if (g_logfile)
    {
        OL_ReportMessage("SDL crash handler closing log");
        SDL_RWclose(g_logfile);
        g_logfile = NULL;
    }
    g_quitting = true;
}

void OL_ReportMessage(const char *str)
{
#if _MSC_VER
	OutputDebugStringA((LPCSTR)str);
	OutputDebugStringA((LPCSTR) "\n");
#else
    printf("%s\n", str);
#endif

    if (g_quitting)
        return;

    if (!g_logfile) {
        const char* path = OL_PathForFile("data/log.txt", "a");
        if (!g_logfile) { // may have been opened by OL_PathForFile
            g_logfile = SDL_RWFromFile(path, "w");
            if (!g_logfile)
                return;
            ReportMessagef("Log file opened at %s", path); // call self recursively
        }
    }
#if _MSC_VER
#define OL_NEWLINE "\r\n"
#else
#define OL_NEWLINE "\n"
#endif

    SDL_RWwrite(g_logfile, str, strlen(str), 1);
    SDL_RWwrite(g_logfile, OL_NEWLINE, strlen(OL_NEWLINE), 1);
}

void OL_SetFullscreen(int fullscreen)
{
    const uint flags = fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    if (flags != g_sdl_flags) {
        SDL_SetWindowFullscreen(g_displayWindow, flags);
        g_sdl_flags = flags;
    }
}

int OL_GetFullscreen(void)
{
    return g_sdl_flags == SDL_WINDOW_FULLSCREEN_DESKTOP;
}

double OL_GetCurrentTime()
{
    // SDL_GetTicks is in milliseconds
#if _MSC_VER
	static double currentTimeSeconds = 0;
	static int64 lastCount;

	if (lastCount == 0)
	{
        LARGE_INTEGER count;
		QueryPerformanceCounter(&count);
        lastCount = count.QuadPart;
        SetThreadAffinityMask(GetCurrentThread(), 0x1);
		return 0.0;
	}

    static double frequency = 0.0;
    if (frequency == 0.0)
    {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        frequency = (double)freq.QuadPart;
    }

	LARGE_INTEGER count;
    QueryPerformanceCounter(&count);

	int64 deltaTicks = count.QuadPart - lastCount;
	lastCount = count.QuadPart;
	
    double deltaSeconds = (double)deltaTicks / frequency;
    if (deltaSeconds < 0.0)
        deltaSeconds = 0.0;
    if (deltaSeconds > 0.5)
        deltaSeconds = 0.5;
	currentTimeSeconds += deltaSeconds;
	return currentTimeSeconds;
#else
    const uint   ticks = SDL_GetTicks();
    const double secs  = (double) ticks / 1000.0;
    return secs;
#endif
}

const char* OL_GetPlatformDateInfo(void)
{
    static string str;
    str.clear();

#if WIN32
    OSVERSIONINFO osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    GetVersionEx(&osvi);
    str += str_format("Windows %d.%d, build %d", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
#endif

    SDL_version compiled;
    SDL_version linked;
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    str += str_format(" SDL %d.%d patch %d ", linked.major, linked.minor, linked.patch);

    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
    std::time_t cstart = std::chrono::system_clock::to_time_t(start);
    str += std::ctime(&cstart);

    return str.c_str();
}

void OL_DoQuit()
{
    g_quitting = true;
}

void OL_GetWindowSize(float *pixelWidth, float *pixelHeight, float *pointWidth, float *pointHeight)
{
    *pixelWidth = g_screenWidth;
    *pixelHeight = g_screenHeight;

    *pointWidth = g_screenWidth;
    *pointHeight = g_screenHeight;
}

void OL_SetSwapInterval(int interval)
{
    // TODO implement me
    SDL_GL_SetSwapInterval(interval);
}

float OL_GetBackingScaleFactor() 
{
    return 1.f;
}

float OL_GetCurrentBackingScaleFactor(void)
{
    return 1.f;
}

struct OutlawTexture OL_LoadTexture(const char* fname)
{
    struct OutlawTexture t;
    memset(&t, 0, sizeof(t));
    
    const char *buf = OL_PathForFile(fname, "r");
    ReportMessagef("loading [%s]...\n", buf);

    SDL_Surface *surface = IMG_Load(buf);
 
    if (!surface) {
        ReportMessagef("SDL could not load '%s': %s\n", buf, SDL_GetError());
        return t;
    }

    GLenum texture_format = 0;
    const int nOfColors = surface->format->BytesPerPixel;
    if (nOfColors == 4) {
        if (surface->format->Rmask == 0x000000ff)
            texture_format = GL_RGBA;
        else
            texture_format = GL_BGRA;
    } else if (nOfColors == 3) {
        if (surface->format->Rmask == 0x000000ff)
            texture_format = GL_RGB;
        else
            texture_format = GL_BGR;
    }

    int w=surface->w, h=surface->h;
    ReportMessagef("texture has %d colors, %dx%d pixels\n", nOfColors, w, h);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
 
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 
    glTexImage2D(GL_TEXTURE_2D, 0, nOfColors, surface->w, surface->h, 0,
                  texture_format, GL_UNSIGNED_BYTE, surface->pixels);

	//gluBuild2DMipmaps( GL_TEXTURE_2D, nOfColors, surface->w, surface->h,
    //               texture_format, GL_UNSIGNED_BYTE, surface->pixels );
 
    SDL_FreeSurface(surface);
    glReportError();

    t.width = w;
    t.height = h;
    t.texnum = texture;

    return t;
}


static lstring                                  g_fontNames[10];
static std::map<std::pair<int, int>, TTF_Font*> g_fonts;

TTF_Font* getFont(int fontName, float size)
{
	const int isize = int(round(size));
    std::pair<int, int> key(fontName, isize);
    TTF_Font* &font = g_fonts[key];
    if (!font)
    {
        const char* file = g_fontNames[fontName].c_str();
        ASSERT(file);
        font = TTF_OpenFont(file, isize);
        if (font) {
            ReportMessagef("Loaded font '%s' at size %d", file, isize);
        } else {
            ReportMessagef("Failed to load font '%s' at size '%d': %s",
                           file, isize, TTF_GetError());
        }
        ASSERT(font);
    }
    return font;
}

void OL_SetFont(int index, const char* file, const char* name)
{
    g_fontNames[index] = lstring(OL_PathForFile(file, "r"));
}

void OL_FontAdvancements(int fontName, float size, struct OLSize* advancements)
{
    TTF_Font* font = getFont(fontName, size);
	const int font_pixel_height = TTF_FontLineSkip(font);
	for (uint i=0; i<128; i++)
	{
		int minx,maxx,miny,maxy,advance;
		if (TTF_GlyphMetrics(font,i,&minx,&maxx,&miny,&maxy,&advance) == 0)
		{
			advancements[i].x = advance;
		}
		else
		{
			ReportMessagef("Error getting glyph size for glyph %d/'%c'", i, i);
			advancements[i].x = 0.f;
		}
		advancements[i].y = font_pixel_height;
	}
}


int OL_StringTexture(OutlawTexture *tex, const char* str, float size, int _font, float maxw, float maxh)
{
	TTF_Font* font = getFont(_font, size);
    if (!font)
        return 0;

    int text_pixel_width = 0;
    vector<string> lines;

    int last_line_start = 0;

    for (int i=0; true; i++)
    {
        if (str[i] == '\n' || str[i] == '\0')
        {
            lines.push_back(string(&str[last_line_start], i - last_line_start));

            int line_pixel_width, line_pixel_height;
            TTF_SizeText(font, lines.back().c_str(), &line_pixel_width, &line_pixel_height);
            text_pixel_width = max(text_pixel_width, line_pixel_width);

            last_line_start = i+1;

            if (str[i] == '\0')
                break;
        }
    }

    const int font_pixel_height = TTF_FontLineSkip(font);
    const int text_pixel_height = lines.size() * font_pixel_height;

	GLuint texture;
    glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	SDL_Surface *intermediary = SDL_CreateRGBSurface(0, text_pixel_width, text_pixel_height, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);

    SDL_Color color;
    memset(&color, 255, sizeof(color));

    SDL_Rect dstrect;
    memset(&dstrect, 0, sizeof(dstrect));

    for (int i=0; i<lines.size(); i++)
    {
        if (lines[i].size())
        {
            SDL_Surface* initial = TTF_RenderText_Blended(font, lines[i].c_str(), color);
            if (initial)
            {
                SDL_SetSurfaceBlendMode(initial, SDL_BLENDMODE_NONE);
                SDL_BlitSurface(initial, 0, intermediary, &dstrect);
                SDL_FreeSurface(initial);
            }
            else
            {
                ReportMessagef("SDL_TTF Error: %s\n", TTF_GetError());
            }
        }

        dstrect.y += font_pixel_height;
    }

	glTexImage2D(GL_TEXTURE_2D, 0, 4, text_pixel_width, text_pixel_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, intermediary->pixels );
	SDL_FreeSurface(intermediary);
    tex->texnum = texture;
    tex->width = text_pixel_width;
    tex->height = text_pixel_height;

    //ReportMessagef("generated %dx%d texture %d for %d line text\n", text_pixel_width, text_pixel_height, texture, lines.size());
    return 1;
}



static int keysymToKey(const SDL_Keysym &keysym)
{
    const SDL_Keycode sym = keysym.sym;
    
    if (keysym.mod & KMOD_SHIFT) {
        // a -> A
        if (sym >= 97 && sym <= 122)
            return sym - 32;
        // 1 -> !
        switch (sym) {
        case SDLK_1: return SDLK_EXCLAIM;
        case SDLK_2: return SDLK_AT;
        case SDLK_3: return SDLK_HASH;
        case SDLK_4: return SDLK_DOLLAR;
        case SDLK_5: return 37;
        case SDLK_6: return SDLK_CARET;
        case SDLK_7: return SDLK_AMPERSAND;
        case SDLK_8: return SDLK_ASTERISK;
        case SDLK_9: return SDLK_LEFTPAREN;
        case SDLK_0: return SDLK_RIGHTPAREN;
        case SDLK_SLASH: return SDLK_QUESTION;
        case SDLK_MINUS: return SDLK_UNDERSCORE;
        case SDLK_EQUALS: return SDLK_PLUS;
        case SDLK_SEMICOLON: return SDLK_COLON;
        case SDLK_COMMA: return SDLK_LESS;
        case SDLK_PERIOD: return SDLK_GREATER;
            
        default:
            ;
        }
    }
    // ascii
    if (sym < 127)
        return sym;
    
    switch (sym)
    {
    case SDLK_LEFT:     return NSLeftArrowFunctionKey;
    case SDLK_RIGHT:    return NSRightArrowFunctionKey;
    case SDLK_UP:       return NSUpArrowFunctionKey;
    case SDLK_DOWN:     return NSDownArrowFunctionKey;
    case SDLK_PAGEUP:   return NSPageUpFunctionKey;
    case SDLK_PAGEDOWN: return NSPageDownFunctionKey;
    case SDLK_F1:       return NSF1FunctionKey;
    case SDLK_F2:       return NSF2FunctionKey;
    case SDLK_F3:       return NSF3FunctionKey;
    case SDLK_F4:       return NSF4FunctionKey;
    case SDLK_F5:       return NSF5FunctionKey;
    case SDLK_F6:       return NSF6FunctionKey;
    case SDLK_F7:       return NSF7FunctionKey;
    case SDLK_F8:       return NSF8FunctionKey;
    case SDLK_F9:       return NSF9FunctionKey;
    case SDLK_F10:      return NSF10FunctionKey;
    case SDLK_F11:      return NSF11FunctionKey;
    case SDLK_F12:      return NSF12FunctionKey;
    case SDLK_RSHIFT:   // fallthrough
    case SDLK_LSHIFT:   return OShiftKey;
    case SDLK_RCTRL:    // fallthrough
    case SDLK_LCTRL:    return OControlKey;
        //case SDLK_RMETA:    // fallthrough
        //case SDLK_LMETA:    // fallthrough
    case SDLK_RALT:     // fallthrough
    case SDLK_LALT:     return OAltKey;
    case SDLK_BACKSPACE: return NSBackspaceCharacter;
    case SDLK_DELETE:   return NSDeleteCharacter;
    default:            return 0;
    }
}


static void HandleEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        OLEvent e;
        memset(&e, 0, sizeof(e));

        switch (event.type) 
        {
        case SDL_WINDOWEVENT:
        {
            switch (event.window.event) {
            case SDL_WINDOWEVENT_SHOWN:
                SDL_Log("Window %d shown", event.window.windowID);
                break;
            case SDL_WINDOWEVENT_HIDDEN:
                SDL_Log("Window %d hidden", event.window.windowID);
                break;
            case SDL_WINDOWEVENT_EXPOSED:
                //SDL_Log("Window %d exposed", event.window.windowID);
                break;
            case SDL_WINDOWEVENT_MOVED:
                SDL_Log("Window %d moved to %d,%d",
                        event.window.windowID, event.window.data1,
                        event.window.data2);
                break;
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                SDL_Log("Window %d size changed", event.window.windowID);
                g_screenWidth = event.window.data1;
                g_screenHeight = event.window.data2;
                glViewport(0, 0, g_screenWidth, g_screenHeight);
                break;
            case SDL_WINDOWEVENT_RESIZED:
                g_screenWidth = event.window.data1;
                g_screenHeight = event.window.data2;
                glViewport(0, 0, g_screenWidth, g_screenHeight);
                SDL_Log("Window %d resized to %dx%d",
                        event.window.windowID, event.window.data1,
                        event.window.data2);
                break;
            case SDL_WINDOWEVENT_MINIMIZED:
                SDL_Log("Window %d minimized", event.window.windowID);
                break;
            case SDL_WINDOWEVENT_MAXIMIZED:
                SDL_Log("Window %d maximized", event.window.windowID);
                break;
            case SDL_WINDOWEVENT_RESTORED:
                SDL_Log("Window %d restored", event.window.windowID);
                break;
            case SDL_WINDOWEVENT_ENTER:
                //SDL_Log("Mouse entered window %d", event.window.windowID);
                break;
            case SDL_WINDOWEVENT_LEAVE:
                //SDL_Log("Mouse left window %d", event.window.windowID);
                break;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
               // SDL_Log("Window %d gained keyboard focus", event.window.windowID);
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST: {
                SDL_Log("Window %d lost keyboard focus", event.window.windowID);
                e.type = OLEvent::LOST_FOCUS;
                OLG_OnEvent(&e);
                break;
            }
            case SDL_WINDOWEVENT_CLOSE:
                SDL_Log("Window %d closed", event.window.windowID);
                g_quitting = true;
                break;
            default:
                SDL_Log("Window %d got unknown event %d",
                        event.window.windowID, event.window.event);
                break;
            }
            break;
        }
        case SDL_KEYUP:         // fallthrough
        case SDL_KEYDOWN:
        {
            e.type = (event.type == SDL_KEYDOWN) ? OLEvent::KEY_DOWN : OLEvent::KEY_UP;
            e.key = keysymToKey(event.key.keysym);

            //ReportMessagef("key %s %d %c\n", (event.type == SDL_KEYDOWN) ? "down" : "up", event.key.keysym.sym, e.key);

            if (e.key)
            {
                OLG_OnEvent(&e);
            }

            break;
        }
            
        case SDL_MOUSEMOTION:
        {
            e.dx = event.motion.xrel;
            e.dy = event.motion.yrel;
            int x, y;
            const Uint8 state = SDL_GetMouseState(&x, &y);
            e.x = x;
            e.y = y;
            e.key = ((state&SDL_BUTTON_LMASK) ? 0 : 
                     (state&SDL_BUTTON_RMASK) ? 1 :
                     (state&SDL_BUTTON_MMASK) ? 2 : -1);
            e.type = e.key != -1 ? OLEvent::MOUSE_DRAGGED : OLEvent::MOUSE_MOVED;
            OLG_OnEvent(&e);
            break;
        }
        case SDL_MOUSEWHEEL:
        {
            e.type = OLEvent::SCROLL_WHEEL;
            e.dy = 5.f * event.wheel.y;
            e.dx = event.wheel.x;
            OLG_OnEvent(&e);
            break;
        }
        case SDL_MOUSEBUTTONDOWN: // fallthrorugh
        case SDL_MOUSEBUTTONUP:
        {
            e.x = event.button.x;
            e.y = event.button.y;
            e.type = event.type == SDL_MOUSEBUTTONDOWN ? OLEvent::MOUSE_DOWN : OLEvent::MOUSE_UP;
            switch (event.button.button)
            {
            case SDL_BUTTON_LEFT: e.key = 0; break;
            case SDL_BUTTON_RIGHT: e.key = 1; break;
            case SDL_BUTTON_MIDDLE: e.key = 2; break;
            }
            OLG_OnEvent(&e);
            break;
        }
        break;
        case SDL_QUIT:
            g_quitting = true;
            break;
        }
    }
}

void OL_Present(void)
{
    SDL_GL_SwapWindow(g_displayWindow);
}


void OL_ThreadBeginIteration(int i)
{
    
}


static std::mutex g_autorelease_mutex;
static std::map<std::thread::id, vector<string> > g_autorelease;

const char* sdl_os_autorelease(std::string &val)
{
    std::lock_guard<std::mutex> l(g_autorelease_mutex);
    std::thread::id tid = std::this_thread::get_id();
    g_autorelease[tid].push_back(std::move(val));
    return g_autorelease[tid].back().c_str();
}

const char *OL_LoadFile(const char *name)
{
    const char *fname = OL_PathForFile(name, "r");

    SDL_RWops *io = SDL_RWFromFile(fname, "r");
    if (!io)
    {
        // ReportMessagef("[win32] error opening '%s' for reading", fname);
        return NULL;
    }

    
    string buf;
    Sint64 size = SDL_RWsize(io);
    buf.resize(size);
    if (SDL_RWread(io, (char*)buf.data(), buf.size(), sizeof(char)) <= 0) {
        ReportMessagef("[sdl] error writing to %s: %s", name, SDL_GetError());
    }
    if (SDL_RWclose(io) != 0) {
        ReportMessagef("[sdl] error closing file %s: %s", name, SDL_GetError());
    }

    return sdl_os_autorelease(buf);
}

void OL_ThreadEndIteration(int i)
{
    std::lock_guard<std::mutex> l(g_autorelease_mutex);
    std::thread::id tid = std::this_thread::get_id();
    g_autorelease[tid].clear();
}

int sdl_os_main(int argc, char **argv)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    g_screenWidth = 960;
    g_screenHeight = 600;

    g_displayWindow = SDL_CreateWindow("Gamma Void", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       g_screenWidth, g_screenHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext _glcontext = SDL_GL_CreateContext(g_displayWindow);

    // FIXME add fullscreen option: SDL_SetWindowFullscreen to toggle

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        ReportMessagef("Glew Error! %s", glewGetErrorString(err));
    }

    if (!glewIsSupported("GL_VERSION_2_0"))
    {
        ReportMessagef("Video Card Unsupported");
        //exit(1);
    }
   
    SDL_ShowCursor(0);
    if (TTF_Init() != 0)
    {
        ReportMessagef("TTF_Init() failed: %s", TTF_GetError());
        exit(2);
    }

    static const double kFrameTime = 1.0 / 60.0;

    while (!g_quitting)
    {
        const double start = OL_GetCurrentTime();
        HandleEvents();
        OLG_Draw();
        const double frameTime = OL_GetCurrentTime() - start;
        if (frameTime < kFrameTime) {
            SDL_Delay((kFrameTime - frameTime) * 1000.0);
        }
    }

    OLG_OnQuit();

    if (g_logfile)
    {
        SDL_RWclose(g_logfile);
        g_logfile = NULL;
    }

    SDL_DestroyWindow(g_displayWindow);

    TTF_Quit();
    SDL_Quit();
    return 0;
}

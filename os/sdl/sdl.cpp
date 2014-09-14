
// Outlaws.h platform implementation for SDL

#include "StdAfx.h"
#include "Graphics.h"
#include "sdl_os.h"


#ifdef _MSC_VER
#include "../../SDL2_ttf-2.0.12/include/SDL_ttf.h"
#include "../../SDL2_image-2.0.0/include/SDL_image.h"
#include "SDL_keyboard.h"
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <sys/utsname.h>
#endif

static int2         g_screenSize;
static uint         g_sdl_flags      = 0;
static float        g_scaling_factor = 1.f;
static SDL_Window*  g_displayWindow  = NULL;
static bool         g_quitting       = false;
static SDL_RWops   *g_logfile        = NULL;
static string       g_logpath;


// don't go through ReportMessagef/ReportMessage!
static void ReportSDL(const char *format, ...)
{
    va_list vl;
    va_start(vl, format);
    const string buf = "\n[SDL] " + str_vformat(format, vl);
    OL_ReportMessage(buf.c_str());
    va_end(vl);
}

void sdl_os_oncrash()
{
    fflush(NULL);

    string dest;
    {
        std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
        const std::time_t cstart = std::chrono::system_clock::to_time_t(start);
        char mbstr[100];
        std::strftime(mbstr, sizeof(mbstr), "%Y%m%d_%I.%M.%S.%p", std::localtime(&cstart));
        dest = OL_PathForFile(str_format("~/Desktop/%s_crashlog_%s.txt", OLG_GetName(), mbstr).c_str(), "w");
        ReportSDL("Copying log from %s to %s", g_logpath.c_str(), dest.c_str());
    }

    fflush(NULL);
    if (g_logfile)
    {
        ReportSDL("crash handler closing log\n");
        SDL_RWclose(g_logfile);
        g_logfile = NULL;
    }
    g_quitting = true;

    os_copy_file(g_logpath.c_str(), dest.c_str());

    os_errormessage(str_format(
        "Oops! %s crashed.\n"
        "Please email\n"
        "%s\nto arthur@anisopteragames.com",
        OLG_GetName(), dest.c_str()).c_str());
    exit(1);
}

void anonymizeUsername(string &str)
{
#if _MSC_VER
    const char *name = "Users";
#else
    const char *name = "/home";
#endif
    size_t start = str.find(name);
    if (start == string::npos)
        return;
    start += strlen(name) + 1;
    if (str.size() > start && strchr("/\\", str[start-1]))
    {
        int end=start+1;
        for (; end<str.size() && !strchr("/\\", str[end]); end++);
        str.replace(start, end-start, "<User>");
    }
}

void OL_ReportMessage(const char *str_)
{
#if _MSC_VER
	OutputDebugStringA((LPCSTR)str_);
#else
    printf("%s", str_);
#endif

    if (g_quitting)
        return;

    string str = str_;
    anonymizeUsername(str);
    
    if (!g_logfile) {
        const char* path = OL_PathForFile(OLG_GetLogFileName(), "w");
        if (!g_logfile) { // may have been opened by OL_PathForFile
            g_logfile = SDL_RWFromFile(path, "w");
            if (!g_logfile)
                return;
            g_logpath = path;
            ReportSDL("Log file opened at %s", path); // call self recursively
        }
    }
#if _MSC_VER
    str = str_replace(str, "\n", "\r\n");
#endif
    
    SDL_RWwrite(g_logfile, str.c_str(), str.size(), 1);
}

void OL_SetFullscreen(int fullscreen)
{
    const uint flags = fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    if (flags != g_sdl_flags) {
        ReportSDL("set fullscreen = %s", fullscreen ? "true" : "false");
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
    static double frequency = 0.0;
    if (frequency == 0.0)
    {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        frequency = (double)freq.QuadPart;
    }

	LARGE_INTEGER count;
    QueryPerformanceCounter(&count);

    static LARGE_INTEGER start = count;

    const uint64 rel = count.QuadPart - start.QuadPart;
    return (double) rel / frequency;
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
    const char* name = NULL;
    if (osvi.dwMajorVersion == 5 && osvi.dwMajorVersion == 1)
        name = "XP";
    else if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 0)
        name = "Vista";
    else if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 1)
        name = "7";
    else if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 2)
        name = "8";
    else if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 3)
        name = "8.1";
    else
        name = "Unknown";
    str += str_format("Windows %s (NT %d.%d), build %d", name, osvi.dwMajorVersion, 
                      osvi.dwMinorVersion, osvi.dwBuildNumber);
#else
    utsname buf;
    if (!uname(&buf))
    {
        str += buf.sysname;
        // str += buf.nodename;
        str += " ";
        str += buf.release;
        str += " ";
        str += buf.version;
        str += " ";
        str += buf.machine;
    }
#endif
    
    SDL_version compiled;
    SDL_version linked;
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);

    const int    cpucount = SDL_GetCPUCount();
    const int    rammb    = SDL_GetSystemRAM();
    const double ramGb    = rammb / 1024.0;

    str += str_format(" SDL %d.%d.%d, %d cores %.1f GB, ",
                      linked.major, linked.minor, linked.patch,
                      cpucount, ramGb);

    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
    std::time_t cstart = std::chrono::system_clock::to_time_t(start);
    str += std::ctime(&cstart);
    str.resize(str.size() - 1); // eat ctime newline

    return str.c_str();
}

void OL_DoQuit()
{
    g_quitting = true;
}

void sdl_set_scaling_factor(float factor)
{
    g_scaling_factor = factor;
}

void OL_GetWindowSize(float *pixelWidth, float *pixelHeight, float *pointWidth, float *pointHeight)
{
    *pixelWidth = g_screenSize.x;
    *pixelHeight = g_screenSize.y;

    *pointWidth = g_screenSize.x / g_scaling_factor;
    *pointHeight = g_screenSize.y / g_scaling_factor;
}

void OL_SetWindowSizePoints(int w, int h)
{
    SDL_SetWindowSize(g_displayWindow, w * g_scaling_factor, h * g_scaling_factor);
}

void OL_SetSwapInterval(int interval)
{
    SDL_GL_SetSwapInterval(interval);
}

float OL_GetBackingScaleFactor() 
{
    return g_scaling_factor;
}

float OL_GetCurrentBackingScaleFactor(void)
{
    return g_scaling_factor;
}

struct OutlawImage OL_LoadImage(const char* fname)
{
    // FIXME implement me
    OutlawImage img;
    return img;
}

struct OutlawTexture OL_LoadTexture(const char* fname)
{
    struct OutlawTexture t;
    memset(&t, 0, sizeof(t));
    
    const char *buf = OL_PathForFile(fname, "r");
    ReportSDL("loading [%s]...\n", buf);

    SDL_Surface *surface = IMG_Load(buf);
 
    if (!surface) {
        ReportSDL("SDL could not load '%s': %s\n", buf, SDL_GetError());
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
    ReportSDL("texture has %d colors, %dx%d pixels\n", nOfColors, w, h);

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

int OL_SaveTexture(const OutlawTexture *tex, const char* fname)
{
    if (!tex || !tex->texnum || tex->width <= 0 || tex->height <= 0)
        return 0;

    const size_t size = tex->width * tex->height * 4;
    uint *pix = (uint*) malloc(size);
    
    if (!pix)
        return 0;

    glBindTexture(GL_TEXTURE_2D, tex->texnum);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix);
    glReportError();

    // invert image
    for (int y=0; y<tex->height/2; y++)
    {
        for (int x=0; x<tex->width; x++)
        {
            const int top = y * tex->width + x;
            const int bot = (tex->height - y - 1) * tex->width + x;
            const uint temp = pix[top];
            pix[top] = pix[bot];
            pix[bot] = temp;
        }
    }
    int success = false;
    SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(pix, tex->width, tex->height, 32, tex->width*4,
                                                 0x000000ff, 0x0000FF00, 0x00FF0000, 0xFF000000);
    if (surf)
    {
        const char *path = OL_PathForFile(fname, "w");
        if (os_create_parent_dirs(path)) {
            success = IMG_SavePNG(surf, path) == 0;
        }
        if (!success) {
            ReportSDL("Failed to write texture %d-%dx%d to '%s': %s",
                      tex->texnum, tex->width, tex->height, path, SDL_GetError());
        }
        SDL_FreeSurface(surf);
    }
    else
    {
        ReportSDL("Failed to create surface %d-%dx%d: %s",
                  tex->texnum, tex->width, tex->height, SDL_GetError());
    }

    free(pix);
    
    return success;
}


static lstring                   g_fontNames[10];
static std::map<uint, TTF_Font*> g_fonts;
static std::mutex                g_fontMutex;

TTF_Font* getFont(int fontName, float size)
{
    std::lock_guard<std::mutex> l(g_fontMutex);

	const int   isize = int(round(size * g_scaling_factor));
    const uint  key   = (fontName<<16)|isize;
    TTF_Font*  &font  = g_fonts[key];

    if (!font)
    {
        const char* file = g_fontNames[fontName].c_str();
        ASSERT(file);
        font = TTF_OpenFont(file, isize);
        if (font) {
            ReportSDL("Loaded font '%s' at size %d", file, isize);
        } else {
            ReportSDL("Failed to load font '%s' at size '%d': %s",
                           file, isize, TTF_GetError());
        }
        ASSERT(font);
    }
    return font;
}

void OL_SetFont(int index, const char* file, const char* name)
{
    std::lock_guard<std::mutex> l(g_fontMutex);
    g_fontNames[index] = lstring(OL_PathForFile(file, "r"));
}

void OL_FontAdvancements(int fontName, float size, struct OLSize* advancements)
{
    TTF_Font* font = getFont(fontName, size);
	for (uint i=0; i<128; i++)
	{
		int minx,maxx,miny,maxy,advance;
		if (TTF_GlyphMetrics(font,i,&minx,&maxx,&miny,&maxy,&advance) == 0)
		{
			advancements[i].x = advance / g_scaling_factor;
		}
		else
		{
			ReportSDL("Error getting glyph size for glyph %d/'%c'", i, i);
			advancements[i].x = 0.f;
		}
		advancements[i].y = 0.f;
	}
}

float OL_FontHeight(int fontName, float size)
{
    const TTF_Font* font = getFont(fontName, size);
	return TTF_FontLineSkip(font) / g_scaling_factor;
}

struct Strip {
    TTF_Font *font;
    int       pixel_width;
    string    text;
};


int OL_StringTexture(OutlawTexture *tex, const char* str, float size, int _font, float maxw, float maxh)
{
	TTF_Font* font = getFont(_font, size);
    if (!font)
        return 0;

    TTF_Font *fallback_font = NULL;

    int text_pixel_width = 0;
    vector< Strip > strips;

    int last_strip_start = 0;
    int newlines = 0;
    bool last_was_fallback = false;
    int line_pixel_width = 0;

    // split string up by lines, fallback font
    for (int i=0; true; i++)
    {
        const int chr = str[i];

        int pixel_width, pixel_height;

        if (chr == '\n' || chr == '\0')
        {
            Strip st = { font, -1, string(&str[last_strip_start], i - last_strip_start) };
            TTF_SizeText(font, st.text.c_str(), &pixel_width, &pixel_height);
            strips.push_back(std::move(st));

            text_pixel_width = max(text_pixel_width, line_pixel_width + pixel_width);
            last_strip_start = i+1;
            newlines++;
            line_pixel_width = 0;

            if (chr == '\0')
                break;
        }
        else if (!TTF_GlyphIsProvided(font, chr))
        {
            if (!last_was_fallback && i > last_strip_start) {
                string strip(&str[last_strip_start], i - last_strip_start);
                TTF_SizeText(font, strip.c_str(), &pixel_width, &pixel_height);
                line_pixel_width += pixel_width;
                Strip st = { font, pixel_width, string(&str[last_strip_start], i - last_strip_start) };
                strips.push_back(std::move(st));
            }

            if (!fallback_font)
                fallback_font = getFont(0, size);

            TTF_SizeText(fallback_font, string(1, chr).c_str(), &pixel_width, &pixel_height);
            line_pixel_width += pixel_width;
            last_strip_start = i+1;

            if (last_was_fallback)
            {
                strips.back().pixel_width += pixel_width;
                strips.back().text += chr;
            }
            else
            {
                Strip st = { fallback_font, pixel_width, string(1, chr) };
                strips.push_back(std::move(st));
            }
            last_was_fallback = true;
        }
        else
        {
            last_was_fallback = false;
        }
    }

    const int font_pixel_height = TTF_FontLineSkip(font);
    const int text_pixel_height = newlines * font_pixel_height;

	SDL_Surface *intermediary = SDL_CreateRGBSurface(0, text_pixel_width, text_pixel_height, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);

    SDL_Color color;
    memset(&color, 255, sizeof(color));

    SDL_Rect dstrect;
    memset(&dstrect, 0, sizeof(dstrect));

    for (int i=0; i<strips.size(); i++)
    {
        if (strips[i].text.size())
        {
            SDL_Surface* initial = TTF_RenderText_Blended(strips[i].font, strips[i].text.c_str(), color);
            if (initial)
            {
                SDL_SetSurfaceBlendMode(initial, SDL_BLENDMODE_NONE);
                SDL_BlitSurface(initial, 0, intermediary, &dstrect);
                SDL_FreeSurface(initial);
            }
            else
            {
                ReportSDL("TTF Error: %s\n", TTF_GetError());
            }
        }

        if (strips[i].pixel_width < 0.f) { 
            dstrect.y += font_pixel_height;
            dstrect.x = 0;
        } else {
            dstrect.x += strips[i].pixel_width;
        }
    }

    glGenTextures(1, &tex->texnum);
	glBindTexture(GL_TEXTURE_2D, tex->texnum);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, text_pixel_width, text_pixel_height, 
                 0, GL_BGRA, GL_UNSIGNED_BYTE, intermediary->pixels);
    glReportError();

	SDL_FreeSurface(intermediary);

    tex->width = text_pixel_width;
    tex->height = text_pixel_height;

    //ReportSDL("generated %dx%d texture %d for %d line text\n", text_pixel_width, text_pixel_height, texture, strips.size());
    return 1;
}



static int keysymToKey(const SDL_Keysym &keysym)
{
    const SDL_Keycode sym = keysym.sym;
    
    if (keysym.mod & (KMOD_SHIFT|KMOD_CAPS) ) {
        // a -> A
        if (sym >= 'a' && sym <= 'z')
            return sym - 32;
        // 1 -> !
        switch (sym) {
        case SDLK_1: return SDLK_EXCLAIM;
        case SDLK_2: return SDLK_AT;
        case SDLK_3: return SDLK_HASH;
        case SDLK_4: return SDLK_DOLLAR;
        case SDLK_5: return '%';
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
        case SDLK_LEFTBRACKET: return '{';
        case SDLK_RIGHTBRACKET: return '}';
        case SDLK_QUOTE: return '"';
        case SDLK_BACKSLASH: return '|';
        case SDLK_BACKQUOTE: return '~';
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
    case SDLK_HOME:     return NSHomeFunctionKey;
    case SDLK_END:      return NSEndFunctionKey;
    case SDLK_PRINTSCREEN: return NSPrintScreenFunctionKey;
    case SDLK_INSERT:   return NSInsertFunctionKey;
    case SDLK_PAUSE:    return NSPauseFunctionKey;
    case SDLK_SCROLLLOCK: return NSScrollLockFunctionKey;
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
    case SDLK_DELETE:   return NSDeleteFunctionKey;
    default:            return 0;
    }
}


static void HandleEvents()
{
    SDL_Event evt;
    while (SDL_PollEvent(&evt))
    {
        OLEvent e;
        memset(&e, 0, sizeof(e));

        switch (evt.type)
        {
        case SDL_WINDOWEVENT:
        {
            switch (evt.window.event) {
            case SDL_WINDOWEVENT_SHOWN:
                ReportSDL("Window %d shown", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_HIDDEN:
                ReportSDL("Window %d hidden", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_EXPOSED:
                //ReportSDL("Window %d exposed", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_MOVED:
                ReportSDL("Window %d moved to %d,%d",
                        evt.window.windowID, evt.window.data1,
                        evt.window.data2);
                break;
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                ReportSDL("Window %d size changed", evt.window.windowID);
                g_screenSize.x = evt.window.data1;
                g_screenSize.y = evt.window.data2;
                glViewport(0, 0, g_screenSize.x, g_screenSize.y);
                break;
            case SDL_WINDOWEVENT_RESIZED:
                g_screenSize.x = evt.window.data1;
                g_screenSize.y = evt.window.data2;
                glViewport(0, 0, g_screenSize.x, g_screenSize.y);
                ReportSDL("Window %d resized to %dx%d",
                        evt.window.windowID, evt.window.data1,
                        evt.window.data2);
                break;
            case SDL_WINDOWEVENT_MINIMIZED:
                ReportSDL("Window %d minimized", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_MAXIMIZED:
                ReportSDL("Window %d maximized", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_RESTORED:
                ReportSDL("Window %d restored", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_ENTER:
                //ReportSDL("Mouse entered window %d", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_LEAVE:
                //ReportSDL("Mouse left window %d", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
               // ReportSDL("Window %d gained keyboard focus", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST: {
                ReportSDL("Window %d lost keyboard focus", evt.window.windowID);
                e.type = OLEvent::LOST_FOCUS;
                OLG_OnEvent(&e);
                break;
            }
            case SDL_WINDOWEVENT_CLOSE:
                ReportSDL("Window %d closed", evt.window.windowID);
                g_quitting = true;
                break;
            default:
                ReportSDL("Window %d got unknown event %d",
                        evt.window.windowID, evt.window.event);
                break;
            }
            break;
        }
        case SDL_KEYUP:         // fallthrough
        case SDL_KEYDOWN:
        {
            e.type = (evt.type == SDL_KEYDOWN) ? OLEvent::KEY_DOWN : OLEvent::KEY_UP;
            e.key = keysymToKey(evt.key.keysym);

            //ReportSDL("key %s %d %c\n", (evt.type == SDL_KEYDOWN) ? "down" : "up", evt.key.keysym.sym, e.key);

            if (e.key)
            {
                OLG_OnEvent(&e);
            }

            break;
        }
            
        case SDL_MOUSEMOTION:
        {
            e.dx = evt.motion.xrel / g_scaling_factor;
            e.dy = evt.motion.yrel / g_scaling_factor;
            e.x = evt.motion.x / g_scaling_factor;
            e.y = (g_screenSize.y - evt.motion.y) / g_scaling_factor;
            const Uint8 state = evt.motion.state;
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
            e.dy = 5.f * evt.wheel.y;
            e.dx = evt.wheel.x;
            OLG_OnEvent(&e);
            break;
        }
        case SDL_MOUSEBUTTONDOWN: // fallthrorugh
        case SDL_MOUSEBUTTONUP:
        {
            e.x = evt.button.x / g_scaling_factor;
            e.y = (g_screenSize.y - evt.button.y) / g_scaling_factor;
            e.type = evt.type == SDL_MOUSEBUTTONDOWN ? OLEvent::MOUSE_DOWN : OLEvent::MOUSE_UP;
            switch (evt.button.button)
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
            // FIXME call OLG_OnClose
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
        return NULL;
    }

    
    string buf;
    Sint64 size = SDL_RWsize(io);
    buf.resize(size);
    if (SDL_RWread(io, (char*)buf.data(), buf.size(), sizeof(char)) <= 0) {
        ReportSDL("error writing to %s: %s", name, SDL_GetError());
    }
    if (SDL_RWclose(io) != 0) {
        ReportSDL("error closing file %s: %s", name, SDL_GetError());
    }

    return sdl_os_autorelease(buf);
}

void OL_ThreadEndIteration(int i)
{
    std::lock_guard<std::mutex> l(g_autorelease_mutex);
    std::thread::id tid = std::this_thread::get_id();
    g_autorelease[tid].clear();
}

void OL_WarpCursorPosition(float x, float y)
{
    SDL_WarpMouseInWindow(g_displayWindow, (int)x * g_scaling_factor, g_screenSize.y - (int) (y * g_scaling_factor));
}

const char* OL_ReadClipboard()
{
    char *ptr = SDL_GetClipboardText();
    if (!ptr)
        return NULL;
    string str = ptr;
#if WIN32
    str_replace(ptr, "\r\n", "\n");
#endif
    SDL_free(ptr);
    return sdl_os_autorelease(str);
}

static void setupInitialResolution()
{
    g_screenSize.x = 960;
    g_screenSize.y = 600;

    const int displayCount = SDL_GetNumVideoDisplays();

    for (int i=0; i<displayCount; i++)
    {
        SDL_DisplayMode mode;
        SDL_GetDesktopDisplayMode(i, &mode);
        ReportSDL("Display %d of %d is %dx%d@%dHz", i+1, displayCount, mode.w, mode.h, mode.refresh_rate);

        if (i == 0)
            g_screenSize = int2(mode.w, mode.h);
        g_screenSize = min(g_screenSize, int2(0.9f * float2(mode.w, mode.h)));
    }
    ReportSDL("Initial window size is %dx%d", g_screenSize.x, g_screenSize.y);
}

int sdl_os_main(int argc, char **argv)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    setupInitialResolution();
    g_displayWindow = SDL_CreateWindow(OLG_GetName(), 
                                       SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                       g_screenSize.x, g_screenSize.y,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_GLContext _glcontext = SDL_GL_CreateContext(g_displayWindow);

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        os_errormessage(str_format("Glew Error! %s", glewGetErrorString(err)).c_str());
        exit(1);
    }

    
    // if (!glewIsSupported("GL_VERSION_2_0"))
    // {
    //     os_errormessage("Video Card Unsupported");
    //     exit(1);
    // }
 
    SDL_ShowCursor(0);
    if (TTF_Init() != 0)
    {
        os_errormessage(str_format("TTF_Init() failed: %s", TTF_GetError()).c_str());
        exit(1);
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
        SDL_RWwrite(g_logfile, "\n", 1, 1);
        SDL_RWclose(g_logfile);
        g_logfile = NULL;
    }

    fflush(NULL);

    SDL_DestroyWindow(g_displayWindow);

    TTF_Quit();
    SDL_Quit();

    ReportSDL("Good bye!\n");
    return 0;
}

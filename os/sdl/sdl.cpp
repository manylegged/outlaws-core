
// Outlaws.h platform implementation for SDL

#include "StdAfx.h"
#include "Graphics.h"
#include "sdl_os.h"


#ifdef _MSC_VER
#include "../../SDL2_ttf-2.0.12/include/SDL_ttf.h"
#include "../../SDL2_image-2.0.0/include/SDL_image.h"
#include "../../SDL2-2.0.1/include/SDL_keyboard.h"
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <sys/utsname.h>
#endif

#include <regex>

static int2 g_screenSize;
static uint g_sdl_flags;

static SDL_Window*   g_displayWindow;

static bool g_quitting;

static SDL_RWops *g_logfile = NULL;
static string g_logpath;

void sdl_os_oncrash()
{
    fflush(NULL);
    if (g_logfile)
    {
        OL_ReportMessage("[SDL] crash handler closing log");
        SDL_RWclose(g_logfile);
        g_logfile = NULL;
    }
    g_quitting = true;

    string lpath = os_copy_to_desktop(g_logpath.c_str());
    os_errormessage(str_format(
        "Oops! %s crashed.\n"
        "Please email\n"
        "%s\nto arthur@anisopteragames.com",
        OLG_GetName(), lpath.c_str()).c_str());
    exit(1);
}

void OL_ReportMessage(const char *str_)
{
#if _MSC_VER
	OutputDebugStringA((LPCSTR)str_);
	OutputDebugStringA((LPCSTR) "\n");
#else
    printf("%s\n", str_);
#endif

    if (g_quitting)
        return;

    string str;
    // anonymize contents of log
    try {
#ifdef _MSC_VER
        static const std::regex reg("([\\\\/]Users[\\\\/])[^\\\\/]+");
#else
        // GCC 4.8 does not support regex, throws an error...
        static const std::regex reg("(/home/)[^/]+", std::regex::extended);
#endif
        static const string repl("$1<Username>");
        str = std::regex_replace(string(str_), reg, repl);
    } catch (...) {
        str = str_;
    }
    
    if (!g_logfile) {
        const char* path = OL_PathForFile("data/log.txt", "a");
        if (!g_logfile) { // may have been opened by OL_PathForFile
            g_logfile = SDL_RWFromFile(path, "w");
            if (!g_logfile)
                return;
            g_logpath = path;
            ReportMessagef("Log file opened at %s", path); // call self recursively
        }
    }
#if _MSC_VER
#define OL_NEWLINE "\r\n"
    str = str_replace(str, "\n", "\r\n");
#else
#define OL_NEWLINE "\n"
#endif
    
    SDL_RWwrite(g_logfile, str.c_str(), str.size(), 1);
    SDL_RWwrite(g_logfile, OL_NEWLINE, strlen(OL_NEWLINE), 1);
}

void OL_SetFullscreen(int fullscreen)
{
    const uint flags = fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    if (flags != g_sdl_flags) {
        ReportMessagef("[SDL] set fullscreen = %s", fullscreen ? "true" : "false");
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

    str += str_format(" SDL %d.%d patch %d, %d cores %.1f GB, ",
                      linked.major, linked.minor, linked.patch,
                      cpucount, ramGb);

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
    *pixelWidth = g_screenSize.x;
    *pixelHeight = g_screenSize.y;

    *pointWidth = g_screenSize.x;
    *pointHeight = g_screenSize.y;
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

int OL_SaveTexture(const OutlawTexture *tex, const char* fname)
{
    if (!tex || !tex->texnum || tex->width <= 0 || tex->height <= 0)
        return 0;

    const size_t size = tex->width * tex->height * 4;
    uint *pix = (uint*) malloc(size);
    
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

    SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(pix, tex->width, tex->height, 32, tex->width*4,
                                                 0x000000ff, 0x0000FF00, 0x00FF0000, 0xFF000000);

    const char *path = OL_PathForFile(fname, "w");
    const int success = IMG_SavePNG(surf, path);
    if (!success) {
        ReportMessagef("[SDL] Failed to write texture %d-%dx%d to '%s': %s",
                       tex->texnum, tex->width, tex->height, fname, SDL_GetError());
    }
    SDL_FreeSurface(surf);
    free(pix);
    
    return success;
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
            ReportMessagef("[SDL] Loaded font '%s' at size %d", file, isize);
        } else {
            ReportMessagef("[SDL] Failed to load font '%s' at size '%d': %s",
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
	for (uint i=0; i<128; i++)
	{
		int minx,maxx,miny,maxy,advance;
		if (TTF_GlyphMetrics(font,i,&minx,&maxx,&miny,&maxy,&advance) == 0)
		{
			advancements[i].x = advance;
		}
		else
		{
			ReportMessagef("[SDL] Error getting glyph size for glyph %d/'%c'", i, i);
			advancements[i].x = 0.f;
		}
		advancements[i].y = 0.f;
	}
}

float OL_FontHeight(int fontName, float size)
{
    TTF_Font* font = getFont(fontName, size);
	return TTF_FontLineSkip(font);
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
                ReportMessagef("[SDL] TTF Error: %s\n", TTF_GetError());
            }
        }

        if (strips[i].pixel_width < 0.f) { 
            dstrect.y += font_pixel_height;
            dstrect.x = 0;
        } else {
            dstrect.x += strips[i].pixel_width;
        }
    }

	glTexImage2D(GL_TEXTURE_2D, 0, 4, text_pixel_width, text_pixel_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, intermediary->pixels );
	SDL_FreeSurface(intermediary);
    tex->texnum = texture;
    tex->width = text_pixel_width;
    tex->height = text_pixel_height;

    //ReportMessagef("generated %dx%d texture %d for %d line text\n", text_pixel_width, text_pixel_height, texture, strips.size());
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
                ReportMessagef("[SDL] Window %d shown", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_HIDDEN:
                ReportMessagef("[SDL] Window %d hidden", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_EXPOSED:
                //ReportMessagef("[SDL] Window %d exposed", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_MOVED:
                ReportMessagef("[SDL] Window %d moved to %d,%d",
                        evt.window.windowID, evt.window.data1,
                        evt.window.data2);
                break;
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                ReportMessagef("[SDL] Window %d size changed", evt.window.windowID);
                g_screenSize.x = evt.window.data1;
                g_screenSize.y = evt.window.data2;
                glViewport(0, 0, g_screenSize.x, g_screenSize.y);
                break;
            case SDL_WINDOWEVENT_RESIZED:
                g_screenSize.x = evt.window.data1;
                g_screenSize.y = evt.window.data2;
                glViewport(0, 0, g_screenSize.x, g_screenSize.y);
                ReportMessagef("[SDL] Window %d resized to %dx%d",
                        evt.window.windowID, evt.window.data1,
                        evt.window.data2);
                break;
            case SDL_WINDOWEVENT_MINIMIZED:
                ReportMessagef("[SDL] Window %d minimized", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_MAXIMIZED:
                ReportMessagef("[SDL] Window %d maximized", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_RESTORED:
                ReportMessagef("[SDL] Window %d restored", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_ENTER:
                //ReportMessagef("[SDL] Mouse entered window %d", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_LEAVE:
                //ReportMessagef("[SDL] Mouse left window %d", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_FOCUS_GAINED:
               // ReportMessagef("[SDL] Window %d gained keyboard focus", evt.window.windowID);
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST: {
                ReportMessagef("[SDL] Window %d lost keyboard focus", evt.window.windowID);
                e.type = OLEvent::LOST_FOCUS;
                OLG_OnEvent(&e);
                break;
            }
            case SDL_WINDOWEVENT_CLOSE:
                ReportMessagef("[SDL] Window %d closed", evt.window.windowID);
                g_quitting = true;
                break;
            default:
                ReportMessagef("[SDL] Window %d got unknown event %d",
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

            //ReportMessagef("key %s %d %c\n", (evt.type == SDL_KEYDOWN) ? "down" : "up", evt.key.keysym.sym, e.key);

            if (e.key)
            {
                OLG_OnEvent(&e);
            }

            break;
        }
            
        case SDL_MOUSEMOTION:
        {
            e.dx = evt.motion.xrel;
            e.dy = evt.motion.yrel;
            int x, y;
            const Uint8 state = SDL_GetMouseState(&x, &y);
            e.x = x;
            e.y = g_screenSize.y - y;
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
            e.x = evt.button.x;
            e.y = g_screenSize.y - evt.button.y;
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
        ReportMessagef("[SDL] error writing to %s: %s", name, SDL_GetError());
    }
    if (SDL_RWclose(io) != 0) {
        ReportMessagef("[SDL] error closing file %s: %s", name, SDL_GetError());
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
    SDL_WarpMouseInWindow(g_displayWindow, (int)x, g_screenSize.y - (int)y);
}

const char* OL_ReadClipboard()
{
    char *ptr = SDL_GetClipboardText();
    string str(ptr);
    SDL_free(ptr);
    return sdl_os_autorelease(str);
}

int sdl_os_main(int argc, char **argv)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    g_screenSize.x = 960;
    g_screenSize.y = 600;

    g_displayWindow = SDL_CreateWindow(OLG_GetName(), 100, 100,
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
        SDL_RWclose(g_logfile);
        g_logfile = NULL;
    }

    SDL_DestroyWindow(g_displayWindow);

    TTF_Quit();
    SDL_Quit();

    ReportMessagef("[SDL] Good bye!");
    return 0;
}


// Outlaws.h platform implementation for SDL

#include "StdAfx.h"

#include <locale>

#include "Graphics.h"
#include "Unicode.h"

#include "sdl_inc.h"
#include "sdl_os.h"

static SDL_Rect     g_savedWindowPos;
static int2         g_windowSize; // size of current window
static float        g_scaling_factor = 1.f;
static SDL_Window*  g_displayWindow  = NULL;
static bool         g_quitting       = false;
static SDL_RWops   *g_logfile        = NULL;
static const char*  g_logpath        = NULL;
static string       g_logdata;
static bool         g_openinglog = false;
static int          g_supportsTearControl = -1;
static bool         g_wantsLogUpload = false;

void SetWindowResizable(SDL_Window *win, SDL_bool resizable)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(g_displayWindow, &info))
        return;

#if WIN32
    HWND hwnd = info.info.win.window;
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    if (resizable)
        style |= WS_THICKFRAME;
    else
        style &= ~WS_THICKFRAME;
    SetWindowLong(hwnd, GWL_STYLE, style);
#endif
}

void os_errormessage(const string &str)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(g_displayWindow, &info);
    return os_errormessage1(str, &info);
}

// don't go through ReportMessagef/ReportMessage!
static void ReportSDL(const char *format, ...)
{
    va_list vl;
    va_start(vl, format);
    const string buf = "\n[SDL] " + str_vformat(format, vl);
    OL_ReportMessage(buf.c_str());
    va_end(vl);
}

static const string loadFile(SDL_RWops *io, const char* name)
{
    if (!io)
        return "";
    string buf;
    Sint64 size = SDL_RWsize(io);
    buf.resize(size);
    if (SDL_RWread(io, (char*)buf.data(), buf.size(), sizeof(char)) <= 0) {
        ReportSDL("error writing to %s: %s", name, SDL_GetError());
    }
    if (SDL_RWclose(io) != 0) {
        ReportSDL("error closing file %s: %s", name, SDL_GetError());
    }

    return buf;
}

static bool readUploadLog(string& data)
{
    data = loadFile(SDL_RWFromFile(g_logpath, "r"), g_logpath);
    return data.size() ? OLG_UploadLog(data.c_str(), data.size()) : 0;
}

void sdl_os_oncrash(const char* message)
{
    fflush(NULL);
    if (g_logfile)
    {
        ReportSDL("crash handler closing log\n");
        SDL_RWclose(g_logfile);
        g_logfile = NULL;
    }
    g_quitting = true;          // prevent log from reopening

    string data;
    bool success = readUploadLog(data);
    SteamAPI_SetMiniDumpComment(data.size() ? data.c_str() : "Error loading log");

    if (success)
    {
        os_errormessage(str_format("%s\nAnonymous log uploaded OK.\n\n%s", message, g_logpath));
        return;
    }
    
    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
    const std::time_t cstart = std::chrono::system_clock::to_time_t(start);
    char mbstr[100];
    std::strftime(mbstr, sizeof(mbstr), "%Y%m%d_%I.%M.%S.%p", std::localtime(&cstart));
    string dest = OL_PathForFile(str_format("~/Desktop/%s_crashlog_%s.txt", OLG_GetName(), mbstr).c_str(), "w");
    ReportSDL("Copying log from %s to %s", g_logpath, dest.c_str());

    OL_CopyFile(g_logpath, dest.c_str());
    
    os_errormessage(str_format(
        "%s\n"
        "Please email\n"
        "%s\nto arthur@anisopteragames.com",
        message, dest.c_str()).c_str());
}

void OL_ScheduleUploadLog(void)
{
    ReportSDL("Log upload scheduled");
    g_wantsLogUpload = true;
}


void anonymizeUsername(string &str)
{
#if OL_LINUX
    const char *name = "/home";
#else
    const char *name = "Users";
#endif
    for (size_t start = str.find(name);
         start != string::npos && start < str.size();
         start = str.find(name, start))
    {
        start += strlen(name) + 1;
        if (start < str.size() && strchr("/\\", str[start-1]))
        {
            int end=start+1;
            for (; end<str.size() && !strchr("/\\", str[end]); end++);
            str.replace(start, end-start, "<User>");
        }
    }
}

void OL_ReportMessage(const char *str_)
{
#if OL_WINDOWS
    OutputDebugStringA(str_);
#endif
    printf("%s", str_);

    if (g_quitting)
        return;

    string str = str_;
    anonymizeUsername(str);
    
    if (!g_logfile) {
        if (g_openinglog) {
            g_logdata += str;
            return;
        }
        g_openinglog = true;
        const char* path = OL_PathForFile(OLG_GetLogFileName(), "w");
        if (!g_logfile) { // may have been opened by OL_PathForFile
            os_create_parent_dirs(path);
            g_logfile = SDL_RWFromFile(path, "w");
            if (!g_logfile)
                return;
            g_logpath = lstring(path).c_str();
            g_openinglog = false;
            if (g_logdata.size())
                SDL_RWwrite(g_logfile, g_logdata.c_str(), g_logdata.size(), 1);
            // call self recursively
            ReportSDL("Log file opened at %s", path);
            const char* latestpath = OL_PathForFile("data/log_latest.txt", "w");
#if !OL_WINDOWS
            int status = unlink(latestpath);
            if (status && status != ENOENT) {
                ReportSDL("Error unlink('%s'): %s", latestpath, strerror(errno));
            }
            if (symlink(g_logpath, latestpath)) {
                ReportSDL("Error symlink('%s', '%s'): %s", g_logpath, latestpath, strerror(errno));
            }
#endif
        }
    }
#if OL_WINDOWS
    str = str_replace(str, "\n", "\r\n");
#endif
    
    SDL_RWwrite(g_logfile, str.c_str(), str.size(), 1);
}

int OL_GetFullscreen(void)
{
    const int flags = SDL_GetWindowFlags(g_displayWindow);
    if (flags&SDL_WINDOW_BORDERLESS)
        return 1;
    else if (flags&(SDL_WINDOW_FULLSCREEN_DESKTOP|SDL_WINDOW_FULLSCREEN))
        return 2;
    else
        return 0;
}

// 0 is windows, 1 is "fake" fullscreen, 2 is "true" fullscreen
void OL_SetFullscreen(int fullscreen)
{
    const int wasfs = OL_GetFullscreen();
    
#if !OL_WINDOWS
    if (fullscreen)
        fullscreen = 2;
#endif

    if (fullscreen != wasfs)
    {
        g_supportsTearControl = -1; // reset

        // disable / save old state
        if (wasfs == 0)
        {
            ReportSDL("Saving windowed window pos");
            SDL_GetWindowPosition(g_displayWindow, &g_savedWindowPos.x, &g_savedWindowPos.y);
            SDL_GetWindowSize(g_displayWindow, &g_savedWindowPos.w, &g_savedWindowPos.h);
        }
        else if (wasfs == 1)
        {
            ReportSDL("Disabled Fake Fullscreen %d,%d/%dx%d",
                      g_savedWindowPos.x, g_savedWindowPos.y, g_savedWindowPos.w, g_savedWindowPos.h);
            SDL_SetWindowBordered(g_displayWindow, SDL_TRUE);
            SetWindowResizable(g_displayWindow, SDL_TRUE);
        }
        else if (wasfs == 2)
        {
            ReportSDL("Disabled Fullscreen");
            SDL_SetWindowFullscreen(g_displayWindow, 0);
#if OL_LINUX
            SDL_SetWindowGrab(g_displayWindow, SDL_FALSE);
#endif
        }

        // enable new state
        if (fullscreen == 0)
        {
            ReportSDL("Restoring windowed window pos");
            SDL_SetWindowSize(g_displayWindow, g_savedWindowPos.w, g_savedWindowPos.h);
            SDL_SetWindowPosition(g_displayWindow, g_savedWindowPos.x, g_savedWindowPos.y);
        }
        else if (fullscreen == 1)
        {
            int idx = SDL_GetWindowDisplayIndex(g_displayWindow);
            SDL_Rect bounds;
            SDL_GetDisplayBounds(idx, &bounds);
                
            ReportSDL("Enabled Fake Fullscreen %d,%d/%dx%d (from %d,%d/%dx%d)",
                      bounds.x, bounds.y, bounds.w, bounds.h,
                      g_savedWindowPos.x, g_savedWindowPos.y, g_savedWindowPos.w, g_savedWindowPos.h);

            SDL_SetWindowBordered(g_displayWindow, SDL_FALSE);
            SetWindowResizable(g_displayWindow, SDL_FALSE);
            SDL_SetWindowPosition(g_displayWindow, bounds.x, bounds.y);
            SDL_SetWindowSize(g_displayWindow, bounds.w, bounds.h);
        }
        else if (fullscreen == 2)
        {
            ReportSDL("Enabled Fullscreen");
            SDL_SetWindowFullscreen(g_displayWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
#if OL_LINUX
            SDL_SetWindowGrab(g_displayWindow, SDL_TRUE);
#endif
        }
    }
}

double OL_GetCurrentTime()
{
    static double frequency = 0.0;
    if (frequency == 0.0)
        frequency = (double) SDL_GetPerformanceFrequency();

    const Uint64 count = SDL_GetPerformanceCounter();

    static Uint64 start = count;

    const uint64 rel = count - start;
    return (double) rel / frequency;
}


#if OL_LINUX

static void* getSymbol(const char* module, const char* symbol)
{
    void *handle = dlopen(module, RTLD_NOW|RTLD_GLOBAL|RTLD_NOLOAD);
    if (!handle) {
        ReportSDL("Failed to access '%s': %s", module, dlerror());
        return NULL;
    }
    void* sym = dlsym(handle, symbol);
    char* error = NULL;
    if ((error = dlerror())) {
        ReportSDL("Failed to get symbol '%s' from '%s': %s", symbol, module, error);
        return NULL;
    }
    return sym;
}

static int getSystemRam()
{
    // this call introduced in SDL 2.0.1 - backwards compatability
    typedef int SDLCALL (*PSDL_GetSystemRam)(void);
    PSDL_GetSystemRam pSDL_GetSystemRam = (PSDL_GetSystemRam) getSymbol("libSDL2-2.0.so.0", "SDL_GetSystemRAM");
    return pSDL_GetSystemRam ? pSDL_GetSystemRam() : 0;
}

#else

static int getSystemRam()
{
    return SDL_GetSystemRAM();
}

#endif


const char* OL_GetPlatformDateInfo(void)
{
    static string str;

    str = os_get_platform_info();

    SDL_version compiled;
    SDL_version linked;
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);

    const int    cpucount = SDL_GetCPUCount();
    const int    rammb    = getSystemRam();
    const double ramGb    = rammb / 1024.0;

    str += str_format(" SDL %d.%d.%d, %d cores %.1f GB, ",
                      linked.major, linked.minor, linked.patch,
                      cpucount, ramGb);

    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
    std::time_t cstart = std::chrono::system_clock::to_time_t(start);
    str += std::ctime(&cstart);
    str.pop_back();             // eat ctime newline

    return str.c_str();
}

int OL_GetCpuCount()
{
    return SDL_GetCPUCount();
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
    *pixelWidth = g_windowSize.x;
    *pixelHeight = g_windowSize.y;

    *pointWidth = g_windowSize.x / g_scaling_factor;
    *pointHeight = g_windowSize.y / g_scaling_factor;
}

void OL_SetWindowSizePoints(int w, int h)
{
    if (!g_displayWindow)
        return;
    SDL_SetWindowSize(g_displayWindow, w * g_scaling_factor, h * g_scaling_factor);
}

void OL_SetSwapInterval(int interval)
{
    int error = SDL_GL_SetSwapInterval(interval);
    if (interval < 0) {
        const int supports = error ? 0 : 1;
        if (supports != g_supportsTearControl) {
            ReportSDL("Tear Control %s Supported", supports ? "is" : "is NOT");
            g_supportsTearControl = supports;
        }
    }
}

int OL_HasTearControl(void)
{
    return g_supportsTearControl;
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
    memset(&img, 0, sizeof(img));
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
    //               texture_format, GL_UNSIGNED_BYTE, surface->pixels);
 
    SDL_FreeSurface(surface);
    glReportError();

    t.width = w;
    t.height = h;
    t.texnum = texture;

    return t;
}

int OL_SaveTexture(const OutlawTexture *tex, const char* fname)
{
    if (!tex || !tex->texnum || tex->width <= 0|| tex->height <= 0)
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
    if (!g_fontNames[fontName])
        return NULL;

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
    TTF_Font    *font;
    int          pixel_width;
    std::string  text;
};


int OL_StringTexture(OutlawTexture *tex, const char* str, float size, int _font, float maxw, float maxh)
{
    TTF_Font* font = getFont(_font, size);
    if (!font)
        return 0;

    TTF_Font *fallback_font = NULL;

    int text_pixel_width = 0;
    vector< Strip > strips;

    int strip_start = 0;
    int newlines = 0;
    bool last_was_fallback = false;
    int line_pixel_width = 0;

    size_t textlen = SDL_strlen(str);
    const size_t totallen = textlen;
    const char* text = str;

    // split string up by lines, fallback font
    while (1)
    {
        const size_t chr_start = totallen - textlen;        
        const Uint16 chr = textlen ? UTF8_getch(&text, &textlen) : '\0';
        const size_t chr_end = totallen - textlen;

        int pixel_width, pixel_height;

        if (chr == '\n' || chr == '\0' || chr == UNKNOWN_UNICODE)
        {
            Strip st = { font, -1, string(&str[strip_start], chr_start - strip_start) };
            if (st.text.size())
                TTF_SizeUTF8(font, st.text.c_str(), &pixel_width, &pixel_height);
            else
                pixel_width = 0;
            strips.push_back(std::move(st));

            text_pixel_width = max(text_pixel_width, line_pixel_width + pixel_width);
            strip_start = chr_end;
            newlines++;
            line_pixel_width = 0;

            if (chr == '\0' || chr == UNKNOWN_UNICODE)
                break;
        }
        else if (!TTF_GlyphIsProvided(font, chr))
        {
            if (!last_was_fallback && chr_start > strip_start) {
                string strip(&str[strip_start], chr_start - strip_start);
                TTF_SizeUTF8(font, strip.c_str(), &pixel_width, &pixel_height);
                line_pixel_width += pixel_width;
                Strip st = { font, pixel_width, strip };
                strips.push_back(std::move(st));
            }

            if (!fallback_font || !TTF_GlyphIsProvided(fallback_font, chr))
            {
                for (int j=0; j<arraySize(g_fontNames); j++)
                {
                    if (j == _font)
                        continue;
                    TTF_Font *fnt = getFont(j, size);
                    if (!fnt)
                        break;
                    if (TTF_GlyphIsProvided(fnt, chr))
                    {
                        fallback_font = fnt;
                        break;
                    }
                }
                if (!fallback_font)
                    fallback_font = font; // just use boxes, oh well...
            }

            const string c8(&str[chr_start], chr_end - chr_start);
            TTF_SizeUTF8(fallback_font, c8.c_str(), &pixel_width, &pixel_height);
            line_pixel_width += pixel_width;
            strip_start = chr_end;

            if (last_was_fallback && strips.back().font == fallback_font)
            {
                strips.back().pixel_width += pixel_width;
                strips.back().text += c8;
            }
            else
            {
                Strip st = { fallback_font, pixel_width, c8 };
                strips.push_back(std::move(st));
            }
            last_was_fallback = true;
        }
        else
        {
            last_was_fallback = false;
        }
    }

    const int font_pixel_height = max(TTF_FontLineSkip(font), fallback_font ? TTF_FontLineSkip(fallback_font) : 0);
    const int text_pixel_height = newlines * font_pixel_height;

    SDL_Surface *intermediary = SDL_CreateRGBSurface(0, text_pixel_width, text_pixel_height, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);

    SDL_Color color;
    memset(&color, 255, sizeof(color));

    SDL_Rect dstrect;
    memset(&dstrect, 0, sizeof(dstrect));

    foreach (const Strip &st, strips)
    {
        if (st.text.size())
        {
            SDL_Surface* initial = TTF_RenderUTF8_Blended(st.font, st.text.c_str(), color);
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

        if (st.pixel_width < 0.f) { 
            dstrect.y += font_pixel_height;
            dstrect.x = 0;
        } else {
            dstrect.x += st.pixel_width;
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
    
    if (keysym.mod & KMOD_SHIFT) {
        // a -> A
        if ('a' <= sym && sym <= 'z')
            return sym - 32;
        // 1 -> !
        switch (sym) {
        case SDLK_1: return SDLK_EXCLAIM;
        case SDLK_2: return SDLK_AT;
        case SDLK_3: return SDLK_HASH;
        case SDLK_4: return SDLK_DOLLAR;
        case SDLK_5: return SDLK_PERCENT;
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
    case SDLK_KP_0:     return Keypad0;
    case SDLK_KP_1:     return Keypad1;
    case SDLK_KP_2:     return Keypad2;
    case SDLK_KP_3:     return Keypad3;
    case SDLK_KP_4:     return Keypad4;
    case SDLK_KP_5:     return Keypad5;
    case SDLK_KP_6:     return Keypad6;
    case SDLK_KP_7:     return Keypad7;
    case SDLK_KP_8:     return Keypad8;
    case SDLK_KP_9:     return Keypad9;
    case SDLK_KP_ENTER: return '\r';
    case SDLK_KP_EQUALS: return '=';
    case SDLK_KP_PLUS:  return '+';
    case SDLK_KP_MINUS: return '-';
    case SDLK_KP_DIVIDE: return '/';
    case SDLK_KP_MULTIPLY: return '*';
    case SDLK_KP_PERIOD: return '.';
    case SDLK_RSHIFT:   // fallthrough
    case SDLK_LSHIFT:   return OShiftKey;
    case SDLK_CAPSLOCK: // fallthrough
    case SDLK_RCTRL:    // fallthrough
    case SDLK_LCTRL:    return OControlKey;
        //case SDLK_RMETA:    // fallthrough
        //case SDLK_LMETA:    // fallthrough
    case SDLK_RALT:     // fallthrough
    case SDLK_LALT:     return OAltKey;
    case SDLK_LGUI:     //fallthrough (windows / apple key)
    case SDLK_RGUI:     return OControlKey;
    case SDLK_BACKSPACE: return NSBackspaceCharacter;
    case SDLK_DELETE:   return NSDeleteFunctionKey;
    default:
        ASSERTF(sym < 0xffff, "%#x", sym);
        return sym;
    }
}


static void HandleEvents()
{
    SDL_Event evt;
    while (SDL_PollEvent(&evt))
    {
        if (Controller_HandleEvent(&evt))
            continue;

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
                g_windowSize.x = evt.window.data1;
                g_windowSize.y = evt.window.data2;
                glViewport(0, 0, g_windowSize.x, g_windowSize.y);
                ReportSDL("Window %d size changed to %dx%d", evt.window.windowID, 
                          evt.window.data1, evt.window.data2);
                break;
            case SDL_WINDOWEVENT_RESIZED:
                g_windowSize.x = evt.window.data1;
                g_windowSize.y = evt.window.data2;
                glViewport(0, 0, g_windowSize.x, g_windowSize.y);
                ReportSDL("Window %d resized to %dx%d", evt.window.windowID, 
                          evt.window.data1, evt.window.data2);
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
            e.y = (g_windowSize.y - evt.motion.y) / g_scaling_factor;
            const Uint8 state = evt.motion.state;
            const int key = ((state&SDL_BUTTON_LMASK)  ? 0 :
                             (state&SDL_BUTTON_RMASK)  ? 1 :
                             (state&SDL_BUTTON_MMASK)  ? 2 :
                             (state&SDL_BUTTON_X1MASK) ? 3 :
                             (state&SDL_BUTTON_X2MASK) ? 4 : -1);
            if (key == -1) {
                e.type = OLEvent::MOUSE_MOVED;
            } else {
                e.key = key;
                e.type = OLEvent::MOUSE_DRAGGED;
            }
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
            e.y = (g_windowSize.y - evt.button.y) / g_scaling_factor;
            e.type = evt.type == SDL_MOUSEBUTTONDOWN ? OLEvent::MOUSE_DOWN : OLEvent::MOUSE_UP;
            switch (evt.button.button)
            {
            case SDL_BUTTON_LEFT:   e.key = 0; break;
            case SDL_BUTTON_RIGHT:  e.key = 1; break;
            case SDL_BUTTON_MIDDLE: e.key = 2; break;
            case SDL_BUTTON_X1:     e.key = 3; break;
            case SDL_BUTTON_X2:     e.key = 4; break;
            default:                e.key = 0; break;
            }
            OLG_OnEvent(&e);
            break;
        }
        case SDL_QUIT:
            OLG_OnClose();
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

struct AutoreleasePool {
    std::mutex                                 mutex;
    std::map<std::thread::id, vector<string> > pool;

    static AutoreleasePool &instance() 
    {
        static AutoreleasePool p;
        return p;
    }

    const char* autorelease(std::string &val)
    {
        std::lock_guard<std::mutex> l(mutex);
        std::thread::id tid = std::this_thread::get_id();
        vector<string> &ref= pool[tid];
        ref.push_back(std::move(val));
        return ref.back().c_str();
    }

    void drain()
    {
        std::lock_guard<std::mutex> l(mutex);
        std::thread::id tid = std::this_thread::get_id();
        pool[tid].clear();
    }
    
};

const char* sdl_os_autorelease(std::string &val)
{
    return AutoreleasePool::instance().autorelease(val);
}

const char *OL_LoadFile(const char *name)
{
    const char *fname = OL_PathForFile(name, "r");

    SDL_RWops *io = SDL_RWFromFile(fname, "r");
    if (!io)
        return NULL;
    string buf = loadFile(io, fname);
    return sdl_os_autorelease(buf);
}

void OL_ThreadEndIteration(int i)
{
    AutoreleasePool::instance().drain();
}

void OL_WarpCursorPosition(float x, float y)
{
    SDL_WarpMouseInWindow(g_displayWindow, (int)x * g_scaling_factor, g_windowSize.y - (int) (y * g_scaling_factor));
}

const char* OL_ReadClipboard()
{
    char *ptr = SDL_GetClipboardText();
    if (!ptr)
        return NULL;
    string str = ptr;
#if OL_WINDOWS
    str_replace(ptr, "\r\n", "\n");
#endif
    SDL_free(ptr);
    return sdl_os_autorelease(str);
}

static void setupInitialResolution()
{
    g_windowSize.x = 960;
    g_windowSize.y = 600;

    const int displayCount = SDL_GetNumVideoDisplays();

    for (int i=0; i<displayCount; i++)
    {
        SDL_DisplayMode mode;
        SDL_GetDesktopDisplayMode(i, &mode);
        ReportSDL("Display %d of %d is %dx%d@%dHz", i+1, displayCount, mode.w, mode.h, mode.refresh_rate);

        if (i == 0)
            g_windowSize = int2(mode.w, mode.h);
        g_windowSize = min(g_windowSize, int2(0.9f * float2(mode.w, mode.h)));
    }
    ReportSDL("Initial window size is %dx%d", g_windowSize.x, g_windowSize.y);
}

#define COPY_GL_EXT_IMPL(X) if (!(X) && (X ## EXT)) { ReportSDL("Using " #X "EXT"); (X) = (X ## EXT); } else if (!(X)) { ReportSDL(#X " Not found!"); }
#define ASSERT_EXT_EQL(X) static_assert(X == X ## _EXT, #X "EXT mismatch")

static bool initGlew()
{
    glewExperimental = GL_TRUE;
    const GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        sdl_os_oncrash(str_format("Glew Error! %s", glewGetErrorString(err)).c_str());
        return 0;
    }
    // GL_EXT_framebuffer_blit
    COPY_GL_EXT_IMPL(glBlitFramebuffer);

    // GL_EXT_framebuffer_object
    ASSERT_EXT_EQL(GL_FRAMEBUFFER);
    ASSERT_EXT_EQL(GL_RENDERBUFFER);
    ASSERT_EXT_EQL(GL_DEPTH_ATTACHMENT);
    ASSERT_EXT_EQL(GL_COLOR_ATTACHMENT0);
    ASSERT_EXT_EQL(GL_FRAMEBUFFER_COMPLETE);
    ASSERT_EXT_EQL(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
    COPY_GL_EXT_IMPL(glBindFramebuffer);
    COPY_GL_EXT_IMPL(glBindRenderbuffer);
    COPY_GL_EXT_IMPL(glCheckFramebufferStatus);
    COPY_GL_EXT_IMPL(glDeleteFramebuffers);
    COPY_GL_EXT_IMPL(glDeleteRenderbuffers);
    COPY_GL_EXT_IMPL(glFramebufferRenderbuffer);
    COPY_GL_EXT_IMPL(glFramebufferTexture2D);
    COPY_GL_EXT_IMPL(glGenFramebuffers);
    COPY_GL_EXT_IMPL(glGenRenderbuffers);
    COPY_GL_EXT_IMPL(glGenerateMipmap);
    COPY_GL_EXT_IMPL(glIsFramebuffer);
    COPY_GL_EXT_IMPL(glIsRenderbuffer);
    COPY_GL_EXT_IMPL(glRenderbufferStorage);

    // make sure we print the gl version no matter what!
    const char* error = OLG_InitGL();
    if (error)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "OpenGL Error", error, NULL);
    }
    return 1;
}


int sdl_os_main(int argc, const char **argv)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER ) < 0)
    {
        sdl_os_oncrash(str_format("SDL Init failed: %s", SDL_GetError()).c_str());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    int mode = OLG_Init(argc, argv);

    if (!os_init())
        return 1;

    if (mode == 0)
    {
        SDL_Window *window = SDL_CreateWindow("OpenGL test", -32, -32, 32, 32, SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if (window) {
            SDL_GLContext context = SDL_GL_CreateContext(window);
            if (context) {
                if (initGlew() != 0)
                    return 1;

                OLG_Draw();

                SDL_GL_DeleteContext(context);
            }
            SDL_DestroyWindow(window);
        }
        return 0;
    }

    setupInitialResolution();
    g_displayWindow = SDL_CreateWindow(OLG_GetName(), 
                                       SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                       g_windowSize.x, g_windowSize.y,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    SDL_GetWindowPosition( g_displayWindow, &g_savedWindowPos.x, &g_savedWindowPos.y );
    SDL_GetWindowSize( g_displayWindow, &g_savedWindowPos.w, &g_savedWindowPos.h );

#if OL_LINUX
    const char* spath = OL_PathForFile("linux/reassembly_icon.png", "r");
    SDL_Surface *surface = IMG_Load(spath);
    if (surface)
    {
        SDL_SetWindowIcon(g_displayWindow, surface);
        SDL_FreeSurface(surface);
    }
    else
    {
        ReportSDL("Failed to load icon from %s", spath);
    }
#endif

    SDL_GLContext _glcontext = SDL_GL_CreateContext(g_displayWindow);
    UNUSED(_glcontext);
    
    if (!initGlew())
        return 1;
 
    SDL_ShowCursor(0);
    if (TTF_Init() != 0)
    {
        sdl_os_oncrash(str_format("TTF_Init() failed: %s", TTF_GetError()).c_str());
        return 1;
    }

    static const double kFrameTime = 1.0 / 60.0;

    while (!g_quitting)
    {
        const double start = OL_GetCurrentTime();
        HandleEvents();
        OLG_Draw();
        const double frameTime = max(0.0, OL_GetCurrentTime() - start);
        if (frameTime < kFrameTime) {
            std::this_thread::sleep_for(std::chrono::microseconds(int(round(1e6 * (kFrameTime - frameTime)))));
            // SDL_Delay((kFrameTime - frameTime) * 1000.0);
        }
    }

    OLG_OnQuit();

    if (g_logfile)
    {
        SDL_RWwrite(g_logfile, "\n", 1, 1);
        SDL_RWclose(g_logfile);
        g_logfile = NULL;

        if (g_wantsLogUpload)
        {
            string data;
            readUploadLog(data);
        }
    }

    fflush(NULL);

    SDL_DestroyWindow(g_displayWindow);

    TTF_Quit();
    SDL_Quit();

    ReportSDL("Good bye!\n");
    return 0;
}


// main.cpp - Outlaws.h platform implementation for linux (together with sdl_os.cpp)

#include "StdAfx.h"
#include "../sdl_os/sdl_os.h"

#include <ftw.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <dlfcn.h>
#include <link.h>

#include "../sdl_os/posix.h"
#include <X11/Xlib.h>

#define MAX_PATH 256

const char *g_binaryName = NULL;

// don't go through ReportMessagef/ReportMessage!
static void ReportLinux(const char *format, ...)
{
    va_list vl;
    va_start(vl, format);
    const string buf = "\n[Linux] " + str_vformat(format, vl);
    OL_ReportMessage(buf.c_str());
    va_end(vl);
}

string expandTilde(const string& path)
{
    if (path[0] == '~')
    {
        return str_path_join("/home", OL_GetUserName(), 
                             path.size() < 2 ? "" : path.c_str()+2);
    }
    return path;
}

static const string& getSaveDir()
{
    static string savedir;
    if (savedir.empty())
    {
        savedir = str_path_join(or_<const char*>(getenv("XDG_DATA_HOME"), "~/.local/share"), OLG_GetName());
        savedir = expandTilde(savedir);
        ASSERT(savedir[0] == '/');
    }
    return savedir;
}

static const string& getDataDir()
{
    static string datadir;
    if (datadir.empty())
    {
        datadir = str_path_join(str_dirname(g_binaryName), "..");
        char buf1[PATH_MAX];
        const char* path = realpath(datadir.c_str(), buf1);
        if (!path) {
            ReportLinux("realpath(%s) error: %s", datadir.c_str(), strerror(errno));
            return datadir;
        }
        datadir = path;
        datadir += "/";
    }
    return datadir;
}

static bool fileDirectoryPathExists(const char* fname)
{
    struct stat buf;
    if (stat(fname, &buf)) {
        if (errno == ENOENT)
            return false;
        ReportLinux("Error stating '%s': '%s'", fname, strerror(errno));
        return false;
    }

    return S_ISREG(buf.st_mode) || S_ISDIR(buf.st_mode);
}

int OL_FileDirectoryPathExists(const char* fname)
{
    const char *path = OL_PathForFile(fname, "r");
    return fileDirectoryPathExists(path);
}



const char *OL_PathForFile(const char *fname, const char* flags)
{
    ASSERT(fname);

    // absolute path
    if (!fname || fname[0] == '/')
        return fname;

    if (fname[0] == '~')
    {
        string p = expandTilde(fname);
        return sdl_os_autorelease(p);
    }

    string path = str_path_join(getSaveDir(), fname);
    if (!OLG_UseDevSavePath() && (flags[0] == 'w' || 
                                  flags[0] == 'a' || 
                                  fileDirectoryPathExists(path.c_str())))
    {
    }
    else
    {
        path = str_path_join(getDataDir(), fname);
    }
    return sdl_os_autorelease(path);
}

static int recursive_mkdir(const char *dir)
{
    char tmp[MAX_PATH];
    char *p = NULL;

    snprintf(tmp, sizeof(tmp),"%s",dir);
    size_t len = strlen(tmp);

    if(tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for(p = tmp + 1; *p; p++)
    {
        if(*p == '/')
        {
            *p = 0;
            if (mkdir(tmp, S_IRWXU) != 0 && errno != EEXIST) {
                ReportLinux("mkdir '%s' error: '%s'", tmp, strerror(errno));
                return 0;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, S_IRWXU) != 0 && errno != EEXIST) {
        ReportLinux("mkdir '%s' error: '%s'", tmp, strerror(errno));
        return 0;
    }
    return 1;
}

int os_create_parent_dirs(const char* path)
{
    return recursive_mkdir(str_dirname(path).c_str());
}

bool os_symlink_f(const char* source, const char* dest)
{
    int status = unlink(dest);
    if (status && status != ENOENT) {
        ReportLinux("Error unlink('%s'): %s", dest, strerror(errno));
    }
    if (symlink(source, dest)) {
        ReportLinux("Error symlink('%s', '%s'): %s", source, dest, strerror(errno));
        return false;
    }
    return true;
}

int OL_SaveFile(const char *name, const char* data, int size)
{
    const char* fname = OL_PathForFile(name, "w");
    os_create_parent_dirs(fname);
    
    string fnameb = string(fname) + ".b";

    FILE *f = fopen(fnameb.c_str(), "w");
    if (!f)
    {
        ReportLinux("error opening '%s' for writing\n", fnameb.c_str());
        return 0;
    }
    const int bytesWritten = fwrite(data, 1, size, f);
    if (bytesWritten != size)
    {
        ReportLinux("writing to '%s', wrote %d bytes of expected %d\n", fnameb.c_str(), bytesWritten, size);
        return 0;
    }
    if (fclose(f) != 0)
    {
        ReportLinux("error closing temp file from '%s': %s'\n", fnameb.c_str(), strerror(errno));
        return 0;
    }
    
    if (rename(fnameb.c_str(), fname) != 0)
    {
        ReportLinux("error renaming temp file from '%s' to '%s': %s'\n", fnameb.c_str(), fname, strerror(errno));
        return 0;
    }

    return 1;
}

int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int rv = remove(fpath);

    if (rv) {
        ReportLinux("Error removing '%s': %s", fpath, strerror(errno));
    }

    return rv;
}

int OL_RemoveFileOrDirectory(const char* dirname)
{
    const char *path = OL_PathForFile(dirname, "r");
    nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
    return 1;
}

const char** OL_ListDirectory(const char* path1)
{
    // not thread safe!!
    static vector<const char*> elements;
    const char* path = OL_PathForFile(path1, "r");
    DIR *dir = opendir(path);
    if (!dir) {
        // ReportLinux("Error opening directory '%s': %s", path, strerror(errno));
        return NULL;
    }
    elements.clear();
    dirent *entry = NULL;
    while ((entry = readdir(dir)))
    {
        if (entry->d_name[0] != '.')
            elements.push_back(lstring(entry->d_name).c_str());
    }
    closedir(dir);
    elements.push_back(NULL);
    return &elements[0];
}

string os_get_platform_info()
{
    utsname buf;
    if (uname(&buf))
        return "Linux (uname failed)";

    return str_format("%s %s %s %s", buf.sysname, buf.release, buf.version, buf.machine);
}

int OL_CopyFile(const char* pa, const char* pb)
{
    const char* ppa = OL_PathForFile(pa, "r");
    FILE *fa = fopen(ppa, "r");
    if (!fa) {
        ReportLinux("OL_CopyFile: can't open source file '%s'", ppa);
        return -1;
    }
    const char *ppb = OL_PathForFile(pb, "w");
    if (!os_create_parent_dirs(ppb))
        return -1;
    FILE *fb = fopen(ppb, "w");
    if (!fb) {
        ReportLinux("OL_CopyFile: can't open dest file '%s'", ppb);
        return -1;
    }
    
    char buf[1024];
    size_t n = 0;

    while ((n = fread(buf, sizeof(char), sizeof(buf), fa) > 0))
    {
        if (fwrite(buf, sizeof(char), n, fb) != n)
            return -1;
    }
    return 0;
}

const char* OL_GetUserName(void)
{
    const char* name = getenv("USERNAME");
    if (!name || !strlen(name))
        name = getenv("USER");
    return name;
}

int OL_OpenWebBrowser(const char* url)
{
    return system(str_format("xdg-open %s", url).c_str()) == 0;
}

struct SoCallbackData {
    string binname;
    int    idx;
};

static int so_callback(struct dl_phdr_info *info, size_t size, void *data)
{
    static const char *patterns[] = {
        "GL", "SDL", "openal", "libm.", "libstd", "libc.", "libvorbis", "libogg", "pthread",
        "drm", "gallium", "dri", "steam", "libz", "curl"
    };

    const char* name = info->dlpi_name;
    if (!name || name[0] == '\0')
        name = ((SoCallbackData*) data)->binname.c_str();

    bool found = false;
    foreach (const char* pat, patterns) {
        if (str_contains(info->dlpi_name, pat) || info->dlpi_name[0]  == '\0') {
            found = true;
            break;
        }
    }
    if (!found)
        return 0;

    for (int j = 0; j < info->dlpi_phnum; j++) 
    {
        const ElfW(Phdr) &phdr = info->dlpi_phdr[j];
        if (phdr.p_type == PT_LOAD && (phdr.p_flags&PF_X))
        {
            uintptr_t beg  = info->dlpi_addr + phdr.p_vaddr;
            size_t    size = phdr.p_memsz;

            int *idx = &((SoCallbackData*) data)->idx;
            ReportLinux("%2d. '%s' base address is %p, size is 0x%x", *idx, name, beg, (int)size);
            (*idx)++;
            return 0;
        }
    }
    return 0;
}

void dump_loaded_shared_objects()
{
    ReportLinux("Dumping loaded shared objects");
    SoCallbackData data = { str_basename(g_binaryName), 0 };
    dl_iterate_phdr(so_callback, &data);
}

void posix_oncrash(const char* msg)
{
    sdl_os_oncrash(msg);
}

static void* getSymbol(const char* module, const char* symbol)
{
    void *handle = dlopen(module, RTLD_NOW|RTLD_GLOBAL|RTLD_NOLOAD);
    if (!handle) {
        ReportLinux("Failed to access '%s': %s", module, dlerror());
        return NULL;
    }
    void* sym = dlsym(handle, symbol);
    char* error = NULL;
    if ((error = dlerror())) {
        ReportLinux("Failed to get symbol '%s' from '%s': %s", symbol, module, error);
        return NULL;
    }
    return sym;
}

#define GET_SYMBOL(MOD, RET, NAME, ARGS)                               \
    typedef RET (*fn ## NAME) ARGS;                                    \
    static fn ## NAME pfn ## NAME = (fn ## NAME) getSymbol(MOD, #NAME) \

static int X_error_handler(Display *d, XErrorEvent *e)
{
    char msg[256];
    GET_SYMBOL("libX11.so", int, XGetErrorText, (Display *display, int code, char *buffer_return, int length));
    if (pfnXGetErrorText)
        pfnXGetErrorText(d, e->error_code, msg, sizeof(msg));
    else
        msg[0] = '\0';
    ReportLinux("X11 Error %d (%s): request %d.%d", e->error_code, msg,
                e->request_code, e->minor_code);
    OL_ScheduleUploadLog("X11 Error");
    return 0;                   // apparently ignored?
}

static int X_IO_error_handler(Display *d)
{
    sdl_os_oncrash("X11 IO Error");
    exit(1);
}

int os_get_system_ram()
{
    // this call introduced in SDL 2.0.1 - backwards compatability
    GET_SYMBOL("libSDL2-2.0.so.0", int, SDL_GetSystemRAM, (void));
    return pfnSDL_GetSystemRAM ? pfnSDL_GetSystemRAM() : 0;
}

int os_init()
{
    GET_SYMBOL("libX11.so.6", XErrorHandler, XSetErrorHandler, (XErrorHandler));
    GET_SYMBOL("libX11.so.6", XIOErrorHandler, XSetIOErrorHandler, (XIOErrorHandler));

    ReportLinux("Setting XErrorHandler: %s", pfnXSetErrorHandler ? "OK" : "FAILED");
    if (pfnXSetErrorHandler)
        pfnXSetErrorHandler(X_error_handler);
    ReportLinux("Setting XIOErrorHandler: %s", pfnXSetIOErrorHandler ? "OK" : "FAILED");
    if (pfnXSetIOErrorHandler)
        pfnXSetIOErrorHandler(X_IO_error_handler);
    
    return 1;
}

int main(int argc, const char** argv)
{
    g_binaryName = argv[0];

    posix_set_signal_handler();

    try {
        return sdl_os_main(argc, argv);
    } catch (const std::exception &e) {
        ReportLinux("unhandled exception: %s", e.what());
        return 1;
    }
}

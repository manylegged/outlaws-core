
// main.cpp - Outlaws.h platform implementation for linux (together with sdl_os.cpp)

#include "StdAfx.h"
#include "../sdl_os/sdl_os.h"

#include <ftw.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <libgen.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <link.h>
#include <ucontext.h>

#define MAX_PATH 256

const char *g_binaryName = NULL;
int g_signaldepth = 0;

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
    char tmp[MAX_PATH];
    snprintf(tmp, sizeof(tmp),"%s", path);

    const char* dir = dirname(tmp);
    return recursive_mkdir(dir);
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

void os_errormessage1(const string& message, SDL_SysWMinfo *info)
{
    OL_ReportMessage(message.c_str());
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
        "GL", "SDL", "openal", "libm", "libstd", "libc", "vorbis", "ogg", "pthread",
        "drm", "gallium", "dri"
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


// from http://spin.atomicobject.com/2013/01/13/exceptions-stack-traces-c/
static const char* signal_to_string(int sig, siginfo_t *siginfo)
{
    switch(sig)
    {
    case SIGSEGV:
        switch(siginfo->si_code)
        {
        case SEGV_MAPERR: return "Caught SIGSEGV: Segmentation Fault (address not mapped to object)";
        case SEGV_ACCERR: return "Caught SIGSEGV: Segmentation Fault (invalid permissions for mapped object)";
        default: return ("Caught SIGSEGV: Segmentation Fault");
        }
    case SIGINT: return ("Caught SIGINT: Interactive attention signal, (usually ctrl+c)");
    case SIGFPE:
        switch(siginfo->si_code)
        {
        case FPE_INTDIV: return ("Caught SIGFPE: (integer divide by zero)");
        case FPE_INTOVF: return ("Caught SIGFPE: (integer overflow)");
        case FPE_FLTDIV: return ("Caught SIGFPE: (floating-point divide by zero)");
        case FPE_FLTOVF: return ("Caught SIGFPE: (floating-point overflow)");
        case FPE_FLTUND: return ("Caught SIGFPE: (floating-point underflow)");
        case FPE_FLTRES: return ("Caught SIGFPE: (floating-point inexact result)");
        case FPE_FLTINV: return ("Caught SIGFPE: (floating-point invalid operation)");
        case FPE_FLTSUB: return ("Caught SIGFPE: (subscript out of range)");
        default: return ("Caught SIGFPE: Arithmetic Exception");
        }
    case SIGILL:
        switch(siginfo->si_code)
        {
        case ILL_ILLOPC: return ("Caught SIGILL: (illegal opcode)");
        case ILL_ILLOPN: return ("Caught SIGILL: (illegal operand)");
        case ILL_ILLADR: return ("Caught SIGILL: (illegal addressing mode)");
        case ILL_ILLTRP: return ("Caught SIGILL: (illegal trap)");
        case ILL_PRVOPC: return ("Caught SIGILL: (privileged opcode)");
        case ILL_PRVREG: return ("Caught SIGILL: (privileged register)");
        case ILL_COPROC: return ("Caught SIGILL: (coprocessor error)");
        case ILL_BADSTK: return ("Caught SIGILL: (internal stack error)");
        default: return ("Caught SIGILL: Illegal Instruction");
        }
    case SIGTERM: return ("Caught SIGTERM: a termination request was sent to the program");
    case SIGABRT: return ("Caught SIGABRT: usually caused by an abort() or assert()");
    default: return "Caught UNKNOWN";
    }
}

static void print_backtrace()
{
    static const int maxbuf = 128;
    void* buffer[maxbuf];
    memset(buffer, 0, sizeof(buffer));
    const int count = backtrace(buffer, maxbuf);

    const pthread_t current_tid = pthread_self();
    ReportLinux("Dumping stack for thread %#llx '%s'", (uint64)current_tid, map_get(_thread_name_map(), (uint64)current_tid).c_str());
    for (int i=0; i<count; i++)
        ReportLinux("%2d. Called from %p", i, buffer[i]);
}

void os_stacktrace()
{
    const int saved_depth = g_signaldepth;
    if (g_signaldepth == 0) {
        g_signaldepth = 1;
    }

    ReportLinux("Dumping loaded shared objects");
    SoCallbackData data = { str_basename(g_binaryName), 0 };
    dl_iterate_phdr(so_callback, &data);
    
    print_backtrace();
    fflush(NULL);

    const pthread_t current_tid = pthread_self();
#if 0
    ReportLinux("Stopping %d threads", (int)_thread_name_map().size()-1);
    foreach (const auto &x, _thread_name_map())
    {
        if (!x.first || x.first == current_tid)
            continue;
        int status;
        if ((status = pthread_kill((pthread_t) x.first, SIGSTOP)))
            ReportLinux("pthread_kill(%#llx, SIGSTOP) failed: %s", x.first, strerror(status));
    }
#endif
    ReportLinux("Observed %d threads from %#llx '%s'",
                (int)_thread_name_map().size(), (uint64)current_tid, 
                map_get(_thread_name_map(), (uint64)current_tid).c_str());
    foreach (const auto &x, _thread_name_map())
    {
        if (!x.first || x.first == current_tid)
            continue;
        ReportLinux("Sending SIGTERM to thread %#llx, '%s'", x.first, x.second.c_str());
        int status;
        if ((status = pthread_kill((pthread_t) x.first, SIGTERM)))
            ReportLinux("pthread_kill(%#llx, SIGTERM) failed: %s", x.first, strerror(status));
    }
    sleep(1);                   // wait for other threads to print
    ReportLinux("Handling thread done backtracing");
    g_signaldepth = saved_depth;
}

static void posix_signal_handler(int sig, siginfo_t *siginfo, void *context)
{
    g_signaldepth++;
    if (g_signaldepth > 2)
    {
        if (g_signaldepth == 3)
            ReportLinux("Recursive Signal Handler detected - returning");
        return;
    }
    else if (g_signaldepth == 2)
    {
        print_backtrace();
        return;
    }
    
    puts("\nsignal handler called");
    fflush(NULL);
    ReportLinux("%s (signal %d)", signal_to_string(sig, siginfo), sig);

    const ucontext_t *ctx = (ucontext_t*)context;
    const mcontext_t &mcontext = ctx->uc_mcontext;

    if (sig == SIGILL || sig == SIGFPE || sig == SIGSEGV || sig == SIGBUS || sig == SIGTRAP)
    {
        const greg_t ecode = mcontext.gregs[REG_ERR];
        ReportLinux("Invalid %s to %p", (ecode&4) ? "Exec" : (ecode&2) ? "Write" : "Read", siginfo->si_addr);
    }
    
#ifdef __LP64__
    ReportLinux("PC/RIP: %#llx SP/RSP: %#llx, FP/RBP: %#llx", mcontext.gregs[REG_RIP], mcontext.gregs[REG_RSP], mcontext.gregs[REG_RBP]);
#else
    ReportLinux("PC/EIP: %#x SP/ESP: %#x, FP/EBP: %#x", mcontext.gregs[REG_EIP], mcontext.gregs[REG_ESP], mcontext.gregs[REG_EBP]);
#endif
    
    if (sig == SIGTERM || sig == SIGINT)
    {
        OLG_OnClose();
        OL_DoQuit();
        g_signaldepth--;
        return;
    }

    os_stacktrace();
    g_signaldepth--;
    
    sdl_os_oncrash("Spacetime Segmentation Fault Detected\n(Reassembly crashed)\n");
    exit(1);
}


void set_signal_handler()
{
#if 0
    /* setup alternate stack */
    {
        static uint8_t alternate_stack[SIGSTKSZ];

        stack_t ss = {};
        /* malloc is usually used here, I'm not 100% sure my static allocation
           is valid but it seems to work just fine. */
        ss.ss_sp = (void*)alternate_stack;
        ss.ss_size = SIGSTKSZ;
        ss.ss_flags = 0;
 
        if (sigaltstack(&ss, NULL) != 0) {
            ReportLinux("signalstack failed: %s", strerror(errno));
        }
    }
#endif
 
    /* register our signal handlers */
    {
        struct sigaction sig_action = {};
        sig_action.sa_sigaction = posix_signal_handler;
        sigemptyset(&sig_action.sa_mask);
 
#ifdef __APPLE__
        /* for some reason we backtrace() doesn't work on osx
           when we use an alternate stack */
        sig_action.sa_flags = SA_SIGINFO;
#else
        sig_action.sa_flags = SA_SIGINFO;// | SA_ONSTACK;
#endif
 
        const int signals[] = { SIGSEGV, SIGFPE, SIGINT, SIGILL, SIGTERM, SIGABRT };
        foreach (int sig, signals) {
            if (sigaction(sig, &sig_action, NULL) != 0) {
                ReportLinux("sigaction failed: %s", strerror(errno));
            }
        }
    }
}

int os_init()
{
    // copy icon to icon dir - does not reliably cause desktop file to have icon
    if (0)
    {
        const char* name = "reassembly_icon.png";
        string src = str_format("linux/%s", name);
        string dest = str_format("~/.local/share/icons/hicolor/128x128/apps/%s", name);
        int status = 1;
        if (!OL_FileDirectoryPathExists(dest.c_str()))
        {
            status = OL_CopyFile(src.c_str(), dest.c_str());
        }
        ReportLinux("Copied icon to %s: %s", dest.c_str(), 
                    status == 1 ? "ALREADY OK" : status == 0 ? "OK" : "FAILED");
    }
    return 1;
}

int main(int argc, const char** argv)
{
    g_binaryName = argv[0];

    set_signal_handler();

    try {
        return sdl_os_main(argc, argv);
    } catch (const std::exception &e) {
        ReportLinux("unhandled exception: %s", e.what());
        return 1;
    }
}


// Win32Main.cpp - Outlaws.h platform implementation for windows (together with sdl_os.cpp)

#include "StdAfx.h"

#include <stdio.h>
#include <inttypes.h>

#include "SDL_syswm.h"

#include "../sdl_os/sdl_os.h"
#include "Graphics.h"
#include "Shaders.h"

#include "Shlwapi.h"
#pragma comment(lib, "Shlwapi.lib")

#include <Shellapi.h>
#pragma comment(lib, "Shell32.lib")

#include <Shlobj.h>
#include <KnownFolders.h>

// timeBeginPeriod
#include <Mmsystem.h>
#pragma comment(lib, "Winmm.lib")

// GetModuleInformation
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")

// StackWalk
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")

// iswow64process
// #include <Wow64apiset.h>

// UNLEN
#include "Lmcons.h"

string ws2s(const std::wstring& wstr)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wstr.length(), 0, 0, NULL, NULL);
    std::string r(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wstr.length(), &r[0], len, NULL, NULL);
    return r;
}

std::wstring s2ws(const std::string& s)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), s.length(), 0, 0);
    std::wstring r(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), s.length(), &r[0], len);
    return r;
}


static const wchar_t* canonicalizePath(const wchar_t* inpt)
{
    static wchar_t buf[MAX_PATH];
    memset(buf, 0, sizeof(buf));
    // copy, remove ..
    const wchar_t* rptr = inpt;
    wchar_t* wptr = buf;
    while (*rptr != L'\0') {
        if (*rptr == L'.' && *(rptr - 1) == L'.') {
            while (*wptr-- != L'\\' && wptr>buf);
            while (*wptr-- != L'\\' && wptr>buf);
            rptr++;
            wptr++;
        }
        if (*rptr == L'/')
            *wptr = L'\\';
        else
            *wptr = *rptr;
        wptr++;
        rptr++;
    }
    *wptr = L'\0';
    return buf;
}

static std::wstring getDirname(const wchar_t *inpt)
{
    wchar_t driv[_MAX_DRIVE];
    wchar_t dir[_MAX_DIR];
    wchar_t fname[_MAX_FNAME];
    wchar_t ext[_MAX_EXT];
    _wsplitpath(inpt, driv, dir, fname, ext);

    return std::wstring(driv) + dir;
}

// don't go through ReportMessagef/ReportMessage!
static void ReportWin32(const char *format, ...)
{
    va_list vl;
    va_start(vl, format);
    string buf = "\n[win32] " + str_vformat(format, vl);
    while (buf.back() == '\n')
        buf.pop_back();
    OL_ReportMessage(buf.c_str());
    va_end(vl);
}

static const std::wstring& getDataDir()
{
    static std::wstring str;
    if (str.empty())
    {
        wchar_t binname[MAX_PATH];
        GetModuleFileName(NULL, binname, MAX_PATH);
        str = getDirname(binname) + L"..\\";
        str = canonicalizePath(str.c_str());

        ReportWin32("Data Directory is %s", ws2s(str).c_str());
    }
    return str;
}

void ReportWin32Err1(const char *msg, DWORD dwLastError, const char* file, int line)
{
    if (dwLastError == 0)
        return;                 // Don't want to see a "operation done successfully" error ;-)
    wchar_t lpBuffer[256] = L"?";
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, dwLastError,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  lpBuffer, (sizeof(lpBuffer)/sizeof(wchar_t)) - 1, NULL);
    const std::string buf = str_strip(ws2s(lpBuffer));
    ReportWin32("%s:%d:error: %s failed: %#x %s", file, line, msg, dwLastError, buf.c_str());
}

#define ReportWin32Err(msg, err) ReportWin32Err1(msg, err, __FILE__, __LINE__)

const char* OL_GetUserName(void)
{
    std::wstring buf(UNLEN +1, ' ');
    DWORD size = buf.size();
    if (!GetUserName(&buf[0], &size) || size == 0)
    {
        ReportWin32Err("GetUserName", GetLastError());
        return "Unknown";
    }

    buf.resize(size-1);
    return sdl_os_autorelease(ws2s(buf));
}

static FARPROC GetModuleAddr(LPCTSTR modName, LPCSTR procName)
{
    HMODULE module = 0;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           modName, &module))
    {
        ReportWin32Err("GetModuleHandleEx", GetLastError());
        return NULL;
    }
    FARPROC proc = GetProcAddress(module, procName);
    if (!proc)
        ReportWin32Err("GetProcAddress", GetLastError());
    return proc;
}

#define IF_STR(ARG, NAME) if (ARG == NAME) return #NAME

const char* knownFolderIdToString(REFKNOWNFOLDERID fid)
{
    IF_STR(fid, FOLDERID_Desktop);
    else IF_STR(fid, FOLDERID_Downloads);
    else IF_STR(fid, FOLDERID_SavedGames);
    else return "FOLDERID_Unknown";
}

const char* csidlToString(int fid)
{
    switch (fid)
    {
        CASE_STR(CSIDL_PERSONAL);
        CASE_STR(CSIDL_DESKTOPDIRECTORY);
    }
    return "CSIDL_Unknown";
}

std::wstring getKnownPath(REFKNOWNFOLDERID fid)
{
    typedef HRESULT (WINAPI *fnSHGetKnownFolderPath)(
        _In_      REFKNOWNFOLDERID rfid,
        _In_      DWORD dwFlags,
        _In_opt_  HANDLE hToken,
        _Out_     PWSTR *ppszPath);

    static fnSHGetKnownFolderPath pSHGetKnownFolderPath = 
        (fnSHGetKnownFolderPath) GetModuleAddr(L"shell32.dll", "SHGetKnownFolderPath");

    if (pSHGetKnownFolderPath)
    {
        LPWSTR path = NULL;
        if (pSHGetKnownFolderPath(fid, 0, NULL, &path) == S_OK)
            return path;
        else
            ReportWin32Err(str_format("SHGetKnownFolderPath(%s)", knownFolderIdToString(fid)).c_str(),
                           GetLastError());
    }
    else
    {
        ReportWin32("SHGetKnownFolderPath not found, falling back on SHGetFolderPath");
    }
    
    int csidl = CSIDL_PERSONAL;
    if (fid == FOLDERID_Downloads || fid == FOLDERID_Desktop)
        csidl = CSIDL_DESKTOPDIRECTORY;

    wchar_t szPath[MAX_PATH];
    HRESULT res = SHGetFolderPath(NULL, csidl, NULL, 0, szPath);
    if (SUCCEEDED(res))
        return szPath;

    ReportWin32Err(str_format("SHGetFolderPath(%s)", csidlToString(csidl)).c_str(), res);
    return std::wstring();
}

static const std::wstring& getSaveDir()
{
    static std::wstring str;
    if (str.empty())
    {
        std::wstring path = getKnownPath(FOLDERID_SavedGames);
        if (path.empty())
            return getDataDir();
        str = path + L'\\' + s2ws(OLG_GetName()) + L'\\';

        if (!PathFileExists(str.c_str()))
        {
#if 0
            std::wstring opath = std::wstring(path) + L"\\Outlaws\\";
            if (PathFileExists(opath.c_str()))
            {
                BOOL success = MoveFileEx(opath.c_str(), str.c_str(), MOVEFILE_COPY_ALLOWED|MOVEFILE_WRITE_THROUGH);

                ReportWin32("Moved save games from %s to %s: %s",
                            ws2s(opath).c_str(), ws2s(str).c_str(), success ? "OK" : "FAILED");
                if (!success)
                    ReportWin32Err("MoveFileEx", GetLastError());
            }
#endif
        }
    }
    return str;
}

int OL_CopyFile(const char* source, const char* dest)
{
    string dpath = OL_PathForFile(dest, "w");
    os_create_parent_dirs(dpath.c_str());
    if (!CopyFile(s2ws(OL_PathForFile(source, "r")).c_str(), 
                  s2ws(dpath).c_str(), FALSE))
    {
        ReportWin32Err("CopyFile", GetLastError());
        return -1;
    }

    return 0;
}

static bool DirectoryExists(const wchar_t* szPath)
{
    DWORD dwAttrib = GetFileAttributes(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
            (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static std::wstring getTildePath(const char* fname, const char* path, REFKNOWNFOLDERID fid)
{
    if (!str_startswith(fname, path))
        return L"";
    std::wstring npath = getKnownPath(fid);
    if (npath.empty())
        return L"";
    npath += s2ws(fname + strlen(path));
    return canonicalizePath(npath.c_str());
}

std::wstring pathForFile(const char *fname, const char* flags)
{
    std::wstring cpath;
    cpath = getTildePath(fname, "~/Desktop", FOLDERID_Desktop);
    if (cpath.size())
        return cpath;

    cpath = getTildePath(fname, "~/Downloads", FOLDERID_Downloads);
    if (cpath.size())
        return cpath;

    cpath = canonicalizePath(s2ws(fname).c_str());

    // absolute path
    if (cpath.size() > 2 && cpath[1] == ':')
        return cpath;

    const std::wstring savepath = getSaveDir() + cpath;
    bool spexists = false;
    if (!OLG_UseDevSavePath() && (flags[0] == 'w' || 
                                  flags[0] == 'a' || 
                                  (spexists = PathFileExists(savepath.c_str()))))
    {
        return savepath;
    }

    std::wstring respath = getDataDir() + cpath;
    return respath;
}

const char *OL_PathForFile(const char *fname, const char* flags)
{
    std::wstring path = pathForFile(fname, flags);
    return path.size() ? sdl_os_autorelease(ws2s(path)) : NULL;
}

int OL_FileDirectoryPathExists(const char* fname)
{
    std::wstring path = pathForFile(fname, "r");
    return PathFileExists(path.c_str());
}

const char** OL_ListDirectory(const char* path1)
{
    // not thread safe!!
    static vector<const char*> elements;
    
    const std::wstring path = pathForFile(path1, "r");

    WIN32_FIND_DATA data;
    memset(&data, 0, sizeof(data));

    HANDLE hdir = FindFirstFile((path + L"\\*").c_str(), &data);
    if (hdir == INVALID_HANDLE_VALUE) {
        ReportWin32Err("FindFirstFile", GetLastError());
        return NULL;
    }

    elements.clear();

    do
    {
        if (!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
        {
            elements.push_back(lstring(ws2s(data.cFileName)).c_str());
        }
    } while (FindNextFile(hdir, &data) != 0);

    FindClose(hdir);
    elements.push_back(NULL);
    return &elements[0];
}

int os_create_parent_dirs(const char* path_)
{
    const std::wstring path = s2ws(path_);
    const std::wstring dirname = getDirname(path.c_str());

    DWORD res = SHCreateDirectoryEx(NULL, dirname.c_str(), NULL);
    if (res != ERROR_SUCCESS && 
        res != ERROR_FILE_EXISTS &&
        res != ERROR_ALREADY_EXISTS)
    {
        ReportWin32Err(str_format("SHCreateDirectoryEx('%s')", ws2s(dirname).c_str()).c_str(), res);
        return 0;
    }
    return 1;
}

bool os_symlink_f(const char* source, const char* dest)
{
    std::wstring wdest = s2ws(dest);
    std::wstring wsrc = s2ws(source);
    
    DeleteFile(wdest.c_str());

#if 0
    // requires stupid access privileges
    typedef BOOLEAN (WINAPI *pfnCreateSymbolicLink)(
        _In_  LPTSTR lpSymlinkFileName,
        _In_  LPTSTR lpTargetFileName,
        _In_  DWORD dwFlags);
    static pfnCreateSymbolicLink pCreateSymbolicLink =
        (pfnCreateSymbolicLink) GetModuleAddr(L"kernel32.dll", "CreateSymbolicLinkW");
    if (!pCreateSymbolicLink)
        return false;

    BOOLEAN status = pCreateSymbolicLink(const_cast<LPTSTR>(wdest.c_str()),
                                         const_cast<LPTSTR>(wsrc.c_str()), 0x0);
    if (!status)
        ReportWin32Err("CreateSymbolicLink", GetLastError());
#else
    BOOL status = CreateHardLink(wdest.c_str(), wsrc.c_str(), NULL);
    if (!status)
        ReportWin32Err("CreateHardLink", GetLastError());
#endif
    return status ? true : false;
}

int OL_SaveFile(const char *name, const char* data, int size)
{
    const char* fname = OL_PathForFile(name, "w");
    const string fnameb = string(fname) + ".b";

    if (!os_create_parent_dirs(fname))
        return 0;

    SDL_RWops *io = SDL_RWFromFile(fnameb.c_str(), "w");

    if (!io)
    {
        ReportWin32("error opening '%s' for writing", fnameb.c_str());
        return 0;
    }

    // translate newlines
#if 1
    string data1;
    for (const char* ptr=data; *ptr != '\0'; ptr++) {
        if (*ptr == '\n')
            data1 += "\r\n";
        else
            data1 += *ptr;
    }
#else
    string data1 = data;
#endif

    const int bytesWritten = SDL_RWwrite(io, data1.c_str(), sizeof(char), data1.size());
    if (bytesWritten != data1.size())
    {
        ReportWin32("writing to '%s', wrote %d bytes of expected %d", fnameb.c_str(), bytesWritten, data1.size());
        return 0;
    }
    if (SDL_RWclose(io) != 0)
    {
        ReportWin32("error closing temp file from '%s': %s'", fnameb.c_str(), SDL_GetError());
        return 0;
    }
    
    if (!MoveFileEx(s2ws(fnameb).c_str(), s2ws(fname).c_str(), MOVEFILE_REPLACE_EXISTING))
    {
		ReportWin32Err("MoveFileEx", GetLastError());
        ReportWin32("Failed to write %s", fname);
        return 0;
    }

    return 1;
}

int OL_RemoveFileOrDirectory(const char* dirname)
{
	const char* path = OL_PathForFile(dirname, "w");
    ReportWin32("RemoveFileOrDirectory '%s'", path);
    fflush(NULL);
    std::wstring buf = s2ws(path);
    buf.push_back(L'\0');
    buf.push_back(L'\0');
    SHFILEOPSTRUCT v;
    memset(&v, 0, sizeof(v));
    v.wFunc = FO_DELETE;
    v.pFrom = buf.c_str();
    v.fFlags = FOF_NO_UI;
    int val = SHFileOperation(&v);
    if (val != 0) {
        ReportWin32Err("SHFileOperation(FO_DELETE)", val);
    }
	return val == 0 ? 1 : 0;
}

int OL_OpenWebBrowser(const char* url)
{
    int stat = (int)ShellExecute(NULL, L"open", s2ws(url).c_str(), NULL, NULL, SW_SHOWNORMAL);
    return stat > 32 ? 1 : 0;
}


// enable optimus!
extern "C" {
    _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}

struct UntypedException {
  UntypedException(const EXCEPTION_RECORD & er)
    : exception_object(reinterpret_cast<void *>(er.ExceptionInformation[1])),
      type_array(reinterpret_cast<_ThrowInfo *>(er.ExceptionInformation[2])->pCatchableTypeArray)
  {}
  void * exception_object;
  _CatchableTypeArray * type_array;
};
 
void * exception_cast_worker(const UntypedException & e, const type_info & ti) {
  for (int i = 0; i < e.type_array->nCatchableTypes; ++i) {
    _CatchableType & type_i = *e.type_array->arrayOfCatchableTypes[i];
    const std::type_info & ti_i = *reinterpret_cast<std::type_info *>(type_i.pType);
    if (ti_i == ti) {
      char * base_address = reinterpret_cast<char *>(e.exception_object);
      base_address += type_i.thisDisplacement.mdisp;
      return base_address;
    }
  }
  return 0;
}
 
template <typename T>
T * exception_cast(const UntypedException & e) {
  const std::type_info & ti = typeid(T);
  return reinterpret_cast<T *>(exception_cast_worker(e, ti));
}

#define PRINT_EXP(X) case EXCEPTION_ ## X: return #X;

static const char* getExceptionCodeName(const EXCEPTION_RECORD *rec)
{
    DWORD code = rec->ExceptionCode;
    static string str;

    switch(code)
    {
        PRINT_EXP(ACCESS_VIOLATION);
        PRINT_EXP(DATATYPE_MISALIGNMENT);
        PRINT_EXP(BREAKPOINT);
        PRINT_EXP(SINGLE_STEP);
        PRINT_EXP(ARRAY_BOUNDS_EXCEEDED);
        PRINT_EXP(FLT_DENORMAL_OPERAND);
        PRINT_EXP(FLT_DIVIDE_BY_ZERO);
        PRINT_EXP(FLT_INEXACT_RESULT);
        PRINT_EXP(FLT_INVALID_OPERATION);
        PRINT_EXP(FLT_OVERFLOW);
        PRINT_EXP(FLT_STACK_CHECK);
        PRINT_EXP(FLT_UNDERFLOW);
        PRINT_EXP(INT_DIVIDE_BY_ZERO);
        PRINT_EXP(INT_OVERFLOW);
        PRINT_EXP(PRIV_INSTRUCTION);
        PRINT_EXP(IN_PAGE_ERROR);
        PRINT_EXP(ILLEGAL_INSTRUCTION);
        PRINT_EXP(NONCONTINUABLE_EXCEPTION);
        PRINT_EXP(STACK_OVERFLOW);
        PRINT_EXP(INVALID_DISPOSITION);
        PRINT_EXP(GUARD_PAGE);
        PRINT_EXP(INVALID_HANDLE);
    case DBG_CONTROL_C: return "DBG_CONTROL_C";
    case 0xE06D7363: {
        UntypedException ue(*rec);
        if (std::exception * e = exception_cast<std::exception>(ue)) {
            const std::type_info & ti = typeid(*e);
            str = str_format("%s(\"%s\")", ti.name(), e->what());
            return str.c_str();
        } else {
            return "Unknown Cxx Exception";
        }
    }
    default: {
        str = str_format("UNKNOWN(%#d)", code);
        return str.c_str();
    }
    }
}

static void printStack(HANDLE thread, CONTEXT &context)
{
    STACKFRAME64 frame;
    DWORD image;
    memset(&frame, 0, sizeof(STACKFRAME64));
#ifdef _M_IX86
    image = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context.Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Esp;
    frame.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
    image = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
    image = IMAGE_FILE_MACHINE_IA64;
    frame.AddrPC.Offset = context.StIIP;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.IntSp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrBStore.Offset = context.RsBSP;
    frame.AddrBStore.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.IntSp;
    frame.AddrStack.Mode = AddrModeFlat;
#else
#error "This platform is not supported."
#endif

    const HANDLE process = GetCurrentProcess();

    int i=0;
    while (StackWalk64(image, process, thread, &frame, &context, NULL, NULL, NULL, NULL))
    {
        ReportWin32("%2d. called from 0x%p", i, (void*)frame.AddrPC.Offset);
        i++;
    }
}

void printModulesStack(CONTEXT *ctx)
{
    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
    std::time_t cstart = std::chrono::system_clock::to_time_t(start);
    ReportWin32("Time is %s", std::ctime(&cstart));

    fflush(NULL);

    ReportWin32("Dumping loaded modules");

    const HANDLE process = GetCurrentProcess();

    static const int kMaxModules = 500;
    HMODULE hmodules[kMaxModules];
    DWORD  moduleBytesNeeded = 0;
    if (EnumProcessModules(process, hmodules, sizeof(hmodules), &moduleBytesNeeded))
    {
        const int modules = min(kMaxModules, (int) (moduleBytesNeeded / sizeof(HMODULE)));
        for (int i=0; i<modules; i++)
        {
            MODULEINFO module_info;
            memset(&module_info, 0, sizeof(module_info));
            if (GetModuleInformation(process, hmodules[i], &module_info, sizeof(module_info)))
            {
                const DWORD module_size = module_info.SizeOfImage;
                const BYTE * module_ptr = (BYTE*)module_info.lpBaseOfDll;

                wchar_t basename[MAX_PATH];
                memset(basename, 0, sizeof(basename));
                GetModuleBaseName(process, hmodules[i], basename, MAX_PATH);

                const std::string name = ws2s(basename); 
                const std::string lname = str_tolower(name);

                // only print dlls matching these patterns
                static const char *substrs[] = {
                    ".exe",
                    "ntdll", "kernel", "shell32", "dbghelp",
                    "msvc",
                    "opengl", "glew", "glu", "ddraw",
                    "sdl2", "openal", "zlib", "freetype", "curl",
                    "ogl", // nvoglv32.dll and atioglxx.dll
                    "igd", // intel drivers
                    "steam",
                };
                foreach (const char* str, substrs)
                {
                    if (str_contains(lname, str))
                    {
                        ReportWin32("%2d. '%s' base address is 0x%p, size is %#x", 
                                       i, name.c_str(), module_ptr, module_size);
                        break;
                    }
                }
            }
        }
    }

    const DWORD current_tid = GetCurrentThreadId();

    ReportWin32("Dumping stack for current thread %#x, '%s'", 
                   current_tid, _thread_name_map()[current_tid].c_str());

    CONTEXT context = *ctx;
    printStack(GetCurrentThread(), context);
    fflush(NULL);

    foreach (const auto &x, _thread_name_map())
    {
        if (!x.first || x.first == current_tid)
            continue;
        ReportWin32("Dumping stack for thread %#x, '%s'", x.first, x.second.c_str());
        HANDLE hthread = OpenThread(THREAD_GET_CONTEXT|THREAD_SUSPEND_RESUME|THREAD_QUERY_INFORMATION,
                                    FALSE, x.first);
        if (!hthread) {
            ReportWin32Err("OpenThread", GetLastError());
            continue;
        }
        if (SuspendThread(hthread) == -1) {
            ReportWin32Err("SuspendThread", GetLastError());
            continue;
        }

        memset(&context, 0, sizeof(context));
        context.ContextFlags = (CONTEXT_FULL);
        if (GetThreadContext(hthread, &context)) {
            printStack(hthread, context);
        } else {
            ReportWin32Err("GetThreadContext", GetLastError());
            continue;
        }
    }
}

void OL_OnTerminate(const char* message)
{
    CONTEXT context;
    memset(&context, 0, sizeof(context));
    RtlCaptureContext(&context);
    
    printModulesStack(&context);

    sdl_os_oncrash(str_format("Spacetime Terminated: %s\n(Reassembly crashed)", message));
}

static LONG WINAPI myExceptionHandler(EXCEPTION_POINTERS *info)
{
    fflush(NULL);
    ReportWin32("Unhandled Top Level Exception");
    const EXCEPTION_RECORD *rec = info->ExceptionRecord;

    string msg = str_format("Code: %s, Flags: %#x, PC: 0x%p",
                            getExceptionCodeName(rec),
                            rec->ExceptionFlags, 
                            (void*)rec->ExceptionAddress);
    ReportWin32("%s", msg.c_str());
    
    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
        rec->ExceptionCode == EXCEPTION_IN_PAGE_ERROR)
    {
        const ULONG_PTR type = rec->ExceptionInformation[0];
        const ULONG_PTR addr = rec->ExceptionInformation[1];
        const char *stype = type == 0 ? "Read" :
                            type == 1 ? "Write" :
                            type == 8 ? "Exec" : "Unknown";
        const string msg2 = str_format("Invalid %s to 0x%p", stype, (void*)addr);
        ReportWin32("%s", msg2.c_str());
        msg += "\n" + msg2;
    }

    printModulesStack(info->ContextRecord);

    sdl_os_oncrash(str_format("Spacetime Segfault:\n%s", msg.c_str()));
	SteamAPI_WriteMiniDump(rec->ExceptionCode, info, 0);
    return EXCEPTION_EXECUTE_HANDLER;
}

static bool verifyOsVersion(const DWORD major, const DWORD minor)
{
	OSVERSIONINFOEXW osvi;
	DWORDLONG dwlConditionMask = 0;

	//Initialize the OSVERSIONINFOEX structure
	memset(&osvi, 0, sizeof(OSVERSIONINFOEXW));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
	osvi.dwMajorVersion = major;
	osvi.dwMinorVersion = minor;
	osvi.dwPlatformId = VER_PLATFORM_WIN32_NT;

	//Initialize the condition mask
	VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION(dwlConditionMask, VER_PLATFORMID, VER_EQUAL);

	// Perform the test
	return VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_PLATFORMID, dwlConditionMask);
}

string os_get_platform_info()
{
    OSVERSIONINFO osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    GetVersionEx(&osvi);

    DWORD major = osvi.dwMajorVersion;
    DWORD minor = osvi.dwMinorVersion;
    //Determine the real *major* version first
    while (verifyOsVersion(major+1, 0)) {
        major++;
        minor = 0;
    }
    while (verifyOsVersion(major, minor+1)) {
        minor++;
    }

    const char* name = NULL;
    if (major == 5 && minor == 1)
        name = "XP";
    else if (major == 6 && minor == 0)
        name = "Vista";
    else if (major == 6 && minor == 1)
        name = "7";
    else if (major == 6 && minor == 2)
        name = "8";
    else if (major == 6 && minor == 3)
        name = "8.1";
    else
        name = "Unknown";

    int bitness = 32;
    typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
    static LPFN_ISWOW64PROCESS fnIsWow64Process =
        (LPFN_ISWOW64PROCESS) GetModuleAddr(L"kernel32", "IsWow64Process");
    BOOL is64 = false;
    if (fnIsWow64Process && fnIsWow64Process(GetCurrentProcess(), &is64) && is64) {
        bitness = 64;
    }

    typedef int (WINAPI *FN_GetUserDefaultLocaleName)(LPWSTR lpLocaleName, int cchLocaleName);
    static FN_GetUserDefaultLocaleName fnGetUserDefaultLocaleName =
        (FN_GetUserDefaultLocaleName) GetModuleAddr(L"kernel32", "GetUserDefaultLocaleName");
    string locale = "<unknown>";
    if (fnGetUserDefaultLocaleName) {
        std::wstring buf(LOCALE_NAME_MAX_LENGTH, '\0');
        int len = fnGetUserDefaultLocaleName(&buf[0], buf.size());
        buf.resize(len - 1);
        locale = ws2s(buf);
    }
    
    return str_format("Windows %s %dbit (NT %d.%d build %d) %s", name, bitness,
                      major, minor, osvi.dwBuildNumber, locale.c_str());
}

int os_get_system_ram()
{
    return SDL_GetSystemRAM();
}

int os_init()
{
    // get scaling factor for retina
    {
        HDC screen = GetDC(0);
        int dpiX = GetDeviceCaps(screen, LOGPIXELSX);
        int dpiY = GetDeviceCaps(screen, LOGPIXELSY);
        ReleaseDC(0, screen);

        const float factor = dpiX / 96.f;
        ReportWin32("DPI scaling factor is %g", factor);
        sdl_set_scaling_factor(factor);
    }

    // increase timer resolution
    {
        const UINT TARGET_RESOLUTION = 1;         // 1-millisecond target resolution

        TIMECAPS tc;

        if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) == TIMERR_NOERROR)
        {
            UINT wTimerRes = min(max(tc.wPeriodMin, TARGET_RESOLUTION), tc.wPeriodMax);
            MMRESULT res = timeBeginPeriod(wTimerRes);
            ReportWin32("Set timer resolution to %dms: %s", wTimerRes, (res == TIMERR_NOERROR) ? "OK" : "FAILED");
        }
        else
        {
            ReportWin32("Error setting timer resolution");
        }
    }

    if (OLG_UseDevSavePath())
    {
        AllocConsole();
        freopen("conin$","r",stdin);
        freopen("conout$","w",stdout);
        freopen("conout$","w",stderr);
    }

    return 1;
}

int main(int argc, char* argv[])
{
    // setup crash handler
    SetUnhandledExceptionFilter(myExceptionHandler);

    // allow highdpi on retina-esque displays
    {
        // this causes a link error on XP
        // SetProcessDPIAware();
        typedef BOOL (WINAPI *PSetProcessDPIAware)();
        PSetProcessDPIAware pSetProcessDPIAware =
            (PSetProcessDPIAware) GetModuleAddr(L"user32.dll", "SetProcessDPIAware");
        if (pSetProcessDPIAware)
            pSetProcessDPIAware();
    }

    return sdl_os_main(argc, (const char**) argv);
}

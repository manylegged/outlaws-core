
// Main.cpp - Outlaws.h platform implementation for windows (together with sdl_os.cpp)

#include "StdAfx.h"

#include <stdio.h>

#include "../sdl_os/sdl_os.h"
#include "Graphics.h"
#include "Shaders.h"

#include <locale>
#include <codecvt>

#include "Shlwapi.h"
#pragma comment(lib, "Shlwapi.lib")

#include <Shellapi.h>
#pragma comment(lib, "Shell32.lib")

#include <Shlobj.h>
#include <KnownFolders.h>

static string ws2s(const std::wstring& wstr)
{
    typedef std::codecvt_utf8<wchar_t> convert_typeX;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.to_bytes(wstr);
}

static std::wstring s2ws(const std::string& s)
{
    int slength = (int)s.length();
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, 0, 0);
    std::wstring r(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, &r[0], len);
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
            while (*wptr-- != L'\\');
            while (*wptr-- != L'\\');
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

static const std::wstring& getDataDir()
{
    static std::wstring str;
    if (str.empty())
    {
        wchar_t binname[MAX_PATH];
        GetModuleFileName(NULL, binname, MAX_PATH);
        wchar_t driv[_MAX_DRIVE];
        wchar_t dir[_MAX_DIR];
        wchar_t fname[_MAX_FNAME];
        wchar_t ext[_MAX_EXT];
        _wsplitpath(binname, driv, dir, fname, ext);
        str = std::wstring(driv) + dir + L"..\\";
        str = canonicalizePath(str.c_str());

        ReportMessagef("Data Directory is %s", ws2s(str).c_str());
    }
    return str;
}

static const std::wstring& getSaveDir()
{
    static std::wstring str;
    if (str.empty())
    {
        LPWSTR path = NULL;
        if (SHGetKnownFolderPath(FOLDERID_SavedGames, 0, NULL, &path) != S_OK)
        {
            ReportMessagef("[win32] Unable to get save directory path");
            return getDataDir();
        }
        str = std::wstring(path) + L'\\' + s2ws(OGL_GetName()) + L'\\';
    }
    return str;
}

bool DirectoryExists(const wchar_t* szPath)
{
    DWORD dwAttrib = GetFileAttributes(szPath);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
            (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

const char *OL_PathForFile(const char *fname, const char* flags)
{
    std::wstring cpath = canonicalizePath(s2ws(fname).c_str());

    // expecting relative paths
    ASSERT(cpath.size() > 1 && cpath[0] != L'/' && cpath[1] != L':');

#if 1//!(DEBUG || DEVELOP)
    std::wstring savepath = getSaveDir() + cpath;
    const bool spexists = PathFileExists(savepath.c_str());
    if (flags[0] == 'w' || spexists)
    {
        if (!spexists)
        {
            const std::wstring dirname = getDirname(savepath.c_str());

            if (!DirectoryExists(dirname.c_str()) && SHCreateDirectoryEx(NULL, dirname.c_str(), NULL) != ERROR_SUCCESS)
            {
                ReportMessagef("[win32] error creating directory '%s'", ws2s(dirname).c_str());
                return 0;
            }
        }
        return sdl_os_autorelease(ws2s(savepath));
    }
#endif

    std::wstring respath = getDataDir() + cpath;
    return sdl_os_autorelease(ws2s(respath));
}


static void ReportWin32Err(const char *msg)
{
	DWORD   dwLastError = GetLastError();
	TCHAR   lpBuffer[256] = _T("?");
	if(dwLastError != 0)    // Don't want to see a "operation done successfully" error ;-)
		::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,                 // It's a system error
		NULL,                                      // No string to be formatted needed
		dwLastError,                               // Hey Windows: Please explain this error!
		MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),  // Do it in the standard language
		lpBuffer,              // Put the message here
		(sizeof(lpBuffer)*sizeof(TCHAR))-1,                     // Number of bytes to store the message
		NULL);
	ReportMessagef("Win32 Error: %s: %ls", msg, lpBuffer);
}

int OL_SaveFile(const char *name, const char* data)
{
    const char* fname = OL_PathForFile(name, "w");
    string fnameb = string(fname) + ".b";

    SDL_RWops *io = SDL_RWFromFile(fnameb.c_str(), "w");

    if (!io)
    {
        ReportMessagef("[win32] error opening '%s' for writing", fnameb.c_str());
        return 0;
    }
    const int size = strlen(data);
    const int bytesWritten = SDL_RWwrite(io, data, sizeof(char), size);
    if (bytesWritten != size)
    {
        ReportMessagef("[win32] writing to '%s', wrote %d bytes of expected %d", fnameb.c_str(), bytesWritten, size);
        return 0;
    }
    if (SDL_RWclose(io) != 0)
    {
        ReportMessagef("[win32] error closing temp file from '%s': %s'", fnameb.c_str(), SDL_GetError());
        return 0;
    }
    
    if (!MoveFileEx(s2ws(fnameb).c_str(), s2ws(fname).c_str(), MOVEFILE_REPLACE_EXISTING))
    {
		ReportWin32Err("MoveFileEx failed");
        ReportMessagef("[win32] Failed to write %s", fname);
        return 0;
    }

    return 1;
}

int OL_RemoveFileOrDirectory(const char* dirname)
{
	const char* path = OL_PathForFile(dirname, "w");
#if 1
    std::wstring buf = s2ws(path);
    buf.push_back(L'\0');
    SHFILEOPSTRUCT v;
    memset(&v, 0, sizeof(v));
    v.wFunc = FO_DELETE;
    v.pFrom = buf.c_str();
    v.fFlags = FOF_NO_UI;
    int val = SHFileOperation(&v);
#else
	string cmd = str_format("rd /s /q %s", path);
	int val = system(cmd.c_str());
#endif
	return val == 0 ? 1 : 0;
}

// enable optimus!
extern "C" {
    _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}

static LONG WINAPI myExceptionHandler(EXCEPTION_POINTERS *info)
{
    ReportMessagef("Unhandled Top Level Exception");
    const EXCEPTION_RECORD *rec = info->ExceptionRecord;
    ReportMessagef("Code: %d, Flags: %d, Address: %d", rec->ExceptionCode, rec->ExceptionFlags, (int) rec->ExceptionAddress);

    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || rec->ExceptionCode == EXCEPTION_IN_PAGE_ERROR)
    {
        ULONG type = rec->ExceptionInformation[0];
        ULONG addr = rec->ExceptionInformation[1];
        ReportMessagef("Invalid %s to %d", type == 0 ? "Read" : type == 1 ? "Write" : type == 8 ? "Exec" : "Unknown", (int) addr);
    }

    sdl_os_oncrash();
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char* argv[])
{
    SetUnhandledExceptionFilter(myExceptionHandler);
    return sdl_os_main(argc, argv);
}

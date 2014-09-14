
// main.cpp - Outlaws.h platform implementation for linux (together with sdl_os.cpp)

#include "StdAfx.h"
#include "../sdl_os/sdl_os.h"

#include <ftw.h>
#include <unistd.h>

#include <libgen.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <wordexp.h>

#define MAX_PATH 256

char *g_binaryName;

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
        return str_format("/home/%s/%s", OL_GetUserName(), 
                          path.size() < 2 ? "" : path.c_str()+2);
    }
    return path;
}

static const string& getSaveDir()
{
    static string savedir;
    if (savedir.empty())
    {
        const char* data_home = getenv("XDG_DATA_HOME");
        savedir = data_home ? data_home : "~/.local/share";
        savedir += str_format("/%s/", OLG_GetName());
        savedir = expandTilde(savedir);
    }
    return savedir;
}

static const string& getDataDir()
{
    static string datadir;
    if (datadir.empty())
    {
        datadir = g_binaryName;
        datadir = dirname((char*)datadir.c_str());
        datadir += "/../";
        char buf1[PATH_MAX];
        const char* path = realpath(datadir.c_str(), buf1);
        if (!path) {
            ReportLinux("realpath(%s) error: %s\n", datadir.c_str(), strerror(errno));
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

    string path = getSaveDir() + fname;
    if (!OLG_UseDevSavePath() && (flags[0] == 'w' || 
                                  flags[0] == 'a' || 
                                  fileDirectoryPathExists(path.c_str())))
    {
    }
    else
    {
        path = getDataDir() + fname;
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
            if (mkdir(tmp, S_IRWXU) != 0) {
                ReportLinux("mkdir '%s' error: '%s'", tmp, strerror(errno));
                return 0;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, S_IRWXU) != 0) {
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

int OL_SaveFile(const char *name, const char* data)
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
    int bytesWritten = fprintf(f, "%s", data);
    if (bytesWritten != strlen(data))
    {
        ReportLinux("writing to '%s', wrote %d bytes of expected %d\n", fnameb.c_str(), bytesWritten, strlen(data));
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
        ReportLinux("Error opening directory '%s': %s", path, strerror(errno));
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

void os_errormessage(const char* msg)
{
    OL_ReportMessage(msg);
}

int os_copy_file(const char* pa, const char* pb)
{
    FILE *fa = fopen(OL_PathForFile(pa, "r"), "r");
    if (!fa)
        return -1;
    FILE *fb = fopen(OL_PathForFile(pb, "w"), "w");
    if (!fb)
        return -1;
    
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


int main(int argc, char** argv)
{
    g_binaryName = argv[0];

    {
        const char* name = "reassembly_icon.png";
        string dest = str_format("~/.local/share/icons/%s", name);
        int status = 1;
        if (!OL_FileDirectoryPathExists(dest.c_str()))
        {
            status = os_copy_file(str_format("linux/%s", name).c_str(), dest.c_str());
        }
        ReportLinux("Copied icon to %s: %s", dest.c_str(), 
                    status == 1 ? "ALREADY OK" : status == 0 ? "OK" : "FAILED");
    }

    try {
        return sdl_os_main(argc, argv);
    } catch (const std::exception &e) {
        ReportLinux("unhandled exception: %s", e.what());
    }
}

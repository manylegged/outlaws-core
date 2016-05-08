
#include "StdAfx.h"
#include "ZipFile.h"

#include <zlib.h>
#include "../minizip/unzip.h"

static DEFINE_CVAR(bool, kReadZipFiles, false);

static void ZF_Report(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    Report("[ZF] " + str_vformat(format, vl));
    va_end(vl);
}

#if WIN32

#define GZ_OPEN(F, M) gzopen_w(s2ws(F).c_str(), (M))
#define FOPEN(F, M) _wfopen(s2ws(F).c_str(), L ## M)

#include "../minizip/iowin32.h"
static unzFile openZip(const string &fil)
{
    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64W(&ffunc);
    return unzOpen2_64(s2ws(fil).c_str(), &ffunc);
}

#else

#define GZ_OPEN(F, M) gzopen((F), (M))
#define FOPEN(F, M) fopen((F), (M))

static unzFile openZip(const string &fil)
{
    zlib_filefunc64_def ffunc;
    fill_fopen64_filefunc(&ffunc);
    return unzOpen2_64(fil.c_str(), &ffunc);
}

#endif

typedef pair< unzFile, std::unordered_map<string, unz64_file_pos> > ZipDirectory;
typedef std::map<std::string, ZipDirectory> ZipFileDir;
typedef pair<std::string, ZipDirectory> ZipFileEntry;

static ZipFileDir& getZipFileDir()
{
    static std::map<std::string, ZipDirectory> zd;
    return zd;
}

static std::mutex &getZipMutex()
{
    static std::mutex m;
    return m;
}

void ZF_ClearCached()
{
    std::lock_guard<std::mutex> l(getZipMutex());
    ZipFileDir &zfd = getZipFileDir();
    foreach (const ZipFileEntry &ze, zfd)
    {
        unzClose(ze.second.first);
        DPRINT(SAVE, ("close cashed %s (%d files)", ze.first.c_str(), (int)ze.second.second.size()));
    }
    zfd.clear();
}

static string getZipFileName(const char* path)
{
    string dirn = path;
    do {
        string zipf = dirn + ".zip";
        if (OL_FileDirectoryPathExists(zipf.c_str()))
            return zipf;
    } while ((dirn = str_dirname(dirn)).size() > 1);
    return "";
}

static const ZipDirectory *getZipDir(const char* path)
{
    string zipf = getZipFileName(path);
    if (!zipf.size())
        return NULL;
    
    zipf = OL_PathForFile(zipf.c_str(), "r");

    std::lock_guard<std::mutex> l(getZipMutex());
    ZipDirectory &zd = getZipFileDir()[zipf];
    if (zd.first)
        return &zd;              // already cached

    zd.first = openZip(zipf);

    char buf[512];
    if (unzGoToFirstFile2(zd.first, NULL, buf, arraySize(buf), NULL, 0, NULL, 0) != UNZ_OK)
        return NULL;            // corrupt?
    
    do {
        unz64_file_pos pos;
        unzGetFilePos64(zd.first, &pos);
        zd.second[str_basename(buf)] = pos;
                        
    } while (unzGoToNextFile2(zd.first, NULL, buf, arraySize(buf), NULL, 0, NULL, 0) == UNZ_OK);

    return &zd;
}

static string loadCurrentFile(unzFile uf)
{
    string sdata;

    unz_file_info64 info;
    if (unzGetCurrentFileInfo64(uf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
        return sdata;

    if (info.uncompressed_size == 0)
        return sdata;

    if (unzOpenCurrentFile(uf) != UNZ_OK)
        return sdata;

    sdata.resize(info.uncompressed_size);
    int read = unzReadCurrentFile(uf, (void*)sdata.data(), sdata.size());

    if (read < sdata.size())
        sdata.resize(read);

    unzCloseCurrentFile(uf);
    return sdata;
}

static gzFile openGzip(const char* path, const char* mode)
{
    string gzp = str_endswith(path, ".gz") ? string(path) : str_concat(path, ".gz");
    const char* abspath = OL_PathForFile(gzp.c_str(), mode);
    if (str_contains(mode, 'w'))
        OL_CreateParentDirs(abspath);
    gzFile gzf = GZ_OPEN(abspath, mode);

    // try opening uncompressed
    if (!gzf && str_contains(mode, 'r'))
    {
        gzp.resize(gzp.size() - 3); // remove .gz
        abspath = OL_PathForFile(gzp.c_str(), mode);
        gzf = GZ_OPEN(abspath, mode);
    }

    if (gzf) {
        DPRINT(SAVE, ("%s %s", (mode[0] == 'r') ? "load" : "save", abspath));
    }
    
    return gzf;
}

string ZF_LoadFile(const char* path)
{
    // 1. read gziped file
    {
        gzFile gzf = openGzip(path, "r");
        if (gzf)
        {
            string buf;
            buf.resize(4 * 1024);
            int offset = 0;
            int read = 0;
            while ((read = gzread(gzf, (void*)(offset + &buf[0]), buf.size() - offset)) == (buf.size() - offset))
            {
                offset = buf.size();
                buf.resize(offset * 2);
            }
            if (read == -1) {
                ZF_Report("Error reading '%s': %s", path, gzerror(gzf, NULL));
                read = 0;
            }
            buf.resize(offset + read);
            gzclose(gzf);
            return buf;
        }
    }

    // 2. read uncompressed file
    // const char* fl = OL_LoadFile(path);
    // if (fl)
        // return fl;

    if (!kReadZipFiles)
        return string();

    // 3. read zip file
    const ZipDirectory *zd = getZipDir(path);
    if (!zd)
        return "";
    const unz64_file_pos &pos = map_get(zd->second, str_basename(path));
    if (!pos.pos_in_zip_directory)
        return "";
    unzGoToFilePos64(zd->first, &pos);
    DPRINT(SAVE, ("load %s/%s", getZipFileName(path).c_str(), str_basename(path).c_str()));
    return loadCurrentFile(zd->first);
}

string ZF_LoadFileRaw(const char *path)
{
    path = OL_PathForFile(path, "r");
    FILE *fil = FOPEN(path, "rb");
    if (!fil)
        return string();
    if (fseek(fil, 0, SEEK_END))
    {
        ZF_Report("fseek(0, SEEK_END) failed: %s", strerror(errno));
        return string();
    }
    const long size = ftell(fil);
    if (fseek(fil, 0, SEEK_SET))
    {
        ZF_Report("fseek(0, SEEK_SET) failed: %s", strerror(errno));
        return string();
    }
    
    string buf;
    buf.resize(size);
    const size_t read = fread(&buf[0], 1, size, fil);
    if (fclose(fil))
    {
        ZF_Report("fclose '%s' failed: %s", path, strerror(errno));
        return string();
    }
    
    ASSERT(read == size);
    buf.resize(read);
    return buf;
}


bool ZF_SaveFile(const char* path, const char* data, size_t size)
{
    gzFile gzf = openGzip(path, "w");
    if (!gzf) {
        ASSERT_FAILED("openGzip", "Failed to open '%s' for %s: %s",
                      path, str_bytes_format(size).c_str(), strerror(errno));
        return false;
    }
    int written = gzwrite(gzf, data, size);
    gzclose(gzf);
    const bool success = (written == size);
    if (!success) {
        ZF_Report("gzwrite wrote %d of %d bytes to '%s'", written, (int)size, path);
    }
    return success;
}

bool ZF_SaveFileRaw(const char* path, const char* data, size_t size)
{
    const char* fname = OL_PathForFile(path, "w");
    const string fnameb = string(fname) + ".b";
    OL_CreateParentDirs(fname);

    FILE *fil = FOPEN(fnameb.c_str(), "wb");
    if (!fil)
    {
        ZF_Report("error opening '%s' for writing", fnameb.c_str());
        return 0;
    }
    
    int bytesWritten = 0;
    int bytesExpected = size;

    // translate newlines
#if WIN32
    if (str_endswith(fname, ".txt") || str_endswith(fname, ".lua"))
    {
        string data1;
        data1.reserve(size);
        for (const char* ptr=data; *ptr != '\0'; ptr++) {
            if (*ptr == '\n')
                data1 += "\r\n";
            else
                data1 += *ptr;
        }
        bytesWritten = fwrite(&data1[0], 1, data1.size(), fil);
        bytesExpected = data1.size();
    }
    else
#endif
    {
        bytesWritten = fwrite(data, 1, size, fil);    
    }
    
    if (bytesWritten != bytesExpected)
    {
        ZF_Report("writing to '%s', wrote %d bytes of expected %d", fnameb.c_str(), bytesWritten, bytesExpected);
        return 0;
    }
    if (fclose(fil))
    {
        ZF_Report("error closing temp file from '%s': %s'", fnameb.c_str(), strerror(errno));
        return 0;
    }

#if WIN32
    // rename does not support overwriting dest on windows
    if (!MoveFileEx(s2ws(fnameb).c_str(), s2ws(fname).c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        
        ReportWin32Err1(str_format("MoveFileEx('%s')", fname).c_str(), GetLastError(), __FILE__, __LINE__);
        return 0;
    }
#else
    if (rename(fnameb.c_str(), fname))
    {
        ZF_Report("error renaming temp file from '%s' to '%s': %s'", fnameb.c_str(), fname, strerror(errno));
        return 0;
    }
#endif

    return 1;
}


ZFDirMap ZF_LoadDirectory(const char* path, float* progress)
{
    ZFDirMap dir;
    
    const char** files = OL_ListDirectory(path);
    if (files)
    {
        int count = 0;
        for (const char** ptr=files; *ptr; ptr++)
            count++;
        int i=0;
        for (const char** ptr=files; *ptr; ptr++)
        {
            const string fname = str_path_join(path, *ptr);
            string data = ZF_LoadFile(fname.c_str());
            if (data.size()) {
                dir[fname] = std::move(data);
            } else {
                ZF_Report("Error loading '%s' from '%s'", *ptr, path);
            }
            if (progress)
                *progress = (float) i / count;
            i++;
        }
        return dir;
    }

    if (!kReadZipFiles)
        return dir;

    string zipf = getZipFileName(path);
    if (!zipf.size())
        return dir;
    zipf = OL_PathForFile(zipf.c_str(), "r");

    unzFile uf = openZip(zipf);
    if (!uf)
        return dir;
    char buf[512];
    if (unzGoToFirstFile2(uf, NULL, buf, arraySize(buf), NULL, 0, NULL, 0) != UNZ_OK)
        goto done;
    dir[buf] = loadCurrentFile(uf);

    while (unzGoToNextFile2(uf, NULL, buf, arraySize(buf), NULL, 0, NULL, 0) == UNZ_OK)
    {
        dir[buf] = loadCurrentFile(uf);
    }

    DPRINT(SAVE, ("load %s", zipf.c_str()));

done:
    unzClose(uf);
    return dir;
}

static string closeDeflate(z_stream &stream, string&& str)
{
    int end_stat = deflateEnd(&stream);
    ASSERTF(end_stat == Z_OK, "(%d): %s", end_stat, stream.msg);
    return std::move(str);
}

#define CHECK_DEFLATE(CALL, EXPECT)                                     \
    if ((stat = (CALL)) != (EXPECT)) {                                  \
        ASSERT_FAILED(#CALL " == " #EXPECT, " (%d): %s", stat, stream.msg); \
        return closeDeflate(stream, "");                                \
    }

static string closeInflate(z_stream &stream, string&& str)
{
    int end_stat = inflateEnd(&stream);
    ASSERTF(end_stat == Z_OK, "(%d): %s", end_stat, stream.msg);
    return std::move(str);
}

#define CHECK_INFLATE(CALL, EXPECT)                                     \
    if ((stat = (CALL)) != (EXPECT)) {                                  \
        ASSERT_FAILED(#CALL " == " #EXPECT, " (%d): %s", stat, stream.msg); \
        return closeInflate(stream, "");                                \
    }

string ZF_Compress(const char* data, size_t size)
{
    z_stream stream;
    int stat = 0;
    string dest;

    stream.next_in = (Bytef *)data;
    stream.avail_in = (uInt)size;
    stream.next_out = 0;
    stream.avail_out = 0;
    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;
    
    CHECK_DEFLATE(deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY), Z_OK);
    dest.resize(deflateBound(&stream, size));
    stream.next_out = (Bytef*)dest.data();
    stream.avail_out = (uInt)dest.size();
    CHECK_DEFLATE(deflate(&stream, Z_FINISH), Z_STREAM_END);
    dest.resize(stream.total_out);
    return closeDeflate(stream, std::move(dest));
}

string ZF_Decompress(const char* data, size_t size)
{
    if (!data || size == 0)
        return string();
    z_stream stream;
    int stat = 0;

    // last dword is uncompressed size, but is only occasionally correct?
    const uint last_dword = ((uint*) data)[(size/sizeof(uint))-1];
    const uint uncompressed_size = clamp(last_dword, 2 * (uint)size, 20 * (uint)size);
    
    string dest;
    dest.resize(uncompressed_size);

    stream.next_in = (Bytef *)data;
    stream.avail_in = (uInt)size;
    stream.next_out = (Bytef*)dest.data();
    stream.avail_out = (uInt)dest.size();
    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;
    
    CHECK_INFLATE(inflateInit2(&stream, 15 + 16), Z_OK);

    while (vec_contains({Z_OK, Z_BUF_ERROR}, (stat = inflate(&stream, Z_FINISH))))
    {
        const int written = stream.next_out - (Bytef*)dest.data();
        dest.resize(2 * dest.size());
        stream.next_out = (Bytef*)dest.data() + written;
        stream.avail_out = dest.size() - written;
    }

    CHECK_INFLATE(stat, Z_STREAM_END);
    dest.resize(stream.total_out);
    return closeInflate(stream, std::move(dest));
}


#include "StdAfx.h"
#include "ZipFile.h"

#include <zlib.h>
#include "../minizip/unzip.h"

#if WIN32

#define GZ_OPEN(F, M) gzopen_w(s2ws(F).c_str(), (M))

#include "../minizip/iowin32.h"
static unzFile openZip(const string &fil)
{
    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64W(&ffunc);
    return unzOpen2_64(s2ws(fil).c_str(), &ffunc);
}

#else

#define GZ_OPEN(F, M) gzopen((F), (M))

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
    const string gzp = str_endswith(path, ".gz") ? string(path) : str_concat(path, ".gz");
    const char* abspath = OL_PathForFile(gzp.c_str(), mode);
    gzFile gzf = GZ_OPEN(abspath, mode);
    
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
                ReportMessagef("Error reading '%s': %s", path, gzerror(gzf, NULL));
                read = 0;
            }
            buf.resize(offset + read);
            gzclose(gzf);
            return buf;
        }
    }

    // 2. read uncompressed file
    const char* fl = OL_LoadFile(path);
    if (fl)
        return fl;


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

int ZF_SaveFile(const char* path, const char* data, size_t size)
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
        ReportMessagef("gzwrite wrote %d of %d bytes to '%s'",
                       written, (int)size, path);
    }
    return success;
}

ZFDirMap ZF_LoadDirectory(const char* path)
{
    ZFDirMap dir;
    
    const char** ptr = OL_ListDirectory(path);
    if (ptr)
    {
        for (; *ptr; ptr++)
        {
            string fname = str_path_join(path, *ptr);
            const char* data = OL_LoadFile(fname.c_str());
            if (data) {
                dir[fname] = data;
            } else {
                ReportMessagef("Error loading '%s' from '%s'", *ptr, path);
            }
        }
        return dir;
    }

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

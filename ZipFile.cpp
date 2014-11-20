
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

void ZF_ClearCached()
{
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
    const char* abspath = OL_PathForFile(str_format("%s.gz", path).c_str(), mode);
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
            const int kChunkSize = 4 * 1024;
            int offset = 0;
            int read = 0;
            buf.resize(kChunkSize);
            while ((read = gzread(gzf, (void*)(offset + buf.data()), buf.size() - offset)) == kChunkSize)
            {
                offset = buf.size();
                buf.resize(offset + kChunkSize);
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
    int written = gzwrite(gzf, data, size);
    if (written != size)
        ReportMessagef("gzwrite wrote %d of %d bytes", written, (int)size);
    gzclose(gzf);
    return (written == size) ? 1 : 0;
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
            dir[fname] = OL_LoadFile(fname.c_str());
        }
        return dir;
    }

    string zipf = getZipFileName(path);
    if (!zipf.size())
        return dir;
    zipf = OL_PathForFile(zipf.c_str(), "r");

    unzFile uf = openZip(zipf);
    char buf[512];
    if (!uf)
        goto done;
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


string ZF_Compress(const char* data, size_t size)
{
    z_stream stream;
    
    string dest;
    dest.resize(size);

    stream.next_in = (Bytef *)data;
    stream.avail_in = (uInt)size;
    stream.next_out = (Bytef*)dest.data();
    stream.avail_out = (uInt)dest.size();
    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;
    
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
    {
        dest.resize(0);
        goto done;
    }
    
    if (deflate(&stream, Z_FINISH) != Z_STREAM_END)
    {
        dest.resize(0);
        goto done;
    }

    dest.resize(stream.total_out);

done:
    deflateEnd(&stream);    
    return dest;
}

string ZF_Decompress(const char* data, size_t size)
{
    z_stream stream;
    int stat = 0;

    string dest;
    dest.resize(2 * size);

    stream.next_in = (Bytef *)data;
    stream.avail_in = (uInt)size;
    stream.next_out = (Bytef*)dest.data();
    stream.avail_out = (uInt)dest.size();
    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;
    
    if (inflateInit2(&stream, 15 + 16) != Z_OK)
    {
        dest.resize(0);
        goto done;
    }

    while ((stat = inflate(&stream, Z_FINISH)) == Z_OK)
    {
        const int written = stream.next_out - (Bytef*)dest.data();
        dest.resize(2 * dest.size());
        stream.next_out = (Bytef*)dest.data() + written;
        stream.avail_out = dest.size() - written;
    }
    
    if (stat != Z_STREAM_END)
    {
        dest.resize(0);
        goto done;
    }

    dest.resize(stream.total_out);

done:
    deflateEnd(&stream);    
    return dest;
}

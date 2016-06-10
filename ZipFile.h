
#ifndef ZIPFILE_H
#define ZIPFILE_H

// transparently load files from gziped or zip files, in this order
// 1. data/ships/foo.lua.gz
// 2. data/ships/foo.lua
// 3. data/ships.zip/foo.lua

// read file, gzip compressed file, or file in zip file
string ZF_LoadFile(const char* path);

// read file with no decompression
string ZF_LoadFileRaw(const char *path);

// close any cached zip files
void ZF_ClearCached();

// write gzip compressed file
bool ZF_SaveFile(const char* path, const char* data, size_t size);
inline bool ZF_SaveFile(const char* path, const string &data) { return ZF_SaveFile(path, &data[0], data.size()); }

// write uncompressed file
bool ZF_SaveFileRaw(const char* path, const char* data, size_t size);
inline bool ZF_SaveFileRaw(const char* path, const string &data) { return ZF_SaveFileRaw(path, &data[0], data.size()); }

// compress data/size into gzip format
string ZF_Compress(const char* data, size_t size);

// decompress data/size in gzip format
string ZF_Decompress(const char* data, size_t size);

// load an entire directory or zip file into memory
typedef std::pair<std::string, std::string> ZFFileData;
typedef std::map<std::string, std::string> ZFDirMap;
ZFDirMap ZF_LoadDirectory(const char* path, float* progress);

#endif

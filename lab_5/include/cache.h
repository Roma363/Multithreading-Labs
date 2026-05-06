#ifndef CACHE_H
#define CACHE_H

#include <cstddef>
#include <string>
#include <windows.h>

#define MAX_CACHED_FILES 100

struct FileCache {
    std::string filepath;
    HANDLE handle;
    std::size_t file_size;
    std::string content_type;
};

FileCache* get_file_cache(const char *filepath);

#endif // CACHE_H

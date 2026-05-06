#include <array>
#include <string>
#include <windows.h>
#include "../include/cache.h"

namespace {
std::array<FileCache, MAX_CACHED_FILES> cache;
std::size_t cache_size = 0;

CRITICAL_SECTION cache_mutex;
INIT_ONCE cache_init_once = INIT_ONCE_STATIC_INIT;

BOOL CALLBACK init_cache_mutex(PINIT_ONCE, PVOID, PVOID*) {
    InitializeCriticalSection(&cache_mutex);
    return TRUE;
}

class CacheLock {
public:
    CacheLock() {
        InitOnceExecuteOnce(&cache_init_once, init_cache_mutex, NULL, NULL);
        EnterCriticalSection(&cache_mutex);
    }
    ~CacheLock() {
        LeaveCriticalSection(&cache_mutex);
    }
};
} // namespace

FileCache* get_file_cache(const char *filepath) {
    CacheLock lock;
    for (std::size_t i = 0; i < cache_size; ++i) {
        if (cache[i].filepath == filepath) {
            return &cache[i];
        }
    }
    if (cache_size >= MAX_CACHED_FILES) {
        return nullptr;
    }
    DWORD attrs = GetFileAttributesA(filepath);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return nullptr;
    }
    HANDLE handle = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        return nullptr;
    }
    LARGE_INTEGER size;
    if (!GetFileSizeEx(handle, &size)) {
        CloseHandle(handle);
        return nullptr;
    }
    FileCache *new_cache = &cache[cache_size++];
    new_cache->filepath = filepath;
    new_cache->handle = handle;
    new_cache->file_size = static_cast<std::size_t>(size.QuadPart);
    const std::string filepath_str(filepath);
    std::string content_type = "text/plain";
    if (filepath_str.find(".html") != std::string::npos) content_type = "text/html";
    else if (filepath_str.find(".css") != std::string::npos) content_type = "text/css";
    else if (filepath_str.find(".js") != std::string::npos) content_type = "application/javascript";
    else if (filepath_str.find(".png") != std::string::npos) content_type = "image/png";
    else if (filepath_str.find(".jpg") != std::string::npos || filepath_str.find(".jpeg") != std::string::npos) content_type = "image/jpeg";
    new_cache->content_type = content_type;
    return new_cache;
}

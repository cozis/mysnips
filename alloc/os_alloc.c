#include <assert.h>

#include "os_alloc.h"

#if defined(_WIN32)
#define PLATFORM_WINDOWS 1
#define PLATFORM_LINUX   0
#define PLATFOEM_OTHER   0
#elif defined(__linux__)
#define PLATFORM_WINDOWS 0
#define PLATFORM_LINUX   1
#define PLATFOEM_OTHER   0
#else
#define PLATFORM_WINDOWS 0
#define PLATFORM_LINUX   0
#define PLATFOEM_OTHER   1
#endif

#if PLATFORM_WINDOWS
#include <windows.h>
#endif

#if PLATFORM_LINUX
#include <unistd.h>
#include <sys/mman.h>
#endif

size_t ospagesize(void)
{
    #if PLATFORM_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
    #endif

    #if PLATFORM_LINUX
    long n = sysconf(_SC_PAGESIZE);
    assert(n > 0);
    return n;
    #endif

    #if PLATFORM_OTHER
    return 0;
    #endif
}

void osfree(void *addr, size_t len)
{
    #if PLATFORM_WINDOWS
    VirtualFree(addr, len, MEM_RELEASE);
    #endif

    #if PLATFORM_LINUX
    munmap(addr, len);
    #endif
}

void *osalloc(size_t len)
{
    #if PLATFORM_WINDOWS
    return VirtualAlloc(NULL, len, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    #endif

    #if PLATFORM_LINUX
    void *addr = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (addr == NULL) { // We use NULL to report problems, so we can't use this page
        osfree(addr, len);
        return NULL;
    }
    if (addr == MAP_FAILED)
        return NULL;
    return addr;
    #endif

    #if PLATFORM_OTHER
    return NULL;
    #endif
}

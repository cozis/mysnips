#include <assert.h>
#include <stdlib.h>

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

size_t os_pagesize(void)
{
    #if PLATFORM_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
    #endif

    #if PLATFORM_LINUX
    long n = sysconf(_SC_PAGESIZE);
    if (n <= 0) abort();
    return n;
    #endif

    #if PLATFORM_OTHER
    return 0;
    #endif
}

void os_free(void *addr, size_t len)
{
    #if PLATFORM_WINDOWS
    VirtualFree(addr, len, MEM_RELEASE);
    #endif

    #if PLATFORM_LINUX
    munmap(addr, len);
    #endif
}

void *os_alloc(size_t len)
{
    #if PLATFORM_WINDOWS
    void *addr = VirtualAlloc(NULL, len, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    if (addr == NULL) abort();
    return addr;
    #endif

    #if PLATFORM_LINUX
    void *addr = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED)
        abort();
    return addr;
    #endif

    #if PLATFORM_OTHER
    abort();
    #endif
}

#include "clock.h"

#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#include <windows.h>
#else
#include <time.h>
#include <stdlib.h>
#endif

/*
 * Returns the current absolute time in microsecods
 * TODO: Specify since when the time is calculated
 */
uint64_t get_absolute_time_us(void)
{
    #ifdef _WIN32
    FILETIME filetime;
    GetSystemTimePreciseAsFileTime(&filetime);
    uint64_t time = (uint64_t) filetime.dwLowDateTime | ((uint64_t) filetime.dwHighDateTime << 32);
    time /= 10;
    return time;
    #else
    struct timespec buffer;
    if (clock_gettime(CLOCK_REALTIME, &buffer))
        abort();
    uint64_t time = buffer.tv_sec * 1000000 + buffer.tv_nsec / 1000;
    return time;
    #endif
}

/*
 * Returns the current time in nanoseconds since
 * an unspecified point in time.
 */
uint64_t get_relative_time_ns(void)
{
    #ifdef _WIN32
    {
        int64_t count;
        int64_t freq;
        int ok;
        
        ok = QueryPerformanceCounter((LARGE_INTEGER*) &count);
        if (!ok) abort();

        ok = QueryPerformanceFrequency((LARGE_INTEGER*) &freq);
        if (!ok) abort();

        uint64_t res = 1000000000 * (double) count / freq;
        return res;
    }
    #else
    {
        struct timespec time;

        if (clock_gettime(CLOCK_REALTIME, &time))
            abort();

        uint64_t res;

        uint64_t sec = time.tv_sec;
        if (sec > UINT64_MAX / 1000000000)
            abort();
        res = sec * 1000000000;
        
        uint64_t nsec = time.tv_nsec;
        if (res > UINT64_MAX - nsec)
            abort();
        res += nsec;

        return res;
    }
    #endif
}
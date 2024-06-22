#include <stddef.h>
#include <stdarg.h>

typedef enum {
    LOG_FLUSH_INTERVAL_MS,
    LOG_BUFFER_SIZE_LOG2,
} logconfigname;

void logconfig(logconfigname name, int value);

typedef void *(*logmallocfn)(size_t, void*);
typedef void  (*logfreefn)(void*, size_t, void*);

void logsetalloc(logmallocfn mallocfn,
                 logfreefn freefn,
                 void *userptr);

void lograw(const char *str);
void lograw2(const char *str, size_t len);

void logf(const char *fmt, ...);
void logfv(const char *fmt, va_list args);

void logfatalf(const char *fmt, ...);
void logfatalfv(const char *fmt, va_list args);
void logfatalraw(const char *str);
void logfatalraw2(const char *str, size_t len);

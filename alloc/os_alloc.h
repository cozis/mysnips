#ifndef OS_ALLOC_H
#define OS_ALLOC_H

#include <stddef.h>

size_t os_pagesize(void);
void   os_free(void *addr, size_t len);
void  *os_alloc(size_t len);

#endif

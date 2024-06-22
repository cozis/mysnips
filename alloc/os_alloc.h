#ifndef CATY_OS_ALLOC_H
#define CATY_OS_ALLOC_H

#include <stddef.h>

size_t ospagesize(void);
void osfree(void *addr, size_t len);
void *osalloc(size_t len);

#endif /* CATY_OS_ALLOC_H */

#ifndef COZISBLOG_PATH_H
#define COZISBLOG_PATH_H

#include <stddef.h>
#include "../common/slice.h"

size_t sanitize_path(char *src, size_t len,
                     char *mem, size_t max);

int match_path_format(struct slice path, char *fmt, ...);

#endif
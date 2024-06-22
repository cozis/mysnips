#include <stddef.h>
#include <stdbool.h>
#include "../common/slice.h"

bool get_query_string_param(char *src, size_t src_len, char *key, char *dst, size_t max, struct slice *out);
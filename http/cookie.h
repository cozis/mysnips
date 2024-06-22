#include <stddef.h>
#include <stdbool.h>

// This is just for slice
#include "parse.h"

bool get_cookie(struct request *r, char *name, struct slice *out);

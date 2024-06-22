#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include "path.h"

#define PATH_SEP '/'

static bool is_print(char c)
{
    return c >= 32 && c < 127;
}

static bool is_pcomp(char c)
{
    return c != '/' && c != ':' && is_print(c);
}

int split_path_components(char *src, size_t len,
                          struct slice *stack,
                          int limit, bool allow_ddots)
{
    size_t cur = 0;

    // Skip the first slash
    if (cur < len && src[cur] == PATH_SEP)
        cur++;

    int depth = 0;
    while (cur < len) {

        if (depth == limit)
            return -1;

        size_t start = cur;
        while (cur < len && (is_pcomp(src[cur]) || (allow_ddots && src[cur] == ':')))
            cur++;

        struct slice comp;
        comp.data = src + start;
        comp.size = cur - start;

        if (comp.size == 0)
            return -1; // We consider paths with empty components invalid

        if (comp.size == 2 && !memcmp(comp.data, "..", 2)) {
            if (depth == 0)
                return -1;
            depth--;
        } else {
            if (comp.size != 1 || memcmp(comp.data, ".", 1))
                stack[depth++] = comp;
        }

        if (cur == len)
            break;

        if (src[cur] != PATH_SEP)
            return -1;
        cur++;
    }
    return depth;
}

/*
 * Sanitize a path string removing ./ and ../
 * components. The final path has an initial
 * / but not final.
 */
size_t sanitize_path(char *src, size_t len,
                     char *mem, size_t max)
{
    #define MAX_COMPS 64

    struct slice stack[MAX_COMPS];
    int depth;

    depth = split_path_components(src, len, stack, MAX_COMPS, false);
    if (depth < 0)
        return -1;
    

    /*
     * Count how many output bytes are required
     */
    size_t req = depth;
    for (int i = 0; i < depth; i++)
        req += stack[i].size;
    if (req >= max)
        return -1; // Buffer too small
    
    /*
     * Copy the sanitized path into the output
     * buffer.
     */
    size_t n = 0;
    for (int i = 0; i < depth; i++) {
        mem[n++] = PATH_SEP;
        memcpy(mem + n, stack[i].data, stack[i].size);
        n += stack[i].size;
    }
    mem[n] = '\0';
    return n;
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

int match_path_format(struct slice path, char *fmt, ...)
{
    #define LIMIT 32
    struct slice p_stack[LIMIT];
    struct slice f_stack[LIMIT];
    int p_depth;
    int f_depth;

    p_depth = split_path_components(path.data, path.size,   p_stack, LIMIT, false);
    f_depth = split_path_components(fmt,       strlen(fmt), f_stack, LIMIT, true);

    if (p_depth < 0 || f_depth < 0)
        return -1; // Error

    if (p_depth != f_depth)
        return 1; // No match

    va_list args;
    va_start(args, fmt);

    for (int i = 0; i < f_depth; i++) {

        assert(f_stack[i].size > 0);
        assert(p_stack[i].size > 0);

        if (f_stack[i].data[0] == ':') {
            if (f_stack[i].size != 2)
                return -1; // Invalid format
            switch (f_stack[i].data[1]) {
                
                case 'l':
                {
                    struct slice *sl = va_arg(args, struct slice*);
                    *sl = p_stack[i];
                }
                break;
                
                case 'n':
                {
                    uint32_t n = 0;
                    size_t cur = 0;
                    while (cur < p_stack[i].size && is_digit(p_stack[i].data[cur])) {
                        int d = p_stack[i].data[cur] - '0';
                        if (n > (UINT32_MAX - d) / 10)
                            return -1; // Overflow
                        n = n * 10 + d;
                        cur++;
                    }
                    if (cur != p_stack[i].size)
                        return -1; // Component isn't a number
                    uint32_t *p = va_arg(args, uint32_t*);
                    *p = n;
                }
                break;

                default:
                return -1; // Invalid formt
            }
        } else {
            if (f_stack[i].size != p_stack[i].size)
                return 1; // No match
            if (memcmp(f_stack[i].data, p_stack[i].data, f_stack[i].size))
                return false;
        }
    }

    va_end(args);
    return 0;
}

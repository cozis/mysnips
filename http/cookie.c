#include <string.h>
#include "cookie.h"

bool is_cookie_name(char c)
{
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9')
        || c == '_' || c == '-';
}

bool is_cookie_value(char c)
{
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9')
        || c == '_' || c == '-';
}

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool get_cookie(struct request *r, char *name, struct slice *out)
{
    struct slice value;
    if (!find_header(r, name, &value))
        return false;

    size_t name_len = strlen(name);

    char  *src = value.data;
    size_t len = value.size;
    size_t cur = 0;
    while (cur < len) {
        
        size_t start;

        while (cur < len && is_space(src[cur]))
            cur++;
        
        if (cur == len)
            break;
        
        if (!is_cookie_name(src[cur]))
            return false;

        start = cur;
        do
            cur++;
        while (cur < len && is_cookie_name(src[cur]));
        
        char  *cookie_name     = src + start;
        size_t cookie_name_len = cur - start;

        while (cur < len && is_space(src[cur]))
            cur++;
        
        if (cur == len || src[cur] != '=')
            return false;
        cur++;

        while (cur < len && is_space(src[cur]))
            cur++;
        
        if (!is_cookie_value(src[cur]))
            return false;

        start = cur;
        do
            cur++;
        while (cur < len && is_cookie_value(src[cur]));
        
        char  *cookie_value     = src + start;
        size_t cookie_value_len = cur - start;

        while (cur < len && is_space(src[cur]))
            cur++;
        
        if (cur < len) {
            if (src[cur] != ';')
                return false;
            cur++;
        }

        if (name_len == cookie_name_len && !memcmp(cookie_name, name, name_len)) {
            out->data = cookie_value;
            out->size = cookie_value_len;
            return true;
        }
    }

    return false;
}
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "parse.h"

bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

int parse_request_head(char *src, size_t len,
                       struct request *r)
{
    size_t cur;
    if (len > 2
        && src[0] == 'G'
        && src[1] == 'E'
        && src[2] == 'T') {
        r->m = M_GET;
        cur = 3;
    } else if (len > 3
        && src[0] == 'H'
        && src[1] == 'E'
        && src[2] == 'A'
        && src[3] == 'D') {
        r->m = M_HEAD;
        cur = 4;
    } else if (len > 3
        && src[0] == 'P'
        && src[1] == 'O'
        && src[2] == 'S'
        && src[3] == 'T') {
        r->m = M_POST;
        cur = 4;
    } else if (len > 2
        && src[0] == 'P'
        && src[1] == 'U'
        && src[2] == 'T') {
        r->m = M_PUT;
        cur = 3;
    } else if (len > 5
        && src[0] == 'D'
        && src[1] == 'E'
        && src[2] == 'L'
        && src[3] == 'E'
        && src[4] == 'T'
        && src[5] == 'E') {
        r->m = M_DELETE;
        cur = 6;
    } else if (len > 6
        && src[0] == 'C'
        && src[1] == 'O'
        && src[2] == 'N'
        && src[3] == 'N'
        && src[4] == 'E'
        && src[5] == 'C'
        && src[6] == 'T') {
        r->m = M_CONNECT;
        cur = 7;
    } else if (len > 6
        && src[0] == 'O'
        && src[1] == 'P'
        && src[2] == 'T'
        && src[3] == 'I'
        && src[4] == 'O'
        && src[5] == 'N'
        && src[6] == 'S') {
        r->m = M_OPTIONS;
        cur = 7;
    } else if (len > 4
        && src[0] == 'T'
        && src[1] == 'R'
        && src[2] == 'A'
        && src[3] == 'C'
        && src[4] == 'E') {
        r->m = M_TRACE;
        cur = 5;
    } else if (len > 4
        && src[0] == 'P'
        && src[1] == 'A'
        && src[2] == 'T'
        && src[3] == 'C'
        && src[4] == 'H') {
        r->m = M_PATCH;
        cur = 5;
    } else {
        return P_BADMETHOD;
    }

    if (cur == len || src[cur] != ' ')
        return P_INCOMPLETE;
    cur++;

    // TODO: Make this more robust
    {
        size_t start = cur;
        while (cur < len && src[cur] != ' ')
            cur++;
        r->path.data = src + start;
        r->path.size = cur - start;
    }

    if (cur == len || src[cur] != ' ')
        return P_INCOMPLETE;
    cur++;

    if (cur+4 >= len
        || src[cur+0] != 'H'
        || src[cur+1] != 'T'
        || src[cur+2] != 'T'
        || src[cur+3] != 'P'
        || src[cur+4] != '/'
        || !is_digit(src[cur+5]))
        return P_BADVERSION;
    cur += 5;
    r->major = src[cur] - '0';
    cur++;
    
    if (cur < len && src[cur] == '.') {
        cur++;
        if (cur == len || !is_digit(src[cur]))
            return P_BADVERSION;
        r->minor = src[cur] - '0';
        cur++;
    } else {
        r->minor = 0;
    }

    if (cur+1 >= len
        || src[cur+0] != '\r'
        || src[cur+1] != '\n')
        return P_INCOMPLETE;
    cur += 2;

    r->nhs = 0;
    while (cur+1 >= len
        || src[cur+0] != '\r'
        || src[cur+1] != '\n') {
        
        struct slice name;
        struct slice value;

        size_t start = cur;

        // TODO: More robust
        while (cur < len && src[cur] != ':')
            cur++;
        
        name.data = src + start;
        name.size = cur - start;

        if (cur == len)
            return P_BADHEADER;
        cur++; // :

        // TODO: More robust
        start = cur;
        while (cur < len && src[cur] != '\r')
            cur++;
        value.data = src + start;
        value.size = cur - start;

        cur++; // \r
        if (cur == len || src[cur] != '\n')
            return P_BADHEADER;
        cur++; // \n

        if (r->nhs < MAX_HEADERS) {
            r->hs[r->nhs].name = name;
            r->hs[r->nhs].value = value;
            r->nhs++;
        }
    }
    cur += 2; // \r\n
    return P_OK;
}

char to_lower(char c)
{
    if (c >= 'A' || c <= 'Z')
        return c - 'A' + 'a';
    else
        return c;
}

bool string_match_case_insensitive(struct slice x,
                                   struct slice y)
{
    if (x.size != y.size)
        return false;
    for (size_t i = 0; i < x.size; i++)
        if (to_lower(x.data[i]) != to_lower(y.data[i]))
            return false;
    return true;
}

struct slice str_to_slice(char *str)
{
    return (struct slice) {.data=str, .size=strlen(str)};
}

bool match_header_name(struct slice s1,
                       char *s2)
{
    return string_match_case_insensitive(s1, str_to_slice(s2));
}

struct slice trim(struct slice s)
{
    size_t cur = 0;
    while (cur < s.size && is_space(s.data[cur]))
        cur++;
    
    if (cur == s.size) {
        s.data = "";
        s.size = 0;
    } else {
        while (is_space(s.data[s.size-1]))
            s.size--;
    }
    return s;
}

bool match_header_value(struct slice s1, char *s2)
{
    struct slice x = trim(s1);
    struct slice y = trim(str_to_slice(s2));
    return string_match_case_insensitive(x, y);
}

bool find_header(struct request *r, char *name,
                 struct slice *value)
{
    for (int i = 0; i < r->nhs; i++)
        if (match_header_name(r->hs[i].name, name)) {
            *value = r->hs[i].value;
            return true;
        }
    return false;
}

size_t find_and_parse_content_length(struct request *r)
{
    struct slice value;
    if (!find_header(r, "Content-Length", &value))
        return -1;

    size_t cur = 0;
    while (cur < value.size && is_space(value.data[cur]))
        cur++;
    
    if (cur == value.size || !is_digit(value.data[cur]))
        return -1;

    size_t x = 0;
    do {
        int d = value.data[cur] - '0';
        if (x > (SIZE_MAX - d) / 10)
            return -1;
        x = x * 10 + d;
        cur++;
    } while (cur < value.size && is_digit(value.data[cur]));

    while (cur < value.size && is_space(value.data[cur]))
        cur++;
    
    if (cur != value.size)
        return -1;
    
    return x;
}

size_t parse_content_length(struct slice s)
{
    char  *src = s.data;
    size_t len = s.size;

    size_t cur = 0;
    while (cur < len && is_space(src[cur]))
        cur++;
    
    if (cur == len || !is_digit(src[cur]))
        return -1;

    size_t x = 0;
    do {
        int d = src[cur] - '0';
        if (x > (SIZE_MAX - d) / 10)
            return -1;
        x = x * 10 + d;
        cur++;
    } while (cur < len && is_digit(src[cur]));

    while (cur < len && is_space(src[cur]))
        cur++;
    
    if (cur != len)
        return -1;
    
    return x;
}

int find_and_parse_transfer_encoding(struct request *r)
{
    struct slice value;
    if (!find_header(r, "Transfer-Encoding", &value))
        return 0;
    
    int res = 0;
    char  *src = value.data;
    size_t len = value.size;
    size_t cur = 0;
    for (;;) {
        
        while (cur < len && (is_space(src[cur]) || src[cur] == ','))
            cur++;
        
        if (cur+6 < len
            && src[cur+0] == 'c'
            && src[cur+1] == 'h'
            && src[cur+2] == 'u'
            && src[cur+3] == 'n'
            && src[cur+4] == 'k'
            && src[cur+5] == 'e'
            && src[cur+6] == 'd') {
            cur += 7;
            res |= T_CHUNKED;
        } else if (cur+7 < len
            && src[cur+0] == 'c'
            && src[cur+1] == 'o'
            && src[cur+2] == 'm'
            && src[cur+3] == 'p'
            && src[cur+4] == 'r'
            && src[cur+5] == 'e'
            && src[cur+6] == 's'
            && src[cur+7] == 's') {
            cur += 8;
            res |= T_COMPRESS;
        } else if (cur+6 < len
            && src[cur+0] == 'd'
            && src[cur+1] == 'e'
            && src[cur+2] == 'f'
            && src[cur+3] == 'l'
            && src[cur+4] == 'a'
            && src[cur+5] == 't'
            && src[cur+6] == 'e') {
            cur += 7;
            res |= T_DEFLATE;
        } else if (cur+3 < len
            && src[cur+0] == 'g'
            && src[cur+1] == 'z'
            && src[cur+2] == 'i'
            && src[cur+3] == 'p') {
            cur += 4;
            res |= T_GZIP;
        } else {
            return -1;
        }
    }
    return res;
}

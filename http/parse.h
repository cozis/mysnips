#ifndef PARSE_H
#define PARSE_H

#include "../common/slice.h"

enum method {
    M_GET,
    M_POST,
    M_HEAD,
    M_PUT,
    M_DELETE,
    M_CONNECT,
    M_OPTIONS,
    M_TRACE,
    M_PATCH,
};

#define MAX_HEADERS 32

struct header {
    struct slice name;
    struct slice value;
};

struct request {
    enum method m;
    struct slice path;
    int major;
    int minor;
    int nhs;
    struct header hs[MAX_HEADERS];
    struct slice content;
};

enum {
    P_OK,
    P_INCOMPLETE,
    P_BADMETHOD,
    P_BADVERSION,
    P_BADHEADER,
};

enum {
    T_CHUNKED  = 1 << 0,
    T_COMPRESS = 1 << 1,
    T_DEFLATE  = 1 << 2,
    T_GZIP     = 1 << 3,
};

int parse_request_head(char *src, size_t len, struct request *r);
bool match_header_name(struct slice s1, char *s2);
bool match_header_value(struct slice s1, char *s2);
size_t find_and_parse_content_length(struct request *r);
int    find_and_parse_transfer_encoding(struct request *r);
size_t parse_content_length(struct slice s);
bool find_header(struct request *r, char *name, struct slice *value);
#endif /* PARSE_H */
#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <poll.h>
#include "parse.h"

#ifndef MAX_CLIENTS
#define MAX_CLIENTS 512
#endif

struct iobuf {
    char  *data;
    size_t size;
    size_t used;
};

enum {
    C_FREE,
    C_IDLE,
    C_QUEUED,
    C_POPPED,
    C_STATUS,
    C_HEADER,
    C_CONTENT,
    C_CLOSE,
};

struct client {
    uint16_t gen;
    int state;
    struct pollfd *pitem;
    struct iobuf   input;
    struct iobuf   output;

    int num_served;

    int minor;
    int connheader; // -1 unspecified, 1 Keep-Alive, 0 Close
    size_t content_length_offset;
    size_t content_offset;
    size_t request_length;
    bool keepalive;
};

struct server {

    int fd;

    int ncs;
    struct client cs[MAX_CLIENTS];
    struct pollfd ps[MAX_CLIENTS+1];
    int          pis[MAX_CLIENTS];
    int         free[MAX_CLIENTS];

    size_t qhead;
    size_t qused;
    struct client *qdata[MAX_CLIENTS];
};
bool     http_server_init(struct server *s, const char *addr, uint16_t port);
void     http_server_free(struct server *s);
uint32_t http_server_wait_request(struct server *s, struct request *r);
void     http_server_set_status(struct server *s, uint32_t handle, int status);
void     http_server_append_header(struct server *s, uint32_t handle, char *text);
void     http_server_append_header_format(struct server *s, uint32_t handle, char *format, ...);
void     http_server_append_content(struct server *s, uint32_t handle, void *data, size_t size);
void     http_server_append_content_string(struct server *s, uint32_t handle, char *text);
void     http_server_append_content_format(struct server *s, uint32_t handle, const char *format, ...);
void     http_server_append_content_format_2(struct server *s, uint32_t handle, const char *format, va_list args);
void     http_server_send_response(struct server *s, uint32_t handle);

#endif /* SERVER_H */

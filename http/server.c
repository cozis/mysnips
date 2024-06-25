#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "server.h"

void print_bytes(const char *prefix,
                 char *bytes,
                 int   count)
{
    bool start = true;
    for (int i = 0; i < count; i++) {
        char c = bytes[i];
        if (start) {
            fprintf(stderr, "%s", prefix);
            start = false;
        }
        if (c == '\r')
            fprintf(stderr, "\\r");
        else if (c == '\n') {
            fprintf(stderr, "\\n\n");
            start = true;
        } else {
            fprintf(stderr, "%c", c);
        }
    }
    if (!start)
        fprintf(stderr, "\n");
}

bool set_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return false;
    flags |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags) == 0;
}

int start_server_ipv4(const char *addr, uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    if (!set_non_blocking(fd)) {
        close(fd);
        return -1;
    }

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct in_addr x;
    if (addr == NULL)
        x.s_addr = htonl(INADDR_ANY);
    else {
        if (inet_pton(AF_INET, addr, &x) != 1)
            return -1;
    }

    struct sockaddr_in y;
    y.sin_family = AF_INET;
    y.sin_port = htons(port);
    y.sin_addr = x;
    if (bind(fd, (struct sockaddr*) &y, sizeof(y))) {
        close(fd);
        return -1;
    }

    int backlog = 32;
    if (listen(fd, backlog)) {
        close(fd);
        return -1;
    }

    return fd;
}

void push_client(struct server *s,
                 struct client *c)
{
    assert(s->qused < MAX_CLIENTS);
    s->qdata[(s->qhead + s->qused) % MAX_CLIENTS] = c;
    s->qused++;
}

struct client *pop_client(struct server *s)
{
    assert(s->qused > 0);
    struct client *c = s->qdata[s->qhead];
    s->qhead = (s->qhead + 1) % MAX_CLIENTS;
    s->qused--;
    return c;
}

static void remove_queued_client(struct server *s,
                                 struct client *c)
{
    assert(c->state == C_QUEUED);
    // TODO
}

void invalidate_handles(struct client *c)
{
    c->gen++;
    if (c->gen == (uint16_t) -1)
        c->gen = 0;
}

void close_client(struct server *s,
                  struct client *c)
{
    int ci = c - s->cs;
    int pi = c->pitem - s->ps;

    invalidate_handles(c);

    if (c->state == C_QUEUED)
        remove_queued_client(s, c);

    close(c->pitem->fd);

    free(c->input.data);
    c->input.data = NULL;
    c->input.size = 0;
    c->input.used = 0;
    free(c->output.data);
    c->output.data = NULL;
    c->output.size = 0;
    c->output.used = 0;
    c->state = C_FREE;
    c->pitem = NULL;

    s->pis[pi-1] = s->pis[s->ncs-1];
    s->ps[pi] = s->ps[s->ncs];
    s->ncs--;

    s->cs[s->pis[pi-1]].pitem = &s->ps[pi];

    s->free[MAX_CLIENTS - (s->ncs + 1)] = ci;
}

size_t find(struct iobuf *hay, char *needle)
{
    size_t needle_len = strlen(needle);
    if (needle_len == 0 || hay->used < needle_len - 1)
        return -1;

    size_t i = 0;
    while (i < hay->used - needle_len + 1 && memcmp(hay->data + i, needle, needle_len))
        i++;

    if (i >= hay->used - needle_len + 1)
        return -1;

    return i;
}

static int ensure_free_space(struct iobuf *b, size_t min)
{
    if (b->data == NULL) {

        size_t init = 512;
        b->data = malloc(init);
        if (b->data == NULL)
            return 0;
        b->size = init;
        b->used = 0;

    } else if (b->size - b->used < min) {

        size_t new_size = 2 * b->size;

        if (new_size - b->used < min)
            new_size = b->used + min;

        char *new_data = malloc(new_size);
        if (new_data == NULL)
            return 0;

        assert(b->data);
        memcpy(new_data, b->data, b->used);
        free(b->data);

        b->data = new_data;
        b->size = new_size;
    }
    return 1;
}

int socket_input(struct server *s, struct client *c)
{
    int fd = c->pitem->fd;

    for (;;) {

        size_t min_recv = 256;
        if (!ensure_free_space(&c->input, min_recv))
            return 0;

        int n = recv(fd, c->input.data + c->input.used, c->input.size - c->input.used, 0);
        if (n == 0)
            return 0; // Peer disconnected.
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return 0;
        }
        assert(n > 0);

        print_bytes(">>> ", c->input.data + c->input.used, n);

        c->input.used += (size_t) n;
    }

    if (c->state == C_IDLE)
        if (find(&c->input, "\r\n\r\n") != (size_t) -1) {
            push_client(s, c);
            c->state = C_QUEUED;
        }

    return 1;
}

int socket_output(struct server *s, struct client *c)
{
    (void) s;

    size_t limit;
    if (c->state == C_CONTENT)
        limit = c->content_length_offset;
    else
        limit = c->output.used;

    int fd = c->pitem->fd;

    int maxzeros = 0;
    size_t sent = 0;
    while (sent < limit) {
        int n = send(fd, c->output.data + sent, limit - sent, 0);
        if (n == 0) {
            maxzeros++;
            if (maxzeros == 128)
                break;
            continue;
        } else {
            maxzeros = 0;
        }
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return 0;
        }

        print_bytes("<<< ", c->output.data + sent, n);

        sent += n;
    }

    if (c->state == C_CONTENT)
        c->content_length_offset -= sent;

    if (sent == limit)
        c->pitem->events &= ~POLLOUT;

    if (sent == c->output.used) {
        free(c->output.data);
        c->output.data = NULL;
        c->output.size = 0;
        c->output.used = 0;
        if (c->state == C_CLOSE)
            return 0;
    } else {
        if (sent > 0) {
            memmove(c->output.data,
                    c->output.data + sent,
                    c->output.used - sent);
            c->output.used -= sent;
        }
    }
    return 1;
}

void process_io(struct server *s)
{
    int timeout = -1;
    int n = poll(s->ps, s->ncs+1, timeout);
    if (n < 0) return;

    if (s->ps[0].revents & POLLIN) {

        while (s->ncs < MAX_CLIENTS) {

            int accept_fd = accept(s->fd, NULL, NULL);
            if (accept_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                assert(0);
                continue;
            }

            if (!set_non_blocking(accept_fd)) {
                close(accept_fd);
                continue;
            }

            struct client *c;
            struct pollfd *p;

            int ft = MAX_CLIENTS - (s->ncs + 1);
            int ci = s->free[ft];
            assert(ci >= 0 && ci < MAX_CLIENTS);

            c = &s->cs[ci];
            assert(c->state == C_FREE);

            p = &s->ps[s->ncs+1];
            p->fd = accept_fd;
            p->events = POLLIN;
            p->revents = 0;

            s->pis[s->ncs] = ci;

            c->state = C_IDLE;
            c->input.data = NULL;
            c->input.used = 0;
            c->input.size = 0;
            c->output.data = NULL;
            c->output.used = 0;
            c->output.size = 0;
            c->pitem = &s->ps[s->ncs+1];
            c->num_served = 0;

            s->ncs++;
        }
    }

    for (int i = 1; i < s->ncs+1; i++) {

        int ci = s->pis[i-1];
        
        struct client *c = &s->cs[ci];
        assert(c->pitem == &s->ps[i]);

        int evs = s->ps[i].revents;
        int ok = 1;

        if (evs & POLLIN)
            ok = socket_input(s, c);

        if (ok && (evs & POLLOUT))
            ok = socket_output(s, c);

        if (!ok) {
            close_client(s, c);
            i--;
        }
    }
}

bool http_server_init(struct server *s,
                      const char *addr,
                      uint16_t port)
{
    int fd = start_server_ipv4(addr, port);
    if (fd < 0)
        return -1;

    s->fd = fd;
    s->ncs = 0;
    s->qhead = 0;
    s->qused = 0;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        s->cs[i].state = C_FREE;
        s->cs[i].gen = 0;
        s->free[i] = MAX_CLIENTS - (i + 1);
    }

    s->ps[0].fd = fd;
    s->ps[0].events = POLLIN;
    s->ps[0].revents = 0;

    return s;
}

void http_server_free(struct server *s)
{
    for (int i = 0; i < s->ncs; i++) {
        int ci = s->pis[i];
        struct client *c = &s->cs[ci];
        close_client(s, c);
    }
    close(s->fd);
}

bool http_server_append_output(struct client *c, void *data, size_t size)
{
    if (data == NULL || size == 0)
        return true;
    if (!ensure_free_space(&c->output, size))
        return false;
    memcpy(c->output.data + c->output.used, data, size);
    c->output.used += size;
    if (size > 0)
        c->pitem->events |= POLLOUT;
    return true;
}

bool http_server_append_output_string(struct client *c, char *str)
{
    return append_output(c, str, str ? strlen(str) : 0);
}

bool http_server_append_output_format_2(struct client *c, const char *f, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);
    int n = vsnprintf(c->output.data + c->output.used,
                      c->output.size - c->output.used,
                      f, args);
    if (n < 0) return false;

    if ((size_t) n + 1 >= c->output.size - c->output.used) {
        if (!ensure_free_space(&c->output, n+1)) {
            va_end(args_copy);
            return false;
        }
        vsnprintf(c->output.data + c->output.used,
                  c->output.size - c->output.used,
                  f, args_copy);
    }
    c->output.used += n;

    if (n > 0)
        c->pitem->events |= POLLOUT;

    va_end(args_copy);
    return true;
}

bool http_server_append_output_format(struct client *c, const char *f, ...)
{
    bool ok;
    va_list args;
    va_start(args, f);
    ok = append_output_format_2(c, f, args);
    va_end(args);
    return ok;
}

const char *reason_phrase(int status)
{
    switch (status) {
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 102: return "Processing";
        case 103: return "Early Hints";
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 203: return "Non-Authoritative Information";
        case 204: return "No Content";
        case 205: return "Reset Content";
        case 206: return "Partial Content";
        case 207: return "Multi-Status";
        case 208: return "Already Reported";
        case 226: return "IM Used";
        case 300: return "Multiple Choices";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 305: return "Use Proxy";
        case 306: return "unused";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 402: return "Payment Required";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 406: return "Not Acceptable";
        case 407: return "Proxy Authentication Required";
        case 408: return "Request Timeout";
        case 409: return "Conflict";
        case 410: return "Gone";
        case 411: return "Length Required";
        case 412: return "Precondition Failed";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        case 416: return "Range Not Satisfiable";
        case 417: return "Expectation Failed";
        case 418: return "I'm a teapot";
        case 421: return "Misdirected Request";
        case 422: return "Unprocessable Content";
        case 423: return "Locked";
        case 424: return "Failed Dependency";
        case 425: return "Too Early";
        case 426: return "Upgrade Required";
        case 428: return "Precondition Required";
        case 429: return "Too Many Requests";
        case 431: return "Request Header Fields Too Large";
        case 451: return "Unavailable For Legal Reasons";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        case 506: return "Variant Also Negotiates";
        case 507: return "Insufficient Storage";
        case 508: return "Loop Detected";
        case 510: return "Not Extended";
        case 511: return "Network Authentication Required";
    }
    return "???";
}

void send_basic_response_and_close(struct server *s, struct client *c, int minor, int status)
{
    bool ok;
    if (minor == 0)
        ok = append_output_format(c,
            "HTTP/1.0 %d %s\r\n"
            "Content-Length: 0\r\n"
            "\r\n",
            status, reason_phrase(status));
    else {
        assert(minor == 1);
        ok = append_output_format(c, 
            "HTTP/1.1 %d %s\r\n"
            "Content-Length: 0\r\n"
            "Connection: Close\r\n"
            "\r\n",
            status, reason_phrase(status));
    }

    if (!ok) {
        close_client(s, c);
    } else {
        c->state = C_CLOSE;
    }
}

uint32_t http_server_wait_request(struct server *s,
                                  struct request *r)
{
    struct client *c;
    for (;;) {

        while (s->qused == 0)
            process_io(s);

        c = pop_client(s);
        c->state = C_POPPED;

        size_t i = find(&c->input, "\r\n\r\n");
        assert(i != (size_t) -1);

        char  *head = c->input.data;
        size_t head_len = i + 4;

        if (parse_request_head(head, head_len, r) != P_OK) {
            send_basic_response_and_close(s, c, 1, 400);
            continue;
        }

        if (r->major != 1 || (r->minor != 0 && r->minor != 1)) {
            send_basic_response_and_close(s, c, 1, 505);
            continue;
        }

        size_t content_length;
        {
            struct slice content_length_header_value;
            if (!find_header(r, "Content-Length", &content_length_header_value)) {
                int transfer_encoding = find_and_parse_transfer_encoding(r);
                if (transfer_encoding < 0) {
                    send_basic_response_and_close(s, c, r->minor, 400);
                    continue;
                }
                if (transfer_encoding & T_CHUNKED) {
                    send_basic_response_and_close(s, c, r->minor, 411); // 411 Length required
                    continue;
                }
                content_length = 0;
            } else {
                content_length = parse_content_length(content_length_header_value);
                if (content_length == (size_t) -1) {
                    send_basic_response_and_close(s, c, r->minor, 400);
                    continue;
                }
            }
        }

        size_t total_request_length = head_len + content_length;
        if (c->input.used < total_request_length)
            continue;

        r->content.data = head + head_len;
        r->content.size = content_length;
        c->state = C_STATUS;
        c->minor = r->minor;
        c->connheader = -1;
        c->request_length = total_request_length;
        break;
    }
    _Static_assert(MAX_CLIENTS < (1<<16)-1);
    assert(c->gen < (1<<16)-1);
    uint32_t gen = c->gen;
    uint32_t idx = c - s->cs;
    uint32_t handle = gen | (idx << 16);
    return handle;
}

struct client *client_from_handle(struct server *s, uint32_t handle)
{
    uint16_t gen = handle & 0xFFFF;
    uint16_t idx = handle >> 16;
    if (idx >= s->ncs)
        return NULL;
    struct client *c = &s->cs[idx];
    if (c->gen != gen)
        return NULL;
    return c;
}

void http_server_set_status(struct server *s, uint32_t handle, int status)
{
    struct client *c = client_from_handle(s, handle);
    if (c == NULL) return;

    if (c->state != C_STATUS) {
        fprintf(stderr, "Warning: Can't set the response status at this time\n");
        return;
    }

    if (!append_output_format(c, "HTTP/1.%d %d %s\r\n", c->minor, status, reason_phrase(status)))
        close_client(s, c);
    else {
        c->state = C_HEADER;
    }
}

bool http_server_parse_header(char *src, size_t len,
                              struct header *h)
{
    size_t cur = 0;
    size_t start;

    start = cur;
    while (cur < len && src[cur] != ':')
        cur++;
    h->name.data = src + start;
    h->name.size = len - start;

    if (cur == len)
        return false;
    cur++;

    h->value.data = src + cur;
    h->value.size = len - cur;
    return true;
}

void http_server_append_header_format(struct server *s, uint32_t handle, char *format, ...)
{
    int num;
    char mem[1<<10];
    va_list args;
    va_start(args, format);
    num = vsnprintf(mem, sizeof(mem), format, args);
    va_end(args);

    if (num < 0) {
        fprintf(stderr, "Invalid header format\n");
        return;
    }
    if (num >= (int) sizeof(mem)) {
        fprintf(stderr, "Header evaluated from format is too large\n");
        return;
    }
    mem[num] = '\0';

    append_header(s, handle, mem);
}

void http_server_append_header(struct server *s, uint32_t handle, char *text)
{
    struct client *c = client_from_handle(s, handle);
    if (c == NULL)
        return;

    if (c->state != C_HEADER)
        return;

    assert(text);
    size_t text_len = strlen(text);

    struct header h;
    if (!parse_header(text, text_len, &h))
        return;    

    if (match_header_name(h.name, "Content-Length"))
        return;
    
    if (match_header_name(h.name, "Connection")) {

        if (match_header_value(h.value, "Keep-Alive")) {
            c->connheader = 1;
        } else if (match_header_value(h.value, "Close")) {
            c->connheader = 0;
        } else {
            c->connheader = -1;
        }

    } else {
        if (!append_output(c, text, text_len))
            return;
        if (!append_output_string(c, "\r\n")) {
            close_client(s, c);
            return;
        }
    }
}

bool append_special_headers(struct server *s, struct client *c)
{
    if (c->minor == 1) {
        bool ok;
        if (c->connheader != 0 && c->num_served < 5 && s->ncs < 0.7 * MAX_CLIENTS) {
            ok = append_output_string(c, "Connection: Keep-Alive\r\n");
            c->keepalive = true;
        } else {
            ok = append_output_string(c, "Connection: Close\r\n");
            c->keepalive = false;
        }
        if (!ok) {
            close_client(s, c);
            return false;
        }
    }

    if (!append_output_string(c, "Content-Length: ")) {
        close_client(s, c);
        return false;
    }
    c->content_length_offset = c->output.used;
    if (!append_output_string(c, "         \r\n")) {
        close_client(s, c);
        return false;
    }
    if (!append_output_string(c, "\r\n")) {
        close_client(s, c);
        return false;
    }
    c->content_offset = c->output.used;
    c->state = C_CONTENT;
    return true;
}

void http_server_append_content(struct server *s, uint32_t handle, void *data, size_t size)
{
    struct client *c = client_from_handle(s, handle);
    if (c == NULL) return;

    if (c->state == C_HEADER)
        if (!append_special_headers(s, c))
            return;

    if (c->state != C_CONTENT)
        return;

    if (!append_output(c, data, size)) {
        close_client(s, c);
        return;
    }
}

void http_server_append_content_format(struct server *s, uint32_t handle, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    append_content_format_2(s, handle, format, args);
    va_end(args);
}

void http_server_append_content_format_2(struct server *s, uint32_t handle, const char *format, va_list args)
{
    struct client *c = client_from_handle(s, handle);
    if (c == NULL) return;

    if (c->state == C_HEADER)
        if (!append_special_headers(s, c))
            return;

    if (c->state != C_CONTENT)
        return;

    if (!append_output_format_2(c, format, args)) {
        close_client(s, c);
        return;
    }
}

void http_server_append_content_string(struct server *s, uint32_t handle, char *text)
{
    append_content(s, handle, text, strlen(text));
}

void http_server_send_response(struct server *s, uint32_t handle)
{
    struct client *c = client_from_handle(s, handle);
    if (c == NULL) return;

    if (c->state == C_STATUS) {
        set_status(s, handle, 200);
        c = client_from_handle(s, handle);
        if (c == NULL) return;
    }

    if (c->state == C_HEADER) {
        append_content(s, handle, NULL, 0);
        c = client_from_handle(s, handle);
        if (c == NULL) return;
    }

    if (c->state != C_CONTENT) {
        close_client(s, c);
        return;
    }

    {
        size_t content_length = c->output.used - c->content_offset;

        char buf[16];
        int n = snprintf(buf, sizeof(buf), "%ld", content_length);
        if (n < 1) {
            close_client(s, c);
            return;
        }
        assert(n < 10);
        memcpy(c->output.data + c->content_length_offset, buf, n);
        c->pitem->events |= POLLOUT;
    }

    if (c->input.used == c->request_length) {
        free(c->input.data);
        c->input.data = NULL;
        c->input.used = 0;
        c->input.size = 0;
    } else {
        memmove(c->input.data,
                c->input.data + c->request_length,
                c->input.used - c->request_length);
        c->input.used -= c->request_length;
    }
    c->request_length = 0;

    c->num_served++;
    if (c->keepalive) {

        if (find(&c->input, "\r\n\r\n") != (size_t) -1) {
            push_client(s, c);
            c->state = C_QUEUED;
        } else {
            c->state = C_IDLE;
        }
        c->keepalive = false;

    } else {
        c->state = C_CLOSE;
    }

    invalidate_handles(c);
}

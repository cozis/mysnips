
#include <assert.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define poll WSAPoll
#define close closesocket
#else
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#include "tcp.h"
#include "utils.h"
#include "byte_queue.h"

#define MAX_SERVERS 1
#define MAX_CLIENTS 1

#define MAX_EVENTS_PER_CLIENT 3

typedef struct TCPClient TCPClient;

typedef enum {
    TCP_SERVER_FREE = 0,
    TCP_SERVER_USED,
} TCPServerState;

typedef struct {

    TCPServerState state;
    uint16_t gen;

    int fd;

    int count;
    int capacity;
    TCPClient *clients;

    int events_head;
    int events_count;
    TCPEvent *events;
} TCPServer;

typedef enum {

    // TCPClient structure is unused
    TCP_CLIENT_FREE = 0,

    // TCPClient structure is used
    TCP_CLIENT_USED,

    // Peer hang up but the user didn't close yet 
    TCP_CLIENT_HANGUP,

    // User closed but we are still flushing
    TCP_CLIENT_CLOSE,

} TCPClientState;

struct TCPClient {

    TCPClientState state;

    /*
     * When stand-alone, these two bools are used as
     * event queue.
     */
    bool event_data;
    bool event_disconnect;

    /*
     * Any time state is set to TCP_CLIENT_FREE, this
     * is incremented. If its value becomes 0 it is set
     * to 1.
     */
    uint16_t gen;

    // -1 if state=TCP_CLIENT_FREE, else is a valid descriptor
    int fd;

    void *user_ptr;

    ByteQueue input;
    ByteQueue output;
};

static TCPServer servers[MAX_SERVERS];
static TCPClient clients[MAX_CLIENTS];

#define GEN_UPPER_BOUND_LOG2 12
#define GEN_UPPER_BOUND (1U << GEN_UPPER_BOUND_LOG2)
#define GEN_MASK (GEN_UPPER_BOUND-1)

static TCPHandle pack_handle(uint32_t sdx, uint32_t idx, uint32_t gen, uint32_t typ)
{
    /*
     * ossssiii iiiiiiii iiiggggg gggggggv
     *
     * o - 1 bit  - object type (server=1, client=0)
     * s - 4 bits - parent server index plus 1 (ignored when o=1)
     * i - 14 bits - object index
     * g - 12 bits - generation
     * v - 1 bit   - This is set to 0 so that the handle with all 1 bits is the invalid handle
     */
    assert(typ < 1U<<1);
    assert(sdx < 1U<<4);
    assert(idx < 1U<<14);
    assert(gen < GEN_UPPER_BOUND);

    return (typ << 31)
         | (sdx << 27)
         | (idx << 12)
         | (gen <<  1);
}

static void unpack_handle(TCPHandle handle, uint32_t *idx,
                          uint32_t *sdx, uint32_t *gen, uint32_t *typ)
{
    *typ = (handle >> 31);
    *sdx = (handle >> 27) & 0xF;
    *idx = (handle >> 12) & 0x3FFF;
    *gen = (handle >>  1) & GEN_MASK;
}

static TCPHandle handle_for_server(TCPServer *server)
{
    uint32_t sdx = 0;
    uint32_t idx = (uint32_t) (server - servers);
    uint32_t gen = server->gen;
    uint32_t typ = 1; // server
    return pack_handle(sdx, idx, gen, typ);
}

static TCPHandle handle_for_client(TCPServer *server, TCPClient *client)
{
    uint32_t sdx = server ? 1 + (uint32_t) (server - servers) : 0;
    uint32_t idx = server ? (uint32_t) (client - server->clients) : (uint32_t) (client - clients);
    uint32_t gen = client->gen;
    uint32_t typ = 0; // client
    return pack_handle(sdx, idx, gen, typ);
}

// TEMP
#include <stdio.h>

static TCPServer *server_from_handle(TCPHandle handle)
{
    if (handle == TCP_INVALID)
        return NULL;

    uint32_t idx;
    uint32_t sdx;
    uint32_t gen;
    uint32_t typ;
    unpack_handle(handle, &idx, &sdx, &gen, &typ);

    if (typ != 1)
        return NULL; // Handle is for a client

    if (idx >= MAX_SERVERS)
        return NULL;

    TCPServer *server = &servers[idx];
    if (server->gen != gen)
        return NULL; // Handle was invalidated
    assert(server->state == TCP_SERVER_USED);

    return server;
}

static TCPClient *client_from_handle(TCPHandle handle, TCPServer **server)
{
    assert(server);

    if (handle == TCP_INVALID) {
        *server = NULL;
        return NULL;
    }

    uint32_t idx;
    uint32_t sdx;
    uint32_t gen;
    uint32_t typ;
    unpack_handle(handle, &idx, &sdx, &gen, &typ);

    if (typ != 0)
        return NULL; // Handle is for a server

    TCPClient *client;

    if (sdx == 0) {

        // Client is stand-alone

        if (idx >= MAX_CLIENTS)
            return NULL; // Index out of bounds
        client = &clients[idx];

        *server = NULL;

    } else {

        // Client has a parent server
        
        sdx--;
        if (sdx >= MAX_SERVERS)
            return NULL; // Server index out of bounds
        TCPServer *server_ = &servers[sdx];

        if (idx >= (uint32_t) server_->capacity)
            return NULL; // Client index out of bounds
        client = &server_->clients[idx];

        if (gen != client->gen)
            return NULL; // Handle was invalidated

        *server = server_;
    }

    assert(client->state != TCP_CLIENT_FREE);
    return client;
}

static TCPServer *get_server_struct(void)
{
    for (int i = 0; i < MAX_SERVERS; i++)
        if (servers[i].state == TCP_SERVER_FREE)
            return &servers[i];
    abort();
}

static TCPClient *get_client_struct(void)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].state == TCP_CLIENT_FREE)
            return &clients[i];
    abort();
}

bool set_socket_blocking(int fd, bool blocking)
{
   if (fd < 0) return false;

#ifdef _WIN32
   unsigned long mode = blocking ? 0 : 1;
   return (ioctlsocket(fd, FIONBIO, &mode) == 0);
#else
   int flags = fcntl(fd, F_GETFL, 0);
   if (flags == -1) return false;
   flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
   return fcntl(fd, F_SETFL, flags) == 0;
#endif
}

TCPHandle tcp_server_create(const char *addr, uint16_t port, int max_clients)
{
    TCPServer *server = get_server_struct();

    #ifdef _WIN32
    WORD wVersionRequested = MAKEWORD(2, 2);

    WSADATA data;
    int err = WSAStartup(wVersionRequested, &data);
    if (err != 0)
       return TCP_INVALID;
    #endif

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    struct in_addr addr2;
    if (addr == NULL)
        addr2.s_addr = htonl(INADDR_ANY);
    else {
        if (inet_pton(AF_INET, addr, &addr2) != 1) {
            close(fd);
            return TCP_INVALID;
        }
    }

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*) &one, sizeof(one));

    struct sockaddr_in addr3;
    addr3.sin_family = AF_INET;
    addr3.sin_port = htons(port);
    addr3.sin_addr = addr2;
    if (bind(fd, (struct sockaddr*) &addr3, sizeof(addr3))) {
        close(fd);
        return TCP_INVALID;
    }

    int backlog = 32;
    if (listen(fd, backlog)) {
        close(fd);
        return TCP_INVALID;
    }

    if (!set_socket_blocking(fd, false)) {
        close(fd);
        return TCP_INVALID;
    }

    void *mem = malloc(max_clients * sizeof(TCPClient) + MAX_EVENTS_PER_CLIENT * max_clients * sizeof(TCPEvent));
    if (!mem) {
        close(fd);
        return TCP_INVALID;
    }

    server->state = TCP_SERVER_USED;
    server->fd = fd;
    server->count = 0;
    server->capacity = max_clients;
    server->clients  = mem;
    server->events_head = 0;
    server->events_count = 0;
    server->events = (TCPEvent*) ((TCPClient*) mem + max_clients);

    for (int i = 0; i < max_clients; i++) {
        server->clients[i].state = TCP_CLIENT_FREE;
        server->clients[i].gen = 1;
        server->clients[i].fd = -1;
    }

    return handle_for_server(server);
}

void tcp_server_delete(TCPHandle handle)
{
    TCPServer *server = server_from_handle(handle);
    if (server == NULL) abort();

    for (int i = 0; i < server->capacity; i++) {
        TCPClient *client = &server->clients[i];
        if (client->fd > -1) close(client->fd);
    }
    close(server->fd);
    free(server->clients);

    server->state = TCP_SERVER_FREE;

    server->gen++;
    if (server->gen == GEN_UPPER_BOUND)
        server->gen = 0;
}

static void push_event(TCPServer *server, TCPClient *client, TCPEventType type)
{
    uint32_t client_index = (uint16_t) (client - server->clients);
    assert(client_index <= UINT16_MAX);

    TCPHandle handle = handle_for_client(server, client);
    TCPEvent event = {.type=type, .handle=handle};

    int events_capacity = server->capacity * MAX_EVENTS_PER_CLIENT;
    assert(server->events_count < events_capacity);

    server->events[(server->events_head + server->events_count) % events_capacity] = event;
    server->events_count++;
}

static void free_client(TCPClient *client)
{
    client->state = TCP_CLIENT_FREE;

    close(client->fd);
    client->fd = -1;

    byte_queue_free(&client->input);
    byte_queue_free(&client->output);

    // Regenerate
    client->gen++;
    if (client->gen == GEN_UPPER_BOUND)
        client->gen = 0;
}

/*
 * Returns:
 *   -1 Error
 *    0 Disconnected
 *    1 OK
 */
static int move_bytes_from_socket_to_queue(int fd, ByteQueue *queue, size_t *moved)
{
    size_t start = byte_queue_used_space(queue);
    bool disconnect = false;
    for (;;) {

        if (!byte_queue_ensure_min_free_space(queue, 256)) {
            fprintf(stderr, "out of memory\n");
            return -1;
        }
        char  *dst = byte_queue_start_write(queue);
        size_t max = byte_queue_free_space(queue);

        int n = recv(fd, dst, max, 0);
        if (n == 0) {
            fprintf(stderr, "peer disconnected!\n");
            disconnect = true;
            break;
        }
        if (n < 0) {
            #ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK)
                break;
            #else
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            #endif
            perror("recv");
            return -1;
        }

        byte_queue_end_write(queue, (size_t) n);
    }

    if (moved) *moved = byte_queue_used_space(queue) - start;
    return !disconnect;
}

static bool move_bytes_from_queue_to_socket(int fd, ByteQueue *queue)
{
    int zeros = 0;
    int max_zero_succession = 10;
    while (byte_queue_used_space(queue) > 0) {

        char  *src = byte_queue_start_read(queue);
        size_t len = byte_queue_used_space(queue);

        int n = send(fd, src, len, 0);
        if (n == 0) {
            zeros++;
            if (zeros == max_zero_succession)
                return false; // To many zeros returned one after the other
        } else {
            zeros = 0;
        }
        if (n < 0) {
            #ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK)
                break;
            #else
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            #endif
            perror("send");
            return false;
        }

        byte_queue_end_read(queue, (size_t) n);
    }
    return true;
}

void tcp_server_poll(TCPHandle handle)
{
    TCPServer *server = server_from_handle(handle);
    if (server == NULL) abort();

    assert(server->events_count == 0);

    #define MAX_POLL 128

    TCPClient    *poll_clients[MAX_POLL-1];
    struct pollfd poll_array[MAX_POLL];
    int           poll_count = 0;

    if (server->count >= COUNTOF(poll_array))
        abort();

    for (int i = 0; i < server->capacity; i++) {

        TCPClient *client = &server->clients[i];
        if (client->state == TCP_CLIENT_FREE ||
            client->state == TCP_CLIENT_HANGUP)
            continue;

        struct pollfd *desc = &poll_array[poll_count];
        poll_clients[poll_count] = client;

        if (client->state == TCP_CLIENT_USED) {

            desc->fd = client->fd;
            desc->events = POLLIN | POLLHUP;
            if (byte_queue_used_space(&client->output) > 0)
                desc->events |= POLLOUT;
            desc->revents = 0;

        } else {
            assert(client->state == TCP_CLIENT_CLOSE);

            desc->fd = client->fd;
            desc->events = POLLOUT | POLLHUP;
            desc->revents = 0;
        }

        poll_count++;
    }

    {
        struct pollfd *desc = &poll_array[poll_count];
        desc->fd = server->fd;
        desc->events = POLLIN;
        desc->revents = 0;
        poll_count++;
    }

    int n = poll(poll_array, poll_count, 0);
    if (n == 0) return;

    /*
     * Process incoming connections
     */
    if (poll_array[poll_count-1].revents & POLLIN) {
        
        // Accept new connections
        while (server->count < server->capacity) {

            int fd = accept(server->fd, NULL, NULL);
            if (fd < 0) {
                #ifdef _WIN32
                if (WSAGetLastError() == WSAEWOULDBLOCK)
                    break;
                #else
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                #endif
                continue;
            }

            if (!set_socket_blocking(fd, false)) {
                close(fd);
                continue;
            }

            int i = 0;
            while (server->clients[i].state != TCP_CLIENT_FREE) {
                i++;
                assert(i < server->capacity);
            }

            TCPClient *client = &server->clients[i];
            client->state = TCP_CLIENT_USED;
            client->fd = fd;
            client->user_ptr = NULL;
            byte_queue_init(&client->input);
            byte_queue_init(&client->output);
            server->count++;

            push_event(server, client, TCP_EVENT_CONNECT);
        }
    }

    /*
     * Process input
     */
    for (int i = 0; i < poll_count-1; i++) {

        TCPClient *client = poll_clients[i];
        if (client->state < TCP_CLIENT_USED) continue;

        if (poll_array[i].revents & (POLLIN | POLLHUP)) {

            size_t moved;
            int ret = move_bytes_from_socket_to_queue(client->fd, &client->input, &moved);
            assert(ret == -1 || ret == 0 || ret == 1);

            bool error = false;
            bool disconnect = false;
            switch (ret) {
                case -1: error = true; break;
                case  0: disconnect = true; break;
                case  1: break;
            }
            if (moved > 0)
                push_event(server, client, TCP_EVENT_DATA);
            if (disconnect || error) {
                client->state = TCP_CLIENT_HANGUP;
                push_event(server, client, TCP_EVENT_DISCONNECT);
            }
        }
    }

    /*
     * Process output
     */
    for (int i = 0; i < poll_count-1; i++) {

        TCPClient *client = poll_clients[i];
        if (client->state != TCP_CLIENT_USED &&
            client->state != TCP_CLIENT_CLOSE) continue;

        if (poll_array[i].revents & POLLOUT) {

            bool error = !move_bytes_from_queue_to_socket(client->fd, &client->output);

            if (client->state == TCP_CLIENT_CLOSE) {
                // User closed but we're still flushing
                if (error || byte_queue_used_space(&client->output) == 0)
                    free_client(client);
            } else {
                assert(client->state == TCP_CLIENT_USED);
                if (error) {
                    client->state = TCP_CLIENT_HANGUP;
                    push_event(server, client, TCP_EVENT_DISCONNECT);
                }
            }
        }
    }
}

TCPEvent tcp_server_event(TCPHandle handle)
{
    TCPServer *server = server_from_handle(handle);
    if (server == NULL) abort();

    if (server->events_count == 0) {
        return (TCPEvent) {
            .type=TCP_EVENT_NONE,
            .handle=TCP_INVALID,
        };
    }

    TCPEvent event = server->events[server->events_head];

    size_t events_capacity = server->capacity * MAX_EVENTS_PER_CLIENT;
    server->events_head = (server->events_head + 1) % events_capacity;
    server->events_count--;

    return event;
}

void tcp_client_close(TCPHandle handle)
{
    TCPServer *server;
    TCPClient *client = client_from_handle(handle, &server);
    if (client == NULL) abort();

    if (client->state == TCP_CLIENT_HANGUP || byte_queue_used_space(&client->output) == 0)
        free_client(client);
    else {
        if (server) {
            client->state = TCP_CLIENT_CLOSE;
        } else {
            // TODO: If this is a stand-alone client then we need
            //       some way to continue flushing before closing
            //       the socket. For now we just free
            free_client(client);
        }
    }
}

char *tcp_client_get_input_data(TCPHandle handle)
{
    TCPServer *server;
    TCPClient *client = client_from_handle(handle, &server);
    if (client == NULL) abort();

    return byte_queue_start_read(&client->input);
}

size_t tcp_client_get_input_size(TCPHandle handle)
{
    TCPServer *server;
    TCPClient *client = client_from_handle(handle, &server);
    if (client == NULL) abort();

    return byte_queue_used_space(&client->input);
}

void *tcp_client_get_user_ptr(TCPHandle handle)
{
    TCPServer *server;
    TCPClient *client = client_from_handle(handle, &server);
    if (client == NULL) abort();

    return client->user_ptr;
}

void tcp_client_set_user_ptr(TCPHandle handle, void *user_ptr)
{
    TCPServer *server;
    TCPClient *client = client_from_handle(handle, &server);
    if (client == NULL) abort();

    client->user_ptr = user_ptr;
}

void tcp_client_read(TCPHandle handle, size_t num)
{
    TCPServer *server;
    TCPClient *client = client_from_handle(handle, &server);
    if (client == NULL) abort();

    byte_queue_end_read(&client->input, num);
}

void tcp_client_write(TCPHandle handle, const void *data, size_t size)
{
    TCPServer *server;
    TCPClient *client = client_from_handle(handle, &server);
    if (client == NULL) abort();

    if (client->state != TCP_CLIENT_USED)
        return;

    if (!byte_queue_ensure_min_free_space(&client->output, size))
        abort();

    char *dst = byte_queue_start_write(&client->output);
    memcpy(dst, data, size);

    byte_queue_end_write(&client->output, size);
}

TCPHandle tcp_client_create(const char *addr, uint16_t port)
{
    TCPClient *client = get_client_struct();

    int fd;
    
    #ifdef _WIN32
    WORD wVersionRequested = MAKEWORD(2, 2);

    WSADATA data;
    int err = WSAStartup(wVersionRequested, &data);
    if (err != 0) return TCP_INVALID;
    #endif

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return TCP_INVALID;

    struct in_addr parsed_addr;
    if (addr == NULL)
        parsed_addr.s_addr = htons(INADDR_ANY);
    else {
        if (inet_pton(AF_INET, addr, &parsed_addr) != 1) {
            close(fd);
            return TCP_INVALID;
        }
    }

    struct sockaddr_in buffer;
    buffer.sin_family = AF_INET;
    buffer.sin_port = htons(port);
    buffer.sin_addr = parsed_addr;
    memset(&buffer.sin_zero, 0, sizeof(buffer.sin_zero));
    int ret = connect(fd, (struct sockaddr*) &buffer, sizeof(buffer));
    if (ret < 0) {
        close(fd);
        return TCP_INVALID;
    }

    if (!set_socket_blocking(fd, false)) {
        close(fd);
        return TCP_INVALID;
    }

    client->state = TCP_CLIENT_USED;
    client->fd = fd;
    client->user_ptr = NULL;
    client->event_data = false;
    client->event_disconnect = false;
    byte_queue_init(&client->input);
    byte_queue_init(&client->output);

    return handle_for_client(NULL, client);
}

void tcp_client_poll(TCPHandle handle, int timeout)
{
    TCPServer *server;
    TCPClient *client;
    client = client_from_handle(handle, &server);
    if (server != NULL) abort(); // This can only be called on stand-alone clients

    assert(client->state == TCP_CLIENT_USED || client->state == TCP_CLIENT_HANGUP);

    if (client->event_data || client->event_disconnect)
        return;

    struct pollfd desc;
    desc.fd = client->fd;
    desc.events = POLLIN | POLLHUP;
    if (byte_queue_used_space(&client->output) > 0)
        desc.events |= POLLOUT;
    desc.revents = 0;
    int n = poll(&desc, 1, timeout);
    if (n < 0) abort();

    if (desc.revents & (POLLIN | POLLHUP)) {

        size_t moved;
        int ret = move_bytes_from_socket_to_queue(client->fd, &client->input, &moved);
        assert(ret == -1 || ret == 0 || ret == 1);

        bool error = false;
        bool disconnect = false;
        switch (ret) {
            case -1: error = true; break;
            case  0: disconnect = true; break;
            case  1: break;
        }
        if (moved > 0)
            client->event_data = true;
        if (error || disconnect) {
            client->state = TCP_CLIENT_HANGUP;
            client->event_disconnect = true;
        }
    }

    if (desc.revents & POLLOUT) {

        bool error = !move_bytes_from_queue_to_socket(client->fd, &client->output);

        if (client->state == TCP_CLIENT_CLOSE) {
            // User closed but we're still flushing
            if (error || byte_queue_used_space(&client->output) == 0)
                free_client(client);
        } else {
            assert(client->state == TCP_CLIENT_USED);
            if (error) {
                client->state = TCP_CLIENT_HANGUP;
                client->event_disconnect = true;
            }
        }
    }
}

TCPEvent tcp_client_event(TCPHandle handle)
{
    TCPServer *server;
    TCPClient *client;
    client = client_from_handle(handle, &server);
    if (server != NULL) abort(); // This can only be called on stand-alone clients

    TCPEvent event;
    if (client->event_data) {
        event.type = TCP_EVENT_DATA;
        event.handle = handle;
        client->event_data = false;
    } else if (client->event_disconnect) {
        event.type = TCP_EVENT_DISCONNECT;
        event.handle = handle;
        client->event_disconnect = false;
    } else {
        event.type = TCP_EVENT_NONE;
        event.handle = TCP_INVALID;
    }

    return event;
}

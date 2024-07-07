#ifndef MCO_TCP_H
#define MCO_TCP_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t TCPHandle;
#define TCP_INVALID (~(TCPHandle) 0)

typedef enum {
    TCP_EVENT_NONE,
    TCP_EVENT_DATA,
    TCP_EVENT_CONNECT,
    TCP_EVENT_DISCONNECT,
} TCPEventType;

typedef struct {
    TCPEventType type;
    TCPHandle handle;
} TCPEvent;

TCPHandle tcp_server_create(const char *addr, uint16_t port, int max_clients);
void      tcp_server_delete(TCPHandle handle);
void      tcp_server_poll(TCPHandle handle);

TCPEvent tcp_server_event(TCPHandle handle);

TCPHandle tcp_client_create(const char *addr, uint16_t port);
void      tcp_client_delete(TCPHandle handle);
void      tcp_client_poll(TCPHandle handle, int timeout);
TCPEvent  tcp_client_event(TCPHandle handle);
void      tcp_client_close(TCPHandle handle);
char     *tcp_client_get_input_data(TCPHandle handle);
size_t    tcp_client_get_input_size(TCPHandle handle);
void     *tcp_client_get_user_ptr(TCPHandle handle);
void      tcp_client_set_user_ptr(TCPHandle handle, void *user_ptr);
void      tcp_client_read(TCPHandle handle, size_t num);
void      tcp_client_write(TCPHandle handle, const void *data, size_t size);

#endif
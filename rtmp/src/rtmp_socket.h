#ifndef __RTMP_SOCKET_H__
#define __RTMP_SOCKET_H__
#include <FreeRTOS.h>
#include <task.h>
#include <nonstdlib.h>
#include <string.h>
#include <stdio.h>
#include <semphr.h>
#include "lwip/sockets.h"

#define SOCKET_ETIME        EWOULDBLOCK
#define SOCKET_ECONNRESET   ECONNRESET
#define SOCKET_ERRNO()      errno
#define SOCKET_RESET(fd) fd = -1; (void)0
#define SOCKET_CLOSE(fd) \
        if (fd > 0) {\
            closesocket(fd); \
            fd = -1; \
        } \
        (void)0
        
#define SOCKET_VALID(x) (x >= 0)
#define SOCKET_SETUP() (void)0
#define SOCKET_CLEANUP() (void)0
#define SOCKET  int
#define SRS_CONSTS_NO_TMMS ((int64_t) -1LL)
#define SRS_SOCKET_DEFAULT_TMMS (10 *1000)/(portTICK_RATE_MS)


typedef void*  srs_hijack_io_t;

struct SrsBlockSyncSocket{
    SOCKET fd;
    int64_t rbytes;
    int64_t sbytes;
    // The send/recv timeout in ms.
    int64_t rtm;
    int64_t stm;
    SemaphoreHandle_t mutex;
};

struct iovec { void *iov_base; size_t iov_len; };

srs_hijack_io_t srs_hijack_io_create();
void srs_hijack_io_destroy(srs_hijack_io_t ctx);
int srs_hijack_io_create_socket(srs_hijack_io_t ctx);
int srs_hijack_io_connect(srs_hijack_io_t ctx, const char* server_ip, int port);
int srs_hijack_io_read(srs_hijack_io_t ctx, void* buf, size_t size, ssize_t* nread);
int srs_hijack_io_set_recv_timeout(srs_hijack_io_t ctx, int64_t tm);
int64_t srs_hijack_io_get_recv_timeout(srs_hijack_io_t ctx);
int64_t srs_hijack_io_get_recv_bytes(srs_hijack_io_t ctx);
int srs_hijack_io_set_send_timeout(srs_hijack_io_t ctx, int64_t tm);
int64_t srs_hijack_io_get_send_timeout(srs_hijack_io_t ctx);
int64_t srs_hijack_io_get_send_bytes(srs_hijack_io_t ctx);
int srs_hijack_io_writev(srs_hijack_io_t ctx,struct iovec *iov, int iov_size, size_t* nwrite);
int srs_hijack_io_is_never_timeout(srs_hijack_io_t ctx, int64_t tm);
int srs_hijack_io_read_fully(srs_hijack_io_t ctx, void* buf, size_t size, ssize_t* nread);
int srs_hijack_io_write(srs_hijack_io_t ctx, void* buf, size_t size, ssize_t* nwrite);


struct  SimpleSocketStream {
    srs_hijack_io_t io;
};

struct SimpleSocketStream* srs_simple_socket_init();
void srs_simple_socket_uinit(struct SimpleSocketStream* simplesocketstream);
srs_hijack_io_t srs_simple_socket_hijack_io(struct SimpleSocketStream* simplesocketstream);
int srs_simple_socket_create_socket(struct          SimpleSocketStream* simplesocketstream);
int srs_simple_socket_connect(struct        SimpleSocketStream* simplesocketstream,const char* server_ip, int port);
int srs_simple_socket_read(struct       SimpleSocketStream* simplesocketstream,void* buf, size_t size, ssize_t* nread);
void srs_simple_socket_set_recv_timeout(struct           SimpleSocketStream* simplesocketstream,int64_t recv_time_out);
int64_t srs_simple_socket_get_recv_timeout(struct SimpleSocketStream* simplesocketstream);
int64_t srs_simple_socket_get_recv_bytes(struct           SimpleSocketStream* simplesocketstream);
void srs_simple_socket_set_send_timeout(struct           SimpleSocketStream* simplesocketstream,int64_t send_time_out);
int64_t srs_simple_socket_get_send_timeout(struct          SimpleSocketStream* simplesocketstream);
int64_t srs_simple_socket_get_send_bytes(struct          SimpleSocketStream* simplesocketstream);
int srs_simple_socket_writev(struct        SimpleSocketStream* simplesocketstream,struct iovec *iov, int iov_size, size_t* nwrite);
bool srs_simple_socket_is_never_timeout(struct          SimpleSocketStream* simplesocketstream,int64_t tm);
int srs_simple_socket_read_fully(struct         SimpleSocketStream* simplesocketstream,void* buf, size_t size, ssize_t* nread);
int srs_simple_socket_write(struct SimpleSocketStream* simplesocketstream,void* buf, size_t size, ssize_t* nwrite);
#endif

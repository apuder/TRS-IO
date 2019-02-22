
#ifndef __TCPIP_H__
#define __TCPIP_H__

//#include "defs.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>

#define IP_VERSION_MAJOR 1
#define IP_VERSION_MINOR 0

#define IP_COMMAND_SUCCESS  0
#define IP_COMMAND_ERROR    1

#define IP_SOCKET_FAMILY_AF_INET  1
#define IP_SOCKET_FAMILY_AF_INET6 2

#define IP_SOCKET_TYPE_SOCK_STREAM  1
#define IP_SOCKET_TYPE_SOCK_DGRAM   2

#define IP_RECV_BLOCKING    0
#define IP_RECV_NONBLOCKING 1

#define IP_HOST_FORMAT_IP_ADDRESS 0
#define IP_HOST_FORMAT_HOSTNAME   1

#define IP_STATE_READY              0
#define IP_STATE_READ_COMMAND       1
#define IP_STATE_READ_SOCKET_FAMILY 2
#define IP_STATE_READ_SOCKET_TYPE   3
#define IP_STATE_READ_IP_ADDR       4
#define IP_STATE_READ_SOCKFD        5
#define IP_STATE_READ_PORT          6
#define IP_STATE_READ_DATA          7
#define IP_STATE_READ_LENGTH        8
#define IP_STATE_SEND_TO_Z80        9
#define IP_STATE_READ_RECV_OPTION   10
#define IP_STATE_READ_VERSION       11
#define IP_STATE_READ_HOSTNAME      12
#define IP_STATE_READ_HOST_FORMAT   13

#define IP_COMMAND_SOCKET     1
#define IP_COMMAND_CONNECT    2
#define IP_COMMAND_SEND       3
#define IP_COMMAND_SENDTO     4
#define IP_COMMAND_RECV       5
#define IP_COMMAND_RECVFROM   6
#define IP_COMMAND_CLOSE      7

#define IP_ERROR_NONE                         0
#define IP_ERROR_UNKNOWN_COMMAND              100
#define IP_ERROR_UNSUPPORTED_PROTOCOL         101
#define IP_ERROR_UNSUPPORTED_COMMAND_OPTION   102
#define IP_ERROR_BAD_SOCKET_FD                103
#define IP_ERROR_UNKNOWN_HOST                 104

int tcp_z80_out(uint8_t value);
void tcp_get_send_buffer(uint8_t** buf, int* size);

#endif

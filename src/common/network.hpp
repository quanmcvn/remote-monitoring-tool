#ifndef COMMON_NETWORK_HPP
#define COMMON_NETWORK_HPP

#ifdef _WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <cstdio>
#include <ws2tcpip.h>

typedef SOCKET socket_t;

#else

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef int socket_t;

#endif

int network_init();

void network_cleanup();

void network_close_socket(socket_t socket);

#endif
#ifndef COMMON_NETWORK_HPP
#define COMMON_NETWORK_HPP

#ifdef _WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <cstdio>
#include <WinSock2.h>
#include <windows.h>
#include <winsock.h>
#include <ws2tcpip.h>

typedef SSIZE_T ssize_t;

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

#include <cstdint>
#include <string>

std::uint32_t ip_string_to_uint32(const std::string& ip);

#include "common/network.hpp"

#ifdef _WIN32

typedef SOCKET socket_t;

int network_init() {
	WSADATA wsa;
	return WSAStartup(MAKEWORD(2, 2), &wsa);
}

void network_cleanup() { WSACleanup(); }

void network_close_socket(socket_t socket) { closesocket(socket); }

#else

#include <signal.h>

typedef int socket_t;

int network_init() {
	// we will ignore SIGPIPE here, a good place
	signal(SIGPIPE, SIG_IGN);
	return 0;
}

void network_cleanup() {}

void network_close_socket(socket_t socket) {
	shutdown(socket, SHUT_RDWR);
	close(socket);
}

#endif
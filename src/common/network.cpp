#include "common/network.hpp"
#include <iostream>

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

std::uint32_t ip_string_to_uint32(const std::string& ip) {
	std::uint32_t ip_addr = 0;
	int result = inet_pton(AF_INET, ip.c_str(), &ip_addr);
	if (result == 1) {
		return ip_addr;
	} else {
		std::cerr << "ip_string_to_uint32: invalid ip address: " << ip << "\n";
		// let's just throw
		throw std::runtime_error("ip_string_to_uint32: invalid ip address: " + ip);
	}
}

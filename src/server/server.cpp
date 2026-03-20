#include "server/server.hpp"

#include <string>
#include <iostream>

#include "common/error.hpp"
#include "common/network.hpp"
#include "common/util.hpp"

#define PORT 12345

int server_main(int argc, char* argv[]) {
	int chosen_port = PORT;
	for (int i = 1; i < argc; ++i) {
		std::string arg = std::string(argv[i]);
		if (arg.starts_with("--server-port=")) {
			std::string server_port = arg.substr(std::string("--server-port=").length());
			int port = str_to_int(server_port);
			if (port < 0) {
				std::cerr << "--server-port: error in converting " << server_port << " to int\n";
			} else if (port < 1024 || port > 65536) {
				std::cerr << "--server-port: invalid port " << port << "\n";
			} else {
				chosen_port = port;
			}
		}
	}

	std::cout << "choosing port: " << chosen_port << "\n";

	if (network_init() != 0) {
		print_error("network init failed");
		return 1;
	}
	socket_t server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		print_error("create socket failed");
		return 1;
	}

	sockaddr_in server_addr{};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(chosen_port);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	int opt = 1;
#ifdef _WIN32
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char *)(&opt), sizeof(opt));
#else
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
	if (bind(server_socket, (sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		print_error("bind failed");
		return 1;
	}

	listen(server_socket, 1);
	std::cout << "server listening on port " << chosen_port << "...\n";

	sockaddr_in client_addr{};
	socklen_t client_size = sizeof(client_addr);
	socket_t client_socket = accept(server_socket, (sockaddr *)&client_addr, &client_size);
	// if (!server_running || client_socket < 0) {
	// 	break;
	// }
	char client_address_ip[18];
	inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_address_ip, 18);
	std::cout << "client connected!\n";
	std::cout << client_address_ip << " " << client_addr.sin_port << "\n";

	while (true) {
		
	}

	return 0;
}
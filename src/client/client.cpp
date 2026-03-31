#include "client/client.hpp"

#include <iostream>
#include <string>
#include <thread>

#include "client/process_table.hpp"
#include "common/util.hpp"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12345

int client_main(int argc, char *argv[]) {
	// std::string chosen_ip = SERVER_IP;
	// int chosen_port = SERVER_PORT;
	// for (int i = 1; i < argc; ++i) {
	// 	std::string arg = std::string(argv[i]);
	// 	if (arg.starts_with("--server-ip=")) {
	// 		std::string server_ip = arg.substr(std::string("--server-ip=").length());
	// 		chosen_ip = server_ip;
	// 	} else if (arg.starts_with("--server-port=")) {
	// 		std::string server_port = arg.substr(std::string("--server-port=").length());
	// 		int port = str_to_int(server_port);
	// 		if (port < 0) {
	// 			std::cerr << "--server-port: error in converting " << server_port << " to int\n";
	// 		} else if (port == 0 || port > 65536) {
	// 			std::cerr << "--server-port: invalid port " << port << "\n";
	// 		} else {
	// 			chosen_port = port;
	// 		}
	// 	}
	// }

	// std::cout << "chosing " << chosen_ip << ":" << chosen_port << "\n";

	std::cout << "init...\n";
	ProcessTable table;
	using clock = std::chrono::steady_clock;
	auto next_time = clock::now();
	while (true) {
		next_time += std::chrono::seconds(1);

		table.update_table();
		std::string dis = table.display_table();
		std::cout << dis;

		std::this_thread::sleep_until(next_time);
	}

	return 0;
}
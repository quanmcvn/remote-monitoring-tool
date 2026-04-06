#include "server/server.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "common/config_entry.hpp"
#include "common/error.hpp"
#include "common/event_bus.hpp"
#include "common/input_event.hpp"
#include "common/log_entry.hpp"
#include "common/network.hpp"
#include "common/serializable.hpp"
#include "common/util.hpp"
#include "common/input_handler.hpp"

#define PORT 12345

void register_server_input(EventBus& event_bus) {
	event_bus.subscribe([](const Event& event) {
		std::cerr << "server_input: got event\n";
		if (const InputEvent* input_event_ptr = dynamic_cast<const InputEvent*>(&event)) {
			const InputEvent& input_event = *input_event_ptr;
			std::string payload = input_event.get_payload();
			std::cerr << "server_input: got input_event: '" << payload << "'\n";
			if (payload.starts_with("config ")) {
				std::string config_file_name = payload.substr(std::string("config ").length());
				std::ifstream config_file_in(std::filesystem::path(config_file_name).string());
				if (!config_file_in) {
					std::cerr << "server_input: config: can't open file or file doesn't exist: '"
					          << config_file_name << "'\n";
					return;
				}
				try {
					std::vector<ConfigEntry> config_entries = nlohmann::json::parse(config_file_in);
					std::cerr << "server_input: config: got config:\n";
					for (const auto& config_entry : config_entries) {
						std::cerr << config_entry.get_process_name() << " "
						          << config_entry.get_cpu_usage() << " "
						          << config_entry.get_mem_usage() << " "
						          << config_entry.get_disk_usage() << " "
						          << config_entry.get_network_usage() << "\n";
					}
				} catch (std::exception& e) {
					std::cerr << "server_input: config: error while parsing config file '"
					          << config_file_name << "': " << e.what() << "\n";
					return;
				}

				return;
			}
			std::cerr << "server_input: invalid command: '" << payload << "'\n";
		}
	});
}

int server_main(int argc, char* argv[]) {
	network_init();

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
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)(&opt), sizeof(opt));
#else
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
	if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		print_error("bind failed");
		return 1;
	}

	listen(server_socket, 1);
	std::cout << "server listening on port " << chosen_port << "...\n";

	EventBus event_bus;
	InputHandler input_handler(event_bus);
	register_server_input(event_bus);

	sockaddr_in client_addr{};
	socklen_t client_size = sizeof(client_addr);
	socket_t client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_size);
	// if (!server_running || client_socket < 0) {
	// 	break;
	// }
	char client_address_ip[18];
	inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_address_ip, 18);
	std::cout << "client connected!\n";
	std::cout << client_address_ip << " " << client_addr.sin_port << "\n";

	std::string client_address_ip_string(client_address_ip);

	std::string client_log_file = "client-log-" + client_address_ip_string + ".txt";
	std::uint64_t last_acked_id = 0;
	std::string last_line = get_last_line_of_file(client_log_file);
	if (!last_line.empty()) {
		LogEntry log_entry;
		std::istringstream iss(last_line);
		log_entry.deserialize_str(iss);
		last_acked_id = log_entry.get_id();
		std::cerr << "got last acked id: " << last_acked_id << "\n";
	} else {
		std::cerr << "did not get last acked id\n";
	}
	{
		// sends an ack now to try resync
		std::ostringstream oss(std::ios::binary);
		SerializableHelper::write_string(oss, std::format("ack {}", last_acked_id));
		SerializableHelper::send_message(client_socket, oss.str());
	}
	while (true) {
		std::string message = SerializableHelper::recv_message(client_socket);
		std::istringstream iss(message, std::ios::binary);
		std::vector<LogEntry> log_entries =
		    SerializableHelper::read_vector_serializeable<LogEntry>(iss);
		std::ofstream client_log_file_out(client_log_file, std::ios::app);
		if (!client_log_file_out) {
			std::cerr << "server: can't open client log file '" << client_log_file << "' \n";
			continue;
		}
		for (const auto& log_entry : log_entries) {
			std::cerr << log_entry.get_id() << " " << log_entry.get_process_name() << " "
			          << log_entry.get_log_type() << " " << log_entry.get_timestamp_ms() << " "
			          << log_entry.get_value() << "\n";
		}
		for (const auto& log_entry : log_entries) {
			if (log_entry.get_id() == last_acked_id + 1) {
				log_entry.serialize_str(client_log_file_out);
				last_acked_id += 1;
			} else {
				std::cerr << "server: got id " << log_entry.get_id() << " while last acked id is "
				          << last_acked_id << "\n";
			}
		}

		std::ostringstream oss(std::ios::binary);
		SerializableHelper::write_string(oss, std::format("ack {}", last_acked_id));
		SerializableHelper::send_message(client_socket, oss.str());
	}

	// std::string client_message = SerializableHelper::recv_message(client_socket);
	// std::cerr << "client says: " << client_message << "\n";
	// std::string message = "hello from server";
	// SerializableHelper::send_message(client_socket, message);

	network_cleanup();

	return 0;
}
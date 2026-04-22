#include "client/client.hpp"

#include <iostream>
#include <string>
#include <thread>

#include "client/client_logger.hpp"
#include "client/process_table.hpp"
#include "client/server_connector.hpp"
#include "common/config.hpp"
#include "common/event_bus.hpp"
#include "common/exponential_backoff.hpp"
#include "common/network_event.hpp"
#include "common/util.hpp"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12345

// receive message from server, blocks
void receive_server_input(ServerConnector& server_connector, EventBus& event_bus) {
	while (true) {
		std::cerr << "receiving server message...\n";
		std::string message = server_connector.recv_input();
		if (message == "") {
			std::cerr << "receive_server_input: no message\n";
			std::this_thread::sleep_for(std::chrono::seconds(1));
		} else {
			std::cerr << "receive_server_input: got message\n";
			event_bus.publish(NetworkRecvEvent(server_connector.get_server_ip(), server_connector.get_server_port(), message));
		}
	}
}

void register_send_outout_to_server(ServerConnector& server_connector, EventBus& event_bus) {
	event_bus.subscribe([&server_connector] (const Event& event) {
		if (const NetworkSendEvent* network_event_ptr = dynamic_cast<const NetworkSendEvent*>(&event)) {
			const NetworkSendEvent& network_event = *network_event_ptr;
			std::cerr << "send_output_to_server: got event\n";
			// spawn a thread here because send() blocks
			std::string payload = network_event.get_payload();
			std::thread t([&server_connector, payload] () {
				server_connector.send_output(payload);
			});
			t.detach();
			// done, now return
		}
	});
}

void register_recv_config(ClientLogger& client_logger, ServerConnector& server_connector, EventBus& event_bus) {
	EventBus& event_bus_ref = event_bus;
	event_bus.subscribe([&server_connector, &client_logger, &event_bus_ref] (const Event& event) {
		if (const NetworkRecvEvent* network_event_ptr = dynamic_cast<const NetworkRecvEvent*>(&event)) {
			const NetworkRecvEvent& network_event = *network_event_ptr;
			std::string payload = network_event.get_payload();
			std::istringstream iss(payload, std::ios::binary);
			std::string message = SerializableHelper::read_string(iss);
			std::cerr << "recv_config: network event: " << message << "\n";
			if (message.starts_with("config ")) {
				std::string content = message.substr(std::string("config ").length());
				try {
					std::vector<ConfigEntry> config_entries = nlohmann::json::parse(content);
					std::cerr << "recv_config: got config from server:\n";
					for (const auto& config_entry : config_entries) {
						std::cerr << config_entry.get_process_name() << " " << config_entry.get_cpu_usage() << " "
								<< config_entry.get_mem_usage() << " " << config_entry.get_disk_usage() << " "
								<< config_entry.get_network_usage() << "\n";
					}
					client_logger.set_config(Config(config_entries));

					std::ostringstream oss;
					SerializableHelper::write_string(oss, "config ok");
					event_bus_ref.publish(NetworkSendEvent(server_connector.get_server_ip(), server_connector.get_server_port(), oss.str()));
				} catch (std::exception& e) {
					std::cerr << "recv_config: failed to parse config from server: '" << content << "'\n";
				}
			}
		}
	});
}

int client_main(int argc, char* argv[]) {
	network_init();

	std::string chosen_ip = SERVER_IP;
	int chosen_port = SERVER_PORT;
	for (int i = 1; i < argc; ++i) {
		std::string arg = std::string(argv[i]);
		if (arg.starts_with("--server-ip=")) {
			std::string server_ip = arg.substr(std::string("--server-ip=").length());
			chosen_ip = server_ip;
		} else if (arg.starts_with("--server-port=")) {
			std::string server_port = arg.substr(std::string("--server-port=").length());
			int port = str_to_int(server_port);
			if (port < 0) {
				std::cerr << "--server-port: error in converting " << server_port << " to int\n";
			} else if (port == 0 || port > 65536) {
				std::cerr << "--server-port: invalid port " << port << "\n";
			} else {
				chosen_port = port;
			}
		}
	}

	std::cout << "chosing " << chosen_ip << ":" << chosen_port << "\n";
	ServerConnector server_connector(chosen_ip, chosen_port);
	EventBus event_bus;

	// while (true) {
	// 	std::string message = "hello from client";
	// 	if (server_connector.send_output(message)) {
	// 		std::string server_message = server_connector.recv_input();
	// 		std::cout << "server says: " << server_message << "\n";
	// 	}
	// 	std::this_thread::sleep_for(std::chrono::seconds(1));
	// }

	std::cout << "init...\n";
	ProcessTable table;
	table.setup_network();
	// call update once to init disk usage
	table.update_table();
	using clock = std::chrono::steady_clock;
	auto next_time = clock::now();
	Config config;
	int ret = config.read_config();
	if (ret != 0) {
		std::cerr << "client: error while reading config? fall back to default\n";
		config = Config::default_config();
	}
	std::cerr << "client: got config:\n";
	for (const auto& config_entry : config.get_config_entries()) {
		std::cerr << config_entry.get_process_name() << " " << config_entry.get_cpu_usage() << " "
		          << config_entry.get_mem_usage() << " " << config_entry.get_disk_usage() << " "
		          << config_entry.get_network_usage() << "\n";
	}
	ClientLogger logger(config, LogQueue("rmt-log.txt", "rmt-ack.txt"), std::ref(event_bus));
	std::thread receiver_thread(receive_server_input, std::ref(server_connector),
	                            std::ref(event_bus));
	receiver_thread.detach();
	register_send_outout_to_server(server_connector, event_bus);
	register_recv_config(std::ref(logger), std::ref(server_connector), std::ref(event_bus));
	while (true) {
		next_time += std::chrono::seconds(1);

		table.update_table();
		logger.generate_log(table);
		std::vector<LogEntry> log_entries = logger.get_batch_log(100);
		if (!log_entries.empty()) {
			// std::cerr << "client: log size " << log_entries.size() << "\n";
			// for (const auto& entry : log_entries) {
			// 	std::cerr << entry.get_id() << " " << entry.get_log_type() << "\n";
			// }
			std::ostringstream oss(std::ios::binary);
			SerializableHelper::write_string(oss, "log");
			SerializableHelper::write_vector_serializeable(oss, log_entries);

			event_bus.publish(NetworkSendEvent(server_connector.get_server_ip(), server_connector.get_server_port(), oss.str()));
		} else {
			std::cerr << "client: log empty! not sending rn...\n";
		}
		// std::cout << table.display_table();

		std::this_thread::sleep_until(next_time);
	}

	network_cleanup();

	return 0;
}
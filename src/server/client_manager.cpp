#include "client_manager.hpp"

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

#include "common/config_entry.hpp"
#include "common/input_event.hpp"
#include "common/network_event.hpp"
#include "common/util.hpp"

ClientManager::ClientManager(EventBus& n_event_bus, socket_t n_server_socket)
    : event_bus(n_event_bus), server_socket(n_server_socket), is_running(false) {}

void ClientManager::add_new_client(ClientHandler client_handler) {
	std::lock_guard<std::mutex> lock(this->client_handlers_mutex);
	int id = client_handler.get_client_id();
	client_handlers.emplace(id, std::move(client_handler));
}

void ClientManager::accept_new_client() {
	int client_id = 1;
	while (true) {
		sockaddr_in client_addr{};
		socklen_t client_size = sizeof(client_addr);
		socket_t client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_size);
		char client_address_ip[18];
		inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_address_ip, 18);
		std::cout << "client connected!\n";
		std::cout << client_address_ip << " " << client_addr.sin_port << "\n";
		std::cout << "this will be client #" << client_id << "\n";
		ClientHandler client_hander(client_id++, client_socket, std::string(client_address_ip),
		                            client_addr.sin_port, event_bus);
		add_new_client(std::move(client_hander));
	}
}

void ClientManager::register_user_input_handler() {
	EventBus& event_bus_ref = event_bus;
	event_bus.subscribe([this, &event_bus_ref](const Event& event) {
		if (const InputEvent* input_event_ptr = dynamic_cast<const InputEvent*>(&event)) {
			const InputEvent& input_event = *input_event_ptr;
			std::string payload = input_event.get_payload();
			std::cerr << "server_input: got input_event: '" << payload << "'\n";
			if (payload == "exit") {
				std::cerr << "server_input: exiting...\n";
				is_running = false;
				return;
			}
			auto [client_info, message] = this->find_client_from_input(payload);
			if (client_info.client_id <= 0)
				return;
			if (message.starts_with("config ")) {
				std::string config_file_name = message.substr(std::string("config ").length());
				std::ifstream config_file_in(std::filesystem::path(config_file_name).string());
				if (!config_file_in) {
					std::cerr << "server_input: config: can't open file or file doesn't exist: '"
					          << config_file_name << "'\n";
					return;
				}
				try {
					{
						// parse to make sure it's valid
						std::vector<ConfigEntry> config_entries =
						    nlohmann::json::parse(config_file_in);
						std::cerr << "server_input: config: got config:\n";
						for (const auto& config_entry : config_entries) {
							std::cerr << config_entry.get_process_name() << " "
							          << config_entry.get_cpu_usage() << " "
							          << config_entry.get_mem_usage() << " "
							          << config_entry.get_disk_usage() << " "
							          << config_entry.get_network_usage() << "\n";
						}
					}
					config_file_in.clear();
					config_file_in.seekg(0, std::ios::beg);
					std::ostringstream buffer;
					buffer << config_file_in.rdbuf();
					std::ostringstream oss(std::ios::binary);
					// dump the file into str to send it over network
					SerializableHelper::write_string(oss, std::format("config {}", buffer.str()));
					event_bus_ref.publish(NetworkSendEvent(client_info.client_ip,
					                                       client_info.client_port, oss.str()));
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

void ClientManager::register_send_output_to_client() {
	event_bus.subscribe([this](const Event& event) {
		if (const NetworkSendEvent* network_event_ptr =
		        dynamic_cast<const NetworkSendEvent*>(&event)) {
			const NetworkSendEvent& network_event = *network_event_ptr;
			std::cerr << "send_output_to_client: got network send event\n";
			ClientInfo client_info = this->get_client_info_from_ip_and_port(
			    network_event.get_ip(), network_event.get_port());
			if (client_info.client_id == 0)
				return;
			std::cerr << "send_output_to_client: sending to client #" << client_info.client_id
			          << "\n";
			socket_t client_socket = client_info.client_socket;
			std::string payload = network_event.get_payload();
			// spawn a thread here because send() blocks
			std::thread t([client_socket, payload]() {
				SerializableHelper::send_message(client_socket, payload);
			});
			t.detach();
			// done, now return
		}
	});
}

void ClientManager::send_output_to_client(int client_id, std::string command) {
	std::lock_guard<std::mutex> lock(client_handlers_mutex);
	auto it = this->client_handlers.find(client_id);
	if (it == this->client_handlers.end()) {
		return;
	}
	SerializableHelper::send_message(it->second.get_client_info().client_socket, command);
}

ClientInfo ClientManager::get_client_info_from_ip_and_port(const std::string& client_ip,
                                                           std::uint16_t client_port) {
	std::lock_guard<std::mutex> lock(client_handlers_mutex);
	for (const auto& [id, client] : client_handlers) {
		if (client.get_client_info().client_ip != client_ip)
			continue;
		if (client.get_client_info().client_port != client_port)
			continue;
		return client.get_client_info();
	}
	return ClientInfo(0, 0, "", 0);
}

std::pair<ClientInfo, std::string> ClientManager::find_client_from_input(const std::string& input) {
	size_t pos = input.find(':');
	if (pos == std::string::npos) {
		std::cerr << "server_input: format: client_id:message: " << input << "\n";
		return std::make_pair(ClientInfo(0, 0, "", 0), "");
	}

	int target_id = str_to_int(input.substr(0, pos));
	std::string message = input.substr(pos + 1);

	std::lock_guard<std::mutex> lock(client_handlers_mutex);

	auto it = client_handlers.find(target_id);
	if (it == client_handlers.end()) {
		std::cerr << "can't find client with id " << target_id << " (from " << input.substr(0, pos)
		          << ")\n";
		return std::make_pair(ClientInfo(0, 0, "", 0), "");
	}
	return std::make_pair(it->second.get_client_info(), message);
}

void ClientManager::run() {
	std::thread t(&ClientManager::accept_new_client, this);
	t.detach();
	register_user_input_handler();
	register_send_output_to_client();
	is_running = true;
	while (is_running) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		// wait here
	}
}

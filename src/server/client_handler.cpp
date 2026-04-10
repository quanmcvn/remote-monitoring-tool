#include "server/client_handler.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <format>
#include <thread>

#include "common/log_entry.hpp"
#include "common/network_event.hpp"
#include "common/serializable.hpp"
#include "common/util.hpp"

ClientHandler::ClientHandler(int n_client_id, socket_t n_client_socket,
                             const std::string& n_client_ip, std::uint16_t n_client_port,
                             EventBus& n_event_bus)
    : client_info(n_client_id, n_client_socket, n_client_ip, n_client_port), event_bus(n_event_bus) {
	std::thread t(&ClientHandler::recv_input_from_client, this, std::ref(event_bus), ClientInfo(n_client_id, n_client_socket, n_client_ip, n_client_port));
	t.detach();

	ClientInfo this_client_info(n_client_id, n_client_socket, n_client_ip, n_client_port);

	event_bus.subscribe([this, this_client_info](const Event& event) {
		if (const NetworkRecvEvent* network_event_ptr =
		        dynamic_cast<const NetworkRecvEvent*>(&event)) {
			const NetworkRecvEvent& network_event = *network_event_ptr;
			if (network_event.get_ip() != this_client_info.client_ip)
				return;
			if (network_event.get_port() != this_client_info.client_port)
				return;
			std::string payload = network_event.get_payload();
			std::istringstream iss(payload, std::ios::binary);
			std::string response = SerializableHelper::read_string(iss);
			std::cerr << "client_handler: got response '" << response << "' from client #"
			          << this_client_info.client_id << ": " << this_client_info.client_ip << ":" << this_client_info.client_port
			          << "\n";
			if (response == "log") {
				this->handle_recv_log(event_bus, this_client_info, iss);
				return;
			}
			if (response == "config ok") {
				std::cerr << "client_handler: client ok config\n";
				return;
			}
			std::cerr << "client_handler: got invalid response '" << response << "' from client #"
			          << this_client_info.client_id << ": " << this_client_info.client_ip << ":" << this_client_info.client_port
			          << "\n";
		}
	});
}

void ClientHandler::recv_input_from_client(EventBus& event_bus, ClientInfo this_client_info) {
	try {
		while (true) {
			std::string message = SerializableHelper::recv_message(this_client_info.client_socket);
			if (message == "")
				continue;
			event_bus.publish(NetworkRecvEvent(this_client_info.client_ip, this_client_info.client_port, message));
		}
	} catch (std::exception& e) {
		std::cerr << "recv_input_from_client: failed to recv from client #" << this_client_info.client_id << ": " << e.what() << " (client probably disconnected)\n";
	}
}

void ClientHandler::handle_recv_log(EventBus& event_bus, ClientInfo this_client_info, std::istringstream& iss) {
	std::string client_log_file = "client-log-" + this_client_info.client_ip + ".txt";
	std::uint64_t last_acked_id = 0;
	std::string last_line = get_last_line_of_file(client_log_file);
	if (!last_line.empty()) {
		LogEntry log_entry;
		std::istringstream iss_last_line(last_line);
		log_entry.deserialize_str(iss_last_line);
		last_acked_id = log_entry.get_id();
		std::cerr << "got last acked id: " << last_acked_id << "\n";
	} else {
		std::cerr << "did not get last acked id\n";
	}
	std::vector<LogEntry> log_entries =
	    SerializableHelper::read_vector_serializeable<LogEntry>(iss);
	std::ofstream client_log_file_out(client_log_file, std::ios::app);
	if (!client_log_file_out) {
		std::cerr << "client_handler: recv log: can't open client log file '" << client_log_file << "' \n";
		return;
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
			std::cerr << "client_handler: recv log: got id " << log_entry.get_id() << " while last acked id is "
			          << last_acked_id << "\n";
		}
	}
	std::ostringstream oss(std::ios::binary);
	SerializableHelper::write_string(oss, std::format("ack {}", last_acked_id));
	event_bus.publish(NetworkSendEvent(this_client_info.client_ip, this_client_info.client_port, oss.str()));
}

int ClientHandler::get_client_id() const {
	return this->client_info.client_id;
}

ClientInfo ClientHandler::get_client_info() const {
	return this->client_info;
}

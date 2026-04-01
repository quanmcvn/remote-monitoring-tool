#include "client/server_connector.hpp"

#include <chrono>
#include <iostream>
#include <thread>

#include "common/serializable.hpp"

ServerConnector::ServerConnector(const std::string &n_server_ip, int n_server_port)
    : server_ip(n_server_ip), server_port(n_server_port) {
	std::thread reconnect_thread(&ServerConnector::reconnect_indefinitely, this);
	reconnect_thread.detach();
}

bool ServerConnector::try_connect() {
	if (is_connected)
		return true;
	client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (client_socket < 0) {
		std::cerr << "server_connector: socket creation failed\n";
		return false;
	}
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(server_port);
	inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);
	if (connect(client_socket, (sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		std::cerr << "server_connector: connection failed\n";
		network_close_socket(client_socket);
		return false;
	}
	std::cerr << "server_connector: connected\n";
	is_connected = true;
	return true;
}

void ServerConnector::reconnect_indefinitely() {
	ExponentialBackoff backoff(1, 10, 2);
	while (true) {
		while (!this->is_connected) {
			if (try_connect()) {
				backoff.success();
			} else {
				backoff.fail();
			}
		}
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

ServerConnector::~ServerConnector() {
	if (this->is_connected) {
		network_close_socket(this->client_socket);
	}
}

std::optional<socket_t> ServerConnector::get_client_socket() const {
	if (this->is_connected) {
		return std::optional<socket_t>(client_socket);
	} else {
		return std::nullopt;
	}
}

std::string ServerConnector::recv_input() {
	if (!this->is_connected) {
		return "";
	}
	try {
		return SerializableHelper::recv_message(this->client_socket);
	} catch (std::exception &e) {
		this->is_connected = false;
		std::cerr << "server_connector: recv failed: " << e.what() << "\n";
		return "";
	}
}

bool ServerConnector::send_output(const std::string &message) {
	if (!this->is_connected) {
		return false;
	}
	try {
		SerializableHelper::send_message(this->client_socket, message);
		return true;
	} catch (std::exception &e) {
		this->is_connected = false;
		std::cerr << "server_connector: send failed: " << e.what() << "\n";
		return false;
	}
}

#ifndef CLIENT_SERVER_CONNECTOR
#define CLIENT_SERVER_CONNECTOR

#include <atomic>
#include <memory>
#include <optional>
#include <string>

#include "common/network.hpp"

// holds connection to server, and only that
// tries to reconnect if it's not connected
class ServerConnector {
private:
	std::string server_ip;
	int server_port;
	socket_t client_socket;
	std::atomic<bool> is_connected;
	// tries to connect once
	bool try_connect();
	// call try_connect() indefinitely (blocks)
	void reconnect_indefinitely();

public:
	ServerConnector(const std::string& server_ip, int server_port);
	~ServerConnector();
	std::optional<socket_t> get_client_socket() const;
	std::string recv_input();
	bool send_output(const std::string& message);
};

#endif
#ifndef SERVER_CLIENT_HANDLER_HPP
#define SERVER_CLIENT_HANDLER_HPP

#include <cstdint>
#include <sstream>
#include <string>

#include "common/event_bus.hpp"
#include "common/network.hpp"

struct ClientInfo {
	int client_id;
	socket_t client_socket;
	std::string client_ip;
	std::uint16_t client_port;
};

// handle client by subcribing to event bus when there's network recv event
class ClientHandler {
private:
	ClientInfo client_info;
	EventBus& event_bus;

public:
	ClientHandler(int client_id, socket_t client_socket, const std::string& client_ip,
	              std::uint16_t client_port, EventBus& event_bus);
	// delete copy
	ClientHandler(const ClientHandler&) = delete;
	ClientHandler& operator=(const ClientHandler&) = delete;
	// move
	ClientHandler(ClientHandler&&) = default;
	ClientHandler& operator=(ClientHandler&&) = default;

	int get_client_id() const;
	ClientInfo get_client_info() const;

private:
	// blocks
	void recv_input_from_client(EventBus& event_bus);
	void handle_recv_log(EventBus& event_bus, std::istringstream& iss);
};

#endif
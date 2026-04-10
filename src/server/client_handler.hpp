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
	ClientInfo(int n_client_id, socket_t n_client_socket, const std::string& n_client_ip,
	           std::uint16_t n_client_port)
	    : client_id(n_client_id), client_socket(n_client_socket), client_ip(n_client_ip),
	      client_port(n_client_port) {}

	
};

// handle client by subcribing to event bus when there's network recv event
class ClientHandler {
private:
	ClientInfo client_info;
	EventBus& event_bus;

public:
	ClientHandler(int client_id, socket_t client_socket, const std::string& client_ip,
	              std::uint16_t client_port, EventBus& event_bus);

	int get_client_id() const;
	ClientInfo get_client_info() const;

private:
	// blocks
	void recv_input_from_client(EventBus& event_bus, ClientInfo client_info);
	void handle_recv_log(EventBus& event_bus, ClientInfo this_client_info, std::istringstream& iss);
};

#endif
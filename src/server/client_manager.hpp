#ifndef SERVER_CLIENT_MANAGER_HPP
#define SERVER_CLIENT_MANAGER_HPP

#include <unordered_map>
#include <mutex>

#include "server/client_handler.hpp"

class ClientManager {
private:
	std::unordered_map<int, ClientHandler> client_handlers;
	std::mutex client_handlers_mutex;
	socket_t server_socket;
	EventBus& event_bus;
	bool is_running;

public:
	ClientManager(EventBus& event_bus, socket_t server_socket);
	void add_new_client(ClientHandler client_handler);
	void run();

private:
	// blocks
	void accept_new_client();
	void register_user_input_handler();
	void register_send_output_to_client();
public:
	void send_output_to_client(int client_id, std::string command);
	ClientInfo get_client_info_from_ip_and_port(const std::string& client_ip, std::uint16_t client_port);
	std::pair<ClientInfo, std::string> find_client_from_input(const std::string& input);
};

#endif
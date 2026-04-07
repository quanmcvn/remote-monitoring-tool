#ifndef COMMON_NETWORK_EVENT_HPP
#define COMMON_NETWORK_EVENT_HPP

#include "common/event.hpp"

#include <string>
#include <cstdint>

class NetworkSendEvent : public Event {
private:
	std::string ip;
	std::uint16_t port;
	std::string payload;

public:
	NetworkSendEvent(const std::string& ip, std::uint16_t port, const std::string& payload);
	std::string get_ip() const;
	std::uint16_t get_port() const;
	std::string get_payload() const;
};

class NetworkRecvEvent : public Event {
private:
	std::string ip;
	std::uint16_t port;
	std::string payload;

public:
	NetworkRecvEvent(const std::string& ip, std::uint16_t port, const std::string& payload);
	std::string get_ip() const;
	std::uint16_t get_port() const;
	std::string get_payload() const;
};

#endif
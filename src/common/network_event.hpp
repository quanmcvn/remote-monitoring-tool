#ifndef COMMON_NETWORK_EVENT_HPP
#define COMMON_NETWORK_EVENT_HPP

#include "common/event.hpp"

#include <string>

class NetworkSendEvent : public Event {
private:
	std::string payload;

public:
	NetworkSendEvent(const std::string& payload);
	std::string get_payload() const;
};

class NetworkRecvEvent : public Event {
private:
	std::string payload;

public:
	NetworkRecvEvent(const std::string& payload);
	std::string get_payload() const;
};

#endif
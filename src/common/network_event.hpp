#ifndef COMMON_NETWORK_EVENT_HPP
#define COMMON_NETWORK_EVENT_HPP

#include "common/event.hpp"

#include <string>

class NetworkEvent : public Event {
private:
	std::string payload;

public:
	NetworkEvent(const std::string& payload);
	std::string get_payload() const;
};

#endif
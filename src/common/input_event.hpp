#ifndef COMMON_INPUT_EVENT_HPP
#define COMMON_INPUT_EVENT_HPP

#include "common/event.hpp"
#include <string>

class InputEvent : public Event {
private:
	std::string payload;

public:
	InputEvent(const std::string& payload);
	std::string get_payload() const;
};

#endif
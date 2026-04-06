#ifndef COMMON_INPUT_HANDLER_HPP
#define COMMON_INPUT_HANDLER_HPP

#include "common/event_bus.hpp"

// handle user input via event bus (just spawn a thread that will read and publish user input)
class InputHandler : public Event {
private:
	EventBus& event_bus;
	void do_input_indefinitely();
public:
	InputHandler(EventBus& event_bus);
};

#endif
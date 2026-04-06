#include "common/input_handler.hpp"

#include <iostream>
#include <string>
#include <thread>

#include "common/input_event.hpp"

InputHandler::InputHandler(EventBus& n_event_bus) : event_bus(n_event_bus) {
	std::thread t(&InputHandler::do_input_indefinitely, this);
	t.detach();
}

void InputHandler::do_input_indefinitely() {
	std::string line;
	while (std::getline(std::cin, line)) {
		if (line.empty())
			continue;
		event_bus.publish(InputEvent(line));
	}
}

#include "common/input_event.hpp"

InputEvent::InputEvent(const std::string& n_payload) : payload(n_payload) {}
std::string InputEvent::get_payload() const { return this->payload; }
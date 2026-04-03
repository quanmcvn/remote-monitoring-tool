#include "common/network_event.hpp"

NetworkEvent::NetworkEvent(const std::string& n_payload) : payload(n_payload) {}
std::string NetworkEvent::get_payload() const { return this->payload; }
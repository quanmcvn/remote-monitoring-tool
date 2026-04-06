#include "common/network_event.hpp"

NetworkSendEvent::NetworkSendEvent(const std::string& n_payload) : payload(n_payload) {}
std::string NetworkSendEvent::get_payload() const { return this->payload; }

NetworkRecvEvent::NetworkRecvEvent(const std::string& n_payload) : payload(n_payload) {}
std::string NetworkRecvEvent::get_payload() const { return this->payload; }
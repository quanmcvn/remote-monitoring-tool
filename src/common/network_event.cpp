#include "common/network_event.hpp"

NetworkSendEvent::NetworkSendEvent(const std::string& n_ip, std::uint16_t n_port,
                                   const std::string& n_payload)
    : ip(n_ip), port(n_port), payload(n_payload) {}
std::string NetworkSendEvent::get_ip() const { return this->ip; }
std::uint16_t NetworkSendEvent::get_port() const { return this->port; }
std::string NetworkSendEvent::get_payload() const { return this->payload; }

NetworkRecvEvent::NetworkRecvEvent(const std::string& n_ip, std::uint16_t n_port,
                                   const std::string& n_payload)
    : ip(n_ip), port(n_port), payload(n_payload) {}
std::string NetworkRecvEvent::get_ip() const { return this->ip; }
std::uint16_t NetworkRecvEvent::get_port() const { return this->port; }
std::string NetworkRecvEvent::get_payload() const { return this->payload; }
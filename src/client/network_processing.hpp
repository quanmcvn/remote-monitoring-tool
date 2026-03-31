#ifndef CLIENT_NETWORK_PROCESSING_HPP
#define CLIENT_NETWORK_PROCESSING_HPP

#include <pcap/pcap.h>
#include <unordered_map>
#include <cstdint>
#include "client/process_stat.hpp"

class PcapHandler {
	pcap_t *handle;

public:
	PcapHandler();
	~PcapHandler();
	std::unordered_map<std::uint32_t, NetworkStat>
	process_packets(const std::uint32_t local_ip,
	                const std::unordered_map<std::uint16_t, std::uint32_t> &port_to_pid_map);
};

#endif
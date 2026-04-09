#ifndef CLIENT_NETWORK_PROCESSING_HPP
#define CLIENT_NETWORK_PROCESSING_HPP

#include "client/process_stat.hpp"
#include <cstdint>
#include <pcap/pcap.h>
#include <unordered_map>

class PcapHandler {
	pcap_t* handle;

public:
	PcapHandler();
	~PcapHandler();
	void setup_network();
	std::unordered_map<std::uint32_t, NetworkStat>
	process_packets(const std::uint32_t local_ip,
	                const std::unordered_map<std::uint16_t, std::uint32_t>& port_to_pid_map);
};

#endif
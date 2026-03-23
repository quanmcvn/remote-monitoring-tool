#ifndef CLIENT_NETWORK_PROCESSING_HPP
#define CLIENT_NETWORK_PROCESSING_HPP

#ifdef _WIN32
// NYI
#else

#include <pcap/pcap.h>
#include <unordered_map>
#include <cstdint>
#include "client/process_stat.hpp"

class PcapHandler {
	pcap_t *handle;
	char errbuf[PCAP_ERRBUF_SIZE];
	struct pcap_pkthdr header;
	const u_char *packet;

public:
	PcapHandler();
	~PcapHandler();
	std::unordered_map<std::uint32_t, NetworkStat>
	process_packets(const std::uint32_t local_ip,
	                const std::unordered_map<std::uint16_t, std::uint32_t> &port_to_pid_map);
};

#endif

#endif
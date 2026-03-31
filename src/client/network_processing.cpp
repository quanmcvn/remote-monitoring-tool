#include "client/network_processing.hpp"

#ifdef _WIN32

#include <pcap.h>
#include <unordered_map>

#include "client/process_stat.hpp"
#include <iostream>

PcapHandler::PcapHandler() {
	pcap_if_t *alldevs;
	char errbuf[PCAP_ERRBUF_SIZE];

	if (pcap_findalldevs(&alldevs, errbuf) != 0) {
		std::cerr << "pcap_findalldevs failed: " << errbuf << std::endl;
	}

	if (!alldevs) {
		std::cerr << "No Npcap devices found." << std::endl;
	}
	int i = 0;
	std::string device_to_open = "";
	for (pcap_if_t *d = alldevs; d != nullptr; d = d->next) {
		std::string device(d->description);
		if (device.find("Gigabit") != std::string::npos) {
			device_to_open = std::string(d->name);
		}
		++i;
	}
	pcap_freealldevs(alldevs);

	handle = pcap_open_live(device_to_open.c_str(), PCAP_ERRBUF_SIZE, 1, 1000, errbuf);
	if (handle == NULL) {
		std::cerr << "Error opening device: " << errbuf << "\n";
	}
	if (pcap_setnonblock(handle, 1, errbuf) < 0) {
		std::cerr << "Error setting non-blocking mode: " << errbuf << "\n";
	}
}

PcapHandler::~PcapHandler() {
	if (handle != NULL) {
		pcap_close(handle);
	}
}

std::unordered_map<std::uint32_t, NetworkStat> PcapHandler::process_packets(
    const std::uint32_t local_ip,
    const std::unordered_map<std::uint16_t, std::uint32_t> &port_to_pid_map) {
	std::unordered_map<std::uint32_t, NetworkStat> pid_network_map;
	struct pcap_pkthdr header;
	const u_char *packet;
	while (true) {
		packet = pcap_next(handle, &header);
		if (packet == NULL) {
			break;
		}
		
		const u_char *ip_header = packet + 14;
		// manual accessing each member without name because no #include <netinet/ip.h> #include <netinet/tcp.h> (rip)
		u_char protocol = ip_header[9];
		if (protocol != 6) {
			continue;
		}

		DWORD src_ip = *(DWORD *)(ip_header + 12);
		DWORD dst_ip = *(DWORD *)(ip_header + 16);
		u_short src_port = ntohs(*(u_short *)(ip_header + 20));
		u_short dst_port = ntohs(*(u_short *)(ip_header + 22));

		// char src_ip_str[INET_ADDRSTRLEN];
		// char dst_ip_str[INET_ADDRSTRLEN];

		// inet_ntop(AF_INET, &src_ip, src_ip_str, INET_ADDRSTRLEN);
		// inet_ntop(AF_INET, &dst_ip, dst_ip_str, INET_ADDRSTRLEN);

		if (src_ip == local_ip) {
			// out going
			// std::cerr << src_port << "\n";
			// std::cerr << "out going: " << src_port << " ";
			// std::cerr << "at " << port_to_pid_map.at(src_port) << "\n";
			auto it = port_to_pid_map.find(src_port);
			if (it == port_to_pid_map.end()) {
				std::cerr << "port_to_pid_map: can't find which out going port " << src_port
				          << " maps to\n";
				continue;
			}
			auto &network_usage = pid_network_map[it->second];
			std::uint64_t network_send = network_usage.network_send.value_or(0);
			network_usage.network_send = network_send + header.len;
		} else {
			// in going
			auto it = port_to_pid_map.find(dst_port);
			if (it == port_to_pid_map.end()) {
				std::cerr << "port_to_pid_map: can't find which in going port " << dst_port
				          << " maps to\n";
				continue;
			}
			auto &network_usage = pid_network_map[it->second];
			std::uint64_t network_recv = network_usage.network_recv.value_or(0);
			network_usage.network_recv = network_recv + header.len;
		}
	}
	return pid_network_map;
}

#else

#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pcap.h>
#include <unordered_map>

#include "client/process_stat.hpp"
#include <iostream>

PcapHandler::PcapHandler() {
	char errbuf[PCAP_ERRBUF_SIZE];
	handle = pcap_open_live("ens33", PCAP_ERRBUF_SIZE, 1, 1000, errbuf);
	if (handle == NULL) {
		std::cerr << "Error opening device: " << errbuf << "\n";
	}
	if (pcap_setnonblock(handle, 1, errbuf) < 0) {
		std::cerr << "Error setting non-blocking mode: " << errbuf << "\n";
	}
}

PcapHandler::~PcapHandler() {
	if (handle != NULL) {
		pcap_close(handle);
	}
}

std::unordered_map<std::uint32_t, NetworkStat> PcapHandler::process_packets(
    const std::uint32_t local_ip,
    const std::unordered_map<std::uint16_t, std::uint32_t> &port_to_pid_map) {
	std::unordered_map<std::uint32_t, NetworkStat> pid_network_map;
	struct pcap_pkthdr header;
	const u_char *packet;
	while (true) {
		packet = pcap_next(handle, &header);
		if (packet == NULL) {
			break;
		}
		struct ip *ip_hdr = (struct ip *)(packet + 14); // skip Ethernet

		if (ip_hdr->ip_p != IPPROTO_TCP) {
			continue;
		}

		struct tcphdr *tcp_hdr = (struct tcphdr *)(packet + 14 + ip_hdr->ip_hl * 4);

		int src_port = ntohs(tcp_hdr->source);
		int dst_port = ntohs(tcp_hdr->dest);
		// char src_ip[INET_ADDRSTRLEN];
		// char dst_ip[INET_ADDRSTRLEN];

		// inet_ntop(AF_INET, &(ip_hdr->ip_src), src_ip, INET_ADDRSTRLEN);
		// inet_ntop(AF_INET, &(ip_hdr->ip_dst), dst_ip, INET_ADDRSTRLEN);

		if (ip_hdr->ip_src.s_addr == local_ip) {
			// out going
			// std::cerr << src_port << "\n";
			// std::cerr << "out going: " << src_port << " ";
			// std::cerr << "at " << port_to_pid_map.at(src_port) << "\n";
			auto it = port_to_pid_map.find(src_port);
			if (it == port_to_pid_map.end()) {
				std::cerr << "port_to_pid_map: can't find which out going port " << src_port
				          << " maps to\n";
				continue;
			}
			auto &network_usage = pid_network_map[it->second];
			std::uint64_t network_send = network_usage.network_send.value_or(0);
			network_usage.network_send = network_send + header.len;
		} else {
			// in going
			auto it = port_to_pid_map.find(dst_port);
			if (it == port_to_pid_map.end()) {
				std::cerr << "port_to_pid_map: can't find which in going port " << dst_port
				          << " maps to\n";
				continue;
			}
			auto &network_usage = pid_network_map[it->second];
			std::uint64_t network_recv = network_usage.network_recv.value_or(0);
			network_usage.network_recv = network_recv + header.len;
		}
	}
	return pid_network_map;
}

#endif
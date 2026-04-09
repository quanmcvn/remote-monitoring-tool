#ifndef CLIENT_PROCESS_STAT_HPP
#define CLIENT_PROCESS_STAT_HPP

#include <cstdint>
#include <optional>
struct CpuStat {
	std::uint64_t cpu_usage = 0;
	std::uint64_t cpu_idle = 0;
};

struct DiskStat {
	std::optional<std::uint64_t> disk_read = std::nullopt;
	std::optional<std::uint64_t> disk_write = std::nullopt;
	DiskStat() {}
	DiskStat(std::uint64_t n_disk_read, std::uint64_t n_disk_write)
	: disk_read(n_disk_read), disk_write(n_disk_write) {}
};

struct NetworkStat {
	std::optional<std::uint64_t> network_recv = std::nullopt;
	std::optional<std::uint64_t> network_send = std::nullopt;
	NetworkStat() {}
	NetworkStat(std::uint64_t n_network_recv,
	            std::uint64_t n_network_send)
	    : network_recv(n_network_recv), network_send(n_network_send) {}
};

struct ProcessStat {
	std::uint32_t cpu_usage_percent = 0;
	std::uint64_t mem_usage = 0;
	DiskStat disk_usage;
	NetworkStat network_usage;
	ProcessStat() {}
	ProcessStat(std::uint32_t n_cpu_usage_percent, std::uint64_t n_mem_usage, DiskStat n_disk_usage,
	            NetworkStat n_network_usage)
	    : cpu_usage_percent(n_cpu_usage_percent), mem_usage(n_mem_usage), disk_usage(n_disk_usage),
	      network_usage(n_network_usage) {}
};

#endif
#ifndef CLIENT_PROCESS_STAT_HPP
#define CLIENT_PROCESS_STAT_HPP

#include <cstdint>
#include <optional>

struct CpuMemStat {
	std::uint64_t total_cpu_usage = 0;
	std::uint64_t mem_usage = 0;
};

struct CpuStat {
	std::uint64_t cpu_usage = 0;
	std::uint64_t cpu_idle = 0;
};

struct DiskStat {
	std::optional<std::uint64_t> disk_read = std::nullopt;
	std::optional<std::uint64_t> disk_write = std::nullopt;
};

struct NetworkStat {
	std::optional<std::uint64_t> network_recv = std::nullopt;
	std::optional<std::uint64_t> network_send = std::nullopt;
};

struct ProcessStat {
	std::uint32_t cpu_usage_percent;
	std::uint64_t mem_usage;
	DiskStat disk_usage;
	NetworkStat network_usage;
};

#endif
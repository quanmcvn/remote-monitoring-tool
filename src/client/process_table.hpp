#ifndef CLIENT_PROCESS_TABLE_HPP
#define CLIENT_PROCESS_TABLE_HPP

#include <unordered_map>

#include "client/network_processing.hpp"
#include "client/process_listing.hpp"
#include "client/process_stat.hpp"

// for use in ProcessTable, has additional field to compute usage
class ProcessLastStat {
public:
	std::uint64_t total_cpu_usage = 0;
	DiskStat disk_stat;
};

class ProcessTable {
private:
	std::unordered_map<std::uint32_t, ProcessListing> list_table;
	std::unordered_map<std::uint32_t, ProcessStat> stat_table;
	std::unordered_map<std::uint32_t, ProcessLastStat> last_stat_table;
	PcapHandler pcap_handler;

public:
	enum ProcessSortType { CPU, MEM, DISK_READ, DISK_WRITE, NETWORK_RECV, NETWORK_SEND };
	struct ProcessFullStat {
		std::uint32_t pid;
		const ProcessListing* meta;
		const ProcessStat* stat;
		ProcessFullStat() {}
		ProcessFullStat(std::uint32_t n_pid, const ProcessListing* n_meta,
		                const ProcessStat* n_stat)
		    : pid(n_pid), meta(n_meta), stat(n_stat) {}
	};
	// update stat_table, additionally change list_table if process died/spawned
	void update_table();
	std::string display_table() const;
	std::vector<ProcessFullStat> get_sorted(ProcessSortType type, std::uint32_t limit = 0) const;
	// get a map from program name to full resource consumption (all processes with same name are
	// grouped)
	std::unordered_map<std::string, ProcessStat> get_map_program_name_resource() const;
};

#endif
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

#ifdef _WIN32

#include <pdh.h>
#include <pdhmsg.h>
#include <windows.h>
class PdhHelper {
private:
	PDH_HQUERY query = NULL;
	PDH_HCOUNTER read_counter = NULL;
	PDH_HCOUNTER write_counter = NULL;
	PDH_HCOUNTER pid_counter = NULL;

public:
	PdhHelper();
	~PdhHelper();

	std::unordered_map<std::uint32_t, DiskStat> query_disk();

	static std::vector<PDH_FMT_COUNTERVALUE_ITEM_W> get_large_array(PDH_HCOUNTER counter) {
		DWORD buffer_size = 0;
		DWORD item_count = 0;

		PDH_STATUS status =
		    PdhGetFormattedCounterArrayW(counter, PDH_FMT_LARGE, &buffer_size, &item_count, NULL);

		if (status != PDH_MORE_DATA) {
			return {};
		}

		std::vector<BYTE> buffer(buffer_size);
		auto items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());

		status =
		    PdhGetFormattedCounterArrayW(counter, PDH_FMT_LARGE, &buffer_size, &item_count, items);

		if (status != ERROR_SUCCESS)
			return {};

		return std::vector<PDH_FMT_COUNTERVALUE_ITEM_W>(items, items + item_count);
	}
};

#endif

class ProcessTable {
	// protected to use in tests
protected:
	std::unordered_map<std::uint32_t, ProcessListing> list_table;
	std::unordered_map<std::uint32_t, ProcessStat> stat_table;
private:
	std::unordered_map<std::uint32_t, ProcessLastStat> last_stat_table;
	PcapHandler pcap_handler;
#ifdef _WIN32
	PdhHelper pdh_helper;
#endif

public:
	enum ProcessSortType { CPU, MEM, DISK_READ, DISK_WRITE, DISK_TOTAL, NETWORK_RECV, NETWORK_SEND, NETWORK_TOTAL };
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
	// virtual to use in tests by deriving
	void setup_network();
	virtual void update_table();
	std::string display_table() const;
	std::vector<ProcessFullStat> get_sorted(ProcessSortType type, std::uint32_t limit = 0) const;
	// get a map from program name to full resource consumption (all processes with same name are
	// grouped)
	std::unordered_map<std::string, ProcessStat> get_map_program_name_resource() const;
};

#endif
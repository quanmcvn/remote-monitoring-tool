#include "client/process_table.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

#include "common/global.hpp"

#ifdef _WIN32

// NYI

#else

#include <assert.h>
#include <charconv>
#include <fcntl.h>
#include <format>
#include <fstream>
#include <iostream>
#include <unistd.h>

namespace {

// reusable buffer for reading all kind of stuff
static constexpr std::uint32_t BUFFER_SIZE = 4096;
static char buffer[BUFFER_SIZE];
static const long page_size = sysconf(_SC_PAGE_SIZE);

// read file to buffer, capped to BUFFER_SIZE
static ssize_t read_file(const std::string &path) {
	int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0)
		return -1;

	ssize_t n = read(fd, buffer, BUFFER_SIZE - 1);
	close(fd);

	if (n > 0)
		buffer[n] = '\0';
	return n;
}

static std::string read_program_name(std::uint32_t pid) {
	ssize_t comm_read = read_file("/proc/" + std::to_string(pid) + "/comm");
	if (comm_read < 0) {
		throw std::runtime_error("read " + std::to_string(pid) + " comm failed");
	}
	return std::string(buffer, buffer + comm_read);
}

static std::vector<std::string> parse_cmdline(std::uint32_t pid) {
	ssize_t cmdline_read = read_file("/proc/" + std::to_string(pid) + "/cmdline");
	if (cmdline_read < 0) {
		throw std::runtime_error("read " + std::to_string(pid) + " cmdline failed");
	}
	std::vector<std::string> args;
	char *now = buffer;
	for (char *p = buffer; p < buffer + cmdline_read; ++p) {
		if (*p == '\0') {
			args.emplace_back(now, p - now);
			now = p + 1;
		}
	}
	return args;
}

static ProcessListing read_process_metadata(std::uint32_t pid) {
	std::vector<std::string> cmdline = parse_cmdline(pid);
	std::string program_name;
	if (cmdline.empty()) {
		program_name = read_program_name(pid);
	} else {
		program_name = std::filesystem::path(cmdline[0]).filename().string();
	}

	return ProcessListing(pid, std::move(program_name), std::move(cmdline));
}

static CpuMemStat read_pid_stat_cpu_mem(std::uint32_t pid) {
	ssize_t stat_read = read_file("/proc/" + std::to_string(pid) + "/stat");
	if (stat_read < 0) {
		throw std::runtime_error("read " + std::to_string(pid) + " stat failed");
	}
	CpuMemStat cpu_mem_stat;
	char *p = buffer;
	// std::cerr << "pid: " << pid  << " buffer: " << buffer << "\n";
	/* what we want is
	(14) utime %lu
	(24) rss %ld
	*/
	// skip field 1
	while (*p != ' ')
		++p;
	++p;
	// skip field 2
	{
		int open_para = 0;
		do {
			if (*p == '(')
				++open_para;
			else if (*p == ')')
				--open_para;
			++p;
		} while (open_para > 0);
	}
	++p;
	int field = 3;
	while (field <= 52) {
		if (*p == '\0') {
			throw std::runtime_error("error while parsing " + std::to_string(pid) + " stat");
		}
		if (*p == ' ') {
			++p;
			continue;
		}
		char *now = p;
		while (*p != ' ') {
			++p;
		}
		// std::cerr << field << " " << std::string(now, p + 1) << "\n";
		if (field == 14) {
			std::from_chars(now, p, cpu_mem_stat.total_cpu_usage);
		} else if (field == 24) {
			std::from_chars(now, p, cpu_mem_stat.mem_usage);
			cpu_mem_stat.mem_usage *= page_size;
		}
		++field;
	}
	// std::cerr << "got " << cpu_mem_stat.total_cpu_usage << " - " << cpu_mem_stat.mem_usage <<
	// "\n";
	return cpu_mem_stat;
}

// read /proc/stat for total cpu
static CpuStat read_stat_cpu() {
	ssize_t stat_read = read_file("/proc/stat");
	if (stat_read < 0) {
		throw std::runtime_error("read /proc/stat failed");
	}
	CpuStat cpu_stat;
	char *p = buffer;
	int field = 1;
	while (field <= 11) {
		if (*p == '\0') {
			throw std::runtime_error("error while parsing /proc/stat");
		}
		if (*p == ' ') {
			++p;
			continue;
		}
		char *now = p;
		while (*p != ' ') {
			++p;
		}
		if (field >= 2 && field <= 11) {
			std::uint64_t number;
			std::from_chars(now, p, number);
			cpu_stat.cpu_usage += number;
			if (field == 4) {
				cpu_stat.cpu_idle = number;
			}
		}
		++field;
	}
	return cpu_stat;
}

static DiskStat read_pid_io(std::uint32_t pid) {
	DiskStat disk_stat;
	ssize_t io_read = read_file("/proc/" + std::to_string(pid) + "/io");
	if (io_read < 0)
		return disk_stat;
	char *p = buffer;
	int line = 1;
	std::cerr << "pid: " << pid << " buffer: " << buffer << "\n\n";
	while (line <= 6) {
		if (line == 5 || line == 6) {
			while (*p != ' ')
				++p;
			++p;
			char *now = p;
			while (*p != '\n')
				++p;
			std::uint64_t number;
			std::from_chars(now, p, number);
			if (line == 5) {
				disk_stat.disk_read = number;
			} else {
				disk_stat.disk_write = number;
			}
			++p;
			++line;
			continue;
		}
		while (*p != '\n')
			++p;
		++p;
		++line;
	}
	return disk_stat;
}

static std::uint64_t parse_socket_inode(const std::string &link) {
	if (link.substr(0, 8) != "socket:[")
		return 0;
	std::uint64_t number;
	std::from_chars(link.c_str() + 8, link.c_str() + link.size() - 9, number);
	return number;
}

static std::vector<std::uint64_t> get_socket_inode_list(std::uint32_t pid) {
	std::filesystem::path fd_path = "/proc/" + std::to_string(pid) + "/fd";
	std::vector<uint64_t> sockets;
	for (auto &entry : std::filesystem::directory_iterator(fd_path)) {
		try {
			std::string link = std::filesystem::read_symlink(entry.path()).string();
			std::uint64_t inode = parse_socket_inode(link);
			if (inode) {
				sockets.push_back(inode);
			}
		} catch (const std::filesystem::filesystem_error &e) {
			continue;
		}
	}
	return sockets;
}

// static std::unordered_map<std::uint64_t, NetworkStat> parse_net_tcp() {
// 	std::ifstream tcp("/proc/net/tcp");
// 	std::string line;
// 	std::getline(tcp, line); // skip header

// 	while (std::getline(tcp, line)) {
// 		std::istringstream iss(line);
// 		std::string local, rem, st;
// 		std::uint64_t txq, rxq;
// 		std::uint64_t inode;
// 		std::string dummy;
// 		iss >> dummy >> local >> rem >> st >> dummy >> dummy >> dummy >> dummy >> dummy >> inode;
// 	}
// }

static std::vector<std::uint32_t> list_pids() {
	const std::filesystem::path proc_path("/proc");
	std::vector<std::uint32_t> pids;
	for (const auto &entry : std::filesystem::directory_iterator(proc_path)) {
		std::string name = entry.path().filename().string();
		if (!name.empty() && isdigit(name[0])) {
			std::uint32_t pid;
			std::from_chars(name.data(), name.data() + name.size(), pid);
			pids.push_back(pid);
		}
	}
	return pids;
}

} // namespace

void ProcessTable::update_table() {
	// static to store last total cpu usage (duh)
	static CpuStat last_cpu;
	CpuStat now_cpu = read_stat_cpu();
	CpuStat cpu_delta;
	cpu_delta.cpu_usage = now_cpu.cpu_usage - last_cpu.cpu_usage;
	cpu_delta.cpu_idle = now_cpu.cpu_idle - last_cpu.cpu_idle;
	if (cpu_delta.cpu_usage == 0) {
		cpu_delta.cpu_usage = 1;
	}
	if (cpu_delta.cpu_idle == 0) {
		cpu_delta.cpu_idle = 1;
	}
	last_cpu = now_cpu;

	for (auto &[pid, p] : this->list_table) {
		p.seen = false;
	}

	std::vector<std::uint32_t> pids = list_pids();
	for (std::uint32_t pid : pids) {
		auto &process_meta = this->list_table[pid];
		try {
			if (process_meta.process_listing.get_args().empty()) {
				process_meta.process_listing = read_process_metadata(pid);
			}
			// if it can't read then it is probably kernel thread
			if (process_meta.process_listing.get_args().empty())
				continue;
			auto &process_stat = this->stat_table[pid];
			auto &process_last_stat = this->last_stat_table[pid];

			CpuMemStat cpu_mem_stat = read_pid_stat_cpu_mem(pid);
			DiskStat disk_stat = read_pid_io(pid);

			std::uint64_t current_cpu_delta =
			    cpu_mem_stat.total_cpu_usage - process_last_stat.total_cpu_usage;
			process_last_stat.total_cpu_usage = cpu_mem_stat.total_cpu_usage;
			double total_cpu_percent = static_cast<double>(current_cpu_delta) /
			                           static_cast<double>(cpu_delta.cpu_usage) * 100;
			std::uint32_t current_cpu_percent =
			    total_cpu_percent *
			    (static_cast<double>(1) - static_cast<double>(cpu_delta.cpu_idle) /
			                                  static_cast<double>(cpu_delta.cpu_usage)) *
			    CPU_UNIT;
			process_stat.cpu_usage_percent = current_cpu_percent;

			process_stat.mem_usage = cpu_mem_stat.mem_usage;

			DiskStat current_disk_delta;
			if (disk_stat.disk_read.has_value()) {
				current_disk_delta.disk_read =
				    disk_stat.disk_read.value() -
				    process_last_stat.disk_stat.disk_read.value_or<std::uint64_t>(0);
			}
			if (disk_stat.disk_write.has_value()) {
				current_disk_delta.disk_write =
				    disk_stat.disk_write.value() -
				    process_last_stat.disk_stat.disk_write.value_or<std::uint64_t>(0);
			}
			process_last_stat.disk_stat = disk_stat;
			process_stat.disk_usage = current_disk_delta;

			process_meta.seen = true;
		} catch (std::exception &e) {
			std::cerr << e.what() << "\n";
		}
	}

	for (auto it = this->list_table.begin(); it != this->list_table.end();) {
		if (it->second.seen == false) {
			this->stat_table.erase(it->first);
			this->last_stat_table.erase(it->first);
			it = this->list_table.erase(it);
		} else {
			++it;
		}
	}
}

std::string ProcessTable::display_table() const {
	constexpr uint32_t max_char_per_line = 140; // arbitrarily chosen
	constexpr uint32_t num_lines = 30;          // arbitrarily chosen
	std::string res;
	res.reserve(max_char_per_line * (num_lines + 1) + 100);
	res += "\033[H\033[J";
	auto fullstat_table = this->get_sorted(ProcessTable::ProcessSortType::DISK_WRITE, num_lines);
#define FORMAT_STRING "{:<7} {:<10} {:<13} {:<10} {:<10} {:<10} {:<10} {:<20} {:<40}"
	res += std::format(FORMAT_STRING, "pid", "cpu", "mem", "disk read", "disk write", "net recv",
	                   "net send", "programm", "cmdline");
	res += "\n";
	for (const auto &[pid, meta, stat] : fullstat_table) {
		std::string disk_read = stat->disk_usage.disk_read.has_value()
		                            ? std::to_string(stat->disk_usage.disk_read.value())
		                            : "N/A";
		std::string disk_write = stat->disk_usage.disk_write.has_value()
		                             ? std::to_string(stat->disk_usage.disk_write.value())
		                             : "N/A";
		std::string line =
		    std::format(FORMAT_STRING, pid, static_cast<double>(stat->cpu_usage_percent) / CPU_UNIT,
		                stat->mem_usage, disk_read, disk_write, stat->network_usage.network_recv,
		                stat->network_usage.network_send, meta->process_listing.get_process_name(),
		                meta->process_listing.get_args_as_string());
		if (line.size() > max_char_per_line)
			line.resize(max_char_per_line);
		res += line;
		res += "\n";
	}
#undef FORMAT_STRING
	return res;
}

static auto get_comparator(ProcessTable::ProcessSortType type) {
	return [type](const ProcessTable::ProcessFullStat &lhs,
	              const ProcessTable::ProcessFullStat &rhs) {
		switch (type) {
		case ProcessTable::ProcessSortType::CPU:
			if (lhs.stat->cpu_usage_percent != rhs.stat->cpu_usage_percent)
				return lhs.stat->cpu_usage_percent > rhs.stat->cpu_usage_percent;
			break;
		case ProcessTable::ProcessSortType::MEM:
			if (lhs.stat->mem_usage != rhs.stat->mem_usage)
				return lhs.stat->mem_usage > rhs.stat->mem_usage;
			break;
		case ProcessTable::ProcessSortType::DISK_READ:
			if (lhs.stat->disk_usage.disk_read.has_value() &&
			    !rhs.stat->disk_usage.disk_read.has_value()) {
				return true;
			}
			if (!lhs.stat->disk_usage.disk_read.has_value() &&
			    rhs.stat->disk_usage.disk_read.has_value()) {
				return false;
			}
			if (lhs.stat->disk_usage.disk_read.has_value() &&
			    rhs.stat->disk_usage.disk_read.has_value() &&
			    lhs.stat->disk_usage.disk_read.value() != rhs.stat->disk_usage.disk_read.value())
				return lhs.stat->disk_usage.disk_read > rhs.stat->disk_usage.disk_read;
			break;
		case ProcessTable::ProcessSortType::DISK_WRITE:
			if (lhs.stat->disk_usage.disk_write.has_value() &&
			    !rhs.stat->disk_usage.disk_write.has_value()) {
				return true;
			}
			if (!lhs.stat->disk_usage.disk_write.has_value() &&
			    rhs.stat->disk_usage.disk_write.has_value()) {
				return false;
			}
			if (lhs.stat->disk_usage.disk_write.has_value() &&
			    rhs.stat->disk_usage.disk_write.has_value() &&
			    lhs.stat->disk_usage.disk_write.value() != rhs.stat->disk_usage.disk_write.value())
				return lhs.stat->disk_usage.disk_write > rhs.stat->disk_usage.disk_write;
			break;
		case ProcessTable::ProcessSortType::NETWORK_RECV:
			if (lhs.stat->network_usage.network_recv != rhs.stat->network_usage.network_recv)
				return lhs.stat->network_usage.network_recv > rhs.stat->network_usage.network_recv;
			break;
		case ProcessTable::ProcessSortType::NETWORK_SEND:
			if (lhs.stat->network_usage.network_send != rhs.stat->network_usage.network_send)
				return lhs.stat->network_usage.network_send > rhs.stat->network_usage.network_send;
			break;
		}
		return lhs.pid < rhs.pid;
	};
}

std::vector<ProcessTable::ProcessFullStat> ProcessTable::get_sorted(ProcessSortType type,
                                                                    std::uint32_t limit) const {

	std::vector<ProcessTable::ProcessFullStat> ret;
	for (const auto &[pid, meta] : this->list_table) {
		const auto &stat = this->stat_table.at(pid);
		ret.push_back(std::move(ProcessTable::ProcessFullStat(pid, &meta, &stat)));
	}
	limit = std::min(limit, static_cast<std::uint32_t>(ret.size()));
	std::partial_sort(ret.begin(), ret.begin() + limit, ret.end(), get_comparator(type));
	ret.resize(limit);
	return ret;
}

#endif
#include "client/process_table.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <iostream>
#include <stdexcept>

#include "common/global.hpp"

#ifdef _WIN32

#include <iphlpapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <windows.h>
#include <winsock2.h>

namespace {

inline std::uint64_t filetime_to_uint64_t(const FILETIME& ft) {
	return ((std::uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

std::uint64_t get_system_total_cpu() {
	FILETIME idle, kernel, user;
	if (!GetSystemTimes(&idle, &kernel, &user)) {
		return 0;
	}
	return filetime_to_uint64_t(kernel) + filetime_to_uint64_t(user);
}

std::uint64_t get_process_total_cpu(HANDLE hProcess) {
	FILETIME create, exit, kernel, user;
	if (!GetProcessTimes(hProcess, &create, &exit, &kernel, &user)) {
		return 0;
	}
	return filetime_to_uint64_t(kernel) + filetime_to_uint64_t(user);
}

std::uint64_t get_process_memory(HANDLE hProcess) {
	PROCESS_MEMORY_COUNTERS pmc;
	if (!GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
		return 0;
	}
	return pmc.WorkingSetSize;
}

DiskStat get_process_total_disk_usage(HANDLE hProcess) {
	IO_COUNTERS io;
	DiskStat disk_usage;
	if (!GetProcessIoCounters(hProcess, &io)) {
		return disk_usage;
	}
	disk_usage.disk_read = io.ReadTransferCount;
	disk_usage.disk_write = io.WriteTransferCount;
	return disk_usage;
}

std::string get_process_name(HANDLE hProcess) {
	char process_name[MAX_PATH];
	DWORD size = GetModuleBaseNameA(hProcess, NULL, process_name, MAX_PATH);
	return std::string(process_name, process_name + size);
}

std::vector<std::uint32_t> get_list_pids() {
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32W pe;
	pe.dwSize = sizeof(pe);
	std::vector<std::uint32_t> list_pids;
	if (Process32FirstW(snapshot, &pe)) {
		do {
			list_pids.push_back(pe.th32ProcessID);
		} while (Process32NextW(snapshot, &pe));
	}
	CloseHandle(snapshot);
	return list_pids;
}

struct SocketPid {
	std::uint32_t pid;
	std::uint32_t local_ip;
	std::uint16_t local_port;
	std::uint32_t rem_ip;
	std::uint16_t rem_port;
};

std::vector<SocketPid> get_list_socket_pid() {
	DWORD size = 0;
	GetExtendedTcpTable(NULL, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
	PMIB_TCPTABLE_OWNER_PID tcpTable = (PMIB_TCPTABLE_OWNER_PID)malloc(size);

	std::vector<SocketPid> list_socket_pid;

	if (GetExtendedTcpTable(tcpTable, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) ==
	    NO_ERROR) {
		for (DWORD i = 0; i < tcpTable->dwNumEntries; ++i) {
			auto& row = tcpTable->table[i];
			SocketPid socket_pid;
			socket_pid.pid = row.dwOwningPid;
			socket_pid.local_ip = row.dwLocalAddr;
			socket_pid.rem_ip = row.dwRemoteAddr;
			socket_pid.local_port = ntohs((u_short)row.dwLocalPort);
			socket_pid.rem_port = ntohs((u_short)row.dwRemotePort);
			if (socket_pid.local_ip == 0)
				continue;
			if ((socket_pid.local_ip & 0xff) == 127)
				continue;
			list_socket_pid.push_back(socket_pid);
		}
	}

	free(tcpTable);
	return list_socket_pid;
}

} // namespace

void ProcessTable::update_table() {
	std::unordered_map<std::uint32_t, bool> seen;

	static std::uint64_t last_system_cpu = 0;
	std::uint64_t system_cpu = get_system_total_cpu();
	std::uint64_t system_cpu_delta = system_cpu - last_system_cpu;
	last_system_cpu = system_cpu;
	if (system_cpu_delta == 0) {
		system_cpu_delta = 1;
	}

	std::vector<std::uint32_t> list_pids = get_list_pids();

	std::vector<SocketPid> list_socket_pid = get_list_socket_pid();

	std::unordered_map<std::uint16_t, std::uint32_t> port_to_pid_map;
	for (const auto& socket_pid : list_socket_pid) {
		port_to_pid_map[socket_pid.local_port] = socket_pid.pid;
	}
	std::unordered_map<std::uint32_t, NetworkStat> pid_network_usage;
	std::uint32_t local_ip = list_socket_pid.begin()->local_ip;
	pid_network_usage = this->pcap_handler.process_packets(local_ip, port_to_pid_map);

	for (const auto& pid : list_pids) {
		// skip reading cmdline for now
		auto& process_listing = this->list_table[pid];
		auto& process_stat = this->stat_table[pid];
		auto& process_last_stat = this->last_stat_table[pid];

		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

		if (hProcess) {
			if (process_listing.get_process_name().empty()) {
				process_listing = std::move(
				    ProcessListing(pid, get_process_name(hProcess), std::vector<std::string>()));
			}
			std::uint64_t total_cpu = get_process_total_cpu(hProcess);
			std::uint64_t mem = get_process_memory(hProcess);
			DiskStat disk_stat = get_process_total_disk_usage(hProcess);

			std::uint64_t current_cpu_delta = total_cpu - process_last_stat.total_cpu_usage;
			process_last_stat.total_cpu_usage = total_cpu;
			std::uint32_t current_cpu_percent = static_cast<double>(current_cpu_delta) /
			                                    static_cast<double>(system_cpu_delta) * 100 *
			                                    CPU_UNIT;
			process_stat.cpu_usage_percent = current_cpu_percent;

			process_stat.mem_usage = mem;

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
			process_stat.network_usage = pid_network_usage[pid];

			CloseHandle(hProcess);
		}

		seen[pid] = true;
	}

	for (auto it = this->list_table.begin(); it != this->list_table.end();) {
		if (seen[it->first] == false) {
			this->stat_table.erase(it->first);
			this->last_stat_table.erase(it->first);
			it = this->list_table.erase(it);
		} else {
			++it;
		}
	}
}

#else

#include <assert.h>
#include <charconv>
#include <fcntl.h>
#include <fstream>
#include <unistd.h>

namespace {

// reusable buffer for reading all kind of stuff
static constexpr std::uint32_t BUFFER_SIZE = 4096;
static char buffer[BUFFER_SIZE];
static const long page_size = sysconf(_SC_PAGE_SIZE);

// read file to buffer, capped to BUFFER_SIZE
static ssize_t read_file(const std::string& path) {
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
	char* now = buffer;
	for (char* p = buffer; p < buffer + cmdline_read; ++p) {
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

struct CpuMemStat {
	std::uint64_t total_cpu_usage = 0;
	std::uint64_t mem_usage = 0;
};

static CpuMemStat read_pid_stat_cpu_mem(std::uint32_t pid) {
	ssize_t stat_read = read_file("/proc/" + std::to_string(pid) + "/stat");
	if (stat_read < 0) {
		throw std::runtime_error("read " + std::to_string(pid) + " stat failed");
	}
	CpuMemStat cpu_mem_stat;
	char* p = buffer;
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
		char* now = p;
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
	char* p = buffer;
	int field = 1;
	while (field <= 11) {
		if (*p == '\0') {
			throw std::runtime_error("error while parsing /proc/stat");
		}
		if (*p == ' ') {
			++p;
			continue;
		}
		char* now = p;
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
	char* p = buffer;
	int line = 1;
	// std::cerr << "pid: " << pid << " buffer: " << buffer << "\n\n";
	while (line <= 6) {
		if (line == 5 || line == 6) {
			while (*p != ' ')
				++p;
			++p;
			char* now = p;
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

static std::uint64_t parse_socket_inode(const std::string& link) {
	if (link.substr(0, 8) != "socket:[")
		return 0;
	std::uint64_t number;
	std::from_chars(link.c_str() + 8, link.c_str() + link.size() - 9, number);
	return number;
}

static std::vector<std::uint32_t> get_socket_inode_list(std::uint32_t pid) {
	std::filesystem::path fd_path = "/proc/" + std::to_string(pid) + "/fd";
	std::vector<uint32_t> sockets;
	try {
		for (auto& entry : std::filesystem::directory_iterator(fd_path)) {
			try {
				std::string link = std::filesystem::read_symlink(entry.path()).string();
				std::uint32_t inode = parse_socket_inode(link);
				if (inode) {
					sockets.push_back(inode);
				}
			} catch (const std::filesystem::filesystem_error& e) {
				continue;
			}
		}
	} catch (const std::filesystem::filesystem_error& e) {
		std::cerr << e.what() << "\n";
	}
	return sockets;
}

struct SocketInode {
	std::uint32_t inode;
	std::uint32_t local_ip;
	std::uint16_t local_port;
	std::uint32_t rem_ip;
	std::uint16_t rem_port;
};

static std::unordered_map<std::uint32_t, SocketInode> parse_net_tcp() {
	std::ifstream tcp("/proc/net/tcp");
	std::string line;
	std::getline(tcp, line); // skip header
	std::unordered_map<std::uint32_t, SocketInode> ret;

	while (std::getline(tcp, line)) {
		std::istringstream iss(line);
		SocketInode inode;
		std::string dummy;
		char dummy_char;

		iss >> dummy;

		iss >> std::hex >> inode.local_ip;

		// loop back
		if ((inode.local_ip & 0xff) == 127) {
			continue;
		}

		iss >> dummy_char; // ':'
		iss >> std::hex >> inode.local_port;

		iss >> std::hex >> inode.rem_ip;
		iss >> dummy_char; // ':'
		iss >> std::hex >> inode.rem_port;
		for (int i = 4; i <= 9; ++i) {
			iss >> dummy;
		}
		iss >> std::dec >> inode.inode;
		ret[inode.inode] = std::move(inode);
	}
	return ret;
}

static std::vector<std::uint32_t> list_pids() {
	const std::filesystem::path proc_path("/proc");
	std::vector<std::uint32_t> pids;
	for (const auto& entry : std::filesystem::directory_iterator(proc_path)) {
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

	std::unordered_map<std::uint32_t, bool> seen;

	std::vector<std::uint32_t> pids = list_pids();
	std::unordered_map<std::uint32_t, SocketInode> inode_list_all = parse_net_tcp();
	// parse /proc/pid/fd for socket inode of pid, then parse /proc/net/tcp for inode list, then
	// combine for port -> pid map
	std::unordered_map<std::uint16_t, std::uint32_t> port_to_pid_map;
	for (std::uint32_t pid : pids) {
		std::vector<std::uint32_t> inode_list_of_pid = get_socket_inode_list(pid);
		if (inode_list_of_pid.empty())
			continue;
		for (const auto& inode : inode_list_of_pid) {
			auto it = inode_list_all.find(inode);
			if (it != inode_list_all.end()) {
				port_to_pid_map[it->second.local_port] = pid;
			}
		}
	}
	std::unordered_map<std::uint32_t, NetworkStat> pid_network_usage;
	std::uint32_t local_ip = inode_list_all.begin()->second.local_ip;
	pid_network_usage = this->pcap_handler.process_packets(local_ip, port_to_pid_map);

	for (std::uint32_t pid : pids) {
		auto& process_listing = this->list_table[pid];
		try {
			if (process_listing.get_args().empty()) {
				process_listing = std::move(read_process_metadata(pid));
			}
			// if it can't read then it is probably kernel thread
			if (process_listing.get_args().empty())
				continue;
			auto& process_stat = this->stat_table[pid];
			auto& process_last_stat = this->last_stat_table[pid];

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
			process_stat.network_usage = pid_network_usage[pid];
			seen[pid] = true;
		} catch (std::exception& e) {
			std::cerr << e.what() << "\n";
		}
	}

	for (auto it = this->list_table.begin(); it != this->list_table.end();) {
		if (seen[it->first] == false) {
			this->stat_table.erase(it->first);
			this->last_stat_table.erase(it->first);
			it = this->list_table.erase(it);
		} else {
			++it;
		}
	}
}

#endif

void ProcessTable::setup_network() {
	pcap_handler.setup_network();
}

std::string ProcessTable::display_table() const {
	constexpr uint32_t max_char_per_line = 140; // arbitrarily chosen
	constexpr uint32_t num_lines = 30;          // arbitrarily chosen
	std::string res;
	res.reserve(max_char_per_line * (num_lines + 1) + 100);
#ifdef _WIN32
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwMode = 0;
	GetConsoleMode(hOut, &dwMode);
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(hOut, dwMode);
#endif
	res += "\033[H\033[J";
	auto fullstat_table = this->get_sorted(ProcessTable::ProcessSortType::MEM, num_lines);
#define FORMAT_STRING "{:<7} {:<10} {:<13} {:<10} {:<10} {:<10} {:<10} {:<20} {:<40}"
	res += std::format(FORMAT_STRING, "pid", "cpu", "mem", "disk read", "disk write", "net recv",
	                   "net send", "programm", "cmdline");
	res += "\n";
	for (const auto& [pid, listing, stat] : fullstat_table) {
		std::string disk_read = stat->disk_usage.disk_read.has_value()
		                            ? std::to_string(stat->disk_usage.disk_read.value())
		                            : "N/A";
		std::string disk_write = stat->disk_usage.disk_write.has_value()
		                             ? std::to_string(stat->disk_usage.disk_write.value())
		                             : "N/A";
		std::string network_recv = stat->network_usage.network_recv.has_value()
		                               ? std::to_string(stat->network_usage.network_recv.value())
		                               : "N/A";
		std::string network_send = stat->network_usage.network_send.has_value()
		                               ? std::to_string(stat->network_usage.network_send.value())
		                               : "N/A";
		std::string line =
		    std::format(FORMAT_STRING, pid, static_cast<double>(stat->cpu_usage_percent) / CPU_UNIT,
		                stat->mem_usage, disk_read, disk_write, network_recv, network_send,
		                listing->get_process_name(), listing->get_args_as_string());
		if (line.size() > max_char_per_line)
			line.resize(max_char_per_line);
		res += line;
		res += "\n";
	}
#undef FORMAT_STRING
	return res;
}

static auto get_comparator(ProcessTable::ProcessSortType type) {
	return [type](const ProcessTable::ProcessFullStat& lhs,
	              const ProcessTable::ProcessFullStat& rhs) {
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
			if (lhs.stat->network_usage.network_recv.has_value() &&
			    !rhs.stat->network_usage.network_recv.has_value()) {
				return true;
			}
			if (!lhs.stat->network_usage.network_recv.has_value() &&
			    rhs.stat->network_usage.network_recv.has_value()) {
				return false;
			}
			if (lhs.stat->network_usage.network_recv.has_value() &&
			    rhs.stat->network_usage.network_recv.has_value() &&
			    lhs.stat->network_usage.network_recv.value() !=
			        rhs.stat->network_usage.network_recv.value())
				return lhs.stat->network_usage.network_recv > rhs.stat->network_usage.network_recv;
			break;
		case ProcessTable::ProcessSortType::NETWORK_SEND:
			if (lhs.stat->network_usage.network_send.has_value() &&
			    !rhs.stat->network_usage.network_send.has_value()) {
				return true;
			}
			if (!lhs.stat->network_usage.network_send.has_value() &&
			    rhs.stat->network_usage.network_send.has_value()) {
				return false;
			}
			if (lhs.stat->network_usage.network_send.has_value() &&
			    rhs.stat->network_usage.network_send.has_value() &&
			    lhs.stat->network_usage.network_send.value() !=
			        rhs.stat->network_usage.network_send.value())
				return lhs.stat->network_usage.network_send > rhs.stat->network_usage.network_send;
			break;
		}
		return lhs.pid < rhs.pid;
	};
}

std::vector<ProcessTable::ProcessFullStat> ProcessTable::get_sorted(ProcessSortType type,
                                                                    std::uint32_t limit) const {

	std::vector<ProcessTable::ProcessFullStat> ret;
	for (const auto& [pid, listing] : this->list_table) {
		const auto& stat = this->stat_table.at(pid);
		ret.push_back(std::move(ProcessTable::ProcessFullStat(pid, &listing, &stat)));
	}
	if (limit == 0)
		limit = static_cast<std::uint32_t>(ret.size());
	else
		limit = std::min(limit, static_cast<std::uint32_t>(ret.size()));
	std::partial_sort(ret.begin(), ret.begin() + limit, ret.end(), get_comparator(type));
	ret.resize(limit);
	return ret;
}

std::unordered_map<std::string, ProcessStat> ProcessTable::get_map_program_name_resource() const {
	std::unordered_map<std::string, ProcessStat> process_name_to_total_resource;
	for (const auto& [pid, listing] : this->list_table) {
		const auto& stat = this->stat_table.at(pid);
		auto& this_name = process_name_to_total_resource[listing.get_process_name()];
		this_name.cpu_usage_percent += stat.cpu_usage_percent;
		this_name.mem_usage += stat.mem_usage;
		this_name.disk_usage.disk_read =
		    this_name.disk_usage.disk_read.value_or(0) + stat.disk_usage.disk_read.value_or(0);
		this_name.disk_usage.disk_write =
		    this_name.disk_usage.disk_write.value_or(0) + stat.disk_usage.disk_write.value_or(0);
		this_name.network_usage.network_recv = this_name.network_usage.network_recv.value_or(0) +
		                                       stat.network_usage.network_recv.value_or(0);
		this_name.network_usage.network_send = this_name.network_usage.network_send.value_or(0) +
		                                       stat.network_usage.network_send.value_or(0);
	}
	return process_name_to_total_resource;
}

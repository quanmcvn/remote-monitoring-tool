#include "client/client_logger.hpp"

#include <chrono>

ClientLogger::ClientLogger(Config n_config, LogQueue n_log_queue)
    : config(std::move(n_config)), log_queue(std::move(n_log_queue)) {}

void ClientLogger::generate_log(const ProcessTable& process_table) {
	auto map = process_table.get_map_program_name_resource();
	auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
	                  std::chrono::system_clock::now().time_since_epoch())
	                  .count();
	for (const auto& config_entry : config.get_config_entries()) {
		std::string name = config_entry.get_process_name();
		const auto& stat = map[name];
		if (stat.cpu_usage_percent > config_entry.get_cpu_usage()) {
			log_queue.add_log(LogEntry(log_queue.get_new_id(), name, now_ms, LogType::CPU,
			                           stat.cpu_usage_percent));
			std::cerr << now_ms << ": " << name << ": " << "cpu\n";
		}
		if (stat.mem_usage > config_entry.get_mem_usage()) {
			log_queue.add_log(
			    LogEntry(log_queue.get_new_id(), name, now_ms, LogType::MEM, stat.mem_usage));
			std::cerr << now_ms << ": " << name << ": " << "mem\n";
		}
		if (stat.disk_usage.disk_read.value_or(0) + stat.disk_usage.disk_write.value_or(0) >
		    config_entry.get_disk_usage()) {
			log_queue.add_log(LogEntry(log_queue.get_new_id(), name, now_ms, LogType::DISK,
			                           stat.disk_usage.disk_read.value_or(0) +
			                               stat.disk_usage.disk_write.value_or(0)));
			std::cerr << now_ms << ": " << name << ": " << "disk\n";
		}
		if (stat.network_usage.network_recv.value_or(0) +
		        stat.network_usage.network_send.value_or(0) >
		    config_entry.get_network_usage()) {
			log_queue.add_log(LogEntry(log_queue.get_new_id(), name, now_ms, LogType::NET,
			                           stat.network_usage.network_recv.value_or(0) +
			                               stat.network_usage.network_send.value_or(0)));
			std::cerr << now_ms << ": " << name << ": " << "net\n";
		}
	}
}
#include "client/client_logger.hpp"

#include "common/network_event.hpp"
#include "common/util.hpp"

#include <iostream>

ClientLogger::ClientLogger(Config n_config, LogQueue n_log_queue, EventBus& event_bus)
    : config(std::move(n_config)), log_queue(std::move(n_log_queue)) {
	event_bus.subscribe([this](const Event& event) {
		std::cerr << "client_logger: got event\n";
		if (const NetworkEvent* network_event_ptr = dynamic_cast<const NetworkEvent*>(&event)) {
			const NetworkEvent& network_event = *network_event_ptr;
			std::string payload = network_event.get_payload();
			std::istringstream iss(payload, std::ios::binary);
			std::string message = SerializableHelper::read_string(iss);
			std::cerr << "client_logger: network event: " << message << "\n";
			if (message.starts_with("ack ")) {
				std::uint64_t id;
				std::string id_str = message.substr(std::string("ack ").size());
				std::from_chars(id_str.c_str(), id_str.c_str() + id_str.size(), id);
				if (id <= 0) {
					std::cerr << "client_logger: can't parse id (got '" << id << "') from "
					          << message << "\n";
					return;
				}
				std::cerr << "client_logger: got id " << id << "\n";
				this->log_queue.ack_log(id);
			}
		}
	});
}

void ClientLogger::generate_log(const ProcessTable& process_table) {
	auto map = process_table.get_map_program_name_resource();
	auto now_ms = get_timestamp_ms();
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

std::vector<LogEntry> ClientLogger::get_batch_log(std::uint32_t batch_size) const {
	return this->log_queue.get_batch_log(batch_size);
}

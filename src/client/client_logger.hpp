#ifndef CLIENT_CLIENT_LOGGER_HPP
#define CLIENT_CLIENT_LOGGER_HPP

#include "client/log_queue.hpp"
#include "client/process_table.hpp"
#include "common/config.hpp"
#include "common/event_bus.hpp"

// holds config and list of logs
// tries to log to a queue to be sent
class ClientLogger {
private:
	Config config;
	LogQueue log_queue;

public:
	ClientLogger(Config config, LogQueue log_queue, EventBus& event_bus);
	void generate_log(const ProcessTable& process_table);
	// delegate to internal log queue
	std::vector<LogEntry> get_batch_log(std::uint32_t batch_size) const;
};

#endif
#ifndef CLIENT_CLIENT_LOGGER_HPP
#define CLIENT_CLIENT_LOGGER_HPP

#include "client/process_table.hpp"
#include "client/log_queue.hpp"
#include "common/config.hpp"

// holds config and list of logs
// tries to log to a queue to be sent
class ClientLogger {
private:
	Config config;
	LogQueue log_queue;

public:
	ClientLogger(Config n_config, LogQueue n_log_queue);
	void generate_log(const ProcessTable& process_table);
};

#endif
#ifndef CLIENT_LOG_QUEUE_HPP
#define CLIENT_LOG_QUEUE_HPP

#include <deque>

#include "common/log_entry.hpp"

// holds a queue of logs, additionally writes to disk for safe keeping
class LogQueue {
private:
	std::string log_file;
	std::string ack_file;
	std::uint64_t ack_id;
	// use deque for efficient peeking
	std::deque<LogEntry> log_entries;
	std::uint64_t new_id;

private:
	void clean_old_logs() const;

public:
	LogQueue(const std::string& log_file, const std::string& ack_file);
	~LogQueue();
	// add entry to log, write to disk
	void add_log(LogEntry log_entry);
	std::vector<LogEntry> get_batch_log(std::uint32_t batch_size) const;
	// ack every log entry with id <= id, mark them for clearing out of disk
	// effectively tcp's ack
	void ack_log(std::uint64_t id);
	std::uint64_t get_new_id();
};

#endif
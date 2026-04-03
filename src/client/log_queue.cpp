#include "client/log_queue.hpp"

#include <fstream>
#include <iostream>

#include "common/global.hpp"

namespace {
const int MAX_LOG_FILE_SIZE = 10 * MB;
}

LogQueue::LogQueue(const std::string& n_log_file, const std::string& n_ack_file)
    : log_file(n_log_file), ack_file(n_ack_file), new_id(1) {
	std::ifstream ack_file_in(ack_file);
	if (!ack_file_in) {
		std::ofstream ack_file_out(ack_file);
		if (!ack_file_out) {
			std::cerr << "log_queue: can't create ack file " << ack_file << "\n";
		} else {
			ack_file_out << 0;
		}
		this->ack_id = 0;
	} else {
		ack_file_in >> this->ack_id;
	}

	std::ifstream log_file_in(log_file);
	if (!log_file_in) {
		std::ofstream log_file_out(log_file);
		if (!log_file_out) {
			std::cerr << "log_queue: can't create log file " << log_file << "\n";
		}
	} else {
		while (!log_file_in.eof()) {
			LogEntry log_entry;
			log_entry.deserialize_str(log_file_in);
			if (log_entry.get_id() > this->ack_id) {
				this->log_entries.push_back(std::move(log_entry));
			}
		}
	}
	if (!log_entries.empty()) {
		new_id = log_entries.back().get_id() + 1;
	}
}

LogQueue::~LogQueue() {
	this->clean_old_logs();
	std::ofstream ack_file_out(ack_file);
	if (!ack_file_out) {
		std::cerr << "log_queue: can't write to ack file " << ack_file << "\n";
	} else {
		ack_file_out << this->ack_id;
	}
}

void LogQueue::clean_old_logs() const {
	std::ifstream log_file_in(log_file, std::ios::ate | std::ios::binary);
	if (log_file_in) {
		std::size_t file_size = log_file_in.tellg();
		if (file_size <= MAX_LOG_FILE_SIZE) {
			return;
		}
	} else {
		return;
	}
	std::string temp_log_file = log_file + ".tmp";

	std::ofstream temp_log_file_out(temp_log_file);
	if (!temp_log_file_out) {
		std::cerr << "log_queue: can't open temporary log file " << temp_log_file
		          << " for cleanup\n";
		return;
	}

	for (const auto& log_entry : this->log_entries) {
		if (log_entry.get_id() > this->ack_id) {
			log_entry.serialize_str(temp_log_file_out);
		}
	}

	temp_log_file_out.flush();

	if (std::rename(temp_log_file.c_str(), log_file.c_str()) != 0) {
		std::cerr << "log_queue: failed to replace the log file with the temporary file\n";
	} else {
		std::cerr << "log_queue: successfully replaced the log file with acked logs\n";
	}
}

void LogQueue::add_log(LogEntry log_entry) {
	// doesn't need to clear right now
	// clean_old_logs();
	std::ofstream log_file_out(log_file, std::ios::app);
	if (!log_file_out) {
		std::cerr << "log_queue: can't write to log file " << log_file << "\n";
	} else {
		log_entry.serialize_str(log_file_out);
		log_file_out.flush();
	}
	this->log_entries.push_back(std::move(log_entry));
}

std::vector<LogEntry> LogQueue::get_batch_log(std::uint32_t batch_size) const {
	batch_size = std::min(batch_size, static_cast<std::uint32_t>(log_entries.size()));
	std::vector<LogEntry> peek;
	for (std::uint32_t i = 0; i < batch_size; ++i) {
		peek.push_back(log_entries.at(i));
	}
	return peek;
}

void LogQueue::ack_log(std::uint64_t id) {
	if (id <= this->ack_id)
		return;
	std::cerr << "log_queue: ack " << id << "\n";
	this->ack_id = id;
	while (!log_entries.empty() && log_entries.front().get_id() <= this->ack_id) {
		log_entries.pop_front();
	}
	std::ofstream ack_file_out(ack_file);
	if (!ack_file_out) {
		std::cerr << "log_queue: can't write to ack file " << ack_file << "\n";
	} else {
		ack_file_out << this->ack_id;
	}
}

std::uint64_t LogQueue::get_new_id() { return new_id++; }

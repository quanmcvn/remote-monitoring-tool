#include "common/log_entry.hpp"

#include <charconv>
#include <sstream>

LogEntry::LogEntry() {}

LogEntry::LogEntry(std::uint64_t n_id, std::string n_process_name, std::uint64_t n_timestamp_ms,
                   LogType n_log_type, std::uint64_t n_value)
    : id(n_id), process_name(n_process_name), timestamp_ms(n_timestamp_ms), log_type(n_log_type),
      value(n_value) {}

void LogEntry::serialize(std::ostream& os) const {
	SerializableHelper::write_uint64(os, this->id);
	SerializableHelper::write_string(os, this->process_name);
	SerializableHelper::write_uint64(os, this->timestamp_ms);
	SerializableHelper::write_uint32(os, static_cast<std::uint32_t>(this->log_type));
	SerializableHelper::write_uint64(os, this->value);
}

void LogEntry::deserialize(std::istream& is) {
	this->id = SerializableHelper::read_uint64(is);
	this->process_name = SerializableHelper::read_string(is);
	this->timestamp_ms = SerializableHelper::read_uint64(is);
	this->log_type = static_cast<LogType>(SerializableHelper::read_uint32(is));
	this->value = SerializableHelper::read_uint64(is);
}

void LogEntry::serialize_str(std::ostream& os) const {
	os << this->id << "|" << this->process_name << "|" << this->timestamp_ms << "|"
	   << static_cast<std::uint32_t>(this->log_type) << "|" << this->value << "\n";
}
void LogEntry::deserialize_str(std::istream& is) {
	std::string line;
	if (!std::getline(is, line))
		return;
	std::istringstream ss(line);
	std::string temp;
	std::getline(ss, temp, '|');
	std::from_chars(temp.c_str(), temp.c_str() + temp.size(), this->id);
	std::getline(ss, this->process_name, '|');
	std::getline(ss, temp, '|');
	std::from_chars(temp.c_str(), temp.c_str() + temp.size(), this->timestamp_ms);
	std::getline(ss, temp, '|');
	std::uint32_t temp_number;
	std::from_chars(temp.c_str(), temp.c_str() + temp.size(), temp_number);
	this->log_type = static_cast<LogType>(temp_number);
	std::getline(ss, temp, '|');
	std::from_chars(temp.c_str(), temp.c_str() + temp.size(), this->value);
}

std::uint64_t LogEntry::get_id() const { return id; }
std::string LogEntry::get_process_name() const { return process_name; }
std::uint64_t LogEntry::get_timestamp_ms() const { return timestamp_ms; }
LogType LogEntry::get_log_type() const { return log_type; }
std::uint64_t LogEntry::get_value() const { return value; }

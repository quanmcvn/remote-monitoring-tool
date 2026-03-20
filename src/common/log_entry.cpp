#include "common/log_entry.hpp"

void LogEntry::serialize(std::ostream &os) const {
	SerializableHelper::write_string(os, this->process_name);
	SerializableHelper::write_uint64(os, this->timestamp_ms);
	SerializableHelper::write_uint32(os, static_cast<std::uint32_t>(this->log_type));
}

void LogEntry::deserialize(std::istream &is) {
	this->process_name = SerializableHelper::read_string(is);
	this->timestamp_ms = SerializableHelper::read_uint64(is);
	this->log_type = static_cast<LogType>(SerializableHelper::read_uint32(is));
}

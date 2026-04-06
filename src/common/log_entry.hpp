#ifndef COMMON_LOG_ENTRY_HPP
#define COMMON_LOG_ENTRY_HPP

#include "common/serializable.hpp"

#include <cstdint>
#include <string>

enum LogType { CPU = 1, MEM, DISK, NET };

class LogEntry : public ISerializable {
private:
	std::uint64_t id;
	std::string process_name;
	std::uint64_t timestamp_ms;
	LogType log_type;
	std::uint64_t value;

public:
	LogEntry();
	LogEntry(std::uint64_t id, std::string process_name, std::uint64_t timestamp_ms,
	         LogType log_type, std::uint64_t value);
	void serialize(std::ostream& os) const override;
	void deserialize(std::istream& is) override;
	// serialize, but not raw byte
	void serialize_str(std::ostream& os) const;
	int deserialize_str(std::istream& is);

	std::uint64_t get_id() const;
	std::string get_process_name() const;
	std::uint64_t get_timestamp_ms() const;
	LogType get_log_type() const;
	std::uint64_t get_value() const;
};

#endif
#ifndef COMMON_LOG_ENTRY_HPP
#define COMMON_LOG_ENTRY_HPP

#include "common/serializable.hpp"

#include <string>
#include <cstdint>

enum LogType {
	CPU,
	MEM,
	DISK,
	NET
};

class LogEntry : public ISerializable {
private:
	std::string process_name;
	std::uint64_t timestamp_ms;
	LogType log_type;
public:
	void serialize(std::ostream &os) const override;
	void deserialize(std::istream &is) override;
};

#endif
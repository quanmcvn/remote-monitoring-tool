#ifndef COMMON_CONFIG_ENTRY_HPP
#define COMMON_CONFIG_ENTRY_HPP

#include "common/serializable.hpp"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class ConfigEntry : public ISerializable {
private:
	std::string process_name;
	std::uint32_t cpu_usage;
	std::uint64_t mem_usage;
	std::uint64_t disk_usage;
	std::uint64_t network_usage;

public:
	ConfigEntry();
	ConfigEntry(std::string n_process_name, std::uint32_t n_cpu_usage,
	            std::uint64_t n_mem_usage, std::uint64_t n_disk_usage,
	            std::uint64_t n_network_usage);
	void serialize(std::ostream &os) const override;
	void deserialize(std::istream &is) override;
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(ConfigEntry, process_name, cpu_usage, mem_usage,
	                               disk_usage, network_usage)
	
	// change config to match real unit after a read
	// X *= X_UNIT
	void convert_after_read();
	// change config to match real unit before a write
	// X /= X_UNIT
	void convert_before_write();
};
#endif
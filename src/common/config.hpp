#ifndef COMMON_CONFIG_HPP
#define COMMON_CONFIG_HPP

#include "common/config_entry.hpp"

#include <vector>

class Config : public ISerializable {
private:
	std::vector<ConfigEntry> config_entries;
public:
	void serialize(std::ostream &os) const override;
	void deserialize(std::istream &is) override;
	// write config to predefined path/registry
	int write_config() const;
	// read config from predefined path/registry
	int read_config();
	static Config default_config();
	const std::vector<ConfigEntry>& get_config_entries() const;
};

#endif
#include "common/config.hpp"

#include <filesystem>
#include <nlohmann/json.hpp>

void Config::serialize(std::ostream &os) const {
	SerializableHelper::write_vector_serializeable<ConfigEntry>(os, this->config_entries);
}

void Config::deserialize(std::istream &is) {
	this->config_entries = SerializableHelper::read_vector_serializeable<ConfigEntry>(is);
}

#ifdef _WIN32

// NYI
int Config::write_config() const { return 0; }

// NYI
int Config::read_config() { return 0; }

#else

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>

int Config::write_config() const {
	struct passwd *pw = getpwuid(getuid());
	std::filesystem::path config_dir = std::filesystem::path(pw->pw_dir) / ".config/rmt";
	std::filesystem::create_directories(config_dir);

	std::ofstream out((config_dir / "rmt-config.json").string());

	if (!out) return 1;

	auto config_entries_to_write = config_entries;

	for (auto& config_entry : config_entries_to_write) {
		config_entry.convert_before_write();
	}
	nlohmann::json json(config_entries_to_write);

	out << json.dump();

	return 0;
}

int Config::read_config() {
	struct passwd *pw = getpwuid(getuid());
	std::filesystem::path config_dir = std::filesystem::path(pw->pw_dir) / ".config/rmt";

	std::ifstream in((config_dir / "rmt-config.json").string());

	if (!in) return 1;

	config_entries = nlohmann::json::parse(in);
	for (auto& config_entry : config_entries) {
		config_entry.convert_after_read();
	}

	return 0;
}

#endif

Config Config::default_config() {
	Config ret;
	ret.config_entries.emplace_back("test_prog", 1, 11, 1, 1);
	ret.config_entries.emplace_back("chrome.exe", 10, 200, 1, 500);
	ret.config_entries.emplace_back("devenv.exe", 50, 100, 10, 100);
	for (auto& config_entry : ret.config_entries) {
		config_entry.convert_after_read();
	}
	return ret;
}

const std::vector<ConfigEntry>& Config::get_config_entries() const {
	return this->config_entries;
}

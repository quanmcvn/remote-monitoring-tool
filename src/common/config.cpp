#include "common/config.hpp"

#include <filesystem>
#include <nlohmann/json.hpp>

#include "common/util.hpp"

Config::~Config() {
	// write config when quitting
	this->write_config();
}

void Config::serialize(std::ostream& os) const {
	SerializableHelper::write_vector_serializeable<ConfigEntry>(os, this->config_entries);
}

void Config::deserialize(std::istream& is) {
	this->config_entries = SerializableHelper::read_vector_serializeable<ConfigEntry>(is);
}

#ifdef _WIN32

#include <string>
#include <vector>
#include <iostream>

#define REGISTRY_PREFIX L"Software\\RemoteMonitoringTool\\config"

int Config::write_config() const {
	RegDeleteTreeW(HKEY_CURRENT_USER, REGISTRY_PREFIX);
	{
		reg::Key key(HKEY_CURRENT_USER, REGISTRY_PREFIX);
		key.set_dword(L"count", this->config_entries.size());
	}
	auto config_entries_to_write = config_entries;
	for (auto& config_entry : config_entries_to_write) {
		config_entry.convert_before_write();
	}

	for (int i = 0; i < config_entries_to_write.size(); ++i) {
		reg::Key key(HKEY_CURRENT_USER, REGISTRY_PREFIX + std::wstring(L"\\") + std::to_wstring(i));
		config_entries_to_write[i].serialize_registry(key);
	}
	// pretty much always work
	return 0;
}

int Config::read_config() {
	int return_code = 0;
	std::error_code ec;
	int count = 0;
	{
		reg::Key key(HKEY_CURRENT_USER, REGISTRY_PREFIX);
		auto n_count = key.get_dword(L"count", ec);
		if (ec || !n_count.has_value()) {
			std::cerr << "config: failed reading 'count' from " << to_string(REGISTRY_PREFIX)
			          << "\n";
			return 1;
		}
		count = static_cast<int>(n_count.value());
	}
	this->config_entries.resize(count);
	for (int i = 0; i < count; ++i) {
		reg::Key key(HKEY_CURRENT_USER, REGISTRY_PREFIX + std::wstring(L"\\") + std::to_wstring(i));
		if (this->config_entries[i].deserialize_registry(key) != 0) {
			return_code = 1;
		}
	}
	for (auto& config_entry : this->config_entries) {
		config_entry.convert_after_read();
	}
	return return_code;
}

#undef REGISTRY_PREFIX
#else

#include <fstream>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

int Config::write_config() const {
	struct passwd* pw = getpwuid(getuid());
	std::filesystem::path config_dir = std::filesystem::path(pw->pw_dir) / ".config/rmt";
	std::filesystem::create_directories(config_dir);

	std::ofstream out((config_dir / "rmt-config.json").string());

	if (!out)
		return 1;

	auto config_entries_to_write = config_entries;

	for (auto& config_entry : config_entries_to_write) {
		config_entry.convert_before_write();
	}
	nlohmann::json json(config_entries_to_write);

	out << json.dump();

	return 0;
}

int Config::read_config() {
	struct passwd* pw = getpwuid(getuid());
	std::filesystem::path config_dir = std::filesystem::path(pw->pw_dir) / ".config/rmt";

	std::ifstream in((config_dir / "rmt-config.json").string());

	if (!in)
		return 1;

	config_entries = nlohmann::json::parse(in);
	for (auto& config_entry : config_entries) {
		config_entry.convert_after_read();
	}

	return 0;
}

#endif

Config Config::default_config() {
	Config ret;
	ret.config_entries.emplace_back("test_prog.exe", 1, 11, 1, 1);
	ret.config_entries.emplace_back("chrome.exe", 10, 200, 1, 500);
	ret.config_entries.emplace_back("devenv.exe", 50, 100, 10, 100);
	for (auto& config_entry : ret.config_entries) {
		config_entry.convert_after_read();
	}
	return ret;
}

const std::vector<ConfigEntry>& Config::get_config_entries() const { return this->config_entries; }

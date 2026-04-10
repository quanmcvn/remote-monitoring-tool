#include "common/config_entry.hpp"

#include "common/global.hpp"
#include "common/serializable.hpp"
#include "common/util.hpp"
#include <iostream>

ConfigEntry::ConfigEntry() {}

ConfigEntry::ConfigEntry(std::string n_process_name, std::uint32_t n_cpu_usage,
                         std::uint64_t n_mem_usage, std::uint64_t n_disk_usage,
                         std::uint64_t n_network_usage)
    : process_name(std::move(n_process_name)), cpu_usage(n_cpu_usage), mem_usage(n_mem_usage),
      disk_usage(n_disk_usage), network_usage(n_network_usage) {}

void ConfigEntry::serialize(std::ostream& os) const {
	SerializableHelper::write_string(os, this->process_name);
	SerializableHelper::write_uint32(os, this->cpu_usage);
	SerializableHelper::write_uint64(os, this->mem_usage);
	SerializableHelper::write_uint64(os, this->disk_usage);
	SerializableHelper::write_uint64(os, this->network_usage);
}

void ConfigEntry::deserialize(std::istream& is) {
	this->process_name = SerializableHelper::read_string(is);
	this->cpu_usage = SerializableHelper::read_uint32(is);
	this->mem_usage = SerializableHelper::read_uint64(is);
	this->disk_usage = SerializableHelper::read_uint64(is);
	this->network_usage = SerializableHelper::read_uint64(is);
}

void ConfigEntry::convert_after_read() {
	this->cpu_usage *= CPU_UNIT;
	this->mem_usage *= MEM_UNIT;
	this->disk_usage *= DISK_UNIT;
	this->network_usage *= NETWORK_UNIT;
}

void ConfigEntry::convert_before_write() {
	this->cpu_usage /= CPU_UNIT;
	this->mem_usage /= MEM_UNIT;
	this->disk_usage /= DISK_UNIT;
	this->network_usage /= NETWORK_UNIT;
}

std::string ConfigEntry::get_process_name() const { return this->process_name; };
std::uint32_t ConfigEntry::get_cpu_usage() const { return this->cpu_usage; };
std::uint64_t ConfigEntry::get_mem_usage() const { return this->mem_usage; };
std::uint64_t ConfigEntry::get_disk_usage() const { return this->disk_usage; };
std::uint64_t ConfigEntry::get_network_usage() const { return this->network_usage; };

#ifdef _WIN32
void ConfigEntry::serialize_registry(RegKey& reg_key) const {
	reg_key.set_wstring(L"process_name", to_wstring(this->process_name));
	reg_key.set_dword(L"cpu_usage", this->cpu_usage);
	reg_key.set_qword(L"mem_usage", this->mem_usage);
	reg_key.set_qword(L"disk_usage", this->disk_usage);
	reg_key.set_qword(L"network_usage", this->network_usage);
}

int ConfigEntry::deserialize_registry(RegKey& reg_key) {
	std::error_code ec;
	int return_code = 0;
	auto n_process_name = reg_key.get_wstring(L"process_name", ec);
	if (ec || !n_process_name.has_value()) {
		std::cerr << "error in reading 'process_name' from reg_key: " << reg_key.get_sub_key()
		          << "\n";
		return_code = 1;
	} else
		this->process_name = to_string(n_process_name.value());
	auto n_cpu_usage = reg_key.get_dword(L"cpu_usage", ec);
	if (ec || !n_cpu_usage.has_value()) {
		std::cerr << "error in reading 'cpu_usage' from reg_key: " << reg_key.get_sub_key() << "\n";
		return_code = 1;
	} else
		this->cpu_usage = n_cpu_usage.value();
	auto n_mem_usage = reg_key.get_qword(L"mem_usage", ec);
	if (ec || !n_mem_usage.has_value()) {
		std::cerr << "error in reading 'mem_usage' from reg_key: " << reg_key.get_sub_key() << "\n";
		return_code = 1;
	} else
		this->mem_usage = n_mem_usage.value();
	auto n_disk_usage = reg_key.get_qword(L"disk_usage", ec);
	if (ec || !n_disk_usage.has_value()) {
		std::cerr << "error in reading 'disk_usage' from reg_key: " << reg_key.get_sub_key()
		          << "\n";
		return_code = 1;
	} else
		this->disk_usage = n_disk_usage.value();
	auto n_network_usage = reg_key.get_qword(L"network_usage", ec);
	if (ec || !n_network_usage.has_value()) {
		std::cerr << "error in reading 'network_usage' from reg_key: " << reg_key.get_sub_key()
		          << "\n";
		return_code = 1;
	} else
		this->network_usage = n_network_usage.value();
	return return_code;
}
#endif

#include "common/config_entry.hpp"

#include "common/global.hpp"
#include "common/serializable.hpp"

ConfigEntry::ConfigEntry() {}

ConfigEntry::ConfigEntry(std::string n_process_name, std::uint32_t n_cpu_usage,
                         std::uint64_t n_mem_usage, std::uint64_t n_disk_usage,
                         std::uint64_t n_network_usage)
    : process_name(std::move(n_process_name)), cpu_usage(n_cpu_usage), mem_usage(n_mem_usage),
      disk_usage(n_disk_usage), network_usage(n_network_usage) {}

void ConfigEntry::serialize(std::ostream &os) const {
	SerializableHelper::write_string(os, this->process_name);
	SerializableHelper::write_uint32(os, this->cpu_usage);
	SerializableHelper::write_uint64(os, this->mem_usage);
	SerializableHelper::write_uint64(os, this->disk_usage);
	SerializableHelper::write_uint64(os, this->network_usage);
}

void ConfigEntry::deserialize(std::istream &is) {
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

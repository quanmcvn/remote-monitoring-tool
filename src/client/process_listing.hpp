#ifndef CLIENT_PROCESS_LISTING_HPP
#define CLIENT_PROCESS_LISTING_HPP

#include <cstdint>
#include <string>
#include <vector>

class ProcessListing {
private:
	std::uint32_t pid;
	std::string process_name;
	std::vector<std::string> args;

public:
	ProcessListing();
	ProcessListing(std::uint32_t n_pid, std::string n_process_name,
	               std::vector<std::string> n_args);

	uint32_t get_pid() const;
	std::string get_process_name() const;
	std::vector<std::string> get_args() const;
	std::string get_args_as_string() const;
};

#endif
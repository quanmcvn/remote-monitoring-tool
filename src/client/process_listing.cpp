#include "client/process_listing.hpp"

ProcessListing::ProcessListing() {}

ProcessListing::ProcessListing(std::uint32_t n_pid, std::string n_process_name, std::vector<std::string> n_args) : 
pid(n_pid), 
process_name(std::move(n_process_name)),
args(std::move(n_args)) {
	
}

uint32_t ProcessListing::get_pid() const { return pid; }
std::string ProcessListing::get_process_name() const { return process_name; }
std::vector<std::string> ProcessListing::get_args() const { return args; }
std::string ProcessListing::get_args_as_string() const {
	std::string res = "";
	for (int i = 0; i < args.size(); ++ i) {
		res += args[i];
		if (i + 1 < args.size()) {
			res += " ";
		}
	}
	// if (res.back() == '\n') res.pop_back();
	return res;
}

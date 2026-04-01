#include "client/exponential_backoff.hpp"

#include <chrono>
#include <thread>
#include <iostream>

ExponentialBackoff::ExponentialBackoff() : start(1), stop(30), multipler(2) {
	current_delay = start;
}

ExponentialBackoff::ExponentialBackoff(int n_start, int n_stop, int n_multiplier)
    : start(n_start), stop(n_stop), multipler(n_multiplier) {
	current_delay = start;
}

void ExponentialBackoff::success() {
	std::cerr << "exponential backoff: success! delay is now " << start << "\n";
	current_delay = start;
}

void ExponentialBackoff::fail() {
	std::cerr << "exponential backoff: failed! %t waiting " << current_delay << "\n";
	std::this_thread::sleep_for(std::chrono::seconds(current_delay));
	current_delay = std::min(current_delay * multipler, stop);
	std::cerr << "exponential backoff: delay is now " << current_delay << "\n";
}
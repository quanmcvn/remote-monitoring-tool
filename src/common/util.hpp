#ifndef COMMON_UTIL_HPP
#define COMMON_UTIL_HPP

#include <chrono>
#include <cstdint>
#include <string>

// actually uint32 but we need error -1
int str_to_int(const std::string& str);

void print_progress(uint64_t total_received, uint64_t file_size,
                    std::chrono::steady_clock::time_point start_time);

std::uint64_t get_timestamp_ms();

// used in server to deduce last acked
std::string get_last_line_of_file(const std::string& filename);

// convert to string under assumption that it's only ascii
std::string to_string(const std::wstring& wstr);
// convert to string under assumption that it's only ascii
std::string to_string(const std::vector<wchar_t>& wstr);

// convert to wstring under assumption that it's only ascii
std::wstring to_wstring(const std::string& str);

#endif
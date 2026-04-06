#include "util.hpp"
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>

int str_to_int(const std::string& str) {
	int ret = 0;
	for (const char& c : str) {
		if ('0' <= c && c <= '9') {
			ret = ret * 10 + (c - '0');
		} else {
			return -1;
		}
	}
	return ret;
}

void print_progress(uint64_t total_received, uint64_t file_size,
                    std::chrono::steady_clock::time_point start_time) {

	double progress = (double)total_received / file_size;
	const int BAR_WIDTH = 50;
	int pos = BAR_WIDTH * progress;

	auto now = std::chrono::steady_clock::now();
	double seconds = std::chrono::duration<double>(now - start_time).count();

	double speed = (total_received / 1024.0 / 1024.0) / seconds;

	std::cout << "\r[";
	for (int i = 0; i < BAR_WIDTH; ++i) {
		if (i < pos)
			std::cout << "=";
		else if (i == pos)
			std::cout << ">";
		else
			std::cout << " ";
	}

	std::cout << "] " << std::fixed << std::setprecision(1) << (progress * 100.0) << "% " << "("
	          << total_received / 1024 / 1024 << " MB / " << file_size / 1024 / 1024 << " MB) "
	          << speed << " MB/s   ";

	std::cout.flush();
}

std::uint64_t get_timestamp_ms() {
	return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
	                                      std::chrono::system_clock::now().time_since_epoch())
	                                      .count());
}

std::string get_last_line_of_file(const std::string& filename) {
	std::ifstream file(filename, std::ios::binary);
	if (!file)
		return "";

	file.seekg(0, std::ios::end);
	std::streamoff pos = file.tellg();

	if (pos == 0)
		return "";

	char ch;

	// skip trailing newlines, me when i always cout << "\n"
	while (pos > 0) {
		file.seekg(--pos);
		file.get(ch);
		if (ch != '\n' && ch != '\r')
			break;
	}

	std::string reversed;

	while (pos >= 0) {
		file.seekg(pos);
		file.get(ch);

		if (ch == '\n')
			break;

		if (ch != '\r')
			reversed.push_back(ch);

		if (pos == 0)
			break;
		--pos;
	}

	std::reverse(reversed.begin(), reversed.end());
	return reversed;
}

std::string to_string(const std::wstring& wstr) { return std::string(wstr.begin(), wstr.end()); }

// convert to wstring under assumption that it's only ascii
std::wstring to_wstring(const std::string& str) { return std::wstring(str.begin(), str.end()); }

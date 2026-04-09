#include <gtest/gtest.h>

#include "client/client_logger.hpp"
#include "client/process_table.hpp"
#include "common/global.hpp"
#include "common/util.hpp"

class ClientTest : public ::testing::Test {
protected:
	std::filesystem::path temp_dir;

	void SetUp() override {
		temp_dir = std::filesystem::temp_directory_path() / "rmt_test";
		std::filesystem::create_directory(temp_dir);
		std::filesystem::current_path(temp_dir);
	}

	void TearDown() override { std::filesystem::remove_all(temp_dir); }
};

namespace {
	class FakeProcessTable : public ProcessTable {
	public:
		void update_table() override {
			this->list_table[1234] = ProcessListing(1234, "fake_prog_1", {});
			this->stat_table[1234] = ProcessStat(1 * CPU_UNIT, 12 * MB, DiskStat(1 * KB, 1 * KB), NetworkStat(0 * KB, 0 * KB));
			this->list_table[4231] = ProcessListing(4231, "fake_prog_2", {});
			this->stat_table[4231] = ProcessStat(20 * CPU_UNIT, 1 * MB, DiskStat(0 * KB, 0 * KB), NetworkStat(2 * KB, 2 * KB));
		}
	};
}

TEST_F(ClientTest, test_log_queue) {
	auto now_ms = get_timestamp_ms();
	LogEntry test_log_entry(1, "fake_prog_3", now_ms, LogType::CPU, 1000);
	{
		LogQueue log_queue("rmt-test-log.txt", "rmt-test-ack.txt");
		log_queue.add_log(test_log_entry);
		std::vector<LogEntry> log_entries = log_queue.get_batch_log(10);
		ASSERT_EQ(log_entries.size(), 1);
		ASSERT_EQ(test_log_entry, log_entries[0]);
		// log_queue goes out of scope, should write to file
	}
	{
		LogQueue log_queue("rmt-test-log.txt", "rmt-test-ack.txt");
		// doesn't add log
		// log_queue.add_log(test_log_entry);
		std::vector<LogEntry> log_entries = log_queue.get_batch_log(10);
		ASSERT_EQ(log_entries.size(), 1);
		ASSERT_EQ(test_log_entry, log_entries[0]);
	}
}

TEST_F(ClientTest, test_client_logger) {
	std::vector<ConfigEntry> config_entries;
	config_entries.emplace_back("fake_prog_1", 5, 2, 1, 1);
	config_entries.emplace_back("fake_prog_2", 5, 2, 1, 1);
	for (auto& config_entry : config_entries) {
		config_entry.convert_after_read();
	}
	Config config(config_entries);
	EventBus event_bus;
	ClientLogger logger(config, LogQueue("rmt-test-log.txt", "rmt-test-ack.txt"), std::ref(event_bus));
	FakeProcessTable fake_process_table;
	fake_process_table.update_table();
	logger.generate_log(fake_process_table);
	std::vector<LogEntry> logs = logger.get_batch_log(100);
	ASSERT_EQ(logs.size(), 4);
}

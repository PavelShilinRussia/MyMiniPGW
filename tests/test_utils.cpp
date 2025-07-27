#include <gtest/gtest.h>
#include "../src/Utils/utils.h"
#include "../src/Configs/pgw_client_config.h"
#include <vector>
#include <string>
#include <fstream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <nlohmann/json.hpp>

TEST(BCDTest, EncodeValidIMSI) {
    std::string imsi = "123456789";
    std::vector<uint8_t> expected = {0x21, 0x43, 0x65, 0x87, 0xF9};
    std::vector<uint8_t> result = encode_bcd(imsi);
    ASSERT_EQ(result, expected);
}

TEST(BCDTest, EncodeOddLengthIMSI) {
    std::string imsi = "12345678";
    std::vector<uint8_t> expected = {0x21, 0x43, 0x65, 0x87};
    std::vector<uint8_t> result = encode_bcd(imsi);
    ASSERT_EQ(result, expected);
}

TEST(BCDTest, EncodeEmptyIMSI) {
    std::string imsi = "";
    std::vector<uint8_t> expected = {};
    std::vector<uint8_t> result = encode_bcd(imsi);
    ASSERT_EQ(result, expected);
}

TEST(BCDTest, DecodeValidBCD) {
    std::vector<uint8_t> bcd = {0x21, 0x43, 0x65, 0x87, 0xF9};
    std::string expected = "123456789";
    std::string result = decode_bcd(bcd);
    ASSERT_EQ(result, expected);
}

TEST(BCDTest, DecodeEmptyBCD) {
    std::vector<uint8_t> bcd = {};
    std::string expected = "";
    std::string result = decode_bcd(bcd);
    ASSERT_EQ(result, expected);
}

TEST(BlacklistTest, CheckBlacklistedIMSI) {
    pgw_server_config config;
    config.blacklist = {"001010123456789", "001010000000001"};
    std::string imsi = "001010123456789";
    bool is_blacklisted = std::find(config.blacklist.begin(), config.blacklist.end(), imsi) != config.blacklist.end();
    ASSERT_TRUE(is_blacklisted);
}

TEST(BlacklistTest, CheckNonBlacklistedIMSI) {
    pgw_server_config config;
    config.blacklist = {"001010123456789", "001010000000001"};
    std::string imsi = "123456789";
    bool is_blacklisted = std::find(config.blacklist.begin(), config.blacklist.end(), imsi) != config.blacklist.end();
    ASSERT_FALSE(is_blacklisted);
}

TEST(LoggingTest, AppendToLogFile) {
    std::string log_file = "./test.log";
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, false);
    auto logger = std::make_shared<spdlog::logger>("test_logger", file_sink);
    spdlog::register_logger(logger);
    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::info);

    logger->info("Тестовое сообщение 1");
    logger->info("Тестовое сообщение 2");
    logger->flush();

    std::ifstream file(log_file);
    ASSERT_TRUE(file.is_open());
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();

    ASSERT_GE(lines.size(), 2);
    ASSERT_NE(lines[lines.size() - 2].find("Тестовое сообщение 1"), std::string::npos);
    ASSERT_NE(lines[lines.size() - 1].find("Тестовое сообщение 2"), std::string::npos);

    std::remove(log_file.c_str());
}

TEST(ClientConfigTest, LoadValidConfig) {
    std::string config_file = "./test_config.json";
    std::ofstream out(config_file);
    out << R"({
        "server_ip": "127.0.0.1",
        "server_port": 9009,
        "log_file": "./client.log",
        "log_level": "info"
    })";
    out.close();

    pgw_client_config config = load_pgw_client_config(config_file);
    ASSERT_EQ(config.server_ip, "127.0.0.1");
    ASSERT_EQ(config.server_port, 9009);
    ASSERT_EQ(config.log_file, "./client.log");
    ASSERT_EQ(config.log_level, "info");

    std::remove(config_file.c_str());
}

TEST(ClientConfigTest, LoadInvalidConfig) {
    std::string config_file = "./test_config.json";
    std::string log_file = "./test_error.log";
    
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true);
    auto logger = std::make_shared<spdlog::logger>("client_logger", file_sink);
    spdlog::register_logger(logger);
    logger->set_level(spdlog::level::err);
    logger->flush_on(spdlog::level::err);

    std::ofstream out(config_file);
    out << "{invalid_json}";
    out.close();

    pgw_client_config config = load_pgw_client_config(config_file);
    ASSERT_TRUE(config.server_ip.empty());
    ASSERT_EQ(config.server_port, 0);
    ASSERT_TRUE(config.log_file.empty());
    ASSERT_TRUE(config.log_level.empty());

    std::ifstream log_stream(log_file);
    ASSERT_TRUE(log_stream.is_open());
    std::string log_content((std::istreambuf_iterator<char>(log_stream)), std::istreambuf_iterator<char>());
    log_stream.close();
    ASSERT_NE(log_content.find("JSON parsing error"), std::string::npos);

    std::remove(config_file.c_str());
    std::remove(log_file.c_str());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
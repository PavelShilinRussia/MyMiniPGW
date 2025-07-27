#include "utils.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <regex>
#include <cctype>
#include "spdlog/spdlog.h"

using json = nlohmann::json;

bool is_valid_ip(const std::string& ip) {
    std::regex ip_regex("^(?:[0-9]{1,3}\\.){3}[0-9]{1,3}$");
    if (!std::regex_match(ip, ip_regex)) return false;
    std::istringstream iss(ip);
    std::string segment;
    while (std::getline(iss, segment, '.')) {
        int num = std::stoi(segment);
        if (num < 0 || num > 255) return false;
    }
    return true;
}

bool validate_pgw_client_config(const pgw_client_config& config) {
    if (!is_valid_ip(config.server_ip)) {
        std::cerr << "Invalid server IP: " << config.server_ip << std::endl;
        return false;
    }
    if (config.server_port == 0 || config.server_port > 65535) {
        std::cerr << "Invalid server port: " << config.server_port << std::endl;
        return false;
    }
    std::ofstream test_file(config.log_file, std::ios::app);
    if (!test_file.is_open()) {
        std::cerr << "Cannot open log file: " << config.log_file << std::endl;
        return false;
    }
    test_file.close();
    if (config.log_level.empty() || (config.log_level != "trace" && config.log_level != "debug" &&
                                    config.log_level != "info" && config.log_level != "warn" &&
                                    config.log_level != "err" && config.log_level != "critical")) {
        std::cerr << "Invalid log level: " << config.log_level << std::endl;
        return false;
    }
    return true;
}

bool validate_pgw_server_config(const pgw_server_config& config) {
    if (!is_valid_ip(config.udp_ip)) {
        std::cerr << "Invalid UDP IP: " << config.udp_ip << std::endl;
        return false;
    }
    if (config.udp_port == 0 || config.udp_port > 65535) {
        std::cerr << "Invalid UDP port: " << config.udp_port << std::endl;
        return false;
    }
    if (config.http_port == 0 || config.http_port > 65535) {
        std::cerr << "Invalid HTTP port: " << config.http_port << std::endl;
        return false;
    }
    std::ofstream test_file(config.log_file, std::ios::app);
    if (!test_file.is_open()) {
        std::cerr << "Cannot open log file: " << config.log_file << std::endl;
        return false;
    }
    test_file.close();
    std::ofstream cdr_file(config.cdr_file, std::ios::app);
    if (!cdr_file.is_open()) {
        std::cerr << "Cannot open CDR file: " << config.cdr_file << std::endl;
        return false;
    }
    cdr_file.close();
    if (config.log_level.empty() || (config.log_level != "trace" && config.log_level != "debug" &&
                                    config.log_level != "info" && config.log_level != "warn" &&
                                    config.log_level != "err" && config.log_level != "critical")) {
        std::cerr << "Invalid log level: " << config.log_level << std::endl;
        return false;
    }
    if (config.session_timeout_sec == 0) {
        std::cerr << "Invalid session timeout: " << config.session_timeout_sec << std::endl;
        return false;
    }
    if (config.graceful_shutdown_rate == 0) {
        std::cerr << "Invalid graceful shutdown rate: " << config.graceful_shutdown_rate << std::endl;
        return false;
    }
    return true;
}

std::vector<uint8_t> encode_bcd(const std::string& imsi) {
    std::vector<uint8_t> bcd;
    std::string padded_imsi = imsi;
    if (imsi.length() % 2 != 0) {
        padded_imsi += 'F';
    }
    for (size_t i = 0; i < padded_imsi.length(); i += 2) {
        uint8_t byte = 0;
        if (std::isdigit(padded_imsi[i])) {
            byte |= (padded_imsi[i] - '0') & 0x0F;
        } else if (padded_imsi[i] == 'F') {
            byte |= 0x0F;
        }
        if (i + 1 < padded_imsi.length()) {
            if (std::isdigit(padded_imsi[i + 1])) {
                byte |= ((padded_imsi[i + 1] - '0') << 4) & 0xF0;
            } else if (padded_imsi[i + 1] == 'F') {
                byte |= 0xF0;
            }
        }
        bcd.push_back(byte);
    }
    return bcd;
}

std::string decode_bcd(const std::vector<uint8_t>& bcd) {
    std::string imsi;
    for (uint8_t byte : bcd) {
        uint8_t low_nibble = byte & 0x0F;
        if (low_nibble != 0x0F) {
            imsi += std::to_string(low_nibble);
        }
        uint8_t high_nibble = (byte >> 4) & 0x0F;
        if (high_nibble != 0x0F) {
            imsi += std::to_string(high_nibble);
        }
    }
    return imsi;
}

pgw_server_config load_pgw_server_config(const std::string& config_path) {
    pgw_server_config config;
    std::ifstream file(config_path);
    if (!file.is_open()) {
        auto logger = spdlog::get("server_logger");
        if (logger) {
            logger->error("Cannot open config file: {}", config_path);
        } else {
            std::cerr << "Cannot open config file: " << config_path << std::endl;
        }
        return config;
    }
    try {
        json j;
        file >> j;
        config.udp_ip = j["udp_ip"].get<std::string>();
        config.udp_port = j["udp_port"].get<int>();
        config.session_timeout_sec = j["session_timeout_sec"].get<int>();
        config.cdr_file = j["cdr_file"].get<std::string>();
        config.http_port = j["http_port"].get<int>();
        config.graceful_shutdown_rate = j["graceful_shutdown_rate"].get<int>();
        config.log_file = j["log_file"].get<std::string>();
        config.log_level = j["log_level"].get<std::string>();
        config.blacklist = j["blacklist"].get<std::vector<std::string>>();
    } catch (const json::exception& e) {
        auto logger = spdlog::get("server_logger");
        if (logger) {
            logger->error("JSON parsing error in {}: {}", config_path, e.what());
        } else {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
        }
        config = pgw_server_config();
    }
    return config;
}

pgw_client_config load_pgw_client_config(const std::string& config_path) {
    pgw_client_config config;
    std::ifstream file(config_path);
    if (!file.is_open()) {
        auto logger = spdlog::get("client_logger");
        if (logger) {
            logger->error("Cannot open config file: {}", config_path);
        } else {
            std::cerr << "Cannot open config file: " << config_path << std::endl;
        }
        return config;
    }
    try {
        json j;
        file >> j;
        config.server_ip = j["server_ip"].get<std::string>();
        config.server_port = j["server_port"].get<int>();
        config.log_file = j["log_file"].get<std::string>();
        config.log_level = j["log_level"].get<std::string>();
    } catch (const json::exception& e) {
        auto logger = spdlog::get("client_logger");
        if (logger) {
            logger->error("JSON parsing error in {}: {}", config_path, e.what());
        } else {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
        }
        config = pgw_client_config();
    }
    return config;
}
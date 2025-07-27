#include <gtest/gtest.h>
#include "../src/Utils/utils.h"
#include "../src/Configs/pgw_client_config.h"
#include "../src/Configs/pgw_server_config.h"
#include <thread>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directory("logs");

        std::ofstream server_config_file("./server_config.json");
        server_config_file << R"({
            "udp_ip": "127.0.0.1",
            "udp_port": 9009,
            "session_timeout_sec": 10,
            "cdr_file": "logs/cdr.log",
            "http_port": 8080,
            "graceful_shutdown_rate": 10,
            "log_file": "logs/server.log",
            "log_level": "info",
            "blacklist": ["001010123456789"]
        })";
        server_config_file.close();

        std::ofstream client_config_file("./client_config.json");
        client_config_file << R"({
            "server_ip": "127.0.0.1",
            "server_port": 9009,
            "log_file": "logs/client.log",
            "log_level": "info"
        })";
        client_config_file.close();

        if (!std::filesystem::exists("../src/Server/server")) {
            std::cerr << "Server executable not found at ../src/Server/server" << std::endl;
            FAIL();
        }

        server_thread = std::thread([this]() {
            system("../src/Server/server ./server_config.json");
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void TearDown() override {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(8080);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
            std::string request = "GET /stop HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
            send(sock, request.c_str(), request.size(), 0);
            char buffer[1024];
            recv(sock, buffer, sizeof(buffer) - 1, 0);
            close(sock);
        } else {
            std::cerr << "Failed to connect to server for shutdown" << std::endl;
        }

        if (server_thread.joinable()) {
            server_thread.join();
        }

        std::remove("./server_config.json");
        std::remove("./client_config.json");
        std::filesystem::remove("logs/server.log");
        std::filesystem::remove("logs/client.log");
        std::filesystem::remove("logs/cdr.log");
        std::filesystem::remove("logs");
    }

    std::thread server_thread;
};

TEST_F(IntegrationTest, ClientServerInteraction) {
    if (!std::filesystem::exists("../src/Client/client")) {
        std::cerr << "Client executable not found at ../src/Client/client" << std::endl;
        FAIL();
    }

    int result = system("../src/Client/client ./client_config.json 123456789012345");
    ASSERT_EQ(result, 0);

    std::ifstream server_log("logs/server.log");
    std::string line;
    bool found_imsi = false;
    while (std::getline(server_log, line)) {
        if (line.find("Получен IMSI: 123456789012345") != std::string::npos) {
            found_imsi = true;
            break;
        }
    }
    server_log.close();
    ASSERT_TRUE(found_imsi);

    std::ifstream cdr_file("logs/cdr.log");
    bool found_cdr = false;
    while (std::getline(cdr_file, line)) {
        if (line.find("123456789012345, created") != std::string::npos) {
            found_cdr = true;
            break;
        }
    }
    cdr_file.close();
    ASSERT_TRUE(found_cdr);
}

TEST_F(IntegrationTest, BlacklistedIMSI) {
    if (!std::filesystem::exists("../src/Client/client")) {
        std::cerr << "Client executable not found at ../src/Client/client" << std::endl;
        FAIL();
    }

    int result = system("../src/Client/client ./client_config.json 001010123456789");
    ASSERT_EQ(result, 0);

    std::ifstream server_log("logs/server.log");
    std::string line;
    bool found_blacklist = false;
    while (std::getline(server_log, line)) {
        if (line.find("IMSI 001010123456789 в черном списке") != std::string::npos) {
            found_blacklist = true;
            break;
        }
    }
    server_log.close();
    ASSERT_TRUE(found_blacklist);

    std::ifstream cdr_file("logs/cdr.log");
    bool found_cdr = false;
    while (std::getline(cdr_file, line)) {
        if (line.find("001010123456789, rejected") != std::string::npos) {
            found_cdr = true;
            break;
        }
    }
    cdr_file.close();
    ASSERT_TRUE(found_cdr);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
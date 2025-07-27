#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include "../Utils/utils.h"
#include "../Configs/pgw_client_config.h"
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

#define BUFFER_SIZE 1024

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <config.json> <IMSI>" << std::endl;
        return 1;
    }
    std::string config_file = argv[1];
    std::string imsi = argv[2];

    auto config = load_pgw_client_config(config_file);
    if (!validate_pgw_client_config(config)) {
        std::cerr << "Invalid client configuration" << std::endl;
        return 1;
    }

    std::ofstream test_file(config.log_file, std::ios::app);
    if (!test_file.is_open()) {
        std::cerr << "Не удалось открыть лог-файл: " << config.log_file << std::endl;
        return 1;
    }
    test_file.close();

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.log_file, false);
    auto logger = std::make_shared<spdlog::logger>("client_logger", file_sink);
    spdlog::register_logger(logger);
    logger->set_level(spdlog::level::from_str(config.log_level));
    logger->flush_on(spdlog::level::info);
    logger->info("Клиент запущен с IMSI: {}", imsi);

    std::vector<uint8_t> bcd_imsi = encode_bcd(imsi);
    logger->info("IMSI закодирован в BCD, длина: {}", bcd_imsi.size());

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        logger->error("Ошибка создания сокета: {}", strerror(errno));
        std::cerr << "Ошибка создания сокета" << std::endl;
        return 1;
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, config.server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        logger->error("Неверный IP-адрес сервера: {}", config.server_ip);
        std::cerr << "Неверный IP-адрес сервера" << std::endl;
        close(sockfd);
        return 1;
    }
    server_addr.sin_port = htons(config.server_port);

    logger->info("Отправка BCD-IMSI на {}:{}", config.server_ip, config.server_port);
    if (sendto(sockfd, bcd_imsi.data(), bcd_imsi.size(), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        logger->error("Ошибка отправки пакета: {}", strerror(errno));
        std::cerr << "Ошибка отправки пакета" << std::endl;
        close(sockfd);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    struct sockaddr_in recv_addr;
    socklen_t addr_len = sizeof(recv_addr);
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);
    struct timeval tv = {5, 0};
    int ret = select(sockfd + 1, &read_fds, nullptr, nullptr, &tv);
    if (ret < 0) {
        logger->error("Ошибка select: {}", strerror(errno));
        std::cerr << "Ошибка select" << std::endl;
        close(sockfd);
        return 1;
    }
    if (ret == 0) {
        logger->error("Таймаут получения ответа");
        std::cerr << "Таймаут получения ответа" << std::endl;
        close(sockfd);
        return 1;
    }
    if (FD_ISSET(sockfd, &read_fds)) {
        int bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)&recv_addr, &addr_len);
        if (bytes_received < 0) {
            logger->error("Ошибка получения ответа: {}", strerror(errno));
            std::cerr << "Ошибка получения ответа" << std::endl;
        } else {
            buffer[bytes_received] = '\0';
            logger->info("Получен ответ: {}", buffer);
            std::cout << "Ответ от сервера: " << buffer << std::endl;
        }
    }

    close(sockfd);
    logger->flush();
    return 0;
}
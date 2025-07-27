#include <fstream>
#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <fstream>
#include "../Utils/utils.h"
#include <nlohmann/json.hpp>
#include "spdlog/spdlog.h"
#include <httplib.h>
#include "spdlog/sinks/basic_file_sink.h"

#define BUFFER_SIZE 1024
#define NUM_THREADS 4

struct Packet {
    char data[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len;
    int bytes_received;
};

struct Session {
    time_t start_time;
    bool active;
    Session() : start_time(time(nullptr)), active(true) {}
};

std::queue<Packet> packet_queue;
std::mutex queue_mutex, session_mutex;
std::condition_variable cv;
std::map<std::string, Session> sessions;
std::atomic<bool> shutdown_flag(false);
std::shared_ptr<spdlog::logger> logger;

void worker_thread(int sockfd, const pgw_server_config& config) {
    while (!shutdown_flag) {
        Packet packet;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait(lock, [] { return !packet_queue.empty() || shutdown_flag; });
            if (shutdown_flag && packet_queue.empty()) break;
            if (!packet_queue.empty()) {
                packet = packet_queue.front();
                packet_queue.pop();
            } else continue;
        }

        std::vector<uint8_t> bcd_imsi(packet.data, packet.data + packet.bytes_received);
        std::string imsi = decode_bcd(bcd_imsi);
        logger->info("Получен IMSI: {} от {}", imsi, inet_ntoa(packet.client_addr.sin_addr));

        bool is_blacklisted = std::find(config.blacklist.begin(), config.blacklist.end(), imsi) != config.blacklist.end();
        const char* response = is_blacklisted ? "rejected" : "created";

        {
            std::lock_guard<std::mutex> lock(session_mutex);
            if (!is_blacklisted && sessions.find(imsi) == sessions.end()) {
                sessions[imsi] = Session();
                logger->info("Сессия создана для IMSI: {}", imsi);
            } else if (is_blacklisted) {
                logger->warn("IMSI {} в черном списке", imsi);
            }
        }

        {
            std::ofstream cdr_file(config.cdr_file, std::ios::app);
            if (cdr_file.is_open()) {
                cdr_file << imsi << ", " << (is_blacklisted ? "rejected" : "created") << "\n";
                cdr_file.flush();
            } else {
                logger->error("Не удалось открыть CDR-файл: {}", config.cdr_file);
            }
        }

        sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&packet.client_addr, packet.client_len);
    }
    logger->info("Рабочий поток завершён");
}

void session_timeout_thread(const pgw_server_config& config) {
    while (!shutdown_flag) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Уменьшено с 5 до 1 секунды
        std::lock_guard<std::mutex> lock(session_mutex);
        auto now = time(nullptr);
        for (auto it = sessions.begin(); it != sessions.end();) {
            if (difftime(now, it->second.start_time) > config.session_timeout_sec && it->second.active) {
                std::ofstream cdr_file(config.cdr_file, std::ios::app);
                if (cdr_file.is_open()) {
                    cdr_file << it->first << ", timeout\n";
                    cdr_file.flush();
                } else {
                    logger->error("Не удалось открыть CDR-файл: {}", config.cdr_file);
                }
                logger->info("Сессия для IMSI {} удалена по тайм-ауту", it->first);
                it = sessions.erase(it);
            } else {
                ++it;
            }
        }
    }
    logger->info("Поток тайм-аута сессий завершён");
}

void http_server(const pgw_server_config& config) {
    httplib::Server svr;
    svr.Get("/check_subscriber", [&](const httplib::Request& req, httplib::Response& res) {
        std::string imsi = req.get_param_value("imsi");
        if (imsi.empty()) {
            logger->error("HTTP /check_subscriber: отсутствует параметр IMSI");
            res.set_content("Ошибка: IMSI не указан", "text/plain");
            res.status = 400;
            return;
        }
        logger->info("HTTP /check_subscriber: запрос для IMSI {}", imsi);
        std::lock_guard<std::mutex> lock(session_mutex);
        res.set_content(sessions.find(imsi) != sessions.end() && sessions[imsi].active ? "active" : "not active", "text/plain");
    });

    svr.Get("/stop", [&](const httplib::Request& req, httplib::Response& res) {
        logger->info("HTTP /stop: Запрос на завершение сервера");
        shutdown_flag = true;
        cv.notify_all();
        res.set_content("Shutting down...", "text/plain");
        res.status = 200;

        auto start = std::chrono::steady_clock::now();
        while (!sessions.empty() && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < 30) {
            std::lock_guard<std::mutex> lock(session_mutex);
            size_t to_remove = std::min(static_cast<size_t>(config.graceful_shutdown_rate), sessions.size());
            auto it = sessions.begin();
            for (size_t i = 0; i < to_remove && it != sessions.end(); ++i) {
                std::ofstream cdr_file(config.cdr_file, std::ios::app);
                if (cdr_file.is_open()) {
                    cdr_file << it->first << ", shutdown\n";
                    cdr_file.flush();
                } else {
                    logger->error("Не удалось открыть CDR-файл: {}", config.cdr_file);
                }
                logger->info("Сессия для IMSI {} удалена при завершении", it->first);
                it = sessions.erase(it);
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        sessions.clear();
        svr.stop();
        logger->info("HTTP-сервер остановлен");
    });

    logger->info("Запуск HTTP-сервера на 0.0.0.0:{}", config.http_port);
    if (!svr.listen("0.0.0.0", config.http_port)) {
        logger->error("Не удалось запустить HTTP-сервер на порту {}", config.http_port);
        std::cerr << "Ошибка запуска HTTP-сервера на порту " << config.http_port << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config.json>" << std::endl;
        return 1;
    }
    std::string config_file = argv[1];

    auto config = load_pgw_server_config(config_file);
    if (!validate_pgw_server_config(config)) {
        std::cerr << "Invalid server configuration" << std::endl;
        return 1;
    }

    std::ofstream test_file(config.log_file, std::ios::app);
    if (!test_file.is_open()) {
        std::cerr << "Не удалось открыть лог-файл: " << config.log_file << std::endl;
        return 1;
    }
    test_file.close();

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.log_file, false);
    logger = std::make_shared<spdlog::logger>("server_logger", file_sink);
    spdlog::register_logger(logger);
    logger->set_level(spdlog::level::from_str(config.log_level));
    logger->flush_on(spdlog::level::info);
    logger->info("Сервер запущен");

    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        logger->error("Ошибка создания сокета");
        std::cerr << "Ошибка создания сокета" << std::endl;
        return 1;
    }

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, config.udp_ip.c_str(), &server_addr.sin_addr) <= 0) {
        logger->error("Неверный IP-адрес из конфига: {}", config.udp_ip);
        std::cerr << "Неверный IP-адрес из конфига: " << config.udp_ip << std::endl;
        close(sockfd);
        return 1;
    }
    server_addr.sin_port = htons(config.udp_port);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        logger->error("Ошибка привязки сокета к порту {}", config.udp_port);
        std::cerr << "Ошибка привязки" << std::endl;
        close(sockfd);
        return 1;
    }

    std::cout << "UDP-сервер запущен на " << config.udp_ip << ":" << config.udp_port << "..." << std::endl;

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(worker_thread, sockfd, config);
    }

    std::thread timeout_thread(session_timeout_thread, config);
    std::thread http_thread(http_server, config);

    while (!shutdown_flag) {
        Packet packet;
        packet.client_len = sizeof(client_addr);
        packet.bytes_received = recvfrom(sockfd, packet.data, BUFFER_SIZE - 1, 0,
                                        (struct sockaddr*)&packet.client_addr, &packet.client_len);
        if (packet.bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            logger->error("Ошибка приема данных");
            std::cerr << "Ошибка приема данных" << std::endl;
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            packet_queue.push(packet);
        }
        cv.notify_one();
    }

    logger->info("Основной цикл завершён, ожидание завершения потоков");

    for (auto& thread : threads) {
        if (thread.joinable()) thread.join();
    }
    if (timeout_thread.joinable()) timeout_thread.join();
    if (http_thread.joinable()) http_thread.join();

    close(sockfd);
    logger->info("Сервер завершил работу");
    logger->flush();
    return 0;
}
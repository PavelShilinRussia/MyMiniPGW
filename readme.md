# Мини-PGW — Выпускная работа C++ школы

## Описание проекта

Проект представляет собой упрощённую модель сетевого компонента Packet Gateway (PGW), разработанную на языке C++ для обработки UDP-запросов от абонентов, управления сессиями и предоставления REST API.

## Основные компоненты

### pgw_server

Основное серверное приложение, реализующее:
- Приём UDP пакетов, содержащих IMSI в BCD-кодировке.
- Создание и управление сессиями.
- Поддержку чёрного списка IMSI.
- Запись событий в CDR-файл.
- Завершение сессий по таймеру.
- HTTP API:
  - `/check_subscriber?imsi=...` — проверка активной сессии.
  - `/stop` — завершение работы с graceful offload.
- Конфигурация из JSON.
- Логирование действий.

#### Пример запуска:
```bash
./pgw_server ./config/server_config.json
```

---

### pgw_client

Клиентское приложение для имитации абонентского запроса:
- Загрузка конфигурации из JSON.
- Отправка UDP-запроса с IMSI.
- Получение ответа (`created` / `rejected`).
- Логирование всех этапов.

#### Пример запуска:
```bash
./pgw_client ./config/client_config.json 001010123456789
```

---

## Формат конфигурации

### server_config.json
```json
{
  "udp_ip": "0.0.0.0",
  "udp_port": 9000,
  "session_timeout_sec": 30,
  "cdr_file": "cdr.log",
  "http_port": 8080,
  "graceful_shutdown_rate": 10,
  "log_file": "pgw.log",
  "log_level": "INFO",
  "blacklist": [
    "001010123456789",
    "001010000000001"
  ]
}
```

### client_config.json
```json
{
  "server_ip": "127.0.0.1",
  "server_port": 9000,
  "log_file": "client.log",
  "log_level": "INFO"
}
```

---

## Сборка проекта

```bash
git clone <repo_url>
cd mini-pgw
mkdir build && cd build
cmake ..
make
```

---

## Требования

- C++17 или новее
- CMake
- Linux
- Используемые библиотеки:
  - `nlohmann::json`
  - `spdlog`
  - `httplib`
  - `gtest` (для unit-тестов)

---

## Unit-тесты

Для запуска модульных тестов:
```bash
cd build
ctest
```

---

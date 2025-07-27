cmake_minimum_required(VERSION 3.10)
project(PGW_Client)

add_executable(client pgw_client.cpp ../Utils/utils.cpp)

target_link_libraries(client PRIVATE nlohmann_json::nlohmann_json spdlog::spdlog)

target_include_directories(client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Configs)
target_include_directories(client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Utils)
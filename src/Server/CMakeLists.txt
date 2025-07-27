cmake_minimum_required(VERSION 3.10)
project(PGW_Server)

add_executable(server server.cpp ../Utils/utils.cpp)

target_link_libraries(server PRIVATE nlohmann_json::nlohmann_json spdlog::spdlog)

include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib
    GIT_TAG v0.12.2
)
FetchContent_MakeAvailable(httplib)
target_link_libraries(server PRIVATE httplib::httplib)

target_include_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Configs)
target_include_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../Utils)
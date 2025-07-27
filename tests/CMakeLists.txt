cmake_minimum_required(VERSION 3.10)

# Find Threads
find_package(Threads REQUIRED)

# Unit tests target
add_executable(test_utils
    test_utils.cpp
    ../src/Utils/utils.cpp
)
target_include_directories(test_utils PRIVATE
    ../src/Configs
    ../src/Utils
    ${CMAKE_BINARY_DIR}/_deps/googletest-src/googletest/include
)
target_link_libraries(test_utils PRIVATE
    gtest
    gtest_main
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Threads::Threads
)
add_test(NAME UtilsTest COMMAND test_utils)

# Integration tests target
add_executable(test_integration
    test_integration.cpp
    ../src/Utils/utils.cpp
)
target_include_directories(test_integration PRIVATE
    ../src/Configs
    ../src/Utils
    ${CMAKE_BINARY_DIR}/_deps/googletest-src/googletest/include
)
target_link_libraries(test_integration PRIVATE
    gtest
    gtest_main
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Threads::Threads
)
add_test(NAME IntegrationTest COMMAND test_integration)
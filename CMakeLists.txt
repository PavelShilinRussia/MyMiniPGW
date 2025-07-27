cmake_minimum_required(VERSION 3.28)
project(PGW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.3
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG v1.14.1
)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib
    GIT_TAG v0.12.2
)

set(BUILD_GTEST ON CACHE BOOL "Builds the googletest subproject")
set(BUILD_GMOCK OFF CACHE BOOL "Builds the googlemock subproject")
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG v1.14.0
)

FetchContent_MakeAvailable(json spdlog httplib googletest)

enable_testing()

add_subdirectory(src/Server)
add_subdirectory(src/Client)
add_subdirectory(tests)
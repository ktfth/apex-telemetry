cmake_minimum_required(VERSION 3.25)
project(apex_api_gateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(simdjson REQUIRED)
find_package(Drogon REQUIRED)

add_executable(apex_api_gateway
    src/main.cpp
)

target_link_libraries(apex_api_gateway PRIVATE
    spdlog::spdlog
    fmt::fmt
    simdjson::simdjson
    Drogon::Drogon
)

target_compile_options(apex_api_gateway PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion
)

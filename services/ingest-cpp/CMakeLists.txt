cmake_minimum_required(VERSION 3.25)
project(apex_ingest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(simdjson REQUIRED)
find_package(CURL REQUIRED)

add_executable(apex_ingest
    src/main.cpp
)

target_link_libraries(apex_ingest PRIVATE
    spdlog::spdlog
    fmt::fmt
    simdjson::simdjson
    CURL::libcurl
)

target_compile_options(apex_ingest PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion
)

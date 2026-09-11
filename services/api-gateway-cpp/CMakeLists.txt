cmake_minimum_required(VERSION 3.25)
project(apex_api_gateway LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(apex_api_gateway
    src/main.cpp
    src/http_server.cpp
    ../analytics-cpp/src/spatial_alignment.cpp
    ../common/simdjson/simdjson.cpp
)

target_include_directories(apex_api_gateway PRIVATE
    include
    ../analytics-cpp/include
    ../common/simdjson
)

target_compile_options(apex_api_gateway PRIVATE
    -Wall -Wextra -Wpedantic
)

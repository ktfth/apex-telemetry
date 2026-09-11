cmake_minimum_required(VERSION 3.25)
project(apex_ingest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)

add_executable(apex_ingest
    src/main.cpp
    src/http_client.cpp
    src/openf1_client.cpp
    src/storage.cpp
    ../common/simdjson/simdjson.cpp
)

target_include_directories(apex_ingest PRIVATE
    include
    ../common/simdjson
)

target_link_libraries(apex_ingest PRIVATE
    CURL::libcurl
)

target_compile_options(apex_ingest PRIVATE
    -Wall -Wextra -Wpedantic
)

cmake_minimum_required(VERSION 3.25)
project(apex_ingest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(Threads REQUIRED)

add_executable(apex_ingest
    src/main.cpp
    src/raw_archive.cpp
    src/warehouse.cpp
    ../common/http/http_client.cpp
    ../common/openf1/openf1_client.cpp
    ../common/simdjson/simdjson.cpp
)

target_include_directories(apex_ingest PRIVATE
    include
    ../common
    ../common/simdjson
)

target_link_libraries(apex_ingest PRIVATE
    CURL::libcurl
    PostgreSQL::PostgreSQL
    Threads::Threads
)

target_compile_options(apex_ingest PRIVATE -Wall -Wextra -Wpedantic)

if(BUILD_TESTING)
    add_executable(apex_ingest_test
        tests/ingest_test.cpp
        src/raw_archive.cpp
        ../common/openf1/openf1_client.cpp
        ../common/http/http_client.cpp
        ../common/simdjson/simdjson.cpp
    )
    target_include_directories(apex_ingest_test PRIVATE
        include
        ../common
        ../common/simdjson
    )
    target_link_libraries(apex_ingest_test PRIVATE CURL::libcurl Threads::Threads)
    target_compile_options(apex_ingest_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME ingest-cpp COMMAND apex_ingest_test)
endif()

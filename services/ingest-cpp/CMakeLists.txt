cmake_minimum_required(VERSION 3.25)
project(apex_ingest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)
find_package(PostgreSQL REQUIRED)

add_executable(apex_ingest
    src/main.cpp
    src/http_client.cpp
    src/openf1_client.cpp
    src/storage.cpp
    src/postgres_storage.cpp
    ../common/simdjson/simdjson.cpp
)

target_include_directories(apex_ingest PRIVATE
    include
    ../common/simdjson
)

target_link_libraries(apex_ingest PRIVATE
    CURL::libcurl
    PostgreSQL::PostgreSQL
)

target_compile_options(apex_ingest PRIVATE
    -Wall -Wextra -Wpedantic
)

if(BUILD_TESTING)
    add_executable(apex_ingest_test
        tests/ingest_test.cpp
        src/storage.cpp
        ../common/simdjson/simdjson.cpp
    )

    target_include_directories(apex_ingest_test PRIVATE
        include
        ../common/simdjson
    )

    target_compile_options(apex_ingest_test PRIVATE
        -Wall -Wextra -Wpedantic
    )

    add_test(NAME ingest-cpp COMMAND apex_ingest_test)
endif()

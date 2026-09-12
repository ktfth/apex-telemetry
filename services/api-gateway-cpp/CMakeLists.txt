cmake_minimum_required(VERSION 3.25)
project(apex_api_gateway LANGUAGES CXX)

find_package(PostgreSQL REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(apex_api_gateway
    src/main.cpp
    src/http_server.cpp
    src/database_repository.cpp
    src/telemetry_resolver.cpp
    src/strategy_client.cpp
    ../analytics-cpp/src/spatial_alignment.cpp
    ../common/http/http_client.cpp
    ../common/openf1/openf1_client.cpp
    ../common/simdjson/simdjson.cpp
)

target_link_libraries(apex_api_gateway PRIVATE
    PostgreSQL::PostgreSQL
    CURL::libcurl
    Threads::Threads
)

target_include_directories(apex_api_gateway PRIVATE
    include
    ../analytics-cpp/include
    ../common
    ../common/simdjson
)

target_compile_options(apex_api_gateway PRIVATE -Wall -Wextra -Wpedantic)

if(BUILD_TESTING)
    add_executable(apex_api_gateway_test
        tests/http_server_test.cpp
        src/http_server.cpp
    )
    target_include_directories(apex_api_gateway_test PRIVATE include)
    target_link_libraries(apex_api_gateway_test PRIVATE Threads::Threads)
    target_compile_options(apex_api_gateway_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME api-gateway-cpp COMMAND apex_api_gateway_test)
endif()

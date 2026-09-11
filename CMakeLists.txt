cmake_minimum_required(VERSION 3.25)
project(apex_telemetry_services LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(services/api-gateway-cpp)
add_subdirectory(services/ingest-cpp)
add_subdirectory(services/analytics-cpp)

cmake_minimum_required(VERSION 3.25)
project(apex_telemetry_services LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# O motor de alinhamento é biblioteca: precisa existir antes do gateway que o liga.
add_subdirectory(services/analytics-cpp)
add_subdirectory(services/api-gateway-cpp)
add_subdirectory(services/ingest-cpp)

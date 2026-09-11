cmake_minimum_required(VERSION 3.25)
project(apex_analytics LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(apex_analytics
    src/main.cpp
    src/spatial_alignment.cpp
    ../common/simdjson/simdjson.cpp
)

target_include_directories(apex_analytics PRIVATE
    include
    ../common/simdjson
)

target_compile_options(apex_analytics PRIVATE
    -Wall -Wextra -Wpedantic
)

if(BUILD_TESTING)
    add_executable(apex_analytics_test
        tests/analytics_test.cpp
        src/spatial_alignment.cpp
        ../common/simdjson/simdjson.cpp
    )

    target_include_directories(apex_analytics_test PRIVATE
        include
        ../common/simdjson
    )

    target_compile_options(apex_analytics_test PRIVATE
        -Wall -Wextra -Wpedantic
    )

    add_test(NAME analytics-cpp COMMAND apex_analytics_test)
endif()

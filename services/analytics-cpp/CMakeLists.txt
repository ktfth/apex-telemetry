cmake_minimum_required(VERSION 3.25)
project(apex_analytics LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# O motor de alinhamento espacial é uma biblioteca de cálculo puro, sem I/O: quem
# a consome é o gateway. Não existe um "serviço de analytics" — expor um binário
# que apenas imprime a própria versão sugeriria um processo que não existe.
add_library(apex_analytics STATIC
    src/spatial_alignment.cpp
)

target_include_directories(apex_analytics PUBLIC include)
target_compile_options(apex_analytics PRIVATE -Wall -Wextra -Wpedantic)

if(BUILD_TESTING)
    add_executable(apex_analytics_test tests/analytics_test.cpp)
    target_link_libraries(apex_analytics_test PRIVATE apex_analytics)
    target_compile_options(apex_analytics_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME analytics-cpp COMMAND apex_analytics_test)
endif()

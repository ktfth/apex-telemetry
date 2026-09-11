#include <iostream>
#include <string>
#include <string_view>
#include <chrono>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << R"({"service": "apex-api-gateway", "version": "1.0.0", "status": "initialized", "cpp_standard": 202302L})" << std::endl;
    return 0;
}

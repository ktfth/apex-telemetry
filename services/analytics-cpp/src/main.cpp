#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << R"({"service": "apex-analytics", "version": "1.0.0", "grid_step_m": 5.0, "status": "initialized"})" << std::endl;
    return 0;
}

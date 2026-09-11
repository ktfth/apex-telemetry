#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << R"({"service": "apex-ingest", "version": "1.0.0", "source": "openf1", "status": "initialized"})" << std::endl;
    return 0;
}

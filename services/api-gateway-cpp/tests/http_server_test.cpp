#include "http_server.hpp"

#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
std::string get(const std::string& target) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(18081);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    assert(connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    const std::string request = "GET " + target + " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    assert(write(fd, request.data(), request.size()) == static_cast<ssize_t>(request.size()));
    std::string response;
    char buffer[1024];
    ssize_t count;
    while ((count = read(fd, buffer, sizeof(buffer))) > 0) response.append(buffer, static_cast<size_t>(count));
    close(fd);
    return response;
}
}

int main() {
    apex::gateway::HttpServer server(18081);
    server.route("GET", "/api/items", [](const apex::gateway::HttpRequest&) {
        return apex::gateway::HttpResponse{200, "application/json", "{\"route\":\"collection\"}", {{"X-Apex-Data-Source", "test"}}};
    });
    server.route("GET", "/api/items/:item_id", [](const apex::gateway::HttpRequest& request) {
        const auto query = request.query_params.find("mode");
        const std::string mode = query == request.query_params.end() ? "" : query->second;
        return apex::gateway::HttpResponse{200, "application/json", request.path_params.at("item_id") + ":" + mode, {}};
    });
    server.start(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    const auto collection = get("/api/items");
    assert(collection.find("{\"route\":\"collection\"}") != std::string::npos);
    assert(collection.find("X-Apex-Data-Source: test") != std::string::npos);
    const auto item = get("/api/items/42?mode=full%20audit");
    assert(item.find("42:full audit") != std::string::npos);
    const auto missing = get("/api/items/42/extra");
    assert(missing.find("404") != std::string::npos);

    server.stop();
    return 0;
}

#include "http_server.hpp"

#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <iostream>

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

std::string read_sse_events(const std::string& target, int max_events = 2) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(18081);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    assert(connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    const std::string request = "GET " + target + " HTTP/1.1\r\nHost: localhost\r\nAccept: text/event-stream\r\n\r\n";
    assert(write(fd, request.data(), request.size()) == static_cast<ssize_t>(request.size()));
    
    std::string response;
    char buffer[512];
    int events_seen = 0;
    while (events_seen < max_events) {
        ssize_t count = read(fd, buffer, sizeof(buffer) - 1);
        if (count <= 0) break;
        buffer[count] = '\0';
        response.append(buffer, static_cast<size_t>(count));
        if (response.find("data:") != std::string::npos) {
            events_seen++;
        }
    }
    close(fd);
    return response;
}
}

int main() {
    apex::gateway::HttpServer server(18081);
    
    // REST Routes
    server.route("GET", "/api/items", [](const apex::gateway::HttpRequest&) {
        return apex::gateway::HttpResponse{200, "application/json", "{\"route\":\"collection\"}", {{"X-Apex-Data-Source", "test"}}};
    });
    server.route("GET", "/api/items/:item_id", [](const apex::gateway::HttpRequest& request) {
        const auto query = request.query_params.find("mode");
        const std::string mode = query == request.query_params.end() ? "" : query->second;
        return apex::gateway::HttpResponse{200, "application/json", request.path_params.at("item_id") + ":" + mode, {}};
    });

    // SSE Route
    server.route_sse("/api/v1/sessions/:session_key/stream", [](const apex::gateway::HttpRequest& req, const apex::gateway::SseWriter& writer, const std::atomic<bool>& running) {
        writer.send(R"({"status":"ready","session":)" + req.path_params.at("session_key") + R"(})", "init", "1");
        int count = 0;
        while (running && writer.connected() && count < 3) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            writer.send(R"({"tick":)" + std::to_string(count) + R"(})", "tick", std::to_string(count + 2));
            count++;
        }
    });

    server.start(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Test REST
    const auto collection = get("/api/items");
    assert(collection.find("{\"route\":\"collection\"}") != std::string::npos);
    assert(collection.find("X-Apex-Data-Source: test") != std::string::npos);
    
    const auto item = get("/api/items/42?mode=full%20audit");
    assert(item.find("42:full audit") != std::string::npos);
    
    const auto missing = get("/api/items/42/extra");
    assert(missing.find("404") != std::string::npos);

    // Test SSE
    const auto sse_output = read_sse_events("/api/v1/sessions/9472/stream", 2);
    assert(sse_output.find("Content-Type: text/event-stream") != std::string::npos);
    assert(sse_output.find("event: init") != std::string::npos);
    assert(sse_output.find("\"session\":9472") != std::string::npos);

    server.stop();
    std::cout << "[PASS] All HTTP and SSE server tests completed successfully.\n";
    return 0;
}

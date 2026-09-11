#pragma once

#include <string>
#include <functional>
#include <map>
#include <atomic>
#include <thread>
#include <vector>

namespace apex::gateway {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::map<std::string, std::string> path_params;
    std::map<std::string, std::string> query_params;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status_code{200};
    std::string content_type{"application/json"};
    std::string body{"{}"};
    std::map<std::string, std::string> headers;
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

/// SSE writer: sends individual SSE frames to the connected client.
/// Returns false if the write failed (client disconnected).
class SseWriter {
public:
    explicit SseWriter(int fd) : fd_(fd) {}

    /// Send an SSE event. Fields follow the spec: event (optional), data, id (optional).
    bool send(const std::string& data, const std::string& event = "", const std::string& id = "") const;

    /// Check if the client is still connected (non-blocking).
    bool connected() const;

    int fd() const { return fd_; }

private:
    int fd_;
};

/// Handler that receives a long-lived SSE connection.
/// The handler is called in a dedicated thread and should loop, writing events
/// via the SseWriter until it returns false or the server is stopping.
using SseHandler = std::function<void(const HttpRequest&, const SseWriter&, const std::atomic<bool>& running)>;

class HttpServer {
public:
    explicit HttpServer(int port = 8080);
    ~HttpServer();

    void route(const std::string& method, const std::string& path_prefix, HttpHandler handler);
    void route_sse(const std::string& path_prefix, SseHandler handler);
    void start(bool block = false);
    void stop();

    int port() const { return port_; }

private:
    struct Route {
        std::string method;
        std::string pattern;
        HttpHandler handler;
    };
    struct SseRoute {
        std::string pattern;
        SseHandler handler;
    };
    int port_;
    int server_fd_{-1};
    std::atomic<bool> running_{false};
    std::vector<std::thread> worker_threads_;
    std::vector<Route> routes_;
    std::vector<SseRoute> sse_routes_;

    void handle_client(int client_fd);
};

} // namespace apex::gateway

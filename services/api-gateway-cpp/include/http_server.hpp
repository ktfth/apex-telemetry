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

class HttpServer {
public:
    explicit HttpServer(int port = 8080);
    ~HttpServer();

    void route(const std::string& method, const std::string& path_prefix, HttpHandler handler);
    void start(bool block = false);
    void stop();

    int port() const { return port_; }

private:
    int port_;
    int server_fd_{-1};
    std::atomic<bool> running_{false};
    std::vector<std::thread> worker_threads_;
    std::map<std::string, HttpHandler> routes_;

    void handle_client(int client_fd);
};

} // namespace apex::gateway

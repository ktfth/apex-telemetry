#include "http_server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <cstring>

namespace apex::gateway {

HttpServer::HttpServer(int port) : port_(port) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::route(const std::string& method, const std::string& path_prefix, HttpHandler handler) {
    routes_[method + ":" + path_prefix] = std::move(handler);
}

void HttpServer::start(bool block) {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "Failed to create server socket\n";
        return;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(port_));

    if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "Failed to bind to port " << port_ << "\n";
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    if (listen(server_fd_, 64) < 0) {
        std::cerr << "Failed to listen on server socket\n";
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    running_ = true;
    std::cout << "[INFO] Apex API Gateway listening on http://0.0.0.0:" << port_ << "\n";

    auto accept_loop = [this]() {
        while (running_) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
            if (client_fd < 0) {
                if (!running_) break;
                continue;
            }
            std::thread([this, client_fd]() {
                handle_client(client_fd);
            }).detach();
        }
    };

    if (block) {
        accept_loop();
    } else {
        worker_threads_.emplace_back(accept_loop);
    }
}

void HttpServer::stop() {
    if (running_) {
        running_ = false;
        if (server_fd_ >= 0) {
            shutdown(server_fd_, SHUT_RDWR);
            close(server_fd_);
            server_fd_ = -1;
        }
        for (auto& t : worker_threads_) {
            if (t.joinable()) t.join();
        }
        worker_threads_.clear();
    }
}

void HttpServer::handle_client(int client_fd) {
    char buffer[8192] = {0};
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }

    std::string raw_req(buffer, static_cast<size_t>(bytes_read));
    std::istringstream stream(raw_req);
    std::string method, full_path, version;
    stream >> method >> full_path >> version;

    // OPTIONS preflight check
    if (method == "OPTIONS") {
        std::string res = "HTTP/1.1 204 No Content\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                          "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
                          "Content-Length: 0\r\n\r\n";
        write(client_fd, res.data(), res.size());
        close(client_fd);
        return;
    }

    std::string path = full_path;
    std::string query = "";
    auto q_pos = full_path.find('?');
    if (q_pos != std::string::npos) {
        path = full_path.substr(0, q_pos);
        query = full_path.substr(q_pos + 1);
    }

    HttpRequest req;
    req.method = method;
    req.path = path;
    req.query = query;

    // Encontra rota correspondente
    HttpResponse res{404, "application/json", R"({"error": "Route not found", "code": "NOT_FOUND"})", {}};
    for (const auto& [route_key, handler] : routes_) {
        auto colon = route_key.find(':');
        std::string r_method = route_key.substr(0, colon);
        std::string r_prefix = route_key.substr(colon + 1);

        if (r_method == method && path.rfind(r_prefix, 0) == 0) {
            res = handler(req);
            break;
        }
    }

    std::ostringstream oss;
    oss << "HTTP/1.1 " << res.status_code << " " << (res.status_code == 200 ? "OK" : "Error") << "\r\n"
        << "Content-Type: " << res.content_type << "\r\n"
        << "Content-Length: " << res.body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        << "Connection: close\r\n\r\n"
        << res.body;

    std::string out = oss.str();
    write(client_fd, out.data(), out.size());
    close(client_fd);
}

} // namespace apex::gateway

#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <map>
#include <string>
#include <thread>

namespace apex::testing {

/**
 * Servidor HTTP mínimo para testes.
 *
 * Serve corpos gravados em caminhos fixos, de modo que o cliente OpenF1 seja
 * exercido pelo seu caminho real — socket, HTTP, simdjson e normalização — em
 * vez de por structs montados à mão no teste.
 */
class StubHttpServer {
public:
    StubHttpServer() = default;

    ~StubHttpServer() { stop(); }

    /** Registra o corpo devolvido para um prefixo de caminho (ex.: "/v1/car_data"). */
    void serve(std::string path_prefix, std::string body) {
        routes_.emplace(std::move(path_prefix), std::move(body));
    }

    /** Sobe o servidor em uma porta livre e devolve a porta escolhida, ou 0 em erro. */
    int start() {
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) return 0;

        int reuse = 1;
        ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
        address.sin_port = 0; // o kernel escolhe uma porta livre
        if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return 0;
        }
        if (::listen(listen_fd_, 8) < 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return 0;
        }

        socklen_t length = sizeof(address);
        if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&address), &length) < 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return 0;
        }
        port_ = ::ntohs(address.sin_port);

        running_ = true;
        worker_ = std::thread([this] { loop(); });
        return port_;
    }

    void stop() {
        if (!running_.exchange(false)) return;
        if (listen_fd_ >= 0) {
            ::shutdown(listen_fd_, SHUT_RDWR);
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
        if (worker_.joinable()) worker_.join();
    }

    int port() const { return port_; }
    std::string base_url() const { return "http://127.0.0.1:" + std::to_string(port_) + "/v1"; }
    int requests_served() const { return requests_served_.load(); }

private:
    int listen_fd_{-1};
    int port_{0};
    std::atomic<bool> running_{false};
    std::atomic<int> requests_served_{0};
    std::thread worker_;
    std::map<std::string, std::string> routes_;

    void loop() {
        while (running_) {
            const int client = ::accept(listen_fd_, nullptr, nullptr);
            if (client < 0) {
                if (!running_) break;
                continue;
            }

            std::string request;
            char buffer[4096];
            while (request.find("\r\n\r\n") == std::string::npos) {
                const auto received = ::recv(client, buffer, sizeof(buffer), 0);
                if (received <= 0) break;
                request.append(buffer, static_cast<size_t>(received));
            }

            std::string path;
            if (const auto start = request.find(' '); start != std::string::npos) {
                const auto end = request.find(' ', start + 1);
                if (end != std::string::npos) path = request.substr(start + 1, end - start - 1);
            }

            const std::string* body = nullptr;
            for (const auto& [prefix, payload] : routes_) {
                if (path.rfind(prefix, 0) == 0) {
                    body = &payload;
                    break;
                }
            }

            std::string response;
            if (body) {
                response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                           std::to_string(body->size()) + "\r\nConnection: close\r\n\r\n" + *body;
            } else {
                response =
                    "HTTP/1.1 404 Not Found\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}";
            }
            ::send(client, response.data(), response.size(), MSG_NOSIGNAL);
            ::close(client);
            ++requests_served_;
        }
    }
};

} // namespace apex::testing

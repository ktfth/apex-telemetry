#include "http_server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cctype>
#include <csignal>

namespace apex::gateway {

HttpServer::HttpServer(int port) : port_(port) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::route(const std::string& method, const std::string& path_prefix, HttpHandler handler) {
    routes_.push_back({method, path_prefix, std::move(handler)});
}

void HttpServer::route_sse(const std::string& path_prefix, SseHandler handler) {
    sse_routes_.push_back({path_prefix, std::move(handler)});
}

// -- SseWriter implementation --

bool SseWriter::send(const std::string& data, const std::string& event, const std::string& id) const {
    std::ostringstream frame;
    if (!id.empty()) frame << "id: " << id << "\n";
    if (!event.empty()) frame << "event: " << event << "\n";
    // Split data by newlines per SSE spec
    std::istringstream lines(data);
    std::string line;
    while (std::getline(lines, line)) {
        frame << "data: " << line << "\n";
    }
    frame << "\n"; // End of event
    const auto payload = frame.str();
    const auto written = ::send(fd_, payload.data(), payload.size(), MSG_NOSIGNAL);
    return written == static_cast<ssize_t>(payload.size());
}

bool SseWriter::connected() const {
    struct pollfd pfd{};
    pfd.fd = fd_;
    pfd.events = POLLOUT;
    const int result = poll(&pfd, 1, 0);
    return result > 0 && !(pfd.revents & (POLLERR | POLLHUP));
}

namespace {
std::vector<std::string> split_path(const std::string& value) {
    std::vector<std::string> parts;
    std::stringstream stream(value);
    std::string part;
    while (std::getline(stream, part, '/')) if (!part.empty()) parts.push_back(part);
    return parts;
}

std::string url_decode(const std::string& value) {
    std::string result;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+' ) result.push_back(' ');
        else if (value[i] == '%' && i + 2 < value.size() && std::isxdigit(value[i + 1]) && std::isxdigit(value[i + 2])) {
            result.push_back(static_cast<char>(std::stoi(value.substr(i + 1, 2), nullptr, 16)));
            i += 2;
        } else result.push_back(value[i]);
    }
    return result;
}

std::map<std::string, std::string> parse_query(const std::string& query) {
    std::map<std::string, std::string> params;
    std::stringstream stream(query);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        const auto pos = pair.find('=');
        const auto key = url_decode(pair.substr(0, pos));
        if (!key.empty()) params[key] = pos == std::string::npos ? "" : url_decode(pair.substr(pos + 1));
    }
    return params;
}

bool match_route(const std::string& pattern, const std::string& path, std::map<std::string, std::string>& params) {
    const auto expected = split_path(pattern);
    const auto actual = split_path(path);
    if (expected.size() != actual.size()) return false;
    for (size_t i = 0; i < expected.size(); ++i) {
        if (!expected[i].empty() && expected[i][0] == ':') params[expected[i].substr(1)] = url_decode(actual[i]);
        else if (expected[i] != actual[i]) return false;
    }
    return true;
}
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
    std::signal(SIGPIPE, SIG_IGN);
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
        send(client_fd, res.data(), res.size(), MSG_NOSIGNAL);
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
    req.query_params = parse_query(query);

    // Parse request headers
    std::string header_line;
    while (std::getline(stream, header_line) && header_line != "\r" && !header_line.empty()) {
        auto colon_pos = header_line.find(':');
        if (colon_pos != std::string::npos) {
            auto key = header_line.substr(0, colon_pos);
            auto val = header_line.substr(colon_pos + 1);
            // Trim whitespace
            while (!val.empty() && (val[0] == ' ' || val[0] == '\t')) val.erase(0, 1);
            while (!val.empty() && (val.back() == '\r' || val.back() == '\n')) val.pop_back();
            req.headers[key] = val;
        }
    }

    // Check SSE routes first (GET only)
    if (method == "GET") {
        for (const auto& sse_route : sse_routes_) {
            std::map<std::string, std::string> path_params;
            if (match_route(sse_route.pattern, path, path_params)) {
                req.path_params = std::move(path_params);

                // Send SSE headers — do NOT close the fd; the handler owns it
                std::string sse_headers =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/event-stream\r\n"
                    "Cache-Control: no-cache\r\n"
                    "Connection: keep-alive\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "\r\n";
                send(client_fd, sse_headers.data(), sse_headers.size(), MSG_NOSIGNAL);

                SseWriter writer(client_fd);
                sse_route.handler(req, writer, running_);

                close(client_fd);
                return;
            }
        }
    }

    // Regular HTTP routes
    HttpResponse res{404, "application/json", R"({"error": "Route not found", "code": "NOT_FOUND"})", {}};
    for (const auto& route : routes_) {
        std::map<std::string, std::string> path_params;
        if (route.method == method && match_route(route.pattern, path, path_params)) {
            req.path_params = std::move(path_params);
            res = route.handler(req);
            break;
        }
    }

    std::ostringstream oss;
    oss << "HTTP/1.1 " << res.status_code << " " << (res.status_code == 200 ? "OK" : "Error") << "\r\n"
        << "Content-Type: " << res.content_type << "\r\n"
        << "Content-Length: " << res.body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        << "Access-Control-Expose-Headers: X-Apex-Data-Source\r\n";
    for (const auto& [name, value] : res.headers) oss << name << ": " << value << "\r\n";
    oss << "Connection: close\r\n\r\n" << res.body;

    std::string out = oss.str();
    send(client_fd, out.data(), out.size(), MSG_NOSIGNAL);
    close(client_fd);
}

} // namespace apex::gateway

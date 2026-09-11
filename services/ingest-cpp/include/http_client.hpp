#pragma once

#include <string>
#include <chrono>
#include <memory>
#include <mutex>

namespace apex::ingest {

struct HttpResponse {
    int status_code{0};
    std::string body{};
    std::string error_message{};
    bool is_success() const { return status_code >= 200 && status_code < 300; }
};

struct HttpClientConfig {
    long timeout_seconds{10};
    int max_retries{3};
    std::chrono::milliseconds retry_delay_ms{500};
    std::chrono::milliseconds rate_limit_min_interval_ms{200}; // ~5 req/s max
    std::string user_agent{"ApexTelemetry-Engine/1.0 (Automotive Telemetry Ingest)"};
};

class HttpClient {
public:
    explicit HttpClient(HttpClientConfig config = {});
    ~HttpClient();

    HttpResponse get(const std::string& url);

private:
    HttpClientConfig config_;
    std::mutex rate_limit_mutex_;
    std::chrono::steady_clock::time_point last_request_time_;

    void enforce_rate_limit();
};

} // namespace apex::ingest

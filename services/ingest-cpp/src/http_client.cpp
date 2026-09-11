#include "http_client.hpp"
#include <curl/curl.h>
#include <thread>
#include <iostream>
#include <cmath>

namespace apex::ingest {

static size_t curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), total_size);
    return total_size;
}

HttpClient::HttpClient(HttpClientConfig config)
    : config_(std::move(config)),
      last_request_time_(std::chrono::steady_clock::now() - config_.rate_limit_min_interval_ms) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

void HttpClient::enforce_rate_limit() {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_request_time_);

    if (elapsed < config_.rate_limit_min_interval_ms) {
        auto wait_time = config_.rate_limit_min_interval_ms - elapsed;
        std::this_thread::sleep_for(wait_time);
    }
    last_request_time_ = std::chrono::steady_clock::now();
}

HttpResponse HttpClient::get(const std::string& url) {
    HttpResponse response;

    for (int attempt = 0; attempt <= config_.max_retries; ++attempt) {
        enforce_rate_limit();

        CURL* curl = curl_easy_init();
        if (!curl) {
            response.error_message = "Failed to initialize CURL easy handle";
            return response;
        }

        std::string response_body;
        char errbuf[CURL_ERROR_SIZE] = {0};

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, config_.timeout_seconds);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, config_.user_agent.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        CURLcode res = curl_easy_perform(curl);

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK && http_code >= 200 && http_code < 300) {
            response.status_code = static_cast<int>(http_code);
            response.body = std::move(response_body);
            response.error_message.clear();
            return response;
        }

        // Falha: log estruturado e cálculo de backoff exponencial
        response.status_code = static_cast<int>(http_code);
        response.error_message = (res != CURLE_OK) ? std::string(errbuf) : ("HTTP " + std::to_string(http_code));

        if (attempt < config_.max_retries) {
            auto backoff = config_.retry_delay_ms * static_cast<long long>(std::pow(2, attempt));
            std::this_thread::sleep_for(backoff);
        }
    }

    return response;
}

} // namespace apex::ingest

#include "http_client.hpp"

#include <curl/curl.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace apex::common {
namespace {

size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    const size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

/** Erros de transporte, 429 e 5xx são transitórios; os demais 4xx não são. */
bool is_retryable(CURLcode code, long http_status) {
    if (code != CURLE_OK) return true;
    if (http_status == 429) return true;
    return http_status >= 500;
}

long ip_resolve_option(IpFamily family) {
    switch (family) {
        case IpFamily::V4: return CURL_IPRESOLVE_V4;
        case IpFamily::V6: return CURL_IPRESOLVE_V6;
        case IpFamily::Auto: break;
    }
    return CURL_IPRESOLVE_WHATEVER;
}

} // namespace

HttpClientConfig http_config_from_environment() {
    HttpClientConfig config;
    if (const char* value = std::getenv("APEX_HTTP_IP_FAMILY")) {
        if (std::strcmp(value, "auto") == 0) config.ip_family = IpFamily::Auto;
        else if (std::strcmp(value, "v6") == 0) config.ip_family = IpFamily::V6;
        else config.ip_family = IpFamily::V4;
    }
    if (const char* value = std::getenv("APEX_HTTP_TIMEOUT_S")) {
        const long parsed = std::strtol(value, nullptr, 10);
        if (parsed > 0 && parsed <= 600) config.timeout_seconds = parsed;
    }
    return config;
}

HttpClient::HttpClient(HttpClientConfig config)
    : config_(std::move(config)),
      last_request_time_(std::chrono::steady_clock::now() - config_.rate_limit_min_interval_ms) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    handle_ = curl_easy_init();
}

HttpClient::~HttpClient() {
    if (handle_) curl_easy_cleanup(static_cast<CURL*>(handle_));
    curl_global_cleanup();
}

std::string HttpClient::url_encode(const std::string& value) {
    CURL* curl = curl_easy_init();
    if (!curl) return value;
    char* escaped = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
    std::string result = escaped ? std::string(escaped) : value;
    if (escaped) curl_free(escaped);
    curl_easy_cleanup(curl);
    return result;
}

void HttpClient::enforce_rate_limit() {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_request_time_);
    if (elapsed < config_.rate_limit_min_interval_ms) {
        std::this_thread::sleep_for(config_.rate_limit_min_interval_ms - elapsed);
    }
    last_request_time_ = std::chrono::steady_clock::now();
}

HttpResponse HttpClient::get(const std::string& url) { return request(url, nullptr); }

HttpResponse HttpClient::post_json(const std::string& url, const std::string& body) {
    return request(url, &body);
}

HttpResponse HttpClient::request(const std::string& url, const std::string* json_body) {
    HttpResponse response;

    std::lock_guard<std::mutex> lock(mutex_);
    auto* curl = static_cast<CURL*>(handle_);
    if (!curl) {
        response.error_message = "CURL easy handle unavailable";
        return response;
    }

    for (int attempt = 0; attempt <= config_.max_retries; ++attempt) {
        enforce_rate_limit();

        std::string body;
        char errbuf[CURL_ERROR_SIZE] = {0};

        curl_easy_reset(curl);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, config_.timeout_seconds);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, config_.connect_timeout_seconds);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, config_.user_agent.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, ip_resolve_option(config_.ip_family));

        curl_slist* headers = nullptr;
        if (json_body) {
            headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body->c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_body->size()));
        }

        const CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (headers) curl_slist_free_all(headers);

        if (res == CURLE_OK && http_code >= 200 && http_code < 300) {
            response.status_code = static_cast<int>(http_code);
            response.body = std::move(body);
            response.error_message.clear();
            return response;
        }

        response.status_code = static_cast<int>(http_code);
        response.error_message = (res != CURLE_OK) ? std::string(errbuf[0] ? errbuf : curl_easy_strerror(res))
                                                   : ("HTTP " + std::to_string(http_code));

        if (!is_retryable(res, http_code)) return response;
        if (attempt < config_.max_retries) {
            // O upstream sinaliza excesso de requisições: recuar bem mais do que em
            // uma falha de transporte, senão a tentativa seguinte também é recusada.
            const long long multiplier = http_code == 429 ? 8 : 1;
            std::this_thread::sleep_for(config_.retry_delay_ms * multiplier *
                                        static_cast<long long>(std::pow(2, attempt)));
        }
    }

    return response;
}

} // namespace apex::common

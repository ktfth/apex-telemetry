#pragma once

#include <chrono>
#include <mutex>
#include <string>

namespace apex::common {

struct HttpResponse {
    int status_code{0};
    std::string body{};
    std::string error_message{};
    bool is_success() const { return status_code >= 200 && status_code < 300; }
    /** O upstream recusou por excesso de requisições; não é ausência de dado. */
    bool is_rate_limited() const { return status_code == 429; }
};

/** Família de endereços usada na resolução de nomes. */
enum class IpFamily { Auto, V4, V6 };

struct HttpClientConfig {
    long timeout_seconds{25};
    long connect_timeout_seconds{8};
    int max_retries{3};
    std::chrono::milliseconds retry_delay_ms{400};
    std::chrono::milliseconds rate_limit_min_interval_ms{200}; // ~5 req/s
    std::string user_agent{"ApexTelemetry-Engine/2.0 (+https://openf1.org)"};
    /**
     * Padrão IPv4. Em redes dual-stack em que o AAAA é anunciado mas inalcançável,
     * a thread de resolução do libcurl fica presa em `poll` e `curl_easy_cleanup`
     * trava ao tentar juntá-la. Ajustável por `APEX_HTTP_IP_FAMILY=auto|v4|v6`.
     */
    IpFamily ip_family{IpFamily::V4};
};

/** Lê `APEX_HTTP_IP_FAMILY` e devolve a configuração correspondente. */
HttpClientConfig http_config_from_environment();

/**
 * Cliente HTTP com rate limiting cooperativo, backoff exponencial e reuso de
 * conexão (keep-alive) através de um handle persistente.
 *
 * Thread-safe: as requisições são serializadas, de modo que o orçamento de
 * chamadas ao upstream é respeitado mesmo sob concorrência.
 */
class HttpClient {
public:
    explicit HttpClient(HttpClientConfig config = http_config_from_environment());
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    HttpResponse get(const std::string& url);

    /** POST com corpo JSON; usado na comunicação com o motor de domínio Haskell. */
    HttpResponse post_json(const std::string& url, const std::string& body);

    /** Codifica um valor para uso seguro em query string (RFC 3986). */
    static std::string url_encode(const std::string& value);

private:
    HttpClientConfig config_;
    std::mutex mutex_;
    std::chrono::steady_clock::time_point last_request_time_;
    void* handle_{nullptr}; // CURL*

    void enforce_rate_limit();
    HttpResponse request(const std::string& url, const std::string* json_body);
};

} // namespace apex::common

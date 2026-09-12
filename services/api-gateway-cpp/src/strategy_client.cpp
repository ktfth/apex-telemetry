#include "strategy_client.hpp"

#include "../../common/simdjson/simdjson.h"

#include <cstdlib>

namespace apex::gateway {
namespace {

apex::common::HttpClientConfig local_service_config() {
    auto config = apex::common::http_config_from_environment();
    // Serviço local: sem orçamento de rate limit e com timeout curto, para que a
    // indisponibilidade do motor degrade a resposta em vez de travar o gateway.
    config.rate_limit_min_interval_ms = std::chrono::milliseconds(0);
    config.timeout_seconds = 5;
    config.connect_timeout_seconds = 2;
    config.max_retries = 1;
    return config;
}

} // namespace

StrategyClient::StrategyClient(std::string base_url) : base_url_(std::move(base_url)) {
    if (base_url_.empty()) {
        if (const char* value = std::getenv("APEX_STRATEGY_URL")) base_url_ = value;
    }
    while (!base_url_.empty() && base_url_.back() == '/') base_url_.pop_back();
    if (!base_url_.empty()) {
        http_ = std::make_unique<apex::common::HttpClient>(local_service_config());
    }
}

bool StrategyClient::healthy() const {
    if (!configured() || !http_) return false;
    const auto response = http_->get(base_url_ + "/health");
    if (!response.is_success()) {
        last_error_ = response.error_message;
        return false;
    }
    return true;
}

std::optional<std::string> StrategyClient::call(const std::string& path, const std::string& body,
                                                const std::string& array_field) {
    if (!configured() || !http_) {
        last_error_ = "strategy engine URL not configured (set APEX_STRATEGY_URL)";
        return std::nullopt;
    }

    const auto response = http_->post_json(base_url_ + path, body);
    if (!response.is_success()) {
        last_error_ = "strategy engine " + path + ": " +
                      (response.error_message.empty() ? "unknown transport error"
                                                      : response.error_message);
        return std::nullopt;
    }

    // Extrai apenas o array pedido, preservando o JSON original sem reserializar.
    try {
        simdjson::dom::parser parser;
        simdjson::dom::element document = parser.parse(response.body);
        simdjson::dom::element array;
        if (document[array_field].get(array) != simdjson::SUCCESS) {
            last_error_ = "strategy engine response has no '" + array_field + "' array";
            return std::nullopt;
        }
        return simdjson::to_string(array);
    } catch (const std::exception& error) {
        last_error_ = std::string("strategy engine response parse: ") + error.what();
        return std::nullopt;
    }
}

std::optional<std::string> StrategyClient::insights(const std::string& evidence_json) {
    return call("/v1/insights", evidence_json, "insights");
}

std::optional<std::string> StrategyClient::degradation(const std::string& stints_json) {
    return call("/v1/degradation", stints_json, "stints");
}

} // namespace apex::gateway

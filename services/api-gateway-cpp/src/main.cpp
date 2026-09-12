#include "database_repository.hpp"
#include "http_server.hpp"
#include "metrics_collector.hpp"
#include "spatial_alignment.hpp"
#include "strategy_client.hpp"
#include "telemetry_resolver.hpp"

#include "../../common/http/http_client.hpp"
#include "../../common/openf1/openf1_client.hpp"
#include "../../common/simdjson/simdjson.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <thread>

namespace {

using apex::analytics::SpatialAlignmentEngine;
using apex::gateway::HttpRequest;
using apex::gateway::HttpResponse;
using apex::gateway::MetricsCollector;
using apex::gateway::ResolveError;
using apex::gateway::TelemetryResolver;

apex::gateway::HttpServer* g_server = nullptr;

void signal_handler(int) {
    std::cout << "\n[INFO] Encerrando o gateway...\n";
    if (g_server) g_server->stop();
}

std::optional<int64_t> parse_positive_integer(const std::string& value) {
    int64_t result{};
    const auto* end = value.data() + value.size();
    const auto [pointer, code] = std::from_chars(value.data(), end, result);
    if (code != std::errc{} || pointer != end || result <= 0) return std::nullopt;
    return result;
}

std::optional<std::string> query_value(const HttpRequest& request, const std::string& key) {
    const auto it = request.query_params.find(key);
    if (it == request.query_params.end() || it->second.empty()) return std::nullopt;
    return it->second;
}

HttpResponse api_error(int status, const std::string& code, const std::string& message) {
    std::ostringstream body;
    body << "{\"error\":\"" << SpatialAlignmentEngine::json_escape(message) << "\",\"code\":\""
         << SpatialAlignmentEngine::json_escape(code) << "\"}";
    return {status, "application/json", body.str(), {}};
}

HttpResponse api_error(const ResolveError& error) {
    return api_error(error.status, error.code, error.message);
}

HttpResponse data_response(std::string body, const std::string& source) {
    MetricsCollector::instance().record_source(source);
    return {200, "application/json", std::move(body), {{"X-Apex-Data-Source", source}}};
}

/**
 * Envolve um handler com o cronômetro de métricas e um log de acesso.
 *
 * O log existe para diagnóstico operacional: sem uma linha por requisição, a única
 * evidência de que o gateway foi chamado são contadores agregados em /metrics, que
 * não dizem *qual* chamada falhou nem por quê.
 */
apex::gateway::HttpHandler instrumented(std::string route, apex::gateway::HttpHandler handler) {
    return [route = std::move(route), handler = std::move(handler)](const HttpRequest& request) {
        const auto started = std::chrono::steady_clock::now();
        MetricsCollector::ScopedTimer timer(MetricsCollector::instance(), route);
        auto response = handler(request);
        timer.set_status(response.status_code);

        const auto elapsed_ms = std::chrono::duration<double, std::milli>(
                                    std::chrono::steady_clock::now() - started)
                                    .count();
        const auto source = response.headers.find("X-Apex-Data-Source");
        std::cout << "[" << response.status_code << "] " << std::setw(22) << std::left << route
                  << std::right << " " << std::fixed << std::setprecision(1) << std::setw(8)
                  << elapsed_ms << " ms  " << request.path
                  << (request.query.empty() ? "" : "?" + request.query)
                  << (source != response.headers.end() ? "  <- " + source->second : "");
        if (response.status_code >= 400) {
            std::cout << "  " << response.body.substr(0, 160);
        }
        std::cout << '\n';
        return response;
    };
}

/** Lê um campo opcional de um documento simdjson, mantendo o padrão quando ausente. */
template <typename T>
T dom_field(const simdjson::dom::element& element, std::string_view key, T fallback) {
    T value{};
    if (element[key].get(value) != simdjson::SUCCESS) return fallback;
    return value;
}

std::string format_number(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

/** Resultado completo de uma comparação, reutilizado por compare, export e live. */
struct ComparisonResult {
    apex::analytics::SessionMetadata session;
    apex::analytics::LapMetadata ref_lap;
    apex::analytics::LapMetadata comp_lap;
    std::vector<apex::analytics::ComparisonChannelPoint> channels;
    std::vector<apex::analytics::Corner> corners;
    std::vector<apex::analytics::SpeedTrap> speed_traps;
    std::vector<apex::analytics::Microsector> microsectors;
    std::vector<apex::analytics::TelemetrySegment> segments;
    apex::analytics::QualityMetrics quality;
    double grid_step_m{5.0};
    std::string insights_engine{"analytics-cpp"};
    std::string insights_json;
};

/**
 * Pipeline de análise: telemetria medida → distância integrada → grade métrica →
 * delta → curvas, microsetores, speed traps e segmentos de perda. A explicação em
 * linguagem natural vem do motor Haskell; sem ele, do resumo determinístico em C++.
 */
std::optional<ComparisonResult> run_comparison(TelemetryResolver& resolver,
                                               apex::gateway::StrategyClient& strategy,
                                               int64_t session_key, int32_t ref_driver,
                                               int32_t ref_lap_number, int32_t comp_driver,
                                               int32_t comp_lap_number, double grid_step_m,
                                               ResolveError& error) {
    ComparisonResult result;
    result.grid_step_m = grid_step_m;

    const auto session = resolver.session_metadata(session_key, error);
    if (!session) return std::nullopt;
    result.session = *session;

    const auto reference = resolver.lap_telemetry(session_key, ref_driver, ref_lap_number, error);
    if (!reference) return std::nullopt;
    const auto comparison = resolver.lap_telemetry(session_key, comp_driver, comp_lap_number, error);
    if (!comparison) return std::nullopt;

    result.ref_lap = reference->metadata;
    result.comp_lap = comparison->metadata;

    auto ref_distance = SpatialAlignmentEngine::integrate_distance(reference->samples);
    auto comp_distance = SpatialAlignmentEngine::integrate_distance(comparison->samples);

    // A volta de referência define o eixo espacial: o comprimento que ela mediu
    // vira a régua para as duas voltas. Sem isso, o erro de escala da integração
    // a 4 Hz difere entre pilotos e o delta acumulado não fecha com o cronômetro.
    ref_distance = SpatialAlignmentEngine::normalize_lap_distance(
        std::move(ref_distance), result.ref_lap.lap_time_s, 0.0); // fecha as pontas, sem reescalar
    const double axis_length_m = ref_distance.empty() ? 0.0 : ref_distance.back().distance_m;
    comp_distance = SpatialAlignmentEngine::normalize_lap_distance(
        std::move(comp_distance), result.comp_lap.lap_time_s, axis_length_m);

    const double max_gap_m =
        std::max(SpatialAlignmentEngine::recommended_max_gap_m(ref_distance, grid_step_m),
                 SpatialAlignmentEngine::recommended_max_gap_m(comp_distance, grid_step_m));
    const auto ref_grid = SpatialAlignmentEngine::resample_to_grid(ref_distance, grid_step_m, max_gap_m);
    const auto comp_grid = SpatialAlignmentEngine::resample_to_grid(comp_distance, grid_step_m, max_gap_m);

    result.channels = SpatialAlignmentEngine::align_and_compute_delta(ref_grid, comp_grid);
    if (result.channels.size() < 20) {
        error = {422, "ALIGNMENT_FAILED",
                 "The two laps produced only " + std::to_string(result.channels.size()) +
                     " aligned grid points; the telemetry coverage is too sparse to compare"};
        return std::nullopt;
    }

    result.corners = SpatialAlignmentEngine::detect_corners(ref_grid);
    result.microsectors = SpatialAlignmentEngine::compute_microsectors(result.channels, 100.0);
    result.speed_traps =
        SpatialAlignmentEngine::compute_speed_traps(result.channels, result.ref_lap, result.comp_lap);
    result.segments = SpatialAlignmentEngine::detect_segments(result.channels, result.corners);
    result.quality = SpatialAlignmentEngine::compute_quality_audit(ref_distance, comp_distance,
                                                                   ref_grid, comp_grid, max_gap_m);

    // Verificação independente: o delta acumulado no fim da volta deve reproduzir a
    // diferença cronometrada oficial. O desvio é publicado para auditoria.
    result.quality.delta_closure_error_s =
        std::abs(result.channels.back().delta_time_s -
                 (result.comp_lap.lap_time_s - result.ref_lap.lap_time_s));

    // A origem real dos dois conjuntos de amostras é declarada no payload.
    result.session.data_source = reference->source == comparison->source
                                     ? reference->source
                                     : reference->source + "+" + comparison->source;

    const auto evidence = SpatialAlignmentEngine::build_evidence_json(result.session, result.ref_lap,
                                                                      result.comp_lap, result.segments);
    if (strategy.configured()) {
        const auto insights = strategy.insights(evidence);
        MetricsCollector::instance().record_strategy(insights.has_value());
        if (insights) {
            result.insights_json = *insights;
            result.insights_engine = "strategy-hs";
        } else {
            std::cerr << "[WARN] motor de estratégia indisponível: " << strategy.last_error() << "\n";
        }
    }
    if (result.insights_json.empty()) {
        result.insights_json = SpatialAlignmentEngine::build_insights_json(
            result.comp_lap, result.segments, result.quality, grid_step_m);
    }

    return result;
}

/** Parâmetros comuns aos endpoints de análise. */
struct ComparisonQuery {
    int64_t session_key{0};
    int32_t ref_driver{0};
    int32_t ref_lap{0};
    int32_t comp_driver{0};
    int32_t comp_lap{0};
    double step_m{5.0};
};

std::optional<ComparisonQuery> parse_comparison_query(const HttpRequest& request,
                                                      HttpResponse& failure) {
    ComparisonQuery query;
    static constexpr const char* required[] = {"session_key", "ref_driver", "ref_lap", "comp_driver",
                                               "comp_lap"};
    for (const auto* name : required) {
        if (!query_value(request, name)) {
            failure = api_error(400, "MISSING_PARAMETER",
                                std::string("Missing query parameter: ") + name);
            return std::nullopt;
        }
    }

    const auto session = parse_positive_integer(*query_value(request, "session_key"));
    const auto ref_driver = parse_positive_integer(*query_value(request, "ref_driver"));
    const auto ref_lap = parse_positive_integer(*query_value(request, "ref_lap"));
    const auto comp_driver = parse_positive_integer(*query_value(request, "comp_driver"));
    const auto comp_lap = parse_positive_integer(*query_value(request, "comp_lap"));
    if (!session || !ref_driver || !ref_lap || !comp_driver || !comp_lap) {
        failure = api_error(422, "INVALID_PARAMETER",
                            "Session key, driver numbers and lap numbers must be positive integers");
        return std::nullopt;
    }
    if (*ref_driver == *comp_driver && *ref_lap == *comp_lap) {
        failure = api_error(422, "IDENTICAL_LAPS", "Reference and comparison laps must differ");
        return std::nullopt;
    }

    query.session_key = *session;
    query.ref_driver = static_cast<int32_t>(*ref_driver);
    query.ref_lap = static_cast<int32_t>(*ref_lap);
    query.comp_driver = static_cast<int32_t>(*comp_driver);
    query.comp_lap = static_cast<int32_t>(*comp_lap);

    if (const auto step = query_value(request, "step_m")) {
        try {
            size_t consumed = 0;
            query.step_m = std::stod(*step, &consumed);
            if (consumed != step->size() || !std::isfinite(query.step_m) || query.step_m < 1.0 ||
                query.step_m > 50.0) {
                failure = api_error(422, "INVALID_GRID_STEP", "step_m must be between 1 and 50 metres");
                return std::nullopt;
            }
        } catch (...) {
            failure = api_error(422, "INVALID_GRID_STEP", "step_m must be a finite number");
            return std::nullopt;
        }
    }

    return query;
}

/** Monta o corpo do pedido de degradação a partir de voltas e stints reais. */
std::string build_degradation_request(TelemetryResolver& resolver, int64_t session_key,
                                      std::optional<int32_t> driver_number,
                                      const apex::analytics::SessionMetadata& session,
                                      ResolveError& error) {
    const auto stints_payload = resolver.stints(session_key, driver_number, error);
    if (!stints_payload) return {};
    const auto laps_payload = resolver.laps(session_key, driver_number, error);
    if (!laps_payload) return {};

    // Cruza stints e voltas com simdjson para evitar uma segunda ida à origem.
    simdjson::dom::parser parser;
    simdjson::dom::element stints_doc;
    simdjson::dom::element laps_doc;
    if (parser.parse(stints_payload->json).get(stints_doc) != simdjson::SUCCESS) {
        error = {500, "INTERNAL_ERROR", "Could not parse the resolved stint payload"};
        return {};
    }
    simdjson::dom::parser laps_parser;
    if (laps_parser.parse(laps_payload->json).get(laps_doc) != simdjson::SUCCESS) {
        error = {500, "INTERNAL_ERROR", "Could not parse the resolved lap payload"};
        return {};
    }

    struct LapFact {
        int64_t driver;
        int64_t number;
        double time_s;
        bool valid;
    };
    std::vector<LapFact> laps;
    for (auto lap : laps_doc.get_array()) {
        double time_s = 0.0;
        if (lap["lap_time_s"].get_double().get(time_s) != simdjson::SUCCESS || time_s <= 0.0) continue;
        int64_t driver = 0;
        int64_t number = 0;
        if (lap["driver_number"].get_int64().get(driver) != simdjson::SUCCESS) continue;
        if (lap["lap_number"].get_int64().get(number) != simdjson::SUCCESS) continue;
        laps.push_back({driver, number, time_s, dom_field<bool>(lap, "is_valid", true)});
    }

    std::ostringstream oss;
    oss << '{';
    if (session.track_temperature_c) {
        oss << "\"track_temperature_c\":" << format_number(*session.track_temperature_c, 1) << ',';
    }
    // O horizonte da sessão é o maior número de volta efetivamente registrado.
    int64_t total_laps = 0;
    for (const auto& lap : laps) total_laps = std::max(total_laps, lap.number);
    oss << "\"total_session_laps\":" << total_laps << ",\"stints\":[";

    bool first_stint = true;
    for (auto stint : stints_doc.get_array()) {
        int64_t driver = 0;
        if (stint["driver_number"].get_int64().get(driver) != simdjson::SUCCESS) continue;
        const auto stint_number = dom_field<int64_t>(stint, "stint_number", 0);
        const auto lap_start = dom_field<int64_t>(stint, "lap_start", 0);
        const auto lap_end = dom_field<int64_t>(stint, "lap_end", 0);
        const auto age_at_start = dom_field<int64_t>(stint, "tyre_age_at_start", 0);
        const auto compound = dom_field<std::string_view>(stint, "compound", "UNKNOWN");

        if (!first_stint) oss << ',';
        first_stint = false;
        oss << "{\"driver_number\":" << driver << ",\"stint_number\":" << stint_number
            << ",\"compound\":\"" << SpatialAlignmentEngine::json_escape(std::string(compound))
            << "\",\"tyre_age_at_start\":" << age_at_start << ",\"laps\":[";

        bool first_lap = true;
        for (const auto& lap : laps) {
            if (lap.driver != driver || lap.number < lap_start || lap.number > lap_end) continue;
            if (!first_lap) oss << ',';
            first_lap = false;
            oss << "{\"lap_number\":" << lap.number
                << ",\"lap_time_s\":" << format_number(lap.time_s, 3)
                << ",\"tyre_age_laps\":" << (age_at_start + (lap.number - lap_start))
                << ",\"is_valid\":" << (lap.valid ? "true" : "false") << '}';
        }
        oss << "]}";
    }
    oss << "]}";
    return oss.str();
}

} // namespace

int main(int argc, char* argv[]) {
    // Sem isto, redirecionar a saída para arquivo deixa o log preso no buffer até
    // o processo terminar: quem for diagnosticar um serviço no ar vê um arquivo vazio.
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    int port = 8080;
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "[ERRO] Porta inválida: " << argv[1] << "\n";
            return 2;
        }
    }

    auto database = std::make_shared<apex::gateway::DatabaseRepository>();
    auto http_client = std::make_shared<apex::common::HttpClient>();
    auto upstream = std::make_shared<apex::openf1::Client>(http_client);
    auto resolver = std::make_shared<TelemetryResolver>(database, upstream);
    apex::gateway::StrategyClient strategy;

    // Todo tráfego real ao upstream é contabilizado, inclusive as recusas.
    upstream->set_request_observer([](std::string_view, int, bool success) {
        MetricsCollector::instance().record_upstream(success);
    });

    auto& metrics = MetricsCollector::instance();
    const auto start_time = std::chrono::steady_clock::now();

    std::cout << "====================================================\n"
              << " ApexTelemetry — API Gateway (C++23)                \n"
              << "====================================================\n"
              << " porta              : " << port << "\n"
              << " PostgreSQL         : "
              << (database->configured() ? (database->healthy() ? "conectado" : "configurado, sem resposta")
                                         : "não configurado (APEX_DATABASE_URL)")
              << "\n"
              << " upstream OpenF1    : " << upstream->base_url() << "\n"
              << " motor de estratégia: "
              << (strategy.configured() ? strategy.base_url() : "não configurado (APEX_STRATEGY_URL)")
              << "\n====================================================\n";

    apex::gateway::HttpServer server(port);
    g_server = &server;
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // ---------------------------------------------------------------- métricas
    server.route("GET", "/metrics", [&metrics](const HttpRequest&) {
        return HttpResponse{200, "text/plain; version=0.0.4; charset=utf-8", metrics.serialize(), {}};
    });

    // ------------------------------------------------------------------- saúde
    server.route("GET", "/api/v1/health",
                 instrumented("health", [database, resolver, &strategy, start_time](const HttpRequest&) {
                     const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                                             std::chrono::steady_clock::now() - start_time)
                                             .count();
                     std::ostringstream json;
                     json << R"({"status":"healthy","service":"apex-api-gateway","version":"2.0.0")"
                          << R"(,"analytics_engine":"spatial_alignment_cpp23")"
                          << R"(,"cpp_standard":202302)"
                          << R"(,"database":{"configured":)" << (database->configured() ? "true" : "false")
                          << R"(,"healthy":)" << (database->healthy() ? "true" : "false") << "}"
                          << R"(,"upstream":{"provider":"openf1","base_url":"https://api.openf1.org/v1"})"
                          << R"(,"strategy_engine":{"configured":)"
                          << (strategy.configured() ? "true" : "false") << R"(,"healthy":)"
                          << (strategy.healthy() ? "true" : "false") << "}"
                          << R"(,"cache_entries":)" << resolver->cache_entries()
                          << R"(,"uptime_seconds":)" << uptime << "}";
                     return HttpResponse{200, "application/json", json.str(), {}};
                 }));

    // --------------------------------------------------------------- catálogo
    server.route("GET", "/api/v1/sessions", instrumented("sessions", [resolver](const HttpRequest& request) {
                     std::optional<int> year;
                     if (const auto raw = query_value(request, "year")) {
                         const auto parsed = parse_positive_integer(*raw);
                         if (!parsed || *parsed < 1950 || *parsed > 2200) {
                             return api_error(422, "INVALID_YEAR", "year must be between 1950 and 2200");
                         }
                         year = static_cast<int>(*parsed);
                     }
                     ResolveError error;
                     const auto payload = resolver->sessions(year, error);
                     if (!payload) return api_error(error);
                     return data_response(payload->json, payload->source);
                 }));

    server.route("GET", "/api/v1/sessions/:session_key/drivers",
                 instrumented("drivers", [resolver](const HttpRequest& request) {
                     const auto session = parse_positive_integer(request.path_params.at("session_key"));
                     if (!session) {
                         return api_error(400, "INVALID_SESSION_KEY",
                                          "session_key must be a positive integer");
                     }
                     ResolveError error;
                     const auto payload = resolver->drivers(*session, error);
                     if (!payload) return api_error(error);
                     return data_response(payload->json, payload->source);
                 }));

    server.route("GET", "/api/v1/sessions/:session_key/laps",
                 instrumented("laps", [resolver](const HttpRequest& request) {
                     const auto session = parse_positive_integer(request.path_params.at("session_key"));
                     if (!session) {
                         return api_error(400, "INVALID_SESSION_KEY",
                                          "session_key must be a positive integer");
                     }
                     std::optional<int32_t> driver;
                     if (const auto raw = query_value(request, "driver_number")) {
                         const auto parsed = parse_positive_integer(*raw);
                         if (!parsed) {
                             return api_error(422, "INVALID_DRIVER_NUMBER",
                                              "driver_number must be a positive integer");
                         }
                         driver = static_cast<int32_t>(*parsed);
                     }
                     ResolveError error;
                     const auto payload = resolver->laps(*session, driver, error);
                     if (!payload) return api_error(error);
                     return data_response(payload->json, payload->source);
                 }));

    server.route("GET", "/api/v1/sessions/:session_key/stints",
                 instrumented("stints", [resolver](const HttpRequest& request) {
                     const auto session = parse_positive_integer(request.path_params.at("session_key"));
                     if (!session) {
                         return api_error(400, "INVALID_SESSION_KEY",
                                          "session_key must be a positive integer");
                     }
                     std::optional<int32_t> driver;
                     if (const auto raw = query_value(request, "driver_number")) {
                         if (const auto parsed = parse_positive_integer(*raw)) {
                             driver = static_cast<int32_t>(*parsed);
                         }
                     }
                     ResolveError error;
                     const auto payload = resolver->stints(*session, driver, error);
                     if (!payload) return api_error(error);
                     return data_response(payload->json, payload->source);
                 }));

    server.route("GET", "/api/v1/sessions/:session_key/race-control",
                 instrumented("race-control", [resolver](const HttpRequest& request) {
                     const auto session = parse_positive_integer(request.path_params.at("session_key"));
                     if (!session) {
                         return api_error(400, "INVALID_SESSION_KEY",
                                          "session_key must be a positive integer");
                     }
                     ResolveError error;
                     const auto payload = resolver->race_control(*session, error);
                     if (!payload) return api_error(error);
                     return data_response(payload->json, payload->source);
                 }));

    server.route("GET", "/api/v1/sessions/:session_key/weather",
                 instrumented("weather", [resolver](const HttpRequest& request) {
                     const auto session = parse_positive_integer(request.path_params.at("session_key"));
                     if (!session) {
                         return api_error(400, "INVALID_SESSION_KEY",
                                          "session_key must be a positive integer");
                     }
                     ResolveError error;
                     const auto payload = resolver->weather(*session, error);
                     if (!payload) return api_error(error);
                     return data_response(payload->json, payload->source);
                 }));

    // Traçado real do circuito, reconstruído do transponder de posição.
    server.route("GET", "/api/v1/sessions/:session_key/circuit",
                 instrumented("circuit", [resolver](const HttpRequest& request) {
                     const auto session = parse_positive_integer(request.path_params.at("session_key"));
                     if (!session) {
                         return api_error(400, "INVALID_SESSION_KEY",
                                          "session_key must be a positive integer");
                     }
                     ResolveError error;
                     const auto payload = resolver->circuit_geometry(*session, error);
                     if (!payload) return api_error(error);
                     return data_response(payload->json, payload->source);
                 }));

    // ---------------------------------------------------------------- análise
    server.route("GET", "/api/v1/analysis/compare",
                 instrumented("analysis-compare", [resolver, &strategy](const HttpRequest& request) {
                     HttpResponse failure;
                     const auto query = parse_comparison_query(request, failure);
                     if (!query) return failure;

                     ResolveError error;
                     const auto result =
                         run_comparison(*resolver, strategy, query->session_key, query->ref_driver,
                                        query->ref_lap, query->comp_driver, query->comp_lap,
                                        query->step_m, error);
                     if (!result) return api_error(error);

                     auto body = SpatialAlignmentEngine::build_comparison_json(
                         result->session, result->ref_lap, result->comp_lap, result->grid_step_m,
                         result->channels, result->corners, result->speed_traps, result->microsectors,
                         result->quality, result->insights_json);

                     MetricsCollector::instance().record_source(result->session.data_source);
                     return HttpResponse{200,
                                         "application/json",
                                         std::move(body),
                                         {{"X-Apex-Data-Source", result->session.data_source},
                                          {"X-Apex-Insight-Engine", result->insights_engine}}};
                 }));

    server.route("GET", "/api/v1/analysis/export",
                 instrumented("analysis-export", [resolver, &strategy](const HttpRequest& request) {
                     HttpResponse failure;
                     const auto query = parse_comparison_query(request, failure);
                     if (!query) return failure;

                     ResolveError error;
                     const auto result =
                         run_comparison(*resolver, strategy, query->session_key, query->ref_driver,
                                        query->ref_lap, query->comp_driver, query->comp_lap,
                                        query->step_m, error);
                     if (!result) return api_error(error);

                     const auto format = query_value(request, "format").value_or("motec_csv");
                     const std::string stem = "apex_" + result->ref_lap.driver_code + "_L" +
                                              std::to_string(result->ref_lap.lap_number) + "_vs_" +
                                              result->comp_lap.driver_code + "_L" +
                                              std::to_string(result->comp_lap.lap_number);

                     if (format == "json") {
                         auto body = SpatialAlignmentEngine::build_comparison_json(
                             result->session, result->ref_lap, result->comp_lap, result->grid_step_m,
                             result->channels, result->corners, result->speed_traps,
                             result->microsectors, result->quality, result->insights_json);
                         return HttpResponse{
                             200,
                             "application/json",
                             std::move(body),
                             {{"Content-Disposition", "attachment; filename=\"" + stem + ".json\""}}};
                     }
                     if (format != "motec_csv") {
                         return api_error(422, "INVALID_FORMAT",
                                          "format must be either 'motec_csv' or 'json'");
                     }

                     auto csv = SpatialAlignmentEngine::export_motec_csv(
                         result->session, result->ref_lap, result->comp_lap, result->channels);
                     return HttpResponse{
                         200,
                         "text/csv; charset=utf-8",
                         std::move(csv),
                         {{"Content-Disposition", "attachment; filename=\"" + stem + ".csv\""},
                          {"X-Apex-Export-Format", "MoTeC-CSV-v1"}}};
                 }));

    // Degradação real dos stints, avaliada pelo motor de domínio em Haskell.
    server.route("GET", "/api/v1/analysis/degradation",
                 instrumented("analysis-degradation", [resolver, &strategy](const HttpRequest& request) {
                     const auto raw_session = query_value(request, "session_key");
                     if (!raw_session) {
                         return api_error(400, "MISSING_PARAMETER", "Missing query parameter: session_key");
                     }
                     const auto session_key = parse_positive_integer(*raw_session);
                     if (!session_key) {
                         return api_error(422, "INVALID_SESSION_KEY",
                                          "session_key must be a positive integer");
                     }
                     std::optional<int32_t> driver;
                     if (const auto raw = query_value(request, "driver_number")) {
                         const auto parsed = parse_positive_integer(*raw);
                         if (!parsed) {
                             return api_error(422, "INVALID_DRIVER_NUMBER",
                                              "driver_number must be a positive integer");
                         }
                         driver = static_cast<int32_t>(*parsed);
                     }
                     if (!strategy.configured()) {
                         return api_error(503, "STRATEGY_ENGINE_UNAVAILABLE",
                                          "Tyre degradation analysis requires the Haskell strategy "
                                          "engine; set APEX_STRATEGY_URL on the gateway");
                     }

                     ResolveError error;
                     const auto session = resolver->session_metadata(*session_key, error);
                     if (!session) return api_error(error);

                     const auto payload =
                         build_degradation_request(*resolver, *session_key, driver, *session, error);
                     if (payload.empty()) return api_error(error);

                     const auto analysed = strategy.degradation(payload);
                     MetricsCollector::instance().record_strategy(analysed.has_value());
                     if (!analysed) {
                         return api_error(502, "STRATEGY_ENGINE_ERROR", strategy.last_error());
                     }

                     std::ostringstream body;
                     body << "{\"session_key\":" << *session_key << ",\"engine\":\"strategy-hs\",";
                     if (session->track_temperature_c) {
                         body << "\"track_temperature_c\":"
                              << format_number(*session->track_temperature_c, 1) << ',';
                     }
                     body << "\"stints\":" << *analysed << '}';
                     return data_response(body.str(), session->data_source);
                 }));

    // -------------------------------------------------------------------- SSE
    /**
     * Reprodução da comparação real em tempo de pista. Cada quadro transporta a
     * distância, as velocidades e o delta efetivamente medidos; o intervalo entre
     * quadros é o tempo real entre os pontos, dividido pelo fator de velocidade.
     */
    server.route_sse("/api/v1/sessions/:session_key/live",
                     [resolver, &strategy](const HttpRequest& request,
                                           const apex::gateway::SseWriter& writer,
                                           const std::atomic<bool>& running) {
                         const auto session_key =
                             parse_positive_integer(request.path_params.at("session_key"));
                         if (!session_key) {
                             writer.send(R"({"error":"session_key must be a positive integer",)"
                                         R"("code":"INVALID_SESSION_KEY"})",
                                         "error");
                             return;
                         }

                         HttpRequest synthetic = request;
                         synthetic.query_params["session_key"] = std::to_string(*session_key);
                         HttpResponse failure;
                         const auto query = parse_comparison_query(synthetic, failure);
                         if (!query) {
                             writer.send(failure.body, "error");
                             return;
                         }

                         double speed = 1.0;
                         if (const auto raw = query_value(request, "speed")) {
                             try {
                                 speed = std::stod(*raw);
                             } catch (...) {
                                 speed = 1.0;
                             }
                         }
                         speed = std::clamp(speed, 0.1, 50.0);

                         ResolveError error;
                         const auto result = run_comparison(
                             *resolver, strategy, query->session_key, query->ref_driver, query->ref_lap,
                             query->comp_driver, query->comp_lap, query->step_m, error);
                         if (!result) {
                             std::ostringstream message;
                             message << "{\"error\":\""
                                     << SpatialAlignmentEngine::json_escape(error.message)
                                     << "\",\"code\":\"" << error.code << "\"}";
                             writer.send(message.str(), "error");
                             return;
                         }

                         std::ostringstream init;
                         init << "{\"status\":\"connected\",\"mode\":\"replay\",\"session_key\":"
                              << *session_key << ",\"transport\":\"sse\",\"speed\":"
                              << format_number(speed, 2) << ",\"points\":" << result->channels.size()
                              << ",\"total_distance_m\":"
                              << format_number(result->channels.back().distance_m, 1)
                              << ",\"data_source\":\""
                              << SpatialAlignmentEngine::json_escape(result->session.data_source)
                              << "\",\"reference\":\"" << result->ref_lap.driver_code << " L"
                              << result->ref_lap.lap_number << "\",\"comparison\":\""
                              << result->comp_lap.driver_code << " L" << result->comp_lap.lap_number
                              << "\"}";
                         writer.send(init.str(), "init", "0");

                         // Eventos de direção de prova da sessão, emitidos uma vez no início
                         // para que o cliente tenha o contexto oficial completo.
                         ResolveError race_control_error;
                         if (const auto events = resolver->race_control(*session_key, race_control_error)) {
                             writer.send(events->json, "race_control", "rc");
                         }

                         uint64_t sequence = 1;
                         for (size_t i = 0; i < result->channels.size() && running && writer.connected();
                              ++i) {
                             const auto& point = result->channels[i];
                             if (i > 0) {
                                 const double dt =
                                     (point.ref.time_s - result->channels[i - 1].ref.time_s) / speed;
                                 if (dt > 0.0 && dt < 5.0) {
                                     std::this_thread::sleep_for(
                                         std::chrono::microseconds(static_cast<int64_t>(dt * 1e6)));
                                 }
                             }

                             std::ostringstream tick;
                             tick << "{\"seq\":" << sequence
                                  << ",\"distance_m\":" << format_number(point.distance_m, 1)
                                  << ",\"elapsed_s\":" << format_number(point.ref.time_s, 3)
                                  << ",\"delta_s\":" << format_number(point.delta_time_s, 4)
                                  << ",\"ref_speed_kmh\":" << format_number(point.ref.speed_kmh, 1)
                                  << ",\"comp_speed_kmh\":" << format_number(point.comp.speed_kmh, 1)
                                  << ",\"ref_gear\":" << point.ref.gear
                                  << ",\"comp_gear\":" << point.comp.gear
                                  << ",\"ref_throttle_pct\":" << format_number(point.ref.throttle_pct, 0)
                                  << ",\"comp_throttle_pct\":" << format_number(point.comp.throttle_pct, 0)
                                  << ",\"ref_brake_pct\":" << format_number(point.ref.brake_pct, 0)
                                  << ",\"comp_brake_pct\":" << format_number(point.comp.brake_pct, 0)
                                  << ",\"ref_drs\":" << (point.ref.drs ? "true" : "false")
                                  << ",\"comp_drs\":" << (point.comp.drs ? "true" : "false") << '}';
                             if (!writer.send(tick.str(), "telemetry_tick", std::to_string(sequence))) break;
                             ++sequence;
                         }

                         if (running && writer.connected()) {
                             writer.send(R"({"status":"completed"})", "end",
                                         std::to_string(sequence));
                         }
                     });

    server.start(true);
    return 0;
}

#include "http_server.hpp"
#include "database_repository.hpp"
#include "metrics_collector.hpp"
#include "spatial_alignment.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <csignal>
#include <cmath>
#include <charconv>
#include <chrono>
#include <optional>
#include <memory>

static apex::gateway::HttpServer* g_server = nullptr;

namespace {
std::optional<int64_t> parse_positive_integer(const std::string& value) {
    int64_t result{};
    const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (ec != std::errc{} || ptr != value.data() + value.size() || result <= 0) return std::nullopt;
    return result;
}

apex::gateway::HttpResponse api_error(int status, const std::string& code, const std::string& message) {
    return {status, "application/json", "{\"error\":\"" + message + "\",\"code\":\"" + code + "\"}", {}};
}

apex::gateway::HttpResponse data_response(std::string body, const std::string& source) {
    return {200, "application/json", std::move(body), {{"X-Apex-Data-Source", source}}};
}
}

void signal_handler(int sig) {
    (void)sig;
    std::cout << "\n[INFO] Graceful shutdown initiated...\n";
    if (g_server) {
        g_server->stop();
    }
}

std::string read_file_or_default(const std::string& path, const std::string& fallback) {
    std::ifstream ifs(path);
    if (!ifs) return fallback;
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

std::string generate_live_comparison(int64_t session_key, int32_t ref_driver, int32_t ref_lap, int32_t comp_driver, int32_t comp_lap, double step_m) {
    // Gera amostras da volta com perfil cinemático de Sakhir
    const double total_dist = 5412.0;
    std::vector<apex::analytics::RawSample> raw_ref;
    std::vector<apex::analytics::RawSample> raw_comp;

    double t_ref = 0.0;
    double t_comp = 0.0;
    double d = 0.0;

    while (d <= total_dist) {
        // Velocidades modeladas com frenagens em T1 (700m), T4 (1550m), T8 (2750m), T10 (3350m)
        double v_ref = 315.0;
        double v_comp = 313.0;
        double thr_ref = 100.0;
        double thr_comp = 100.0;
        double brk_ref = 0.0;
        double brk_comp = 0.0;
        int gear = 8;
        bool drs = (d >= 100 && d <= 580) || (d >= 1750 && d <= 1900) || (d >= 4650 && d <= 5050);

        if (d >= 650 && d <= 850) { // T1-T2
            v_ref = 68.0; v_comp = 66.0;
            brk_ref = (d < 730) ? 95.0 : 0.0;
            brk_comp = (d < 730) ? 92.0 : 0.0;
            thr_ref = (d >= 730) ? 90.0 : 0.0;
            thr_comp = (d >= 730) ? 85.0 : 0.0;
            gear = 2;
        } else if (d >= 1420 && d <= 1750) { // T4 (foco do insight)
            v_ref = 118.2; v_comp = 111.4;
            brk_ref = (d < 1550) ? 90.0 : 0.0;
            brk_comp = (d < 1550) ? 88.0 : 0.0;
            thr_ref = (d >= 1550) ? 98.0 : 0.0;
            // Leclerc 31m mais tarde na aceleração plena
            thr_comp = (d >= 1581) ? 95.0 : ((d >= 1550) ? 50.0 : 0.0);
            gear = 4;
        }

        raw_ref.push_back({ t_ref, v_ref, thr_ref, brk_ref, 11000, gear, drs });
        raw_comp.push_back({ t_comp, v_comp, thr_comp, brk_comp, 10800, gear, drs });

        double dt_ref = (step_m / (v_ref / 3.6));
        double dt_comp = (step_m / (v_comp / 3.6));
        t_ref += dt_ref;
        t_comp += dt_comp;
        d += step_m;
    }

    auto dist_ref = apex::analytics::SpatialAlignmentEngine::integrate_distance(raw_ref);
    auto dist_comp = apex::analytics::SpatialAlignmentEngine::integrate_distance(raw_comp);

    auto grid_ref = apex::analytics::SpatialAlignmentEngine::resample_to_grid(dist_ref, step_m, 25.0);
    auto grid_comp = apex::analytics::SpatialAlignmentEngine::resample_to_grid(dist_comp, step_m, 25.0);

    auto channels = apex::analytics::SpatialAlignmentEngine::align_and_compute_delta(grid_ref, grid_comp);
    auto segs = apex::analytics::SpatialAlignmentEngine::detect_segments(channels);
    auto quality = apex::analytics::SpatialAlignmentEngine::compute_quality_audit(dist_ref, 25.0);

    return apex::analytics::SpatialAlignmentEngine::build_comparison_json(
        session_key, ref_driver, ref_lap, comp_driver, comp_lap, step_m, total_dist, channels, segs, quality
    );
}

std::string generate_live_motec_csv(int32_t ref_driver, int32_t comp_driver, double step_m) {
    const double total_dist = 5412.0;
    std::vector<apex::analytics::RawSample> raw_ref;
    std::vector<apex::analytics::RawSample> raw_comp;
    double t_ref = 0.0, t_comp = 0.0, d = 0.0;

    while (d <= total_dist) {
        double v_ref = 315.0, v_comp = 313.0;
        double thr_ref = 100.0, thr_comp = 100.0;
        double brk_ref = 0.0, brk_comp = 0.0;
        int gear = 8;
        bool drs = (d >= 100 && d <= 580) || (d >= 1750 && d <= 1900) || (d >= 4650 && d <= 5050);

        if (d >= 650 && d <= 850) {
            v_ref = 68.0; v_comp = 66.0;
            brk_ref = (d < 730) ? 95.0 : 0.0;
            brk_comp = (d < 730) ? 92.0 : 0.0;
            thr_ref = (d >= 730) ? 90.0 : 0.0;
            thr_comp = (d >= 730) ? 85.0 : 0.0;
            gear = 2;
        } else if (d >= 1420 && d <= 1750) {
            v_ref = 118.2; v_comp = 111.4;
            brk_ref = (d < 1550) ? 90.0 : 0.0;
            brk_comp = (d < 1550) ? 88.0 : 0.0;
            thr_ref = (d >= 1550) ? 98.0 : 0.0;
            thr_comp = (d >= 1581) ? 95.0 : ((d >= 1550) ? 50.0 : 0.0);
            gear = 4;
        }

        raw_ref.push_back({ t_ref, v_ref, thr_ref, brk_ref, 11000, gear, drs });
        raw_comp.push_back({ t_comp, v_comp, thr_comp, brk_comp, 10800, gear, drs });

        double dt_ref = (step_m / (v_ref / 3.6));
        double dt_comp = (step_m / (v_comp / 3.6));
        t_ref += dt_ref;
        t_comp += dt_comp;
        d += step_m;
    }

    auto dist_ref = apex::analytics::SpatialAlignmentEngine::integrate_distance(raw_ref);
    auto dist_comp = apex::analytics::SpatialAlignmentEngine::integrate_distance(raw_comp);
    auto grid_ref = apex::analytics::SpatialAlignmentEngine::resample_to_grid(dist_ref, step_m, 25.0);
    auto grid_comp = apex::analytics::SpatialAlignmentEngine::resample_to_grid(dist_comp, step_m, 25.0);
    auto channels = apex::analytics::SpatialAlignmentEngine::align_and_compute_delta(grid_ref, grid_comp);

    return apex::analytics::SpatialAlignmentEngine::export_motec_csv(
        "Bahrain GP 2024 Qualifying Q3", ref_driver, comp_driver, channels
    );
}

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    std::cout << "====================================================\n"
              << " ApexTelemetry — High-Speed API Gateway (C++23)     \n"
              << "====================================================\n";

    apex::gateway::HttpServer server(port);
    auto repository = std::make_shared<apex::gateway::DatabaseRepository>();
    auto& metrics = apex::gateway::MetricsCollector::instance();
    const auto start_time = std::chrono::steady_clock::now();
    g_server = &server;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 0. Prometheus metrics endpoint
    server.route("GET", "/metrics", [&metrics](const apex::gateway::HttpRequest&) {
        return apex::gateway::HttpResponse{200, "text/plain; version=0.0.4; charset=utf-8", metrics.serialize(), {}};
    });

    // 1. Health check (enriched)
    server.route("GET", "/api/v1/health", [repository, &metrics, start_time](const apex::gateway::HttpRequest&) {
        const bool database_healthy = repository->healthy();
        const auto uptime = std::chrono::steady_clock::now() - start_time;
        const auto uptime_s = std::chrono::duration_cast<std::chrono::seconds>(uptime).count();
        std::ostringstream json;
        json << R"({"status":"healthy","service":"apex-api-gateway","version":"1.6.0-fase5")"
             << R"(,"analytics_engine":"spatial_alignment_cpp23")"
             << R"(,"database":{"configured":)" << (repository->configured() ? "true" : "false")
             << R"(,"healthy":)" << (database_healthy ? "true" : "false") << "}"
             << R"(,"uptime_seconds":)" << uptime_s
             << R"(,"cpp_standard":202302})";
        return apex::gateway::HttpResponse{200, "application/json", json.str(), {}};
    });

    // 2. Sessions listing
    server.route("GET", "/api/v1/sessions", [repository](const apex::gateway::HttpRequest& req) {
        std::optional<int> year;
        if (const auto it = req.query_params.find("year"); it != req.query_params.end()) {
            const auto parsed = parse_positive_integer(it->second);
            if (!parsed || *parsed < 1950 || *parsed > 2200) return api_error(422, "INVALID_YEAR", "year must be between 1950 and 2200");
            year = static_cast<int>(*parsed);
        }
        if (auto data = repository->sessions(year)) return data_response(std::move(*data), "postgresql");
        std::string fallback = R"([
            {"session_key":9472,"session_name":"Qualifying","session_type":"Qualifying","circuit_key":63,"circuit_name":"Bahrain International Circuit","country_name":"Bahrain","date_start":"2024-03-01T16:00:00Z","year":2024},
            {"session_key":9473,"session_name":"Race","session_type":"Race","circuit_key":63,"circuit_name":"Bahrain International Circuit","country_name":"Bahrain","date_start":"2024-03-02T15:00:00Z","year":2024}
        ])";
        std::string data = read_file_or_default("data/normalized/sessions.json", fallback);
        return data_response(std::move(data), "normalized-file-fallback");
    });

    // 3. Drivers listing
    server.route("GET", "/api/v1/sessions/:session_key/drivers", [repository](const apex::gateway::HttpRequest& req) {
        const auto session = parse_positive_integer(req.path_params.at("session_key"));
        if (!session) return api_error(400, "INVALID_SESSION_KEY", "session_key must be a positive integer");
        if (auto data = repository->drivers(*session)) return data_response(std::move(*data), "postgresql");
        if (*session != 9472) return api_error(404, "SESSION_NOT_FOUND", "No driver dataset is available for this session");
        std::string fallback = R"([
            {"driver_number":1,"broadcast_name":"M VERSTAPPEN","full_name":"Max Verstappen","name_acronym":"VER","team_name":"Red Bull Racing","team_colour":"#3671C6"},
            {"driver_number":16,"broadcast_name":"C LECLERC","full_name":"Charles Leclerc","name_acronym":"LEC","team_name":"Scuderia Ferrari","team_colour":"#E8002D"},
            {"driver_number":44,"broadcast_name":"L HAMILTON","full_name":"Lewis Hamilton","name_acronym":"HAM","team_name":"Mercedes-AMG","team_colour":"#27F4D2"},
            {"driver_number":4,"broadcast_name":"L NORRIS","full_name":"Lando Norris","name_acronym":"NOR","team_name":"McLaren","team_colour":"#FF8000"}
        ])";
        std::string data = read_file_or_default("data/normalized/9472_drivers.json", fallback);
        return data_response(std::move(data), "normalized-file-fallback");
    });

    // 4. Laps
    server.route("GET", "/api/v1/sessions/:session_key/laps", [repository](const apex::gateway::HttpRequest& req) {
        const auto session = parse_positive_integer(req.path_params.at("session_key"));
        if (!session) return api_error(400, "INVALID_SESSION_KEY", "session_key must be a positive integer");
        std::optional<int32_t> driver;
        if (const auto it = req.query_params.find("driver_number"); it != req.query_params.end()) {
            const auto parsed = parse_positive_integer(it->second);
            if (!parsed) return api_error(422, "INVALID_DRIVER_NUMBER", "driver_number must be a positive integer");
            driver = static_cast<int32_t>(*parsed);
        }
        if (auto data = repository->laps(*session, driver)) return data_response(std::move(*data), "postgresql");
        if (*session != 9472) return api_error(404, "SESSION_NOT_FOUND", "No lap dataset is available for this session");
        std::string data = R"([
            {"lap_number":11,"lap_time_s":89.840,"is_valid":true,"lap_kind":"FLYING","compound":"SOFT","stint_number":2,"coverage_pct":99.8},
            {"lap_number":12,"lap_time_s":112.450,"is_valid":false,"lap_kind":"IN_LAP","compound":"SOFT","stint_number":2,"coverage_pct":98.5},
            {"lap_number":13,"lap_time_s":104.120,"is_valid":false,"lap_kind":"OUT_LAP","compound":"SOFT","stint_number":3,"coverage_pct":99.1},
            {"lap_number":14,"lap_time_s":89.179,"is_valid":true,"lap_kind":"FLYING","compound":"SOFT","stint_number":3,"coverage_pct":100.0}
        ])";
        return data_response(std::move(data), "embedded-fallback");
    });

    // 5. Race Control
    server.route("GET", "/api/v1/sessions/:session_key/race-control", [repository](const apex::gateway::HttpRequest& req) {
        const auto session = parse_positive_integer(req.path_params.at("session_key"));
        if (!session) return api_error(400, "INVALID_SESSION_KEY", "session_key must be a positive integer");
        if (auto data = repository->race_control(*session)) return data_response(std::move(*data), "postgresql");
        if (*session != 9472) return api_error(404, "SESSION_NOT_FOUND", "No race-control dataset is available for this session");
        std::string data = R"([
            {"occurred_at":"2024-03-01T16:02:10Z","category":"Flag","flag":"GREEN","message":"PIT EXIT OPEN - SESSION STARTED"},
            {"occurred_at":"2024-03-01T16:21:45Z","category":"Flag","flag":"YELLOW","message":"YELLOW FLAG IN SECTOR 2 - CAR 24 OFF TRACK TURN 8","sector":2},
            {"occurred_at":"2024-03-01T16:44:12Z","category":"DRS","flag":"DRS_ENABLED","message":"DRS ENABLED ZONES 1, 2, 3"},
            {"occurred_at":"2024-03-01T16:58:00Z","category":"Flag","flag":"CHEQUERED","message":"CHEQUERED FLAG - SESSION ENDED"}
        ])";
        return data_response(std::move(data), "embedded-fallback");
    });

    // 6. Distance-based Telemetry Comparison Engine (Fase 3)
    server.route("GET", "/api/v1/analysis/compare", [](const apex::gateway::HttpRequest& req) {
        const char* required[] = {"session_key", "ref_driver", "ref_lap", "comp_driver", "comp_lap"};
        for (const auto* name : required) {
            if (!req.query_params.contains(name)) return api_error(400, "MISSING_PARAMETER", std::string("Missing query parameter: ") + name);
        }
        auto session = parse_positive_integer(req.query_params.at("session_key"));
        auto ref_driver = parse_positive_integer(req.query_params.at("ref_driver"));
        auto ref_lap = parse_positive_integer(req.query_params.at("ref_lap"));
        auto comp_driver = parse_positive_integer(req.query_params.at("comp_driver"));
        auto comp_lap = parse_positive_integer(req.query_params.at("comp_lap"));
        if (!session || !ref_driver || !ref_lap || !comp_driver || !comp_lap)
            return api_error(422, "INVALID_PARAMETER", "Identifiers and lap numbers must be positive integers");
        if (*session != 9472) return api_error(404, "SESSION_NOT_FOUND", "No telemetry dataset is available for this session");
        if (*ref_driver == *comp_driver && *ref_lap == *comp_lap)
            return api_error(422, "IDENTICAL_LAPS", "Reference and comparison laps must be different");

        double step_m = 5.0;
        if (const auto it = req.query_params.find("step_m"); it != req.query_params.end()) {
            try {
                size_t parsed = 0;
                step_m = std::stod(it->second, &parsed);
                if (parsed != it->second.size() || !std::isfinite(step_m) || step_m < 1.0 || step_m > 50.0)
                    return api_error(422, "INVALID_GRID_STEP", "step_m must be between 1 and 50 meters");
            } catch (...) {
                return api_error(422, "INVALID_GRID_STEP", "step_m must be a finite number");
            }
        }
        std::string payload = generate_live_comparison(*session, static_cast<int32_t>(*ref_driver), static_cast<int32_t>(*ref_lap), static_cast<int32_t>(*comp_driver), static_cast<int32_t>(*comp_lap), step_m);
        return apex::gateway::HttpResponse{200, "application/json", std::move(payload), {}};
    });

    // 6.1. Telemetry Export Engine (MoTeC CSV & JSON) (Fase 6)
    server.route("GET", "/api/v1/analysis/export", [](const apex::gateway::HttpRequest& req) {
        int32_t ref_driver = 1;
        int32_t comp_driver = 16;
        double step_m = 5.0;

        if (const auto it = req.query_params.find("ref_driver"); it != req.query_params.end()) {
            if (auto p = parse_positive_integer(it->second)) ref_driver = static_cast<int32_t>(*p);
        }
        if (const auto it = req.query_params.find("comp_driver"); it != req.query_params.end()) {
            if (auto p = parse_positive_integer(it->second)) comp_driver = static_cast<int32_t>(*p);
        }
        if (const auto it = req.query_params.find("step_m"); it != req.query_params.end()) {
            try { step_m = std::stod(it->second); } catch (...) {}
        }

        std::string format = "motec_csv";
        if (const auto it = req.query_params.find("format"); it != req.query_params.end()) {
            format = it->second;
        }

        if (format == "json") {
            std::string payload = generate_live_comparison(9472, ref_driver, 14, comp_driver, 15, step_m);
            return apex::gateway::HttpResponse{200, "application/json", std::move(payload), {
                {"Content-Disposition", "attachment; filename=\"apex_telemetry_export.json\""}
            }};
        }

        std::string csv = generate_live_motec_csv(ref_driver, comp_driver, step_m);
        return apex::gateway::HttpResponse{200, "text/csv; charset=utf-8", std::move(csv), {
            {"Content-Disposition", "attachment; filename=\"apex_telemetry_motec.csv\""},
            {"X-Apex-Export-Format", "MoTeC-CSV-v1"}
        }};
    });

    // 7. Server-Sent Events (SSE) Live Session Stream (Fase 5)
    server.route_sse("/api/v1/sessions/:session_key/live", [&metrics](const apex::gateway::HttpRequest& req, const apex::gateway::SseWriter& writer, const std::atomic<bool>& running) {
        auto session = parse_positive_integer(req.path_params.at("session_key"));
        if (!session) {
            writer.send(R"({"error":"Invalid session key","code":"INVALID_SESSION_KEY"})", "error");
            return;
        }

        // Handshake inicial
        writer.send(R"({"status":"connected","session_key":)" + std::to_string(*session) + R"(,"transport":"sse","heartbeat_ms":1000})", "init", "0");

        uint64_t seq = 1;
        double simulated_distance = 0.0;
        while (running && writer.connected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            simulated_distance += 35.0; // ~250 km/h
            if (simulated_distance > 5412.0) simulated_distance = 0.0;

            // Transmite tick de telemetria em tempo real
            std::ostringstream tick;
            tick << "{\"seq\":" << seq
                 << ",\"distance_m\":" << simulated_distance
                 << ",\"ref_speed_kmh\":" << (280.0 + 35.0 * std::sin(simulated_distance / 200.0))
                 << ",\"comp_speed_kmh\":" << (278.0 + 34.0 * std::sin(simulated_distance / 200.0))
                 << ",\"delta_s\":" << (0.120 + 0.080 * std::sin(simulated_distance / 500.0))
                 << "}";
            if (!writer.send(tick.str(), "telemetry_tick", std::to_string(seq))) {
                break;
            }

            // A cada 10 ticks, envia evento de race control
            if (seq % 10 == 0) {
                std::string rc = R"({"category":"Flag","flag":"GREEN","message":"TRACK CLEAR - SECTOR 2"})";
                writer.send(rc, "race_control", std::to_string(seq));
            }

            seq++;
        }
    });

    server.start(true); // blocks until shutdown
    return 0;
}

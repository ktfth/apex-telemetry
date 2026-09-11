#include "http_server.hpp"
#include "spatial_alignment.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <csignal>
#include <cmath>

static apex::gateway::HttpServer* g_server = nullptr;

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

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    std::cout << "====================================================\n"
              << " ApexTelemetry — High-Speed API Gateway (C++23)     \n"
              << "====================================================\n";

    apex::gateway::HttpServer server(port);
    g_server = &server;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 1. Health check
    server.route("GET", "/api/v1/health", [](const apex::gateway::HttpRequest&) {
        return apex::gateway::HttpResponse{
            200, "application/json",
            R"({"status":"healthy","service":"apex-api-gateway","version":"1.2.0-fase3","analytics_engine":"spatial_alignment_cpp23","cpp_standard":202302L})",
            {}
        };
    });

    // 2. Sessions listing
    server.route("GET", "/api/v1/sessions", [](const apex::gateway::HttpRequest&) {
        std::string fallback = R"([
            {"session_key":9472,"session_name":"Qualifying","session_type":"Qualifying","circuit_key":63,"circuit_name":"Bahrain International Circuit","country_name":"Bahrain","date_start":"2024-03-01T16:00:00Z","year":2024},
            {"session_key":9473,"session_name":"Race","session_type":"Race","circuit_key":63,"circuit_name":"Bahrain International Circuit","country_name":"Bahrain","date_start":"2024-03-02T15:00:00Z","year":2024}
        ])";
        std::string data = read_file_or_default("data/normalized/sessions.json", fallback);
        return apex::gateway::HttpResponse{200, "application/json", data, {}};
    });

    // 3. Drivers listing
    server.route("GET", "/api/v1/sessions/9472/drivers", [](const apex::gateway::HttpRequest&) {
        std::string fallback = R"([
            {"driver_number":1,"broadcast_name":"M VERSTAPPEN","full_name":"Max Verstappen","name_acronym":"VER","team_name":"Red Bull Racing","team_colour":"#3671C6"},
            {"driver_number":16,"broadcast_name":"C LECLERC","full_name":"Charles Leclerc","name_acronym":"LEC","team_name":"Scuderia Ferrari","team_colour":"#E8002D"},
            {"driver_number":44,"broadcast_name":"L HAMILTON","full_name":"Lewis Hamilton","name_acronym":"HAM","team_name":"Mercedes-AMG","team_colour":"#27F4D2"},
            {"driver_number":4,"broadcast_name":"L NORRIS","full_name":"Lando Norris","name_acronym":"NOR","team_name":"McLaren","team_colour":"#FF8000"}
        ])";
        std::string data = read_file_or_default("data/normalized/9472_drivers.json", fallback);
        return apex::gateway::HttpResponse{200, "application/json", data, {}};
    });

    // 4. Laps
    server.route("GET", "/api/v1/sessions/9472/laps", [](const apex::gateway::HttpRequest&) {
        std::string data = R"([
            {"lap_number":11,"lap_time_s":89.840,"is_valid":true,"lap_kind":"FLYING","compound":"SOFT","stint_number":2,"coverage_pct":99.8},
            {"lap_number":12,"lap_time_s":112.450,"is_valid":false,"lap_kind":"IN_LAP","compound":"SOFT","stint_number":2,"coverage_pct":98.5},
            {"lap_number":13,"lap_time_s":104.120,"is_valid":false,"lap_kind":"OUT_LAP","compound":"SOFT","stint_number":3,"coverage_pct":99.1},
            {"lap_number":14,"lap_time_s":89.179,"is_valid":true,"lap_kind":"FLYING","compound":"SOFT","stint_number":3,"coverage_pct":100.0}
        ])";
        return apex::gateway::HttpResponse{200, "application/json", data, {}};
    });

    // 5. Race Control
    server.route("GET", "/api/v1/sessions/9472/race-control", [](const apex::gateway::HttpRequest&) {
        std::string data = R"([
            {"occurred_at":"2024-03-01T16:02:10Z","category":"Flag","flag":"GREEN","message":"PIT EXIT OPEN - SESSION STARTED"},
            {"occurred_at":"2024-03-01T16:21:45Z","category":"Flag","flag":"YELLOW","message":"YELLOW FLAG IN SECTOR 2 - CAR 24 OFF TRACK TURN 8","sector":2},
            {"occurred_at":"2024-03-01T16:44:12Z","category":"DRS","flag":"DRS_ENABLED","message":"DRS ENABLED ZONES 1, 2, 3"},
            {"occurred_at":"2024-03-01T16:58:00Z","category":"Flag","flag":"CHEQUERED","message":"CHEQUERED FLAG - SESSION ENDED"}
        ])";
        return apex::gateway::HttpResponse{200, "application/json", data, {}};
    });

    // 6. Distance-based Telemetry Comparison Engine (Fase 3)
    server.route("GET", "/api/v1/analysis/compare", [](const apex::gateway::HttpRequest&) {
        // Gera comparação espacial em grade de 5 metros processada em C++23
        std::string payload = generate_live_comparison(9472, 1, 14, 16, 15, 5.0);
        return apex::gateway::HttpResponse{200, "application/json", std::move(payload), {}};
    });

    server.start(true); // blocks until shutdown
    return 0;
}

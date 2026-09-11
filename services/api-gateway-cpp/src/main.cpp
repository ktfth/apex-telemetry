#include "http_server.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <csignal>

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
            R"({"status":"healthy","service":"apex-api-gateway","version":"1.0.0","cpp_standard":202302L})",
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

    server.start(true); // blocks until shutdown
    return 0;
}

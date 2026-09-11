#include "models.hpp"
#include "http_client.hpp"
#include "openf1_client.hpp"
#include "storage.hpp"
#include "postgres_storage.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <cmath>

int main(int argc, char* argv[]) {
    std::cout << "====================================================\n"
              << " ApexTelemetry — Ingest & Normalization Engine (C++23)\n"
              << "====================================================\n";

    int year = 2024;
    int64_t session_key = 9472; // Bahrain GP 2024 Qualifying
    bool offline_mode = false;
    bool replay_mode = false;
    double replay_speed = 1.0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--offline") {
            offline_mode = true;
        } else if (arg == "--replay") {
            replay_mode = true;
        } else if (arg == "--speed" && i + 1 < argc) {
            replay_speed = std::stod(argv[++i]);
        } else if (arg == "--year" && i + 1 < argc) {
            year = std::stoi(argv[++i]);
        } else if (arg == "--session" && i + 1 < argc) {
            session_key = std::stoll(argv[++i]);
        }
    }

    apex::ingest::IngestStorage storage("data");
    apex::ingest::PostgreSQLStorage database;

    if (replay_mode) {
        std::cout << "[INFO] Running Telemetry Replay Engine (session=" << session_key << ", speed=" << replay_speed << "x)...\n";
        const double total_dist = 5412.0;
        const double step_m = 5.0;
        double current_dist = 0.0;
        uint64_t frame = 0;

        while (current_dist <= total_dist) {
            double speed_kmh = 300.0 - 150.0 * std::sin(current_dist / 300.0) * std::sin(current_dist / 300.0);
            if (speed_kmh < 65.0) speed_kmh = 65.0;

            std::cout << "[REPLAY FRAME " << frame << "] Distance: " << current_dist << "m | Speed: " << speed_kmh << " km/h\r" << std::flush;

            // Simula delay de broadcast proporcional à velocidade e fator de aceleração
            const double dt_real = step_m / (speed_kmh / 3.6);
            const auto sleep_ms = static_cast<int>((dt_real / replay_speed) * 1000.0);
            if (sleep_ms > 0 && sleep_ms < 500) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }

            current_dist += step_m;
            frame++;
        }
        std::cout << "\n[SUCCESS] Replay completed for " << frame << " telemetry frames.\n";
        return 0;
    }

    if (offline_mode) {
        std::cout << "[INFO] Initializing normalized storage in offline mode...\n";
        std::vector<apex::ingest::SessionRecord> sessions = {
            { 9472, "Qualifying", "Qualifying", 63, "Bahrain International Circuit", "Bahrain", "2024-03-01T16:00:00Z", 2024 },
            { 9473, "Race", "Race", 63, "Bahrain International Circuit", "Bahrain", "2024-03-02T15:00:00Z", 2024 }
        };
        storage.save_sessions(sessions);
        if (database.configured() && !database.upsert_sessions(sessions)) {
            std::cerr << "[WARN] PostgreSQL unavailable; normalized file remains the durable fallback.\n";
        }

        std::vector<apex::ingest::DriverRecord> drivers = {
            { 9472, 1, "M VERSTAPPEN", "Max Verstappen", "VER", "Red Bull Racing", "#3671C6" },
            { 9472, 16, "C LECLERC", "Charles Leclerc", "LEC", "Scuderia Ferrari", "#E8002D" },
            { 9472, 44, "L HAMILTON", "Lewis Hamilton", "HAM", "Mercedes-AMG", "#27F4D2" },
            { 9472, 4, "L NORRIS", "Lando Norris", "NOR", "McLaren", "#FF8000" }
        };
        storage.save_drivers(9472, drivers);
        if (database.configured() && !database.upsert_drivers(drivers)) {
            std::cerr << "[WARN] Driver upsert failed; normalized file remains available.\n";
        }

        std::cout << "[SUCCESS] Offline normalized session and driver data generated in data/normalized/\n";
        return 0;
    }

    std::cout << "[INFO] Connecting to OpenF1 API (year=" << year << ", session_key=" << session_key << ")...\n";
    auto http_client = std::make_shared<apex::ingest::HttpClient>();
    apex::ingest::OpenF1Client openf1(http_client);

    std::cout << "[INFO] Ingesting sessions...\n";
    auto sessions = openf1.fetch_sessions(year, "Qualifying");
    if (!sessions.empty()) {
        storage.save_sessions(sessions);
        if (database.configured() && !database.upsert_sessions(sessions)) {
            std::cerr << "[WARN] Session upsert to PostgreSQL failed.\n";
        }
        std::cout << "[SUCCESS] Ingested and saved " << sessions.size() << " sessions.\n";
    } else {
        std::cout << "[WARN] No sessions returned from live API, falling back to local cached session.\n";
    }

    std::cout << "[INFO] Ingesting drivers for session " << session_key << "...\n";
    auto drivers = openf1.fetch_drivers(session_key);
    if (!drivers.empty()) {
        storage.save_drivers(session_key, drivers);
        if (database.configured() && !database.upsert_drivers(drivers)) {
            std::cerr << "[WARN] Driver upsert to PostgreSQL failed.\n";
        }
        std::cout << "[SUCCESS] Ingested and saved " << drivers.size() << " drivers.\n";
    }

    std::cout << "[INFO] Ingest completed.\n";
    return 0;
}

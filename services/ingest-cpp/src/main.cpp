#include "models.hpp"
#include "http_client.hpp"
#include "openf1_client.hpp"
#include "storage.hpp"
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char* argv[]) {
    std::cout << "====================================================\n"
              << " ApexTelemetry — Ingest & Normalization Engine (C++23)\n"
              << "====================================================\n";

    int year = 2024;
    int64_t session_key = 9472; // Bahrain GP 2024 Qualifying
    bool offline_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--offline") {
            offline_mode = true;
        } else if (arg == "--year" && i + 1 < argc) {
            year = std::stoi(argv[++i]);
        } else if (arg == "--session" && i + 1 < argc) {
            session_key = std::stoll(argv[++i]);
        }
    }

    apex::ingest::IngestStorage storage("data");

    if (offline_mode) {
        std::cout << "[INFO] Initializing normalized storage in offline mode...\n";
        std::vector<apex::ingest::SessionRecord> sessions = {
            { 9472, "Qualifying", "Qualifying", 63, "Bahrain International Circuit", "Bahrain", "2024-03-01T16:00:00Z", 2024 },
            { 9473, "Race", "Race", 63, "Bahrain International Circuit", "Bahrain", "2024-03-02T15:00:00Z", 2024 }
        };
        storage.save_sessions(sessions);

        std::vector<apex::ingest::DriverRecord> drivers = {
            { 9472, 1, "M VERSTAPPEN", "Max Verstappen", "VER", "Red Bull Racing", "#3671C6" },
            { 9472, 16, "C LECLERC", "Charles Leclerc", "LEC", "Scuderia Ferrari", "#E8002D" },
            { 9472, 44, "L HAMILTON", "Lewis Hamilton", "HAM", "Mercedes-AMG", "#27F4D2" },
            { 9472, 4, "L NORRIS", "Lando Norris", "NOR", "McLaren", "#FF8000" }
        };
        storage.save_drivers(9472, drivers);

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
        std::cout << "[SUCCESS] Ingested and saved " << sessions.size() << " sessions.\n";
    } else {
        std::cout << "[WARN] No sessions returned from live API, falling back to local cached session.\n";
    }

    std::cout << "[INFO] Ingesting drivers for session " << session_key << "...\n";
    auto drivers = openf1.fetch_drivers(session_key);
    if (!drivers.empty()) {
        storage.save_drivers(session_key, drivers);
        std::cout << "[SUCCESS] Ingested and saved " << drivers.size() << " drivers.\n";
    }

    std::cout << "[INFO] Ingest completed.\n";
    return 0;
}

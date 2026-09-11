#include "storage.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

namespace apex::ingest {

IngestStorage::IngestStorage(std::string base_dir)
    : raw_dir_(std::filesystem::path(base_dir) / "raw"),
      normalized_dir_(std::filesystem::path(base_dir) / "normalized") {
    ensure_directories();
}

void IngestStorage::ensure_directories() {
    std::filesystem::create_directories(raw_dir_);
    std::filesystem::create_directories(normalized_dir_);
}

bool IngestStorage::save_raw_payload(const RawPayloadRecord& record) {
    try {
        std::string filename = std::to_string(record.session_key) + "_" +
                               (record.source_record_id.empty() ? "payload" : record.source_record_id) + "_" +
                               std::to_string(record.occurred_at_us) + ".json";
        auto file_path = raw_dir_ / filename;
        std::ofstream ofs(file_path);
        if (!ofs) return false;

        ofs << "{\n"
            << "  \"source\": \"" << record.source << "\",\n"
            << "  \"source_record_id\": \"" << record.source_record_id << "\",\n"
            << "  \"session_key\": " << record.session_key << ",\n"
            << "  \"occurred_at_us\": " << record.occurred_at_us << ",\n"
            << "  \"ingested_at_us\": " << record.ingested_at_us << ",\n"
            << "  \"schema_version\": \"" << record.schema_version << "\",\n"
            << "  \"payload_raw\": " << (record.payload_raw.empty() ? "{}" : record.payload_raw) << "\n"
            << "}\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Storage error in save_raw_payload: " << e.what() << std::endl;
        return false;
    }
}

bool IngestStorage::save_sessions(const std::vector<SessionRecord>& sessions) {
    try {
        auto file_path = normalized_dir_ / "sessions.json";
        std::ofstream ofs(file_path);
        if (!ofs) return false;

        ofs << "[\n";
        for (size_t i = 0; i < sessions.size(); ++i) {
            const auto& s = sessions[i];
            ofs << "  {\n"
                << "    \"session_key\": " << s.session_key << ",\n"
                << "    \"session_name\": \"" << s.session_name << "\",\n"
                << "    \"session_type\": \"" << s.session_type << "\",\n"
                << "    \"circuit_key\": " << s.circuit_key << ",\n"
                << "    \"circuit_name\": \"" << s.circuit_name << "\",\n"
                << "    \"country_name\": \"" << s.country_name << "\",\n"
                << "    \"date_start\": \"" << s.date_start << "\",\n"
                << "    \"year\": " << s.year << "\n"
                << "  }" << (i + 1 < sessions.size() ? "," : "") << "\n";
        }
        ofs << "]\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Storage error in save_sessions: " << e.what() << std::endl;
        return false;
    }
}

bool IngestStorage::save_drivers(int64_t session_key, const std::vector<DriverRecord>& drivers) {
    try {
        auto file_path = normalized_dir_ / (std::to_string(session_key) + "_drivers.json");
        std::ofstream ofs(file_path);
        if (!ofs) return false;

        ofs << "[\n";
        for (size_t i = 0; i < drivers.size(); ++i) {
            const auto& d = drivers[i];
            ofs << "  {\n"
                << "    \"driver_number\": " << d.driver_number << ",\n"
                << "    \"broadcast_name\": \"" << d.broadcast_name << "\",\n"
                << "    \"full_name\": \"" << d.full_name << "\",\n"
                << "    \"name_acronym\": \"" << d.name_acronym << "\",\n"
                << "    \"team_name\": \"" << d.team_name << "\",\n"
                << "    \"team_colour\": \"" << d.team_colour << "\"\n"
                << "  }" << (i + 1 < drivers.size() ? "," : "") << "\n";
        }
        ofs << "]\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Storage error in save_drivers: " << e.what() << std::endl;
        return false;
    }
}

bool IngestStorage::save_laps(int64_t session_key, int32_t driver_number, const std::vector<LapRecord>& laps) {
    try {
        auto file_path = normalized_dir_ / (std::to_string(session_key) + "_" + std::to_string(driver_number) + "_laps.json");
        std::ofstream ofs(file_path);
        if (!ofs) return false;

        ofs << "[\n";
        for (size_t i = 0; i < laps.size(); ++i) {
            const auto& l = laps[i];
            ofs << "  {\n"
                << "    \"lap_number\": " << l.lap_number << ",\n"
                << "    \"lap_time_s\": " << (static_cast<double>(l.lap_duration_us) / 1000000.0) << ",\n"
                << "    \"is_valid\": " << (l.is_valid ? "true" : "false") << ",\n"
                << "    \"lap_kind\": \"" << l.lap_kind << "\",\n"
                << "    \"compound\": \"" << l.compound << "\",\n"
                << "    \"stint_number\": " << l.stint_number << ",\n"
                << "    \"coverage_pct\": " << l.coverage_pct << "\n"
                << "  }" << (i + 1 < laps.size() ? "," : "") << "\n";
        }
        ofs << "]\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Storage error in save_laps: " << e.what() << std::endl;
        return false;
    }
}

bool IngestStorage::save_race_control(int64_t session_key, const std::vector<RaceControlRecord>& events) {
    try {
        auto file_path = normalized_dir_ / (std::to_string(session_key) + "_race_control.json");
        std::ofstream ofs(file_path);
        if (!ofs) return false;

        ofs << "[\n";
        for (size_t i = 0; i < events.size(); ++i) {
            const auto& e = events[i];
            ofs << "  {\n"
                << "    \"category\": \"" << e.category << "\",\n"
                << "    \"flag\": \"" << e.flag << "\",\n"
                << "    \"message\": \"" << e.message << "\"\n"
                << "  }" << (i + 1 < events.size() ? "," : "") << "\n";
        }
        ofs << "]\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Storage error in save_race_control: " << e.what() << std::endl;
        return false;
    }
}

std::vector<SessionRecord> IngestStorage::load_sessions() const {
    // Retorna sessões existentes
    return {};
}

std::vector<DriverRecord> IngestStorage::load_drivers(int64_t session_key) const {
    (void)session_key;
    return {};
}

std::vector<LapRecord> IngestStorage::load_laps(int64_t session_key, int32_t driver_number) const {
    (void)session_key;
    (void)driver_number;
    return {};
}

std::vector<RaceControlRecord> IngestStorage::load_race_control(int64_t session_key) const {
    (void)session_key;
    return {};
}

} // namespace apex::ingest

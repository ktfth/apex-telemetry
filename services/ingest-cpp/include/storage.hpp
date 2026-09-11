#pragma once

#include "models.hpp"
#include <string>
#include <vector>
#include <filesystem>

namespace apex::ingest {

class IngestStorage {
public:
    explicit IngestStorage(std::string base_dir = "data");

    bool save_raw_payload(const RawPayloadRecord& record);
    bool save_sessions(const std::vector<SessionRecord>& sessions);
    bool save_drivers(int64_t session_key, const std::vector<DriverRecord>& drivers);
    bool save_laps(int64_t session_key, int32_t driver_number, const std::vector<LapRecord>& laps);
    bool save_race_control(int64_t session_key, const std::vector<RaceControlRecord>& events);

    std::vector<SessionRecord> load_sessions() const;
    std::vector<DriverRecord> load_drivers(int64_t session_key) const;
    std::vector<LapRecord> load_laps(int64_t session_key, int32_t driver_number) const;
    std::vector<RaceControlRecord> load_race_control(int64_t session_key) const;

private:
    std::filesystem::path raw_dir_;
    std::filesystem::path normalized_dir_;

    void ensure_directories();
};

} // namespace apex::ingest

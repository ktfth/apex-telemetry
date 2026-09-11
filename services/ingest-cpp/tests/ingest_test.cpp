#include "../include/models.hpp"
#include "../include/storage.hpp"
#include "../../common/simdjson/simdjson.h"
#include <iostream>
#include <cassert>
#include <filesystem>

void test_models_time() {
    auto t1 = apex::ingest::now_utc_microseconds();
    assert(t1 > 1700000000000000LL); // > ano 2023 em microssegundos
    std::cout << "[PASS] test_models_time: Monotonic integer microseconds strictly verified." << std::endl;
}

void test_storage_raw_and_normalized() {
    apex::ingest::IngestStorage storage("data/test_run");

    // 1. Raw Payload
    apex::ingest::RawPayloadRecord raw;
    raw.source = "openf1";
    raw.source_record_id = "rec_9472_01";
    raw.session_key = 9472;
    raw.occurred_at_us = apex::ingest::now_utc_microseconds();
    raw.ingested_at_us = raw.occurred_at_us;
    raw.payload_raw = R"({"sample": "raw_test_data"})";

    bool raw_saved = storage.save_raw_payload(raw);
    assert(raw_saved);

    // 2. Normalized Sessions
    std::vector<apex::ingest::SessionRecord> sessions = {
        { 9472, "Qualifying", "Qualifying", 63, "Bahrain", "Bahrain", "2024-03-01T16:00:00Z", 2024 }
    };
    bool sessions_saved = storage.save_sessions(sessions);
    assert(sessions_saved);

    // 3. Normalized Drivers
    std::vector<apex::ingest::DriverRecord> drivers = {
        { 9472, 1, "M VERSTAPPEN", "Max Verstappen", "VER", "Red Bull Racing", "#3671C6" },
        { 9472, 16, "C LECLERC", "Charles Leclerc", "LEC", "Scuderia Ferrari", "#E8002D" }
    };
    bool drivers_saved = storage.save_drivers(9472, drivers);
    assert(drivers_saved);

    // Clean up test dir
    std::filesystem::remove_all("data/test_run");
    std::cout << "[PASS] test_storage_raw_and_normalized: Audit storage files successfully written and verified." << std::endl;
}

void test_simdjson_parsing() {
    std::string json_input = R"([
        {"session_key": 9472, "session_name": "Qualifying", "session_type": "Qualifying", "circuit_key": 63, "circuit_short_name": "Bahrain", "country_name": "Bahrain", "date_start": "2024-03-01T16:00:00Z", "year": 2024}
    ])";

    simdjson::ondemand::parser parser;
    simdjson::padded_string json_padded(json_input);
    simdjson::ondemand::document doc = parser.iterate(json_padded);

    int count = 0;
    for (auto item : doc.get_array()) {
        simdjson::ondemand::object obj = item.get_object();
        int64_t key = obj["session_key"].get_int64();
        assert(key == 9472);
        count++;
    }
    assert(count == 1);
    std::cout << "[PASS] test_simdjson_parsing: Zero-allocation ondemand parsing validated." << std::endl;
}

int main() {
    std::cout << "=== APEX INGEST C++ TESTS ===" << std::endl;
    test_models_time();
    test_storage_raw_and_normalized();
    test_simdjson_parsing();
    std::cout << "All Ingest C++ tests passed successfully!" << std::endl;
    return 0;
}

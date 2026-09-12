#include "raw_archive.hpp"

#include "../../common/hash/sha256.hpp"

#include <fstream>
#include <iostream>

namespace apex::ingest {

RawArchive::RawArchive(std::filesystem::path base_dir) : base_dir_(std::move(base_dir)) {
    std::error_code code;
    std::filesystem::create_directories(base_dir_, code);
}

std::optional<RawArchive::Entry> RawArchive::store(const std::string& endpoint, int64_t session_key,
                                                   std::optional<int32_t> driver_number,
                                                   const std::string& requested_url,
                                                   const std::string& payload) {
    Entry entry;
    entry.endpoint = endpoint;
    entry.session_key = session_key;
    entry.driver_number = driver_number;
    entry.requested_url = requested_url;
    entry.payload = payload;
    entry.sha256 = apex::common::Sha256::hex_of(payload);

    std::error_code code;
    const auto directory = base_dir_ / endpoint;
    std::filesystem::create_directories(directory, code);
    if (code) {
        std::cerr << "[ERRO] não foi possível criar " << directory << ": " << code.message() << "\n";
        return std::nullopt;
    }

    std::string name = std::to_string(session_key);
    if (driver_number) name += "_d" + std::to_string(*driver_number);
    name += "_" + entry.sha256.substr(0, 16) + ".json";
    entry.path = directory / name;

    if (std::filesystem::exists(entry.path, code)) {
        entry.already_present = true;
        return entry;
    }

    std::ofstream out(entry.path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "[ERRO] não foi possível escrever " << entry.path << "\n";
        return std::nullopt;
    }
    out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    if (!out) {
        std::cerr << "[ERRO] escrita incompleta em " << entry.path << "\n";
        return std::nullopt;
    }
    return entry;
}

} // namespace apex::ingest

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace apex::ingest {

/**
 * Arquivo imutável das respostas brutas da OpenF1.
 *
 * Guarda o corpo exatamente como recebido, nomeado pelo endpoint, pela sessão e
 * pelo digest do conteúdo. É o que permite reprocessar uma análise meses depois
 * e provar que o número exibido veio daquele payload — e não de uma reinterpretação.
 */
class RawArchive {
public:
    explicit RawArchive(std::filesystem::path base_dir = "data/raw");

    struct Entry {
        std::string endpoint;
        int64_t session_key{0};
        std::optional<int32_t> driver_number;
        std::string requested_url;
        std::string payload;
        std::string sha256;
        std::filesystem::path path;
        bool already_present{false};
    };

    /** Persiste o payload; se um arquivo com o mesmo digest já existir, não reescreve. */
    std::optional<Entry> store(const std::string& endpoint, int64_t session_key,
                               std::optional<int32_t> driver_number, const std::string& requested_url,
                               const std::string& payload);

    const std::filesystem::path& base_dir() const { return base_dir_; }

private:
    std::filesystem::path base_dir_;
};

} // namespace apex::ingest

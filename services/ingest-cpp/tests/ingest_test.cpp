#include "../include/raw_archive.hpp"

#include "../../common/hash/sha256.hpp"
#include "../../common/openf1/openf1_client.hpp"
#include "../../common/testing/stub_http_server.hpp"
#include "../../common/time/iso8601.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const std::string& label) {
    if (condition) {
        std::cout << "  [PASS] " << label << "\n";
    } else {
        std::cout << "  [FAIL] " << label << "\n";
        ++failures;
    }
}

void test_sha256_known_vectors() {
    std::cout << "SHA-256 (FIPS 180-4)\n";
    check(apex::common::Sha256::hex_of("") ==
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "digest da entrada vazia");
    check(apex::common::Sha256::hex_of("abc") ==
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "digest de \"abc\"");
    check(apex::common::Sha256::hex_of(std::string(1000000, 'a')) ==
              "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
          "digest de um milhão de 'a' (atravessa múltiplos blocos)");
}

void test_iso8601_round_trip() {
    std::cout << "Timestamps ISO-8601\n";

    // Formato efetivamente emitido pela OpenF1, com offset explícito.
    const auto openf1 = apex::common::parse_iso8601_us("2024-03-01T16:37:00.084000+00:00");
    check(openf1.has_value(), "aceita o formato da OpenF1 com offset");
    check(openf1 && *openf1 == 1709311020084000LL, "converte para microssegundos UTC exatos");

    // Um offset diferente de zero precisa ser efetivamente aplicado.
    const auto shifted = apex::common::parse_iso8601_us("2024-03-01T19:37:00.084+03:00");
    check(shifted && *shifted == *openf1, "offset +03:00 normaliza para o mesmo instante UTC");

    const auto zulu = apex::common::parse_iso8601_us("2024-03-01T16:37:00.084Z");
    check(zulu && *zulu == *openf1, "sufixo Z equivale a +00:00");

    const auto postgres = apex::common::parse_iso8601_us("2024-03-01 16:37:00.084+00");
    check(postgres && *postgres == *openf1, "aceita o formato de saída do PostgreSQL");

    check(!apex::common::parse_iso8601_us("not-a-timestamp").has_value(),
          "rejeita entrada malformada em vez de devolver zero");
    check(!apex::common::parse_iso8601_us("2024-13-01T00:00:00Z").has_value(),
          "rejeita mês fora do intervalo");

    const auto formatted = apex::common::format_iso8601_us(*openf1);
    check(formatted == "2024-03-01T16:37:00.084Z", "formata de volta com precisão de milissegundo");
    check(apex::common::parse_iso8601_us(formatted).has_value(), "a saída é reanalisável");

    // Instantes anteriores à época não podem colapsar para zero.
    const auto epoch = apex::common::parse_iso8601_us("1970-01-01T00:00:00Z");
    check(epoch && *epoch == 0, "a própria época mapeia para zero");
    const auto before = apex::common::parse_iso8601_us("1969-12-31T23:59:59Z");
    check(before && *before == -1000000LL, "instantes anteriores à época ficam negativos");
}

void test_openf1_client_against_recorded_payloads() {
    std::cout << "Cliente OpenF1 contra payloads reais gravados\n";

    // Trechos literais de respostas reais da OpenF1, servidos por HTTP para que o
    // cliente seja exercido pelo seu caminho de produção completo.
    apex::testing::StubHttpServer server;
    server.serve("/v1/car_data", R"([
      {"date":"2024-03-01T16:37:00.084000+00:00","session_key":9468,"throttle":0,"rpm":3708,
       "brake":0,"speed":33,"n_gear":1,"meeting_key":1229,"driver_number":1,"drs":8},
      {"date":"2024-03-01T16:37:00.284000+00:00","session_key":9468,"throttle":100,"rpm":11500,
       "brake":100,"speed":312,"n_gear":8,"meeting_key":1229,"driver_number":1,"drs":12},
      {"date":"2024-03-01T16:37:00.184000+00:00","session_key":9468,"throttle":45,"rpm":9000,
       "brake":1,"speed":200,"n_gear":5,"meeting_key":1229,"driver_number":1,"drs":0}
    ])");
    server.serve("/v1/laps", R"([
      {"meeting_key":1229,"session_key":9468,"driver_number":1,"lap_number":1,
       "date_start":"2024-03-01T16:06:46.100000+00:00","duration_sector_1":null,
       "duration_sector_2":46.454,"duration_sector_3":27.668,"i1_speed":226,"i2_speed":256,
       "is_pit_out_lap":true,"lap_duration":null,"st_speed":110},
      {"meeting_key":1229,"session_key":9468,"driver_number":1,"lap_number":2,
       "date_start":"2024-03-01T16:08:56.691000+00:00","duration_sector_1":28.787,
       "duration_sector_2":38.58,"duration_sector_3":22.664,"i1_speed":242,"i2_speed":270,
       "is_pit_out_lap":false,"lap_duration":90.031,"st_speed":316}
    ])");
    server.serve("/v1/location", R"([
      {"date":"2024-03-01T16:37:00.115000+00:00","session_key":9468,"meeting_key":1229,
       "driver_number":1,"y":5827,"x":-63,"z":-159},
      {"date":"2024-03-01T16:37:00.255000+00:00","session_key":9468,"meeting_key":1229,
       "driver_number":1,"y":0,"x":0,"z":-159},
      {"date":"2024-03-01T16:37:00.555000+00:00","session_key":9468,"meeting_key":1229,
       "driver_number":1,"y":5867,"x":-65,"z":-159}
    ])");
    server.serve("/v1/stints", R"([
      {"meeting_key":1229,"session_key":9468,"stint_number":1,"driver_number":1,"lap_start":1,
       "lap_end":4,"compound":"SOFT","tyre_age_at_start":0}
    ])");

    const int port = server.start();
    check(port > 0, "servidor de teste no ar");
    if (port == 0) return;

    auto http = std::make_shared<apex::common::HttpClient>();
    apex::openf1::Client client(http, server.base_url());

    std::string observed_endpoint;
    size_t observed_bytes = 0;
    client.set_payload_observer([&](std::string_view endpoint, const std::string&,
                                    const std::string& body) {
        observed_endpoint = std::string(endpoint);
        observed_bytes = body.size();
    });

    const auto samples = client.car_data(9468, 1, 0, 2'000'000'000'000'000LL);
    check(samples.size() == 3, "car_data devolve as três amostras do payload");
    check(client.ok(), "nenhum erro de upstream registrado");
    check(observed_endpoint == "car_data", "o observador recebe o endpoint consultado");
    check(observed_bytes > 100, "o observador recebe o corpo bruto completo");

    if (samples.size() == 3) {
        // A ordenação é por instante, não pela ordem de chegada no payload.
        check(samples[0].date_us < samples[1].date_us && samples[1].date_us < samples[2].date_us,
              "as amostras saem ordenadas por instante");
        check(std::abs(samples[0].brake_pct - 0.0) < 1e-9, "freio 0 permanece 0%");
        check(std::abs(samples[1].brake_pct - 100.0) < 1e-9,
              "freio 1 (codificação antiga) normaliza para 100%");
        check(std::abs(samples[2].brake_pct - 100.0) < 1e-9, "freio 100 permanece 100%");
        check(!samples[0].drs_active(), "DRS 8 é elegibilidade, não abertura");
        check(samples[2].drs_active(), "DRS 12 é aba aberta");
        check(samples[1].gear == 5 && samples[1].rpm == 9000, "canais discretos preservados");
    }

    const auto laps = client.laps(9468, 1);
    check(laps.size() == 2, "laps devolve as duas voltas");
    if (laps.size() == 2) {
        check(!laps[0].lap_duration_s.has_value(), "lap_duration nulo continua ausente, não vira 0");
        check(!laps[0].sector_1_s.has_value(), "setor nulo continua ausente");
        check(laps[0].is_pit_out_lap, "a volta de saída dos boxes é identificada");
        check(laps[1].lap_duration_s && std::abs(*laps[1].lap_duration_s - 90.031) < 1e-9,
              "o tempo de volta cronometrado é lido com precisão de milésimo");
        check(laps[1].st_speed_kmh && std::abs(*laps[1].st_speed_kmh - 316.0) < 1e-9,
              "a velocidade do speed trap oficial é preservada");
        check(laps[1].date_start_us ==
                  *apex::common::parse_iso8601_us("2024-03-01T16:08:56.691000+00:00"),
              "o início da volta vira microssegundos UTC exatos");
    }

    const auto positions = client.location(9468, 1, 0, 2'000'000'000'000'000LL);
    check(positions.size() == 2, "amostras de posição em (0,0) são descartadas como perda de sinal");

    const auto stints = client.stints(9468, 1);
    check(stints.size() == 1 && stints[0].compound == "SOFT" && stints[0].lap_end == 4,
          "stints trazem composto e faixa de voltas reais");

    // Um endpoint sem rota devolve 404: o cliente precisa reportar erro, não vazio silencioso.
    const auto missing = client.weather(9468);
    check(missing.empty() && !client.ok(), "erro de upstream é reportado em last_error()");

    server.stop();
}

void test_raw_archive_is_content_addressed() {
    std::cout << "Arquivo bruto endereçado por conteúdo\n";

    const std::filesystem::path root = "data/test_archive";
    std::filesystem::remove_all(root);
    apex::ingest::RawArchive archive(root);

    const std::string payload = R"([{"session_key":9468,"driver_number":1}])";
    const auto first = archive.store("laps", 9468, 1, "https://api.openf1.org/v1/laps", payload);
    check(first.has_value(), "grava o payload");
    check(first && !first->already_present, "a primeira gravação cria o arquivo");
    check(first && std::filesystem::exists(first->path), "o arquivo existe em disco");
    check(first && first->sha256 == apex::common::Sha256::hex_of(payload),
          "o digest registrado corresponde ao conteúdo");

    const auto again = archive.store("laps", 9468, 1, "https://api.openf1.org/v1/laps", payload);
    check(again && again->already_present, "reingerir o mesmo corpo não duplica o arquivo");
    check(again && again->path == first->path, "o caminho é estável para o mesmo conteúdo");

    const auto different =
        archive.store("laps", 9468, 1, "https://api.openf1.org/v1/laps", payload + " ");
    check(different && !different->already_present, "um corpo diferente gera um arquivo novo");
    check(different && different->path != first->path, "conteúdos distintos não colidem");

    std::ifstream stored(first->path, std::ios::binary);
    const std::string round_trip((std::istreambuf_iterator<char>(stored)),
                                 std::istreambuf_iterator<char>());
    check(round_trip == payload, "o corpo é arquivado byte a byte, sem reserialização");

    std::filesystem::remove_all(root);
}

} // namespace

int main() {
    std::cout << "=== APEX INGEST — TESTES C++23 ===\n";
    test_sha256_known_vectors();
    test_iso8601_round_trip();
    test_openf1_client_against_recorded_payloads();
    test_raw_archive_is_content_addressed();

    if (failures == 0) {
        std::cout << "\nTodos os testes de ingestão passaram.\n";
        return 0;
    }
    std::cout << "\n" << failures << " teste(s) de ingestão falharam.\n";
    return 1;
}

#include "raw_archive.hpp"
#include "warehouse.hpp"

#include "../../common/http/http_client.hpp"
#include "../../common/openf1/openf1_client.hpp"
#include "../../common/time/iso8601.hpp"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Options {
    std::optional<int> year;
    std::string country;
    std::string session_name;
    std::optional<int64_t> session_key;
    std::vector<int32_t> drivers;
    std::vector<int32_t> laps;
    int fastest_laps{3};
    bool telemetry{false};
    bool location{false};
    bool dry_run{false};
    bool list_only{false};
    std::string archive_dir{"data/raw"};
};

void print_usage() {
    std::cout << R"(ApexTelemetry — motor de ingestão OpenF1 (C++23)

Descoberta de sessões:
  apex_ingest --year 2024 [--country Bahrain] [--session-name Qualifying] --list

Ingestão de uma sessão:
  apex_ingest --session 9468 [opções]

Opções:
  --session <chave>       Chave da sessão OpenF1 a ingerir.
  --year <ano>            Ano usado na descoberta de sessões.
  --country <nome>        Filtro de país na descoberta.
  --session-name <nome>   Filtro de nome da sessão (Qualifying, Race, Practice 1, ...).
  --list                  Apenas lista as sessões encontradas e termina.
  --drivers 1,16,44       Restringe os pilotos (padrão: todos os inscritos).
  --laps 14,15            Voltas exatas para a telemetria de alta frequência.
  --fastest <n>           Em vez de --laps, as n voltas mais rápidas de cada piloto (padrão: 3).
  --telemetry             Baixa e carrega as amostras de ECU (car_data).
  --location              Baixa e carrega as amostras de posição (traçado real).
  --archive <dir>         Diretório do arquivo bruto (padrão: data/raw).
  --dry-run               Busca e relata, sem escrever no PostgreSQL.
  -h, --help              Mostra esta ajuda.

Variáveis de ambiente:
  APEX_DATABASE_URL       Destino PostgreSQL/TimescaleDB. Sem ela, a ingestão roda
                          em modo somente-arquivo e avisa no relatório final.
  APEX_HTTP_IP_FAMILY     auto | v4 | v6 (padrão v4).
)";
}

std::vector<int32_t> parse_int_list(const std::string& raw) {
    std::vector<int32_t> values;
    std::stringstream stream(raw);
    std::string item;
    while (std::getline(stream, item, ',')) {
        if (item.empty()) continue;
        try {
            values.push_back(static_cast<int32_t>(std::stol(item)));
        } catch (...) {
            std::cerr << "[AVISO] ignorando valor não numérico na lista: '" << item << "'\n";
        }
    }
    return values;
}

std::string format_duration(double seconds) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << seconds;
    return oss.str();
}

/** Cobertura efetivamente medida: fração do tempo de volta coberta por amostras. */
double coverage_of(const std::vector<apex::openf1::CarSample>& samples, int64_t lap_start_us,
                   double lap_time_s) {
    if (samples.size() < 2 || lap_time_s <= 0.0) return 0.0;
    std::vector<double> times;
    times.reserve(samples.size());
    for (const auto& sample : samples) {
        times.push_back(static_cast<double>(sample.date_us - lap_start_us) / 1'000'000.0);
    }
    std::vector<double> intervals;
    intervals.reserve(times.size() - 1);
    for (size_t i = 1; i < times.size(); ++i) intervals.push_back(times[i] - times[i - 1]);
    auto sorted = intervals;
    std::nth_element(sorted.begin(), sorted.begin() + static_cast<long>(sorted.size() / 2),
                     sorted.end());
    const double median_dt = sorted[sorted.size() / 2];
    const double threshold = std::max(0.5, median_dt * 3.0);

    double missing = std::max(0.0, times.front());
    missing += std::max(0.0, lap_time_s - times.back());
    for (const double interval : intervals) {
        if (interval > threshold) missing += interval - median_dt;
    }
    return std::clamp(100.0 * (1.0 - missing / lap_time_s), 0.0, 100.0);
}

struct Report {
    int sessions{0};
    int drivers{0};
    int stints{0};
    int laps{0};
    int race_control{0};
    int weather{0};
    int telemetry_laps{0};
    long long telemetry_samples{0};
    int location_laps{0};
    long long location_samples{0};
    int archived_payloads{0};
    int write_failures{0};
};

} // namespace

int main(int argc, char* argv[]) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        const auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "[ERRO] " << name << " exige um valor\n";
                std::exit(2);
            }
            return argv[++i];
        };

        if (argument == "-h" || argument == "--help") {
            print_usage();
            return 0;
        } else if (argument == "--year") {
            options.year = std::stoi(next("--year"));
        } else if (argument == "--country") {
            options.country = next("--country");
        } else if (argument == "--session-name") {
            options.session_name = next("--session-name");
        } else if (argument == "--session") {
            options.session_key = std::stoll(next("--session"));
        } else if (argument == "--drivers") {
            options.drivers = parse_int_list(next("--drivers"));
        } else if (argument == "--laps") {
            options.laps = parse_int_list(next("--laps"));
        } else if (argument == "--fastest") {
            options.fastest_laps = std::stoi(next("--fastest"));
        } else if (argument == "--telemetry") {
            options.telemetry = true;
        } else if (argument == "--location") {
            options.location = true;
        } else if (argument == "--archive") {
            options.archive_dir = next("--archive");
        } else if (argument == "--dry-run") {
            options.dry_run = true;
        } else if (argument == "--list") {
            options.list_only = true;
        } else {
            std::cerr << "[ERRO] argumento desconhecido: " << argument << "\n\n";
            print_usage();
            return 2;
        }
    }

    if (!options.session_key && !options.year) {
        print_usage();
        return 2;
    }

    auto http = std::make_shared<apex::common::HttpClient>();
    apex::openf1::Client openf1(http);
    apex::ingest::RawArchive archive(options.archive_dir);
    apex::ingest::Warehouse warehouse;
    Report report;

    const bool writable = warehouse.configured() && !options.dry_run;
    std::cout << "====================================================\n"
              << " ApexTelemetry — Ingestão OpenF1 (C++23)            \n"
              << "====================================================\n"
              << " upstream    : " << openf1.base_url() << "\n"
              << " arquivo     : " << archive.base_dir().string() << "\n"
              << " PostgreSQL  : "
              << (warehouse.configured()
                      ? (options.dry_run ? "configurado (dry-run: nada será escrito)"
                                         : (warehouse.healthy() ? "conectado" : "SEM RESPOSTA"))
                      : "não configurado (APEX_DATABASE_URL)")
              << "\n====================================================\n";

    // Cada resposta bem-sucedida é arquivada exatamente como veio do upstream e
    // registrada no armazém pelo seu digest, antes de qualquer interpretação.
    int64_t archive_session_key = options.session_key.value_or(0);
    std::optional<int32_t> archive_driver;
    openf1.set_payload_observer([&](std::string_view endpoint, const std::string& url,
                                    const std::string& body) {
        const auto entry = archive.store(std::string(endpoint), archive_session_key, archive_driver,
                                         url, body);
        if (!entry) return;
        if (!entry->already_present) ++report.archived_payloads;
        if (writable && !warehouse.record_raw_payload(*entry)) {
            std::cerr << "[AVISO] raw_payloads: " << warehouse.last_error() << "\n";
            ++report.write_failures;
        }
    });

    // ------------------------------------------------------------ descoberta
    if (!options.session_key || options.list_only) {
        const auto sessions = openf1.sessions(options.year, options.session_name, options.country);
        if (sessions.empty()) {
            std::cerr << "[ERRO] nenhuma sessão encontrada. " << openf1.last_error() << "\n";
            return 1;
        }
        std::cout << "\nSessões encontradas (" << sessions.size() << "):\n";
        for (const auto& session : sessions) {
            std::cout << "  " << std::setw(6) << session.session_key << "  " << std::setw(24)
                      << std::left << (session.circuit_name + " / " + session.country_name)
                      << std::right << "  " << std::setw(14) << std::left << session.session_type
                      << std::right << "  " << std::setw(16) << std::left << session.session_name
                      << std::right << "  " << session.date_start << "\n";
        }
        if (options.list_only || !options.session_key) {
            std::cout << "\nUse --session <chave> para ingerir uma delas.\n";
            return 0;
        }
    }

    const int64_t session_key = *options.session_key;

    // ------------------------------------------------------- metadados da sessão
    const auto session = openf1.session(session_key);
    if (!session) {
        std::cerr << "[ERRO] sessão " << session_key << " não existe na OpenF1. "
                  << openf1.last_error() << "\n";
        return 1;
    }
    std::cout << "\n[1/6] Sessão " << session_key << ": " << session->circuit_name << " / "
              << session->country_name << " — " << session->session_name << " (" << session->year
              << ")\n";
    if (writable) {
        if (warehouse.upsert_session(*session)) {
            ++report.sessions;
        } else {
            std::cerr << "[ERRO] sessions: " << warehouse.last_error() << "\n";
            ++report.write_failures;
        }
    } else {
        ++report.sessions;
    }

    // ------------------------------------------------------------------ pilotos
    const auto drivers = openf1.drivers(session_key);
    std::cout << "[2/6] Pilotos inscritos: " << drivers.size() << "\n";
    if (drivers.empty()) {
        std::cerr << "[ERRO] a sessão não possui lista de pilotos. " << openf1.last_error() << "\n";
        return 1;
    }
    report.drivers = static_cast<int>(drivers.size());
    if (writable && !warehouse.upsert_drivers(session_key, drivers)) {
        std::cerr << "[ERRO] drivers: " << warehouse.last_error() << "\n";
        ++report.write_failures;
    }

    std::vector<int32_t> selected = options.drivers;
    if (selected.empty()) {
        for (const auto& driver : drivers) selected.push_back(driver.driver_number);
    } else {
        for (const auto number : selected) {
            const bool entered = std::any_of(drivers.begin(), drivers.end(), [number](const auto& d) {
                return d.driver_number == number;
            });
            if (!entered) {
                std::cerr << "[AVISO] piloto " << number << " não participou desta sessão.\n";
            }
        }
    }

    // ------------------------------------------------------------------- stints
    const auto stints = openf1.stints(session_key);
    std::cout << "[3/6] Stints: " << stints.size() << "\n";
    report.stints = static_cast<int>(stints.size());
    if (writable && !stints.empty() && !warehouse.upsert_stints(session_key, stints)) {
        std::cerr << "[ERRO] stints: " << warehouse.last_error() << "\n";
        ++report.write_failures;
    }

    // -------------------------------------------------------------------- voltas
    const auto laps = openf1.laps(session_key);
    const auto timed = std::count_if(laps.begin(), laps.end(),
                                     [](const auto& lap) { return lap.lap_duration_s.has_value(); });
    std::cout << "[4/6] Voltas: " << laps.size() << " (" << timed << " cronometradas)\n";
    report.laps = static_cast<int>(laps.size());
    if (writable && !laps.empty() && !warehouse.upsert_laps(session_key, laps, stints)) {
        std::cerr << "[ERRO] laps: " << warehouse.last_error() << "\n";
        ++report.write_failures;
    }

    // ------------------------------------------------- direção de prova e clima
    const auto race_control = openf1.race_control(session_key);
    std::cout << "[5/6] Direção de prova: " << race_control.size() << " eventos\n";
    report.race_control = static_cast<int>(race_control.size());
    if (writable && !race_control.empty() &&
        !warehouse.upsert_race_control(session_key, race_control)) {
        std::cerr << "[ERRO] race_control_events: " << warehouse.last_error() << "\n";
        ++report.write_failures;
    }

    const auto weather = openf1.weather(session_key);
    std::cout << "      Condições de pista: " << weather.size() << " leituras";
    if (!weather.empty()) {
        double track_sum = 0.0;
        for (const auto& sample : weather) track_sum += sample.track_temperature_c;
        std::cout << " (média " << std::fixed << std::setprecision(1)
                  << track_sum / static_cast<double>(weather.size()) << " °C)";
    }
    std::cout << "\n";
    report.weather = static_cast<int>(weather.size());
    if (writable && !weather.empty() && !warehouse.upsert_weather(session_key, weather)) {
        std::cerr << "[ERRO] weather_samples: " << warehouse.last_error() << "\n";
        ++report.write_failures;
    }

    // --------------------------------------------- telemetria de alta frequência
    if (!options.telemetry && !options.location) {
        std::cout << "[6/6] Telemetria de alta frequência não solicitada (--telemetry / --location).\n";
    } else {
        std::cout << "[6/6] Telemetria de alta frequência\n";
        for (const auto driver_number : selected) {
            // Voltas candidatas: as pedidas explicitamente ou as n mais rápidas.
            std::vector<apex::openf1::Lap> candidates;
            for (const auto& lap : laps) {
                if (lap.driver_number != driver_number) continue;
                if (!lap.lap_duration_s || lap.date_start_us == 0) continue;
                if (!options.laps.empty()) {
                    if (std::find(options.laps.begin(), options.laps.end(), lap.lap_number) ==
                        options.laps.end()) {
                        continue;
                    }
                }
                candidates.push_back(lap);
            }
            if (options.laps.empty() && options.fastest_laps > 0) {
                std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
                    return *a.lap_duration_s < *b.lap_duration_s;
                });
                if (candidates.size() > static_cast<size_t>(options.fastest_laps)) {
                    candidates.resize(static_cast<size_t>(options.fastest_laps));
                }
            }
            if (candidates.empty()) continue;

            for (const auto& lap : candidates) {
                const auto from_us = lap.date_start_us;
                const auto to_us =
                    from_us + static_cast<int64_t>((*lap.lap_duration_s + 0.4) * 1'000'000.0);

                archive_driver = driver_number;
                if (options.telemetry) {
                    const auto samples =
                        openf1.car_data(session_key, driver_number, from_us, to_us);
                    if (samples.empty()) {
                        std::cerr << "      [AVISO] #" << driver_number << " volta " << lap.lap_number
                                  << ": sem car_data. " << openf1.last_error() << "\n";
                    } else {
                        ++report.telemetry_laps;
                        report.telemetry_samples += static_cast<long long>(samples.size());
                        const double coverage =
                            coverage_of(samples, from_us, *lap.lap_duration_s);
                        std::cout << "      #" << std::setw(2) << driver_number << " volta "
                                  << std::setw(2) << lap.lap_number << "  "
                                  << format_duration(*lap.lap_duration_s) << " s  "
                                  << std::setw(4) << samples.size() << " amostras  cobertura "
                                  << std::fixed << std::setprecision(1) << coverage << "%\n";
                        if (writable) {
                            if (!warehouse.copy_telemetry(session_key, driver_number, lap.lap_number,
                                                          samples)) {
                                std::cerr << "      [ERRO] telemetry_samples: "
                                          << warehouse.last_error() << "\n";
                                ++report.write_failures;
                            } else if (!warehouse.update_lap_coverage(session_key, driver_number,
                                                                      lap.lap_number, coverage)) {
                                std::cerr << "      [ERRO] laps.coverage_pct: "
                                          << warehouse.last_error() << "\n";
                                ++report.write_failures;
                            }
                        }
                    }
                }

                if (options.location) {
                    const auto positions =
                        openf1.location(session_key, driver_number, from_us, to_us);
                    if (!positions.empty()) {
                        ++report.location_laps;
                        report.location_samples += static_cast<long long>(positions.size());
                        if (writable && !warehouse.copy_location(session_key, driver_number,
                                                                 lap.lap_number, positions)) {
                            std::cerr << "      [ERRO] location_samples: " << warehouse.last_error()
                                      << "\n";
                            ++report.write_failures;
                        }
                    }
                }
            }
            archive_driver.reset();
        }
    }

    std::cout << "\n====================================================\n"
              << " Relatório de ingestão — sessão " << session_key << "\n"
              << "----------------------------------------------------\n"
              << "  sessões              : " << report.sessions << "\n"
              << "  pilotos              : " << report.drivers << "\n"
              << "  stints               : " << report.stints << "\n"
              << "  voltas               : " << report.laps << "\n"
              << "  eventos de prova     : " << report.race_control << "\n"
              << "  leituras de clima    : " << report.weather << "\n"
              << "  voltas com telemetria: " << report.telemetry_laps << " ("
              << report.telemetry_samples << " amostras)\n"
              << "  voltas com posição   : " << report.location_laps << " ("
              << report.location_samples << " amostras)\n"
              << "  payloads arquivados  : " << report.archived_payloads << "\n"
              << "  falhas de escrita    : " << report.write_failures << "\n"
              << "====================================================\n";

    if (!warehouse.configured()) {
        std::cout << "[AVISO] APEX_DATABASE_URL não definida: nada foi persistido no armazém.\n";
        return 0;
    }
    return report.write_failures == 0 ? 0 : 1;
}

#include "../include/spatial_alignment.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <numbers>
#include <random>
#include <string>
#include <vector>

namespace {

using namespace apex::analytics;

int failures = 0;

void check(bool condition, const std::string& label) {
    if (condition) {
        std::cout << "  [PASS] " << label << "\n";
    } else {
        std::cout << "  [FAIL] " << label << "\n";
        ++failures;
    }
}

/**
 * Gera um traçado cinemático a partir de um perfil de velocidade arbitrário,
 * amostrado na frequência real da OpenF1 (~4 Hz). Não é telemetria de F1 — é um
 * banco de ensaio com resposta analítica conhecida, para que cada propriedade do
 * motor possa ser verificada contra um valor exato.
 */
std::vector<RawSample> synthesize(const std::function<double(double)>& speed_kmh_at,
                                  double duration_s, double dt_s) {
    std::vector<RawSample> samples;
    for (double t = 0.0; t <= duration_s + 1e-9; t += dt_s) {
        RawSample sample;
        sample.time_s = t;
        sample.speed_kmh = speed_kmh_at(t);
        sample.throttle_pct = sample.speed_kmh > 150.0 ? 100.0 : 40.0;
        sample.brake_pct = 0.0;
        sample.rpm = static_cast<int32_t>(std::lround(sample.speed_kmh * 35.0));
        sample.gear = std::clamp(static_cast<int32_t>(sample.speed_kmh / 45.0) + 1, 1, 8);
        sample.drs = false;
        samples.push_back(sample);
    }
    return samples;
}

void test_distance_integration_is_exact_for_constant_speed() {
    std::cout << "Integração de distância\n";

    // 360 km/h = 100 m/s exatos: em 10 s o carro percorre 1000 m.
    const auto samples = synthesize([](double) { return 360.0; }, 10.0, 0.25);
    const auto integrated = SpatialAlignmentEngine::integrate_distance(samples);

    check(!integrated.empty(), "produz amostras");
    check(std::abs(integrated.front().distance_m) < 1e-12, "a volta começa no marco zero");
    check(std::abs(integrated.back().distance_m - 1000.0) < 1e-9,
          "velocidade constante integra exatamente (1000 m em 10 s a 100 m/s)");

    // Aceleração linear: v(t) = 36*t km/h = 10*t m/s, distância = 5*t².
    const auto ramp = SpatialAlignmentEngine::integrate_distance(
        synthesize([](double t) { return 36.0 * t; }, 10.0, 0.05));
    check(std::abs(ramp.back().distance_m - 500.0) < 0.5,
          "aceleração linear converge para a integral analítica (500 m)");

    check(SpatialAlignmentEngine::integrate_distance({}).empty(), "entrada vazia devolve vazio");
}

void test_distance_is_monotonic() {
    std::cout << "Monotonicidade do eixo espacial\n";

    std::mt19937 generator(20240301);
    std::uniform_real_distribution<double> speeds(0.0, 340.0);
    std::vector<RawSample> noisy;
    for (int i = 0; i < 400; ++i) {
        RawSample sample;
        sample.time_s = i * 0.24;
        sample.speed_kmh = speeds(generator);
        noisy.push_back(sample);
    }

    const auto integrated = SpatialAlignmentEngine::integrate_distance(noisy);
    bool monotonic = true;
    for (size_t i = 1; i < integrated.size(); ++i) {
        if (integrated[i].distance_m < integrated[i - 1].distance_m) monotonic = false;
    }
    check(monotonic, "a distância nunca retrocede, qualquer que seja o perfil de velocidade");
}

void test_grid_resampling_preserves_measurements() {
    std::cout << "Reamostragem em grade métrica\n";

    const auto integrated = SpatialAlignmentEngine::integrate_distance(
        synthesize([](double t) { return 200.0 + 100.0 * std::sin(t / 3.0); }, 60.0, 0.25));
    const double max_gap = SpatialAlignmentEngine::recommended_max_gap_m(integrated, 5.0);
    const auto grid = SpatialAlignmentEngine::resample_to_grid(integrated, 5.0, max_gap);

    check(grid.size() > 100, "a grade cobre a volta inteira");
    bool spaced = true;
    for (size_t i = 1; i < grid.size(); ++i) {
        if (std::abs((grid[i].distance_m - grid[i - 1].distance_m) - 5.0) > 1e-9) spaced = false;
    }
    check(spaced, "os nós ficam exatamente a 5 m um do outro");

    bool time_monotonic = true;
    for (size_t i = 1; i < grid.size(); ++i) {
        if (grid[i].time_s < grid[i - 1].time_s) time_monotonic = false;
    }
    check(time_monotonic, "o tempo cresce ao longo da grade");

    const auto no_extrapolation =
        std::none_of(grid.begin(), grid.end(), [](const auto& p) { return p.is_extrapolated; });
    check(no_extrapolation, "a 4 Hz o espaçamento normal não é marcado como descontinuidade");

    // Com um limite artificialmente apertado, a lacuna passa a ser sinalizada.
    const auto strict = SpatialAlignmentEngine::resample_to_grid(integrated, 5.0, 1.0);
    const auto flagged =
        std::any_of(strict.begin(), strict.end(), [](const auto& p) { return p.is_extrapolated; });
    check(flagged, "acima do limite a lacuna é marcada, e não interpolada silenciosamente");
}

void test_adaptive_gap_tracks_sampling_rate() {
    std::cout << "Limite adaptativo de lacuna\n";

    const auto fast = SpatialAlignmentEngine::integrate_distance(
        synthesize([](double) { return 320.0; }, 60.0, 0.24));
    const auto slow = SpatialAlignmentEngine::integrate_distance(
        synthesize([](double) { return 320.0; }, 60.0, 0.05));

    const double fast_gap = SpatialAlignmentEngine::recommended_max_gap_m(fast, 5.0);
    const double slow_gap = SpatialAlignmentEngine::recommended_max_gap_m(slow, 5.0);
    check(fast_gap > slow_gap, "amostragem mais esparsa exige tolerância maior");
    check(fast_gap > 320.0 / 3.6 * 0.24, "a tolerância cobre o passo nominal em velocidade máxima");
}

void test_normalization_closes_the_delta_against_the_stopwatch() {
    std::cout << "Normalização do eixo e fechamento do delta\n";

    // Duas voltas do mesmo traçado com ritmos diferentes e erro de escala distinto.
    const double reference_lap_time = 90.0;
    const double comparison_lap_time = 90.4;

    const auto reference = SpatialAlignmentEngine::integrate_distance(
        synthesize([](double t) { return 220.0 + 80.0 * std::sin(t / 5.0); }, reference_lap_time, 0.24));
    const auto comparison = SpatialAlignmentEngine::integrate_distance(synthesize(
        [](double t) { return 218.0 + 79.0 * std::sin(t / 5.02); }, comparison_lap_time, 0.21));

    const double axis =
        SpatialAlignmentEngine::distance_at_time(reference, reference_lap_time);
    const auto reference_norm =
        SpatialAlignmentEngine::normalize_lap_distance(reference, reference_lap_time, axis);
    const auto comparison_norm =
        SpatialAlignmentEngine::normalize_lap_distance(comparison, comparison_lap_time, axis);

    check(std::abs(reference_norm.back().distance_m - axis) < 1e-6,
          "a volta de referência termina exatamente no comprimento do eixo");
    check(std::abs(comparison_norm.back().distance_m - axis) < 1e-6,
          "a volta de comparação é reescalada para o mesmo comprimento");
    check(std::abs(reference_norm.back().time_s - reference_lap_time) < 1e-9,
          "a volta é recortada no instante cronometrado");

    const auto ref_grid = SpatialAlignmentEngine::resample_to_grid(reference_norm, 5.0, 60.0);
    const auto comp_grid = SpatialAlignmentEngine::resample_to_grid(comparison_norm, 5.0, 60.0);
    const auto channels = SpatialAlignmentEngine::align_and_compute_delta(ref_grid, comp_grid);

    check(!channels.empty(), "o alinhamento produz canais");
    const double closure =
        std::abs(channels.back().delta_time_s - (comparison_lap_time - reference_lap_time));
    check(closure < 0.05,
          "o delta acumulado converge para a diferença cronometrada (erro " +
              std::to_string(closure) + " s)");
}

void test_corner_detection_finds_real_minima() {
    std::cout << "Detecção de zonas lentas\n";

    // Três frenagens claras em um traçado de 90 s.
    const auto profile = [](double t) {
        const double base = 300.0;
        double speed = base;
        for (const double centre : {20.0, 45.0, 70.0}) {
            const double distance = std::abs(t - centre);
            if (distance < 4.0) speed = std::min(speed, 80.0 + 40.0 * distance);
        }
        return speed;
    };
    const auto integrated = SpatialAlignmentEngine::integrate_distance(synthesize(profile, 90.0, 0.2));
    const auto grid = SpatialAlignmentEngine::resample_to_grid(
        integrated, 5.0, SpatialAlignmentEngine::recommended_max_gap_m(integrated, 5.0));
    const auto corners = SpatialAlignmentEngine::detect_corners(grid);

    check(corners.size() == 3, "encontra exatamente as três frenagens (achou " +
                                   std::to_string(corners.size()) + ")");
    bool ordered = true;
    for (size_t i = 1; i < corners.size(); ++i) {
        if (corners[i].apex_distance_m <= corners[i - 1].apex_distance_m) ordered = false;
        if (corners[i].index != corners[i - 1].index + 1) ordered = false;
    }
    check(ordered, "as curvas saem ordenadas e numeradas por distância");
    const auto slow = std::all_of(corners.begin(), corners.end(),
                                  [](const auto& c) { return c.apex_speed_kmh < 120.0; });
    check(slow, "cada ápice detectado é de fato um mínimo de velocidade");
    check(corners.empty() || corners.front().label == "C1",
          "o rótulo é C1..Cn, e não a numeração oficial do circuito");

    // Uma volta sem frenagem nenhuma não pode inventar curvas.
    const auto flat = SpatialAlignmentEngine::integrate_distance(
        synthesize([](double) { return 300.0; }, 60.0, 0.2));
    const auto flat_grid = SpatialAlignmentEngine::resample_to_grid(flat, 5.0, 60.0);
    check(SpatialAlignmentEngine::detect_corners(flat_grid).empty(),
          "velocidade constante não produz curva alguma");
}

void test_microsectors_partition_the_lap() {
    std::cout << "Microsetores\n";

    const auto integrated = SpatialAlignmentEngine::integrate_distance(
        synthesize([](double t) { return 200.0 + 60.0 * std::sin(t / 4.0); }, 90.0, 0.24));
    const auto grid = SpatialAlignmentEngine::resample_to_grid(integrated, 5.0, 60.0);
    const auto channels = SpatialAlignmentEngine::align_and_compute_delta(grid, grid);
    const auto microsectors = SpatialAlignmentEngine::compute_microsectors(channels, 100.0);

    check(!microsectors.empty(), "a volta é particionada");
    bool contiguous = true;
    for (size_t i = 1; i < microsectors.size(); ++i) {
        if (microsectors[i].distance_start_m < microsectors[i - 1].distance_end_m - 5.0) {
            contiguous = false;
        }
    }
    check(contiguous, "os microsetores cobrem a volta sem sobreposição");

    const auto all_equal = std::all_of(microsectors.begin(), microsectors.end(), [](const auto& m) {
        return m.winner == "EQUAL" && std::abs(m.delta_s) < 1e-9;
    });
    check(all_equal, "comparar uma volta com ela mesma dá delta zero em todo microsetor");

    double sum = 0.0;
    for (const auto& microsector : microsectors) sum += microsector.delta_s;
    check(std::abs(sum) < 1e-6, "a soma dos deltas dos microsetores é o delta total");
}

void test_quality_audit_reports_measured_coverage() {
    std::cout << "Auditoria de qualidade\n";

    const auto integrated = SpatialAlignmentEngine::integrate_distance(
        synthesize([](double) { return 280.0; }, 60.0, 0.24));
    const double max_gap = SpatialAlignmentEngine::recommended_max_gap_m(integrated, 5.0);
    const auto grid = SpatialAlignmentEngine::resample_to_grid(integrated, 5.0, max_gap);

    const auto quality = SpatialAlignmentEngine::compute_quality_audit(integrated, integrated, grid,
                                                                       grid, max_gap);
    check(quality.coverage_pct > 99.9, "telemetria íntegra reporta cobertura total");
    check(quality.discontinuous_segments == 0, "nenhuma descontinuidade em dados íntegros");
    check(std::abs(quality.median_sample_interval_s - 0.24) < 1e-6,
          "o intervalo mediano reportado é o medido");
    check(quality.raw_samples_ref == static_cast<int32_t>(integrated.size()),
          "a contagem de amostras brutas é a real");
    check(quality.confidence_score > 0.9, "confiança alta para dados íntegros");

    const auto strict_grid = SpatialAlignmentEngine::resample_to_grid(integrated, 5.0, 1.0);
    const auto degraded = SpatialAlignmentEngine::compute_quality_audit(integrated, integrated,
                                                                        strict_grid, strict_grid, 1.0);
    check(degraded.coverage_pct < quality.coverage_pct,
          "lacunas reduzem a cobertura reportada em vez de serem escondidas");
    check(degraded.confidence_score < quality.confidence_score, "e reduzem a confiança");
}

void test_json_is_well_formed_and_escapes_hostile_text() {
    std::cout << "Serialização JSON\n";

    check(SpatialAlignmentEngine::json_escape(R"(aspas " barra \ quebra)" "\n") ==
              R"(aspas \" barra \\ quebra\n)",
          "aspas, barras e quebras são escapadas");

    SessionMetadata session;
    session.session_key = 9468;
    session.circuit_key = 63;
    session.circuit_name = R"(Circuito "teste" \ 1)";
    session.country_name = "Bahrain";
    session.session_name = "Qualifying";
    session.track_temperature_c = 21.5;
    session.data_source = "openf1-upstream";

    LapMetadata reference;
    reference.driver_number = 1;
    reference.driver_code = "VER";
    reference.team_name = "Red Bull Racing";
    reference.lap_number = 16;
    reference.lap_time_s = 89.179;
    reference.compound = "SOFT";
    reference.sector_1_s = 28.535;
    LapMetadata comparison = reference;
    comparison.driver_number = 16;
    comparison.driver_code = "LEC";
    comparison.lap_time_s = 89.480;
    comparison.sector_1_s = std::nullopt;

    const auto integrated = SpatialAlignmentEngine::integrate_distance(
        synthesize([](double t) { return 220.0 + 70.0 * std::sin(t / 4.0); }, 90.0, 0.24));
    const auto grid = SpatialAlignmentEngine::resample_to_grid(integrated, 5.0, 60.0);
    const auto channels = SpatialAlignmentEngine::align_and_compute_delta(grid, grid);
    const auto corners = SpatialAlignmentEngine::detect_corners(grid);
    const auto microsectors = SpatialAlignmentEngine::compute_microsectors(channels, 100.0);
    const auto traps = SpatialAlignmentEngine::compute_speed_traps(channels, reference, comparison);
    const auto segments = SpatialAlignmentEngine::detect_segments(channels, corners);
    const auto quality =
        SpatialAlignmentEngine::compute_quality_audit(integrated, integrated, grid, grid, 60.0);

    const auto insights =
        SpatialAlignmentEngine::build_insights_json(comparison, segments, quality, 5.0);
    const auto payload = SpatialAlignmentEngine::build_comparison_json(
        session, reference, comparison, 5.0, channels, corners, traps, microsectors, quality, insights);

    check(payload.front() == '{' && payload.back() == '}', "o payload é um objeto JSON completo");
    check(payload.find(R"("circuit_name":"Circuito \"teste\" \\ 1")") != std::string::npos,
          "o nome do circuito é escapado corretamente");
    check(payload.find(R"("sector_1_s":null)") != std::string::npos,
          "um setor ausente vira null, e não zero");
    check(payload.find(R"("sector_1_s":28.535)") != std::string::npos,
          "um setor presente mantém a precisão de milésimo");
    check(payload.find(R"("data_source":"openf1-upstream")") != std::string::npos,
          "a origem dos dados é declarada no payload");
    check(payload.find("\"channels\":[{") != std::string::npos, "os canais são emitidos");

    int depth = 0;
    bool balanced = true;
    bool in_string = false;
    bool escaped = false;
    for (const char ch : payload) {
        if (escaped) { escaped = false; continue; }
        if (ch == '\\') { escaped = true; continue; }
        if (ch == '"') { in_string = !in_string; continue; }
        if (in_string) continue;
        if (ch == '{' || ch == '[') ++depth;
        if (ch == '}' || ch == ']') --depth;
        if (depth < 0) balanced = false;
    }
    check(balanced && depth == 0 && !in_string, "chaves, colchetes e aspas fecham corretamente");

    const auto evidence =
        SpatialAlignmentEngine::build_evidence_json(session, reference, comparison, segments);
    check(evidence.front() == '{' && evidence.back() == '}', "a evidência para o Haskell é um objeto");
    check(evidence.find("\"segments\":[") != std::string::npos, "a evidência inclui os segmentos");
}

void test_motec_export_has_one_row_per_grid_node() {
    std::cout << "Exportação MoTeC CSV\n";

    SessionMetadata session;
    session.circuit_name = "Sakhir";
    session.session_name = "Qualifying";
    session.session_key = 9468;
    session.data_source = "postgresql";

    LapMetadata reference;
    reference.driver_code = "VER";
    reference.driver_number = 1;
    reference.lap_number = 16;
    reference.lap_time_s = 89.179;
    reference.compound = "SOFT";
    LapMetadata comparison = reference;
    comparison.driver_code = "LEC";
    comparison.driver_number = 16;

    const auto integrated = SpatialAlignmentEngine::integrate_distance(
        synthesize([](double) { return 250.0; }, 30.0, 0.24));
    const auto grid = SpatialAlignmentEngine::resample_to_grid(integrated, 5.0, 60.0);
    const auto channels = SpatialAlignmentEngine::align_and_compute_delta(grid, grid);
    const auto csv = SpatialAlignmentEngine::export_motec_csv(session, reference, comparison, channels);

    const auto data_start = csv.find("Distance,Time_Ref");
    check(data_start != std::string::npos, "o cabeçalho de colunas está presente");
    check(csv.find("\"Venue\",\"Sakhir\"") != std::string::npos, "o circuito real vai no cabeçalho");
    check(csv.find("\"Ref Driver\",\"VER\"") != std::string::npos, "o piloto real vai no cabeçalho");

    size_t rows = 0;
    for (size_t position = csv.find('\n', data_start); position != std::string::npos;
         position = csv.find('\n', position + 1)) {
        ++rows;
    }
    check(rows >= channels.size(), "há uma linha de dados por nó da grade (" +
                                       std::to_string(rows) + " >= " +
                                       std::to_string(channels.size()) + ")");
}

void test_empty_and_degenerate_inputs_are_safe() {
    std::cout << "Entradas degeneradas\n";

    check(SpatialAlignmentEngine::resample_to_grid({}, 5.0, 25.0).empty(), "grade de entrada vazia");
    check(SpatialAlignmentEngine::align_and_compute_delta({}, {}).empty(), "alinhamento vazio");
    check(SpatialAlignmentEngine::detect_corners({}).empty(), "detecção de curvas em vazio");
    check(SpatialAlignmentEngine::compute_microsectors({}, 100.0).empty(), "microsetores em vazio");
    check(SpatialAlignmentEngine::detect_segments({}, {}).empty(), "segmentos em vazio");

    const auto single = SpatialAlignmentEngine::integrate_distance({RawSample{0.0, 100.0, 0, 0, 0, 1, false}});
    check(single.size() == 1 && single.front().distance_m == 0.0, "uma única amostra fica no zero");
    check(SpatialAlignmentEngine::resample_to_grid(single, 5.0, 25.0).empty(),
          "uma única amostra não gera grade");

    const auto integrated = SpatialAlignmentEngine::integrate_distance(
        synthesize([](double) { return 200.0; }, 10.0, 0.25));
    check(SpatialAlignmentEngine::resample_to_grid(integrated, 0.0, 25.0).empty(),
          "passo de grade zero é rejeitado em vez de travar");
    check(SpatialAlignmentEngine::normalize_lap_distance(integrated, 0.0, 100.0).size() ==
              integrated.size(),
          "tempo de volta inválido devolve as amostras intactas");
}

} // namespace

int main() {
    std::cout << "=== APEX ANALYTICS — TESTES C++23 DO MOTOR ESPACIAL ===\n";
    test_distance_integration_is_exact_for_constant_speed();
    test_distance_is_monotonic();
    test_grid_resampling_preserves_measurements();
    test_adaptive_gap_tracks_sampling_rate();
    test_normalization_closes_the_delta_against_the_stopwatch();
    test_corner_detection_finds_real_minima();
    test_microsectors_partition_the_lap();
    test_quality_audit_reports_measured_coverage();
    test_json_is_well_formed_and_escapes_hostile_text();
    test_motec_export_has_one_row_per_grid_node();
    test_empty_and_degenerate_inputs_are_safe();

    if (failures == 0) {
        std::cout << "\nTodos os testes do motor de alinhamento espacial passaram.\n";
        return 0;
    }
    std::cout << "\n" << failures << " teste(s) do motor falharam.\n";
    return 1;
}

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace apex::analytics {

/** Amostra bruta da ECU já referenciada ao instante zero da volta. */
struct RawSample {
    double time_s{0.0};
    double speed_kmh{0.0};
    double throttle_pct{0.0};
    double brake_pct{0.0};
    int32_t rpm{0};
    int32_t gear{0};
    bool drs{false};
};

struct DistanceSample : RawSample {
    double distance_m{0.0};
};

struct AlignedGridPoint {
    double distance_m{0.0};
    double time_s{0.0};
    double speed_kmh{0.0};
    double throttle_pct{0.0};
    double brake_pct{0.0};
    int32_t rpm{0};
    int32_t gear{0};
    bool drs{false};
    /** Valor obtido por interpolação linear entre duas amostras vizinhas válidas. */
    bool is_interpolated{false};
    /** Lacuna acima do limite: o ponto repete a última amostra e não é confiável. */
    bool is_extrapolated{false};
};

struct ComparisonChannelPoint {
    double distance_m{0.0};
    double delta_time_s{0.0};
    AlignedGridPoint ref;
    AlignedGridPoint comp;
};

/**
 * Zona lenta detectada empiricamente como mínimo local de velocidade na volta de
 * referência. O rótulo é `C1..Cn` na ordem em que aparecem na volta: são curvas
 * *observadas na telemetria*, não a numeração oficial do circuito — a taxa de
 * amostragem da OpenF1 não resolve curvas tomadas em carga plena.
 */
struct Corner {
    int32_t index{0};
    std::string label;
    double entry_distance_m{0.0};
    double apex_distance_m{0.0};
    double exit_distance_m{0.0};
    double apex_speed_kmh{0.0};
};

/** Causa dominante da perda de tempo, derivada da evidência e não de heurística textual. */
enum class LossCause { ApexSpeed, ThrottleApplication, BrakingPoint, TopSpeed, Mixed };

std::string to_string(LossCause cause);

struct TelemetrySegment {
    std::string id;
    std::string corner_label;
    double distance_start_m{0.0};
    double distance_end_m{0.0};
    double apex_distance_m{0.0};
    double time_loss_s{0.0};
    double min_speed_ref_kmh{0.0};
    double min_speed_comp_kmh{0.0};
    double max_speed_ref_kmh{0.0};
    double max_speed_comp_kmh{0.0};
    double full_throttle_dist_ref_m{0.0};
    double full_throttle_dist_comp_m{0.0};
    double braking_point_ref_m{0.0};
    double braking_point_comp_m{0.0};
    double braking_point_diff_m{0.0};
    /** Trecho ancorado em uma zona lenta detectada; falso para retas. */
    bool is_corner{false};
    LossCause cause{LossCause::Mixed};
    std::string summary;
    double confidence{0.0};
};

struct Microsector {
    int32_t index{0};
    double distance_start_m{0.0};
    double distance_end_m{0.0};
    double delta_s{0.0};
    double ref_avg_speed_kmh{0.0};
    double comp_avg_speed_kmh{0.0};
    std::string winner{"EQUAL"};
};

struct SpeedTrap {
    std::string name;
    double distance_m{0.0};
    double ref_speed_kmh{0.0};
    double comp_speed_kmh{0.0};
    double delta_kmh{0.0};
    /** "openf1_marshalling_loop" quando vem do sensor oficial; "channel_peak" quando derivado. */
    std::string origin{"channel_peak"};
};

struct QualityMetrics {
    double max_interpolation_gap_m{0.0};
    int32_t discontinuous_segments{0};
    double coverage_pct{0.0};
    double confidence_score{0.0};
    double median_sample_interval_s{0.0};
    int32_t raw_samples_ref{0};
    int32_t raw_samples_comp{0};
    /**
     * |delta acumulado no fim da volta − diferença cronometrada oficial|.
     * É a verificação independente do alinhamento espacial: se o eixo de
     * distância estiver mal escalado, este número cresce imediatamente.
     */
    double delta_closure_error_s{0.0};
    std::string source_notes;
};

/** Metadados reais da volta; nenhum campo é inventado pelo motor numérico. */
struct LapMetadata {
    int32_t driver_number{0};
    std::string driver_code;
    std::string team_name;
    std::string team_colour;
    int32_t lap_number{0};
    double lap_time_s{0.0};
    std::string compound{"UNKNOWN"};
    int32_t stint_number{0};
    int32_t stint_lap{0};
    int32_t tyre_age_laps{0};
    double coverage_pct{0.0};
    int32_t samples_count{0};
    std::optional<double> sector_1_s;
    std::optional<double> sector_2_s;
    std::optional<double> sector_3_s;
    std::optional<double> i1_speed_kmh;
    std::optional<double> i2_speed_kmh;
    std::optional<double> st_speed_kmh;
};

struct SessionMetadata {
    int64_t session_key{0};
    int32_t circuit_key{0};
    std::string circuit_name;
    std::string country_name;
    std::string session_name;
    std::optional<double> track_temperature_c;
    std::optional<double> air_temperature_c;
    /** Origem verificável dos dados: "postgresql" ou "openf1-upstream". */
    std::string data_source{"unknown"};
};

class SpatialAlignmentEngine {
public:
    /**
     * Integração trapezoidal da distância percorrida:
     *   d[i] = d[i-1] + ((v[i] + v[i-1]) / 2) * (t[i] - t[i-1])
     */
    static std::vector<DistanceSample> integrate_distance(const std::vector<RawSample>& raw);

    /** Distância acumulada em um instante arbitrário, por interpolação linear. */
    static double distance_at_time(const std::vector<DistanceSample>& samples, double time_s);

    /**
     * Recorta a volta exatamente em `lap_time_s` e reescala o eixo de distância
     * para que o fim da volta caia em `target_length_m`.
     *
     * A integração trapezoidal a 4 Hz acumula um erro de escala de até ~1% que
     * difere entre pilotos. Sem esta normalização, o delta acumulado no fim da
     * volta não converge para a diferença cronometrada oficial. Com ela, os dois
     * eixos espaciais passam a medir a mesma pista e o delta final é auditável
     * contra o cronômetro.
     */
    static std::vector<DistanceSample> normalize_lap_distance(std::vector<DistanceSample> samples,
                                                              double lap_time_s,
                                                              double target_length_m);

    /**
     * Maior lacuna aceitável entre amostras antes de declarar descontinuidade.
     * Derivada do intervalo mediano real e da velocidade máxima medida: a 4 Hz e
     * 320 km/h, 21 m entre amostras é o regime normal, não um buraco de dados.
     */
    static double recommended_max_gap_m(const std::vector<DistanceSample>& samples,
                                        double grid_step_m);

    /**
     * Reamostra sobre grade métrica regular. Lacunas acima de `max_gap_m` não são
     * interpoladas: o ponto é marcado como extrapolado e entra no cálculo de cobertura.
     */
    static std::vector<AlignedGridPoint> resample_to_grid(const std::vector<DistanceSample>& samples,
                                                          double grid_step_m = 5.0,
                                                          double max_gap_m = 25.0);

    /** Delta(d) = tempo_comp(d) − tempo_ref(d); positivo significa comparação mais lenta. */
    static std::vector<ComparisonChannelPoint> align_and_compute_delta(
        const std::vector<AlignedGridPoint>& ref_grid,
        const std::vector<AlignedGridPoint>& comp_grid);

    /**
     * Detecta curvas como mínimos locais de velocidade com proeminência mínima,
     * a partir do traçado real de referência. Nenhuma coordenada é pré-cadastrada.
     */
    static std::vector<Corner> detect_corners(const std::vector<AlignedGridPoint>& ref_grid,
                                              double min_prominence_kmh = 25.0);

    /**
     * Isola os trechos onde a perda cumulativa de tempo cresce de forma consistente e
     * quantifica a evidência (ápice, ponto de freada, retomada de acelerador).
     */
    static std::vector<TelemetrySegment> detect_segments(
        const std::vector<ComparisonChannelPoint>& channels,
        const std::vector<Corner>& corners,
        size_t max_segments = 3,
        double min_time_loss_s = 0.02);

    static std::vector<Microsector> compute_microsectors(
        const std::vector<ComparisonChannelPoint>& channels, double microsector_len_m = 100.0);

    /**
     * Combina os sensores oficiais de passagem da OpenF1 (I1, I2, linha de chegada),
     * quando disponíveis, com os picos de velocidade efetivamente medidos nos canais.
     */
    static std::vector<SpeedTrap> compute_speed_traps(
        const std::vector<ComparisonChannelPoint>& channels, const LapMetadata& ref_lap,
        const LapMetadata& comp_lap);

    static QualityMetrics compute_quality_audit(const std::vector<DistanceSample>& ref_samples,
                                                const std::vector<DistanceSample>& comp_samples,
                                                const std::vector<AlignedGridPoint>& ref_grid,
                                                const std::vector<AlignedGridPoint>& comp_grid,
                                                double max_gap_m = 25.0);

    static std::string export_motec_csv(const SessionMetadata& session, const LapMetadata& ref_lap,
                                        const LapMetadata& comp_lap,
                                        const std::vector<ComparisonChannelPoint>& channels);

    /**
     * Serializa o contrato `lap-comparison` a partir exclusivamente de dados medidos.
     * `insights_json` é injetado pronto (array JSON) para permitir que o motor de
     * explicabilidade em Haskell produza a narrativa sem duplicar regras aqui.
     */
    static std::string build_comparison_json(const SessionMetadata& session,
                                             const LapMetadata& ref_lap, const LapMetadata& comp_lap,
                                             double grid_step_m,
                                             const std::vector<ComparisonChannelPoint>& channels,
                                             const std::vector<Corner>& corners,
                                             const std::vector<SpeedTrap>& speed_traps,
                                             const std::vector<Microsector>& microsectors,
                                             const QualityMetrics& quality,
                                             const std::string& insights_json);

    /** Serializa os segmentos detectados como evidência de entrada para o motor Haskell. */
    static std::string build_evidence_json(const SessionMetadata& session, const LapMetadata& ref_lap,
                                           const LapMetadata& comp_lap,
                                           const std::vector<TelemetrySegment>& segments);

    /** Fallback determinístico em C++ quando o motor Haskell não está acessível. */
    static std::string build_insights_json(const LapMetadata& comp_lap,
                                           const std::vector<TelemetrySegment>& segments,
                                           const QualityMetrics& quality, double grid_step_m);

    /** Escapa um texto para inclusão segura em string JSON. */
    static std::string json_escape(const std::string& text);
};

} // namespace apex::analytics

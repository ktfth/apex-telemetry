#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <optional>

namespace apex::analytics {

struct RawSample {
    double time_s{0.0};
    double speed_kmh{0.0};
    double throttle_pct{0.0};
    double brake_pct{0.0};
    int32_t rpm{0};
    int32_t gear{0};
    bool drs{false};
};

struct DistanceSample : public RawSample {
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
    bool is_interpolated{false};
};

struct ComparisonChannelPoint {
    double distance_m{0.0};
    double delta_time_s{0.0};
    AlignedGridPoint ref;
    AlignedGridPoint comp;
};

struct TelemetrySegment {
    std::string id;
    double distance_start_m{0.0};
    double distance_end_m{0.0};
    double time_loss_s{0.0};
    double min_speed_ref_kmh{0.0};
    double min_speed_comp_kmh{0.0};
    double full_throttle_dist_ref_m{0.0};
    double full_throttle_dist_comp_m{0.0};
    double braking_point_diff_m{0.0};
    std::string summary;
    double confidence{0.90};
};

struct QualityMetrics {
    double max_interpolation_gap_m{0.0};
    int32_t discontinuous_segments{0};
    double coverage_pct{100.0};
    double confidence_score{0.95};
    std::string source_notes{"OpenF1 Sakhir 10Hz processed by analytics-cpp v1.0.0"};
};

class SpatialAlignmentEngine {
public:
    /**
     * Integração trapezoidal de distância acumulada:
     * d[i] = d[i-1] + ((v[i] + v[i-1]) / 2) * (t[i] - t[i-1])
     */
    static std::vector<DistanceSample> integrate_distance(const std::vector<RawSample>& raw);

    /**
     * Reamostragem sobre grade regular fixa (ex: a cada 5,0 metros).
     * Salvaguarda: interpola linearmente apenas se a lacuna for <= max_gap_m.
     */
    static std::vector<AlignedGridPoint> resample_to_grid(
        const std::vector<DistanceSample>& samples,
        double grid_step_m = 5.0,
        double max_gap_m = 25.0
    );

    /**
     * Alinha canais e calcula delta de tempo cumulativo:
     * Delta(d) = TimeComp(d) - TimeRef(d)
     */
    static std::vector<ComparisonChannelPoint> align_and_compute_delta(
        const std::vector<AlignedGridPoint>& ref_grid,
        const std::vector<AlignedGridPoint>& comp_grid
    );

    /**
     * Identifica micro-setores com perda/ganho de tempo e ápices de curva.
     */
    static std::vector<TelemetrySegment> detect_segments(
        const std::vector<ComparisonChannelPoint>& channels
    );

    /**
     * Audita a integridade espacial e calcula métricas de qualidade.
     */
    static QualityMetrics compute_quality_audit(
        const std::vector<DistanceSample>& raw_samples,
        double max_gap_m = 25.0
    );

    /**
     * Gera payload JSON completo conforme contrato lap-comparison.json.
     */
    static std::string build_comparison_json(
        int64_t session_key,
        int32_t ref_driver,
        int32_t ref_lap,
        int32_t comp_driver,
        int32_t comp_lap,
        double grid_step_m,
        double total_distance_m,
        const std::vector<ComparisonChannelPoint>& channels,
        const std::vector<TelemetrySegment>& segments,
        const QualityMetrics& quality
    );
};

} // namespace apex::analytics

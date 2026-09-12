#include "spatial_alignment.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace apex::analytics {
namespace {

constexpr double kFullThrottlePct = 95.0;
constexpr double kBrakeOnsetPct = 20.0;

/** Média móvel centrada; preserva o comprimento e não desloca fase. */
std::vector<double> moving_average(const std::vector<double>& values, size_t half_window) {
    std::vector<double> smoothed(values.size(), 0.0);
    if (values.empty()) return smoothed;
    for (size_t i = 0; i < values.size(); ++i) {
        const size_t lo = i >= half_window ? i - half_window : 0;
        const size_t hi = std::min(values.size() - 1, i + half_window);
        double sum = 0.0;
        for (size_t j = lo; j <= hi; ++j) sum += values[j];
        smoothed[i] = sum / static_cast<double>(hi - lo + 1);
    }
    return smoothed;
}

double median(std::vector<double> values) {
    if (values.empty()) return 0.0;
    const size_t mid = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + static_cast<long>(mid), values.end());
    return values[mid];
}

/** Índice da grade mais próximo de uma distância alvo. */
size_t index_at_distance(const std::vector<ComparisonChannelPoint>& channels, double distance_m) {
    if (channels.empty()) return 0;
    const double step = channels.size() > 1 ? channels[1].distance_m - channels[0].distance_m : 5.0;
    if (step <= 0.0) return 0;
    const auto raw = static_cast<long long>(std::llround((distance_m - channels.front().distance_m) / step));
    return static_cast<size_t>(std::clamp<long long>(raw, 0, static_cast<long long>(channels.size()) - 1));
}

std::string format_number(double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    std::string text = oss.str();
    if (text == "-0.000" || text == "-0.00" || text == "-0.0" || text == "-0") {
        text.erase(text.begin());
    }
    return text;
}

void append_optional(std::ostringstream& oss, const char* key, const std::optional<double>& value,
                     int precision) {
    oss << "\"" << key << "\":";
    if (value) {
        oss << format_number(*value, precision);
    } else {
        oss << "null";
    }
}

} // namespace

std::string to_string(LossCause cause) {
    switch (cause) {
        case LossCause::ApexSpeed: return "APEX_SPEED";
        case LossCause::ThrottleApplication: return "THROTTLE_APPLICATION";
        case LossCause::BrakingPoint: return "BRAKING_POINT";
        case LossCause::TopSpeed: return "TOP_SPEED";
        case LossCause::Mixed: break;
    }
    return "MIXED";
}

std::string SpatialAlignmentEngine::json_escape(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (const unsigned char ch : text) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (ch < 0x20) {
                    char buffer[7];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
                    out += buffer;
                } else {
                    out += static_cast<char>(ch);
                }
        }
    }
    return out;
}

std::vector<DistanceSample> SpatialAlignmentEngine::integrate_distance(
    const std::vector<RawSample>& raw) {
    std::vector<DistanceSample> result;
    if (raw.empty()) return result;

    result.reserve(raw.size());
    double accumulated = 0.0;

    DistanceSample first;
    static_cast<RawSample&>(first) = raw.front();
    first.distance_m = 0.0;
    result.push_back(first);

    for (size_t i = 1; i < raw.size(); ++i) {
        double dt = raw[i].time_s - raw[i - 1].time_s;
        if (dt < 0.0) dt = 0.0;
        const double v_avg_ms = ((raw[i].speed_kmh + raw[i - 1].speed_kmh) / 2.0) / 3.6;
        accumulated += v_avg_ms * dt;

        DistanceSample sample;
        static_cast<RawSample&>(sample) = raw[i];
        sample.distance_m = accumulated;
        result.push_back(sample);
    }

    return result;
}

double SpatialAlignmentEngine::distance_at_time(const std::vector<DistanceSample>& samples,
                                                double time_s) {
    if (samples.empty()) return 0.0;
    if (time_s <= samples.front().time_s) return samples.front().distance_m;
    for (size_t i = 1; i < samples.size(); ++i) {
        if (samples[i].time_s >= time_s) {
            const double t0 = samples[i - 1].time_s;
            const double t1 = samples[i].time_s;
            const double alpha = (t1 - t0) > 1e-9 ? (time_s - t0) / (t1 - t0) : 0.0;
            return samples[i - 1].distance_m +
                   alpha * (samples[i].distance_m - samples[i - 1].distance_m);
        }
    }
    return samples.back().distance_m;
}

std::vector<DistanceSample> SpatialAlignmentEngine::normalize_lap_distance(
    std::vector<DistanceSample> samples, double lap_time_s, double target_length_m) {
    if (samples.size() < 2 || lap_time_s <= 0.0) return samples;

    // Descarta o que vier depois da linha de chegada.
    std::vector<DistanceSample> lap;
    lap.reserve(samples.size() + 2);
    for (const auto& sample : samples) {
        if (sample.time_s > lap_time_s) break;
        lap.push_back(sample);
    }
    if (lap.size() < 2) return samples;

    // A telemetria quase nunca começa exatamente no cruzamento da linha nem termina
    // nele. O trecho que falta em cada ponta é curto — no máximo um intervalo de
    // amostragem — e o carro estava em movimento nele. Fechamos as duas pontas
    // prolongando a última velocidade medida, e não fingindo distância nula: parar o
    // eixo espacial antes da linha empurraria o tempo restante para fora da grade e o
    // delta acumulado deixaria de fechar com o cronômetro oficial.
    constexpr double kMaxEdgeExtrapolationS = 2.0;

    if (lap.front().time_s > 0.0 && lap.front().time_s <= kMaxEdgeExtrapolationS) {
        DistanceSample start = lap.front();
        const double travelled = start.time_s * (start.speed_kmh / 3.6);
        start.time_s = 0.0;
        start.distance_m -= travelled;
        lap.insert(lap.begin(), start);
    }

    const double tail_s = lap_time_s - lap.back().time_s;
    if (tail_s > 0.0 && tail_s <= kMaxEdgeExtrapolationS) {
        DistanceSample finish = lap.back();
        finish.distance_m += tail_s * (finish.speed_kmh / 3.6);
        finish.time_s = lap_time_s;
        lap.push_back(finish);
    }

    // Reancora a origem do eixo na linha de chegada.
    const double origin = lap.front().distance_m;
    for (auto& sample : lap) sample.distance_m -= origin;

    const double measured_length = lap.back().distance_m;
    if (measured_length <= 1.0) return samples;

    if (target_length_m > 1.0) {
        const double scale = target_length_m / measured_length;
        for (auto& sample : lap) sample.distance_m *= scale;
    }
    return lap;
}

double SpatialAlignmentEngine::recommended_max_gap_m(const std::vector<DistanceSample>& samples,
                                                     double grid_step_m) {
    if (samples.size() < 3) return std::max(25.0, grid_step_m * 5.0);

    std::vector<double> intervals;
    intervals.reserve(samples.size() - 1);
    double max_speed_kmh = 0.0;
    for (size_t i = 1; i < samples.size(); ++i) {
        intervals.push_back(samples[i].time_s - samples[i - 1].time_s);
        max_speed_kmh = std::max(max_speed_kmh, samples[i].speed_kmh);
    }
    const double median_dt = median(std::move(intervals));
    const double nominal_gap_m = median_dt * (max_speed_kmh / 3.6);
    // 2,5x o passo nominal em velocidade máxima: tolera jitter do transponder sem
    // deixar passar uma perda real de sinal.
    return std::max({grid_step_m * 3.0, 25.0, nominal_gap_m * 2.5});
}

std::vector<AlignedGridPoint> SpatialAlignmentEngine::resample_to_grid(
    const std::vector<DistanceSample>& samples, double grid_step_m, double max_gap_m) {
    std::vector<AlignedGridPoint> grid;
    if (samples.size() < 2 || grid_step_m <= 0.0) return grid;

    const double max_distance = samples.back().distance_m;
    grid.reserve(static_cast<size_t>(max_distance / grid_step_m) + 2);
    size_t cursor = 0;

    for (double target = 0.0; target <= max_distance; target += grid_step_m) {
        while (cursor + 1 < samples.size() && samples[cursor + 1].distance_m < target) ++cursor;
        if (cursor + 1 >= samples.size()) break;

        const auto& p0 = samples[cursor];
        const auto& p1 = samples[cursor + 1];
        const double gap = p1.distance_m - p0.distance_m;

        AlignedGridPoint point;
        point.distance_m = target;

        if (gap > max_gap_m) {
            // Lacuna acima do tolerado: repetimos a última amostra medida e marcamos o
            // ponto como não confiável em vez de fabricar uma transição suave.
            point.time_s = p0.time_s;
            point.speed_kmh = p0.speed_kmh;
            point.throttle_pct = p0.throttle_pct;
            point.brake_pct = p0.brake_pct;
            point.rpm = p0.rpm;
            point.gear = p0.gear;
            point.drs = p0.drs;
            point.is_extrapolated = true;
            grid.push_back(point);
            continue;
        }

        double alpha = gap > 1e-6 ? (target - p0.distance_m) / gap : 0.0;
        alpha = std::clamp(alpha, 0.0, 1.0);

        point.time_s = p0.time_s + alpha * (p1.time_s - p0.time_s);
        point.speed_kmh = p0.speed_kmh + alpha * (p1.speed_kmh - p0.speed_kmh);
        point.throttle_pct = p0.throttle_pct + alpha * (p1.throttle_pct - p0.throttle_pct);
        point.brake_pct = p0.brake_pct + alpha * (p1.brake_pct - p0.brake_pct);
        point.rpm = static_cast<int32_t>(std::lround(p0.rpm + alpha * (p1.rpm - p0.rpm)));
        point.gear = alpha < 0.5 ? p0.gear : p1.gear; // canais discretos não se interpolam
        point.drs = alpha < 0.5 ? p0.drs : p1.drs;
        point.is_interpolated = alpha > 0.001 && alpha < 0.999;
        grid.push_back(point);
    }

    return grid;
}

std::vector<ComparisonChannelPoint> SpatialAlignmentEngine::align_and_compute_delta(
    const std::vector<AlignedGridPoint>& ref_grid, const std::vector<AlignedGridPoint>& comp_grid) {
    std::vector<ComparisonChannelPoint> channels;
    const size_t count = std::min(ref_grid.size(), comp_grid.size());
    channels.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        ComparisonChannelPoint point;
        point.distance_m = ref_grid[i].distance_m;
        point.delta_time_s = comp_grid[i].time_s - ref_grid[i].time_s;
        point.ref = ref_grid[i];
        point.comp = comp_grid[i];
        channels.push_back(point);
    }

    return channels;
}

std::vector<Corner> SpatialAlignmentEngine::detect_corners(
    const std::vector<AlignedGridPoint>& ref_grid, double min_prominence_kmh) {
    std::vector<Corner> corners;
    if (ref_grid.size() < 40) return corners;

    const double step_m = ref_grid.size() > 1 ? ref_grid[1].distance_m - ref_grid[0].distance_m : 5.0;
    if (step_m <= 0.0) return corners;

    std::vector<double> speed;
    speed.reserve(ref_grid.size());
    for (const auto& point : ref_grid) speed.push_back(point.speed_kmh);
    const auto smoothed = moving_average(speed, static_cast<size_t>(std::max(1.0, 15.0 / step_m)));

    const auto window = static_cast<size_t>(std::max(4.0, 70.0 / step_m));
    std::vector<size_t> apex_indices;

    for (size_t i = window; i + window < smoothed.size(); ++i) {
        const auto lo = i - window;
        const auto hi = i + window;
        const auto local_min = std::min_element(smoothed.begin() + static_cast<long>(lo),
                                                smoothed.begin() + static_cast<long>(hi) + 1);
        if (static_cast<size_t>(local_min - smoothed.begin()) != i) continue;

        const auto peak_before = *std::max_element(smoothed.begin() + static_cast<long>(lo),
                                                   smoothed.begin() + static_cast<long>(i) + 1);
        const auto peak_after = *std::max_element(smoothed.begin() + static_cast<long>(i),
                                                  smoothed.begin() + static_cast<long>(hi) + 1);
        const double prominence = std::min(peak_before, peak_after) - smoothed[i];
        if (prominence < min_prominence_kmh) continue;

        // Funde ápices muito próximos: uma chicane é uma curva composta, não duas.
        if (!apex_indices.empty() &&
            (ref_grid[i].distance_m - ref_grid[apex_indices.back()].distance_m) < 90.0) {
            if (smoothed[i] < smoothed[apex_indices.back()]) apex_indices.back() = i;
            continue;
        }
        apex_indices.push_back(i);
    }

    corners.reserve(apex_indices.size());
    int32_t label_index = 1;
    for (const size_t apex : apex_indices) {
        Corner corner;
        corner.index = label_index;
        corner.label = "C" + std::to_string(label_index);
        corner.apex_distance_m = ref_grid[apex].distance_m;
        corner.apex_speed_kmh = ref_grid[apex].speed_kmh;

        // Entrada: último ponto antes do ápice em que o carro ainda estava a 95% do pico.
        size_t entry = apex;
        double peak_before = smoothed[apex];
        for (size_t j = apex; j > 0 && (ref_grid[apex].distance_m - ref_grid[j].distance_m) < 400.0; --j) {
            peak_before = std::max(peak_before, smoothed[j]);
        }
        for (size_t j = apex; j > 0; --j) {
            if (smoothed[j] >= peak_before * 0.95) { entry = j; break; }
            entry = j;
        }

        size_t exit = apex;
        double peak_after = smoothed[apex];
        for (size_t j = apex; j < smoothed.size() && (ref_grid[j].distance_m - ref_grid[apex].distance_m) < 400.0; ++j) {
            peak_after = std::max(peak_after, smoothed[j]);
        }
        for (size_t j = apex; j < smoothed.size(); ++j) {
            if (smoothed[j] >= peak_after * 0.95) { exit = j; break; }
            exit = j;
        }

        corner.entry_distance_m = ref_grid[entry].distance_m;
        corner.exit_distance_m = ref_grid[exit].distance_m;
        corners.push_back(std::move(corner));
        ++label_index;
    }

    return corners;
}

std::vector<TelemetrySegment> SpatialAlignmentEngine::detect_segments(
    const std::vector<ComparisonChannelPoint>& channels, const std::vector<Corner>& corners,
    size_t max_segments, double min_time_loss_s) {
    std::vector<TelemetrySegment> segments;
    if (channels.size() < 20) return segments;

    const double total_distance = channels.back().distance_m;

    // Janelas candidatas: uma por curva detectada; na ausência de curvas, uma varredura
    // regular de 300 m garante que a análise nunca dependa de geometria pré-cadastrada.
    struct Window {
        double start_m;
        double end_m;
        std::string label;
        double apex_m;
        bool is_corner;
    };
    std::vector<Window> windows;

    if (!corners.empty()) {
        for (const auto& corner : corners) {
            windows.push_back({std::max(0.0, corner.entry_distance_m - 50.0),
                               std::min(total_distance, corner.exit_distance_m + 50.0), corner.label,
                               corner.apex_distance_m, true});
        }
        // Retas entre curvas: perda de ponta também precisa ser explicável.
        for (size_t i = 0; i + 1 < corners.size(); ++i) {
            const double start = corners[i].exit_distance_m;
            const double end = corners[i + 1].entry_distance_m;
            if (end - start >= 250.0) {
                windows.push_back({start, end,
                                   "Reta " + corners[i].label + "\u2192" + corners[i + 1].label,
                                   (start + end) / 2.0, false});
            }
        }
    } else {
        for (double start = 0.0; start < total_distance; start += 300.0) {
            const double end = std::min(start + 300.0, total_distance);
            if (end - start < 100.0) break;
            windows.push_back({start, end,
                               "Trecho " + format_number(start, 0) + "-" + format_number(end, 0) + " m",
                               (start + end) / 2.0, false});
        }
    }

    std::vector<TelemetrySegment> candidates;
    candidates.reserve(windows.size());

    for (const auto& window : windows) {
        const size_t start_idx = index_at_distance(channels, window.start_m);
        const size_t end_idx = index_at_distance(channels, window.end_m);
        if (end_idx <= start_idx + 2) continue;

        TelemetrySegment segment;
        segment.distance_start_m = channels[start_idx].distance_m;
        segment.distance_end_m = channels[end_idx].distance_m;
        segment.corner_label = window.label;
        segment.is_corner = window.is_corner;
        segment.time_loss_s = channels[end_idx].delta_time_s - channels[start_idx].delta_time_s;

        segment.min_speed_ref_kmh = channels[start_idx].ref.speed_kmh;
        segment.min_speed_comp_kmh = channels[start_idx].comp.speed_kmh;
        segment.max_speed_ref_kmh = channels[start_idx].ref.speed_kmh;
        segment.max_speed_comp_kmh = channels[start_idx].comp.speed_kmh;

        size_t apex_idx = start_idx;
        size_t extrapolated = 0;
        for (size_t i = start_idx; i <= end_idx; ++i) {
            const auto& point = channels[i];
            if (point.ref.speed_kmh < segment.min_speed_ref_kmh) {
                segment.min_speed_ref_kmh = point.ref.speed_kmh;
                apex_idx = i;
            }
            segment.min_speed_comp_kmh = std::min(segment.min_speed_comp_kmh, point.comp.speed_kmh);
            segment.max_speed_ref_kmh = std::max(segment.max_speed_ref_kmh, point.ref.speed_kmh);
            segment.max_speed_comp_kmh = std::max(segment.max_speed_comp_kmh, point.comp.speed_kmh);
            if (point.ref.is_extrapolated || point.comp.is_extrapolated) ++extrapolated;
        }
        segment.apex_distance_m = window.is_corner ? window.apex_m : channels[apex_idx].distance_m;

        // Ponto de freada: primeira aplicação relevante de freio dentro da janela.
        segment.braking_point_ref_m = 0.0;
        segment.braking_point_comp_m = 0.0;
        for (size_t i = start_idx; i <= end_idx; ++i) {
            if (segment.braking_point_ref_m == 0.0 && channels[i].ref.brake_pct >= kBrakeOnsetPct) {
                segment.braking_point_ref_m = channels[i].distance_m;
            }
            if (segment.braking_point_comp_m == 0.0 && channels[i].comp.brake_pct >= kBrakeOnsetPct) {
                segment.braking_point_comp_m = channels[i].distance_m;
            }
        }
        segment.braking_point_diff_m =
            (segment.braking_point_ref_m > 0.0 && segment.braking_point_comp_m > 0.0)
                ? segment.braking_point_comp_m - segment.braking_point_ref_m
                : 0.0;

        // Retomada: primeira vez, após o ápice, em que o acelerador ultrapassa 95%.
        const size_t apex_search = index_at_distance(channels, segment.apex_distance_m);
        segment.full_throttle_dist_ref_m = 0.0;
        segment.full_throttle_dist_comp_m = 0.0;
        for (size_t i = apex_search; i <= end_idx; ++i) {
            if (segment.full_throttle_dist_ref_m == 0.0 &&
                channels[i].ref.throttle_pct >= kFullThrottlePct) {
                segment.full_throttle_dist_ref_m = channels[i].distance_m;
            }
            if (segment.full_throttle_dist_comp_m == 0.0 &&
                channels[i].comp.throttle_pct >= kFullThrottlePct) {
                segment.full_throttle_dist_comp_m = channels[i].distance_m;
            }
        }

        // Causa dominante: a evidência de maior peso relativo, nunca um rótulo fixo.
        const double apex_deficit = segment.min_speed_ref_kmh - segment.min_speed_comp_kmh;
        const double throttle_delay =
            (segment.full_throttle_dist_ref_m > 0.0 && segment.full_throttle_dist_comp_m > 0.0)
                ? segment.full_throttle_dist_comp_m - segment.full_throttle_dist_ref_m
                : 0.0;
        const double top_speed_deficit = segment.max_speed_ref_kmh - segment.max_speed_comp_kmh;
        const double early_braking = -segment.braking_point_diff_m;

        // Em uma reta não há ápice: atribuir a perda à "velocidade de ápice" seria
        // descrever um fenômeno que não existe naquele trecho.
        const double score_apex = window.is_corner ? apex_deficit / 3.0 : 0.0;
        const double score_throttle = throttle_delay / 12.0;
        const double score_braking = early_braking / 8.0;
        const double score_top = window.is_corner ? 0.0 : top_speed_deficit / 4.0;
        const double best = std::max({score_apex, score_throttle, score_braking, score_top});

        if (best < 1.0) {
            segment.cause = LossCause::Mixed;
        } else if (best == score_apex) {
            segment.cause = LossCause::ApexSpeed;
        } else if (best == score_throttle) {
            segment.cause = LossCause::ThrottleApplication;
        } else if (best == score_braking) {
            segment.cause = LossCause::BrakingPoint;
        } else {
            segment.cause = LossCause::TopSpeed;
        }

        const double span = static_cast<double>(end_idx - start_idx + 1);
        const double window_coverage = 1.0 - static_cast<double>(extrapolated) / span;
        segment.confidence = std::clamp(
            0.55 + 0.35 * window_coverage + std::min(0.10, std::abs(segment.time_loss_s) * 0.5), 0.0,
            0.99);

        segment.id = "seg-" + format_number(segment.distance_start_m, 0) + "-" +
                     format_number(segment.distance_end_m, 0);
        candidates.push_back(std::move(segment));
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const TelemetrySegment& a, const TelemetrySegment& b) {
                  return a.time_loss_s > b.time_loss_s;
              });

    for (auto& candidate : candidates) {
        if (segments.size() >= max_segments) break;
        if (candidate.time_loss_s < min_time_loss_s) break;

        std::ostringstream summary;
        summary << std::fixed << std::setprecision(3);
        summary << candidate.corner_label << ": perda de " << format_number(candidate.time_loss_s, 3)
                << " s entre " << format_number(candidate.distance_start_m, 0) << " m e "
                << format_number(candidate.distance_end_m, 0) << " m. ";

        switch (candidate.cause) {
            case LossCause::ApexSpeed:
                summary << "Velocidade de ápice "
                        << format_number(candidate.min_speed_ref_kmh - candidate.min_speed_comp_kmh, 1)
                        << " km/h inferior no ápice ("
                        << format_number(candidate.min_speed_comp_kmh, 1) << " contra "
                        << format_number(candidate.min_speed_ref_kmh, 1) << " km/h).";
                break;
            case LossCause::ThrottleApplication:
                summary << "Acelerador acima de 95% aplicado "
                        << format_number(candidate.full_throttle_dist_comp_m -
                                             candidate.full_throttle_dist_ref_m,
                                         0)
                        << " m mais tarde"
                        << (candidate.is_corner ? " na saída da curva." : " neste trecho.");
                break;
            case LossCause::BrakingPoint:
                summary << "Freada iniciada "
                        << format_number(-candidate.braking_point_diff_m, 0)
                        << " m antes da referência.";
                break;
            case LossCause::TopSpeed:
                summary << "Velocidade de ponta "
                        << format_number(candidate.max_speed_ref_kmh - candidate.max_speed_comp_kmh, 1)
                        << " km/h inferior na reta.";
                break;
            case LossCause::Mixed:
                summary << "Nenhum canal isolado domina a perda: diferença distribuída ao longo do "
                           "trecho.";
                break;
        }

        candidate.summary = summary.str();
        segments.push_back(candidate);
    }

    return segments;
}

std::vector<Microsector> SpatialAlignmentEngine::compute_microsectors(
    const std::vector<ComparisonChannelPoint>& channels, double microsector_len_m) {
    std::vector<Microsector> microsectors;
    if (channels.empty() || microsector_len_m <= 0.0) return microsectors;

    const double max_distance = channels.back().distance_m;
    int32_t index = 0;

    for (double start = 0.0; start < max_distance; start += microsector_len_m) {
        const double end = std::min(start + microsector_len_m, max_distance);
        const size_t start_idx = index_at_distance(channels, start);
        const size_t end_idx = index_at_distance(channels, end);
        if (end_idx <= start_idx) continue;

        double sum_ref = 0.0;
        double sum_comp = 0.0;
        for (size_t i = start_idx; i <= end_idx; ++i) {
            sum_ref += channels[i].ref.speed_kmh;
            sum_comp += channels[i].comp.speed_kmh;
        }
        const auto count = static_cast<double>(end_idx - start_idx + 1);

        Microsector microsector;
        microsector.index = index++;
        microsector.distance_start_m = channels[start_idx].distance_m;
        microsector.distance_end_m = channels[end_idx].distance_m;
        microsector.delta_s = channels[end_idx].delta_time_s - channels[start_idx].delta_time_s;
        microsector.ref_avg_speed_kmh = sum_ref / count;
        microsector.comp_avg_speed_kmh = sum_comp / count;
        microsector.winner = microsector.delta_s > 0.001
                                 ? "REF"
                                 : (microsector.delta_s < -0.001 ? "COMP" : "EQUAL");
        microsectors.push_back(microsector);
    }

    return microsectors;
}

std::vector<SpeedTrap> SpatialAlignmentEngine::compute_speed_traps(
    const std::vector<ComparisonChannelPoint>& channels, const LapMetadata& ref_lap,
    const LapMetadata& comp_lap) {
    std::vector<SpeedTrap> traps;
    if (channels.size() < 10) return traps;

    /**
     * Os sensores oficiais I1 e I2 ficam nas linhas de setor. Como conhecemos o tempo
     * real de cada setor, localizamos a linha percorrendo o canal de tempo da volta de
     * referência até o instante correspondente — a distância resultante é medida, não
     * estimada a partir de um mapa fixo de circuito.
     */
    const auto distance_at_elapsed = [&channels](double elapsed_s) -> std::optional<double> {
        if (elapsed_s <= 0.0) return std::nullopt;
        for (size_t i = 1; i < channels.size(); ++i) {
            if (channels[i].ref.time_s >= elapsed_s) {
                const double t0 = channels[i - 1].ref.time_s;
                const double t1 = channels[i].ref.time_s;
                const double alpha = (t1 - t0) > 1e-9 ? (elapsed_s - t0) / (t1 - t0) : 0.0;
                return channels[i - 1].distance_m +
                       alpha * (channels[i].distance_m - channels[i - 1].distance_m);
            }
        }
        return std::nullopt;
    };

    const auto push_official = [&](const char* name, std::optional<double> distance,
                                   std::optional<double> ref_speed, std::optional<double> comp_speed) {
        if (!distance || !ref_speed || !comp_speed) return;
        SpeedTrap trap;
        trap.name = name;
        trap.distance_m = *distance;
        trap.ref_speed_kmh = *ref_speed;
        trap.comp_speed_kmh = *comp_speed;
        trap.delta_kmh = *comp_speed - *ref_speed;
        trap.origin = "openf1_marshalling_loop";
        traps.push_back(std::move(trap));
    };

    if (ref_lap.sector_1_s) {
        push_official("Intermediate 1", distance_at_elapsed(*ref_lap.sector_1_s),
                      ref_lap.i1_speed_kmh, comp_lap.i1_speed_kmh);
    }
    if (ref_lap.sector_1_s && ref_lap.sector_2_s) {
        push_official("Intermediate 2", distance_at_elapsed(*ref_lap.sector_1_s + *ref_lap.sector_2_s),
                      ref_lap.i2_speed_kmh, comp_lap.i2_speed_kmh);
    }

    // Speed trap principal: o pico real de velocidade medido na volta de referência.
    const auto fastest = std::max_element(channels.begin(), channels.end(),
                                          [](const auto& a, const auto& b) {
                                              return a.ref.speed_kmh < b.ref.speed_kmh;
                                          });
    if (ref_lap.st_speed_kmh && comp_lap.st_speed_kmh && fastest != channels.end()) {
        push_official("Speed Trap", fastest->distance_m, ref_lap.st_speed_kmh, comp_lap.st_speed_kmh);
    }

    // Picos de canal: fim de cada reta, com proeminência mínima para não captar ruído.
    const double step_m = channels.size() > 1 ? channels[1].distance_m - channels[0].distance_m : 5.0;
    const auto window = static_cast<size_t>(std::max(4.0, 120.0 / std::max(1.0, step_m)));
    int32_t peak_index = 1;

    for (size_t i = window; i + window < channels.size(); ++i) {
        bool is_peak = true;
        for (size_t j = i - window; j <= i + window; ++j) {
            if (channels[j].ref.speed_kmh > channels[i].ref.speed_kmh) { is_peak = false; break; }
        }
        if (!is_peak) continue;

        double valley = channels[i].ref.speed_kmh;
        for (size_t j = i - window; j <= i + window; ++j) {
            valley = std::min(valley, channels[j].ref.speed_kmh);
        }
        if (channels[i].ref.speed_kmh - valley < 30.0) continue;

        const bool duplicates_official =
            std::any_of(traps.begin(), traps.end(), [&](const SpeedTrap& trap) {
                return std::abs(trap.distance_m - channels[i].distance_m) < 150.0;
            });
        if (duplicates_official) continue;

        SpeedTrap trap;
        trap.name = "Pico de reta " + std::to_string(peak_index++);
        trap.distance_m = channels[i].distance_m;
        trap.ref_speed_kmh = channels[i].ref.speed_kmh;
        trap.comp_speed_kmh = channels[i].comp.speed_kmh;
        trap.delta_kmh = trap.comp_speed_kmh - trap.ref_speed_kmh;
        trap.origin = "channel_peak";
        traps.push_back(std::move(trap));
    }

    std::sort(traps.begin(), traps.end(),
              [](const SpeedTrap& a, const SpeedTrap& b) { return a.distance_m < b.distance_m; });
    return traps;
}

QualityMetrics SpatialAlignmentEngine::compute_quality_audit(
    const std::vector<DistanceSample>& ref_samples, const std::vector<DistanceSample>& comp_samples,
    const std::vector<AlignedGridPoint>& ref_grid, const std::vector<AlignedGridPoint>& comp_grid,
    double max_gap_m) {
    QualityMetrics quality;
    quality.raw_samples_ref = static_cast<int32_t>(ref_samples.size());
    quality.raw_samples_comp = static_cast<int32_t>(comp_samples.size());

    std::vector<double> intervals;
    for (const auto* samples : {&ref_samples, &comp_samples}) {
        for (size_t i = 1; i < samples->size(); ++i) {
            const double gap = (*samples)[i].distance_m - (*samples)[i - 1].distance_m;
            quality.max_interpolation_gap_m = std::max(quality.max_interpolation_gap_m, gap);
            if (gap > max_gap_m) ++quality.discontinuous_segments;
            if (samples == &ref_samples) {
                intervals.push_back((*samples)[i].time_s - (*samples)[i - 1].time_s);
            }
        }
    }
    quality.median_sample_interval_s = median(std::move(intervals));

    size_t extrapolated = 0;
    size_t total = 0;
    for (const auto* grid : {&ref_grid, &comp_grid}) {
        for (const auto& point : *grid) {
            ++total;
            if (point.is_extrapolated) ++extrapolated;
        }
    }
    quality.coverage_pct =
        total > 0 ? 100.0 * (1.0 - static_cast<double>(extrapolated) / static_cast<double>(total))
                  : 0.0;

    // A confiança é uma função contínua da cobertura e da maior lacuna observada.
    const double coverage_term = quality.coverage_pct / 100.0;
    const double gap_term =
        quality.max_interpolation_gap_m <= 0.0
            ? 1.0
            : std::clamp(1.0 - (quality.max_interpolation_gap_m - max_gap_m) / (max_gap_m * 4.0), 0.3,
                         1.0);
    quality.confidence_score = std::clamp(coverage_term * gap_term, 0.0, 0.99);

    std::ostringstream notes;
    notes << "Distância integrada por regra trapezoidal sobre " << quality.raw_samples_ref << "/"
          << quality.raw_samples_comp << " amostras brutas de ECU (intervalo mediano "
          << format_number(quality.median_sample_interval_s, 3)
          << " s); reamostragem linear limitada a lacunas de " << format_number(max_gap_m, 1) << " m.";
    quality.source_notes = notes.str();

    return quality;
}

std::string SpatialAlignmentEngine::export_motec_csv(
    const SessionMetadata& session, const LapMetadata& ref_lap, const LapMetadata& comp_lap,
    const std::vector<ComparisonChannelPoint>& channels) {
    std::ostringstream csv;
    csv << std::fixed << std::setprecision(3);

    const auto quoted = [](const std::string& text) {
        std::string escaped;
        escaped.reserve(text.size());
        for (const char ch : text) {
            if (ch == '"') escaped += '"';
            escaped += ch;
        }
        return "\"" + escaped + "\"";
    };

    csv << "\"Format\",\"MoTeC CSV Telemetry Export\"\n"
        << "\"Venue\"," << quoted(session.circuit_name) << "\n"
        << "\"Vehicle\",\"Formula 1\"\n"
        << "\"Session\"," << quoted(session.session_name) << "\n"
        << "\"Session Key\"," << session.session_key << "\n"
        << "\"Data Source\"," << quoted(session.data_source) << "\n"
        << "\"Ref Driver\"," << quoted(ref_lap.driver_code) << "," << ref_lap.driver_number
        << ",\"Lap\"," << ref_lap.lap_number << ",\"Time\"," << ref_lap.lap_time_s << ",\"Compound\","
        << quoted(ref_lap.compound) << "\n"
        << "\"Comp Driver\"," << quoted(comp_lap.driver_code) << "," << comp_lap.driver_number
        << ",\"Lap\"," << comp_lap.lap_number << ",\"Time\"," << comp_lap.lap_time_s
        << ",\"Compound\"," << quoted(comp_lap.compound) << "\n";
    if (session.track_temperature_c) {
        csv << "\"Track Temp C\"," << *session.track_temperature_c << "\n";
    }
    csv << "\"Sample Count\"," << channels.size() << "\n\n";

    csv << "Distance,Time_Ref,Time_Comp,Delta_Time,Speed_Ref,Speed_Comp,Throttle_Ref,Throttle_Comp,"
           "Brake_Ref,Brake_Comp,Gear_Ref,Gear_Comp,DRS_Ref,DRS_Comp,RPM_Ref,RPM_Comp\n"
        << "m,s,s,s,km/h,km/h,%,%,%,%,,,,,rpm,rpm\n\n";

    for (const auto& channel : channels) {
        csv << channel.distance_m << ',' << channel.ref.time_s << ',' << channel.comp.time_s << ','
            << channel.delta_time_s << ',' << channel.ref.speed_kmh << ',' << channel.comp.speed_kmh
            << ',' << channel.ref.throttle_pct << ',' << channel.comp.throttle_pct << ','
            << channel.ref.brake_pct << ',' << channel.comp.brake_pct << ',' << channel.ref.gear << ','
            << channel.comp.gear << ',' << (channel.ref.drs ? 1 : 0) << ','
            << (channel.comp.drs ? 1 : 0) << ',' << channel.ref.rpm << ',' << channel.comp.rpm << '\n';
    }

    return csv.str();
}

std::string SpatialAlignmentEngine::build_evidence_json(
    const SessionMetadata& session, const LapMetadata& ref_lap, const LapMetadata& comp_lap,
    const std::vector<TelemetrySegment>& segments) {
    std::ostringstream oss;
    oss << "{\"session\":{\"session_key\":" << session.session_key << ",\"circuit_name\":\""
        << json_escape(session.circuit_name) << "\",\"session_name\":\""
        << json_escape(session.session_name) << "\",";
    append_optional(oss, "track_temperature_c", session.track_temperature_c, 1);
    oss << "},\"reference\":{\"driver_number\":" << ref_lap.driver_number << ",\"driver_code\":\""
        << json_escape(ref_lap.driver_code) << "\",\"lap_number\":" << ref_lap.lap_number
        << ",\"lap_time_s\":" << format_number(ref_lap.lap_time_s, 3) << ",\"compound\":\""
        << json_escape(ref_lap.compound) << "\",\"tyre_age_laps\":" << ref_lap.tyre_age_laps
        << "},\"comparison\":{\"driver_number\":" << comp_lap.driver_number << ",\"driver_code\":\""
        << json_escape(comp_lap.driver_code) << "\",\"lap_number\":" << comp_lap.lap_number
        << ",\"lap_time_s\":" << format_number(comp_lap.lap_time_s, 3) << ",\"compound\":\""
        << json_escape(comp_lap.compound) << "\",\"tyre_age_laps\":" << comp_lap.tyre_age_laps
        << "},\"segments\":[";

    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& segment = segments[i];
        if (i > 0) oss << ',';
        oss << "{\"id\":\"" << json_escape(segment.id) << "\",\"corner_label\":\""
            << json_escape(segment.corner_label)
            << "\",\"distance_start_m\":" << format_number(segment.distance_start_m, 1)
            << ",\"distance_end_m\":" << format_number(segment.distance_end_m, 1)
            << ",\"apex_distance_m\":" << format_number(segment.apex_distance_m, 1)
            << ",\"time_loss_s\":" << format_number(segment.time_loss_s, 4)
            << ",\"min_speed_ref_kmh\":" << format_number(segment.min_speed_ref_kmh, 1)
            << ",\"min_speed_comp_kmh\":" << format_number(segment.min_speed_comp_kmh, 1)
            << ",\"max_speed_ref_kmh\":" << format_number(segment.max_speed_ref_kmh, 1)
            << ",\"max_speed_comp_kmh\":" << format_number(segment.max_speed_comp_kmh, 1)
            << ",\"full_throttle_distance_ref_m\":" << format_number(segment.full_throttle_dist_ref_m, 1)
            << ",\"full_throttle_distance_comp_m\":"
            << format_number(segment.full_throttle_dist_comp_m, 1)
            << ",\"braking_point_ref_m\":" << format_number(segment.braking_point_ref_m, 1)
            << ",\"braking_point_comp_m\":" << format_number(segment.braking_point_comp_m, 1)
            << ",\"braking_point_diff_m\":" << format_number(segment.braking_point_diff_m, 1)
            << ",\"is_corner\":" << (segment.is_corner ? "true" : "false") << ",\"cause\":\""
            << to_string(segment.cause) << "\",\"confidence\":"
            << format_number(segment.confidence, 3) << "}";
    }

    oss << "]}";
    return oss.str();
}

std::string SpatialAlignmentEngine::build_insights_json(const LapMetadata& comp_lap,
                                                        const std::vector<TelemetrySegment>& segments,
                                                        const QualityMetrics& quality,
                                                        double grid_step_m) {
    std::ostringstream oss;
    oss << '[';
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& segment = segments[i];
        if (i > 0) oss << ',';
        oss << "{\"id\":\"" << json_escape(segment.id) << "\",\"driver_code\":\""
            << json_escape(comp_lap.driver_code)
            << "\",\"distance_start_m\":" << format_number(segment.distance_start_m, 1)
            << ",\"distance_end_m\":" << format_number(segment.distance_end_m, 1)
            << ",\"time_loss_s\":" << format_number(segment.time_loss_s, 3) << ",\"summary\":\""
            << json_escape(segment.summary) << "\",\"evidence\":{\"min_speed_ref_kmh\":"
            << format_number(segment.min_speed_ref_kmh, 1)
            << ",\"min_speed_comp_kmh\":" << format_number(segment.min_speed_comp_kmh, 1)
            << ",\"full_throttle_distance_ref_m\":" << format_number(segment.full_throttle_dist_ref_m, 1)
            << ",\"full_throttle_distance_comp_m\":"
            << format_number(segment.full_throttle_dist_comp_m, 1)
            << ",\"braking_point_diff_m\":" << format_number(segment.braking_point_diff_m, 1)
            << ",\"apex_distance_m\":" << format_number(segment.apex_distance_m, 1)
            << ",\"is_corner\":" << (segment.is_corner ? "true" : "false") << ",\"cause\":\""
            << to_string(segment.cause) << "\",\"corner\":\""
            << json_escape(segment.corner_label) << "\"},\"assumptions\":[\"Comparação entre voltas "
            << "cronometradas da mesma sessão, sob a mesma condição de pista reportada\"],"
            << "\"limitations\":[\"Telemetria OpenF1 amostrada a "
            << format_number(quality.median_sample_interval_s, 3)
            << " s e reamostrada em grade de " << format_number(grid_step_m, 1)
            << " m\",\"Cobertura efetiva de " << format_number(quality.coverage_pct, 1)
            << "% dos pontos da grade\"],\"engine\":\"analytics-cpp\",\"confidence\":"
            << format_number(segment.confidence, 3) << "}";
    }
    oss << ']';
    return oss.str();
}

std::string SpatialAlignmentEngine::build_comparison_json(
    const SessionMetadata& session, const LapMetadata& ref_lap, const LapMetadata& comp_lap,
    double grid_step_m, const std::vector<ComparisonChannelPoint>& channels,
    const std::vector<Corner>& corners, const std::vector<SpeedTrap>& speed_traps,
    const std::vector<Microsector>& microsectors, const QualityMetrics& quality,
    const std::string& insights_json) {
    std::ostringstream oss;

    const auto lap_header = [&](const LapMetadata& lap) {
        std::ostringstream header;
        header << "{\"driver_number\":" << lap.driver_number << ",\"driver_code\":\""
               << json_escape(lap.driver_code) << "\",\"team_name\":\"" << json_escape(lap.team_name)
               << "\",\"team_colour\":\"" << json_escape(lap.team_colour)
               << "\",\"lap_number\":" << lap.lap_number
               << ",\"lap_time_s\":" << format_number(lap.lap_time_s, 3) << ",\"compound\":\""
               << json_escape(lap.compound) << "\",\"stint_number\":" << lap.stint_number
               << ",\"stint_lap\":" << lap.stint_lap << ",\"tyre_age_laps\":" << lap.tyre_age_laps
               << ",\"coverage_pct\":" << format_number(lap.coverage_pct, 2)
               << ",\"samples_count\":" << lap.samples_count << ',';
        append_optional(header, "sector_1_s", lap.sector_1_s, 3);
        header << ',';
        append_optional(header, "sector_2_s", lap.sector_2_s, 3);
        header << ',';
        append_optional(header, "sector_3_s", lap.sector_3_s, 3);
        header << '}';
        return header.str();
    };

    oss << "{\"schema_version\":\"1.2.0\",\"session_key\":" << session.session_key
        << ",\"circuit_key\":" << session.circuit_key << ",\"circuit_name\":\""
        << json_escape(session.circuit_name) << "\",\"country_name\":\""
        << json_escape(session.country_name) << "\",\"session_name\":\""
        << json_escape(session.session_name) << "\",\"data_source\":\""
        << json_escape(session.data_source) << "\",\"grid_step_m\":" << format_number(grid_step_m, 2)
        << ",\"total_distance_m\":"
        << format_number(channels.empty() ? 0.0 : channels.back().distance_m, 1) << ',';
    append_optional(oss, "track_temperature_c", session.track_temperature_c, 1);
    oss << ',';
    append_optional(oss, "air_temperature_c", session.air_temperature_c, 1);
    oss << ",\"reference_lap\":" << lap_header(ref_lap) << ",\"comparison_lap\":"
        << lap_header(comp_lap) << ",\"corners\":[";

    for (size_t i = 0; i < corners.size(); ++i) {
        const auto& corner = corners[i];
        if (i > 0) oss << ',';
        oss << "{\"index\":" << corner.index << ",\"label\":\"" << json_escape(corner.label)
            << "\",\"entry_distance_m\":" << format_number(corner.entry_distance_m, 1)
            << ",\"apex_distance_m\":" << format_number(corner.apex_distance_m, 1)
            << ",\"exit_distance_m\":" << format_number(corner.exit_distance_m, 1)
            << ",\"apex_speed_kmh\":" << format_number(corner.apex_speed_kmh, 1) << '}';
    }

    oss << "],\"speed_traps\":[";
    for (size_t i = 0; i < speed_traps.size(); ++i) {
        const auto& trap = speed_traps[i];
        if (i > 0) oss << ',';
        oss << "{\"name\":\"" << json_escape(trap.name)
            << "\",\"distance_m\":" << format_number(trap.distance_m, 1)
            << ",\"ref_speed_kmh\":" << format_number(trap.ref_speed_kmh, 1)
            << ",\"comp_speed_kmh\":" << format_number(trap.comp_speed_kmh, 1)
            << ",\"delta_kmh\":" << format_number(trap.delta_kmh, 1) << ",\"origin\":\""
            << json_escape(trap.origin) << "\"}";
    }

    oss << "],\"microsectors\":[";
    for (size_t i = 0; i < microsectors.size(); ++i) {
        const auto& microsector = microsectors[i];
        if (i > 0) oss << ',';
        oss << "{\"index\":" << microsector.index
            << ",\"distance_start_m\":" << format_number(microsector.distance_start_m, 1)
            << ",\"distance_end_m\":" << format_number(microsector.distance_end_m, 1)
            << ",\"delta_s\":" << format_number(microsector.delta_s, 4)
            << ",\"ref_avg_speed_kmh\":" << format_number(microsector.ref_avg_speed_kmh, 1)
            << ",\"comp_avg_speed_kmh\":" << format_number(microsector.comp_avg_speed_kmh, 1)
            << ",\"winner\":\"" << microsector.winner << "\"}";
    }

    oss << "],\"channels\":[";
    for (size_t i = 0; i < channels.size(); ++i) {
        const auto& channel = channels[i];
        if (i > 0) oss << ',';
        oss << "{\"distance_m\":" << format_number(channel.distance_m, 1)
            << ",\"delta_time_s\":" << format_number(channel.delta_time_s, 4)
            << ",\"ref\":{\"time_s\":" << format_number(channel.ref.time_s, 4)
            << ",\"speed_kmh\":" << format_number(channel.ref.speed_kmh, 1)
            << ",\"throttle_pct\":" << format_number(channel.ref.throttle_pct, 1)
            << ",\"brake_pct\":" << format_number(channel.ref.brake_pct, 1)
            << ",\"rpm\":" << channel.ref.rpm << ",\"gear\":" << channel.ref.gear
            << ",\"drs\":" << (channel.ref.drs ? "true" : "false")
            << "},\"comp\":{\"time_s\":" << format_number(channel.comp.time_s, 4)
            << ",\"speed_kmh\":" << format_number(channel.comp.speed_kmh, 1)
            << ",\"throttle_pct\":" << format_number(channel.comp.throttle_pct, 1)
            << ",\"brake_pct\":" << format_number(channel.comp.brake_pct, 1)
            << ",\"rpm\":" << channel.comp.rpm << ",\"gear\":" << channel.comp.gear
            << ",\"drs\":" << (channel.comp.drs ? "true" : "false") << "}}";
    }

    oss << "],\"insights\":" << (insights_json.empty() ? "[]" : insights_json)
        << ",\"quality_audit\":{\"max_interpolation_gap_m\":"
        << format_number(quality.max_interpolation_gap_m, 2)
        << ",\"discontinuous_segments\":" << quality.discontinuous_segments
        << ",\"coverage_pct\":" << format_number(quality.coverage_pct, 2)
        << ",\"confidence_score\":" << format_number(quality.confidence_score, 3)
        << ",\"median_sample_interval_s\":" << format_number(quality.median_sample_interval_s, 4)
        << ",\"raw_samples_ref\":" << quality.raw_samples_ref
        << ",\"raw_samples_comp\":" << quality.raw_samples_comp
        << ",\"delta_closure_error_s\":" << format_number(quality.delta_closure_error_s, 4)
        << ",\"source_notes\":\""
        << json_escape(quality.source_notes) << "\"}}";

    return oss.str();
}

} // namespace apex::analytics

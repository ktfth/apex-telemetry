#include "../include/spatial_alignment.hpp"
#include "../../common/simdjson/simdjson.h"
#include <iostream>
#include <cassert>
#include <cmath>

void test_integrate_distance() {
    std::vector<apex::analytics::RawSample> raw = {
        {0.0, 72.0, 100.0, 0.0, 10000, 4, false},
        {1.0, 72.0, 100.0, 0.0, 10000, 4, false},
        {2.0, 72.0, 100.0, 0.0, 10000, 4, false},
        {3.0, 72.0, 100.0, 0.0, 10000, 4, false},
        {4.0, 72.0, 100.0, 0.0, 10000, 4, false},
        {5.0, 72.0, 100.0, 0.0, 10000, 4, false}
    };

    auto integrated = apex::analytics::SpatialAlignmentEngine::integrate_distance(raw);
    assert(integrated.size() == 6);
    assert(integrated.front().distance_m == 0.0);
    assert(std::abs(integrated.back().distance_m - 100.0) < 0.1);
    std::cout << "[PASS] test_integrate_distance: Distance integrated accurately from velocity in C++23." << std::endl;
}

void test_resample_to_grid() {
    std::vector<apex::analytics::DistanceSample> samples = {
        {{0.0, 36.0, 50.0, 0.0, 5000, 2, false}, 0.0},
        {{2.0, 36.0, 50.0, 0.0, 5000, 2, false}, 20.0},
        {{4.0, 36.0, 50.0, 0.0, 5000, 2, false}, 40.0}
    };

    auto grid = apex::analytics::SpatialAlignmentEngine::resample_to_grid(samples, 5.0, 25.0);
    assert(grid.size() == 9); // 0, 5, 10, 15, 20, 25, 30, 35, 40
    assert(grid[0].distance_m == 0.0);
    assert(grid[1].distance_m == 5.0);
    assert(std::abs(grid[1].time_s - 0.5) < 0.05);
    assert(grid.back().distance_m == 40.0);
    std::cout << "[PASS] test_resample_to_grid: 5-meter spatial grid resampling verified." << std::endl;
}

void test_delta_and_json() {
    std::vector<apex::analytics::AlignedGridPoint> ref_grid = {
        {0.0, 0.0, 200.0, 100.0, 0.0, 10000, 6, false, false},
        {5.0, 0.10, 200.0, 100.0, 0.0, 10000, 6, false, false}
    };
    std::vector<apex::analytics::AlignedGridPoint> comp_grid = {
        {0.0, 0.0, 180.0, 100.0, 0.0, 9500, 5, false, false},
        {5.0, 0.12, 180.0, 100.0, 0.0, 9500, 5, false, false}
    };

    auto channels = apex::analytics::SpatialAlignmentEngine::align_and_compute_delta(ref_grid, comp_grid);
    assert(channels.size() == 2);
    assert(std::abs(channels[1].delta_time_s - 0.02) < 0.001);

    auto segs = apex::analytics::SpatialAlignmentEngine::detect_segments(channels);
    apex::analytics::QualityMetrics quality{12.4, 0, 100.0, 0.98, "Test audit"};

    std::string json = apex::analytics::SpatialAlignmentEngine::build_comparison_json(
        9472, 1, 14, 16, 15, 5.0, 5.0, channels, segs, quality
    );

    // Verify valid JSON via simdjson
    simdjson::ondemand::parser parser;
    simdjson::padded_string padded(json);
    simdjson::ondemand::document doc = parser.iterate(padded);
    int64_t key = doc["session_key"].get_int64();
    assert(key == 9472);
    std::cout << "[PASS] test_delta_and_json: Delta computation and JSON serialization verified." << std::endl;
}

int main() {
    std::cout << "=== APEX ANALYTICS C++ TESTS ===" << std::endl;
    test_integrate_distance();
    test_resample_to_grid();
    test_delta_and_json();
    std::cout << "All Analytics C++ tests passed successfully!" << std::endl;
    return 0;
}

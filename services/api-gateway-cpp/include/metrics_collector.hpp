#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace apex::gateway {

/// Lock-free Prometheus-compatible metrics collector for the API Gateway.
/// All counters use relaxed atomics; histograms are protected by a shared mutex.
class MetricsCollector {
public:
    static MetricsCollector& instance() {
        static MetricsCollector collector;
        return collector;
    }

    /// Record a completed HTTP request with its route, status code, and duration.
    void record_request(const std::string& route, int status_code, double duration_seconds) {
        requests_total_.fetch_add(1, std::memory_order_relaxed);

        const std::string status_class = std::to_string(status_code / 100) + "xx";
        {
            std::lock_guard<std::mutex> lock(mutex_);
            route_requests_[route]++;
            status_requests_[status_class]++;

            // Record latency into histogram buckets
            for (size_t i = 0; i < histogram_bounds_.size(); ++i) {
                if (duration_seconds <= histogram_bounds_[i]) {
                    latency_buckets_[i]++;
                    break;
                }
            }
            // +Inf bucket
            latency_buckets_[histogram_bounds_.size()]++;

            latency_sum_ += duration_seconds;
            latency_count_++;
        }
    }

    /// Record a database query (success or failure).
    void record_db_query(bool success) {
        db_queries_total_.fetch_add(1, std::memory_order_relaxed);
        if (!success) db_errors_total_.fetch_add(1, std::memory_order_relaxed);
    }

    /// Record which data source served a request ("postgresql", "openf1-upstream", ...).
    void record_source(const std::string& source) {
        std::lock_guard<std::mutex> lock(mutex_);
        source_requests_[source]++;
    }

    /// Record a call to the OpenF1 upstream.
    void record_upstream(bool success) {
        upstream_calls_total_.fetch_add(1, std::memory_order_relaxed);
        if (!success) upstream_errors_total_.fetch_add(1, std::memory_order_relaxed);
    }

    /// Record a call to the Haskell strategy engine.
    void record_strategy(bool success) {
        strategy_calls_total_.fetch_add(1, std::memory_order_relaxed);
        if (!success) strategy_errors_total_.fetch_add(1, std::memory_order_relaxed);
    }

    /// Serialize all metrics in Prometheus text exposition format.
    std::string serialize() const {
        std::ostringstream out;

        // -- Total requests
        out << "# HELP apex_http_requests_total Total HTTP requests handled.\n"
            << "# TYPE apex_http_requests_total counter\n"
            << "apex_http_requests_total " << requests_total_.load(std::memory_order_relaxed) << "\n\n";

        // -- Requests by route
        {
            std::lock_guard<std::mutex> lock(mutex_);
            out << "# HELP apex_http_requests_by_route_total HTTP requests by route.\n"
                << "# TYPE apex_http_requests_by_route_total counter\n";
            for (const auto& [route, count] : route_requests_) {
                out << "apex_http_requests_by_route_total{route=\"" << route << "\"} " << count << "\n";
            }

            out << "\n# HELP apex_http_requests_by_status_total HTTP requests by status class.\n"
                << "# TYPE apex_http_requests_by_status_total counter\n";
            for (const auto& [status, count] : status_requests_) {
                out << "apex_http_requests_by_status_total{status=\"" << status << "\"} " << count << "\n";
            }

            // -- Latency histogram
            out << "\n# HELP apex_http_request_duration_seconds HTTP request latency.\n"
                << "# TYPE apex_http_request_duration_seconds histogram\n";
            uint64_t cumulative = 0;
            for (size_t i = 0; i < histogram_bounds_.size(); ++i) {
                cumulative += latency_buckets_[i];
                out << "apex_http_request_duration_seconds_bucket{le=\"" << histogram_bounds_[i] << "\"} " << cumulative << "\n";
            }
            cumulative += latency_buckets_[histogram_bounds_.size()];
            out << "apex_http_request_duration_seconds_bucket{le=\"+Inf\"} " << cumulative << "\n"
                << "apex_http_request_duration_seconds_sum " << latency_sum_ << "\n"
                << "apex_http_request_duration_seconds_count " << latency_count_ << "\n";

            out << "\n# HELP apex_requests_by_source_total Requests by resolved data source.\n"
                << "# TYPE apex_requests_by_source_total counter\n";
            for (const auto& [source, count] : source_requests_) {
                out << "apex_requests_by_source_total{source=\"" << source << "\"} " << count << "\n";
            }
        }

        // -- Database metrics
        out << "\n# HELP apex_db_queries_total Total database queries.\n"
            << "# TYPE apex_db_queries_total counter\n"
            << "apex_db_queries_total " << db_queries_total_.load(std::memory_order_relaxed) << "\n";

        out << "\n# HELP apex_db_errors_total Total database query errors.\n"
            << "# TYPE apex_db_errors_total counter\n"
            << "apex_db_errors_total " << db_errors_total_.load(std::memory_order_relaxed) << "\n";

        out << "\n# HELP apex_upstream_calls_total Calls issued to the OpenF1 upstream.\n"
            << "# TYPE apex_upstream_calls_total counter\n"
            << "apex_upstream_calls_total " << upstream_calls_total_.load(std::memory_order_relaxed) << "\n";

        out << "\n# HELP apex_upstream_errors_total Failed OpenF1 upstream calls.\n"
            << "# TYPE apex_upstream_errors_total counter\n"
            << "apex_upstream_errors_total " << upstream_errors_total_.load(std::memory_order_relaxed) << "\n";

        out << "\n# HELP apex_strategy_calls_total Calls issued to the Haskell strategy engine.\n"
            << "# TYPE apex_strategy_calls_total counter\n"
            << "apex_strategy_calls_total " << strategy_calls_total_.load(std::memory_order_relaxed) << "\n";

        out << "\n# HELP apex_strategy_errors_total Failed strategy engine calls.\n"
            << "# TYPE apex_strategy_errors_total counter\n"
            << "apex_strategy_errors_total " << strategy_errors_total_.load(std::memory_order_relaxed) << "\n";

        return out.str();
    }

    /// RAII timer for measuring request latency.
    class ScopedTimer {
    public:
        explicit ScopedTimer(MetricsCollector& collector, std::string route)
            : collector_(collector), route_(std::move(route))
            , start_(std::chrono::steady_clock::now()) {}

        void set_status(int code) { status_code_ = code; }

        ~ScopedTimer() {
            const auto elapsed = std::chrono::steady_clock::now() - start_;
            const double seconds = std::chrono::duration<double>(elapsed).count();
            collector_.record_request(route_, status_code_, seconds);
        }
    private:
        MetricsCollector& collector_;
        std::string route_;
        int status_code_ = 200;
        std::chrono::steady_clock::time_point start_;
    };

private:
    MetricsCollector() : latency_buckets_(histogram_bounds_.size() + 1, 0) {}

    std::atomic<uint64_t> requests_total_{0};
    std::atomic<uint64_t> db_queries_total_{0};
    std::atomic<uint64_t> db_errors_total_{0};
    std::atomic<uint64_t> upstream_calls_total_{0};
    std::atomic<uint64_t> upstream_errors_total_{0};
    std::atomic<uint64_t> strategy_calls_total_{0};
    std::atomic<uint64_t> strategy_errors_total_{0};

    mutable std::mutex mutex_;
    std::map<std::string, uint64_t> route_requests_;
    std::map<std::string, uint64_t> status_requests_;
    std::map<std::string, uint64_t> source_requests_;

    // Histogram buckets: 1ms, 5ms, 10ms, 25ms, 50ms, 100ms, 250ms, 500ms, 1s, 5s
    const std::vector<double> histogram_bounds_ = {0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 5.0};
    std::vector<uint64_t> latency_buckets_;
    double latency_sum_ = 0.0;
    uint64_t latency_count_ = 0;
};

} // namespace apex::gateway

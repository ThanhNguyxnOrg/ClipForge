#include "clipforge/dsp/kernels.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

struct BenchRow {
    std::string kernel;
    std::string backend;
    std::size_t sample_count = 0;
    std::size_t iterations = 0;
    double median_us = 0.0;
    double samples_per_sec = 0.0;
};

volatile float g_float_sink = 0.0f;
volatile std::uint64_t g_u64_sink = 0;

double compute_median(std::vector<double>& v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    if (n % 2 == 1) {
        return v[n / 2];
    }
    return 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

} // namespace

int main(int argc, char* argv[]) {
    using namespace clipforge::dsp;
    using Clock = std::chrono::steady_clock;

    std::string csv_out_path = "benchmark_results.csv";
    if (argc > 2 && std::string(argv[1]) == "--csv") {
        csv_out_path = argv[2];
    }

    const std::vector<std::size_t> sizes = {
        256,
        4096,
        65536,
        1048576,
        16777216
    };

    constexpr std::size_t kWarmupIters = 5;
    constexpr std::size_t kMeasureIters = 25;

    std::cout << "ClipForge DSP Benchmark\n"
              << "ASM Backend Compiled: " << (is_asm_backend_available() ? "YES (NASM x64 SSE2)" : "NO (C++ only)") << "\n\n";

    std::vector<BenchRow> results;
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.05f, 1.05f);

    std::vector<Backend> backends = {Backend::Cpp};
    if (is_asm_backend_available()) {
        backends.push_back(Backend::Asm);
    }

    for (std::size_t n : sizes) {
        std::vector<float> source(n);
        for (std::size_t i = 0; i < n; ++i) {
            source[i] = dist(rng);
        }

        if (is_asm_backend_available()) {
            const float p_cpp = peak_abs_f32(source.data(), n, Backend::Cpp);
            const float p_asm = peak_abs_f32(source.data(), n, Backend::Asm);
            const auto c_cpp = count_clipped_f32(source.data(), n, 0.999f, Backend::Cpp);
            const auto c_asm = count_clipped_f32(source.data(), n, 0.999f, Backend::Asm);
            if (p_cpp != p_asm || c_cpp != c_asm) {
                std::cerr << "FATAL: Output mismatch between C++ and ASM on size " << n << "\n";
                return 2;
            }
        }

        for (Backend b : backends) {
            const std::string b_name(to_string(b));

            {
                for (std::size_t w = 0; w < kWarmupIters; ++w) {
                    g_float_sink = peak_abs_f32(source.data(), n, b);
                }
                std::vector<double> times_us;
                times_us.reserve(kMeasureIters);
                for (std::size_t it = 0; it < kMeasureIters; ++it) {
                    const auto t0 = Clock::now();
                    g_float_sink = peak_abs_f32(source.data(), n, b);
                    const auto t1 = Clock::now();
                    times_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
                }
                const double med_us = compute_median(times_us);
                results.push_back({
                    "cf_peak_abs_f32", b_name, n, kMeasureIters, med_us,
                    (med_us > 0.0) ? (static_cast<double>(n) / (med_us * 1e-6)) : 0.0
                });
            }

            {
                for (std::size_t w = 0; w < kWarmupIters; ++w) {
                    g_u64_sink = count_clipped_f32(source.data(), n, 0.999f, b);
                }
                std::vector<double> times_us;
                times_us.reserve(kMeasureIters);
                for (std::size_t it = 0; it < kMeasureIters; ++it) {
                    const auto t0 = Clock::now();
                    g_u64_sink = count_clipped_f32(source.data(), n, 0.999f, b);
                    const auto t1 = Clock::now();
                    times_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
                }
                const double med_us = compute_median(times_us);
                results.push_back({
                    "cf_count_clipped_f32", b_name, n, kMeasureIters, med_us,
                    (med_us > 0.0) ? (static_cast<double>(n) / (med_us * 1e-6)) : 0.0
                });
            }

            {
                std::vector<float> work = source;
                for (std::size_t w = 0; w < kWarmupIters; ++w) {
                    apply_gain_clamp_f32(work.data(), n, 1.0001f, 0.99f, b);
                    g_float_sink = work[n - 1];
                }
                std::vector<double> times_us;
                times_us.reserve(kMeasureIters);
                for (std::size_t it = 0; it < kMeasureIters; ++it) {
                    std::memcpy(work.data(), source.data(), n * sizeof(float));
                    const auto t0 = Clock::now();
                    apply_gain_clamp_f32(work.data(), n, 1.15f, 0.89125f, b);
                    const auto t1 = Clock::now();
                    g_float_sink = work[n - 1];
                    times_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
                }
                const double med_us = compute_median(times_us);
                results.push_back({
                    "cf_apply_gain_clamp_f32", b_name, n, kMeasureIters, med_us,
                    (med_us > 0.0) ? (static_cast<double>(n) / (med_us * 1e-6)) : 0.0
                });
            }
        }
    }

    std::ofstream csv(csv_out_path, std::ios::binary | std::ios::trunc);
    csv << "kernel,backend,sample_count,iterations,median_us,throughput_samples_per_sec\n";
    csv << std::fixed << std::setprecision(3);

    std::cout << std::left << std::setw(26) << "Kernel"
              << std::setw(8)  << "Backend"
              << std::right << std::setw(12) << "Samples"
              << std::setw(14) << "Median (us)"
              << std::setw(18) << "MSamples/sec\n"
              << std::string(78, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left << std::setw(26) << r.kernel
                  << std::setw(8)  << r.backend
                  << std::right << std::setw(12) << r.sample_count
                  << std::fixed << std::setprecision(2) << std::setw(14) << r.median_us
                  << std::setw(18) << (r.samples_per_sec / 1e6) << "\n";

        csv << r.kernel << ',' << r.backend << ',' << r.sample_count << ','
            << r.iterations << ',' << r.median_us << ',' << r.samples_per_sec << '\n';
    }

    std::cout << "\nWrote machine-readable CSV benchmark report to: " << csv_out_path << "\n";
    return 0;
}

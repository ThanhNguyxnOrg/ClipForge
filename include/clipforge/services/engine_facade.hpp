#pragma once

#include "clipforge/audio/wav_writer.hpp"
#include "clipforge/core/result.hpp"
#include "clipforge/dsp/kernels.hpp"
#include "clipforge/processing/analyzer.hpp"
#include "clipforge/processing/normalizer.hpp"
#include "clipforge/processing/silence_trimmer.hpp"

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace clipforge::services {

using ProgressCallback = std::function<void(
    std::size_t completed_files,
    std::size_t total_files,
    std::string_view current_file
)>;

enum class BatchOperation {
    Normalize,
    Trim,
    NormalizeAndTrim
};

[[nodiscard]] constexpr std::string_view to_string(BatchOperation op) noexcept {
    switch (op) {
        case BatchOperation::Normalize:        return "normalize";
        case BatchOperation::Trim:             return "trim";
        case BatchOperation::NormalizeAndTrim: return "normalize+trim";
    }
    return "unknown";
}

struct BatchRequest {
    std::filesystem::path input_dir;
    std::filesystem::path output_dir;
    BatchOperation operation = BatchOperation::Normalize;
    bool recursive = false;
    bool dry_run = false;
    bool overwrite_existing = false;
    dsp::Backend backend = dsp::Backend::Auto;

    processing::NormalizeOptions normalize_options{};
    processing::SilenceTrimOptions trim_options{};
    std::optional<std::filesystem::path> csv_report_path = std::nullopt;
};

struct BatchItemRecord {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    bool succeeded = false;
    bool dry_run = false;
    double input_duration_sec = 0.0;
    double output_duration_sec = 0.0;
    float input_peak_linear = 0.0f;
    float output_peak_linear = 0.0f;
    std::string status_message;
};

struct BatchSummary {
    std::size_t total_discovered = 0;
    std::size_t succeeded_count = 0;
    std::size_t failed_count = 0;
    std::size_t skipped_dry_run_count = 0;
    std::vector<BatchItemRecord> records;
};

// Public Qt-ready engine facade. Stateless and thread-safe per call.
class EngineFacade {
public:
    [[nodiscard]] static Result<processing::AudioAnalysisReport> inspect_file(
        const std::filesystem::path& input_path,
        const processing::AnalysisOptions& options = {}
    );

    [[nodiscard]] static Result<processing::NormalizeResult> normalize_file(
        const std::filesystem::path& input_path,
        const std::filesystem::path& output_path,
        const processing::NormalizeOptions& options = {},
        bool overwrite_existing = false
    );

    [[nodiscard]] static Result<processing::SilenceTrimResult> trim_file(
        const std::filesystem::path& input_path,
        const std::filesystem::path& output_path,
        const processing::SilenceTrimOptions& options = {},
        bool overwrite_existing = false
    );

    [[nodiscard]] static Result<BatchSummary> run_batch(
        const BatchRequest& request,
        const ProgressCallback& on_progress = nullptr,
        const std::atomic<bool>* cancel_token = nullptr
    );

    [[nodiscard]] static Status write_batch_csv(
        const std::filesystem::path& csv_path,
        const BatchSummary& summary
    );
};

} // namespace clipforge::services

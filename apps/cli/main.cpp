#include "clipforge/services/engine_facade.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

enum ExitCode : int {
    kExitOk = 0,
    kExitUsageError = 2,
    kExitProcessingError = 3,
    kExitBatchPartialFailure = 4
};

void print_help() {
    std::cout <<
R"(ClipForge CLI — Offline Audio Inspection & Batch Processing Toolkit

Usage:
  clipforge-cli analyze <input.wav> [--backend auto|cpp|asm]

  clipforge-cli normalize <input.wav> \
      --target-dbfs <float> \
      --output <output.wav> \
      [--backend auto|cpp|asm]

  clipforge-cli trim <input.wav> \
      --threshold-dbfs <float> \
      [--tail-ms <float>] \
      [--head-ms <float>] \
      [--min-audio-ms <float>] \
      --output <trimmed.wav>

  clipforge-cli batch <input_dir> \
      --operation normalize|trim|both \
      --output-dir <output_dir> \
      [--target-dbfs <float>] \
      [--threshold-dbfs <float>] \
      [--tail-ms <float>] \
      [--recursive] \
      [--report <summary.csv>] \
      [--backend auto|cpp|asm] \
      [--dry-run]

Options:
  --help, -h         Show this help message and exit.
)";
}

[[nodiscard]] std::optional<double> parse_double(std::string_view sv) {
    try {
        std::size_t idx = 0;
        const std::string s(sv);
        const double val = std::stod(s, &idx);
        if (idx != s.size() || !std::isfinite(val)) {
            return std::nullopt;
        }
        return val;
    } catch (...) {
        return std::nullopt;
    }
}

[[nodiscard]] std::optional<clipforge::dsp::Backend> parse_backend(std::string_view sv) {
    if (sv == "auto") return clipforge::dsp::Backend::Auto;
    if (sv == "cpp")  return clipforge::dsp::Backend::Cpp;
    if (sv == "asm")  return clipforge::dsp::Backend::Asm;
    return std::nullopt;
}

std::string format_dbfs(double dbfs) {
    if (!std::isfinite(dbfs)) {
        return "-inf dBFS";
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << dbfs << " dBFS";
    return oss.str();
}

int cmd_analyze(const std::vector<std::string_view>& args) {
    if (args.empty()) {
        std::cerr << "Error: 'analyze' requires an <input.wav> path.\n";
        return kExitUsageError;
    }

    std::filesystem::path input_path(args[0]);
    clipforge::processing::AnalysisOptions opts{};

    for (std::size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--backend" && i + 1 < args.size()) {
            auto b = parse_backend(args[++i]);
            if (!b) {
                std::cerr << "Error: Invalid backend '" << args[i] << "'. Expected auto|cpp|asm.\n";
                return kExitUsageError;
            }
            opts.backend = *b;
        } else {
            std::cerr << "Error: Unknown or incomplete argument '" << args[i] << "'.\n";
            return kExitUsageError;
        }
    }

    auto res = clipforge::services::EngineFacade::inspect_file(input_path, opts);
    if (!res.has_value()) {
        std::cerr << "Error [" << clipforge::to_string(res.error().code) << "]: "
                  << res.error().message << "\n";
        return kExitProcessingError;
    }

    const auto& r = *res;
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "File:               " << input_path.string() << "\n"
              << "Format:             " << clipforge::audio::to_string(r.sample_format) << "\n"
              << "Bit Depth:          " << r.bits_per_sample << "-bit\n"
              << "Channels:           " << r.num_channels << "\n"
              << "Sample Rate:        " << r.sample_rate << " Hz\n"
              << "Frames:             " << r.total_frames << "\n"
              << "Duration:           " << r.duration_seconds << " s\n"
              << "Peak Linear:        " << r.peak_linear << "\n"
              << "Peak Level:         " << format_dbfs(r.peak_dbfs) << "\n"
              << "RMS Level:          " << format_dbfs(r.rms_dbfs) << "\n"
              << "Clipped Samples:    " << r.clipped_sample_count << "\n"
              << "Leading Silence:    " << r.leading_silence_seconds << " s\n"
              << "Trailing Silence:   " << r.trailing_silence_seconds << " s\n"
              << "DSP Backend Used:   " << clipforge::dsp::to_string(r.active_backend) << "\n";
    return kExitOk;
}

int cmd_normalize(const std::vector<std::string_view>& args) {
    if (args.empty()) {
        std::cerr << "Error: 'normalize' requires an <input.wav> path.\n";
        return kExitUsageError;
    }

    std::filesystem::path input_path(args[0]);
    std::filesystem::path output_path;
    clipforge::processing::NormalizeOptions opts{};

    for (std::size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--target-dbfs" && i + 1 < args.size()) {
            auto v = parse_double(args[++i]);
            if (!v) {
                std::cerr << "Error: Invalid numeric value for --target-dbfs.\n";
                return kExitUsageError;
            }
            opts.target_dbfs = static_cast<float>(*v);
        } else if (args[i] == "--output" && i + 1 < args.size()) {
            output_path = std::filesystem::path(args[++i]);
        } else if (args[i] == "--backend" && i + 1 < args.size()) {
            auto b = parse_backend(args[++i]);
            if (!b) {
                std::cerr << "Error: Invalid backend '" << args[i] << "'.\n";
                return kExitUsageError;
            }
            opts.backend = *b;
        } else {
            std::cerr << "Error: Unknown or incomplete argument '" << args[i] << "'.\n";
            return kExitUsageError;
        }
    }

    if (output_path.empty()) {
        std::cerr << "Error: --output <output.wav> is required for normalize.\n";
        return kExitUsageError;
    }

    auto res = clipforge::services::EngineFacade::normalize_file(input_path, output_path, opts, false);
    if (!res.has_value()) {
        std::cerr << "Error [" << clipforge::to_string(res.error().code) << "]: "
                  << res.error().message << "\n";
        return kExitProcessingError;
    }

    std::cout << std::fixed << std::setprecision(4)
              << "Normalized '" << input_path.string() << "' -> '" << output_path.string() << "'\n"
              << "  Input Peak:     " << res->input_peak_linear << "\n"
              << "  Output Peak:    " << res->output_peak_linear << "\n"
              << "  Applied Gain:   " << res->applied_gain_db << " dB\n"
              << "  Backend Used:   " << clipforge::dsp::to_string(res->active_backend) << "\n";
    return kExitOk;
}

int cmd_trim(const std::vector<std::string_view>& args) {
    if (args.empty()) {
        std::cerr << "Error: 'trim' requires an <input.wav> path.\n";
        return kExitUsageError;
    }

    std::filesystem::path input_path(args[0]);
    std::filesystem::path output_path;
    clipforge::processing::SilenceTrimOptions opts{};

    for (std::size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--threshold-dbfs" && i + 1 < args.size()) {
            auto v = parse_double(args[++i]);
            if (!v) {
                std::cerr << "Error: Invalid numeric value for --threshold-dbfs.\n";
                return kExitUsageError;
            }
            opts.threshold_dbfs = static_cast<float>(*v);
        } else if (args[i] == "--tail-ms" && i + 1 < args.size()) {
            auto v = parse_double(args[++i]);
            if (!v) {
                std::cerr << "Error: Invalid numeric value for --tail-ms.\n";
                return kExitUsageError;
            }
            opts.tail_pad_ms = *v;
        } else if (args[i] == "--head-ms" && i + 1 < args.size()) {
            auto v = parse_double(args[++i]);
            if (!v) {
                std::cerr << "Error: Invalid numeric value for --head-ms.\n";
                return kExitUsageError;
            }
            opts.head_pad_ms = *v;
        } else if (args[i] == "--min-audio-ms" && i + 1 < args.size()) {
            auto v = parse_double(args[++i]);
            if (!v) {
                std::cerr << "Error: Invalid numeric value for --min-audio-ms.\n";
                return kExitUsageError;
            }
            opts.min_non_silent_ms = *v;
        } else if (args[i] == "--output" && i + 1 < args.size()) {
            output_path = std::filesystem::path(args[++i]);
        } else {
            std::cerr << "Error: Unknown or incomplete argument '" << args[i] << "'.\n";
            return kExitUsageError;
        }
    }

    if (output_path.empty()) {
        std::cerr << "Error: --output <trimmed.wav> is required for trim.\n";
        return kExitUsageError;
    }

    auto res = clipforge::services::EngineFacade::trim_file(input_path, output_path, opts, false);
    if (!res.has_value()) {
        std::cerr << "Error [" << clipforge::to_string(res.error().code) << "]: "
                  << res.error().message << "\n";
        return kExitProcessingError;
    }

    std::cout << "Trimmed '" << input_path.string() << "' -> '" << output_path.string() << "'\n"
              << "  Original Frames: " << res->original_frames << "\n"
              << "  Trimmed Frames:  " << res->trimmed_frames << "\n"
              << "  Removed Lead:    " << res->removed_leading_frames << " frames\n"
              << "  Removed Trail:   " << res->removed_trailing_frames << " frames\n";
    return kExitOk;
}

int cmd_batch(const std::vector<std::string_view>& args) {
    if (args.empty()) {
        std::cerr << "Error: 'batch' requires an <input_dir> path.\n";
        return kExitUsageError;
    }

    clipforge::services::BatchRequest req{};
    req.input_dir = std::filesystem::path(args[0]);

    for (std::size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--operation" && i + 1 < args.size()) {
            const auto op = args[++i];
            if (op == "normalize") {
                req.operation = clipforge::services::BatchOperation::Normalize;
            } else if (op == "trim") {
                req.operation = clipforge::services::BatchOperation::Trim;
            } else if (op == "both" || op == "normalize+trim") {
                req.operation = clipforge::services::BatchOperation::NormalizeAndTrim;
            } else {
                std::cerr << "Error: Invalid --operation '" << op << "'.\n";
                return kExitUsageError;
            }
        } else if (args[i] == "--output-dir" && i + 1 < args.size()) {
            req.output_dir = std::filesystem::path(args[++i]);
        } else if (args[i] == "--target-dbfs" && i + 1 < args.size()) {
            auto v = parse_double(args[++i]);
            if (!v) return kExitUsageError;
            req.normalize_options.target_dbfs = static_cast<float>(*v);
        } else if (args[i] == "--threshold-dbfs" && i + 1 < args.size()) {
            auto v = parse_double(args[++i]);
            if (!v) return kExitUsageError;
            req.trim_options.threshold_dbfs = static_cast<float>(*v);
        } else if (args[i] == "--tail-ms" && i + 1 < args.size()) {
            auto v = parse_double(args[++i]);
            if (!v) return kExitUsageError;
            req.trim_options.tail_pad_ms = *v;
        } else if (args[i] == "--report" && i + 1 < args.size()) {
            req.csv_report_path = std::filesystem::path(args[++i]);
        } else if (args[i] == "--backend" && i + 1 < args.size()) {
            auto b = parse_backend(args[++i]);
            if (!b) return kExitUsageError;
            req.backend = *b;
        } else if (args[i] == "--recursive") {
            req.recursive = true;
        } else if (args[i] == "--dry-run") {
            req.dry_run = true;
        } else {
            std::cerr << "Error: Unknown or incomplete argument '" << args[i] << "'.\n";
            return kExitUsageError;
        }
    }

    if (req.output_dir.empty()) {
        std::cerr << "Error: --output-dir <output_dir> is required for batch.\n";
        return kExitUsageError;
    }

    auto res = clipforge::services::EngineFacade::run_batch(req);
    if (!res.has_value()) {
        std::cerr << "Error [" << clipforge::to_string(res.error().code) << "]: "
                  << res.error().message << "\n";
        return kExitProcessingError;
    }

    std::cout << "Batch Summary:\n"
              << "  Discovered WAVs: " << res->total_discovered << "\n"
              << "  Succeeded:       " << res->succeeded_count << "\n"
              << "  Failed:          " << res->failed_count << "\n"
              << "  Dry-Run Mode:    " << (req.dry_run ? "yes" : "no") << "\n";

    for (const auto& rec : res->records) {
        std::cout << "  [" << (rec.succeeded ? "OK" : "FAIL") << "] "
                  << rec.input_path.filename().string() << " -> " << rec.status_message << "\n";
    }

    return (res->failed_count == 0) ? kExitOk : kExitBatchPartialFailure;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return kExitUsageError;
    }

    const std::string_view subcommand(argv[1]);
    if (subcommand == "--help" || subcommand == "-h" || subcommand == "help") {
        print_help();
        return kExitOk;
    }

    std::vector<std::string_view> sub_args;
    for (int i = 2; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--help" || std::string_view(argv[i]) == "-h") {
            print_help();
            return kExitOk;
        }
        sub_args.emplace_back(argv[i]);
    }

    if (subcommand == "analyze")   return cmd_analyze(sub_args);
    if (subcommand == "normalize") return cmd_normalize(sub_args);
    if (subcommand == "trim")      return cmd_trim(sub_args);
    if (subcommand == "batch")     return cmd_batch(sub_args);

    std::cerr << "Error: Unknown command '" << subcommand << "'. Run with --help for usage.\n";
    return kExitUsageError;
}

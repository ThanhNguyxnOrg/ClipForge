#include "clipforge/services/engine_facade.hpp"
#include "clipforge/audio/wav_reader.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace clipforge::services {

namespace {

[[nodiscard]] bool has_wav_extension(const std::filesystem::path& p) {
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext == ".wav";
}

[[nodiscard]] std::string escape_csv(std::string_view field) {
    bool needs_quotes = false;
    for (char c : field) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes) {
        return std::string(field);
    }
    std::string out = "\"";
    for (char c : field) {
        if (c == '"') {
            out += "\"\"";
        } else {
            out += c;
        }
    }
    out += '"';
    return out;
}

} // namespace

Result<processing::AudioAnalysisReport> EngineFacade::inspect_file(
    const std::filesystem::path& input_path,
    const processing::AnalysisOptions& options
) {
    auto buf = audio::WavReader::read_file(input_path);
    if (!buf.has_value()) {
        return std::unexpected(buf.error());
    }
    return processing::Analyzer::analyze(*buf, options);
}

Result<processing::NormalizeResult> EngineFacade::normalize_file(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path,
    const processing::NormalizeOptions& options,
    bool overwrite_existing
) {
    auto buf = audio::WavReader::read_file(input_path);
    if (!buf.has_value()) {
        return std::unexpected(buf.error());
    }

    auto norm_res = processing::Normalizer::normalize_in_place(*buf, options);
    if (!norm_res.has_value()) {
        return std::unexpected(norm_res.error());
    }

    audio::WavWriteOptions write_opts{};
    write_opts.overwrite_existing = overwrite_existing;
    write_opts.source_path_guard = input_path;

    auto write_status = audio::WavWriter::write_file(output_path, *buf, write_opts);
    if (!write_status.has_value()) {
        return std::unexpected(write_status.error());
    }

    return norm_res;
}

Result<processing::SilenceTrimResult> EngineFacade::trim_file(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path,
    const processing::SilenceTrimOptions& options,
    bool overwrite_existing
) {
    auto buf = audio::WavReader::read_file(input_path);
    if (!buf.has_value()) {
        return std::unexpected(buf.error());
    }

    auto trim_res = processing::SilenceTrimmer::trim(*buf, options);
    if (!trim_res.has_value()) {
        return std::unexpected(trim_res.error());
    }

    audio::WavWriteOptions write_opts{};
    write_opts.overwrite_existing = overwrite_existing;
    write_opts.source_path_guard = input_path;

    auto write_status = audio::WavWriter::write_file(output_path, trim_res->buffer, write_opts);
    if (!write_status.has_value()) {
        return std::unexpected(write_status.error());
    }

    return trim_res;
}

Result<BatchSummary> EngineFacade::run_batch(
    const BatchRequest& request,
    const ProgressCallback& on_progress,
    const std::atomic<bool>* cancel_token
) {
    std::error_code ec;
    if (!std::filesystem::exists(request.input_dir, ec) ||
        !std::filesystem::is_directory(request.input_dir, ec)) {
        return make_error(
            ErrorCode::IoError,
            "Batch input directory does not exist: " + request.input_dir.string()
        );
    }

    if (request.output_dir.empty()) {
        return make_error(ErrorCode::InvalidParameter, "Batch output directory cannot be empty.");
    }

    const auto canon_in = std::filesystem::weakly_canonical(request.input_dir, ec);
    const auto canon_out = std::filesystem::weakly_canonical(request.output_dir, ec);
    if (!ec && canon_in == canon_out) {
        return make_error(
            ErrorCode::PathCollision,
            "Batch output directory must be distinct from the input directory."
        );
    }

    std::vector<std::filesystem::path> wav_files;
    if (request.recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(request.input_dir, ec)) {
            if (ec) {
                return make_error(ErrorCode::IoError, "Directory traversal error: " + ec.message());
            }
            if (entry.is_regular_file() && has_wav_extension(entry.path())) {
                wav_files.push_back(entry.path());
            }
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(request.input_dir, ec)) {
            if (ec) {
                return make_error(ErrorCode::IoError, "Directory traversal error: " + ec.message());
            }
            if (entry.is_regular_file() && has_wav_extension(entry.path())) {
                wav_files.push_back(entry.path());
            }
        }
    }

    std::sort(wav_files.begin(), wav_files.end());

    BatchSummary summary{};
    summary.total_discovered = wav_files.size();
    std::set<std::filesystem::path> planned_outputs;

    for (std::size_t idx = 0; idx < wav_files.size(); ++idx) {
        if (cancel_token != nullptr && cancel_token->load(std::memory_order_relaxed)) {
            return make_error(ErrorCode::Cancelled, "Batch operation cancelled by caller.");
        }

        const auto& in_file = wav_files[idx];
        if (on_progress) {
            on_progress(idx, wav_files.size(), in_file.string());
        }

        std::filesystem::path rel_path =
            request.recursive ? std::filesystem::relative(in_file, request.input_dir, ec)
                              : in_file.filename();
        if (ec || rel_path.empty()) {
            rel_path = in_file.filename();
        }

        const std::filesystem::path out_file = request.output_dir / rel_path;
        const auto norm_out = std::filesystem::weakly_canonical(out_file, ec);
        const auto norm_in = std::filesystem::weakly_canonical(in_file, ec);

        BatchItemRecord record{};
        record.input_path = in_file;
        record.output_path = out_file;
        record.dry_run = request.dry_run;

        if (norm_in == norm_out) {
            record.succeeded = false;
            record.status_message = "PathCollision: Output resolves to input file";
            ++summary.failed_count;
            summary.records.push_back(std::move(record));
            continue;
        }

        if (!planned_outputs.insert(norm_out).second) {
            record.succeeded = false;
            record.status_message = "PathCollision: Duplicate target path in batch";
            ++summary.failed_count;
            summary.records.push_back(std::move(record));
            continue;
        }

        if (!request.overwrite_existing && std::filesystem::exists(out_file, ec)) {
            record.succeeded = false;
            record.status_message = "TargetExists: Output file already exists";
            ++summary.failed_count;
            summary.records.push_back(std::move(record));
            continue;
        }

        auto buf_res = audio::WavReader::read_file(in_file);
        if (!buf_res.has_value()) {
            record.succeeded = false;
            record.status_message = std::string(to_string(buf_res.error().code)) + ": " + buf_res.error().message;
            ++summary.failed_count;
            summary.records.push_back(std::move(record));
            continue;
        }

        audio::AudioBuffer working_buf = std::move(*buf_res);
        record.input_duration_sec = working_buf.duration_seconds();
        record.input_peak_linear = dsp::peak_abs_f32(
            working_buf.data(),
            static_cast<std::uint64_t>(working_buf.sample_count()),
            request.backend
        );

        if (request.operation == BatchOperation::Trim ||
            request.operation == BatchOperation::NormalizeAndTrim) {
            auto trim_res = processing::SilenceTrimmer::trim(working_buf, request.trim_options);
            if (!trim_res.has_value()) {
                record.succeeded = false;
                record.status_message = std::string(to_string(trim_res.error().code)) + ": " + trim_res.error().message;
                ++summary.failed_count;
                summary.records.push_back(std::move(record));
                continue;
            }
            working_buf = std::move(trim_res->buffer);
        }

        if (request.operation == BatchOperation::Normalize ||
            request.operation == BatchOperation::NormalizeAndTrim) {
            processing::NormalizeOptions norm_opts = request.normalize_options;
            norm_opts.backend = request.backend;
            auto norm_res = processing::Normalizer::normalize_in_place(working_buf, norm_opts);
            if (!norm_res.has_value()) {
                record.succeeded = false;
                record.status_message = std::string(to_string(norm_res.error().code)) + ": " + norm_res.error().message;
                ++summary.failed_count;
                summary.records.push_back(std::move(record));
                continue;
            }
        }

        record.output_duration_sec = working_buf.duration_seconds();
        record.output_peak_linear = dsp::peak_abs_f32(
            working_buf.data(),
            static_cast<std::uint64_t>(working_buf.sample_count()),
            request.backend
        );

        if (request.dry_run) {
            record.succeeded = true;
            record.status_message = "OK (Dry-Run)";
            ++summary.succeeded_count;
            ++summary.skipped_dry_run_count;
            summary.records.push_back(std::move(record));
            continue;
        }

        audio::WavWriteOptions write_opts{};
        write_opts.overwrite_existing = request.overwrite_existing;
        write_opts.source_path_guard = in_file;

        auto write_status = audio::WavWriter::write_file(out_file, working_buf, write_opts);
        if (!write_status.has_value()) {
            record.succeeded = false;
            record.status_message =
                std::string(to_string(write_status.error().code)) + ": " + write_status.error().message;
            ++summary.failed_count;
            summary.records.push_back(std::move(record));
            continue;
        }

        record.succeeded = true;
        record.status_message = "OK";
        ++summary.succeeded_count;
        summary.records.push_back(std::move(record));
    }

    if (on_progress) {
        on_progress(wav_files.size(), wav_files.size(), "");
    }

    if (request.csv_report_path.has_value()) {
        auto csv_status = write_batch_csv(*request.csv_report_path, summary);
        if (!csv_status.has_value()) {
            return std::unexpected(csv_status.error());
        }
    }

    return summary;
}

Status EngineFacade::write_batch_csv(
    const std::filesystem::path& csv_path,
    const BatchSummary& summary
) {
    std::error_code ec;
    const auto parent = csv_path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent, ec)) {
        std::filesystem::create_directories(parent, ec);
    }

    std::ofstream out(csv_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return make_error(ErrorCode::IoError, "Failed to open CSV report path: " + csv_path.string());
    }

    out << "input_path,output_path,succeeded,dry_run,input_duration_sec,output_duration_sec,"
           "input_peak_linear,output_peak_linear,status_message\n";
    out << std::fixed << std::setprecision(6);

    for (const auto& rec : summary.records) {
        out << escape_csv(rec.input_path.string()) << ','
            << escape_csv(rec.output_path.string()) << ','
            << (rec.succeeded ? "true" : "false") << ','
            << (rec.dry_run ? "true" : "false") << ','
            << rec.input_duration_sec << ','
            << rec.output_duration_sec << ','
            << rec.input_peak_linear << ','
            << rec.output_peak_linear << ','
            << escape_csv(rec.status_message) << '\n';
    }

    out.flush();
    if (!out) {
        return make_error(ErrorCode::IoError, "Failed to flush CSV report to disk: " + csv_path.string());
    }
    return {};
}

} // namespace clipforge::services

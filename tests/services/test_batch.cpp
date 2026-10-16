#include "../test_harness.hpp"
#include "clipforge/services/engine_facade.hpp"

#include <filesystem>
#include <fstream>
#include <vector>

namespace clipforge::tests {

namespace {

clipforge::audio::WavFormatInfo mono_fmt() {
    clipforge::audio::WavFormatInfo fmt{};
    fmt.sample_format = clipforge::audio::SampleFormat::Float32;
    fmt.num_channels = 1;
    fmt.sample_rate = 1000;
    fmt.bits_per_sample = 32;
    fmt.block_align = 4;
    fmt.byte_rate = 4000;
    return fmt;
}

void write_test_wav(const std::filesystem::path& path, float level = 0.25f) {
    clipforge::audio::AudioBuffer buf(mono_fmt(), std::vector<float>(100, level));
    auto st = clipforge::audio::WavWriter::write_file(path, buf);
    CF_REQUIRE(st.has_value());
}

clipforge::services::BatchRequest make_request(
    const std::filesystem::path& in_dir,
    const std::filesystem::path& out_dir
) {
    clipforge::services::BatchRequest req{};
    req.input_dir = in_dir;
    req.output_dir = out_dir;
    req.operation = clipforge::services::BatchOperation::Normalize;
    req.normalize_options.target_dbfs = -1.0f;
    req.normalize_options.backend = clipforge::dsp::Backend::Cpp;
    return req;
}

} // namespace

void run_batch_tests() {
    using namespace clipforge;
    using namespace clipforge::services;

    // 1. Dry-run normalize: counted as success, nothing written
    {
        const auto base = std::filesystem::temp_directory_path() / "clipforge_test_batch_dry";
        std::error_code ec;
        std::filesystem::remove_all(base, ec);
        std::filesystem::create_directories(base / "in", ec);
        write_test_wav(base / "in" / "a.wav");

        auto req = make_request(base / "in", base / "out");
        req.dry_run = true;
        auto res = EngineFacade::run_batch(req);
        CF_REQUIRE(res.has_value());
        CF_REQUIRE(res->total_discovered == 1);
        CF_REQUIRE(res->succeeded_count == 1);
        CF_REQUIRE(res->failed_count == 0);
        CF_REQUIRE(res->skipped_dry_run_count == 1);
        CF_REQUIRE(!std::filesystem::exists(base / "out" / "a.wav", ec));

        std::filesystem::remove_all(base, ec);
    }

    // 2. Real normalize writes output and preserves count
    {
        const auto base = std::filesystem::temp_directory_path() / "clipforge_test_batch_real";
        std::error_code ec;
        std::filesystem::remove_all(base, ec);
        std::filesystem::create_directories(base / "in", ec);
        write_test_wav(base / "in" / "a.wav");
        write_test_wav(base / "in" / "b.wav", 0.1f);

        auto req = make_request(base / "in", base / "out");
        auto res = EngineFacade::run_batch(req);
        CF_REQUIRE(res.has_value());
        CF_REQUIRE(res->total_discovered == 2);
        CF_REQUIRE(res->succeeded_count == 2);
        CF_REQUIRE(std::filesystem::exists(base / "out" / "a.wav", ec));
        CF_REQUIRE(std::filesystem::exists(base / "out" / "b.wav", ec));

        std::filesystem::remove_all(base, ec);
    }

    // 3. Missing input directory
    {
        auto req = make_request(
            std::filesystem::temp_directory_path() / "clipforge_no_such_dir_xyz",
            std::filesystem::temp_directory_path() / "clipforge_test_batch_out");
        auto res = EngineFacade::run_batch(req);
        CF_REQUIRE(!res.has_value());
        CF_REQUIRE(res.error().code == ErrorCode::IoError);
    }

    // 4. Identical input/output directory is a collision
    {
        const auto base = std::filesystem::temp_directory_path() / "clipforge_test_batch_same";
        std::error_code ec;
        std::filesystem::remove_all(base, ec);
        std::filesystem::create_directories(base / "io", ec);
        write_test_wav(base / "io" / "a.wav");

        auto req = make_request(base / "io", base / "io");
        auto res = EngineFacade::run_batch(req);
        CF_REQUIRE(!res.has_value());
        CF_REQUIRE(res.error().code == ErrorCode::PathCollision);

        std::filesystem::remove_all(base, ec);
    }

    // 5. Existing target without overwrite records TargetExists
    {
        const auto base = std::filesystem::temp_directory_path() / "clipforge_test_batch_exist";
        std::error_code ec;
        std::filesystem::remove_all(base, ec);
        std::filesystem::create_directories(base / "in", ec);
        std::filesystem::create_directories(base / "out", ec);
        write_test_wav(base / "in" / "a.wav");
        write_test_wav(base / "out" / "a.wav");

        auto req = make_request(base / "in", base / "out");
        req.overwrite_existing = false;
        auto res = EngineFacade::run_batch(req);
        CF_REQUIRE(res.has_value());
        CF_REQUIRE(res->succeeded_count == 0);
        CF_REQUIRE(res->failed_count == 1);
        CF_REQUIRE(res->records.size() == 1);
        CF_REQUIRE(!res->records[0].succeeded);

        std::filesystem::remove_all(base, ec);
    }

    // 6. CSV report is written with a header row
    {
        const auto base = std::filesystem::temp_directory_path() / "clipforge_test_batch_csv";
        std::error_code ec;
        std::filesystem::remove_all(base, ec);
        std::filesystem::create_directories(base / "in", ec);
        write_test_wav(base / "in" / "a.wav");

        auto req = make_request(base / "in", base / "out");
        req.dry_run = true;
        req.csv_report_path = base / "report.csv";
        auto res = EngineFacade::run_batch(req);
        CF_REQUIRE(res.has_value());
        CF_REQUIRE(std::filesystem::exists(base / "report.csv", ec));

        std::ifstream csv(base / "report.csv");
        std::string header;
        std::getline(csv, header);
        CF_REQUIRE(header.find("input_path") != std::string::npos);
        CF_REQUIRE(header.find("status_message") != std::string::npos);
        std::string row;
        std::getline(csv, row);
        CF_REQUIRE(!row.empty());

        std::filesystem::remove_all(base, ec);
    }

    // 7. Trim operation in dry-run keeps batch green
    {
        const auto base = std::filesystem::temp_directory_path() / "clipforge_test_batch_trim";
        std::error_code ec;
        std::filesystem::remove_all(base, ec);
        std::filesystem::create_directories(base / "in", ec);
        write_test_wav(base / "in" / "a.wav");

        auto req = make_request(base / "in", base / "out");
        req.operation = BatchOperation::Trim;
        req.dry_run = true;
        auto res = EngineFacade::run_batch(req);
        CF_REQUIRE(res.has_value());
        CF_REQUIRE(res->succeeded_count == 1);

        std::filesystem::remove_all(base, ec);
    }
}

} // namespace clipforge::tests

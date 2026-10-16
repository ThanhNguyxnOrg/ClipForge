#include "../test_harness.hpp"
#include "clipforge/services/engine_facade.hpp"

#include <cmath>
#include <filesystem>
#include <limits>
#include <vector>

namespace clipforge::tests {

void run_processing_tests() {
    using namespace clipforge;
    using namespace clipforge::audio;
    using namespace clipforge::processing;

    WavFormatInfo stereo_fmt{};
    stereo_fmt.sample_format = SampleFormat::Float32;
    stereo_fmt.num_channels = 2;
    stereo_fmt.sample_rate = 1000;
    stereo_fmt.bits_per_sample = 32;
    stereo_fmt.block_align = 8;
    stereo_fmt.byte_rate = 8000;

    // 1. Silent input normalization safety
    {
        AudioBuffer silent_buf(stereo_fmt, std::vector<float>(200, 0.0f));
        auto norm_res = Normalizer::normalize_in_place(silent_buf, {-1.0f, dsp::Backend::Cpp});
        CF_REQUIRE(norm_res.has_value());
        CF_REQUIRE(norm_res->was_silent);
        CF_REQUIRE_NEAR(norm_res->output_peak_linear, 0.0, 1e-9);
    }

    // 2. Peak normalization to -6.0206 dBFS (~0.5 linear)
    {
        AudioBuffer buf(stereo_fmt, {0.1f, -0.25f, 0.2f, -0.1f});
        const float target_dbfs = -6.0205999f;
        auto norm_res = Normalizer::normalize_in_place(buf, {target_dbfs, dsp::Backend::Auto});
        CF_REQUIRE(norm_res.has_value());
        CF_REQUIRE(!norm_res->was_silent);
        CF_REQUIRE_NEAR(norm_res->output_peak_linear, 0.5, 1e-4);
    }

    // 3. Silence trimming at frame level
    {
        std::vector<float> samples(20, 0.0f);
        for (std::size_t f = 2; f <= 5; ++f) {
            samples[f * 2 + 1] = 0.5f;
        }
        AudioBuffer buf(stereo_fmt, samples);

        SilenceTrimOptions opts_no_pad{-40.0f, 0.0, 0.0, 0.0};
        auto trimmed_no_pad = SilenceTrimmer::trim(buf, opts_no_pad);
        CF_REQUIRE(trimmed_no_pad.has_value());
        CF_REQUIRE(trimmed_no_pad->trimmed_frames == 4);
        CF_REQUIRE(trimmed_no_pad->removed_leading_frames == 2);
        CF_REQUIRE(trimmed_no_pad->removed_trailing_frames == 4);

        SilenceTrimOptions opts_tail{-40.0f, 0.0, 2.0, 0.0};
        auto trimmed_tail = SilenceTrimmer::trim(buf, opts_tail);
        CF_REQUIRE(trimmed_tail.has_value());
        CF_REQUIRE(trimmed_tail->trimmed_frames == 6);
        CF_REQUIRE(trimmed_tail->removed_leading_frames == 2);
        CF_REQUIRE(trimmed_tail->removed_trailing_frames == 2);
    }

    // 4. Protection against overwriting the source file
    {
        const auto tmp_dir = std::filesystem::temp_directory_path() / "clipforge_test_safety";
        std::error_code ec;
        std::filesystem::remove_all(tmp_dir, ec);
        std::filesystem::create_directories(tmp_dir, ec);

        const auto src_file = tmp_dir / "source.wav";
        AudioBuffer buf(stereo_fmt, {0.25f, -0.25f, 0.5f, -0.5f});
        CF_REQUIRE(WavWriter::write_file(src_file, buf).has_value());

        auto overwrite_attempt = services::EngineFacade::normalize_file(
            src_file, src_file, {-1.0f, dsp::Backend::Auto}, false
        );
        CF_REQUIRE(!overwrite_attempt.has_value());
        CF_REQUIRE(overwrite_attempt.error().code == ErrorCode::PathCollision);

        std::filesystem::remove_all(tmp_dir, ec);
    }

    // 5. All-silent input trims to a single retained frame
    {
        AudioBuffer buf(stereo_fmt, std::vector<float>(20, 0.0f));
        SilenceTrimOptions opts{-40.0f, 0.0, 0.0, 0.0};
        auto res = SilenceTrimmer::trim(buf, opts);
        CF_REQUIRE(res.has_value());
        CF_REQUIRE(res->all_silent);
        CF_REQUIRE(res->original_frames == 10);
        CF_REQUIRE(res->trimmed_frames == 1);
    }

    // 6. Minimum non-silent duration filters out short blips
    {
        std::vector<float> samples(20, 0.0f);
        samples[4] = 0.5f; // single active frame in the left channel
        AudioBuffer buf(stereo_fmt, samples);

        SilenceTrimOptions strict{-40.0f, 0.0, 0.0, 5.0}; // 5 ms at 1000 Hz
        auto res = SilenceTrimmer::trim(buf, strict);
        CF_REQUIRE(res.has_value());
        CF_REQUIRE(res->all_silent);

        SilenceTrimOptions lax{-40.0f, 0.0, 0.0, 1.0}; // 1 ms: the blip survives
        auto kept = SilenceTrimmer::trim(buf, lax);
        CF_REQUIRE(kept.has_value());
        CF_REQUIRE(!kept->all_silent);
        CF_REQUIRE(kept->trimmed_frames == 1);
    }

    // 7. Head padding retains leading frames
    {
        std::vector<float> samples(20, 0.0f);
        for (std::size_t f = 4; f <= 5; ++f) {
            samples[f * 2] = 0.5f;
        }
        AudioBuffer buf(stereo_fmt, samples);

        SilenceTrimOptions opts{-40.0f, 2.0, 0.0, 0.0}; // 2 ms head pad
        auto res = SilenceTrimmer::trim(buf, opts);
        CF_REQUIRE(res.has_value());
        CF_REQUIRE(res->trimmed_frames == 4);
        CF_REQUIRE(res->removed_leading_frames == 2);
        CF_REQUIRE(res->removed_trailing_frames == 4);
    }

    // 8. Analyzer reports peak, clipping, RMS, and silence on known input
    {
        // 4 stereo frames at 1000 Hz: active, silent, active, silent.
        AudioBuffer buf(stereo_fmt, {
            0.5f, -0.5f,
            0.0f, 0.0f,
            0.25f, 0.125f,
            0.0f, 0.0f,
        });
        AnalysisOptions opts{0.5f, -50.0f, dsp::Backend::Cpp};
        auto res = Analyzer::analyze(buf, opts);
        CF_REQUIRE(res.has_value());
        CF_REQUIRE_NEAR(res->peak_linear, 0.5, 1e-6);
        CF_REQUIRE_NEAR(res->peak_dbfs, -6.0205999, 1e-4);
        CF_REQUIRE(res->clipped_sample_count == 2); // +0.5 and -0.5
        CF_REQUIRE(res->rms_linear > 0.0);
        CF_REQUIRE(!res->all_silent);
        CF_REQUIRE(res->total_frames == 4);
        CF_REQUIRE(res->total_samples == 8);
        CF_REQUIRE_NEAR(res->duration_seconds, 0.004, 1e-9);
    }

    // 9. Analyzer on empty input returns a zeroed report
    {
        AudioBuffer buf(stereo_fmt, {});
        auto res = Analyzer::analyze(buf, {});
        CF_REQUIRE(res.has_value());
        CF_REQUIRE(res->all_silent);
        CF_REQUIRE_NEAR(res->peak_linear, 0.0, 1e-9);
        CF_REQUIRE(res->total_frames == 0);
    }

    // 10. Invalid parameters are rejected with InvalidParameter
    {
        AudioBuffer buf(stereo_fmt, {0.25f, -0.25f});
        auto over = Normalizer::normalize_in_place(buf, {1.0f, dsp::Backend::Cpp});
        CF_REQUIRE(!over.has_value());
        CF_REQUIRE(over.error().code == ErrorCode::InvalidParameter);

        auto under = Normalizer::normalize_in_place(buf, {-121.0f, dsp::Backend::Cpp});
        CF_REQUIRE(!under.has_value());
        CF_REQUIRE(under.error().code == ErrorCode::InvalidParameter);

        SilenceTrimOptions bad_thresh{5.0f, 0.0, 0.0, 0.0};
        auto trim_bad = SilenceTrimmer::trim(buf, bad_thresh);
        CF_REQUIRE(!trim_bad.has_value());
        CF_REQUIRE(trim_bad.error().code == ErrorCode::InvalidParameter);

        AnalysisOptions bad_clip{-1.0f, -50.0f, dsp::Backend::Cpp};
        auto analyze_bad = Analyzer::analyze(buf, bad_clip);
        CF_REQUIRE(!analyze_bad.has_value());
        CF_REQUIRE(analyze_bad.error().code == ErrorCode::InvalidParameter);
    }

    // 11. NaN buffers are rejected, never processed
    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        AudioBuffer nan_buf(stereo_fmt, {nan, 0.1f});
        auto norm_res = Normalizer::normalize_in_place(nan_buf, {-1.0f, dsp::Backend::Cpp});
        CF_REQUIRE(!norm_res.has_value());
        CF_REQUIRE(norm_res.error().code == ErrorCode::InvalidSampleValue);

        auto trim_res = SilenceTrimmer::trim(nan_buf, {});
        CF_REQUIRE(!trim_res.has_value());
        CF_REQUIRE(trim_res.error().code == ErrorCode::InvalidSampleValue);

        auto analyze_res = Analyzer::analyze(nan_buf, {});
        CF_REQUIRE(!analyze_res.has_value());
        CF_REQUIRE(analyze_res.error().code == ErrorCode::InvalidSampleValue);
    }
}

} // namespace clipforge::tests

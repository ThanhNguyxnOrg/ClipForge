#include "../test_harness.hpp"
#include "clipforge/services/engine_facade.hpp"

#include <cmath>
#include <filesystem>
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
}

} // namespace clipforge::tests

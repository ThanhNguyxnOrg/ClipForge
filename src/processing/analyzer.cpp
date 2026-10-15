#include "clipforge/processing/analyzer.hpp"

#include <algorithm>
#include <cmath>

namespace clipforge::processing {

Result<AudioAnalysisReport> Analyzer::analyze(
    const audio::AudioBuffer& buffer,
    const AnalysisOptions& options
) {
    if (!std::isfinite(options.clip_threshold_linear) || options.clip_threshold_linear <= 0.0f) {
        return make_error(
            ErrorCode::InvalidParameter,
            "Clipping threshold must be a finite positive linear amplitude."
        );
    }
    if (!std::isfinite(options.silence_threshold_dbfs) || options.silence_threshold_dbfs >= 0.0f) {
        return make_error(
            ErrorCode::InvalidParameter,
            "Silence threshold must be a finite negative dBFS value."
        );
    }

    const auto samples = buffer.samples();
    for (float s : samples) {
        if (!std::isfinite(s)) {
            return make_error(
                ErrorCode::InvalidSampleValue,
                "AudioBuffer contains non-finite sample value."
            );
        }
    }

    const dsp::Backend resolved = dsp::resolve_backend(options.backend);

    AudioAnalysisReport report{};
    report.sample_format = buffer.sample_format();
    report.num_channels = buffer.channels();
    report.sample_rate = buffer.sample_rate();
    report.bits_per_sample = buffer.bit_depth();
    report.total_frames = buffer.frame_count();
    report.total_samples = buffer.sample_count();
    report.duration_seconds = buffer.duration_seconds();
    report.active_backend = resolved;

    if (samples.empty() || buffer.channels() == 0 || buffer.sample_rate() == 0) {
        return report;
    }

    report.peak_linear = dsp::peak_abs_f32(
        samples.data(),
        static_cast<std::uint64_t>(samples.size()),
        resolved
    );
    if (report.peak_linear > 0.0f) {
        report.peak_dbfs = 20.0 * std::log10(static_cast<double>(report.peak_linear));
    }

    report.clipped_sample_count = dsp::count_clipped_f32(
        samples.data(),
        static_cast<std::uint64_t>(samples.size()),
        options.clip_threshold_linear,
        resolved
    );

    double sum_sq = 0.0;
    for (float s : samples) {
        const double d = static_cast<double>(s);
        sum_sq += d * d;
    }
    report.rms_linear = std::sqrt(sum_sq / static_cast<double>(samples.size()));
    if (report.rms_linear > 0.0) {
        report.rms_dbfs = 20.0 * std::log10(report.rms_linear);
    }

    const float silence_linear =
        static_cast<float>(std::pow(10.0, static_cast<double>(options.silence_threshold_dbfs) / 20.0));

    const std::size_t channels = buffer.channels();
    const std::size_t frames = buffer.frame_count();
    std::size_t first_active = frames;
    std::size_t last_active = 0;

    for (std::size_t f = 0; f < frames; ++f) {
        float frame_peak = 0.0f;
        const std::size_t base = f * channels;
        for (std::size_t c = 0; c < channels; ++c) {
            frame_peak = std::max(frame_peak, std::fabs(samples[base + c]));
        }
        if (frame_peak > silence_linear) {
            if (first_active == frames) {
                first_active = f;
            }
            last_active = f;
        }
    }

    if (first_active == frames) {
        report.all_silent = true;
        report.leading_silence_seconds = report.duration_seconds;
        report.trailing_silence_seconds = report.duration_seconds;
    } else {
        report.all_silent = false;
        report.leading_silence_seconds =
            static_cast<double>(first_active) / static_cast<double>(buffer.sample_rate());
        const std::size_t trailing_frames = frames - 1 - last_active;
        report.trailing_silence_seconds =
            static_cast<double>(trailing_frames) / static_cast<double>(buffer.sample_rate());
    }

    return report;
}

} // namespace clipforge::processing

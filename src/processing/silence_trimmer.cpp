#include "clipforge/processing/silence_trimmer.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace clipforge::processing {

Result<SilenceTrimResult> SilenceTrimmer::trim(
    const audio::AudioBuffer& input,
    const SilenceTrimOptions& options
) {
    if (!std::isfinite(options.threshold_dbfs) || options.threshold_dbfs >= 0.0f ||
        options.threshold_dbfs < -140.0f) {
        return make_error(
            ErrorCode::InvalidParameter,
            "Silence threshold must be finite and within [-140.0, 0.0) dBFS."
        );
    }
    if (!std::isfinite(options.head_pad_ms) || options.head_pad_ms < 0.0 ||
        !std::isfinite(options.tail_pad_ms) || options.tail_pad_ms < 0.0 ||
        !std::isfinite(options.min_non_silent_ms) || options.min_non_silent_ms < 0.0) {
        return make_error(
            ErrorCode::InvalidParameter,
            "Padding and minimum duration values must be finite and non-negative."
        );
    }

    const auto samples = input.samples();
    for (float s : samples) {
        if (!std::isfinite(s)) {
            return make_error(
                ErrorCode::InvalidSampleValue,
                "Cannot trim audio buffer containing NaN or Infinity."
            );
        }
    }

    const std::size_t channels = input.channels();
    const std::size_t total_frames = input.frame_count();
    const std::uint32_t sr = input.sample_rate();

    if (channels == 0 || sr == 0 || total_frames == 0) {
        SilenceTrimResult empty_res{};
        empty_res.buffer = input;
        empty_res.original_frames = total_frames;
        empty_res.trimmed_frames = total_frames;
        empty_res.all_silent = true;
        return empty_res;
    }

    const float threshold_linear =
        static_cast<float>(std::pow(10.0, static_cast<double>(options.threshold_dbfs) / 20.0));

    std::size_t first_active = total_frames;
    std::size_t last_active = 0;

    for (std::size_t f = 0; f < total_frames; ++f) {
        float frame_peak = 0.0f;
        const std::size_t base = f * channels;
        for (std::size_t c = 0; c < channels; ++c) {
            frame_peak = std::max(frame_peak, std::fabs(samples[base + c]));
        }
        if (frame_peak > threshold_linear) {
            if (first_active == total_frames) {
                first_active = f;
            }
            last_active = f;
        }
    }

    const auto ms_to_frames = [sr](double ms) -> std::size_t {
        return static_cast<std::size_t>(std::llround((ms / 1000.0) * static_cast<double>(sr)));
    };

    const std::size_t min_active_frames = ms_to_frames(options.min_non_silent_ms);

    if (first_active == total_frames || (last_active - first_active + 1) < min_active_frames) {
        std::vector<float> minimal_frame(channels, 0.0f);
        SilenceTrimResult res{};
        res.buffer = audio::AudioBuffer(input.format(), std::move(minimal_frame));
        res.original_frames = total_frames;
        res.trimmed_frames = 1;
        res.removed_leading_frames = total_frames - 1;
        res.removed_trailing_frames = 0;
        res.all_silent = true;
        return res;
    }

    const std::size_t head_pad_frames = ms_to_frames(options.head_pad_ms);
    const std::size_t tail_pad_frames = ms_to_frames(options.tail_pad_ms);

    const std::size_t start_frame =
        (first_active > head_pad_frames) ? (first_active - head_pad_frames) : 0;
    const std::size_t end_frame =
        std::min(total_frames, last_active + 1 + tail_pad_frames);

    const std::size_t kept_frames = end_frame - start_frame;
    std::vector<float> trimmed_samples(kept_frames * channels);

    const auto begin_it = samples.begin() + static_cast<std::ptrdiff_t>(start_frame * channels);
    const auto end_it = begin_it + static_cast<std::ptrdiff_t>(kept_frames * channels);
    std::copy(begin_it, end_it, trimmed_samples.begin());

    SilenceTrimResult res{};
    res.buffer = audio::AudioBuffer(input.format(), std::move(trimmed_samples));
    res.original_frames = total_frames;
    res.trimmed_frames = kept_frames;
    res.removed_leading_frames = start_frame;
    res.removed_trailing_frames = total_frames - end_frame;
    res.all_silent = false;
    return res;
}

} // namespace clipforge::processing

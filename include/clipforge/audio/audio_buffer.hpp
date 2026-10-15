#pragma once

#include "clipforge/audio/wav_types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace clipforge::audio {

class AudioBuffer {
public:
    AudioBuffer() = default;

    AudioBuffer(WavFormatInfo format, std::vector<float> interleaved_samples)
        : format_(format), samples_(std::move(interleaved_samples)) {}

    [[nodiscard]] const WavFormatInfo& format() const noexcept { return format_; }
    [[nodiscard]] std::uint16_t channels() const noexcept { return format_.num_channels; }
    [[nodiscard]] std::uint32_t sample_rate() const noexcept { return format_.sample_rate; }
    [[nodiscard]] SampleFormat sample_format() const noexcept { return format_.sample_format; }
    [[nodiscard]] std::uint16_t bit_depth() const noexcept { return format_.bits_per_sample; }

    [[nodiscard]] std::size_t sample_count() const noexcept { return samples_.size(); }

    [[nodiscard]] std::size_t frame_count() const noexcept {
        return format_.num_channels > 0 ? (samples_.size() / format_.num_channels) : 0;
    }

    [[nodiscard]] double duration_seconds() const noexcept {
        if (format_.sample_rate == 0) {
            return 0.0;
        }
        return static_cast<double>(frame_count()) / static_cast<double>(format_.sample_rate);
    }

    [[nodiscard]] const float* data() const noexcept { return samples_.data(); }
    [[nodiscard]] float* data() noexcept { return samples_.data(); }

    [[nodiscard]] std::span<const float> samples() const noexcept { return samples_; }
    [[nodiscard]] std::span<float> samples() noexcept { return samples_; }

    void replace_samples(std::vector<float> new_samples) noexcept {
        samples_ = std::move(new_samples);
    }

private:
    WavFormatInfo format_{};
    std::vector<float> samples_{};
};

} // namespace clipforge::audio

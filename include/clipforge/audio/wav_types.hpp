#pragma once

#include <cstdint>
#include <string_view>

namespace clipforge::audio {

enum class SampleFormat {
    Pcm16,
    Pcm24,
    Float32
};

[[nodiscard]] constexpr std::string_view to_string(SampleFormat fmt) noexcept {
    switch (fmt) {
        case SampleFormat::Pcm16:   return "PCM Signed 16-bit";
        case SampleFormat::Pcm24:   return "PCM Signed 24-bit";
        case SampleFormat::Float32: return "IEEE Float 32-bit";
    }
    return "Unknown";
}

[[nodiscard]] constexpr std::uint16_t bits_per_sample(SampleFormat fmt) noexcept {
    switch (fmt) {
        case SampleFormat::Pcm16:   return 16;
        case SampleFormat::Pcm24:   return 24;
        case SampleFormat::Float32: return 32;
    }
    return 0;
}

struct WavFormatInfo {
    SampleFormat sample_format = SampleFormat::Pcm16;
    std::uint16_t num_channels = 1;
    std::uint32_t sample_rate = 48000;
    std::uint16_t bits_per_sample = 16;
    std::uint16_t block_align = 2;
    std::uint32_t byte_rate = 96000;
};

} // namespace clipforge::audio

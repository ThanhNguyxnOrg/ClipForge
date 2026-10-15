#pragma once

#include "clipforge/audio/audio_buffer.hpp"
#include "clipforge/core/result.hpp"
#include "clipforge/dsp/kernels.hpp"

#include <cmath>
#include <cstdint>

namespace clipforge::processing {

struct AnalysisOptions {
    float clip_threshold_linear = 0.9999f;
    float silence_threshold_dbfs = -50.0f;
    dsp::Backend backend = dsp::Backend::Auto;
};

struct AudioAnalysisReport {
    audio::SampleFormat sample_format = audio::SampleFormat::Pcm16;
    std::uint16_t num_channels = 1;
    std::uint32_t sample_rate = 48000;
    std::uint16_t bits_per_sample = 16;
    std::uint64_t total_frames = 0;
    std::uint64_t total_samples = 0;
    double duration_seconds = 0.0;

    float peak_linear = 0.0f;
    double peak_dbfs = -HUGE_VAL;
    double rms_linear = 0.0;
    double rms_dbfs = -HUGE_VAL;

    std::uint64_t clipped_sample_count = 0;
    double leading_silence_seconds = 0.0;
    double trailing_silence_seconds = 0.0;
    bool all_silent = true;
    dsp::Backend active_backend = dsp::Backend::Cpp;
};

class Analyzer {
public:
    [[nodiscard]] static Result<AudioAnalysisReport> analyze(
        const audio::AudioBuffer& buffer,
        const AnalysisOptions& options = {}
    );
};

} // namespace clipforge::processing

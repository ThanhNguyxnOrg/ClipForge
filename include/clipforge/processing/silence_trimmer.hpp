#pragma once

#include "clipforge/audio/audio_buffer.hpp"
#include "clipforge/core/result.hpp"

#include <cstddef>

namespace clipforge::processing {

struct SilenceTrimOptions {
    float threshold_dbfs = -45.0f;
    double head_pad_ms = 0.0;
    double tail_pad_ms = 100.0;
    double min_non_silent_ms = 0.0;
};

struct SilenceTrimResult {
    audio::AudioBuffer buffer;
    std::size_t original_frames = 0;
    std::size_t trimmed_frames = 0;
    std::size_t removed_leading_frames = 0;
    std::size_t removed_trailing_frames = 0;
    bool all_silent = false;
};

class SilenceTrimmer {
public:
    // Evaluates multi-channel activity at the frame level and trims leading/trailing silence.
    [[nodiscard]] static Result<SilenceTrimResult> trim(
        const audio::AudioBuffer& input,
        const SilenceTrimOptions& options = {}
    );
};

} // namespace clipforge::processing

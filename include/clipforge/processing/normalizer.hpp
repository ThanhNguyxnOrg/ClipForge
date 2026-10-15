#pragma once

#include "clipforge/audio/audio_buffer.hpp"
#include "clipforge/core/result.hpp"
#include "clipforge/dsp/kernels.hpp"

namespace clipforge::processing {

struct NormalizeOptions {
    float target_dbfs = -1.0f;
    dsp::Backend backend = dsp::Backend::Auto;
};

struct NormalizeResult {
    float input_peak_linear = 0.0f;
    float output_peak_linear = 0.0f;
    float target_ceiling_linear = 0.0f;
    float applied_gain_linear = 1.0f;
    double applied_gain_db = 0.0;
    bool was_silent = false;
    dsp::Backend active_backend = dsp::Backend::Cpp;
};

class Normalizer {
public:
    // Normalizes buffer in-place to options.target_dbfs and returns normalization metadata.
    [[nodiscard]] static Result<NormalizeResult> normalize_in_place(
        audio::AudioBuffer& buffer,
        const NormalizeOptions& options = {}
    );
};

} // namespace clipforge::processing

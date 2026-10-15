#include "clipforge/processing/normalizer.hpp"

#include <cmath>

namespace clipforge::processing {

Result<NormalizeResult> Normalizer::normalize_in_place(
    audio::AudioBuffer& buffer,
    const NormalizeOptions& options
) {
    if (!std::isfinite(options.target_dbfs) || options.target_dbfs > 0.0f || options.target_dbfs < -120.0f) {
        return make_error(
            ErrorCode::InvalidParameter,
            "Target dBFS must be finite and within [-120.0, 0.0] dBFS."
        );
    }

    auto samples = buffer.samples();
    for (float s : samples) {
        if (!std::isfinite(s)) {
            return make_error(
                ErrorCode::InvalidSampleValue,
                "Cannot normalize buffer containing NaN or Infinity."
            );
        }
    }

    const dsp::Backend resolved = dsp::resolve_backend(options.backend);
    const float target_linear =
        static_cast<float>(std::pow(10.0, static_cast<double>(options.target_dbfs) / 20.0));

    NormalizeResult result{};
    result.target_ceiling_linear = target_linear;
    result.active_backend = resolved;

    if (samples.empty()) {
        result.was_silent = true;
        return result;
    }

    const float input_peak = dsp::peak_abs_f32(
        samples.data(),
        static_cast<std::uint64_t>(samples.size()),
        resolved
    );
    result.input_peak_linear = input_peak;

    constexpr float kSilenceEpsilon = 1.0e-12f;
    if (input_peak <= kSilenceEpsilon) {
        result.was_silent = true;
        result.output_peak_linear = input_peak;
        result.applied_gain_linear = 1.0f;
        result.applied_gain_db = 0.0;
        return result;
    }

    const float gain = target_linear / input_peak;
    if (!std::isfinite(gain) || gain <= 0.0f) {
        return make_error(ErrorCode::InvalidParameter, "Computed normalization gain is non-finite.");
    }

    dsp::apply_gain_clamp_f32(
        samples.data(),
        static_cast<std::uint64_t>(samples.size()),
        gain,
        target_linear,
        resolved
    );

    result.output_peak_linear = dsp::peak_abs_f32(
        samples.data(),
        static_cast<std::uint64_t>(samples.size()),
        resolved
    );
    result.applied_gain_linear = gain;
    result.applied_gain_db = 20.0 * std::log10(static_cast<double>(gain));
    result.was_silent = false;

    return result;
}

} // namespace clipforge::processing

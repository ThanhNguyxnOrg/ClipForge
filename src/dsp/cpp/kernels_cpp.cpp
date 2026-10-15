#include "clipforge/dsp/kernels.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace clipforge::dsp {

namespace {

[[nodiscard]] float bitwise_abs_f32(float v) noexcept {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(float));
    bits &= 0x7FFFFFFFu;
    float out = 0.0f;
    std::memcpy(&out, &bits, sizeof(float));
    return out;
}

} // namespace

float cf_peak_abs_f32_cpp(const float* samples, std::uint64_t count) noexcept {
    if (samples == nullptr || count == 0) {
        return 0.0f;
    }

    float max_val = 0.0f;
    for (std::uint64_t i = 0; i < count; ++i) {
        const float a = bitwise_abs_f32(samples[i]);
        if (a > max_val) {
            max_val = a;
        }
    }
    return max_val;
}

std::uint64_t cf_count_clipped_f32_cpp(
    const float* samples,
    std::uint64_t count,
    float threshold
) noexcept {
    if (samples == nullptr || count == 0) {
        return 0;
    }

    const float abs_thresh = bitwise_abs_f32(threshold);
    std::uint64_t clipped = 0;
    for (std::uint64_t i = 0; i < count; ++i) {
        if (bitwise_abs_f32(samples[i]) >= abs_thresh) {
            ++clipped;
        }
    }
    return clipped;
}

void cf_apply_gain_clamp_f32_cpp(
    float* samples,
    std::uint64_t count,
    float gain,
    float ceiling
) noexcept {
    if (samples == nullptr || count == 0 || ceiling < 0.0f) {
        return;
    }

    const float neg_ceiling = -ceiling;
    for (std::uint64_t i = 0; i < count; ++i) {
        float v = samples[i] * gain;
        v = (v > neg_ceiling) ? v : neg_ceiling;
        v = (v < ceiling) ? v : ceiling;
        samples[i] = v;
    }
}

} // namespace clipforge::dsp

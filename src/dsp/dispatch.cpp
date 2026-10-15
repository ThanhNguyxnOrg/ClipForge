#include "clipforge/dsp/kernels.hpp"

namespace clipforge::dsp {

bool is_asm_backend_available() noexcept {
#if defined(CLIPFORGE_HAS_NASM_X64) && (CLIPFORGE_HAS_NASM_X64 == 1)
    return true;
#else
    return false;
#endif
}

Backend resolve_backend(Backend requested) noexcept {
    if (requested == Backend::Cpp) {
        return Backend::Cpp;
    }
    if (is_asm_backend_available()) {
        return Backend::Asm;
    }
    return Backend::Cpp;
}

float peak_abs_f32(const float* samples, std::uint64_t count, Backend backend) noexcept {
    if (resolve_backend(backend) == Backend::Asm) {
#if defined(CLIPFORGE_HAS_NASM_X64) && (CLIPFORGE_HAS_NASM_X64 == 1)
        return cf_peak_abs_f32(samples, count);
#endif
    }
    return cf_peak_abs_f32_cpp(samples, count);
}

std::uint64_t count_clipped_f32(
    const float* samples,
    std::uint64_t count,
    float threshold,
    Backend backend
) noexcept {
    if (resolve_backend(backend) == Backend::Asm) {
#if defined(CLIPFORGE_HAS_NASM_X64) && (CLIPFORGE_HAS_NASM_X64 == 1)
        return cf_count_clipped_f32(samples, count, threshold);
#endif
    }
    return cf_count_clipped_f32_cpp(samples, count, threshold);
}

void apply_gain_clamp_f32(
    float* samples,
    std::uint64_t count,
    float gain,
    float ceiling,
    Backend backend
) noexcept {
    if (resolve_backend(backend) == Backend::Asm) {
#if defined(CLIPFORGE_HAS_NASM_X64) && (CLIPFORGE_HAS_NASM_X64 == 1)
        cf_apply_gain_clamp_f32(samples, count, gain, ceiling);
        return;
#endif
    }
    cf_apply_gain_clamp_f32_cpp(samples, count, gain, ceiling);
}

} // namespace clipforge::dsp

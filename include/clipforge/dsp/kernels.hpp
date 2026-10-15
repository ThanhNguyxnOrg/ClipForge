#pragma once

#include <cstdint>
#include <string_view>

namespace clipforge::dsp {

enum class Backend {
    Auto,
    Cpp,
    Asm
};

[[nodiscard]] constexpr std::string_view to_string(Backend backend) noexcept {
    switch (backend) {
        case Backend::Auto: return "auto";
        case Backend::Cpp:  return "cpp";
        case Backend::Asm:  return "asm";
    }
    return "unknown";
}

// Returns true if the NASM x86-64 SSE2 backend was compiled into this binary.
[[nodiscard]] bool is_asm_backend_available() noexcept;

// Resolves Backend::Auto to Backend::Asm when compiled in, or Backend::Cpp otherwise.
[[nodiscard]] Backend resolve_backend(Backend requested) noexcept;

// C++ reference implementations (always compiled and testable).
[[nodiscard]] float cf_peak_abs_f32_cpp(
    const float* samples,
    std::uint64_t count
) noexcept;

[[nodiscard]] std::uint64_t cf_count_clipped_f32_cpp(
    const float* samples,
    std::uint64_t count,
    float threshold
) noexcept;

void cf_apply_gain_clamp_f32_cpp(
    float* samples,
    std::uint64_t count,
    float gain,
    float ceiling
) noexcept;

// High-level dispatched entry points.
[[nodiscard]] float peak_abs_f32(
    const float* samples,
    std::uint64_t count,
    Backend backend = Backend::Auto
) noexcept;

[[nodiscard]] std::uint64_t count_clipped_f32(
    const float* samples,
    std::uint64_t count,
    float threshold,
    Backend backend = Backend::Auto
) noexcept;

void apply_gain_clamp_f32(
    float* samples,
    std::uint64_t count,
    float gain,
    float ceiling,
    Backend backend = Backend::Auto
) noexcept;

} // namespace clipforge::dsp

// Stable C ABI declarations for NASM x86-64 routines.
extern "C" {

float cf_peak_abs_f32(
    const float* samples,
    std::uint64_t count
);

std::uint64_t cf_count_clipped_f32(
    const float* samples,
    std::uint64_t count,
    float threshold
);

void cf_apply_gain_clamp_f32(
    float* samples,
    std::uint64_t count,
    float gain,
    float ceiling
);

} // extern "C"

#include "../test_harness.hpp"
#include "clipforge/dsp/kernels.hpp"

#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

namespace clipforge::tests {

namespace {

[[nodiscard]] bool float_bits_equal(float a, float b) noexcept {
    std::uint32_t ba = 0;
    std::uint32_t bb = 0;
    std::memcpy(&ba, &a, sizeof(float));
    std::memcpy(&bb, &b, sizeof(float));
    return ba == bb;
}

} // namespace

void run_dsp_differential_tests() {
    using namespace clipforge::dsp;

    const std::vector<std::size_t> counts = {
        0, 1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 31, 33, 63, 64, 65, 255, 1025, 4099, 65537
    };

    std::mt19937 rng(0xC11DF026u);
    std::uniform_real_distribution<float> dist(-1.5f, 1.5f);

    for (std::size_t misalignment = 0; misalignment < 4; ++misalignment) {
        for (std::size_t count : counts) {
            std::vector<float> raw(count + misalignment + 8, 12345.0f);
            float* base = raw.data() + misalignment;

            for (std::size_t i = 0; i < count; ++i) {
                base[i] = dist(rng);
            }
            if (count >= 4) {
                base[0] = -0.9999f;
                base[1] = 0.9999f;
                base[2] = -0.0f;
                base[3] = 1.0f;
            }

            const float peak_cpp = peak_abs_f32(base, count, Backend::Cpp);
            const float peak_asm = peak_abs_f32(base, count, Backend::Asm);
            CF_REQUIRE(float_bits_equal(peak_cpp, peak_asm));

            for (float thresh : {0.0f, 0.5f, 0.9999f, 1.0f, 1.25f, 2.0f}) {
                const auto cnt_cpp = count_clipped_f32(base, count, thresh, Backend::Cpp);
                const auto cnt_asm = count_clipped_f32(base, count, thresh, Backend::Asm);
                CF_REQUIRE(cnt_cpp == cnt_asm);
            }

            std::vector<float> buf_cpp = raw;
            std::vector<float> buf_asm = raw;

            apply_gain_clamp_f32(buf_cpp.data() + misalignment, count, 1.35f, 0.89125f, Backend::Cpp);
            apply_gain_clamp_f32(buf_asm.data() + misalignment, count, 1.35f, 0.89125f, Backend::Asm);

            for (std::size_t i = 0; i < raw.size(); ++i) {
                CF_REQUIRE(float_bits_equal(buf_cpp[i], buf_asm[i]));
            }
        }
    }

    CF_REQUIRE(peak_abs_f32(nullptr, 100, Backend::Asm) == 0.0f);
    CF_REQUIRE(count_clipped_f32(nullptr, 100, 0.9f, Backend::Asm) == 0);
    apply_gain_clamp_f32(nullptr, 100, 1.0f, 1.0f, Backend::Asm);
}

} // namespace clipforge::tests

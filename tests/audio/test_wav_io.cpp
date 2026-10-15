#include "../test_harness.hpp"
#include "clipforge/audio/wav_reader.hpp"
#include "clipforge/audio/wav_writer.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace clipforge::tests {

namespace {

void push_u16(std::vector<std::uint8_t>& v, std::uint16_t x) {
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
}

void push_u32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 16) & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 24) & 0xFF));
}

void push_tag(std::vector<std::uint8_t>& v, const char (&tag)[5]) {
    v.insert(v.end(), tag, tag + 4);
}

} // namespace

void run_wav_io_tests() {
    using namespace clipforge::audio;

    for (SampleFormat fmt : {SampleFormat::Pcm16, SampleFormat::Pcm24, SampleFormat::Float32}) {
        for (std::uint16_t ch : {1, 2}) {
            WavFormatInfo info{};
            info.sample_format = fmt;
            info.num_channels = ch;
            info.sample_rate = (ch == 1) ? 44100 : 48000;
            info.bits_per_sample = bits_per_sample(fmt);
            info.block_align = static_cast<std::uint16_t>(ch * (info.bits_per_sample / 8u));
            info.byte_rate = info.sample_rate * info.block_align;

            std::vector<float> samples = {0.0f, 0.25f, -0.5f, 0.75f, -0.25f, 0.125f};
            AudioBuffer buf(info, samples);

            auto encoded = WavWriter::encode_bytes(buf);
            CF_REQUIRE(encoded.has_value());

            auto decoded = WavReader::read_bytes(*encoded);
            CF_REQUIRE(decoded.has_value());
            CF_REQUIRE(decoded->channels() == ch);
            CF_REQUIRE(decoded->sample_rate() == info.sample_rate);
            CF_REQUIRE(decoded->sample_format() == fmt);
            CF_REQUIRE(decoded->sample_count() == samples.size());

            const double tol = (fmt == SampleFormat::Pcm16) ? 1.0e-4 : 1.0e-6;
            for (std::size_t i = 0; i < samples.size(); ++i) {
                CF_REQUIRE_NEAR(decoded->samples()[i], samples[i], tol);
            }
        }
    }

    // Odd-sized JUNK chunk + reordered chunks (data before fmt)
    {
        std::vector<std::uint8_t> raw;
        push_tag(raw, "RIFF");
        push_u32(raw, 0);
        push_tag(raw, "WAVE");

        push_tag(raw, "JUNK");
        push_u32(raw, 3);
        raw.push_back(0xAA);
        raw.push_back(0xBB);
        raw.push_back(0xCC);
        raw.push_back(0x00);

        push_tag(raw, "data");
        push_u32(raw, 4);
        push_u16(raw, static_cast<std::uint16_t>(static_cast<std::int16_t>(16384)));
        push_u16(raw, static_cast<std::uint16_t>(static_cast<std::int16_t>(-16384)));

        push_tag(raw, "fmt ");
        push_u32(raw, 16);
        push_u16(raw, 1);
        push_u16(raw, 1);
        push_u32(raw, 22050);
        push_u32(raw, 44100);
        push_u16(raw, 2);
        push_u16(raw, 16);

        const auto riff_size = static_cast<std::uint32_t>(raw.size() - 8);
        std::memcpy(raw.data() + 4, &riff_size, 4);

        auto parsed = WavReader::read_bytes(raw);
        CF_REQUIRE(parsed.has_value());
        CF_REQUIRE(parsed->sample_count() == 2);
        CF_REQUIRE_NEAR(parsed->samples()[0], 0.5, 1e-4);
        CF_REQUIRE_NEAR(parsed->samples()[1], -0.5, 1e-4);
    }

    // Reject NaN in IEEE Float32 WAV
    {
        std::vector<std::uint8_t> raw;
        push_tag(raw, "RIFF");
        push_u32(raw, 36 + 4);
        push_tag(raw, "WAVE");
        push_tag(raw, "fmt ");
        push_u32(raw, 16);
        push_u16(raw, 3);
        push_u16(raw, 1);
        push_u32(raw, 48000);
        push_u32(raw, 192000);
        push_u16(raw, 4);
        push_u16(raw, 32);
        push_tag(raw, "data");
        push_u32(raw, 4);
        const float nan_val = std::numeric_limits<float>::quiet_NaN();
        std::uint32_t nan_bits = 0;
        std::memcpy(&nan_bits, &nan_val, 4);
        push_u32(raw, nan_bits);

        auto parsed = WavReader::read_bytes(raw);
        CF_REQUIRE(!parsed.has_value());
        CF_REQUIRE(parsed.error().code == clipforge::ErrorCode::InvalidSampleValue);
    }

    // Reject truncated files
    {
        std::vector<std::uint8_t> truncated = {'R', 'I', 'F', 'F', 100, 0, 0, 0, 'W', 'A', 'V', 'E'};
        auto res = WavReader::read_bytes(truncated);
        CF_REQUIRE(!res.has_value());
        CF_REQUIRE(res.error().code == clipforge::ErrorCode::TruncatedFile);
    }
}

} // namespace clipforge::tests

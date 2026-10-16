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

// Minimal valid PCM16 mono 22050 Hz file; callers patch bytes for negative cases.
std::vector<std::uint8_t> make_pcm16_mono(const std::vector<std::int16_t>& samples) {
    std::vector<std::uint8_t> raw;
    push_tag(raw, "RIFF");
    push_u32(raw, 0); // patched below
    push_tag(raw, "WAVE");
    push_tag(raw, "fmt ");
    push_u32(raw, 16);
    push_u16(raw, 1);      // PCM
    push_u16(raw, 1);      // mono
    push_u32(raw, 22050);
    push_u32(raw, 44100);
    push_u16(raw, 2);
    push_u16(raw, 16);
    push_tag(raw, "data");
    push_u32(raw, static_cast<std::uint32_t>(samples.size() * 2));
    for (std::int16_t s : samples) {
        push_u16(raw, static_cast<std::uint16_t>(s));
    }
    const auto riff_size = static_cast<std::uint32_t>(raw.size() - 8);
    std::memcpy(raw.data() + 4, &riff_size, 4);
    return raw;
}

void fix_riff_size(std::vector<std::uint8_t>& raw) {
    const auto riff_size = static_cast<std::uint32_t>(raw.size() - 8);
    std::memcpy(raw.data() + 4, &riff_size, 4);
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

    // Duplicate 'fmt ' chunk
    {
        auto raw = make_pcm16_mono({1000, -1000});
        // Insert a second identical fmt chunk right before 'data' (offset 36).
        std::vector<std::uint8_t> dup;
        dup.insert(dup.end(), raw.begin(), raw.begin() + 36);
        dup.insert(dup.end(), raw.begin() + 12, raw.begin() + 36);
        dup.insert(dup.end(), raw.begin() + 36, raw.end());
        fix_riff_size(dup);
        auto res = WavReader::read_bytes(dup);
        CF_REQUIRE(!res.has_value());
        CF_REQUIRE(res.error().code == clipforge::ErrorCode::CorruptedChunk);
    }

    // Duplicate 'data' chunk
    {
        auto raw = make_pcm16_mono({1000, -1000});
        raw.insert(raw.end(), raw.begin() + 36, raw.end());
        fix_riff_size(raw);
        auto res = WavReader::read_bytes(raw);
        CF_REQUIRE(!res.has_value());
        CF_REQUIRE(res.error().code == clipforge::ErrorCode::CorruptedChunk);
    }

    // Inconsistent blockAlign (3 instead of 2)
    {
        auto raw = make_pcm16_mono({1000, -1000});
        raw[32] = 3;
        raw[33] = 0;
        auto res = WavReader::read_bytes(raw);
        CF_REQUIRE(!res.has_value());
        CF_REQUIRE(res.error().code == clipforge::ErrorCode::CorruptedChunk);
    }

    // Unsupported channel count (3, otherwise consistent)
    {
        auto raw = make_pcm16_mono({1000, -1000, 500, -500, 250, -250});
        raw[22] = 3; raw[23] = 0;             // channels
        raw[32] = 6; raw[33] = 0;             // blockAlign = 3 * 2
        raw[28] = 0xCC; raw[29] = 0x04; raw[30] = 0x02; raw[31] = 0x00; // byteRate 132300
        auto res = WavReader::read_bytes(raw);
        CF_REQUIRE(!res.has_value());
        CF_REQUIRE(res.error().code == clipforge::ErrorCode::UnsupportedWavFormat);
    }

    // Unsupported 8-bit PCM (otherwise consistent)
    {
        auto raw = make_pcm16_mono({1000, -1000});
        raw[34] = 8; raw[35] = 0;             // bitsPerSample
        raw[32] = 1; raw[33] = 0;             // blockAlign
        raw[28] = 0x22; raw[29] = 0x56; raw[30] = 0x00; raw[31] = 0x00; // byteRate 22050
        auto res = WavReader::read_bytes(raw);
        CF_REQUIRE(!res.has_value());
        CF_REQUIRE(res.error().code == clipforge::ErrorCode::UnsupportedWavFormat);
    }

    // Empty 'data' chunk
    {
        auto raw = make_pcm16_mono({1000, -1000});
        raw.erase(raw.begin() + 44, raw.end());
        raw[40] = 0; raw[41] = 0; raw[42] = 0; raw[43] = 0;
        fix_riff_size(raw);
        auto res = WavReader::read_bytes(raw);
        CF_REQUIRE(!res.has_value());
        CF_REQUIRE(res.error().code == clipforge::ErrorCode::EmptyAudio);
    }

    // Missing 'fmt ' chunk
    {
        std::vector<std::uint8_t> raw;
        push_tag(raw, "RIFF");
        push_u32(raw, 0);
        push_tag(raw, "WAVE");
        push_tag(raw, "data");
        push_u32(raw, 4);
        push_u16(raw, 1000);
        push_u16(raw, 2000);
        fix_riff_size(raw);
        auto res = WavReader::read_bytes(raw);
        CF_REQUIRE(!res.has_value());
        CF_REQUIRE(res.error().code == clipforge::ErrorCode::InvalidWavHeader);
    }

    // Missing 'data' chunk
    {
        auto raw = make_pcm16_mono({1000, -1000});
        raw.erase(raw.begin() + 36, raw.end());
        fix_riff_size(raw);
        auto res = WavReader::read_bytes(raw);
        CF_REQUIRE(!res.has_value());
        CF_REQUIRE(res.error().code == clipforge::ErrorCode::EmptyAudio);
    }

    // 'data' size not a multiple of blockAlign (3 payload bytes + pad)
    {
        auto raw = make_pcm16_mono({1000, -1000});
        raw.erase(raw.begin() + 44, raw.end());
        raw.push_back(0x11);
        raw.push_back(0x22);
        raw.push_back(0x33);
        raw.push_back(0x00); // word-alignment pad
        raw[40] = 3; raw[41] = 0; raw[42] = 0; raw[43] = 0;
        fix_riff_size(raw);
        auto res = WavReader::read_bytes(raw);
        CF_REQUIRE(!res.has_value());
        CF_REQUIRE(res.error().code == clipforge::ErrorCode::CorruptedChunk);
    }

    // Odd-sized trailing chunk without its pad byte
    {
        auto raw = make_pcm16_mono({1000, -1000});
        push_tag(raw, "JUNK");
        push_u32(raw, 3);
        raw.push_back(0xAA);
        raw.push_back(0xBB);
        raw.push_back(0xCC);
        // NOTE: required pad byte deliberately omitted.
        fix_riff_size(raw);
        auto res = WavReader::read_bytes(raw);
        CF_REQUIRE(!res.has_value());
        CF_REQUIRE(res.error().code == clipforge::ErrorCode::TruncatedFile);
    }

    // Valid WAVE_FORMAT_EXTENSIBLE (PCM16) is accepted
    {
        std::vector<std::uint8_t> raw;
        push_tag(raw, "RIFF");
        push_u32(raw, 0);
        push_tag(raw, "WAVE");
        push_tag(raw, "fmt ");
        push_u32(raw, 40);
        push_u16(raw, 0xFFFE); // WAVE_FORMAT_EXTENSIBLE
        push_u16(raw, 1);
        push_u32(raw, 48000);
        push_u32(raw, 96000);
        push_u16(raw, 2);
        push_u16(raw, 16);
        push_u16(raw, 22);    // cbSize
        push_u16(raw, 16);    // validBitsPerSample
        push_u32(raw, 4);     // channel mask
        push_u16(raw, 1);     // PCM subformat GUID
        push_u16(raw, 0);
        const std::uint8_t tail[12] = {
            0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
        };
        raw.insert(raw.end(), tail, tail + 12);
        push_tag(raw, "data");
        push_u32(raw, 4);
        push_u16(raw, 16384);
        push_u16(raw, static_cast<std::uint16_t>(static_cast<std::int16_t>(-16384)));
        fix_riff_size(raw);

        auto parsed = WavReader::read_bytes(raw);
        CF_REQUIRE(parsed.has_value());
        CF_REQUIRE(parsed->sample_format() == SampleFormat::Pcm16);
        CF_REQUIRE(parsed->channels() == 1);
        CF_REQUIRE(parsed->sample_rate() == 48000);
        CF_REQUIRE(parsed->sample_count() == 2);
        CF_REQUIRE_NEAR(parsed->samples()[0], 0.5, 1e-4);
        CF_REQUIRE_NEAR(parsed->samples()[1], -0.5, 1e-4);
    }

    // WAVE_FORMAT_EXTENSIBLE with wrong subformat GUID
    {
        std::vector<std::uint8_t> raw;
        push_tag(raw, "RIFF");
        push_u32(raw, 0);
        push_tag(raw, "WAVE");
        push_tag(raw, "fmt ");
        push_u32(raw, 40);
        push_u16(raw, 0xFFFE);
        push_u16(raw, 1);
        push_u32(raw, 48000);
        push_u32(raw, 96000);
        push_u16(raw, 2);
        push_u16(raw, 16);
        push_u16(raw, 22);
        push_u16(raw, 16);
        push_u32(raw, 4);
        push_u16(raw, 1);
        push_u16(raw, 0);
        const std::uint8_t bad_tail[12] = {
            0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x72
        };
        raw.insert(raw.end(), bad_tail, bad_tail + 12);
        push_tag(raw, "data");
        push_u32(raw, 4);
        push_u16(raw, 16384);
        push_u16(raw, 16384);
        fix_riff_size(raw);

        auto res = WavReader::read_bytes(raw);
        CF_REQUIRE(!res.has_value());
        CF_REQUIRE(res.error().code == clipforge::ErrorCode::UnsupportedWavFormat);
    }

    // Reject +Infinity in IEEE Float32 WAV
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
        const float inf_val = std::numeric_limits<float>::infinity();
        std::uint32_t inf_bits = 0;
        std::memcpy(&inf_bits, &inf_val, 4);
        push_u32(raw, inf_bits);

        auto parsed = WavReader::read_bytes(raw);
        CF_REQUIRE(!parsed.has_value());
        CF_REQUIRE(parsed.error().code == clipforge::ErrorCode::InvalidSampleValue);
    }
}

} // namespace clipforge::tests

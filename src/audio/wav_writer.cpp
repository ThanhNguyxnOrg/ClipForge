#include "clipforge/audio/wav_writer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

namespace clipforge::audio {

namespace {

void append_u16_le(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
}

void append_u32_le(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
}

void append_fourcc(std::vector<std::uint8_t>& out, const char (&tag)[5]) {
    out.insert(out.end(), tag, tag + 4);
}

[[nodiscard]] bool paths_refer_to_same_target(
    const std::filesystem::path& a,
    const std::filesystem::path& b
) noexcept {
    std::error_code ec1;
    std::error_code ec2;
    if (std::filesystem::exists(a, ec1) && std::filesystem::exists(b, ec2)) {
        std::error_code eq_ec;
        if (std::filesystem::equivalent(a, b, eq_ec) && !eq_ec) {
            return true;
        }
    }
    const auto norm_a = std::filesystem::weakly_canonical(a, ec1);
    const auto norm_b = std::filesystem::weakly_canonical(b, ec2);
    if (!ec1 && !ec2) {
        return norm_a == norm_b;
    }
    return a.lexically_normal() == b.lexically_normal();
}

} // namespace

Result<std::vector<std::uint8_t>> WavWriter::encode_bytes(
    const AudioBuffer& buffer,
    std::optional<SampleFormat> override_format
) {
    const std::uint16_t channels = buffer.channels();
    const std::uint32_t sample_rate = buffer.sample_rate();
    if (channels < 1 || channels > 2) {
        return make_error(ErrorCode::InvalidParameter, "Invalid channel count for WAV output.");
    }
    if (sample_rate == 0) {
        return make_error(ErrorCode::InvalidParameter, "Sample rate cannot be zero.");
    }

    const SampleFormat target_fmt = override_format.value_or(buffer.sample_format());
    const std::uint16_t bps = bits_per_sample(target_fmt);
    const std::uint16_t bytes_per_sample = static_cast<std::uint16_t>(bps / 8u);
    const std::uint16_t block_align = static_cast<std::uint16_t>(channels * bytes_per_sample);
    const std::uint32_t byte_rate = sample_rate * static_cast<std::uint32_t>(block_align);

    const auto samples = buffer.samples();
    if (samples.empty() || (samples.size() % channels != 0)) {
        return make_error(
            ErrorCode::EmptyAudio,
            "Cannot write WAV with empty or non-frame-aligned sample buffer."
        );
    }

    for (std::size_t i = 0; i < samples.size(); ++i) {
        if (!std::isfinite(samples[i])) {
            return make_error(
                ErrorCode::InvalidSampleValue,
                "Refusing to encode non-finite float sample at index " + std::to_string(i) + "."
            );
        }
    }

    const std::uint64_t raw_data_bytes =
        static_cast<std::uint64_t>(samples.size()) * bytes_per_sample;
    const std::uint64_t pad_byte = (raw_data_bytes & 1ULL) != 0ULL ? 1ULL : 0ULL;
    const std::uint64_t riff_payload_bytes = 4ULL + 24ULL + 8ULL + raw_data_bytes + pad_byte;

    if (riff_payload_bytes > std::numeric_limits<std::uint32_t>::max()) {
        return make_error(
            ErrorCode::InvalidParameter,
            "Encoded WAV exceeds 4 GiB standard RIFF size limit."
        );
    }

    std::vector<std::uint8_t> out;
    out.reserve(static_cast<std::size_t>(riff_payload_bytes + 8ULL));

    append_fourcc(out, "RIFF");
    append_u32_le(out, static_cast<std::uint32_t>(riff_payload_bytes));
    append_fourcc(out, "WAVE");

    append_fourcc(out, "fmt ");
    append_u32_le(out, 16u);
    const std::uint16_t format_tag = (target_fmt == SampleFormat::Float32) ? 0x0003u : 0x0001u;
    append_u16_le(out, format_tag);
    append_u16_le(out, channels);
    append_u32_le(out, sample_rate);
    append_u32_le(out, byte_rate);
    append_u16_le(out, block_align);
    append_u16_le(out, bps);

    append_fourcc(out, "data");
    append_u32_le(out, static_cast<std::uint32_t>(raw_data_bytes));

    switch (target_fmt) {
        case SampleFormat::Pcm16: {
            for (float s : samples) {
                const float scaled = std::clamp(s * 32768.0f, -32768.0f, 32767.0f);
                const auto q = static_cast<std::int16_t>(std::lrintf(scaled));
                append_u16_le(out, static_cast<std::uint16_t>(q));
            }
            break;
        }
        case SampleFormat::Pcm24: {
            for (float s : samples) {
                const float scaled = std::clamp(s * 8388608.0f, -8388608.0f, 8388607.0f);
                const auto q = static_cast<std::int32_t>(std::lrintf(scaled));
                out.push_back(static_cast<std::uint8_t>(q & 0xFF));
                out.push_back(static_cast<std::uint8_t>((q >> 8) & 0xFF));
                out.push_back(static_cast<std::uint8_t>((q >> 16) & 0xFF));
            }
            break;
        }
        case SampleFormat::Float32: {
            for (float s : samples) {
                std::uint32_t bits = 0;
                std::memcpy(&bits, &s, sizeof(float));
                append_u32_le(out, bits);
            }
            break;
        }
    }

    if (pad_byte != 0ULL) {
        out.push_back(0u);
    }

    return out;
}

Status WavWriter::write_file(
    const std::filesystem::path& output_path,
    const AudioBuffer& buffer,
    const WavWriteOptions& options
) {
    if (output_path.empty()) {
        return make_error(ErrorCode::InvalidParameter, "Output path cannot be empty.");
    }

    if (options.source_path_guard.has_value()) {
        if (paths_refer_to_same_target(*options.source_path_guard, output_path)) {
            return make_error(
                ErrorCode::PathCollision,
                "Refusing to overwrite input file: " + output_path.string()
            );
        }
    }

    std::error_code ec;
    if (std::filesystem::exists(output_path, ec) && !options.overwrite_existing) {
        return make_error(
            ErrorCode::TargetExists,
            "Destination file already exists (overwrite disabled): " + output_path.string()
        );
    }

    auto encoded = encode_bytes(buffer, options.override_format);
    if (!encoded.has_value()) {
        return std::unexpected(encoded.error());
    }

    const auto parent_dir = output_path.parent_path();
    if (!parent_dir.empty() && !std::filesystem::exists(parent_dir, ec)) {
        if (!std::filesystem::create_directories(parent_dir, ec) && ec) {
            return make_error(
                ErrorCode::IoError,
                "Failed to create output directory '" + parent_dir.string() + "': " + ec.message()
            );
        }
    }

    std::filesystem::path temp_path = output_path;
    temp_path += ".clipforge.tmp";

    {
        std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            return make_error(
                ErrorCode::IoError,
                "Failed to open temporary output file: " + temp_path.string()
            );
        }
        out.write(
            reinterpret_cast<const char*>(encoded->data()),
            static_cast<std::streamsize>(encoded->size())
        );
        out.flush();
        if (!out) {
            out.close();
            std::filesystem::remove(temp_path, ec);
            return make_error(
                ErrorCode::IoError,
                "Failed while writing audio data to temporary file: " + temp_path.string()
            );
        }
    }

    if (options.overwrite_existing && std::filesystem::exists(output_path, ec)) {
        std::filesystem::remove(output_path, ec);
    }

    std::filesystem::rename(temp_path, output_path, ec);
    if (ec) {
        std::error_code rm_ec;
        std::filesystem::remove(temp_path, rm_ec);
        return make_error(
            ErrorCode::IoError,
            "Failed to finalize output file '" + output_path.string() + "': " + ec.message()
        );
    }

    return {};
}

} // namespace clipforge::audio

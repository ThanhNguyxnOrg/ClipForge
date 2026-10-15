#include "clipforge/audio/wav_reader.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace clipforge::audio {

namespace {

constexpr std::uint16_t kWaveFormatPcm        = 0x0001;
constexpr std::uint16_t kWaveFormatIeeeFloat  = 0x0003;
constexpr std::uint16_t kWaveFormatExtensible = 0xFFFE;

// Maximum single WAV allocation limit (1 GiB raw file size guard)
constexpr std::uintmax_t kMaxWavFileBytes = 1024ULL * 1024ULL * 1024ULL;
constexpr std::uint32_t kMinSampleRate = 1;
constexpr std::uint32_t kMaxSampleRate = 768000;

[[nodiscard]] std::uint16_t read_u16_le(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(p[0]) |
        (static_cast<std::uint16_t>(p[1]) << 8)
    );
}

[[nodiscard]] std::uint32_t read_u32_le(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

[[nodiscard]] bool fourcc_eq(const std::uint8_t* p, const char (&tag)[5]) noexcept {
    return std::memcmp(p, tag, 4) == 0;
}

} // namespace

Result<AudioBuffer> WavReader::read_file(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return make_error(ErrorCode::IoError, "Input file does not exist: " + path.string());
    }
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        return make_error(ErrorCode::IoError, "Input path is not a regular file: " + path.string());
    }

    const auto file_size = std::filesystem::file_size(path, ec);
    if (ec) {
        return make_error(ErrorCode::IoError, "Failed to query file size: " + path.string());
    }
    if (file_size < 12) {
        return make_error(ErrorCode::TruncatedFile, "File is smaller than minimal 12-byte RIFF header.");
    }
    if (file_size > kMaxWavFileBytes) {
        return make_error(ErrorCode::InvalidParameter, "WAV file exceeds safety allocation cap (1 GiB).");
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return make_error(ErrorCode::IoError, "Unable to open input file for reading: " + path.string());
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file_size));
    if (!in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        return make_error(ErrorCode::IoError, "Failed to read complete WAV file: " + path.string());
    }

    return read_bytes(bytes);
}

Result<AudioBuffer> WavReader::read_bytes(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 12) {
        return make_error(ErrorCode::TruncatedFile, "Input buffer is smaller than 12-byte RIFF header.");
    }

    if (!fourcc_eq(bytes.data(), "RIFF")) {
        return make_error(ErrorCode::InvalidWavHeader, "Missing 'RIFF' container signature.");
    }

    const std::uint32_t riff_chunk_size = read_u32_le(bytes.data() + 4);
    if (!fourcc_eq(bytes.data() + 8, "WAVE")) {
        return make_error(ErrorCode::InvalidWavHeader, "RIFF container is not of type 'WAVE'.");
    }

    const std::size_t declared_total = static_cast<std::size_t>(riff_chunk_size) + 8u;
    if (declared_total > bytes.size()) {
        return make_error(
            ErrorCode::TruncatedFile,
            "RIFF chunk size (" + std::to_string(riff_chunk_size) +
            " bytes) exceeds actual file size (" + std::to_string(bytes.size()) + " bytes)."
        );
    }

    const std::size_t container_end = declared_total;
    std::size_t offset = 12;

    bool found_fmt = false;
    bool found_data = false;
    WavFormatInfo fmt_info{};
    const std::uint8_t* data_ptr = nullptr;
    std::uint32_t data_size = 0;

    while (offset < container_end) {
        if (container_end - offset < 8) {
            return make_error(ErrorCode::TruncatedFile, "Truncated RIFF subchunk header.");
        }

        const std::uint8_t* chunk_hdr = bytes.data() + offset;
        const std::uint32_t chunk_size = read_u32_le(chunk_hdr + 4);
        offset += 8;

        if (chunk_size > container_end - offset) {
            return make_error(
                ErrorCode::TruncatedFile,
                "Subchunk size exceeds remaining RIFF container bounds."
            );
        }

        const std::uint8_t* chunk_payload = bytes.data() + offset;

        if (fourcc_eq(chunk_hdr, "fmt ")) {
            if (found_fmt) {
                return make_error(ErrorCode::CorruptedChunk, "Duplicate 'fmt ' chunk encountered.");
            }
            if (chunk_size < 16) {
                return make_error(ErrorCode::CorruptedChunk, "'fmt ' chunk is smaller than 16 bytes.");
            }

            std::uint16_t format_tag = read_u16_le(chunk_payload + 0);
            const std::uint16_t channels = read_u16_le(chunk_payload + 2);
            const std::uint32_t sample_rate = read_u32_le(chunk_payload + 4);
            const std::uint32_t byte_rate = read_u32_le(chunk_payload + 8);
            const std::uint16_t block_align = read_u16_le(chunk_payload + 12);
            const std::uint16_t bits_per_sample = read_u16_le(chunk_payload + 14);

            if (format_tag == kWaveFormatExtensible) {
                if (chunk_size < 40) {
                    return make_error(
                        ErrorCode::CorruptedChunk,
                        "WAVE_FORMAT_EXTENSIBLE 'fmt ' chunk requires at least 40 bytes."
                    );
                }
                const std::uint16_t cb_size = read_u16_le(chunk_payload + 16);
                const std::uint16_t valid_bits = read_u16_le(chunk_payload + 18);
                if (cb_size < 22 || valid_bits != bits_per_sample) {
                    return make_error(
                        ErrorCode::UnsupportedWavFormat,
                        "Unsupported WAVE_FORMAT_EXTENSIBLE validBitsPerSample configuration."
                    );
                }
                static constexpr std::uint8_t kGuidTail[12] = {
                    0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71
                };
                if (std::memcmp(chunk_payload + 28, kGuidTail, 12) != 0) {
                    return make_error(
                        ErrorCode::UnsupportedWavFormat,
                        "Unsupported WAVE_FORMAT_EXTENSIBLE subformat GUID."
                    );
                }
                format_tag = read_u16_le(chunk_payload + 24);
            }

            if (channels < 1 || channels > 2) {
                return make_error(
                    ErrorCode::UnsupportedWavFormat,
                    "Unsupported channel count (" + std::to_string(channels) +
                    "). Only mono (1) and stereo (2) are supported in MVP."
                );
            }

            if (sample_rate < kMinSampleRate || sample_rate > kMaxSampleRate) {
                return make_error(
                    ErrorCode::InvalidWavHeader,
                    "Invalid sample rate: " + std::to_string(sample_rate) + " Hz."
                );
            }

            SampleFormat resolved_format{};
            if (format_tag == kWaveFormatPcm) {
                if (bits_per_sample == 16) {
                    resolved_format = SampleFormat::Pcm16;
                } else if (bits_per_sample == 24) {
                    resolved_format = SampleFormat::Pcm24;
                } else {
                    return make_error(
                        ErrorCode::UnsupportedWavFormat,
                        "Unsupported PCM bit depth (" + std::to_string(bits_per_sample) +
                        "-bit). Supported PCM bit depths: 16, 24."
                    );
                }
            } else if (format_tag == kWaveFormatIeeeFloat) {
                if (bits_per_sample == 32) {
                    resolved_format = SampleFormat::Float32;
                } else {
                    return make_error(
                        ErrorCode::UnsupportedWavFormat,
                        "Unsupported IEEE float bit depth (" + std::to_string(bits_per_sample) +
                        "-bit). Only 32-bit float is supported."
                    );
                }
            } else {
                return make_error(
                    ErrorCode::UnsupportedWavFormat,
                    "Unsupported WAVE format tag: " + std::to_string(format_tag) + "."
                );
            }

            const std::uint16_t expected_block_align =
                static_cast<std::uint16_t>(channels * (bits_per_sample / 8u));
            const std::uint32_t expected_byte_rate =
                sample_rate * static_cast<std::uint32_t>(expected_block_align);

            if (block_align != expected_block_align) {
                return make_error(
                    ErrorCode::CorruptedChunk,
                    "Inconsistent blockAlign in 'fmt ' chunk (got " +
                    std::to_string(block_align) + ", expected " +
                    std::to_string(expected_block_align) + ")."
                );
            }
            if (byte_rate != expected_byte_rate) {
                return make_error(
                    ErrorCode::CorruptedChunk,
                    "Inconsistent byteRate in 'fmt ' chunk (got " +
                    std::to_string(byte_rate) + ", expected " +
                    std::to_string(expected_byte_rate) + ")."
                );
            }

            fmt_info.sample_format = resolved_format;
            fmt_info.num_channels = channels;
            fmt_info.sample_rate = sample_rate;
            fmt_info.bits_per_sample = bits_per_sample;
            fmt_info.block_align = block_align;
            fmt_info.byte_rate = byte_rate;
            found_fmt = true;
        } else if (fourcc_eq(chunk_hdr, "data")) {
            if (found_data) {
                return make_error(ErrorCode::CorruptedChunk, "Duplicate 'data' chunk encountered.");
            }
            data_ptr = chunk_payload;
            data_size = chunk_size;
            found_data = true;
        }

        offset += chunk_size;
        if ((chunk_size & 1u) != 0u) {
            if (offset >= container_end) {
                return make_error(
                    ErrorCode::TruncatedFile,
                    "Missing required RIFF padding byte after odd-sized chunk."
                );
            }
            offset += 1;
        }
    }

    if (!found_fmt) {
        return make_error(ErrorCode::InvalidWavHeader, "Missing mandatory 'fmt ' chunk.");
    }
    if (!found_data || data_ptr == nullptr) {
        return make_error(ErrorCode::EmptyAudio, "Missing mandatory 'data' chunk.");
    }
    if (data_size == 0) {
        return make_error(ErrorCode::EmptyAudio, "WAV 'data' chunk is empty (0 bytes).");
    }
    if (data_size % fmt_info.block_align != 0) {
        return make_error(
            ErrorCode::CorruptedChunk,
            "WAV 'data' chunk size (" + std::to_string(data_size) +
            ") is not a multiple of blockAlign (" + std::to_string(fmt_info.block_align) + ")."
        );
    }

    const std::size_t num_frames = data_size / fmt_info.block_align;
    if (num_frames > std::numeric_limits<std::size_t>::max() / fmt_info.num_channels) {
        return make_error(ErrorCode::CorruptedChunk, "Sample count calculation overflowed.");
    }
    const std::size_t total_samples = num_frames * fmt_info.num_channels;

    std::vector<float> samples(total_samples);

    switch (fmt_info.sample_format) {
        case SampleFormat::Pcm16: {
            constexpr float kInv32768 = 1.0f / 32768.0f;
            for (std::size_t i = 0; i < total_samples; ++i) {
                const std::uint8_t* p = data_ptr + i * 2;
                const auto raw = static_cast<std::int16_t>(read_u16_le(p));
                samples[i] = static_cast<float>(raw) * kInv32768;
            }
            break;
        }
        case SampleFormat::Pcm24: {
            constexpr float kInv8388608 = 1.0f / 8388608.0f;
            for (std::size_t i = 0; i < total_samples; ++i) {
                const std::uint8_t* p = data_ptr + i * 3;
                const std::int32_t raw =
                    static_cast<std::int32_t>(p[0]) |
                    (static_cast<std::int32_t>(p[1]) << 8) |
                    (static_cast<std::int32_t>(static_cast<std::int8_t>(p[2])) << 16);
                samples[i] = static_cast<float>(raw) * kInv8388608;
            }
            break;
        }
        case SampleFormat::Float32: {
            for (std::size_t i = 0; i < total_samples; ++i) {
                const std::uint32_t bits = read_u32_le(data_ptr + i * 4);
                float val = 0.0f;
                std::memcpy(&val, &bits, sizeof(float));
                if (!std::isfinite(val)) {
                    return make_error(
                        ErrorCode::InvalidSampleValue,
                        "Non-finite floating-point sample (NaN or Infinity) detected at index " +
                        std::to_string(i) + "."
                    );
                }
                samples[i] = val;
            }
            break;
        }
    }

    return AudioBuffer(fmt_info, std::move(samples));
}

} // namespace clipforge::audio

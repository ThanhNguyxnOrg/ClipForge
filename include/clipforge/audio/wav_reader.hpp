#pragma once

#include "clipforge/audio/audio_buffer.hpp"
#include "clipforge/core/result.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace clipforge::audio {

class WavReader {
public:
    // Reads and validates a WAV file from disk into a normalized float32 AudioBuffer.
    [[nodiscard]] static Result<AudioBuffer> read_file(const std::filesystem::path& path);

    // Parses and validates a WAV image from an in-memory byte buffer (also used by tests).
    [[nodiscard]] static Result<AudioBuffer> read_bytes(std::span<const std::uint8_t> bytes);
};

} // namespace clipforge::audio

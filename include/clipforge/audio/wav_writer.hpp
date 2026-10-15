#pragma once

#include "clipforge/audio/audio_buffer.hpp"
#include "clipforge/core/result.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace clipforge::audio {

struct WavWriteOptions {
    bool overwrite_existing = false;
    std::optional<std::filesystem::path> source_path_guard = std::nullopt;
    std::optional<SampleFormat> override_format = std::nullopt;
};

class WavWriter {
public:
    // Safely writes an AudioBuffer to disk using a temporary sibling file + atomic rename.
    // Refuses to write to source_path_guard or overwrite existing files unless enabled.
    [[nodiscard]] static Status write_file(
        const std::filesystem::path& output_path,
        const AudioBuffer& buffer,
        const WavWriteOptions& options = {}
    );

    // Encodes an AudioBuffer to a valid RIFF/WAVE byte vector in memory.
    [[nodiscard]] static Result<std::vector<std::uint8_t>> encode_bytes(
        const AudioBuffer& buffer,
        std::optional<SampleFormat> override_format = std::nullopt
    );
};

} // namespace clipforge::audio

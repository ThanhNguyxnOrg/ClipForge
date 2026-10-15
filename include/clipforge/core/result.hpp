#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace clipforge {

enum class ErrorCode {
    IoError,
    InvalidWavHeader,
    UnsupportedWavFormat,
    CorruptedChunk,
    TruncatedFile,
    EmptyAudio,
    InvalidSampleValue,
    InvalidParameter,
    PathCollision,
    TargetExists,
    Cancelled
};

[[nodiscard]] constexpr std::string_view to_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::IoError:              return "IoError";
        case ErrorCode::InvalidWavHeader:     return "InvalidWavHeader";
        case ErrorCode::UnsupportedWavFormat: return "UnsupportedWavFormat";
        case ErrorCode::CorruptedChunk:       return "CorruptedChunk";
        case ErrorCode::TruncatedFile:        return "TruncatedFile";
        case ErrorCode::EmptyAudio:           return "EmptyAudio";
        case ErrorCode::InvalidSampleValue:   return "InvalidSampleValue";
        case ErrorCode::InvalidParameter:     return "InvalidParameter";
        case ErrorCode::PathCollision:        return "PathCollision";
        case ErrorCode::TargetExists:         return "TargetExists";
        case ErrorCode::Cancelled:            return "Cancelled";
    }
    return "UnknownError";
}

struct Error {
    ErrorCode code;
    std::string message;

    Error(ErrorCode c, std::string msg)
        : code(c), message(std::move(msg)) {}
};

template <typename T>
using Result = std::expected<T, Error>;

using Status = std::expected<void, Error>;

[[nodiscard]] inline std::unexpected<Error> make_error(ErrorCode code, std::string message) {
    return std::unexpected<Error>(Error{code, std::move(message)});
}

} // namespace clipforge

// STALKER-MP — Unified error reporting (Sprint-001, §7.12)
//
// Why this exists:
//   Every recoverable failure in the module is reported through a strongly
//   typed Error value instead of ad-hoc return codes or silent failure.
//
// Responsibilities:
//   - Define the project-wide ErrorCode enumeration.
//   - Provide the Error value type carried by Expected<T>.
//
// Ownership / lifetime:
//   Error is a value type. It owns its message string and has no external
//   dependencies.
//
// Interactions:
//   Consumed by Expected<T> (Expected.h) and logged through core::Logger.

#pragma once

#include <string>
#include <string_view>

namespace stalkermp::core
{
    enum class ErrorCode
    {
        None = 0,
        InvalidArgument,
        NotFound,
        AlreadyExists,
        NotInitialized,
        AlreadyInitialized,
        IoError,
        ParseError,
        VersionMismatch,
        DependencyMissing,
        Internal,
    };

    // Human-readable name of an ErrorCode. Never returns null.
    [[nodiscard]] constexpr std::string_view ErrorCodeName(const ErrorCode code) noexcept
    {
        switch (code)
        {
        case ErrorCode::None:               return "None";
        case ErrorCode::InvalidArgument:    return "InvalidArgument";
        case ErrorCode::NotFound:           return "NotFound";
        case ErrorCode::AlreadyExists:      return "AlreadyExists";
        case ErrorCode::NotInitialized:     return "NotInitialized";
        case ErrorCode::AlreadyInitialized: return "AlreadyInitialized";
        case ErrorCode::IoError:            return "IoError";
        case ErrorCode::ParseError:         return "ParseError";
        case ErrorCode::VersionMismatch:    return "VersionMismatch";
        case ErrorCode::DependencyMissing:  return "DependencyMissing";
        case ErrorCode::Internal:           return "Internal";
        }
        return "Unknown";
    }

    // Value type describing a recoverable failure.
    class Error
    {
    public:
        Error() = default;

        Error(const ErrorCode code, std::string message)
            : m_code(code)
            , m_message(std::move(message))
        {
        }

        [[nodiscard]] ErrorCode Code() const noexcept { return m_code; }
        [[nodiscard]] const std::string& Message() const noexcept { return m_message; }

        // "[ParseError] line 12: missing '='"
        [[nodiscard]] std::string Describe() const
        {
            std::string text;
            text.reserve(m_message.size() + 16);
            text += '[';
            text += ErrorCodeName(m_code);
            text += "] ";
            text += m_message;
            return text;
        }

    private:
        ErrorCode m_code = ErrorCode::None;
        std::string m_message;
    };

    [[nodiscard]] inline Error MakeError(const ErrorCode code, std::string message)
    {
        return Error(code, std::move(message));
    }
} // namespace stalkermp::core

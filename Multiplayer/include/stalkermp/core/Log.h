// STALKER-MP — Centralized logging (Sprint-001, §7.5)
//
// Why this exists:
//   Every subsystem reports through one logger so that diagnostics carry a
//   consistent format: timestamp, severity, subsystem category, message
//   (02_Engine.md §19.7).
//
// Responsibilities:
//   - Severity levels: Verbose, Debug, Info, Warning, Error.
//   - Subsystem categories (free-form, e.g. "Core", "Config").
//   - Timestamped records.
//   - Pluggable sinks: console and file provided in Sprint-001.
//
// Ownership / lifetime:
//   The Logger instance is owned by the Bootstrap runtime. It is created
//   during stalkermp::Initialize and destroyed during stalkermp::Shutdown.
//   Access goes through core::Log(), which is valid only between those two
//   calls (asserted).
//
// Threading:
//   Logger::Write is guarded by a mutex. Logging is an approved
//   limited-lock domain (09_Threading.md §15.3); it never participates in
//   gameplay state.

#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"

namespace stalkermp::core
{
    enum class LogLevel : std::uint8_t
    {
        Verbose = 0,
        Debug,
        Info,
        Warning,
        Error,
    };

    [[nodiscard]] constexpr std::string_view LogLevelName(const LogLevel level) noexcept
    {
        switch (level)
        {
        case LogLevel::Verbose: return "VERBOSE";
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error:   return "ERROR";
        }
        return "UNKNOWN";
    }

    struct LogRecord
    {
        std::chrono::system_clock::time_point timestamp;
        LogLevel level = LogLevel::Info;
        std::string_view category;
        std::string_view message;
    };

    // Output target for log records. Implementations must be thread-safe
    // only with respect to their own resources; the Logger serializes calls.
    class ILogSink
    {
    public:
        virtual ~ILogSink() = default;

        virtual void Write(const LogRecord& record) = 0;
        virtual void Flush() = 0;
    };

    class Logger
    {
    public:
        Logger() = default;

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        void AddSink(std::unique_ptr<ILogSink> sink);

        // Records below this level are discarded.
        void SetMinimumLevel(LogLevel level) noexcept;
        [[nodiscard]] LogLevel MinimumLevel() const noexcept;

        void Write(LogLevel level, std::string_view category, std::string_view message);

        void Verbose(std::string_view category, std::string_view message) { Write(LogLevel::Verbose, category, message); }
        void Debug(std::string_view category, std::string_view message) { Write(LogLevel::Debug, category, message); }
        void Info(std::string_view category, std::string_view message) { Write(LogLevel::Info, category, message); }
        void Warning(std::string_view category, std::string_view message) { Write(LogLevel::Warning, category, message); }
        void Error(std::string_view category, std::string_view message) { Write(LogLevel::Error, category, message); }

        void Flush();

    private:
        mutable std::mutex m_mutex;
        std::vector<std::unique_ptr<ILogSink>> m_sinks;
        LogLevel m_minimumLevel = LogLevel::Info;
    };

    // Formats a record as: "2026-07-03 14:02:11.123 [INFO   ] [Core] message".
    // Shared by the provided sinks and available to future ones.
    [[nodiscard]] std::string FormatLogRecord(const LogRecord& record);

    // Writes formatted records to stdout (stderr for Error).
    class ConsoleLogSink final : public ILogSink
    {
    public:
        void Write(const LogRecord& record) override;
        void Flush() override;
    };

    // Appends formatted records to a text file. The file is opened on
    // construction; construction fails by throwing std::runtime_error, which
    // Bootstrap converts to an Error (file sinks are created only there).
    class FileLogSink final : public ILogSink
    {
    public:
        static Expected<std::unique_ptr<FileLogSink>> Create(const std::string& filePath);
        ~FileLogSink() override;

        void Write(const LogRecord& record) override;
        void Flush() override;

    private:
        struct Impl;
        explicit FileLogSink(std::unique_ptr<Impl> impl);
        std::unique_ptr<Impl> m_impl;
    };

    // Access to the module logger. Valid only between stalkermp::Initialize
    // and stalkermp::Shutdown (asserted in debug builds).
    [[nodiscard]] Logger& Log() noexcept;

    // True while the module logger exists. Used by the assertion framework,
    // which must work before logging is available.
    [[nodiscard]] bool IsLogAvailable() noexcept;

    namespace detail
    {
        // Installs/clears the module logger. Called exclusively by the
        // Bootstrap pipeline (Bootstrap.cpp); never by user code.
        void SetModuleLogger(Logger* logger) noexcept;
    } // namespace detail
} // namespace stalkermp::core

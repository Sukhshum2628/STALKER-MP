#include "stalkermp/core/Log.h"

#include <cstdio>

#include "stalkermp/core/Assert.h"
#include "stalkermp/common/Time.h"

namespace stalkermp::core
{
    // ----------------------------------------------------------------- Logger

    void Logger::AddSink(std::unique_ptr<ILogSink> sink)
    {
        SMP_ASSERT_MSG(sink != nullptr, "Logger::AddSink: null sink");
        if (!sink)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sinks.push_back(std::move(sink));
    }

    void Logger::SetMinimumLevel(const LogLevel level) noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_minimumLevel = level;
    }

    LogLevel Logger::MinimumLevel() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_minimumLevel;
    }

    void Logger::Write(const LogLevel level, const std::string_view category, const std::string_view message)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (level < m_minimumLevel)
        {
            return;
        }

        const LogRecord record{std::chrono::system_clock::now(), level, category, message};
        for (const auto& sink : m_sinks)
        {
            sink->Write(record);
        }
    }

    void Logger::Flush()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& sink : m_sinks)
        {
            sink->Flush();
        }
    }

    // ------------------------------------------------------------- Formatting

    std::string FormatLogRecord(const LogRecord& record)
    {
        std::string line;
        line.reserve(48 + record.category.size() + record.message.size());
        line += common::FormatTimestamp(record.timestamp);
        line += " [";
        line += LogLevelName(record.level);
        line += "] [";
        line += record.category;
        line += "] ";
        line += record.message;
        return line;
    }

    // ---------------------------------------------------------- ConsoleLogSink

    void ConsoleLogSink::Write(const LogRecord& record)
    {
        std::FILE* stream = record.level >= LogLevel::Error ? stderr : stdout;
        const std::string line = FormatLogRecord(record);
        std::fputs(line.c_str(), stream);
        std::fputc('\n', stream);
    }

    void ConsoleLogSink::Flush()
    {
        std::fflush(stdout);
        std::fflush(stderr);
    }

    // ------------------------------------------------------------- FileLogSink

    struct FileLogSink::Impl
    {
        std::FILE* file = nullptr;
    };

    FileLogSink::FileLogSink(std::unique_ptr<Impl> impl)
        : m_impl(std::move(impl))
    {
    }

    FileLogSink::~FileLogSink()
    {
        if (m_impl && m_impl->file)
        {
            std::fclose(m_impl->file);
        }
    }

    Expected<std::unique_ptr<FileLogSink>> FileLogSink::Create(const std::string& filePath)
    {
        std::FILE* file = nullptr;
        if (fopen_s(&file, filePath.c_str(), "ab") != 0 || !file)
        {
            return MakeError(ErrorCode::IoError, "FileLogSink: cannot open log file '" + filePath + "'");
        }

        auto impl = std::make_unique<Impl>();
        impl->file = file;

        return std::unique_ptr<FileLogSink>(new FileLogSink(std::move(impl)));
    }

    void FileLogSink::Write(const LogRecord& record)
    {
        if (m_impl && m_impl->file)
        {
            std::string line = FormatLogRecord(record);
            line += '\n';
            std::fwrite(line.data(), 1, line.size(), m_impl->file);
        }
    }

    void FileLogSink::Flush()
    {
        if (m_impl && m_impl->file)
        {
            std::fflush(m_impl->file);
        }
    }

    // ----------------------------------------------------------- Module logger
    //
    // The module logger pointer is process-wide state set exclusively by
    // Bootstrap (see Bootstrap.cpp). This is the one deliberate global in
    // the module: logging must be reachable from any code without threading
    // a Logger& through every signature. It is documented, initialized and
    // torn down at exactly one place each.

    namespace detail
    {
        Logger* g_moduleLogger = nullptr;

        void SetModuleLogger(Logger* logger) noexcept
        {
            g_moduleLogger = logger;
        }
    } // namespace detail

    Logger& Log() noexcept
    {
        SMP_ASSERT_MSG(detail::g_moduleLogger != nullptr,
                       "core::Log() called outside Initialize/Shutdown");
        return *detail::g_moduleLogger;
    }

    bool IsLogAvailable() noexcept
    {
        return detail::g_moduleLogger != nullptr;
    }
} // namespace stalkermp::core

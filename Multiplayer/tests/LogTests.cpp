#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "stalkermp/core/Log.h"

using namespace stalkermp::core;

namespace
{
    // Captures records for inspection.
    class CaptureSink final : public ILogSink
    {
    public:
        struct Captured
        {
            LogLevel level;
            std::string category;
            std::string message;
        };

        explicit CaptureSink(std::vector<Captured>& target)
            : m_target(target)
        {
        }

        void Write(const LogRecord& record) override
        {
            m_target.push_back(Captured{record.level, std::string(record.category), std::string(record.message)});
        }

        void Flush() override
        {
            ++flushCount;
        }

        int flushCount = 0;

    private:
        std::vector<Captured>& m_target;
    };
} // namespace

TEST(Log, WritesToSinksWithCategoryAndLevel)
{
    std::vector<CaptureSink::Captured> captured;
    Logger logger;
    logger.AddSink(std::make_unique<CaptureSink>(captured));
    logger.SetMinimumLevel(LogLevel::Verbose);

    logger.Info("Core", "hello");
    logger.Error("Config", "bad file");

    ASSERT_EQ(captured.size(), static_cast<std::size_t>(2));
    EXPECT_TRUE(captured[0].level == LogLevel::Info);
    EXPECT_EQ(captured[0].category, std::string("Core"));
    EXPECT_EQ(captured[0].message, std::string("hello"));
    EXPECT_TRUE(captured[1].level == LogLevel::Error);
}

TEST(Log, FiltersBelowMinimumLevel)
{
    std::vector<CaptureSink::Captured> captured;
    Logger logger;
    logger.AddSink(std::make_unique<CaptureSink>(captured));
    logger.SetMinimumLevel(LogLevel::Warning);

    logger.Verbose("Core", "dropped");
    logger.Debug("Core", "dropped");
    logger.Info("Core", "dropped");
    logger.Warning("Core", "kept");
    logger.Error("Core", "kept");

    ASSERT_EQ(captured.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(captured[0].message, std::string("kept"));
    EXPECT_TRUE(captured[0].level == LogLevel::Warning);
}

TEST(Log, FormatIncludesLevelCategoryMessage)
{
    const LogRecord record{std::chrono::system_clock::now(), LogLevel::Warning, "Config", "watch out"};
    const std::string line = FormatLogRecord(record);

    EXPECT_TRUE(line.find("[WARNING]") != std::string::npos);
    EXPECT_TRUE(line.find("[Config]") != std::string::npos);
    EXPECT_TRUE(line.find("watch out") != std::string::npos);
    // Line starts with the timestamp: "YYYY-".
    ASSERT_TRUE(line.size() > 4);
    EXPECT_EQ(line[4], '-');
}

TEST(Log, ModuleLoggerAvailability)
{
    EXPECT_FALSE(IsLogAvailable());

    Logger logger;
    detail::SetModuleLogger(&logger);
    EXPECT_TRUE(IsLogAvailable());

    detail::SetModuleLogger(nullptr);
    EXPECT_FALSE(IsLogAvailable());
}

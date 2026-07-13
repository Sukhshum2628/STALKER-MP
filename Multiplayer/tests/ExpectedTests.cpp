#include <gtest/gtest.h>

#include <string>

#include "stalkermp/core/Expected.h"

using namespace stalkermp::core;

namespace
{
    Expected<int> ParsePositive(const int value)
    {
        if (value <= 0)
        {
            return MakeError(ErrorCode::InvalidArgument, "value must be positive");
        }
        return value;
    }
} // namespace

TEST(Expected, CarriesValue)
{
    const auto result = ParsePositive(5);
    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(result.Value(), 5);
    EXPECT_EQ(result.ValueOr(-1), 5);
}

TEST(Expected, CarriesError)
{
    const auto result = ParsePositive(-3);
    ASSERT_FALSE(result.HasValue());
    EXPECT_TRUE(result.GetError().Code() == ErrorCode::InvalidArgument);
    EXPECT_EQ(result.GetError().Message(), std::string("value must be positive"));
    EXPECT_EQ(result.ValueOr(-1), -1);
}

TEST(Expected, VoidSpecialization)
{
    const Expected<void> ok = Success();
    EXPECT_TRUE(ok.HasValue());

    const Expected<void> failed = MakeError(ErrorCode::IoError, "disk on fire");
    ASSERT_FALSE(failed.HasValue());
    EXPECT_TRUE(failed.GetError().Code() == ErrorCode::IoError);
}

TEST(Expected, ErrorDescribeIncludesCodeName)
{
    const Error error = MakeError(ErrorCode::ParseError, "line 3");
    EXPECT_EQ(error.Describe(), std::string("[ParseError] line 3"));
}

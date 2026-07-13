#include <gtest/gtest.h>

#include "stalkermp/common/Hash.h"
#include "stalkermp/common/Math.h"
#include "stalkermp/common/StringUtil.h"
#include "stalkermp/common/Time.h"
#include "stalkermp/common/Uuid.h"

using namespace stalkermp::common;

// ------------------------------------------------------------------ Hashing

TEST(Hash, Fnv1aKnownVectors)
{
    // Published FNV-1a test vectors.
    EXPECT_EQ(Fnv1a32(""), 0x811c9dc5u);
    EXPECT_EQ(Fnv1a32("a"), 0xe40c292cu);
    EXPECT_EQ(Fnv1a32("foobar"), 0xbf9cf968u);
    EXPECT_EQ(Fnv1a64(""), 0xcbf29ce484222325ull);
    EXPECT_EQ(Fnv1a64("a"), 0xaf63dc4c8601ec8cull);
    EXPECT_EQ(Fnv1a64("foobar"), 0x85944171f73967e8ull);
}

TEST(Hash, IsConstexpr)
{
    static_assert(Fnv1a32("stalkermp") != 0, "Fnv1a32 must be usable at compile time");
    static_assert(Fnv1a64("stalkermp") != 0, "Fnv1a64 must be usable at compile time");
    EXPECT_TRUE(true);
}

// ------------------------------------------------------------------- String

TEST(StringUtil, Trim)
{
    EXPECT_EQ(Trim("  hello  "), std::string_view("hello"));
    EXPECT_EQ(Trim("\t\r\n x \n"), std::string_view("x"));
    EXPECT_EQ(Trim(""), std::string_view(""));
    EXPECT_EQ(Trim("   "), std::string_view(""));
}

TEST(StringUtil, Split)
{
    const auto pieces = Split("a,b,,c", ',');
    ASSERT_EQ(pieces.size(), static_cast<std::size_t>(4));
    EXPECT_EQ(pieces[0], std::string_view("a"));
    EXPECT_EQ(pieces[1], std::string_view("b"));
    EXPECT_EQ(pieces[2], std::string_view(""));
    EXPECT_EQ(pieces[3], std::string_view("c"));

    const auto single = Split("nosep", ',');
    ASSERT_EQ(single.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(single[0], std::string_view("nosep"));
}

TEST(StringUtil, CaseHelpers)
{
    EXPECT_EQ(ToLower("HeLLo"), std::string("hello"));
    EXPECT_TRUE(EqualsIgnoreCase("TRUE", "true"));
    EXPECT_FALSE(EqualsIgnoreCase("TRUE", "truest"));
    EXPECT_TRUE(StartsWith("X-Ray Monolith v1.5.3", "X-Ray Monolith v1.5"));
    EXPECT_FALSE(StartsWith("short", "longer prefix"));
    EXPECT_TRUE(EndsWith("stalkermp.log", ".log"));
    EXPECT_FALSE(EndsWith(".log", "stalkermp.log"));
}

TEST(StringUtil, Format)
{
    EXPECT_EQ(Format("no placeholders"), std::string("no placeholders"));
    EXPECT_EQ(Format("{} + {} = {}", 1, 2, 3), std::string("1 + 2 = 3"));
    EXPECT_EQ(Format("name={} ok={}", "zone", true), std::string("name=zone ok=true"));
    EXPECT_EQ(Format("surplus {} stays {}", 1), std::string("surplus 1 stays {}"));
}

// --------------------------------------------------------------------- Math

TEST(Math, LerpAndApproxEqual)
{
    EXPECT_TRUE(ApproxEqual(Lerp(0.0, 10.0, 0.5), 5.0));
    EXPECT_TRUE(ApproxEqual(0.1 + 0.2, 0.3, 1e-9));
    EXPECT_FALSE(ApproxEqual(0.1, 0.2));
}

// --------------------------------------------------------------------- UUID

TEST(Uuid, GenerateProducesValidVersion4)
{
    const Uuid uuid = Uuid::Generate();
    EXPECT_FALSE(uuid.IsNil());

    const std::string text = uuid.ToString();
    ASSERT_EQ(text.size(), static_cast<std::size_t>(36));
    EXPECT_EQ(text[14], '4'); // Version nibble.

    // Variant nibble is one of 8, 9, a, b.
    const char variant = text[19];
    EXPECT_TRUE(variant == '8' || variant == '9' || variant == 'a' || variant == 'b');
}

TEST(Uuid, RoundTripsThroughString)
{
    const Uuid original = Uuid::Generate();
    const auto parsed = Uuid::Parse(original.ToString());
    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(*parsed == original);
}

TEST(Uuid, ParseRejectsMalformedInput)
{
    EXPECT_FALSE(Uuid::Parse("").has_value());
    EXPECT_FALSE(Uuid::Parse("not-a-uuid").has_value());
    EXPECT_FALSE(Uuid::Parse("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx").has_value());
    EXPECT_FALSE(Uuid::Parse("123456781234123412341234567890ab").has_value()); // No dashes.
}

TEST(Uuid, DistinctGenerations)
{
    EXPECT_TRUE(Uuid::Generate() != Uuid::Generate());
}

// --------------------------------------------------------------------- Time

TEST(Time, StopwatchMeasuresForward)
{
    Stopwatch stopwatch;
    volatile int sink = 0;
    for (int i = 0; i < 1000; ++i)
    {
        sink += i;
    }
    EXPECT_TRUE(stopwatch.ElapsedSeconds() >= 0.0);
    EXPECT_TRUE(stopwatch.Elapsed().count() >= 0);
}

TEST(Time, TimestampFormats)
{
    const auto now = std::chrono::system_clock::now();

    const std::string local = FormatTimestamp(now);
    ASSERT_EQ(local.size(), static_cast<std::size_t>(23)); // "YYYY-MM-DD HH:MM:SS.mmm"
    EXPECT_EQ(local[4], '-');
    EXPECT_EQ(local[10], ' ');
    EXPECT_EQ(local[19], '.');

    const std::string utc = FormatTimestampIso8601Utc(now);
    ASSERT_EQ(utc.size(), static_cast<std::size_t>(20)); // "YYYY-MM-DDTHH:MM:SSZ"
    EXPECT_EQ(utc[10], 'T');
    EXPECT_EQ(utc.back(), 'Z');
}

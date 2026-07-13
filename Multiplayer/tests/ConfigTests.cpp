#include <gtest/gtest.h>

#include "stalkermp/core/Config.h"

using namespace stalkermp::core;

TEST(Config, ParsesSectionsKeysAndComments)
{
    const auto result = config::ParseText(
        "; comment\n"
        "# another comment\n"
        "global_key = global_value\n"
        "\n"
        "[log]\n"
        "level = debug\n"
        "  padded_key   =   padded value  \n",
        "test");
    ASSERT_TRUE(result.HasValue());

    const ConfigStore& store = result.Value();
    EXPECT_EQ(store.KeyCount(), static_cast<std::size_t>(3));
    EXPECT_EQ(store.GetString("", "global_key").value_or(""), "global_value");
    EXPECT_EQ(store.GetString("log", "level").value_or(""), "debug");
    EXPECT_EQ(store.GetString("log", "padded_key").value_or(""), "padded value");
    EXPECT_FALSE(store.GetString("log", "missing").has_value());
    EXPECT_TRUE(store.HasSection("log"));
    EXPECT_FALSE(store.HasSection("network"));
}

TEST(Config, TypedAccessors)
{
    const auto result = config::ParseText(
        "int_value = 42\n"
        "negative = -7\n"
        "float_value = 1.5\n"
        "bool_true = yes\n"
        "bool_false = Off\n"
        "not_a_number = hello\n",
        "test");
    ASSERT_TRUE(result.HasValue());

    const ConfigStore& store = result.Value();
    EXPECT_EQ(store.GetInt("", "int_value").value_or(0), static_cast<std::int64_t>(42));
    EXPECT_EQ(store.GetInt("", "negative").value_or(0), static_cast<std::int64_t>(-7));
    EXPECT_EQ(store.GetDouble("", "float_value").value_or(0.0), 1.5);
    EXPECT_TRUE(store.GetBool("", "bool_true").value_or(false));
    EXPECT_FALSE(store.GetBool("", "bool_false").value_or(true));
    EXPECT_FALSE(store.GetInt("", "not_a_number").has_value());
    EXPECT_FALSE(store.GetBool("", "not_a_number").has_value());
}

TEST(Config, RejectsMalformedLines)
{
    const auto missingEquals = config::ParseText("just some text\n", "test");
    ASSERT_FALSE(missingEquals.HasValue());
    EXPECT_TRUE(missingEquals.GetError().Code() == ErrorCode::ParseError);

    const auto emptyKey = config::ParseText("= value\n", "test");
    ASSERT_FALSE(emptyKey.HasValue());

    const auto badSection = config::ParseText("[unclosed\n", "test");
    ASSERT_FALSE(badSection.HasValue());

    const auto emptySection = config::ParseText("[]\n", "test");
    ASSERT_FALSE(emptySection.HasValue());
}

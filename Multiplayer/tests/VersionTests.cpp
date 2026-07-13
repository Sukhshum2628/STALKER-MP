#include <gtest/gtest.h>

#include "stalkermp/core/Version.h"

using namespace stalkermp::core;

TEST(Version, SemanticVersionToString)
{
    const SemanticVersion v{1, 2, 3};
    EXPECT_EQ(v.ToString(), std::string("1.2.3"));
    EXPECT_TRUE(v == (SemanticVersion{1, 2, 3}));
    EXPECT_TRUE(v != (SemanticVersion{1, 2, 4}));
}

TEST(Version, BannerContainsProjectVersion)
{
    const std::string banner = version::Banner();
    EXPECT_TRUE(banner.find("STALKER-MP") != std::string::npos);
    EXPECT_TRUE(banner.find(version::kProject.ToString()) != std::string::npos);
}

TEST(Version, AcceptsCompatibleEngine)
{
    EXPECT_TRUE(version::VerifyEngineCompatibility("X-Ray Monolith v1.5.3").HasValue());
    EXPECT_TRUE(version::VerifyEngineCompatibility("X-Ray Monolith v1.5.4-beta").HasValue());
}

TEST(Version, RejectsIncompatibleEngine)
{
    const auto wrongEngine = version::VerifyEngineCompatibility("OpenXRay 1.6");
    ASSERT_FALSE(wrongEngine.HasValue());
    EXPECT_TRUE(wrongEngine.GetError().Code() == ErrorCode::VersionMismatch);

    const auto emptyVersion = version::VerifyEngineCompatibility("");
    ASSERT_FALSE(emptyVersion.HasValue());
    EXPECT_TRUE(emptyVersion.GetError().Code() == ErrorCode::InvalidArgument);
}

// STALKER-MP — Plugin Framework tests (Sprint-014)
//
// Batch-1 (Steps 01-02): the engine-free / loader-free / OS-free value vocabulary
// (`PluginTypes.h`) and the `[plugins]` configuration parser (`PluginConfiguration`).
// Types-and-config only — no contract, registry, loader, or wiring.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include "stalkermp/core/Config.h"
#include "stalkermp/plugin/PluginConfiguration.h"
#include "stalkermp/plugin/PluginTypes.h"

namespace
{
    using namespace stalkermp;
} // namespace

// ============================================================================
// Step 01 — PluginTypes (value vocabulary)
// ============================================================================

TEST(PluginTypesStep1, EnumsHaveUint8UnderlyingType)
{
    EXPECT_TRUE((std::is_same_v<std::underlying_type_t<plugin::PluginOutcome>, std::uint8_t>));
    EXPECT_TRUE((std::is_same_v<std::underlying_type_t<plugin::PluginState>, std::uint8_t>));
    EXPECT_TRUE((std::is_same_v<std::underlying_type_t<plugin::PluginEvent>, std::uint8_t>));
    // Reserved 0 is the neutral value in each enumeration.
    EXPECT_EQ(static_cast<std::uint8_t>(plugin::PluginOutcome::Ok), 0u);
    EXPECT_EQ(static_cast<std::uint8_t>(plugin::PluginState::Discovered), 0u);
    EXPECT_EQ(static_cast<std::uint8_t>(plugin::PluginEvent::OnServerStart), 0u);
}

TEST(PluginTypesStep1, ScalarValueShapesAreTriviallyCopyable)
{
    EXPECT_TRUE(std::is_trivially_copyable_v<plugin::PluginId>);
    EXPECT_TRUE(std::is_trivially_copyable_v<plugin::PluginVersion>);
    EXPECT_TRUE(std::is_trivially_copyable_v<plugin::PluginStatistics>);
    EXPECT_TRUE(std::is_trivially_copyable_v<plugin::PluginConsistency>);
}

TEST(PluginTypesStep1, DefaultConstructionIsZeroedNeutral)
{
    plugin::PluginId id{};
    EXPECT_TRUE(id.IsNone());

    plugin::PluginVersion v{};
    EXPECT_EQ(v.major, 0u);
    EXPECT_EQ(v.minor, 0u);
    EXPECT_EQ(v.patch, 0u);

    plugin::PluginDescriptor d{};
    EXPECT_TRUE(d.id.IsNone());
    EXPECT_EQ(d.minApiVersion, 0u);
    EXPECT_EQ(d.maxApiVersion, 0u);
    EXPECT_EQ(d.generation, 0u);
    EXPECT_TRUE(d.requiredApis.empty());
    EXPECT_TRUE(d.optionalDependencies.empty());

    plugin::PluginStatistics s{};
    EXPECT_EQ(s.loadedPlugins, 0u);
    EXPECT_EQ(s.activePlugins, 0u);
    EXPECT_EQ(s.eventDispatches, 0u);
    EXPECT_EQ(s.apiCalls, 0u);
    EXPECT_EQ(s.pluginErrors, 0u);

    plugin::PluginConsistency c{};
    EXPECT_EQ(c.registeredPlugins, 0u);
    EXPECT_EQ(c.boundHooks, 0u);
    EXPECT_TRUE(c.consistent);
}

TEST(PluginTypesStep1, ValueIdSemantics)
{
    plugin::PluginId a{1};
    plugin::PluginId b{2};
    plugin::PluginId a2{1};
    EXPECT_FALSE(a.IsNone());
    EXPECT_TRUE(plugin::PluginId{}.IsNone());
    EXPECT_EQ(a, a2);
    EXPECT_NE(a, b);
    EXPECT_LT(a, b);
}

TEST(PluginTypesStep1, VersionOrdering)
{
    EXPECT_LT((plugin::PluginVersion{1, 0, 0}), (plugin::PluginVersion{1, 1, 0}));
    EXPECT_LT((plugin::PluginVersion{1, 2, 3}), (plugin::PluginVersion{2, 0, 0}));
    EXPECT_LT((plugin::PluginVersion{1, 2, 3}), (plugin::PluginVersion{1, 2, 4}));
    EXPECT_EQ((plugin::PluginVersion{2, 5, 1}), (plugin::PluginVersion{2, 5, 1}));
    EXPECT_NE((plugin::PluginVersion{2, 5, 1}), (plugin::PluginVersion{2, 5, 2}));
}

TEST(PluginTypesStep1, OutcomeNamesAreTotal)
{
    EXPECT_STREQ(plugin::PluginOutcomeName(plugin::PluginOutcome::Ok), "Ok");
    EXPECT_STREQ(plugin::PluginOutcomeName(plugin::PluginOutcome::VersionMismatch), "VersionMismatch");
    EXPECT_STREQ(plugin::PluginOutcomeName(plugin::PluginOutcome::MissingDependency), "MissingDependency");
    EXPECT_STREQ(plugin::PluginOutcomeName(plugin::PluginOutcome::NotFound), "NotFound");
    for (std::uint8_t i = 0; i <= static_cast<std::uint8_t>(plugin::PluginOutcome::NotFound); ++i)
    {
        EXPECT_STRNE(plugin::PluginOutcomeName(static_cast<plugin::PluginOutcome>(i)), "Unknown");
    }
}

TEST(PluginTypesStep1, StateAndEventNamesAreTotal)
{
    EXPECT_STREQ(plugin::PluginStateName(plugin::PluginState::Active), "Active");
    EXPECT_STREQ(plugin::PluginStateName(plugin::PluginState::Unloaded), "Unloaded");
    for (std::uint8_t i = 0; i <= static_cast<std::uint8_t>(plugin::PluginState::Failed); ++i)
    {
        EXPECT_STRNE(plugin::PluginStateName(static_cast<plugin::PluginState>(i)), "Unknown");
    }

    EXPECT_STREQ(plugin::PluginEventName(plugin::PluginEvent::OnPlayerJoin), "OnPlayerJoin");
    EXPECT_STREQ(plugin::PluginEventName(plugin::PluginEvent::OnTick), "OnTick");
    for (std::uint8_t i = 0; i <= static_cast<std::uint8_t>(plugin::PluginEvent::OnTick); ++i)
    {
        EXPECT_STRNE(plugin::PluginEventName(static_cast<plugin::PluginEvent>(i)), "Unknown");
    }
}

// ============================================================================
// Step 02 — PluginConfiguration::FromConfig
// ============================================================================

TEST(PluginConfigStep2, DefaultsWhenSectionAbsent)
{
    core::ConfigStore store;
    const auto r = plugin::PluginConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.pluginDirectoryToken, "plugins");
    EXPECT_EQ(c.maxPlugins, 64u);
    EXPECT_TRUE(c.enabled);
    EXPECT_TRUE(c.validateOnLoad);
    EXPECT_TRUE(c.strictApiVersion);
    EXPECT_EQ(c.version, 1u);
}

TEST(PluginConfigStep2, OverridesParsed)
{
    core::ConfigStore store;
    store.Set("plugins", "plugin_directory", "server/plugins");
    store.Set("plugins", "max_plugins", "128");
    store.Set("plugins", "enabled", "false");
    store.Set("plugins", "validate_on_load", "false");
    store.Set("plugins", "strict_api_version", "false");
    store.Set("plugins", "version", "3");
    const auto r = plugin::PluginConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.pluginDirectoryToken, "server/plugins");
    EXPECT_EQ(c.maxPlugins, 128u);
    EXPECT_FALSE(c.enabled);
    EXPECT_FALSE(c.validateOnLoad);
    EXPECT_FALSE(c.strictApiVersion);
    EXPECT_EQ(c.version, 3u);
}

TEST(PluginConfigStep2, BoundaryMinimumsAccepted)
{
    core::ConfigStore store;
    store.Set("plugins", "max_plugins", "1");
    store.Set("plugins", "version", "1");
    const auto r = plugin::PluginConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(r.Value().maxPlugins, 1u);
    EXPECT_EQ(r.Value().version, 1u);
}

TEST(PluginConfigStep2, InvalidValuesRejected)
{
    {
        core::ConfigStore s;
        s.Set("plugins", "max_plugins", "0"); // below min 1
        EXPECT_FALSE(plugin::PluginConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("plugins", "version", "0"); // below min 1
        EXPECT_FALSE(plugin::PluginConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("plugins", "max_plugins", "-9"); // negative
        EXPECT_FALSE(plugin::PluginConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("plugins", "plugin_directory", ""); // empty token
        EXPECT_FALSE(plugin::PluginConfiguration::FromConfig(s).HasValue());
    }
}

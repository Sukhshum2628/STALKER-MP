// STALKER-MP — Plugin Framework tests (Sprint-014)
//
// Batch-1 (Steps 01-02): the engine-free / loader-free / OS-free value vocabulary
// (`PluginTypes.h`) and the `[plugins]` configuration parser (`PluginConfiguration`).
// Types-and-config only — no contract, registry, loader, or wiring.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include "stalkermp/adapters/PlatformPluginStore.h"
#include "stalkermp/core/Config.h"
#include "stalkermp/lua/RecordingScriptApi.h"
#include "stalkermp/plugin/EventBinding.h"
#include "stalkermp/plugin/IPlugin.h"
#include "stalkermp/plugin/IPluginSource.h"
#include "stalkermp/plugin/InMemoryPluginSource.h"
#include "stalkermp/plugin/NullPluginSource.h"
#include "stalkermp/plugin/PluginConfiguration.h"
#include "stalkermp/plugin/PluginContext.h"
#include "stalkermp/plugin/PluginHostSurface.h"
#include "stalkermp/plugin/PluginRegistry.h"
#include "stalkermp/plugin/PluginTypes.h"
#include "stalkermp/plugin/PluginValidator.h"

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

// ============================================================================
// Step 03 — IPlugin contract (FakePlugin / NullPlugin)
// ============================================================================

TEST(PluginContractStep3, InterfaceIsPureAbstract)
{
    EXPECT_TRUE(std::is_abstract_v<plugin::IPlugin>);
    EXPECT_TRUE(std::has_virtual_destructor_v<plugin::IPlugin>);
    // Concrete doubles are usable through the interface.
    EXPECT_FALSE(std::is_abstract_v<plugin::FakePlugin>);
    EXPECT_FALSE(std::is_abstract_v<plugin::NullPlugin>);
}

TEST(PluginContractStep3, FakePluginRecordsLifecycleAndEvents)
{
    lua::RecordingScriptApi api;                  // reused Sprint-013 host surface ([AR-P3] Option A)
    const plugin::PluginHostServices host = api.AsSet();

    plugin::FakePlugin fake;
    fake.descriptor.id = plugin::PluginId{7};
    fake.descriptor.version = plugin::PluginVersion{1, 2, 0};

    plugin::IPlugin& p = fake;                    // drive through the contract
    EXPECT_EQ(p.Descriptor().id.value, 7u);
    EXPECT_EQ(p.Descriptor().version.minor, 2u);

    EXPECT_EQ(p.Initialize(host), plugin::PluginOutcome::Ok);
    EXPECT_TRUE(fake.initialized);

    EXPECT_EQ(p.OnEvent(plugin::PluginEvent::OnPlayerJoin, plugin::PluginArgs{}), plugin::PluginOutcome::Ok);
    EXPECT_EQ(p.OnEvent(plugin::PluginEvent::OnTick, plugin::PluginArgs{}), plugin::PluginOutcome::Ok);
    ASSERT_EQ(fake.events.size(), 2u);
    EXPECT_EQ(fake.events[0], plugin::PluginEvent::OnPlayerJoin);
    EXPECT_EQ(fake.events[1], plugin::PluginEvent::OnTick);

    p.Shutdown();
    EXPECT_TRUE(fake.shutdownCalled);
    EXPECT_FALSE(fake.initialized);
}

TEST(PluginContractStep3, FakePluginScriptableFailureOutcomes)
{
    lua::RecordingScriptApi api;
    const plugin::PluginHostServices host = api.AsSet();

    plugin::FakePlugin fake;
    fake.initOutcome = plugin::PluginOutcome::InitFailed;
    EXPECT_EQ(fake.Initialize(host), plugin::PluginOutcome::InitFailed);
    EXPECT_FALSE(fake.initialized); // not activated on failure

    fake.eventOutcome = plugin::PluginOutcome::RuntimeError;
    EXPECT_EQ(fake.OnEvent(plugin::PluginEvent::OnTick, plugin::PluginArgs{}), plugin::PluginOutcome::RuntimeError);
}

TEST(PluginContractStep3, NullPluginIsInert)
{
    lua::RecordingScriptApi api;
    const plugin::PluginHostServices host = api.AsSet();
    plugin::NullPlugin null;
    plugin::IPlugin& p = null;
    EXPECT_TRUE(p.Descriptor().id.IsNone());
    EXPECT_EQ(p.Initialize(host), plugin::PluginOutcome::Ok);
    EXPECT_EQ(p.OnEvent(plugin::PluginEvent::OnServerStart, plugin::PluginArgs{}), plugin::PluginOutcome::Ok);
    p.Shutdown(); // no-op, no throw
}

// ============================================================================
// Step 04 — PluginContext + PluginRegistry
// ============================================================================

namespace
{
    plugin::PluginContext MakeContext(std::uint64_t id, plugin::PluginState state = plugin::PluginState::Loaded)
    {
        plugin::PluginContext c;
        c.id = plugin::PluginId{id};
        c.descriptor.id = plugin::PluginId{id};
        c.descriptor.version = plugin::PluginVersion{1, 0, 0};
        c.state = state;
        return c;
    }
} // namespace

TEST(PluginRegistryStep4, RegisterFindEnumerateAscending)
{
    plugin::PluginRegistry reg;
    // Insert out of order; enumeration must come back ascending.
    EXPECT_EQ(reg.Register(MakeContext(30)), plugin::PluginOutcome::Ok);
    EXPECT_EQ(reg.Register(MakeContext(10)), plugin::PluginOutcome::Ok);
    EXPECT_EQ(reg.Register(MakeContext(20)), plugin::PluginOutcome::Ok);
    EXPECT_EQ(reg.Count(), 3u);

    const auto all = reg.Enumerate();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].id.value, 10u);
    EXPECT_EQ(all[1].id.value, 20u);
    EXPECT_EQ(all[2].id.value, 30u);

    const plugin::PluginContext* found = reg.Find(plugin::PluginId{20});
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->state, plugin::PluginState::Loaded);
    EXPECT_EQ(reg.Find(plugin::PluginId{999}), nullptr);
    EXPECT_TRUE(reg.Contains(plugin::PluginId{10}));
    EXPECT_FALSE(reg.Contains(plugin::PluginId{11}));
}

TEST(PluginRegistryStep4, DuplicateRejectedAsValueOutcome)
{
    plugin::PluginRegistry reg;
    EXPECT_EQ(reg.Register(MakeContext(5)), plugin::PluginOutcome::Ok);
    EXPECT_EQ(reg.Register(MakeContext(5)), plugin::PluginOutcome::DuplicatePlugin);
    EXPECT_EQ(reg.Count(), 1u); // unchanged
}

TEST(PluginRegistryStep4, NoneIdRejected)
{
    plugin::PluginRegistry reg;
    EXPECT_EQ(reg.Register(MakeContext(0)), plugin::PluginOutcome::NotFound);
    EXPECT_EQ(reg.Count(), 0u);
}

TEST(PluginRegistryStep4, UnregisterMissingIsNotFound)
{
    plugin::PluginRegistry reg;
    EXPECT_EQ(reg.Register(MakeContext(7)), plugin::PluginOutcome::Ok);
    EXPECT_EQ(reg.Unregister(plugin::PluginId{8}), plugin::PluginOutcome::NotFound);
    EXPECT_EQ(reg.Unregister(plugin::PluginId{7}), plugin::PluginOutcome::Ok);
    EXPECT_EQ(reg.Count(), 0u);
    EXPECT_EQ(reg.Unregister(plugin::PluginId{7}), plugin::PluginOutcome::NotFound); // already gone
}

TEST(PluginRegistryStep4, MutableFindAllowsInPlaceStateUpdate)
{
    plugin::PluginRegistry reg;
    EXPECT_EQ(reg.Register(MakeContext(1, plugin::PluginState::Loaded)), plugin::PluginOutcome::Ok);
    plugin::PluginContext* c = reg.Find(plugin::PluginId{1});
    ASSERT_NE(c, nullptr);
    c->state = plugin::PluginState::Active;
    c->hooks.push_back(plugin::PluginEvent::OnTick);
    const plugin::PluginContext* again = reg.Find(plugin::PluginId{1});
    ASSERT_NE(again, nullptr);
    EXPECT_EQ(again->state, plugin::PluginState::Active);
    ASSERT_EQ(again->hooks.size(), 1u);
    EXPECT_EQ(again->hooks[0], plugin::PluginEvent::OnTick);
}

// ============================================================================
// Step 05 — EventBinding registry
// ============================================================================

TEST(PluginEventBindingStep5, BindQueryAscendingOrder)
{
    plugin::EventBinding eb;
    // Bind out of order across two events; queries must be ascending by plugin id.
    EXPECT_EQ(eb.Bind(plugin::PluginEvent::OnTick, plugin::PluginId{3}), plugin::PluginOutcome::Ok);
    EXPECT_EQ(eb.Bind(plugin::PluginEvent::OnTick, plugin::PluginId{1}), plugin::PluginOutcome::Ok);
    EXPECT_EQ(eb.Bind(plugin::PluginEvent::OnTick, plugin::PluginId{2}), plugin::PluginOutcome::Ok);
    EXPECT_EQ(eb.Bind(plugin::PluginEvent::OnPlayerJoin, plugin::PluginId{5}), plugin::PluginOutcome::Ok);

    const auto tick = eb.SubscribersFor(plugin::PluginEvent::OnTick);
    ASSERT_EQ(tick.size(), 3u);
    EXPECT_EQ(tick[0].value, 1u);
    EXPECT_EQ(tick[1].value, 2u);
    EXPECT_EQ(tick[2].value, 3u);

    EXPECT_EQ(eb.CountFor(plugin::PluginEvent::OnPlayerJoin), 1u);
    EXPECT_EQ(eb.CountFor(plugin::PluginEvent::OnServerStop), 0u);
    EXPECT_EQ(eb.Count(), 4u);
    EXPECT_TRUE(eb.SubscribersFor(plugin::PluginEvent::OnServerStop).empty());
}

TEST(PluginEventBindingStep5, DuplicateBindingRejected)
{
    plugin::EventBinding eb;
    EXPECT_EQ(eb.Bind(plugin::PluginEvent::OnTick, plugin::PluginId{1}), plugin::PluginOutcome::Ok);
    EXPECT_EQ(eb.Bind(plugin::PluginEvent::OnTick, plugin::PluginId{1}), plugin::PluginOutcome::DuplicatePlugin);
    EXPECT_EQ(eb.Count(), 1u);
    // Same plugin under a DIFFERENT event is a distinct, allowed subscription.
    EXPECT_EQ(eb.Bind(plugin::PluginEvent::OnPlayerLeave, plugin::PluginId{1}), plugin::PluginOutcome::Ok);
    EXPECT_EQ(eb.Count(), 2u);
}

TEST(PluginEventBindingStep5, NonePluginRejected)
{
    plugin::EventBinding eb;
    EXPECT_EQ(eb.Bind(plugin::PluginEvent::OnTick, plugin::PluginId{0}), plugin::PluginOutcome::InvalidHook);
    EXPECT_EQ(eb.Count(), 0u);
}

TEST(PluginEventBindingStep5, UnbindRemovesAndMissingIsNotFound)
{
    plugin::EventBinding eb;
    EXPECT_EQ(eb.Bind(plugin::PluginEvent::OnTick, plugin::PluginId{1}), plugin::PluginOutcome::Ok);
    EXPECT_EQ(eb.Bind(plugin::PluginEvent::OnTick, plugin::PluginId{2}), plugin::PluginOutcome::Ok);
    EXPECT_EQ(eb.Unbind(plugin::PluginEvent::OnTick, plugin::PluginId{1}), plugin::PluginOutcome::Ok);
    EXPECT_EQ(eb.CountFor(plugin::PluginEvent::OnTick), 1u);
    EXPECT_EQ(eb.SubscribersFor(plugin::PluginEvent::OnTick)[0].value, 2u);
    EXPECT_EQ(eb.Unbind(plugin::PluginEvent::OnTick, plugin::PluginId{1}), plugin::PluginOutcome::NotFound);
    EXPECT_EQ(eb.Unbind(plugin::PluginEvent::OnPlayerJoin, plugin::PluginId{9}), plugin::PluginOutcome::NotFound);
}

// ============================================================================
// Step 06 — PluginValidator
// ============================================================================

namespace
{
    plugin::PluginConfiguration EnabledConfig(std::uint32_t version = 5, bool strict = true)
    {
        plugin::PluginConfiguration c;
        c.enabled = true;
        c.version = version;         // host API version target
        c.strictApiVersion = strict;
        return c;
    }

    plugin::PluginDescriptor Descriptor(std::uint64_t id, std::uint32_t minApi, std::uint32_t maxApi)
    {
        plugin::PluginDescriptor d;
        d.id = plugin::PluginId{id};
        d.version = plugin::PluginVersion{1, 0, 0};
        d.minApiVersion = minApi;
        d.maxApiVersion = maxApi;
        return d;
    }
} // namespace

TEST(PluginValidatorStep6, IndividualChecks)
{
    plugin::PluginRegistry reg;
    {
        plugin::PluginContext c;
        c.id = plugin::PluginId{1};
        ASSERT_EQ(reg.Register(c), plugin::PluginOutcome::Ok);
    }

    // Configuration: disabled framework rejects.
    plugin::PluginConfiguration disabled;
    disabled.enabled = false;
    EXPECT_EQ(plugin::PluginValidator::ValidateConfiguration(disabled), plugin::PluginOutcome::PluginDisabled);
    EXPECT_EQ(plugin::PluginValidator::ValidateConfiguration(EnabledConfig()), plugin::PluginOutcome::Ok);

    // Descriptor: none id -> NotFound; inverted range -> VersionMismatch.
    EXPECT_EQ(plugin::PluginValidator::ValidateDescriptor(Descriptor(0, 1, 2)), plugin::PluginOutcome::NotFound);
    EXPECT_EQ(plugin::PluginValidator::ValidateDescriptor(Descriptor(2, 5, 3)),
              plugin::PluginOutcome::VersionMismatch);
    EXPECT_EQ(plugin::PluginValidator::ValidateDescriptor(Descriptor(2, 1, 9)), plugin::PluginOutcome::Ok);

    // Duplicate.
    EXPECT_EQ(plugin::PluginValidator::ValidateDuplicate(plugin::PluginId{1}, reg),
              plugin::PluginOutcome::DuplicatePlugin);
    EXPECT_EQ(plugin::PluginValidator::ValidateDuplicate(plugin::PluginId{2}, reg), plugin::PluginOutcome::Ok);

    // Version negotiation (host = 5): host < min -> ApiIncompatible; strict host > max -> VersionMismatch.
    EXPECT_EQ(plugin::PluginValidator::ValidateVersion(Descriptor(2, 6, 9), 5, true),
              plugin::PluginOutcome::ApiIncompatible);
    EXPECT_EQ(plugin::PluginValidator::ValidateVersion(Descriptor(2, 1, 4), 5, true),
              plugin::PluginOutcome::VersionMismatch);
    EXPECT_EQ(plugin::PluginValidator::ValidateVersion(Descriptor(2, 1, 4), 5, false), plugin::PluginOutcome::Ok);
    EXPECT_EQ(plugin::PluginValidator::ValidateVersion(Descriptor(2, 3, 7), 5, true), plugin::PluginOutcome::Ok);

    // Required APIs.
    EXPECT_EQ(plugin::PluginValidator::ValidateRequiredApis({10, 20}, {10, 20, 30}), plugin::PluginOutcome::Ok);
    EXPECT_EQ(plugin::PluginValidator::ValidateRequiredApis({10, 99}, {10, 20, 30}),
              plugin::PluginOutcome::ApiIncompatible);

    // Dependency declaration validity.
    plugin::PluginDescriptor okDeps = Descriptor(2, 1, 9);
    okDeps.optionalDependencies = {plugin::PluginId{5}, plugin::PluginId{6}};
    EXPECT_EQ(plugin::PluginValidator::ValidateDependencies(okDeps), plugin::PluginOutcome::Ok);

    plugin::PluginDescriptor selfDep = Descriptor(2, 1, 9);
    selfDep.optionalDependencies = {plugin::PluginId{2}}; // self-reference
    EXPECT_EQ(plugin::PluginValidator::ValidateDependencies(selfDep), plugin::PluginOutcome::MissingDependency);

    plugin::PluginDescriptor dupDep = Descriptor(2, 1, 9);
    dupDep.optionalDependencies = {plugin::PluginId{5}, plugin::PluginId{5}}; // duplicate
    EXPECT_EQ(plugin::PluginValidator::ValidateDependencies(dupDep), plugin::PluginOutcome::MissingDependency);

    plugin::PluginDescriptor noneDep = Descriptor(2, 1, 9);
    noneDep.optionalDependencies = {plugin::PluginId{0}}; // none id
    EXPECT_EQ(plugin::PluginValidator::ValidateDependencies(noneDep), plugin::PluginOutcome::MissingDependency);
}

TEST(PluginValidatorStep6, CompositeValidatePassesWhenAllGood)
{
    plugin::PluginRegistry reg;
    plugin::PluginDescriptor d = Descriptor(20, /*min*/ 3, /*max*/ 7);
    d.requiredApis = {10};
    d.optionalDependencies = {plugin::PluginId{99}}; // optional; declaration valid (non-none, not self)

    EXPECT_EQ(plugin::PluginValidator::Validate(d, reg, /*available*/ {10, 20}, EnabledConfig(/*host*/ 5, true)),
              plugin::PluginOutcome::Ok);
}

TEST(PluginValidatorStep6, CompositeValidateShortCircuitsInOrder)
{
    plugin::PluginRegistry reg;
    {
        plugin::PluginContext existing;
        existing.id = plugin::PluginId{20};
        ASSERT_EQ(reg.Register(existing), plugin::PluginOutcome::Ok);
    }
    // Disabled config is checked FIRST, before duplicate/version/etc.
    plugin::PluginConfiguration disabled = EnabledConfig();
    disabled.enabled = false;
    plugin::PluginDescriptor d = Descriptor(20, 6, 9); // also duplicate + version-incompatible
    d.requiredApis = {99};                             // also missing api
    EXPECT_EQ(plugin::PluginValidator::Validate(d, reg, {10}, disabled), plugin::PluginOutcome::PluginDisabled);

    // With config enabled, duplicate is the next failing gate.
    EXPECT_EQ(plugin::PluginValidator::Validate(d, reg, {10}, EnabledConfig(5, true)),
              plugin::PluginOutcome::DuplicatePlugin);
}

// ============================================================================
// Step 07 — IPluginSource discovery seam (InMemoryPluginSource / NullPluginSource)
// ============================================================================

namespace
{
    plugin::PluginDescriptor Manifest(std::uint64_t id, std::uint32_t minApi = 1, std::uint32_t maxApi = 1)
    {
        plugin::PluginDescriptor d;
        d.id = plugin::PluginId{id};
        d.version = plugin::PluginVersion{1, 0, 0};
        d.minApiVersion = minApi;
        d.maxApiVersion = maxApi;
        return d;
    }
} // namespace

TEST(PluginSourceStep7, InMemoryStoreEnumerateAscending)
{
    plugin::InMemoryPluginSource src;
    // Store out of order; enumeration must be ascending by id.
    src.Store(Manifest(30));
    src.Store(Manifest(10));
    src.Store(Manifest(20));
    EXPECT_EQ(src.Count(), 3u);

    const auto all = src.Enumerate();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].id.value, 10u);
    EXPECT_EQ(all[1].id.value, 20u);
    EXPECT_EQ(all[2].id.value, 30u);
}

TEST(PluginSourceStep7, InMemoryReadManifestAndExists)
{
    plugin::InMemoryPluginSource src;
    src.Store(Manifest(7, /*min*/ 2, /*max*/ 5));

    EXPECT_TRUE(src.Exists(plugin::PluginId{7}));
    EXPECT_FALSE(src.Exists(plugin::PluginId{8}));

    const auto ok = src.ReadManifest(plugin::PluginId{7});
    ASSERT_TRUE(ok.HasValue());
    EXPECT_EQ(ok.Value().id.value, 7u);
    EXPECT_EQ(ok.Value().minApiVersion, 2u);
    EXPECT_EQ(ok.Value().maxApiVersion, 5u);

    // Missing -> value outcome (NotFound), nothing thrown.
    EXPECT_FALSE(src.ReadManifest(plugin::PluginId{8}).HasValue());
}

TEST(PluginSourceStep7, InMemoryStoreReplacesAndRemove)
{
    plugin::InMemoryPluginSource src;
    src.Store(Manifest(5, /*min*/ 1, /*max*/ 1));
    src.Store(Manifest(5, /*min*/ 3, /*max*/ 9)); // replace same id
    EXPECT_EQ(src.Count(), 1u);
    ASSERT_TRUE(src.ReadManifest(plugin::PluginId{5}).HasValue());
    EXPECT_EQ(src.ReadManifest(plugin::PluginId{5}).Value().maxApiVersion, 9u);

    src.Remove(plugin::PluginId{5});
    EXPECT_EQ(src.Count(), 0u);
    EXPECT_FALSE(src.Exists(plugin::PluginId{5}));
    src.Remove(plugin::PluginId{5}); // benign when absent
    EXPECT_EQ(src.Count(), 0u);
}

TEST(PluginSourceStep7, NullSourceIsInert)
{
    plugin::NullPluginSource src;
    EXPECT_TRUE(src.Enumerate().empty());
    EXPECT_FALSE(src.Exists(plugin::PluginId{1}));
    EXPECT_FALSE(src.ReadManifest(plugin::PluginId{1}).HasValue());
}

TEST(PluginSourceStep7, UsableThroughInterfaceReferenceAndFakeAlias)
{
    plugin::FakePluginSource concrete; // alias of InMemoryPluginSource
    concrete.Store(Manifest(1));
    const plugin::IPluginSource& src = concrete; // engine-free polymorphic use
    EXPECT_EQ(src.Enumerate().size(), 1u);
    EXPECT_TRUE(src.Exists(plugin::PluginId{1}));
    ASSERT_TRUE(src.ReadManifest(plugin::PluginId{1}).HasValue());
    EXPECT_EQ(src.ReadManifest(plugin::PluginId{1}).Value().id.value, 1u);
}

// ============================================================================
// Step 08 — PlatformPluginStore (single platform boundary; static registration)
// ============================================================================

TEST(PlatformPluginStoreStep8, StaticRegistrationDeterministicOrder)
{
    adapters::ClearStaticPlugins();
    // Register out of order; the discovery source must enumerate ascending by id.
    EXPECT_EQ(adapters::RegisterStaticPlugin(Manifest(30)), plugin::PluginOutcome::Ok);
    EXPECT_EQ(adapters::RegisterStaticPlugin(Manifest(10)), plugin::PluginOutcome::Ok);
    EXPECT_EQ(adapters::RegisterStaticPlugin(Manifest(20)), plugin::PluginOutcome::Ok);
    EXPECT_EQ(adapters::StaticPluginCount(), 3u);

    plugin::PluginConfiguration cfg;
    auto src = adapters::CreatePlatformPluginSource(cfg);
    ASSERT_NE(src, nullptr);
    const auto all = src->Enumerate();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].id.value, 10u);
    EXPECT_EQ(all[1].id.value, 20u);
    EXPECT_EQ(all[2].id.value, 30u);

    adapters::ClearStaticPlugins();
}

TEST(PlatformPluginStoreStep8, ManifestLookupAndExists)
{
    adapters::ClearStaticPlugins();
    ASSERT_EQ(adapters::RegisterStaticPlugin(Manifest(7, /*min*/ 2, /*max*/ 5)), plugin::PluginOutcome::Ok);

    plugin::PluginConfiguration cfg;
    auto src = adapters::CreatePlatformPluginSource(cfg);
    ASSERT_NE(src, nullptr);
    EXPECT_TRUE(src->Exists(plugin::PluginId{7}));
    EXPECT_FALSE(src->Exists(plugin::PluginId{8}));
    const auto m = src->ReadManifest(plugin::PluginId{7});
    ASSERT_TRUE(m.HasValue());
    EXPECT_EQ(m.Value().minApiVersion, 2u);
    EXPECT_EQ(m.Value().maxApiVersion, 5u);
    EXPECT_FALSE(src->ReadManifest(plugin::PluginId{8}).HasValue()); // NotFound value outcome

    adapters::ClearStaticPlugins();
}

TEST(PlatformPluginStoreStep8, DuplicateAndNoneRegistrationRejected)
{
    adapters::ClearStaticPlugins();
    EXPECT_EQ(adapters::RegisterStaticPlugin(Manifest(5)), plugin::PluginOutcome::Ok);
    EXPECT_EQ(adapters::RegisterStaticPlugin(Manifest(5)), plugin::PluginOutcome::DuplicatePlugin);
    EXPECT_EQ(adapters::RegisterStaticPlugin(Manifest(0)), plugin::PluginOutcome::NotFound); // none id
    EXPECT_EQ(adapters::StaticPluginCount(), 1u); // only the first registration stuck

    adapters::ClearStaticPlugins();
}

TEST(PlatformPluginStoreStep8, EmptyRegistrationYieldsInertSource)
{
    adapters::ClearStaticPlugins();
    plugin::PluginConfiguration cfg;
    auto src = adapters::CreatePlatformPluginSource(cfg);
    ASSERT_NE(src, nullptr);
    EXPECT_TRUE(src->Enumerate().empty());
    EXPECT_FALSE(src->Exists(plugin::PluginId{1}));
    EXPECT_FALSE(src->ReadManifest(plugin::PluginId{1}).HasValue());
}

TEST(PlatformPluginStoreStep8, SourceIsReadOnlySnapshotAfterConstruction)
{
    adapters::ClearStaticPlugins();
    ASSERT_EQ(adapters::RegisterStaticPlugin(Manifest(1)), plugin::PluginOutcome::Ok);

    plugin::PluginConfiguration cfg;
    auto src = adapters::CreatePlatformPluginSource(cfg); // snapshots {1}
    // Registering more AFTER construction does not affect the existing source (read-only).
    ASSERT_EQ(adapters::RegisterStaticPlugin(Manifest(2)), plugin::PluginOutcome::Ok);
    EXPECT_EQ(src->Enumerate().size(), 1u);
    EXPECT_FALSE(src->Exists(plugin::PluginId{2}));

    // A freshly-created source sees the current registrations.
    auto src2 = adapters::CreatePlatformPluginSource(cfg);
    EXPECT_EQ(src2->Enumerate().size(), 2u);

    adapters::ClearStaticPlugins();
}

// ============================================================================
// Step 09 — Plugin host-service exposure seam (gameplay facades only, AR-P3 Option A)
// ============================================================================

TEST(PluginHostSurfaceStep9, ServiceTypeNamesAreTotalAndCount)
{
    EXPECT_EQ(plugin::kGameplayServiceCount, 7u);
    EXPECT_STREQ(plugin::PluginServiceTypeName(plugin::PluginServiceType::World), "World");
    EXPECT_STREQ(plugin::PluginServiceTypeName(plugin::PluginServiceType::Config), "Config");
    for (std::uint8_t i = 0; i <= static_cast<std::uint8_t>(plugin::PluginServiceType::Config); ++i)
    {
        EXPECT_STRNE(plugin::PluginServiceTypeName(static_cast<plugin::PluginServiceType>(i)), "Unknown");
    }
}

TEST(PluginHostSurfaceStep9, CatalogExposesExactlyTheSevenGameplayServices)
{
    lua::RecordingScriptApi api;
    plugin::GameplayHostSurface surface(api.AsSet());
    const plugin::IPluginHostSurface& s = surface;

    const auto catalog = s.Catalog();
    ASSERT_EQ(catalog.size(), 7u); // exactly the gameplay services; no admin/tooling
    // Deterministic ascending-by-type order with correct metadata.
    EXPECT_EQ(catalog[0].type, plugin::PluginServiceType::World);
    EXPECT_STREQ(catalog[0].name, "World");
    EXPECT_TRUE(catalog[0].available);
    EXPECT_EQ(catalog[6].type, plugin::PluginServiceType::Config);
    EXPECT_STREQ(catalog[6].name, "Config");

    // Every gameplay service is available by default; lookup is deterministic.
    for (std::uint8_t i = 0; i <= static_cast<std::uint8_t>(plugin::PluginServiceType::Config); ++i)
    {
        EXPECT_TRUE(s.IsAvailable(static_cast<plugin::PluginServiceType>(i)));
    }
}

TEST(PluginHostSurfaceStep9, ServiceAvailabilityIsQueryableAndImmutableMetadata)
{
    lua::RecordingScriptApi api;
    plugin::GameplayHostSurface surface(api.AsSet());
    surface.SetAvailable(plugin::PluginServiceType::Inventory, false);

    const plugin::IPluginHostSurface& s = surface;
    EXPECT_FALSE(s.IsAvailable(plugin::PluginServiceType::Inventory));
    EXPECT_TRUE(s.IsAvailable(plugin::PluginServiceType::World));

    // The catalog reflects availability but its entries are immutable value metadata.
    const auto catalog = s.Catalog();
    plugin::PluginServiceInfo copy = catalog[static_cast<std::size_t>(plugin::PluginServiceType::Inventory)];
    EXPECT_FALSE(copy.available);
    copy.available = true;                 // mutating the copy does not affect the surface
    EXPECT_FALSE(s.IsAvailable(plugin::PluginServiceType::Inventory));
    ASSERT_EQ(catalog.size(), 7u);
    EXPECT_STREQ(catalog[static_cast<std::size_t>(plugin::PluginServiceType::Inventory)].name, "Inventory");
}

TEST(PluginHostSurfaceStep9, SurfaceExposesFacadeBundleWithoutExecuting)
{
    lua::RecordingScriptApi api;
    plugin::GameplayHostSurface surface(api.AsSet());
    const plugin::IPluginHostSurface& s = surface;

    // The exposed bundle references the same underlying facades (exposure, not execution).
    const lua::ScriptApiSet exposed = s.Services();
    EXPECT_EQ(&exposed.world, static_cast<lua::IWorldApi*>(&api));
    EXPECT_EQ(&exposed.logging, static_cast<lua::ILoggingApi*>(&api));
    // The seam performed no service call: the recorder observed nothing.
    EXPECT_TRUE(api.logCalls.empty());
    EXPECT_TRUE(api.setWeatherCalls.empty());
}

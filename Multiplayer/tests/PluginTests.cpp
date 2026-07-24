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
#include "stalkermp/plugin/FaultIsolation.h"
#include "stalkermp/plugin/IPlugin.h"
#include "stalkermp/plugin/IPluginSource.h"
#include "stalkermp/plugin/InMemoryPluginSource.h"
#include "stalkermp/plugin/NullPluginSource.h"
#include "stalkermp/plugin/PluginConfiguration.h"
#include "stalkermp/plugin/PluginContext.h"
#include "stalkermp/plugin/PluginDiagnostics.h"
#include "stalkermp/plugin/PluginHost.h"
#include "stalkermp/plugin/PluginHostSurface.h"
#include "stalkermp/plugin/PluginLifecycle.h"
#include "stalkermp/plugin/PluginLoader.h"
#include "stalkermp/plugin/PluginManager.h"
#include "stalkermp/plugin/PluginRegistry.h"
#include "stalkermp/plugin/PluginThreadGuard.h"
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

// ============================================================================
// Step 10 — PluginLifecycle state machine
// ============================================================================

TEST(PluginLifecycleStep10, InitialStateAndLegalChain)
{
    plugin::PluginState s = plugin::PluginState::Discovered; // initial
    EXPECT_EQ(plugin::PluginLifecycle::Apply(s, plugin::LifecycleAction::Validate), plugin::PluginOutcome::Ok);
    EXPECT_EQ(s, plugin::PluginState::Validated);
    EXPECT_EQ(plugin::PluginLifecycle::Apply(s, plugin::LifecycleAction::Load), plugin::PluginOutcome::Ok);
    EXPECT_EQ(s, plugin::PluginState::Loaded);
    EXPECT_EQ(plugin::PluginLifecycle::Apply(s, plugin::LifecycleAction::Initialize), plugin::PluginOutcome::Ok);
    EXPECT_EQ(s, plugin::PluginState::Initialized);
    EXPECT_EQ(plugin::PluginLifecycle::Apply(s, plugin::LifecycleAction::Activate), plugin::PluginOutcome::Ok);
    EXPECT_EQ(s, plugin::PluginState::Active);
    EXPECT_EQ(plugin::PluginLifecycle::Apply(s, plugin::LifecycleAction::Suspend), plugin::PluginOutcome::Ok);
    EXPECT_EQ(s, plugin::PluginState::Suspended);
    EXPECT_EQ(plugin::PluginLifecycle::Apply(s, plugin::LifecycleAction::Resume), plugin::PluginOutcome::Ok);
    EXPECT_EQ(s, plugin::PluginState::Active);
    EXPECT_EQ(plugin::PluginLifecycle::Apply(s, plugin::LifecycleAction::Shutdown), plugin::PluginOutcome::Ok);
    EXPECT_EQ(s, plugin::PluginState::Shutdown);
    EXPECT_EQ(plugin::PluginLifecycle::Apply(s, plugin::LifecycleAction::Unload), plugin::PluginOutcome::Ok);
    EXPECT_EQ(s, plugin::PluginState::Unloaded);
}

TEST(PluginLifecycleStep10, IllegalTransitionsRejectedStateUnchanged)
{
    plugin::PluginState s = plugin::PluginState::Discovered;
    // Cannot Initialize/Activate before Validate+Load.
    EXPECT_EQ(plugin::PluginLifecycle::Apply(s, plugin::LifecycleAction::Initialize), plugin::PluginOutcome::RuntimeError);
    EXPECT_EQ(s, plugin::PluginState::Discovered); // unchanged
    EXPECT_FALSE(plugin::PluginLifecycle::IsLegal(plugin::PluginState::Discovered, plugin::LifecycleAction::Activate));
    // Resume only from Suspended.
    EXPECT_FALSE(plugin::PluginLifecycle::IsLegal(plugin::PluginState::Active, plugin::LifecycleAction::Resume));
    // Unloaded is terminal.
    EXPECT_FALSE(plugin::PluginLifecycle::IsLegal(plugin::PluginState::Unloaded, plugin::LifecycleAction::Unload));
}

TEST(PluginLifecycleStep10, NextQueryAndFailedRecovery)
{
    const auto next = plugin::PluginLifecycle::Next(plugin::PluginState::Loaded, plugin::LifecycleAction::Initialize);
    ASSERT_TRUE(next.HasValue());
    EXPECT_EQ(next.Value(), plugin::PluginState::Initialized);
    EXPECT_FALSE(plugin::PluginLifecycle::Next(plugin::PluginState::Discovered, plugin::LifecycleAction::Activate)
                     .HasValue());
    // A Failed plugin may be shut down and unloaded.
    EXPECT_TRUE(plugin::PluginLifecycle::IsLegal(plugin::PluginState::Failed, plugin::LifecycleAction::Shutdown));
    EXPECT_TRUE(plugin::PluginLifecycle::IsLegal(plugin::PluginState::Failed, plugin::LifecycleAction::Unload));
}

// ============================================================================
// Step 11 — PluginLoader (discover -> validate -> register -> lifecycle to Loaded)
// ============================================================================

TEST(PluginLoaderStep11, LoadAllRegistersLoadedPlugins)
{
    plugin::InMemoryPluginSource source;
    source.Store(Manifest(10, /*min*/ 1, /*max*/ 5));
    source.Store(Manifest(20, /*min*/ 1, /*max*/ 5));
    plugin::PluginRegistry registry;
    const std::vector<std::uint32_t> availableApis; // no required apis on these manifests
    const plugin::PluginConfiguration config = EnabledConfig(/*host*/ 3, /*strict*/ true);

    plugin::PluginLoader loader(source, registry, availableApis, config);
    const plugin::PluginLoadReport report = loader.LoadAll();

    EXPECT_EQ(report.attempted, 2u);
    EXPECT_EQ(report.loaded, 2u);
    EXPECT_EQ(report.failed, 0u);
    EXPECT_EQ(registry.Count(), 2u);

    const auto* c10 = registry.Find(plugin::PluginId{10});
    ASSERT_NE(c10, nullptr);
    EXPECT_EQ(c10->state, plugin::PluginState::Loaded); // Validated -> Loaded; NOT Initialized
}

TEST(PluginLoaderStep11, ValidationFailureIsolatesPlugin)
{
    plugin::InMemoryPluginSource source;
    source.Store(Manifest(1, /*min*/ 1, /*max*/ 5));  // ok
    source.Store(Manifest(2, /*min*/ 6, /*max*/ 9));  // host 3 < min 6 -> ApiIncompatible
    source.Store(Manifest(3, /*min*/ 1, /*max*/ 5));  // ok
    plugin::PluginRegistry registry;
    const std::vector<std::uint32_t> noApis;                    // lifetime exceeds the loader
    const plugin::PluginConfiguration config = EnabledConfig(3, true); // lifetime exceeds the loader
    plugin::PluginLoader loader(source, registry, noApis, config);

    const auto report = loader.LoadAll();
    EXPECT_EQ(report.attempted, 3u);
    EXPECT_EQ(report.loaded, 2u);  // plugins 1 and 3
    EXPECT_EQ(report.failed, 1u);
    ASSERT_EQ(report.failures.size(), 1u);
    EXPECT_EQ(report.failures[0].first.value, 2u);
    EXPECT_EQ(report.failures[0].second, plugin::PluginOutcome::ApiIncompatible);
    EXPECT_TRUE(registry.Contains(plugin::PluginId{1}));
    EXPECT_FALSE(registry.Contains(plugin::PluginId{2}));
    EXPECT_TRUE(registry.Contains(plugin::PluginId{3}));
}

TEST(PluginLoaderStep11, DuplicateAgainstExistingRegistryIsolated)
{
    plugin::InMemoryPluginSource source;
    source.Store(Manifest(7, 1, 5));
    plugin::PluginRegistry registry;
    {
        plugin::PluginContext existing;
        existing.id = plugin::PluginId{7};
        ASSERT_EQ(registry.Register(existing), plugin::PluginOutcome::Ok);
    }
    const std::vector<std::uint32_t> noApis;                    // lifetime exceeds the loader
    const plugin::PluginConfiguration config = EnabledConfig(3, true); // lifetime exceeds the loader
    plugin::PluginLoader loader(source, registry, noApis, config);
    const auto report = loader.LoadAll();
    EXPECT_EQ(report.loaded, 0u);
    ASSERT_EQ(report.failures.size(), 1u);
    EXPECT_EQ(report.failures[0].second, plugin::PluginOutcome::DuplicatePlugin);
    EXPECT_EQ(registry.Count(), 1u); // unchanged (the pre-existing one)
}

TEST(PluginLoaderStep11, DisabledConfigAndEmptySource)
{
    // Disabled config -> every plugin isolated as PluginDisabled.
    {
        plugin::InMemoryPluginSource source;
        source.Store(Manifest(1, 1, 5));
        plugin::PluginRegistry registry;
        plugin::PluginConfiguration disabled = EnabledConfig(3, true);
        disabled.enabled = false;
        const std::vector<std::uint32_t> noApis; // lifetime exceeds the loader
        plugin::PluginLoader loader(source, registry, noApis, disabled);
        const auto report = loader.LoadAll();
        EXPECT_EQ(report.loaded, 0u);
        ASSERT_EQ(report.failures.size(), 1u);
        EXPECT_EQ(report.failures[0].second, plugin::PluginOutcome::PluginDisabled);
    }
    // Empty source -> nothing attempted.
    {
        plugin::InMemoryPluginSource source;
        plugin::PluginRegistry registry;
        const std::vector<std::uint32_t> noApis;                    // lifetime exceeds the loader
        const plugin::PluginConfiguration config = EnabledConfig(3, true); // lifetime exceeds the loader
        plugin::PluginLoader loader(source, registry, noApis, config);
        const auto report = loader.LoadAll();
        EXPECT_EQ(report.attempted, 0u);
        EXPECT_EQ(report.loaded, 0u);
        EXPECT_EQ(registry.Count(), 0u);
    }
}

// ============================================================================
// Step 12 — PluginHost (bind, initialize/activate, dispatch, shutdown; faults)
// ============================================================================

namespace
{
    // Register a Loaded context for `id` (the state the loader leaves plugins in).
    void RegisterLoaded(plugin::PluginRegistry& registry, std::uint64_t id)
    {
        ASSERT_EQ(registry.Register(MakeContext(id, plugin::PluginState::Loaded)), plugin::PluginOutcome::Ok);
    }

    plugin::FakePlugin MakeFake(std::uint64_t id)
    {
        plugin::FakePlugin f;
        f.descriptor.id = plugin::PluginId{id};
        f.descriptor.version = plugin::PluginVersion{1, 0, 0};
        return f;
    }
} // namespace

TEST(PluginHostStep12, BindRejectsNoneAndDuplicate)
{
    lua::RecordingScriptApi api;
    plugin::GameplayHostSurface surface(api.AsSet());
    plugin::PluginRegistry registry;
    plugin::FaultIsolation faults;
    plugin::PluginHost host(surface, registry, faults);

    plugin::FakePlugin a = MakeFake(5);
    plugin::NullPlugin none;

    EXPECT_EQ(host.Bind(plugin::PluginId{}, none), plugin::PluginOutcome::NotFound);
    EXPECT_EQ(host.Bind(plugin::PluginId{5}, a), plugin::PluginOutcome::Ok);
    EXPECT_EQ(host.Bind(plugin::PluginId{5}, a), plugin::PluginOutcome::DuplicatePlugin);
    EXPECT_EQ(host.BoundCount(), 1u);
}

TEST(PluginHostStep12, InitializeAllActivatesLoadedPluginsAscending)
{
    lua::RecordingScriptApi api;
    plugin::GameplayHostSurface surface(api.AsSet());
    plugin::PluginRegistry registry;
    plugin::FaultIsolation faults;
    plugin::PluginHost host(surface, registry, faults);

    RegisterLoaded(registry, 20);
    RegisterLoaded(registry, 10);
    plugin::FakePlugin p10 = MakeFake(10);
    plugin::FakePlugin p20 = MakeFake(20);
    ASSERT_EQ(host.Bind(plugin::PluginId{20}, p20), plugin::PluginOutcome::Ok);
    ASSERT_EQ(host.Bind(plugin::PluginId{10}, p10), plugin::PluginOutcome::Ok);

    const plugin::PluginHostReport report = host.InitializeAll();
    EXPECT_EQ(report.invoked, 2u);
    EXPECT_EQ(report.isolated, 0u);
    EXPECT_TRUE(p10.initialized);
    EXPECT_TRUE(p20.initialized);
    EXPECT_EQ(registry.Find(plugin::PluginId{10})->state, plugin::PluginState::Active);
    EXPECT_EQ(registry.Find(plugin::PluginId{20})->state, plugin::PluginState::Active);
    EXPECT_EQ(host.ActiveCount(), 2u);
}

TEST(PluginHostStep12, InitializeFaultIsolatesOnlyTheOffender)
{
    lua::RecordingScriptApi api;
    plugin::GameplayHostSurface surface(api.AsSet());
    plugin::PluginRegistry registry;
    plugin::FaultIsolation faults;
    plugin::PluginHost host(surface, registry, faults);

    RegisterLoaded(registry, 1);
    RegisterLoaded(registry, 2);
    plugin::FakePlugin good = MakeFake(1);
    plugin::FakePlugin bad = MakeFake(2);
    bad.initOutcome = plugin::PluginOutcome::InitFailed;
    ASSERT_EQ(host.Bind(plugin::PluginId{1}, good), plugin::PluginOutcome::Ok);
    ASSERT_EQ(host.Bind(plugin::PluginId{2}, bad), plugin::PluginOutcome::Ok);

    const plugin::PluginHostReport report = host.InitializeAll();
    EXPECT_EQ(report.invoked, 1u);
    EXPECT_EQ(report.isolated, 1u);
    EXPECT_FALSE(host.IsIsolated(plugin::PluginId{1}));
    EXPECT_TRUE(host.IsIsolated(plugin::PluginId{2}));
    EXPECT_EQ(registry.Find(plugin::PluginId{1})->state, plugin::PluginState::Active);
    EXPECT_EQ(registry.Find(plugin::PluginId{2})->state, plugin::PluginState::Failed);
}

TEST(PluginHostStep12, DispatchInvokeAndShutdown)
{
    lua::RecordingScriptApi api;
    plugin::GameplayHostSurface surface(api.AsSet());
    plugin::PluginRegistry registry;
    plugin::FaultIsolation faults;
    plugin::PluginHost host(surface, registry, faults);

    RegisterLoaded(registry, 1);
    RegisterLoaded(registry, 2);
    plugin::FakePlugin p1 = MakeFake(1);
    plugin::FakePlugin p2 = MakeFake(2);
    p2.eventOutcome = plugin::PluginOutcome::RuntimeError; // faults on first event
    ASSERT_EQ(host.Bind(plugin::PluginId{1}, p1), plugin::PluginOutcome::Ok);
    ASSERT_EQ(host.Bind(plugin::PluginId{2}, p2), plugin::PluginOutcome::Ok);
    (void)host.InitializeAll(); // both Active

    // Dispatch: p1 handles Ok, p2 faults -> isolated; siblings continue.
    const plugin::PluginHostReport d = host.Dispatch(plugin::PluginEvent::OnTick, plugin::PluginArgs{});
    EXPECT_EQ(d.invoked, 1u);
    EXPECT_EQ(d.isolated, 1u);
    EXPECT_TRUE(host.IsIsolated(plugin::PluginId{2}));

    // Invoke on the isolated plugin -> PluginDisabled skip; unbound -> NotFound.
    EXPECT_EQ(host.Invoke(plugin::PluginId{2}, plugin::PluginEvent::OnTick, plugin::PluginArgs{}),
              plugin::PluginOutcome::PluginDisabled);
    EXPECT_EQ(host.Invoke(plugin::PluginId{99}, plugin::PluginEvent::OnTick, plugin::PluginArgs{}),
              plugin::PluginOutcome::NotFound);
    // Invoke on the healthy active plugin -> Ok.
    EXPECT_EQ(host.Invoke(plugin::PluginId{1}, plugin::PluginEvent::OnPlayerJoin, plugin::PluginArgs{}),
              plugin::PluginOutcome::Ok);

    host.ShutdownAll();
    EXPECT_TRUE(p1.shutdownCalled);
    EXPECT_TRUE(p2.shutdownCalled); // isolated (Failed) plugins are still shut down
    EXPECT_EQ(registry.Find(plugin::PluginId{1})->state, plugin::PluginState::Shutdown);
    EXPECT_EQ(registry.Find(plugin::PluginId{2})->state, plugin::PluginState::Shutdown);
}

// ============================================================================
// Step 13 — PluginManager (orchestrate loader + host; startup/tick/shutdown)
// ============================================================================

TEST(PluginManagerStep13, StartupLoadsAndActivatesBoundPlugins)
{
    plugin::InMemoryPluginSource source;
    source.Store(Manifest(10, /*min*/ 1, /*max*/ 5));
    source.Store(Manifest(20, /*min*/ 1, /*max*/ 5));
    lua::RecordingScriptApi api;
    plugin::GameplayHostSurface surface(api.AsSet());
    const std::vector<std::uint32_t> noApis;
    const plugin::PluginConfiguration config = EnabledConfig(/*host*/ 3, /*strict*/ true);
    plugin::PluginManager manager(source, surface, noApis, config);

    plugin::FakePlugin p10 = MakeFake(10);
    plugin::FakePlugin p20 = MakeFake(20);
    ASSERT_EQ(manager.BindPlugin(plugin::PluginId{10}, p10), plugin::PluginOutcome::Ok);
    ASSERT_EQ(manager.BindPlugin(plugin::PluginId{20}, p20), plugin::PluginOutcome::Ok);

    const plugin::PluginStartupReport report = manager.Startup();
    EXPECT_EQ(report.discovered, 2u);
    EXPECT_EQ(report.loaded, 2u);
    EXPECT_EQ(report.initialized, 2u);
    EXPECT_EQ(report.isolated, 0u);
    EXPECT_EQ(manager.PluginCount(), 2u);
    EXPECT_TRUE(p10.initialized);
    EXPECT_TRUE(p20.initialized);
}

TEST(PluginManagerStep13, SubscribeDispatchAndTick)
{
    plugin::InMemoryPluginSource source;
    source.Store(Manifest(1, 1, 5));
    source.Store(Manifest(2, 1, 5));
    lua::RecordingScriptApi api;
    plugin::GameplayHostSurface surface(api.AsSet());
    const std::vector<std::uint32_t> noApis;
    const plugin::PluginConfiguration config = EnabledConfig(3, true);
    plugin::PluginManager manager(source, surface, noApis, config);

    plugin::FakePlugin p1 = MakeFake(1);
    plugin::FakePlugin p2 = MakeFake(2);
    p2.eventOutcome = plugin::PluginOutcome::RuntimeError;
    ASSERT_EQ(manager.BindPlugin(plugin::PluginId{1}, p1), plugin::PluginOutcome::Ok);
    ASSERT_EQ(manager.BindPlugin(plugin::PluginId{2}, p2), plugin::PluginOutcome::Ok);
    (void)manager.Startup();

    // Subscribe both to OnTick; none id rejected.
    EXPECT_EQ(manager.Subscribe(plugin::PluginEvent::OnTick, plugin::PluginId{}), plugin::PluginOutcome::InvalidHook);
    ASSERT_EQ(manager.Subscribe(plugin::PluginEvent::OnTick, plugin::PluginId{1}), plugin::PluginOutcome::Ok);
    ASSERT_EQ(manager.Subscribe(plugin::PluginEvent::OnTick, plugin::PluginId{2}), plugin::PluginOutcome::Ok);

    const plugin::PluginHostReport d = manager.DispatchEvent(plugin::PluginEvent::OnTick, plugin::PluginArgs{});
    EXPECT_EQ(d.invoked, 1u);   // p1 ok
    EXPECT_EQ(d.isolated, 1u);  // p2 faulted this dispatch
    EXPECT_TRUE(manager.IsIsolated(plugin::PluginId{2}));
    EXPECT_EQ(manager.IsolatedCount(), 1u);

    // Tick dispatches OnTick again deterministically; p2 already isolated -> skipped.
    const std::uint64_t before = manager.Ticks();
    manager.Tick(0.016);
    EXPECT_EQ(manager.Ticks(), before + 1u);
}

TEST(PluginManagerStep13, ServiceContractAndShutdown)
{
    plugin::InMemoryPluginSource source;
    source.Store(Manifest(1, 1, 5));
    lua::RecordingScriptApi api;
    plugin::GameplayHostSurface surface(api.AsSet());
    const std::vector<std::uint32_t> noApis;
    const plugin::PluginConfiguration config = EnabledConfig(3, true);
    plugin::PluginManager manager(source, surface, noApis, config);

    EXPECT_EQ(manager.Name(), "Plugins");
    const auto deps = manager.Dependencies();
    EXPECT_FALSE(deps.empty());
    EXPECT_TRUE(manager.Initialize().HasValue());

    plugin::FakePlugin p1 = MakeFake(1);
    ASSERT_EQ(manager.BindPlugin(plugin::PluginId{1}, p1), plugin::PluginOutcome::Ok);
    (void)manager.Startup();
    EXPECT_TRUE(p1.initialized);

    manager.Shutdown();
    EXPECT_TRUE(p1.shutdownCalled);
    EXPECT_EQ(manager.IsolatedCount(), 0u); // cleared on shutdown
}

// ============================================================================
// Step 14 — FaultIsolation (deterministic isolation policy)
// ============================================================================

TEST(PluginHardeningStep14, IsolateMarksFailedAndRecordsIdempotent)
{
    plugin::PluginRegistry registry;
    RegisterLoaded(registry, 5);
    plugin::FaultIsolation faults;

    EXPECT_FALSE(faults.IsIsolated(plugin::PluginId{5}));
    EXPECT_EQ(faults.Isolate(plugin::PluginId{5}, registry), plugin::PluginOutcome::PluginDisabled);
    EXPECT_TRUE(faults.IsIsolated(plugin::PluginId{5}));
    EXPECT_EQ(registry.Find(plugin::PluginId{5})->state, plugin::PluginState::Failed);
    EXPECT_EQ(faults.Count(), 1u);

    // Idempotent: re-isolating is a no-op that still reports PluginDisabled.
    EXPECT_EQ(faults.Isolate(plugin::PluginId{5}, registry), plugin::PluginOutcome::PluginDisabled);
    EXPECT_EQ(faults.Count(), 1u);
}

TEST(PluginHardeningStep14, IsolationIsSortedAndClearable)
{
    plugin::PluginRegistry registry;
    plugin::FaultIsolation faults;

    // Insert out of order; disabled set stays sorted/deduplicated internally.
    (void)faults.Isolate(plugin::PluginId{30}, registry);
    (void)faults.Isolate(plugin::PluginId{10}, registry);
    (void)faults.Isolate(plugin::PluginId{20}, registry);
    EXPECT_EQ(faults.Count(), 3u);
    EXPECT_TRUE(faults.IsIsolated(plugin::PluginId{10}));
    EXPECT_TRUE(faults.IsIsolated(plugin::PluginId{20}));
    EXPECT_TRUE(faults.IsIsolated(plugin::PluginId{30}));
    EXPECT_FALSE(faults.IsIsolated(plugin::PluginId{25}));

    faults.Clear();
    EXPECT_EQ(faults.Count(), 0u);
    EXPECT_FALSE(faults.IsIsolated(plugin::PluginId{10}));
}

TEST(PluginHardeningStep14, IsolateMissingRegistryEntryStillRecords)
{
    plugin::PluginRegistry registry; // empty — id not present
    plugin::FaultIsolation faults;

    // No registry context to mark Failed, but the plugin is still recorded disabled.
    EXPECT_EQ(faults.Isolate(plugin::PluginId{42}, registry), plugin::PluginOutcome::PluginDisabled);
    EXPECT_TRUE(faults.IsIsolated(plugin::PluginId{42}));
    EXPECT_EQ(faults.Count(), 1u);
    EXPECT_EQ(registry.Count(), 0u);
}

// ============================================================================
// Step 15 — PluginDiagnostics (non-invasive collector + read-only inspectors)
// ============================================================================

TEST(PluginDiagnosticsStep15, CountersAreMonotonicAndSnapshotIsAValueCopy)
{
    plugin::PluginDiagnostics diag;
    plugin::PluginStatistics zero = diag.Snapshot();
    EXPECT_EQ(zero.loadedPlugins, 0u);
    EXPECT_EQ(zero.activePlugins, 0u);
    EXPECT_EQ(zero.eventDispatches, 0u);
    EXPECT_EQ(zero.apiCalls, 0u);
    EXPECT_EQ(zero.pluginErrors, 0u);

    diag.RecordPluginLoaded();
    diag.RecordPluginLoaded();
    diag.RecordPluginActivated();
    diag.RecordEventDispatch();
    diag.RecordEventDispatch();
    diag.RecordEventDispatch();
    diag.RecordApiCall();
    diag.RecordPluginError();

    const plugin::PluginStatistics s = diag.Snapshot();
    EXPECT_EQ(s.loadedPlugins, 2u);
    EXPECT_EQ(s.activePlugins, 1u);
    EXPECT_EQ(s.eventDispatches, 3u);
    EXPECT_EQ(s.apiCalls, 1u);
    EXPECT_EQ(s.pluginErrors, 1u);

    // The earlier snapshot is an independent value copy (non-invasive).
    EXPECT_EQ(zero.loadedPlugins, 0u);

    // Decrement counters saturate at zero (never underflow).
    diag.RecordPluginUnloaded();
    diag.RecordPluginUnloaded();
    diag.RecordPluginUnloaded(); // extra — must clamp
    diag.RecordPluginDeactivated();
    diag.RecordPluginDeactivated(); // extra — must clamp
    const plugin::PluginStatistics s2 = diag.Snapshot();
    EXPECT_EQ(s2.loadedPlugins, 0u);
    EXPECT_EQ(s2.activePlugins, 0u);

    diag.Reset();
    EXPECT_EQ(diag.Snapshot().eventDispatches, 0u);
}

TEST(PluginDiagnosticsStep15, DiagnosticTimingIsDeterministicAndNeverGates)
{
    plugin::PluginDiagnostics diag;
    EXPECT_EQ(diag.AverageExecutionMicros(), 0u); // no samples

    diag.RecordExecutionTime(100);
    diag.RecordExecutionTime(300);
    EXPECT_EQ(diag.AverageExecutionMicros(), 200u); // deterministic integer average
    EXPECT_EQ(diag.Snapshot().executionTimeMicros, 300u); // last sample

    diag.RecordMemory(4096);
    EXPECT_EQ(diag.Snapshot().memoryBytes, 4096u);
}

TEST(PluginDiagnosticsStep15, InspectStatesReportsLifecycleAndIsolationReadOnly)
{
    plugin::PluginRegistry registry;
    ASSERT_EQ(registry.Register(MakeContext(30, plugin::PluginState::Active)), plugin::PluginOutcome::Ok);
    ASSERT_EQ(registry.Register(MakeContext(10, plugin::PluginState::Loaded)), plugin::PluginOutcome::Ok);
    ASSERT_EQ(registry.Register(MakeContext(20, plugin::PluginState::Failed)), plugin::PluginOutcome::Ok);
    plugin::FaultIsolation faults;
    (void)faults.Isolate(plugin::PluginId{10}, registry); // also flips 10 -> Failed

    const std::size_t countBefore = registry.Count();
    const auto states = plugin::PluginDiagnostics::InspectStates(registry, faults);

    // Ascending by id; read-only (registry size unchanged).
    ASSERT_EQ(states.size(), 3u);
    EXPECT_EQ(states[0].id.value, 10u);
    EXPECT_EQ(states[1].id.value, 20u);
    EXPECT_EQ(states[2].id.value, 30u);
    EXPECT_TRUE(states[0].isolated);  // isolated via FaultIsolation (and now Failed)
    EXPECT_TRUE(states[1].isolated);  // Failed state
    EXPECT_FALSE(states[2].isolated); // Active, not isolated
    EXPECT_EQ(states[2].state, plugin::PluginState::Active);
    EXPECT_EQ(registry.Count(), countBefore); // inspection mutated nothing

    EXPECT_EQ(plugin::PluginDiagnostics::CountInState(registry, plugin::PluginState::Failed), 2u);
    EXPECT_EQ(plugin::PluginDiagnostics::CountInState(registry, plugin::PluginState::Active), 1u);
}

TEST(PluginDiagnosticsStep15, InspectConsistencyDetectsActiveAndIsolatedBreach)
{
    plugin::PluginRegistry registry;
    ASSERT_EQ(registry.Register(MakeContext(1, plugin::PluginState::Active)), plugin::PluginOutcome::Ok);
    ASSERT_EQ(registry.Register(MakeContext(2, plugin::PluginState::Loaded)), plugin::PluginOutcome::Ok);
    plugin::EventBinding events;
    ASSERT_EQ(events.Bind(plugin::PluginEvent::OnTick, plugin::PluginId{1}), plugin::PluginOutcome::Ok);
    plugin::FaultIsolation faults;

    // Consistent baseline: one active (id 1), one loaded, one bound hook.
    plugin::PluginConsistency c = plugin::PluginDiagnostics::InspectConsistency(registry, events, faults);
    EXPECT_EQ(c.registeredPlugins, 2u);
    EXPECT_EQ(c.boundHooks, 1u);
    EXPECT_EQ(c.activePlugins, 1u);
    EXPECT_TRUE(c.consistent);

    // Force an invariant breach: id 1 is Active but recorded isolated in FaultIsolation.
    // (Isolate would normally flip it to Failed; simulate a stale record by isolating a
    // *different* absent id would not breach — so isolate 1 but restore its Active state to
    // model the "active-and-isolated" invariant the inspector must catch.)
    (void)faults.Isolate(plugin::PluginId{1}, registry);
    if (plugin::PluginContext* ctx = registry.Find(plugin::PluginId{1}))
    {
        ctx->state = plugin::PluginState::Active; // stale: active yet isolated
    }
    c = plugin::PluginDiagnostics::InspectConsistency(registry, events, faults);
    EXPECT_FALSE(c.consistent);
}

// ============================================================================
// Step 16 — PluginThreadGuard (Simulation-Thread-only enforcement; ADR-011)
// ============================================================================

TEST(PluginThreadSafetyStep16, ApprovedThreadSucceedsNonApprovedRejected)
{
    plugin::PluginThreadGuard guard;
    constexpr std::uint64_t kOwner = 0xABCDEF01u;
    constexpr std::uint64_t kWorker = 0x1234u;

    // Unbound: every entry is a violation (fails closed).
    EXPECT_FALSE(guard.IsBound());
    EXPECT_EQ(guard.Verify(kOwner), plugin::PluginOutcome::RuntimeError);

    guard.Bind(kOwner);
    EXPECT_TRUE(guard.IsBound());
    EXPECT_TRUE(guard.IsOnOwner(kOwner));
    EXPECT_FALSE(guard.IsOnOwner(kWorker));

    // Approved (Simulation Thread) entry succeeds; worker entry is rejected as a value outcome.
    EXPECT_EQ(guard.Verify(kOwner), plugin::PluginOutcome::Ok);
    EXPECT_EQ(guard.Verify(kWorker), plugin::PluginOutcome::RuntimeError);
}

TEST(PluginThreadSafetyStep16, ViolationsCountedDeterministicallyAndResettable)
{
    plugin::PluginThreadGuard guard;
    guard.Bind(7);

    EXPECT_EQ(guard.Verify(7), plugin::PluginOutcome::Ok);      // no violation
    EXPECT_EQ(guard.Verify(8), plugin::PluginOutcome::RuntimeError);
    EXPECT_EQ(guard.Verify(9), plugin::PluginOutcome::RuntimeError);
    EXPECT_EQ(guard.Violations(), 2u); // deterministic count; Ok never counted

    guard.Reset();
    EXPECT_FALSE(guard.IsBound());
    EXPECT_EQ(guard.Violations(), 0u);
    // After reset, still fails closed until re-bound.
    EXPECT_EQ(guard.Verify(7), plugin::PluginOutcome::RuntimeError);
    EXPECT_EQ(guard.Violations(), 1u);
}

TEST(PluginThreadSafetyStep16, GuardIsTriviallyValueTypedNoThreadingLeakage)
{
    // The guard is a pure value component: trivially copyable, no threading primitive state.
    EXPECT_TRUE(std::is_trivially_copyable_v<plugin::PluginThreadGuard>);

    // A copy carries the bound owner but is otherwise independent (no shared/atomic state).
    plugin::PluginThreadGuard a;
    a.Bind(42);
    plugin::PluginThreadGuard b = a;
    EXPECT_TRUE(b.IsOnOwner(42));
    EXPECT_EQ(b.Verify(42), plugin::PluginOutcome::Ok);
    EXPECT_EQ(a.Violations(), 0u); // independent instances
}

// ============================================================================
// Step 17 — Composed integration (PluginManager + PluginHost + host surface)
//
// These exercise the composed subsystem the way Bootstrap wires it (Step 17): a
// PluginManager loads statically-registered manifests from the discovery source and,
// through the PluginHost, initializes/activates the bound plugin instances and dispatches
// events/OnTick over the reused Sprint-013 gameplay host surface ([AR-P3] Option A) — all
// engine-free (in-memory source + fake plugins + recording facades). The real static-
// registration backend + engine host facades are Antigravity-smoke-verified on Windows.
// ============================================================================

TEST(PluginIntegrationStep17, StartupLoadsManifestsAndActivatesBoundPlugins)
{
    plugin::InMemoryPluginSource source;
    source.Store(Manifest(10, /*min*/ 1, /*max*/ 5));
    source.Store(Manifest(20, /*min*/ 1, /*max*/ 5));
    lua::RecordingScriptApi api; // reused gameplay facades ([AR-P3] Option A)
    plugin::GameplayHostSurface surface(api.AsSet());
    const std::vector<std::uint32_t> noApis;
    const plugin::PluginConfiguration config = EnabledConfig(/*host*/ 3, /*strict*/ true);
    plugin::PluginManager manager(source, surface, noApis, config);

    plugin::FakePlugin p10 = MakeFake(10);
    plugin::FakePlugin p20 = MakeFake(20);
    ASSERT_EQ(manager.BindPlugin(plugin::PluginId{10}, p10), plugin::PluginOutcome::Ok);
    ASSERT_EQ(manager.BindPlugin(plugin::PluginId{20}, p20), plugin::PluginOutcome::Ok);

    const plugin::PluginStartupReport report = manager.Startup();
    EXPECT_EQ(report.discovered, 2u);
    EXPECT_EQ(report.loaded, 2u);
    EXPECT_EQ(report.initialized, 2u);
    EXPECT_EQ(report.isolated, 0u);
    EXPECT_TRUE(p10.initialized);
    EXPECT_TRUE(p20.initialized);
}

TEST(PluginIntegrationStep17, ServerAndGameplayEventFlow)
{
    plugin::InMemoryPluginSource source;
    source.Store(Manifest(1, 1, 5));
    source.Store(Manifest(2, 1, 5));
    lua::RecordingScriptApi api;
    plugin::GameplayHostSurface surface(api.AsSet());
    const std::vector<std::uint32_t> noApis;
    const plugin::PluginConfiguration config = EnabledConfig(3, true);
    plugin::PluginManager manager(source, surface, noApis, config);

    plugin::FakePlugin p1 = MakeFake(1);
    plugin::FakePlugin p2 = MakeFake(2);
    ASSERT_EQ(manager.BindPlugin(plugin::PluginId{1}, p1), plugin::PluginOutcome::Ok);
    ASSERT_EQ(manager.BindPlugin(plugin::PluginId{2}, p2), plugin::PluginOutcome::Ok);
    ASSERT_EQ(manager.Startup().initialized, 2u);

    ASSERT_EQ(manager.Subscribe(plugin::PluginEvent::OnServerStart, plugin::PluginId{1}), plugin::PluginOutcome::Ok);
    ASSERT_EQ(manager.Subscribe(plugin::PluginEvent::OnPlayerJoin, plugin::PluginId{2}), plugin::PluginOutcome::Ok);

    EXPECT_EQ(manager.DispatchEvent(plugin::PluginEvent::OnServerStart, plugin::PluginArgs{}).invoked, 1u);
    EXPECT_EQ(manager.DispatchEvent(plugin::PluginEvent::OnPlayerJoin, plugin::PluginArgs{}).invoked, 1u);
    EXPECT_EQ(manager.DispatchEvent(plugin::PluginEvent::OnPlayerLeave, plugin::PluginArgs{}).invoked, 0u); // none bound
    EXPECT_EQ(p1.events.size(), 1u);
    EXPECT_EQ(p2.events.size(), 1u);
}

TEST(PluginIntegrationStep17, LargePluginCollectionTickDispatch)
{
    plugin::InMemoryPluginSource source;
    for (std::uint64_t id = 1; id <= 200; ++id)
    {
        source.Store(Manifest(id, 1, 5));
    }
    lua::RecordingScriptApi api;
    plugin::GameplayHostSurface surface(api.AsSet());
    const std::vector<std::uint32_t> noApis;
    const plugin::PluginConfiguration config = EnabledConfig(3, true);
    plugin::PluginManager manager(source, surface, noApis, config);

    std::vector<plugin::FakePlugin> plugins;
    plugins.reserve(200);
    for (std::uint64_t id = 1; id <= 200; ++id)
    {
        plugins.push_back(MakeFake(id));
    }
    for (std::uint64_t id = 1; id <= 200; ++id)
    {
        ASSERT_EQ(manager.BindPlugin(plugin::PluginId{id}, plugins[id - 1]), plugin::PluginOutcome::Ok);
    }
    EXPECT_EQ(manager.Startup().initialized, 200u);

    for (std::uint64_t id = 1; id <= 200; ++id)
    {
        ASSERT_EQ(manager.Subscribe(plugin::PluginEvent::OnTick, plugin::PluginId{id}), plugin::PluginOutcome::Ok);
    }
    manager.Tick(0.016);
    // All 200 reacted deterministically (one OnTick each).
    for (const plugin::FakePlugin& p : plugins)
    {
        EXPECT_EQ(p.events.size(), 1u);
    }
    EXPECT_EQ(manager.Ticks(), 1u);
}

TEST(PluginIntegrationStep17, FaultIsolationAcrossTicks)
{
    plugin::InMemoryPluginSource source;
    source.Store(Manifest(1, 1, 5));
    source.Store(Manifest(2, 1, 5));
    source.Store(Manifest(3, 1, 5));
    lua::RecordingScriptApi api;
    plugin::GameplayHostSurface surface(api.AsSet());
    const std::vector<std::uint32_t> noApis;
    const plugin::PluginConfiguration config = EnabledConfig(3, true);
    plugin::PluginManager manager(source, surface, noApis, config);

    plugin::FakePlugin p1 = MakeFake(1);
    plugin::FakePlugin p2 = MakeFake(2);
    plugin::FakePlugin p3 = MakeFake(3);
    p2.eventOutcome = plugin::PluginOutcome::RuntimeError; // faults on its first OnTick
    ASSERT_EQ(manager.BindPlugin(plugin::PluginId{1}, p1), plugin::PluginOutcome::Ok);
    ASSERT_EQ(manager.BindPlugin(plugin::PluginId{2}, p2), plugin::PluginOutcome::Ok);
    ASSERT_EQ(manager.BindPlugin(plugin::PluginId{3}, p3), plugin::PluginOutcome::Ok);
    ASSERT_EQ(manager.Startup().initialized, 3u);
    for (std::uint64_t id = 1; id <= 3; ++id)
    {
        ASSERT_EQ(manager.Subscribe(plugin::PluginEvent::OnTick, plugin::PluginId{id}), plugin::PluginOutcome::Ok);
    }

    manager.Tick(0.016); // Tick 1: dispatches OnTick, isolates the faulting plugin (id 2)
    EXPECT_TRUE(manager.IsIsolated(plugin::PluginId{2}));
    EXPECT_FALSE(manager.IsIsolated(plugin::PluginId{1}));
    EXPECT_FALSE(manager.IsIsolated(plugin::PluginId{3}));

    // Subsequent ticks keep running the healthy siblings, skipping the isolated one.
    const std::size_t p1Before = p1.events.size();
    const std::size_t p3Before = p3.events.size();
    manager.Tick(0.016);
    manager.Tick(0.016);
    EXPECT_EQ(p1.events.size() - p1Before, 2u); // two more ticks
    EXPECT_EQ(p3.events.size() - p3Before, 2u);
    EXPECT_EQ(manager.Ticks(), 3u);
    EXPECT_EQ(manager.IsolatedCount(), 1u);
}

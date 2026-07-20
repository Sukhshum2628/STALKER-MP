// STALKER-MP — Lua Integration tests (Sprint-013)
//
// Batch-1 (Steps 01-02): the engine-free / VM-free / OS-free value vocabulary
// (`LuaScriptTypes.h`) and the `[lua]` configuration parser (`LuaConfiguration`).
// Types-and-config only — no runtime, loading, validation, dispatch, or wiring.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include <cstddef>
#include <vector>

#include <cstdio>
#include <filesystem>
#include <string>

#include "stalkermp/adapters/PlatformScriptStore.h"
#include "stalkermp/core/Config.h"
#include "stalkermp/lua/EventBinding.h"
#include "stalkermp/lua/ILuaRuntime.h"
#include "stalkermp/lua/IScriptSource.h"
#include "stalkermp/lua/InMemoryScriptSource.h"
#include "stalkermp/lua/LuaConfiguration.h"
#include "stalkermp/lua/LuaScriptTypes.h"
#include "stalkermp/lua/NullScriptSource.h"
#include "stalkermp/lua/ScriptContext.h"
#include "stalkermp/lua/ScriptRegistry.h"
#include "stalkermp/lua/ScriptValidator.h"

namespace
{
    using namespace stalkermp;
} // namespace

// ============================================================================
// Step 01 — LuaScriptTypes (value vocabulary)
// ============================================================================

TEST(LuaTypesStep1, EnumsHaveUint8UnderlyingType)
{
    EXPECT_TRUE((std::is_same_v<std::underlying_type_t<lua::ScriptOutcome>, std::uint8_t>));
    EXPECT_TRUE((std::is_same_v<std::underlying_type_t<lua::ScriptState>, std::uint8_t>));
    EXPECT_TRUE((std::is_same_v<std::underlying_type_t<lua::ScriptEvent>, std::uint8_t>));
    // Reserved 0 is the neutral value in each enumeration.
    EXPECT_EQ(static_cast<std::uint8_t>(lua::ScriptOutcome::Ok), 0u);
    EXPECT_EQ(static_cast<std::uint8_t>(lua::ScriptState::Unloaded), 0u);
    EXPECT_EQ(static_cast<std::uint8_t>(lua::ScriptEvent::OnServerStart), 0u);
}

TEST(LuaTypesStep1, ValueShapesAreTriviallyCopyable)
{
    EXPECT_TRUE(std::is_trivially_copyable_v<lua::ScriptId>);
    EXPECT_TRUE(std::is_trivially_copyable_v<lua::CallbackId>);
    EXPECT_TRUE(std::is_trivially_copyable_v<lua::ScriptDescriptor>);
    EXPECT_TRUE(std::is_trivially_copyable_v<lua::ScriptCallResult>);
    EXPECT_TRUE(std::is_trivially_copyable_v<lua::ScriptStatistics>);
    EXPECT_TRUE(std::is_trivially_copyable_v<lua::ScriptConsistency>);
}

TEST(LuaTypesStep1, DefaultConstructionIsZeroedNeutral)
{
    lua::ScriptDescriptor d{};
    EXPECT_TRUE(d.id.IsNone());
    EXPECT_EQ(d.apiVersion, 0u);
    EXPECT_EQ(d.generation, 0u);

    lua::ScriptCallResult r{};
    EXPECT_EQ(r.outcome, lua::ScriptOutcome::Ok);
    EXPECT_EQ(r.returnValue, 0u);

    lua::ScriptStatistics s{};
    EXPECT_EQ(s.loadedScripts, 0u);
    EXPECT_EQ(s.executionTimeMicros, 0u);
    EXPECT_EQ(s.memoryBytes, 0u);
    EXPECT_EQ(s.apiCalls, 0u);
    EXPECT_EQ(s.callbackCount, 0u);
    EXPECT_EQ(s.scriptErrors, 0u);

    lua::ScriptConsistency c{};
    EXPECT_EQ(c.registeredScripts, 0u);
    EXPECT_EQ(c.boundCallbacks, 0u);
    EXPECT_EQ(c.activeScripts, 0u);
    EXPECT_TRUE(c.consistent);
}

TEST(LuaTypesStep1, ValueIdSemantics)
{
    lua::ScriptId a{1};
    lua::ScriptId b{2};
    lua::ScriptId a2{1};
    EXPECT_TRUE(a.IsNone() == false);
    EXPECT_TRUE(lua::ScriptId{}.IsNone());
    EXPECT_EQ(a, a2);
    EXPECT_NE(a, b);
    EXPECT_LT(a, b);

    lua::CallbackId c{5};
    EXPECT_FALSE(c.IsNone());
    EXPECT_NE(c, lua::CallbackId{6});
}

TEST(LuaTypesStep1, OutcomeNamesAreTotal)
{
    EXPECT_STREQ(lua::ScriptOutcomeName(lua::ScriptOutcome::Ok), "Ok");
    EXPECT_STREQ(lua::ScriptOutcomeName(lua::ScriptOutcome::SyntaxError), "SyntaxError");
    EXPECT_STREQ(lua::ScriptOutcomeName(lua::ScriptOutcome::RecursionLimit), "RecursionLimit");
    EXPECT_STREQ(lua::ScriptOutcomeName(lua::ScriptOutcome::ExecutionTimeout), "ExecutionTimeout");
    EXPECT_STREQ(lua::ScriptOutcomeName(lua::ScriptOutcome::NotFound), "NotFound");
    // Every enumerator maps to a non-"Unknown" name.
    for (std::uint8_t i = 0; i <= static_cast<std::uint8_t>(lua::ScriptOutcome::NotFound); ++i)
    {
        EXPECT_STRNE(lua::ScriptOutcomeName(static_cast<lua::ScriptOutcome>(i)), "Unknown");
    }
}

TEST(LuaTypesStep1, StateAndEventNamesAreTotal)
{
    EXPECT_STREQ(lua::ScriptStateName(lua::ScriptState::Executing), "Executing");
    EXPECT_STREQ(lua::ScriptStateName(lua::ScriptState::Destroyed), "Destroyed");
    for (std::uint8_t i = 0; i <= static_cast<std::uint8_t>(lua::ScriptState::Destroyed); ++i)
    {
        EXPECT_STRNE(lua::ScriptStateName(static_cast<lua::ScriptState>(i)), "Unknown");
    }

    EXPECT_STREQ(lua::ScriptEventName(lua::ScriptEvent::OnPlayerJoin), "OnPlayerJoin");
    EXPECT_STREQ(lua::ScriptEventName(lua::ScriptEvent::OnTick), "OnTick");
    for (std::uint8_t i = 0; i <= static_cast<std::uint8_t>(lua::ScriptEvent::OnTick); ++i)
    {
        EXPECT_STRNE(lua::ScriptEventName(static_cast<lua::ScriptEvent>(i)), "Unknown");
    }
}

// ============================================================================
// Step 02 — LuaConfiguration::FromConfig
// ============================================================================

TEST(LuaConfigStep2, DefaultsWhenSectionAbsent)
{
    core::ConfigStore store;
    const auto r = lua::LuaConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.scriptDirectoryToken, "scripts");
    EXPECT_EQ(c.maxScripts, 256u);
    EXPECT_EQ(c.executionBudgetMicros, 2000u);
    EXPECT_EQ(c.recursionLimit, 64u);
    EXPECT_TRUE(c.enabled);
    EXPECT_TRUE(c.validateOnLoad);
    EXPECT_TRUE(c.strictApiVersion);
    EXPECT_EQ(c.version, 1u);
}

TEST(LuaConfigStep2, OverridesParsed)
{
    core::ConfigStore store;
    store.Set("lua", "script_directory", "mods/scripts");
    store.Set("lua", "max_scripts", "512");
    store.Set("lua", "execution_budget_micros", "5000");
    store.Set("lua", "recursion_limit", "128");
    store.Set("lua", "enabled", "false");
    store.Set("lua", "validate_on_load", "false");
    store.Set("lua", "strict_api_version", "false");
    store.Set("lua", "version", "3");
    const auto r = lua::LuaConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.scriptDirectoryToken, "mods/scripts");
    EXPECT_EQ(c.maxScripts, 512u);
    EXPECT_EQ(c.executionBudgetMicros, 5000u);
    EXPECT_EQ(c.recursionLimit, 128u);
    EXPECT_FALSE(c.enabled);
    EXPECT_FALSE(c.validateOnLoad);
    EXPECT_FALSE(c.strictApiVersion);
    EXPECT_EQ(c.version, 3u);
}

TEST(LuaConfigStep2, BoundaryMinimumsAccepted)
{
    core::ConfigStore store;
    store.Set("lua", "max_scripts", "1");
    store.Set("lua", "execution_budget_micros", "1");
    store.Set("lua", "recursion_limit", "1");
    store.Set("lua", "version", "1");
    const auto r = lua::LuaConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.maxScripts, 1u);
    EXPECT_EQ(c.executionBudgetMicros, 1u);
    EXPECT_EQ(c.recursionLimit, 1u);
    EXPECT_EQ(c.version, 1u);
}

TEST(LuaConfigStep2, InvalidValuesRejected)
{
    {
        core::ConfigStore s;
        s.Set("lua", "max_scripts", "0"); // below min 1
        EXPECT_FALSE(lua::LuaConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("lua", "execution_budget_micros", "0"); // below min 1
        EXPECT_FALSE(lua::LuaConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("lua", "recursion_limit", "0"); // below min 1
        EXPECT_FALSE(lua::LuaConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("lua", "version", "0"); // below min 1
        EXPECT_FALSE(lua::LuaConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("lua", "max_scripts", "-7"); // negative
        EXPECT_FALSE(lua::LuaConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("lua", "script_directory", ""); // empty token
        EXPECT_FALSE(lua::LuaConfiguration::FromConfig(s).HasValue());
    }
}

// ============================================================================
// Step 03 — ILuaRuntime seam (FakeLuaRuntime / NullLuaRuntime)
// ============================================================================

namespace
{
    std::vector<std::byte> Bytes(std::initializer_list<int> vals)
    {
        std::vector<std::byte> b;
        b.reserve(vals.size());
        for (int v : vals)
        {
            b.push_back(static_cast<std::byte>(v));
        }
        return b;
    }
} // namespace

TEST(LuaRuntimeSeamStep3, FakeRecordsAndAssignsAscendingIds)
{
    lua::FakeLuaRuntime rt;
    EXPECT_EQ(rt.CreateState(), lua::ScriptOutcome::Ok);
    EXPECT_TRUE(rt.stateCreated);
    EXPECT_EQ(rt.LoadStandardLibraries(), lua::ScriptOutcome::Ok);
    EXPECT_TRUE(rt.librariesLoaded);

    const auto a = rt.LoadChunk("a.lua", Bytes({1, 2, 3}));
    const auto b = rt.LoadChunk("b.lua", Bytes({9}));
    ASSERT_TRUE(a.HasValue());
    ASSERT_TRUE(b.HasValue());
    EXPECT_EQ(a.Value().value, 1u); // deterministic ascending, never 0
    EXPECT_EQ(b.Value().value, 2u);
    ASSERT_EQ(rt.loads.size(), 2u);
    EXPECT_EQ(rt.loads[0].name, "a.lua");
    EXPECT_EQ(rt.loads[0].byteCount, 3u);
    EXPECT_EQ(rt.loads[1].byteCount, 1u);

    EXPECT_EQ(rt.Execute(a.Value()), lua::ScriptOutcome::Ok);
    ASSERT_EQ(rt.executions.size(), 1u);
    EXPECT_EQ(rt.executions[0].value, 1u);

    EXPECT_EQ(rt.RegisterFunction("world.get", lua::HostFunctionId{7}), lua::ScriptOutcome::Ok);
    ASSERT_EQ(rt.registrations.size(), 1u);
    EXPECT_EQ(rt.registrations[0].name, "world.get");
    EXPECT_EQ(rt.registrations[0].handle.value, 7u);

    lua::ScriptArgs args{};
    args.count = 2;
    args.words[0] = 42;
    args.words[1] = 43;
    EXPECT_EQ(rt.InvokeCallback(lua::CallbackId{5}, args), lua::ScriptOutcome::Ok);
    ASSERT_EQ(rt.invocations.size(), 1u);
    EXPECT_EQ(rt.invocations[0].id.value, 5u);
    EXPECT_EQ(rt.invocations[0].args.words[0], 42u);
}

TEST(LuaRuntimeSeamStep3, FakeScriptableFailureOutcomes)
{
    lua::FakeLuaRuntime rt;
    rt.loadOutcome = lua::ScriptOutcome::SyntaxError;
    const auto r = rt.LoadChunk("bad.lua", Bytes({0}));
    EXPECT_FALSE(r.HasValue()); // value outcome on failure; nothing recorded
    EXPECT_TRUE(rt.loads.empty());

    rt.executeOutcome = lua::ScriptOutcome::RuntimeError;
    EXPECT_EQ(rt.Execute(lua::ScriptId{1}), lua::ScriptOutcome::RuntimeError);

    rt.invokeOutcome = lua::ScriptOutcome::ExecutionTimeout;
    EXPECT_EQ(rt.InvokeCallback(lua::CallbackId{1}, lua::ScriptArgs{}), lua::ScriptOutcome::ExecutionTimeout);
}

TEST(LuaRuntimeSeamStep3, NullRuntimeIsInertButWellFormed)
{
    lua::NullLuaRuntime rt;
    EXPECT_EQ(rt.CreateState(), lua::ScriptOutcome::Ok);
    EXPECT_EQ(rt.LoadStandardLibraries(), lua::ScriptOutcome::Ok);
    const auto a = rt.LoadChunk("x", Bytes({1}));
    const auto b = rt.LoadChunk("y", Bytes({2}));
    ASSERT_TRUE(a.HasValue());
    ASSERT_TRUE(b.HasValue());
    EXPECT_LT(a.Value(), b.Value()); // ascending, addressable
    EXPECT_EQ(rt.Execute(a.Value()), lua::ScriptOutcome::Ok);
    EXPECT_EQ(rt.RegisterFunction("n", lua::HostFunctionId{1}), lua::ScriptOutcome::Ok);
    EXPECT_EQ(rt.InvokeCallback(lua::CallbackId{1}, lua::ScriptArgs{}), lua::ScriptOutcome::Ok);
    rt.DestroyState(); // no-op, no throw
}

// ============================================================================
// Step 04 — ScriptContext + ScriptRegistry
// ============================================================================

namespace
{
    lua::ScriptContext MakeContext(std::uint64_t id, lua::ScriptState state = lua::ScriptState::Loaded)
    {
        lua::ScriptContext c;
        c.id = lua::ScriptId{id};
        c.descriptor.id = lua::ScriptId{id};
        c.descriptor.apiVersion = 1;
        c.state = state;
        return c;
    }
} // namespace

TEST(ScriptRegistryStep4, RegisterFindEnumerateAscending)
{
    lua::ScriptRegistry reg;
    // Insert out of order; enumeration must come back ascending.
    EXPECT_EQ(reg.Register(MakeContext(30)), lua::ScriptOutcome::Ok);
    EXPECT_EQ(reg.Register(MakeContext(10)), lua::ScriptOutcome::Ok);
    EXPECT_EQ(reg.Register(MakeContext(20)), lua::ScriptOutcome::Ok);
    EXPECT_EQ(reg.Count(), 3u);

    const auto all = reg.Enumerate();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].id.value, 10u);
    EXPECT_EQ(all[1].id.value, 20u);
    EXPECT_EQ(all[2].id.value, 30u);

    const lua::ScriptContext* found = reg.Find(lua::ScriptId{20});
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->state, lua::ScriptState::Loaded);
    EXPECT_EQ(reg.Find(lua::ScriptId{999}), nullptr);
    EXPECT_TRUE(reg.Contains(lua::ScriptId{10}));
    EXPECT_FALSE(reg.Contains(lua::ScriptId{11}));
}

TEST(ScriptRegistryStep4, DuplicateRejectedAsValueOutcome)
{
    lua::ScriptRegistry reg;
    EXPECT_EQ(reg.Register(MakeContext(5)), lua::ScriptOutcome::Ok);
    EXPECT_EQ(reg.Register(MakeContext(5)), lua::ScriptOutcome::DuplicateScript);
    EXPECT_EQ(reg.Count(), 1u); // unchanged
}

TEST(ScriptRegistryStep4, NoneIdRejected)
{
    lua::ScriptRegistry reg;
    EXPECT_EQ(reg.Register(MakeContext(0)), lua::ScriptOutcome::NotFound);
    EXPECT_EQ(reg.Count(), 0u);
}

TEST(ScriptRegistryStep4, UnregisterMissingIsNotFound)
{
    lua::ScriptRegistry reg;
    EXPECT_EQ(reg.Register(MakeContext(7)), lua::ScriptOutcome::Ok);
    EXPECT_EQ(reg.Unregister(lua::ScriptId{8}), lua::ScriptOutcome::NotFound);
    EXPECT_EQ(reg.Unregister(lua::ScriptId{7}), lua::ScriptOutcome::Ok);
    EXPECT_EQ(reg.Count(), 0u);
    EXPECT_EQ(reg.Unregister(lua::ScriptId{7}), lua::ScriptOutcome::NotFound); // already gone
}

TEST(ScriptRegistryStep4, MutableFindAllowsInPlaceStateUpdate)
{
    lua::ScriptRegistry reg;
    EXPECT_EQ(reg.Register(MakeContext(1, lua::ScriptState::Loaded)), lua::ScriptOutcome::Ok);
    lua::ScriptContext* c = reg.Find(lua::ScriptId{1});
    ASSERT_NE(c, nullptr);
    c->state = lua::ScriptState::Initialized;
    c->callbacks.push_back(lua::CallbackId{9});
    const lua::ScriptContext* again = reg.Find(lua::ScriptId{1});
    ASSERT_NE(again, nullptr);
    EXPECT_EQ(again->state, lua::ScriptState::Initialized);
    ASSERT_EQ(again->callbacks.size(), 1u);
    EXPECT_EQ(again->callbacks[0].value, 9u);
}

// ============================================================================
// Step 05 — EventBinding registry
// ============================================================================

TEST(EventBindingStep5, BindQueryAscendingOrder)
{
    lua::EventBinding eb;
    // Bind out of order across two events; queries must be ascending by (script, callback).
    EXPECT_EQ(eb.Bind(lua::ScriptEvent::OnTick, lua::ScriptId{3}, lua::CallbackId{1}), lua::ScriptOutcome::Ok);
    EXPECT_EQ(eb.Bind(lua::ScriptEvent::OnTick, lua::ScriptId{1}, lua::CallbackId{2}), lua::ScriptOutcome::Ok);
    EXPECT_EQ(eb.Bind(lua::ScriptEvent::OnTick, lua::ScriptId{1}, lua::CallbackId{1}), lua::ScriptOutcome::Ok);
    EXPECT_EQ(eb.Bind(lua::ScriptEvent::OnPlayerJoin, lua::ScriptId{2}, lua::CallbackId{5}), lua::ScriptOutcome::Ok);

    const auto tick = eb.CallbacksFor(lua::ScriptEvent::OnTick);
    ASSERT_EQ(tick.size(), 3u);
    EXPECT_EQ(tick[0].script.value, 1u);
    EXPECT_EQ(tick[0].callback.value, 1u);
    EXPECT_EQ(tick[1].script.value, 1u);
    EXPECT_EQ(tick[1].callback.value, 2u);
    EXPECT_EQ(tick[2].script.value, 3u);

    EXPECT_EQ(eb.CountFor(lua::ScriptEvent::OnPlayerJoin), 1u);
    EXPECT_EQ(eb.CountFor(lua::ScriptEvent::OnServerStop), 0u);
    EXPECT_EQ(eb.Count(), 4u);
    // Events with no bindings return empty.
    EXPECT_TRUE(eb.CallbacksFor(lua::ScriptEvent::OnServerStop).empty());
}

TEST(EventBindingStep5, DuplicateBindingRejected)
{
    lua::EventBinding eb;
    EXPECT_EQ(eb.Bind(lua::ScriptEvent::OnTick, lua::ScriptId{1}, lua::CallbackId{1}), lua::ScriptOutcome::Ok);
    EXPECT_EQ(eb.Bind(lua::ScriptEvent::OnTick, lua::ScriptId{1}, lua::CallbackId{1}),
              lua::ScriptOutcome::DuplicateScript);
    EXPECT_EQ(eb.Count(), 1u);
    // Same script/callback under a DIFFERENT event is a distinct, allowed binding.
    EXPECT_EQ(eb.Bind(lua::ScriptEvent::OnPlayerLeave, lua::ScriptId{1}, lua::CallbackId{1}),
              lua::ScriptOutcome::Ok);
    EXPECT_EQ(eb.Count(), 2u);
}

TEST(EventBindingStep5, NoneScriptOrCallbackRejected)
{
    lua::EventBinding eb;
    EXPECT_EQ(eb.Bind(lua::ScriptEvent::OnTick, lua::ScriptId{0}, lua::CallbackId{1}),
              lua::ScriptOutcome::InvalidCallback);
    EXPECT_EQ(eb.Bind(lua::ScriptEvent::OnTick, lua::ScriptId{1}, lua::CallbackId{0}),
              lua::ScriptOutcome::InvalidCallback);
    EXPECT_EQ(eb.Count(), 0u);
}

TEST(EventBindingStep5, UnbindRemovesAndMissingIsNotFound)
{
    lua::EventBinding eb;
    EXPECT_EQ(eb.Bind(lua::ScriptEvent::OnTick, lua::ScriptId{1}, lua::CallbackId{1}), lua::ScriptOutcome::Ok);
    EXPECT_EQ(eb.Bind(lua::ScriptEvent::OnTick, lua::ScriptId{2}, lua::CallbackId{1}), lua::ScriptOutcome::Ok);
    EXPECT_EQ(eb.Unbind(lua::ScriptEvent::OnTick, lua::ScriptId{1}, lua::CallbackId{1}), lua::ScriptOutcome::Ok);
    EXPECT_EQ(eb.CountFor(lua::ScriptEvent::OnTick), 1u);
    EXPECT_EQ(eb.CallbacksFor(lua::ScriptEvent::OnTick)[0].script.value, 2u);
    // Removing again / removing an unknown binding is NotFound.
    EXPECT_EQ(eb.Unbind(lua::ScriptEvent::OnTick, lua::ScriptId{1}, lua::CallbackId{1}),
              lua::ScriptOutcome::NotFound);
    EXPECT_EQ(eb.Unbind(lua::ScriptEvent::OnPlayerJoin, lua::ScriptId{9}, lua::CallbackId{9}),
              lua::ScriptOutcome::NotFound);
}

// ============================================================================
// Step 06 — ScriptValidator
// ============================================================================

TEST(ScriptValidatorStep6, IndividualChecksPassAndFail)
{
    lua::ScriptRegistry reg;
    // Registry already has script 1 (for duplicate + dependency tests).
    {
        lua::ScriptContext c;
        c.id = lua::ScriptId{1};
        ASSERT_EQ(reg.Register(c), lua::ScriptOutcome::Ok);
    }

    // Duplicate.
    EXPECT_EQ(lua::ScriptValidator::ValidateDuplicate(lua::ScriptId{1}, reg), lua::ScriptOutcome::DuplicateScript);
    EXPECT_EQ(lua::ScriptValidator::ValidateDuplicate(lua::ScriptId{2}, reg), lua::ScriptOutcome::Ok);

    // Version: candidate > current -> ApiIncompatible; strict mismatch -> VersionMismatch.
    EXPECT_EQ(lua::ScriptValidator::ValidateVersion(3, 2, false), lua::ScriptOutcome::ApiIncompatible);
    EXPECT_EQ(lua::ScriptValidator::ValidateVersion(1, 2, true), lua::ScriptOutcome::VersionMismatch);
    EXPECT_EQ(lua::ScriptValidator::ValidateVersion(1, 2, false), lua::ScriptOutcome::Ok); // lenient older ok
    EXPECT_EQ(lua::ScriptValidator::ValidateVersion(2, 2, true), lua::ScriptOutcome::Ok);

    // Callbacks: a none callback is invalid.
    EXPECT_EQ(lua::ScriptValidator::ValidateCallbacks({lua::CallbackId{1}, lua::CallbackId{2}}),
              lua::ScriptOutcome::Ok);
    EXPECT_EQ(lua::ScriptValidator::ValidateCallbacks({lua::CallbackId{1}, lua::CallbackId{0}}),
              lua::ScriptOutcome::InvalidCallback);

    // Dependencies: present ok; absent/none -> MissingDependency.
    EXPECT_EQ(lua::ScriptValidator::ValidateDependencies({lua::ScriptId{1}}, reg), lua::ScriptOutcome::Ok);
    EXPECT_EQ(lua::ScriptValidator::ValidateDependencies({lua::ScriptId{99}}, reg),
              lua::ScriptOutcome::MissingDependency);
    EXPECT_EQ(lua::ScriptValidator::ValidateDependencies({lua::ScriptId{0}}, reg),
              lua::ScriptOutcome::MissingDependency);

    // Syntax via the runtime seam.
    lua::FakeLuaRuntime rt;
    EXPECT_EQ(lua::ScriptValidator::ValidateSyntax(rt, "ok.lua", Bytes({1, 2})), lua::ScriptOutcome::Ok);
    rt.loadOutcome = lua::ScriptOutcome::SyntaxError;
    EXPECT_EQ(lua::ScriptValidator::ValidateSyntax(rt, "bad.lua", Bytes({0})), lua::ScriptOutcome::SyntaxError);
}

TEST(ScriptValidatorStep6, CompositeValidatePassesWhenAllGood)
{
    lua::ScriptRegistry reg;
    {
        lua::ScriptContext dep;
        dep.id = lua::ScriptId{10};
        ASSERT_EQ(reg.Register(dep), lua::ScriptOutcome::Ok);
    }
    lua::FakeLuaRuntime rt;

    lua::ScriptValidationInput in;
    in.id = lua::ScriptId{20}; // not yet registered
    in.apiVersion = 2;
    in.bytes = Bytes({5, 6, 7});
    in.callbacks = {lua::CallbackId{1}};
    in.dependencies = {lua::ScriptId{10}}; // present

    EXPECT_EQ(lua::ScriptValidator::Validate(in, rt, reg, /*current*/ 2, /*strict*/ true, "s.lua"),
              lua::ScriptOutcome::Ok);
}

TEST(ScriptValidatorStep6, CompositeValidateShortCircuitsInOrder)
{
    lua::ScriptRegistry reg;
    {
        lua::ScriptContext existing;
        existing.id = lua::ScriptId{20};
        ASSERT_EQ(reg.Register(existing), lua::ScriptOutcome::Ok);
    }
    lua::FakeLuaRuntime rt;
    rt.loadOutcome = lua::ScriptOutcome::SyntaxError; // would fail last, but duplicate fails first

    lua::ScriptValidationInput in;
    in.id = lua::ScriptId{20};             // duplicate -> checked first
    in.apiVersion = 9;                     // also version-incompatible
    in.bytes = Bytes({0});
    in.callbacks = {lua::CallbackId{0}};   // also invalid
    in.dependencies = {lua::ScriptId{77}}; // also missing

    // Duplicate is first in the fixed order, so that is the reported outcome.
    EXPECT_EQ(lua::ScriptValidator::Validate(in, rt, reg, 2, true, "s.lua"), lua::ScriptOutcome::DuplicateScript);
    EXPECT_TRUE(rt.loads.empty()); // short-circuited before the syntax load
}

// ============================================================================
// Step 07 — IScriptSource read seam (InMemoryScriptSource / NullScriptSource)
// ============================================================================

namespace
{
    lua::ScriptDescriptor Descriptor(std::uint64_t id, std::uint32_t apiVersion = 1, std::uint32_t generation = 0)
    {
        lua::ScriptDescriptor d;
        d.id = lua::ScriptId{id};
        d.apiVersion = apiVersion;
        d.generation = generation;
        return d;
    }
} // namespace

TEST(ScriptSourceStep7, InMemoryStoreEnumerateAscending)
{
    lua::InMemoryScriptSource src;
    // Store out of order; enumeration must be ascending by id.
    src.Store(Descriptor(30), Bytes({3, 3}));
    src.Store(Descriptor(10), Bytes({1}));
    src.Store(Descriptor(20), Bytes({2, 2, 2}));
    EXPECT_EQ(src.Count(), 3u);

    const auto all = src.Enumerate();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].id.value, 10u);
    EXPECT_EQ(all[1].id.value, 20u);
    EXPECT_EQ(all[2].id.value, 30u);
}

TEST(ScriptSourceStep7, InMemoryReadAndExists)
{
    lua::InMemoryScriptSource src;
    src.Store(Descriptor(7), Bytes({0xAB, 0xCD}));

    EXPECT_TRUE(src.Exists(lua::ScriptId{7}));
    EXPECT_FALSE(src.Exists(lua::ScriptId{8}));

    const auto ok = src.Read(lua::ScriptId{7});
    ASSERT_TRUE(ok.HasValue());
    ASSERT_EQ(ok.Value().size(), 2u);
    EXPECT_EQ(ok.Value()[0], static_cast<std::byte>(0xAB));
    EXPECT_EQ(ok.Value()[1], static_cast<std::byte>(0xCD));

    // Missing -> value outcome (NotFound), nothing thrown.
    EXPECT_FALSE(src.Read(lua::ScriptId{8}).HasValue());
}

TEST(ScriptSourceStep7, InMemoryStoreReplacesAndRemove)
{
    lua::InMemoryScriptSource src;
    src.Store(Descriptor(5, /*api*/ 1), Bytes({1}));
    src.Store(Descriptor(5, /*api*/ 4), Bytes({9, 9})); // replace same id
    EXPECT_EQ(src.Count(), 1u);
    const auto all = src.Enumerate();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].apiVersion, 4u);
    ASSERT_TRUE(src.Read(lua::ScriptId{5}).HasValue());
    EXPECT_EQ(src.Read(lua::ScriptId{5}).Value().size(), 2u);

    src.Remove(lua::ScriptId{5});
    EXPECT_EQ(src.Count(), 0u);
    EXPECT_FALSE(src.Exists(lua::ScriptId{5}));
    src.Remove(lua::ScriptId{5}); // benign when absent
    EXPECT_EQ(src.Count(), 0u);
}

TEST(ScriptSourceStep7, NullSourceIsInert)
{
    lua::NullScriptSource src;
    EXPECT_TRUE(src.Enumerate().empty());
    EXPECT_FALSE(src.Exists(lua::ScriptId{1}));
    EXPECT_FALSE(src.Read(lua::ScriptId{1}).HasValue());
}

TEST(ScriptSourceStep7, UsableThroughInterfaceReference)
{
    lua::InMemoryScriptSource concrete;
    concrete.Store(Descriptor(1), Bytes({42}));
    const lua::IScriptSource& src = concrete; // engine-free polymorphic use
    EXPECT_EQ(src.Enumerate().size(), 1u);
    EXPECT_TRUE(src.Exists(lua::ScriptId{1}));
    ASSERT_TRUE(src.Read(lua::ScriptId{1}).HasValue());
    EXPECT_EQ(src.Read(lua::ScriptId{1}).Value()[0], static_cast<std::byte>(42));
}

// ============================================================================
// Step 08 — PlatformScriptStore (platform boundary; real filesystem source)
// ============================================================================

namespace
{
    // Test-only helper: write a file via C stdio (no iostream). Returns success.
    bool WriteFile(const std::string& path, std::initializer_list<int> bytes)
    {
        std::FILE* f = nullptr;
#if defined(_MSC_VER)
        if (fopen_s(&f, path.c_str(), "wb") != 0 || f == nullptr)
        {
            return false;
        }
#else
        f = std::fopen(path.c_str(), "wb");
        if (f == nullptr)
        {
            return false;
        }
#endif
        for (int b : bytes)
        {
            const unsigned char c = static_cast<unsigned char>(b);
            std::fwrite(&c, 1, 1, f);
        }
        std::fclose(f);
        return true;
    }
} // namespace

// --- Real filesystem source enumerates/reads deterministically ----------------
TEST(PlatformScriptStoreStep8, FilesystemSourceEnumerateReadExists)
{
    namespace fs = std::filesystem;
    const std::string dir = "lua_step8_testdir";
    std::error_code ec;
    fs::remove_all(dir, ec);
    ASSERT_TRUE(fs::create_directories(dir, ec));

    ASSERT_TRUE(WriteFile(dir + "/alpha.lua", {1, 2}));
    ASSERT_TRUE(WriteFile(dir + "/beta.lua", {3, 4, 5}));
    ASSERT_TRUE(WriteFile(dir + "/notes.txt", {9})); // non-.lua ignored

    auto src = adapters::CreatePlatformScriptSource(dir);
    ASSERT_NE(src, nullptr);

    const auto all = src->Enumerate();
    ASSERT_EQ(all.size(), 2u); // only the two .lua files
    // Deterministic ascending by derived id.
    EXPECT_TRUE(all[0].id < all[1].id);

    // Reading each enumerated id yields one of the known contents; every id exists.
    std::size_t sizeTwo = 0, sizeThree = 0;
    for (const auto& d : all)
    {
        EXPECT_TRUE(src->Exists(d.id));
        const auto bytes = src->Read(d.id);
        ASSERT_TRUE(bytes.HasValue());
        if (bytes.Value().size() == 2u)
        {
            ++sizeTwo;
        }
        else if (bytes.Value().size() == 3u)
        {
            ++sizeThree;
        }
    }
    EXPECT_EQ(sizeTwo, 1u);
    EXPECT_EQ(sizeThree, 1u);

    // A never-stored id is a value outcome, not a throw.
    EXPECT_FALSE(src->Exists(lua::ScriptId{123456789u}));
    EXPECT_FALSE(src->Read(lua::ScriptId{123456789u}).HasValue());

    fs::remove_all(dir, ec);
}

// --- Missing directory yields an inert (empty) source, never an error ----------
TEST(PlatformScriptStoreStep8, MissingDirectoryIsInert)
{
    auto src = adapters::CreatePlatformScriptSource("lua_step8_absent_dir_xyz");
    ASSERT_NE(src, nullptr);
    EXPECT_TRUE(src->Enumerate().empty());
    EXPECT_FALSE(src->Exists(lua::ScriptId{1}));
    EXPECT_FALSE(src->Read(lua::ScriptId{1}).HasValue());
}

// --- A given file maps to a stable id across separate source instances ---------
TEST(PlatformScriptStoreStep8, DerivedIdIsStableAcrossInstances)
{
    namespace fs = std::filesystem;
    const std::string dir = "lua_step8_stableid";
    std::error_code ec;
    fs::remove_all(dir, ec);
    ASSERT_TRUE(fs::create_directories(dir, ec));
    ASSERT_TRUE(WriteFile(dir + "/quest.lua", {7}));

    auto a = adapters::CreatePlatformScriptSource(dir);
    auto b = adapters::CreatePlatformScriptSource(dir);
    const auto ea = a->Enumerate();
    const auto eb = b->Enumerate();
    ASSERT_EQ(ea.size(), 1u);
    ASSERT_EQ(eb.size(), 1u);
    EXPECT_EQ(ea[0].id.value, eb[0].id.value); // stable, deterministic id
    EXPECT_NE(ea[0].id.value, 0u);             // never the reserved none id

    fs::remove_all(dir, ec);
}

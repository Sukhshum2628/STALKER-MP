// STALKER-MP — Plugin contract (Sprint-014, Step 3)
//
// The stable, engine-free contract every compiled plugin implements. It is a PURE
// abstract interface: identity/version metadata, an Initialize that receives the host
// service surface, a Shutdown, and an OnEvent hook. All fallible operations are value
// outcomes (ADR-007); nothing here throws, allocates on behalf of the engine, exposes an
// engine/loader type, or touches a thread.
//
// The host handle passed to Initialize is the EXISTING Sprint-013 public API surface
// (`lua::ScriptApiSet`), reused as the plugin host-service surface per the frozen
// `[AR-P3]` Option A decision (gameplay facades only; administration/tooling deferred).
// No new host type is introduced here; the concrete host is constructed at Steps 12/17.
//
// This step introduces the CONTRACT ONLY, plus two engine-free test doubles:
//   * `FakePlugin` — deterministic; records init/shutdown/events; scriptable outcomes.
//   * `NullPlugin` — inert; accepts everything, does nothing.
// No registry (Step-04), loader (Step-11), host (Step-12), or wiring (Step-17).
//
// Engine-free / loader-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>
#include <vector>

#include "stalkermp/lua/ScriptApi.h"          // lua::ScriptApiSet (reused host surface, [AR-P3] Option A)
#include "stalkermp/plugin/PluginTypes.h"     // PluginDescriptor / PluginEvent / PluginOutcome

namespace stalkermp::plugin
{
    // The host service surface handed to a plugin (the reused Sprint-013 facade bundle).
    using PluginHostServices = lua::ScriptApiSet;

    // Value-only event payload crossing the contract (opaque value words; no engine type).
    struct PluginArgs
    {
        std::uint32_t count = 0;
        std::uint64_t words[4] = {0, 0, 0, 0};
    };

    // The stable plugin contract. Value outcomes only (ADR-007); no engine/loader type.
    class IPlugin
    {
    public:
        virtual ~IPlugin() = default;

        // Identity + version metadata (manifest). Const, deterministic.
        [[nodiscard]] virtual PluginDescriptor Descriptor() const = 0;

        // Initialize the plugin, giving it the host service surface. A failure is a value
        // outcome (InitFailed / …); the plugin must not be activated on failure.
        [[nodiscard]] virtual PluginOutcome Initialize(const PluginHostServices& host) = 0;

        // Shut the plugin down (idempotent; never throws).
        virtual void Shutdown() noexcept = 0;

        // React to a host-dispatched event with value args. A fault is a value outcome
        // that the manager isolates (Step-14); the plugin never owns or reorders events.
        [[nodiscard]] virtual PluginOutcome OnEvent(PluginEvent event, const PluginArgs& args) = 0;
    };

    // ------------------------------------------------------------- FakePlugin
    // Deterministic in-memory double: records every contract interaction and returns
    // scriptable outcomes so tests can drive both success and each failure path.
    class FakePlugin final : public IPlugin
    {
    public:
        // Configurable identity + scriptable outcomes.
        PluginDescriptor descriptor{};
        PluginOutcome initOutcome = PluginOutcome::Ok;
        PluginOutcome eventOutcome = PluginOutcome::Ok;

        // Inspection surface.
        bool initialized = false;
        bool shutdownCalled = false;
        std::vector<PluginEvent> events;

        [[nodiscard]] PluginDescriptor Descriptor() const override { return descriptor; }

        [[nodiscard]] PluginOutcome Initialize(const PluginHostServices&) override
        {
            if (initOutcome == PluginOutcome::Ok)
            {
                initialized = true;
            }
            return initOutcome;
        }

        void Shutdown() noexcept override
        {
            shutdownCalled = true;
            initialized = false;
        }

        [[nodiscard]] PluginOutcome OnEvent(PluginEvent event, const PluginArgs&) override
        {
            events.push_back(event);
            return eventOutcome;
        }
    };

    // ------------------------------------------------------------- NullPlugin
    // Inert double: accepts everything, records nothing, does nothing.
    class NullPlugin final : public IPlugin
    {
    public:
        [[nodiscard]] PluginDescriptor Descriptor() const override { return PluginDescriptor{}; }
        [[nodiscard]] PluginOutcome Initialize(const PluginHostServices&) override { return PluginOutcome::Ok; }
        void Shutdown() noexcept override {}
        [[nodiscard]] PluginOutcome OnEvent(PluginEvent, const PluginArgs&) override { return PluginOutcome::Ok; }
    };
} // namespace stalkermp::plugin

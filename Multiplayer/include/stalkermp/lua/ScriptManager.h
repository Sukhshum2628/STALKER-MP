// STALKER-MP — Script manager (Sprint-013, Steps 13-14)
//
// The engine-free orchestrator of the scripting subsystem: it owns the ScriptRegistry
// and EventBinding, drives the ScriptLoader over the IScriptSource, binds callbacks,
// and dispatches events (including OnTick) through the ILuaRuntime seam. It is a
// `core::IService` + `core::ITickable` whose Tick dispatches the per-frame OnTick phase;
// it is NOT subscribed to the FrameDispatcher here — that wiring (at the reserved
// Gameplay-phase tick key) is the Step-17 composition gate.
//
// Fault isolation (Step 14): every callback dispatch is a value outcome; a fault
// (runtime error, invalid API use, missing script, recursion limit, execution timeout,
// crash) DISABLES the offending script (state -> Failed) and skips it thereafter, while
// its siblings keep running. No C++ exception escapes the runtime seam (ADR-007).
//
// It coordinates ONLY previously-completed engine-free seams; it instantiates no VM and
// executes no real script (the ILuaRuntime seam is a test double in this build).
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/interfaces/IService.h"
#include "stalkermp/core/interfaces/ITickable.h"
#include "stalkermp/lua/EventBinding.h"
#include "stalkermp/lua/ILuaRuntime.h"
#include "stalkermp/lua/IScriptSource.h"
#include "stalkermp/lua/LuaScriptTypes.h"
#include "stalkermp/lua/ScriptLoader.h"
#include "stalkermp/lua/ScriptRegistry.h"

namespace stalkermp::lua
{
    // The result of dispatching one event to its bound callbacks.
    struct ScriptDispatchReport
    {
        std::size_t invoked = 0;  // callbacks invoked (skipped disabled/missing not counted)
        std::size_t isolated = 0; // scripts disabled this dispatch due to a fault
        std::size_t skipped = 0;  // callbacks skipped (disabled script / missing script)
    };

    class ScriptManager final : public core::IService, public core::ITickable
    {
    public:
        ScriptManager(ILuaRuntime& runtime, IScriptSource& source, std::uint32_t currentApiVersion,
                      bool strictApiVersion) noexcept
            : m_runtime(runtime), m_source(source), m_apiVersion(currentApiVersion), m_strict(strictApiVersion)
        {
        }

        // --- Orchestration ---------------------------------------------------
        // Discover + load every script from the source (deterministic; per-script
        // failure isolated by the loader).
        [[nodiscard]] ScriptLoadReport LoadAll();

        // Unload a script: unbind its callbacks, remove it from the registry, and clear
        // any disabled flag. Missing -> NotFound.
        [[nodiscard]] ScriptOutcome Unload(ScriptId id);

        // Reload a script from the source (unload then load). Absent from source -> NotFound.
        [[nodiscard]] ScriptOutcome Reload(ScriptId id);

        // Bind a callback of a loaded script to an event. Unknown script -> NotFound.
        [[nodiscard]] ScriptOutcome BindCallback(ScriptEvent event, ScriptId script, CallbackId callback);

        // Dispatch an event to its bound callbacks (ascending, deterministic) through the
        // runtime seam. Faults isolate the offending script; siblings continue.
        ScriptDispatchReport DispatchEvent(ScriptEvent event, const ScriptArgs& args);

        // --- Introspection ---------------------------------------------------
        [[nodiscard]] std::size_t ScriptCount() const noexcept { return m_registry.Count(); }
        [[nodiscard]] bool IsDisabled(ScriptId id) const noexcept;
        [[nodiscard]] std::size_t DisabledCount() const noexcept { return m_disabled.size(); }
        [[nodiscard]] const ScriptRegistry& Registry() const noexcept { return m_registry; }
        [[nodiscard]] std::uint64_t Ticks() const noexcept { return m_tick; }

        // --- core::IService / ISubsystem -------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "Scripting"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override;
        [[nodiscard]] core::Expected<void> Initialize() override;
        void Shutdown() noexcept override;

        // --- core::ITickable -------------------------------------------------
        // One per-frame OnTick dispatch. `deltaSeconds` is DIAGNOSTIC and never used in
        // control flow (the tick counter drives dispatch — deterministic). Wired at the
        // reserved Gameplay-phase tick key in Step-17, not here.
        void Tick(double deltaSeconds) override;

    private:
        void Disable(ScriptId id);                              // mark Failed + record
        [[nodiscard]] static bool IsFault(ScriptOutcome o) noexcept { return o != ScriptOutcome::Ok; }

        ILuaRuntime& m_runtime;
        IScriptSource& m_source;
        std::uint32_t m_apiVersion;
        bool m_strict;
        ScriptRegistry m_registry;
        EventBinding m_events;
        std::vector<ScriptId> m_disabled; // sorted ascending
        std::uint64_t m_tick = 0;
    };
} // namespace stalkermp::lua

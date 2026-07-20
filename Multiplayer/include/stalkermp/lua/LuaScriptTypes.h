// STALKER-MP — Lua scripting value types & vocabulary (Sprint-013, Step 1)
//
// The engine-free, VM-free, OS-free foundational vocabulary of the host-side LUA
// INTEGRATION subsystem: the outcome/state/event enumerations, the script/callback
// value ids, the script descriptor, a value-only call result, and the
// statistics/consistency value shapes that every later Sprint-013 step builds on.
// Types ONLY — no runtime, loading, validation, dispatch, event binding, diagnostics,
// platform/OS access, or engine/VM headers.
//
// Scripting is HOST-SIDE and executes as gameplay on the single Simulation Thread
// (Host Authority; deterministic simulation). These value captures hold only plain
// scalars — never a live object, reference, VM handle, or engine/OS handle.
// Deterministic layout, `0 = none/neutral`; VM/timing/memory figures are diagnostic
// only and never participate in replay identity or control flow.
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>

namespace stalkermp::lua
{
    // ------------------------------------------------------------- Enumerations
    // Fixed std::uint8_t underlying type (deterministic ABI); reserved 0 carries the
    // neutral/safe meaning; new enumerators are appended, never renumbered.

    // Named value outcome for the (later) fallible scripting operations (ADR-007).
    enum class ScriptOutcome : std::uint8_t
    {
        Ok = 0,
        LoadFailed,        // the script could not be loaded from its source
        SyntaxError,       // the script failed to parse/compile
        ApiIncompatible,   // the script requires an unavailable API surface
        VersionMismatch,   // the script's API version is incompatible with the build
        DuplicateScript,   // a script with the same id is already registered
        InvalidCallback,   // a declared callback is invalid/unknown
        MissingDependency, // a required dependency is absent
        RuntimeError,      // the script raised a runtime error during execution
        RecursionLimit,    // the recursion-depth guard tripped
        ExecutionTimeout,  // the per-invocation execution budget was exceeded
        ScriptDisabled,    // the script was isolated/disabled after a fault
        NotFound,          // no script matches the requested id
    };

    [[nodiscard]] constexpr const char* ScriptOutcomeName(const ScriptOutcome o) noexcept
    {
        switch (o)
        {
        case ScriptOutcome::Ok:                return "Ok";
        case ScriptOutcome::LoadFailed:        return "LoadFailed";
        case ScriptOutcome::SyntaxError:       return "SyntaxError";
        case ScriptOutcome::ApiIncompatible:   return "ApiIncompatible";
        case ScriptOutcome::VersionMismatch:   return "VersionMismatch";
        case ScriptOutcome::DuplicateScript:   return "DuplicateScript";
        case ScriptOutcome::InvalidCallback:   return "InvalidCallback";
        case ScriptOutcome::MissingDependency: return "MissingDependency";
        case ScriptOutcome::RuntimeError:      return "RuntimeError";
        case ScriptOutcome::RecursionLimit:    return "RecursionLimit";
        case ScriptOutcome::ExecutionTimeout:  return "ExecutionTimeout";
        case ScriptOutcome::ScriptDisabled:    return "ScriptDisabled";
        case ScriptOutcome::NotFound:          return "NotFound";
        }
        return "Unknown";
    }

    // The lifecycle state of a single script (deterministic order; §7.6).
    enum class ScriptState : std::uint8_t
    {
        Unloaded = 0,
        Loaded,
        Initialized,
        Executing,
        Suspended,
        Failed,
        Destroyed,
    };

    [[nodiscard]] constexpr const char* ScriptStateName(const ScriptState s) noexcept
    {
        switch (s)
        {
        case ScriptState::Unloaded:    return "Unloaded";
        case ScriptState::Loaded:      return "Loaded";
        case ScriptState::Initialized: return "Initialized";
        case ScriptState::Executing:   return "Executing";
        case ScriptState::Suspended:   return "Suspended";
        case ScriptState::Failed:      return "Failed";
        case ScriptState::Destroyed:   return "Destroyed";
        }
        return "Unknown";
    }

    // The fixed set of host-dispatched gameplay events scripts may react to (§7.5).
    // Scripts REACT to these; they never own, emit, or reorder them.
    enum class ScriptEvent : std::uint8_t
    {
        OnServerStart = 0,
        OnServerStop,
        OnPlayerJoin,
        OnPlayerLeave,
        OnPlayerDeath,
        OnEntitySpawn,
        OnEntityDestroy,
        OnWorldLoaded,
        OnWorldSaved,
        OnTick,
    };

    [[nodiscard]] constexpr const char* ScriptEventName(const ScriptEvent e) noexcept
    {
        switch (e)
        {
        case ScriptEvent::OnServerStart:   return "OnServerStart";
        case ScriptEvent::OnServerStop:    return "OnServerStop";
        case ScriptEvent::OnPlayerJoin:    return "OnPlayerJoin";
        case ScriptEvent::OnPlayerLeave:   return "OnPlayerLeave";
        case ScriptEvent::OnPlayerDeath:   return "OnPlayerDeath";
        case ScriptEvent::OnEntitySpawn:   return "OnEntitySpawn";
        case ScriptEvent::OnEntityDestroy: return "OnEntityDestroy";
        case ScriptEvent::OnWorldLoaded:   return "OnWorldLoaded";
        case ScriptEvent::OnWorldSaved:    return "OnWorldSaved";
        case ScriptEvent::OnTick:          return "OnTick";
        }
        return "Unknown";
    }

    // ------------------------------------------------------------- Value ids
    // Opaque monotonic value ids; `0 = none`. Value-only, trivially copyable.

    struct ScriptId
    {
        std::uint64_t value = 0;

        [[nodiscard]] constexpr bool IsNone() const noexcept { return value == 0; }
        [[nodiscard]] constexpr bool operator==(const ScriptId& o) const noexcept { return value == o.value; }
        [[nodiscard]] constexpr bool operator!=(const ScriptId& o) const noexcept { return value != o.value; }
        [[nodiscard]] constexpr bool operator<(const ScriptId& o) const noexcept { return value < o.value; }
    };

    struct CallbackId
    {
        std::uint64_t value = 0;

        [[nodiscard]] constexpr bool IsNone() const noexcept { return value == 0; }
        [[nodiscard]] constexpr bool operator==(const CallbackId& o) const noexcept { return value == o.value; }
        [[nodiscard]] constexpr bool operator!=(const CallbackId& o) const noexcept { return value != o.value; }
        [[nodiscard]] constexpr bool operator<(const CallbackId& o) const noexcept { return value < o.value; }
    };

    // ------------------------------------------------------------- Value shapes

    // Identifies a script known to the subsystem (value-only; no source path — that is
    // an opaque token resolved only by the platform source backend, later steps).
    struct ScriptDescriptor
    {
        ScriptId id{};                   // 0 = none
        std::uint32_t apiVersion = 0;    // required API surface version
        std::uint32_t generation = 0;    // reload generation counter
    };

    // The value result of a (later) script invocation. Value-only (ADR-007).
    struct ScriptCallResult
    {
        ScriptOutcome outcome = ScriptOutcome::Ok;
        std::uint64_t returnValue = 0;   // opaque value word (0 = none)
    };

    // Non-invasive diagnostic tallies (§7.11). Monotonic counters; execution time and
    // memory are DIAGNOSTIC ONLY and never gate control flow or replay identity.
    struct ScriptStatistics
    {
        std::uint64_t loadedScripts = 0;
        std::uint64_t executionTimeMicros = 0; // diagnostic
        std::uint64_t memoryBytes = 0;         // diagnostic
        std::uint64_t apiCalls = 0;
        std::uint64_t callbackCount = 0;
        std::uint64_t scriptErrors = 0;
    };

    // A deterministic consistency snapshot of the registry/bindings (value-only).
    struct ScriptConsistency
    {
        std::uint64_t registeredScripts = 0;
        std::uint64_t boundCallbacks = 0;
        std::uint64_t activeScripts = 0;
        bool consistent = true; // registry/bindings agree
    };
} // namespace stalkermp::lua

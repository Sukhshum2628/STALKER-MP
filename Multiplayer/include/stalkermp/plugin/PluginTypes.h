// STALKER-MP — Plugin framework value types & vocabulary (Sprint-014, Step 1)
//
// The engine-free, VM-free, OS-free foundational vocabulary of the host-side PLUGIN
// FRAMEWORK: the outcome/state/event enumerations, the plugin value id, the plugin
// descriptor (manifest), and the statistics/consistency value shapes that every later
// Sprint-014 step builds on. Types ONLY — no loading, registration, validation,
// dispatch, platform/OS access, or engine headers.
//
// Plugins are host-side, compiled extensions that consume engine services through the
// stable public interfaces and react to host-dispatched events (Host Authority;
// deterministic simulation). These value captures hold only plain scalars and small
// value collections — never a live object, reference, or engine/OS handle.
// Deterministic layout, `0 = none/neutral`; timing/memory figures are diagnostic only
// and never participate in replay identity or control flow.
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>
#include <vector>

namespace stalkermp::plugin
{
    // ------------------------------------------------------------- Enumerations
    // Fixed std::uint8_t underlying type (deterministic ABI); reserved 0 carries the
    // neutral/safe meaning; new enumerators are appended, never renumbered.

    // Named value outcome for the (later) fallible plugin operations (ADR-007).
    enum class PluginOutcome : std::uint8_t
    {
        Ok = 0,
        LoadFailed,        // the plugin could not be loaded/registered
        VersionMismatch,   // the plugin's version is incompatible with the host
        ApiIncompatible,   // the plugin requires an unavailable API surface/version
        DuplicatePlugin,   // a plugin with the same id is already registered
        MissingDependency, // a required dependency is absent
        InvalidHook,       // a declared event hook is invalid/unknown
        InitFailed,        // the plugin's Initialize reported failure
        RuntimeError,      // the plugin reported a runtime error through the contract
        PluginDisabled,    // the plugin was isolated/disabled after a fault
        NotFound,          // no plugin matches the requested id
    };

    [[nodiscard]] constexpr const char* PluginOutcomeName(const PluginOutcome o) noexcept
    {
        switch (o)
        {
        case PluginOutcome::Ok:                return "Ok";
        case PluginOutcome::LoadFailed:        return "LoadFailed";
        case PluginOutcome::VersionMismatch:   return "VersionMismatch";
        case PluginOutcome::ApiIncompatible:   return "ApiIncompatible";
        case PluginOutcome::DuplicatePlugin:   return "DuplicatePlugin";
        case PluginOutcome::MissingDependency: return "MissingDependency";
        case PluginOutcome::InvalidHook:       return "InvalidHook";
        case PluginOutcome::InitFailed:        return "InitFailed";
        case PluginOutcome::RuntimeError:      return "RuntimeError";
        case PluginOutcome::PluginDisabled:    return "PluginDisabled";
        case PluginOutcome::NotFound:          return "NotFound";
        }
        return "Unknown";
    }

    // The lifecycle state of a single plugin (deterministic order; 10_Extensibility §11.3).
    enum class PluginState : std::uint8_t
    {
        Discovered = 0,
        Validated,
        Loaded,
        Initialized,
        Active,
        Suspended,
        Shutdown,
        Unloaded,
        Failed,
    };

    [[nodiscard]] constexpr const char* PluginStateName(const PluginState s) noexcept
    {
        switch (s)
        {
        case PluginState::Discovered:  return "Discovered";
        case PluginState::Validated:   return "Validated";
        case PluginState::Loaded:      return "Loaded";
        case PluginState::Initialized: return "Initialized";
        case PluginState::Active:      return "Active";
        case PluginState::Suspended:   return "Suspended";
        case PluginState::Shutdown:    return "Shutdown";
        case PluginState::Unloaded:    return "Unloaded";
        case PluginState::Failed:      return "Failed";
        }
        return "Unknown";
    }

    // The fixed set of host-dispatched events plugins may react to (scope §7.6). Plugins
    // REACT to these; they never own, emit, or reorder them.
    enum class PluginEvent : std::uint8_t
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

    [[nodiscard]] constexpr const char* PluginEventName(const PluginEvent e) noexcept
    {
        switch (e)
        {
        case PluginEvent::OnServerStart:   return "OnServerStart";
        case PluginEvent::OnServerStop:    return "OnServerStop";
        case PluginEvent::OnPlayerJoin:    return "OnPlayerJoin";
        case PluginEvent::OnPlayerLeave:   return "OnPlayerLeave";
        case PluginEvent::OnPlayerDeath:   return "OnPlayerDeath";
        case PluginEvent::OnEntitySpawn:   return "OnEntitySpawn";
        case PluginEvent::OnEntityDestroy: return "OnEntityDestroy";
        case PluginEvent::OnWorldLoaded:   return "OnWorldLoaded";
        case PluginEvent::OnWorldSaved:    return "OnWorldSaved";
        case PluginEvent::OnTick:          return "OnTick";
        }
        return "Unknown";
    }

    // ------------------------------------------------------------- Value ids
    // Opaque monotonic value id; `0 = none`. Value-only, trivially copyable.
    struct PluginId
    {
        std::uint64_t value = 0;

        [[nodiscard]] constexpr bool IsNone() const noexcept { return value == 0; }
        [[nodiscard]] constexpr bool operator==(const PluginId& o) const noexcept { return value == o.value; }
        [[nodiscard]] constexpr bool operator!=(const PluginId& o) const noexcept { return value != o.value; }
        [[nodiscard]] constexpr bool operator<(const PluginId& o) const noexcept { return value < o.value; }
    };

    // ------------------------------------------------------------- Value shapes

    // A semantic version value (major.minor.patch). Value-only; deterministic ordering.
    struct PluginVersion
    {
        std::uint32_t major = 0;
        std::uint32_t minor = 0;
        std::uint32_t patch = 0;

        [[nodiscard]] constexpr bool operator==(const PluginVersion& o) const noexcept
        {
            return major == o.major && minor == o.minor && patch == o.patch;
        }
        [[nodiscard]] constexpr bool operator!=(const PluginVersion& o) const noexcept { return !(*this == o); }
        [[nodiscard]] constexpr bool operator<(const PluginVersion& o) const noexcept
        {
            if (major != o.major) { return major < o.major; }
            if (minor != o.minor) { return minor < o.minor; }
            return patch < o.patch;
        }
    };

    // The plugin manifest: identity + declared version, supported host/engine API version
    // range, required-API list, and optional dependencies. Value-only (no live handle).
    struct PluginDescriptor
    {
        PluginId id{};                          // 0 = none
        PluginVersion version{};                // the plugin's own version
        std::uint32_t minApiVersion = 0;        // lowest host API version the plugin supports
        std::uint32_t maxApiVersion = 0;        // highest host API version the plugin supports
        std::uint32_t generation = 0;           // reload/registration generation counter
        std::vector<std::uint32_t> requiredApis;   // required host-API capability ids
        std::vector<PluginId> optionalDependencies; // optional plugin dependencies (value ids)
    };

    // Non-invasive diagnostic tallies. Monotonic counters; execution time and memory are
    // DIAGNOSTIC ONLY and never gate control flow or replay identity.
    struct PluginStatistics
    {
        std::uint64_t loadedPlugins = 0;
        std::uint64_t activePlugins = 0;
        std::uint64_t eventDispatches = 0;
        std::uint64_t apiCalls = 0;
        std::uint64_t pluginErrors = 0;
        std::uint64_t executionTimeMicros = 0; // diagnostic
        std::uint64_t memoryBytes = 0;         // diagnostic
    };

    // A deterministic consistency snapshot of the registry/bindings (value-only).
    struct PluginConsistency
    {
        std::uint64_t registeredPlugins = 0;
        std::uint64_t boundHooks = 0;
        std::uint64_t activePlugins = 0;
        bool consistent = true; // registry/bindings agree
    };
} // namespace stalkermp::plugin

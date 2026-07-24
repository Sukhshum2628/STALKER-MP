// STALKER-MP — Plugin lifecycle state machine (Sprint-014, Step 10)
//
// The engine-free deterministic state machine governing a single plugin's lifecycle
// (10_Extensibility.md §11.3): Discovered → Validated → Loaded → Initialized → Active →
// Suspended → Shutdown → Unloaded. It enforces legal transitions only; an illegal
// transition is a value outcome (ADR-007) and leaves the state unchanged. Pure value
// logic — no engine, no platform, no execution: it INVOKES NO PLUGIN CALLBACK, performs
// no loading/unloading, and manages no ownership. This is the transition table ONLY.
//
// Engine-free / platform-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>

#include "stalkermp/core/Expected.h"       // core::Expected
#include "stalkermp/plugin/PluginTypes.h"  // PluginState / PluginOutcome

namespace stalkermp::plugin
{
    // The lifecycle actions a plugin may be driven through (§11.3).
    enum class LifecycleAction : std::uint8_t
    {
        Validate = 0,
        Load,
        Initialize,
        Activate,
        Suspend,
        Resume,
        Shutdown,
        Unload,
    };

    [[nodiscard]] constexpr const char* LifecycleActionName(const LifecycleAction a) noexcept
    {
        switch (a)
        {
        case LifecycleAction::Validate:   return "Validate";
        case LifecycleAction::Load:       return "Load";
        case LifecycleAction::Initialize: return "Initialize";
        case LifecycleAction::Activate:   return "Activate";
        case LifecycleAction::Suspend:    return "Suspend";
        case LifecycleAction::Resume:     return "Resume";
        case LifecycleAction::Shutdown:   return "Shutdown";
        case LifecycleAction::Unload:     return "Unload";
        }
        return "Unknown";
    }

    class PluginLifecycle
    {
    public:
        // Whether `action` is legal from `from`.
        [[nodiscard]] static bool IsLegal(PluginState from, LifecycleAction action) noexcept;

        // The resulting state for a legal transition, or a value outcome (RuntimeError)
        // for an illegal one.
        [[nodiscard]] static core::Expected<PluginState> Next(PluginState from, LifecycleAction action);

        // Apply `action` to `state` in place. On a legal transition `state` advances and Ok
        // is returned; on an illegal one `state` is unchanged and RuntimeError is returned
        // (an illegal transition is a logic error, ADR-007 value outcome).
        [[nodiscard]] static PluginOutcome Apply(PluginState& state, LifecycleAction action);
    };
} // namespace stalkermp::plugin

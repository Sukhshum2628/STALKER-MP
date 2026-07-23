// STALKER-MP — Script lifecycle state machine (Sprint-013, Step 10)
//
// The engine-free deterministic state machine governing a single script's lifecycle:
// Load → Initialize → Execute → Suspend → Resume → Unload → Destroy. It enforces legal
// transitions only; an illegal transition is a value outcome (ADR-007) and leaves the
// state unchanged. Pure value logic — no VM, no engine, no OS, no execution (this is
// the transition table ONLY; actual script execution/orchestration is later steps).
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>

#include "stalkermp/core/Expected.h"       // core::Expected
#include "stalkermp/lua/LuaScriptTypes.h"  // ScriptState / ScriptOutcome

namespace stalkermp::lua
{
    // The lifecycle actions a script may be driven through (§7.6).
    enum class LifecycleAction : std::uint8_t
    {
        Load = 0,
        Initialize,
        Execute,
        Suspend,
        Resume,
        Unload,
        Destroy,
    };

    [[nodiscard]] constexpr const char* LifecycleActionName(const LifecycleAction a) noexcept
    {
        switch (a)
        {
        case LifecycleAction::Load:       return "Load";
        case LifecycleAction::Initialize: return "Initialize";
        case LifecycleAction::Execute:    return "Execute";
        case LifecycleAction::Suspend:    return "Suspend";
        case LifecycleAction::Resume:     return "Resume";
        case LifecycleAction::Unload:     return "Unload";
        case LifecycleAction::Destroy:    return "Destroy";
        }
        return "Unknown";
    }

    class ScriptLifecycle
    {
    public:
        // Whether `action` is legal from `from`.
        [[nodiscard]] static bool IsLegal(ScriptState from, LifecycleAction action) noexcept;

        // The resulting state for a legal transition, or a value outcome (RuntimeError)
        // for an illegal one.
        [[nodiscard]] static core::Expected<ScriptState> Next(ScriptState from, LifecycleAction action);

        // Apply `action` to `state` in place. On a legal transition `state` advances and
        // Ok is returned; on an illegal one `state` is unchanged and RuntimeError is
        // returned (an illegal transition is a logic error, ADR-007 value outcome).
        [[nodiscard]] static ScriptOutcome Apply(ScriptState& state, LifecycleAction action);
    };
} // namespace stalkermp::lua

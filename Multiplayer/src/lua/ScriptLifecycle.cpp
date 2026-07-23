// STALKER-MP — Script lifecycle state machine (Sprint-013, Step 10)
//
// See ScriptLifecycle.h. Engine-free / VM-free / OS-free; no exceptions, no RTTI;
// value outcomes. Deterministic transition table along the frozen lifecycle chain.

#include "stalkermp/lua/ScriptLifecycle.h"

#include "stalkermp/core/Error.h" // core::MakeError / ErrorCode

namespace stalkermp::lua
{
    namespace
    {
        // The resulting state for a legal (from, action) pair, or ScriptState::Destroyed
        // sentinel handled by the caller. Returns true + outNext on a legal transition.
        [[nodiscard]] bool Resolve(const ScriptState from, const LifecycleAction action,
                                   ScriptState& outNext) noexcept
        {
            switch (action)
            {
            case LifecycleAction::Load:
                if (from == ScriptState::Unloaded) { outNext = ScriptState::Loaded; return true; }
                return false;
            case LifecycleAction::Initialize:
                if (from == ScriptState::Loaded) { outNext = ScriptState::Initialized; return true; }
                return false;
            case LifecycleAction::Execute:
                if (from == ScriptState::Initialized) { outNext = ScriptState::Executing; return true; }
                return false;
            case LifecycleAction::Suspend:
                if (from == ScriptState::Executing) { outNext = ScriptState::Suspended; return true; }
                return false;
            case LifecycleAction::Resume:
                if (from == ScriptState::Suspended) { outNext = ScriptState::Executing; return true; }
                return false;
            case LifecycleAction::Unload:
                // Any loaded/live/suspended/failed script may be unloaded.
                if (from == ScriptState::Loaded || from == ScriptState::Initialized ||
                    from == ScriptState::Executing || from == ScriptState::Suspended ||
                    from == ScriptState::Failed)
                {
                    outNext = ScriptState::Unloaded;
                    return true;
                }
                return false;
            case LifecycleAction::Destroy:
                // Any non-destroyed script may be destroyed.
                if (from != ScriptState::Destroyed) { outNext = ScriptState::Destroyed; return true; }
                return false;
            }
            return false;
        }
    } // namespace

    bool ScriptLifecycle::IsLegal(const ScriptState from, const LifecycleAction action) noexcept
    {
        ScriptState next{};
        return Resolve(from, action, next);
    }

    core::Expected<ScriptState> ScriptLifecycle::Next(const ScriptState from, const LifecycleAction action)
    {
        ScriptState next{};
        if (!Resolve(from, action, next))
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "illegal script lifecycle transition");
        }
        return next;
    }

    ScriptOutcome ScriptLifecycle::Apply(ScriptState& state, const LifecycleAction action)
    {
        ScriptState next{};
        if (!Resolve(state, action, next))
        {
            return ScriptOutcome::RuntimeError; // illegal transition (logic error); state unchanged
        }
        state = next;
        return ScriptOutcome::Ok;
    }
} // namespace stalkermp::lua

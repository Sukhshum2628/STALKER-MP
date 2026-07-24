// STALKER-MP — Plugin lifecycle state machine (Sprint-014, Step 10)
//
// See PluginLifecycle.h. Engine-free / platform-free / OS-free; no exceptions, no RTTI;
// value outcomes. Deterministic transition table along the frozen lifecycle chain.

#include "stalkermp/plugin/PluginLifecycle.h"

#include "stalkermp/core/Error.h" // core::MakeError / ErrorCode

namespace stalkermp::plugin
{
    namespace
    {
        [[nodiscard]] bool Resolve(const PluginState from, const LifecycleAction action, PluginState& outNext) noexcept
        {
            switch (action)
            {
            case LifecycleAction::Validate:
                if (from == PluginState::Discovered) { outNext = PluginState::Validated; return true; }
                return false;
            case LifecycleAction::Load:
                if (from == PluginState::Validated) { outNext = PluginState::Loaded; return true; }
                return false;
            case LifecycleAction::Initialize:
                if (from == PluginState::Loaded) { outNext = PluginState::Initialized; return true; }
                return false;
            case LifecycleAction::Activate:
                if (from == PluginState::Initialized) { outNext = PluginState::Active; return true; }
                return false;
            case LifecycleAction::Suspend:
                if (from == PluginState::Active) { outNext = PluginState::Suspended; return true; }
                return false;
            case LifecycleAction::Resume:
                if (from == PluginState::Suspended) { outNext = PluginState::Active; return true; }
                return false;
            case LifecycleAction::Shutdown:
                // Any live/loaded/failed plugin may be shut down.
                if (from == PluginState::Loaded || from == PluginState::Initialized ||
                    from == PluginState::Active || from == PluginState::Suspended || from == PluginState::Failed)
                {
                    outNext = PluginState::Shutdown;
                    return true;
                }
                return false;
            case LifecycleAction::Unload:
                // Unload from a completed Shutdown, an early (pre-load) state, or a Failed one.
                if (from == PluginState::Shutdown || from == PluginState::Discovered ||
                    from == PluginState::Validated || from == PluginState::Failed)
                {
                    outNext = PluginState::Unloaded;
                    return true;
                }
                return false;
            }
            return false;
        }
    } // namespace

    bool PluginLifecycle::IsLegal(const PluginState from, const LifecycleAction action) noexcept
    {
        PluginState next{};
        return Resolve(from, action, next);
    }

    core::Expected<PluginState> PluginLifecycle::Next(const PluginState from, const LifecycleAction action)
    {
        PluginState next{};
        if (!Resolve(from, action, next))
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "illegal plugin lifecycle transition");
        }
        return next;
    }

    PluginOutcome PluginLifecycle::Apply(PluginState& state, const LifecycleAction action)
    {
        PluginState next{};
        if (!Resolve(state, action, next))
        {
            return PluginOutcome::RuntimeError; // illegal transition (logic error); state unchanged
        }
        state = next;
        return PluginOutcome::Ok;
    }
} // namespace stalkermp::plugin

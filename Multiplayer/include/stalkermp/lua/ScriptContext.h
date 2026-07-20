// STALKER-MP — Script context (Sprint-013, Step 4)
//
// The engine-free per-script value context: identity, descriptor, lifecycle state, and
// the ids of the callbacks the script has bound. Value-oriented — it holds only value
// ids and value shapes (no VM handle, no engine object, no live reference). It is a
// passive record; lifecycle transitions (Step 10) and orchestration (Step 13) are
// later steps.
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <vector>

#include "stalkermp/lua/LuaScriptTypes.h" // ScriptId / CallbackId / ScriptState / ScriptDescriptor

namespace stalkermp::lua
{
    // The value context of a single script known to the subsystem.
    struct ScriptContext
    {
        ScriptId id{};                       // 0 = none
        ScriptDescriptor descriptor{};       // descriptor (id/apiVersion/generation)
        ScriptState state = ScriptState::Unloaded;
        std::vector<CallbackId> callbacks;   // callbacks bound by this script (value ids)
    };
} // namespace stalkermp::lua

// STALKER-MP — Public API bundle seam (Sprint-013, Step 17)
//
// Composition-root helper: an owner of the seven Public API facade implementations that
// exposes them as one ScriptApiSet for the LuaManager to register. This lets Bootstrap
// request the whole API surface from a single factory (adapters::CreateEngineScriptApi)
// without naming the concrete facades — the engine build returns a bundle of the real
// EngineAdapters facades; the test build returns recording/null facades. Engine-free (no
// engine header enters the core).
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include "stalkermp/lua/ScriptApi.h" // ScriptApiSet

namespace stalkermp::lua
{
    // Owns the seven Public API facades and exposes them as one ScriptApiSet. The bundle
    // outlives the LuaManager/ScriptManager that use the returned set.
    class ILuaApiBundle
    {
    public:
        virtual ~ILuaApiBundle() = default;

        // The Public API surface as a single value handle (references into *this).
        [[nodiscard]] virtual ScriptApiSet Apis() noexcept = 0;
    };
} // namespace stalkermp::lua

// STALKER-MP — Lua manager (Sprint-013, Step 12)
//
// Coordinates the engine-free scripting runtime lifecycle: it owns a borrowed
// `ILuaRuntime` seam and, on Init, creates the state, loads the standard libraries, and
// registers the seven Public API facades (held as a `ScriptApiSet`) as named host
// functions through the seam. It exposes the runtime to the loader/manager. Engine-free
// — it holds only seams; the concrete VM binding is Step-17. It does NOT tick, dispatch
// events, or execute scripts (that is the ScriptManager, Step 13).
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include "stalkermp/lua/ILuaRuntime.h"     // ILuaRuntime / HostFunctionId
#include "stalkermp/lua/LuaScriptTypes.h"  // ScriptOutcome
#include "stalkermp/lua/ScriptApi.h"       // ScriptApiSet

namespace stalkermp::lua
{
    class LuaManager
    {
    public:
        LuaManager(ILuaRuntime& runtime, const ScriptApiSet& apis) noexcept : m_runtime(runtime), m_apis(apis) {}

        // Create the state, load standard libraries, and register the Public API facades
        // as host functions. Returns the first non-Ok outcome, else Ok (and marks the
        // manager initialized). Registering the same manager twice is AlreadyInitialized-
        // safe: a second Init with an existing state is a no-op returning Ok.
        [[nodiscard]] ScriptOutcome Init();

        // Destroy the runtime state (idempotent).
        void Shutdown() noexcept;

        [[nodiscard]] bool Initialized() const noexcept { return m_initialized; }

        // The managed runtime seam, exposed to the loader/manager.
        [[nodiscard]] ILuaRuntime& Runtime() noexcept { return m_runtime; }

        // The number of Public API facades registered as host functions (diagnostic).
        [[nodiscard]] std::size_t RegisteredApiCount() const noexcept { return m_registeredApis; }

    private:
        ILuaRuntime& m_runtime;
        ScriptApiSet m_apis;
        bool m_initialized = false;
        std::size_t m_registeredApis = 0;
    };
} // namespace stalkermp::lua

// STALKER-MP — Lua manager (Sprint-013, Step 12)
//
// See LuaManager.h. Engine-free / VM-free / OS-free; no exceptions, no RTTI; value
// outcomes. Registration binds the Public API surface names to host-function ids
// through the seam; the concrete native binding is the real runtime's job (Step 17).

#include "stalkermp/lua/LuaManager.h"

namespace stalkermp::lua
{
    namespace
    {
        // The sanctioned Public API surface names exposed to scripts (§7.4). Order is
        // fixed and deterministic; host-function ids are assigned 1..N in this order.
        constexpr const char* kApiNames[] = {"world",     "environment", "entity", "player",
                                             "inventory", "logging",     "config"};
        constexpr std::size_t kApiCount = sizeof(kApiNames) / sizeof(kApiNames[0]);
    } // namespace

    ScriptOutcome LuaManager::Init()
    {
        if (m_initialized)
        {
            return ScriptOutcome::Ok; // idempotent
        }
        if (const ScriptOutcome o = m_runtime.CreateState(); o != ScriptOutcome::Ok)
        {
            return o;
        }
        if (const ScriptOutcome o = m_runtime.LoadStandardLibraries(); o != ScriptOutcome::Ok)
        {
            return o;
        }

        // Register each Public API facade as a named host function (value id only). The
        // held ScriptApiSet routes these to the real facades in the engine build (Step 17).
        m_registeredApis = 0;
        for (std::size_t i = 0; i < kApiCount; ++i)
        {
            const HostFunctionId handle{static_cast<std::uint64_t>(i + 1)};
            if (const ScriptOutcome o = m_runtime.RegisterFunction(kApiNames[i], handle); o != ScriptOutcome::Ok)
            {
                return o;
            }
            ++m_registeredApis;
        }

        (void)m_apis; // held for the Step-17 native binding; not dereferenced here
        m_initialized = true;
        return ScriptOutcome::Ok;
    }

    void LuaManager::Shutdown() noexcept
    {
        if (!m_initialized)
        {
            return;
        }
        m_runtime.DestroyState();
        m_initialized = false;
        m_registeredApis = 0;
    }
} // namespace stalkermp::lua

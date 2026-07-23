// STALKER-MP — Null Lua seams (test build only)
//
// Test-build counterpart of the Lua composition-root factories defined in
// src/adapters/EngineAdapters.cpp (CreateEngineLuaRuntime / CreateEngineScriptApi /
// CreateEngineScriptSource). It provides the same factory symbols with no engine, VM, or
// filesystem dependency, so composition-root code (Bootstrap.cpp) is identical in both
// builds:
//
//   * CreateEngineLuaRuntime  -> the inert NullLuaRuntime.
//   * CreateEngineScriptApi   -> the recording NullLuaApiBundle.
//   * CreateEngineScriptSource -> the engine-free in-memory source (no filesystem).
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI.

#include "stalkermp/adapters/LuaSeams.h"

#include "stalkermp/lua/ILuaRuntime.h"          // lua::NullLuaRuntime
#include "stalkermp/lua/InMemoryScriptSource.h" // lua::InMemoryScriptSource
#include "stalkermp/lua/NullLuaApiBundle.h"     // lua::NullLuaApiBundle

namespace stalkermp::adapters
{
    std::unique_ptr<lua::ILuaRuntime> CreateEngineLuaRuntime()
    {
        return std::make_unique<lua::NullLuaRuntime>();
    }

    std::unique_ptr<lua::ILuaApiBundle> CreateEngineScriptApi()
    {
        return std::make_unique<lua::NullLuaApiBundle>();
    }

    std::unique_ptr<lua::IScriptSource> CreateEngineScriptSource(const lua::LuaConfiguration& /*config*/)
    {
        return std::make_unique<lua::InMemoryScriptSource>();
    }
} // namespace stalkermp::adapters

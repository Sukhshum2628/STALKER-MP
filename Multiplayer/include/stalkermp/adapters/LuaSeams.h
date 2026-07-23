// STALKER-MP — Lua composition-root seams (Sprint-013, Step 17)
//
// The three factories the composition root (Bootstrap.cpp) uses to obtain the
// build-appropriate scripting boundary WITHOUT naming the concrete engine/VM/platform
// types:
//
//   * CreateEngineLuaRuntime  — the concrete Lua VM binding. Engine build: the real
//     binding over the X-Ray engine's existing Lua runtime ([AR-2]); test build: the
//     inert NullLuaRuntime. Confined to EngineAdapters.cpp (One Engine/Platform Boundary).
//   * CreateEngineScriptApi   — the seven Public API facades as one bundle. Engine build:
//     the real EngineAdapters facades over the sanctioned Sprint-002/003/005/007/008
//     seams ([AR-4]); test build: recording/null facades. No engine type reaches the core.
//   * CreateEngineScriptSource — the script source over the configured directory token.
//     Engine build: the real filesystem source (PlatformScriptStore, ADR-009); test build:
//     the in-memory source.
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <memory>

#include "stalkermp/lua/ILuaApiBundle.h"    // lua::ILuaApiBundle
#include "stalkermp/lua/ILuaRuntime.h"      // lua::ILuaRuntime
#include "stalkermp/lua/IScriptSource.h"    // lua::IScriptSource
#include "stalkermp/lua/LuaConfiguration.h" // lua::LuaConfiguration

namespace stalkermp::adapters
{
    // The concrete Lua runtime seam (real VM binding / null in tests).
    [[nodiscard]] std::unique_ptr<lua::ILuaRuntime> CreateEngineLuaRuntime();

    // The Public API facades as a bundle (real engine facades / recording in tests).
    [[nodiscard]] std::unique_ptr<lua::ILuaApiBundle> CreateEngineScriptApi();

    // The script source over `config.scriptDirectoryToken` (real filesystem / in-memory
    // in tests).
    [[nodiscard]] std::unique_ptr<lua::IScriptSource> CreateEngineScriptSource(const lua::LuaConfiguration& config);
} // namespace stalkermp::adapters

// STALKER-MP — Null Public API bundle (Sprint-013, Step 17)
//
// The engine-free, inert ILuaApiBundle used by the test/default build in place of the
// real EngineAdapters facades. It owns a single RecordingScriptApi and exposes it as the
// whole ScriptApiSet, so composition-root code is identical in both builds and tests can
// inspect what scripts would have observed/effected. No VM, no engine, no OS.
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include "stalkermp/lua/ILuaApiBundle.h"
#include "stalkermp/lua/RecordingScriptApi.h"

namespace stalkermp::lua
{
    class NullLuaApiBundle final : public ILuaApiBundle
    {
    public:
        [[nodiscard]] ScriptApiSet Apis() noexcept override { return m_api.AsSet(); }

        // Inspection surface for tests.
        [[nodiscard]] RecordingScriptApi& Recorder() noexcept { return m_api; }

    private:
        RecordingScriptApi m_api;
    };
} // namespace stalkermp::lua

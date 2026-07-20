// STALKER-MP — Script source read seam (Sprint-013, Step 7)
//
// The additive engine-free READ boundary for script sources: enumerate available
// scripts, read a script's bytes, and test existence. It is engine-free and OS-free —
// the REAL filesystem backend is Step-08 (a single platform TU behind an engine-free
// factory); the default/test build uses the in-memory / null implementations here.
// Mirrors the Sprint-012 `ISaveSource` read-seam pattern (D-LUA4).
//
// This step introduces the interface + in-memory/null doubles ONLY — no platform
// filesystem, no loading orchestration (Step 11), no wiring (Step 17).
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream;
// value outcomes / Expected<T>.

#pragma once

#include <cstddef>
#include <vector>

#include "stalkermp/core/Expected.h"       // core::Expected
#include "stalkermp/lua/LuaScriptTypes.h"  // ScriptId / ScriptDescriptor

namespace stalkermp::lua
{
    class IScriptSource
    {
    public:
        virtual ~IScriptSource() = default;

        // All available scripts, ascending by script id (deterministic order).
        [[nodiscard]] virtual std::vector<ScriptDescriptor> Enumerate() const = 0;

        // The raw script bytes for `id`, or an error (NotFound) when absent.
        [[nodiscard]] virtual core::Expected<std::vector<std::byte>> Read(ScriptId id) const = 0;

        // Whether a script with `id` exists.
        [[nodiscard]] virtual bool Exists(ScriptId id) const = 0;
    };
} // namespace stalkermp::lua

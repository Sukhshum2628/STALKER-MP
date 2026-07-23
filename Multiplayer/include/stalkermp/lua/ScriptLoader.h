// STALKER-MP — Script loader (Sprint-013, Step 11)
//
// The engine-free deterministic loader that composes the previously-established seams:
// discover via `IScriptSource` (ascending), read bytes, validate via `ScriptValidator`
// (duplicate + version), LOAD the chunk into the `ILuaRuntime` seam (Step-3; a compile/
// load — NEVER Execute), register a `ScriptContext` into the `ScriptRegistry`, and
// advance its lifecycle (Load → Initialize) via `ScriptLifecycle`. A failure isolates
// the single offending script (recorded as a value outcome) and loading continues with
// the rest. No VM instantiation, no execution, no engine, no OS.
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "stalkermp/lua/ILuaRuntime.h"     // ILuaRuntime (chunk load, Step 3)
#include "stalkermp/lua/IScriptSource.h"   // IScriptSource (Step 7)
#include "stalkermp/lua/LuaScriptTypes.h"  // ScriptId / ScriptOutcome / ScriptDescriptor
#include "stalkermp/lua/ScriptRegistry.h"  // ScriptRegistry (Step 4)

namespace stalkermp::lua
{
    // The outcome of a batch load. `failures` records the per-script isolation
    // (id -> outcome) for every script that did not load, in deterministic order.
    struct ScriptLoadReport
    {
        std::size_t attempted = 0;
        std::size_t loaded = 0;
        std::size_t failed = 0;
        std::vector<std::pair<ScriptId, ScriptOutcome>> failures;
    };

    class ScriptLoader
    {
    public:
        ScriptLoader(IScriptSource& source, ILuaRuntime& runtime, ScriptRegistry& registry,
                     std::uint32_t currentApiVersion, bool strictApiVersion) noexcept
            : m_source(source), m_runtime(runtime), m_registry(registry),
              m_currentApiVersion(currentApiVersion), m_strictApiVersion(strictApiVersion)
        {
        }

        // Load every script the source enumerates, in ascending id order. A per-script
        // failure is isolated (recorded) and does not abort the batch.
        [[nodiscard]] ScriptLoadReport LoadAll();

        // Load a single script described by `descriptor`. Returns Ok, or the value
        // outcome of the first failing stage (nothing registered on failure).
        [[nodiscard]] ScriptOutcome LoadOne(const ScriptDescriptor& descriptor);

    private:
        IScriptSource& m_source;
        ILuaRuntime& m_runtime;
        ScriptRegistry& m_registry;
        std::uint32_t m_currentApiVersion;
        bool m_strictApiVersion;
    };
} // namespace stalkermp::lua

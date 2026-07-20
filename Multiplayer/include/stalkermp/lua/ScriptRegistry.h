// STALKER-MP — Script registry (Sprint-013, Step 4)
//
// The engine-free registry of `ScriptContext` records, keyed by `ScriptId`. It keeps
// deterministic ascending order, detects duplicate ids, and supports lookup and
// enumeration. Pure value component — no VM, no engine, no OS; every fallible
// operation returns a `ScriptOutcome` value outcome (ADR-007). Loading, validation,
// and orchestration are later steps; this holds registration state only.
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <vector>

#include "stalkermp/lua/LuaScriptTypes.h" // ScriptId / ScriptOutcome
#include "stalkermp/lua/ScriptContext.h"  // ScriptContext

namespace stalkermp::lua
{
    class ScriptRegistry
    {
    public:
        // Register a script context. Duplicate id -> DuplicateScript. A none (0) id is
        // rejected as NotFound (a registry entry must be addressable).
        [[nodiscard]] ScriptOutcome Register(const ScriptContext& context);

        // Remove a script by id. Missing -> NotFound.
        [[nodiscard]] ScriptOutcome Unregister(ScriptId id);

        // Lookup by id; nullptr when absent. The pointer is valid until the next
        // mutation of the registry.
        [[nodiscard]] const ScriptContext* Find(ScriptId id) const;
        [[nodiscard]] ScriptContext* Find(ScriptId id);

        // All contexts in ascending id order (value copies).
        [[nodiscard]] std::vector<ScriptContext> Enumerate() const;

        [[nodiscard]] std::size_t Count() const noexcept { return m_scripts.size(); }
        [[nodiscard]] bool Contains(ScriptId id) const noexcept;

    private:
        // Sorted ascending by id (deterministic enumeration; binary-search lookup).
        std::vector<ScriptContext> m_scripts;
    };
} // namespace stalkermp::lua

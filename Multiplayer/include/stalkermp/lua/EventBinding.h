// STALKER-MP — Event-binding registry (Sprint-013, Step 5)
//
// The engine-free table mapping each host-dispatched `ScriptEvent` to the ordered set
// of (script, callback) bindings that react to it. Scripts REACT to events; they never
// own, emit, or reorder them — this registry exposes only Bind / Unbind / query, and
// has no event-emission surface (§7.5). Dispatch order is deterministic (ascending by
// script id, then callback id). Pure value component: no VM, no engine, no OS; every
// fallible operation is a `ScriptOutcome` value outcome (ADR-007).
//
// This step introduces the registry ONLY — no dispatch through a runtime (Step 13),
// no engine-raised events (Step 17).
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <vector>

#include "stalkermp/lua/LuaScriptTypes.h" // ScriptEvent / ScriptId / CallbackId / ScriptOutcome

namespace stalkermp::lua
{
    // A single (script, callback) reaction, returned in deterministic order.
    struct EventCallback
    {
        ScriptId script{};
        CallbackId callback{};
    };

    class EventBinding
    {
    public:
        // Bind a callback to an event. A none (0) script or callback is InvalidCallback;
        // an exact duplicate (event, script, callback) is DuplicateScript.
        [[nodiscard]] ScriptOutcome Bind(ScriptEvent event, ScriptId script, CallbackId callback);

        // Remove a specific binding. Missing -> NotFound.
        [[nodiscard]] ScriptOutcome Unbind(ScriptEvent event, ScriptId script, CallbackId callback);

        // The callbacks bound to `event`, ascending by (script id, callback id).
        [[nodiscard]] std::vector<EventCallback> CallbacksFor(ScriptEvent event) const;

        [[nodiscard]] std::size_t Count() const noexcept { return m_entries.size(); }
        [[nodiscard]] std::size_t CountFor(ScriptEvent event) const noexcept;

    private:
        struct Entry
        {
            ScriptEvent event = ScriptEvent::OnServerStart;
            ScriptId script{};
            CallbackId callback{};
        };

        // Total order: event, then script id, then callback id (deterministic).
        [[nodiscard]] static bool Less(const Entry& a, const Entry& b) noexcept;

        // Sorted ascending by (event, script, callback).
        std::vector<Entry> m_entries;
    };
} // namespace stalkermp::lua

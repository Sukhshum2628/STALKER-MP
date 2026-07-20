// STALKER-MP — Event-binding registry (Sprint-013, Step 5)
//
// See EventBinding.h. Engine-free / VM-free / OS-free; no exceptions, no RTTI; value
// outcomes. Entries are kept in a vector sorted by (event, script, callback) so
// per-event queries are a contiguous ascending range and lookups are binary searches.

#include "stalkermp/lua/EventBinding.h"

#include <algorithm>
#include <cstdint>

namespace stalkermp::lua
{
    bool EventBinding::Less(const Entry& a, const Entry& b) noexcept
    {
        if (a.event != b.event)
        {
            return static_cast<std::uint8_t>(a.event) < static_cast<std::uint8_t>(b.event);
        }
        if (a.script.value != b.script.value)
        {
            return a.script.value < b.script.value;
        }
        return a.callback.value < b.callback.value;
    }

    ScriptOutcome EventBinding::Bind(const ScriptEvent event, const ScriptId script, const CallbackId callback)
    {
        if (script.IsNone() || callback.IsNone())
        {
            return ScriptOutcome::InvalidCallback;
        }
        const Entry candidate{event, script, callback};
        const auto it = std::lower_bound(m_entries.begin(), m_entries.end(), candidate, Less);
        if (it != m_entries.end() && !Less(candidate, *it) && !Less(*it, candidate))
        {
            return ScriptOutcome::DuplicateScript; // exact duplicate binding
        }
        m_entries.insert(it, candidate); // preserves the sort order
        return ScriptOutcome::Ok;
    }

    ScriptOutcome EventBinding::Unbind(const ScriptEvent event, const ScriptId script, const CallbackId callback)
    {
        const Entry key{event, script, callback};
        const auto it = std::lower_bound(m_entries.begin(), m_entries.end(), key, Less);
        if (it == m_entries.end() || Less(key, *it) || Less(*it, key))
        {
            return ScriptOutcome::NotFound;
        }
        m_entries.erase(it);
        return ScriptOutcome::Ok;
    }

    std::vector<EventCallback> EventBinding::CallbacksFor(const ScriptEvent event) const
    {
        std::vector<EventCallback> out;
        for (const Entry& e : m_entries)
        {
            if (e.event == event)
            {
                out.push_back(EventCallback{e.script, e.callback});
            }
        }
        return out; // ascending: m_entries is sorted and filtered in order
    }

    std::size_t EventBinding::CountFor(const ScriptEvent event) const noexcept
    {
        std::size_t n = 0;
        for (const Entry& e : m_entries)
        {
            if (e.event == event)
            {
                ++n;
            }
        }
        return n;
    }
} // namespace stalkermp::lua

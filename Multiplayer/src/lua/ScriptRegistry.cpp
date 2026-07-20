// STALKER-MP — Script registry (Sprint-013, Step 4)
//
// See ScriptRegistry.h. Engine-free / VM-free / OS-free; no exceptions, no RTTI;
// value outcomes. The backing store is a vector kept sorted ascending by ScriptId so
// enumeration is deterministic and lookup is a binary search.

#include "stalkermp/lua/ScriptRegistry.h"

#include <algorithm>

namespace stalkermp::lua
{
    namespace
    {
        // Lower-bound position by id (ascending order invariant).
        template <typename Vec>
        auto LowerBoundById(Vec& v, const ScriptId id)
        {
            return std::lower_bound(v.begin(), v.end(), id,
                                    [](const ScriptContext& c, const ScriptId key) { return c.id < key; });
        }
    } // namespace

    ScriptOutcome ScriptRegistry::Register(const ScriptContext& context)
    {
        if (context.id.IsNone())
        {
            return ScriptOutcome::NotFound; // an entry must be addressable
        }
        const auto it = LowerBoundById(m_scripts, context.id);
        if (it != m_scripts.end() && it->id == context.id)
        {
            return ScriptOutcome::DuplicateScript;
        }
        m_scripts.insert(it, context); // preserves ascending order
        return ScriptOutcome::Ok;
    }

    ScriptOutcome ScriptRegistry::Unregister(const ScriptId id)
    {
        const auto it = LowerBoundById(m_scripts, id);
        if (it == m_scripts.end() || it->id != id)
        {
            return ScriptOutcome::NotFound;
        }
        m_scripts.erase(it);
        return ScriptOutcome::Ok;
    }

    const ScriptContext* ScriptRegistry::Find(const ScriptId id) const
    {
        const auto it = LowerBoundById(m_scripts, id);
        if (it == m_scripts.end() || it->id != id)
        {
            return nullptr;
        }
        return &*it;
    }

    ScriptContext* ScriptRegistry::Find(const ScriptId id)
    {
        const auto it = LowerBoundById(m_scripts, id);
        if (it == m_scripts.end() || it->id != id)
        {
            return nullptr;
        }
        return &*it;
    }

    std::vector<ScriptContext> ScriptRegistry::Enumerate() const
    {
        return m_scripts; // already ascending by id
    }

    bool ScriptRegistry::Contains(const ScriptId id) const noexcept
    {
        const auto it = LowerBoundById(m_scripts, id);
        return it != m_scripts.end() && it->id == id;
    }
} // namespace stalkermp::lua

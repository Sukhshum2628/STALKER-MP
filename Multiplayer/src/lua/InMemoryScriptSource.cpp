// STALKER-MP — In-memory script source (Sprint-013, Step 7)
//
// See InMemoryScriptSource.h. Engine-free / VM-free / OS-free; no exceptions, no RTTI;
// value outcomes. Entries are kept sorted ascending by script id so enumeration is
// deterministic and lookup is a binary search.

#include "stalkermp/lua/InMemoryScriptSource.h"

#include <algorithm>
#include <utility>

#include "stalkermp/core/Error.h" // core::MakeError / ErrorCode

namespace stalkermp::lua
{
    namespace
    {
        template <typename Vec>
        auto LowerBoundById(Vec& v, const ScriptId id)
        {
            return std::lower_bound(
                v.begin(), v.end(), id,
                [](const typename Vec::value_type& e, const ScriptId key) { return e.descriptor.id < key; });
        }
    } // namespace

    void InMemoryScriptSource::Store(const ScriptDescriptor& descriptor, std::vector<std::byte> bytes)
    {
        const auto it = LowerBoundById(m_entries, descriptor.id);
        if (it != m_entries.end() && it->descriptor.id == descriptor.id)
        {
            it->descriptor = descriptor; // replace in place (order preserved)
            it->bytes = std::move(bytes);
            return;
        }
        m_entries.insert(it, Entry{descriptor, std::move(bytes)});
    }

    void InMemoryScriptSource::Remove(const ScriptId id) noexcept
    {
        const auto it = LowerBoundById(m_entries, id);
        if (it != m_entries.end() && it->descriptor.id == id)
        {
            m_entries.erase(it);
        }
    }

    std::vector<ScriptDescriptor> InMemoryScriptSource::Enumerate() const
    {
        std::vector<ScriptDescriptor> out;
        out.reserve(m_entries.size());
        for (const Entry& e : m_entries) // already ascending by id
        {
            out.push_back(e.descriptor);
        }
        return out;
    }

    core::Expected<std::vector<std::byte>> InMemoryScriptSource::Read(const ScriptId id) const
    {
        const auto it = LowerBoundById(m_entries, id);
        if (it == m_entries.end() || it->descriptor.id != id)
        {
            return core::MakeError(core::ErrorCode::NotFound, "script not found in in-memory source");
        }
        return it->bytes;
    }

    bool InMemoryScriptSource::Exists(const ScriptId id) const
    {
        const auto it = LowerBoundById(m_entries, id);
        return it != m_entries.end() && it->descriptor.id == id;
    }
} // namespace stalkermp::lua

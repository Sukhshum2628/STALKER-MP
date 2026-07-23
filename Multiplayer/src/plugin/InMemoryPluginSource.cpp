// STALKER-MP — In-memory plugin source (Sprint-014, Step 7)
//
// See InMemoryPluginSource.h. Engine-free / loader-free / OS-free; no exceptions, no
// RTTI; value outcomes. Manifests are kept sorted ascending by plugin id so enumeration
// is deterministic and lookup is a binary search.

#include "stalkermp/plugin/InMemoryPluginSource.h"

#include <algorithm>

#include "stalkermp/core/Error.h" // core::MakeError / ErrorCode

namespace stalkermp::plugin
{
    namespace
    {
        template <typename Vec>
        auto LowerBoundById(Vec& v, const PluginId id)
        {
            return std::lower_bound(v.begin(), v.end(), id,
                                    [](const PluginDescriptor& d, const PluginId key) { return d.id < key; });
        }
    } // namespace

    void InMemoryPluginSource::Store(const PluginDescriptor& descriptor)
    {
        const auto it = LowerBoundById(m_manifests, descriptor.id);
        if (it != m_manifests.end() && it->id == descriptor.id)
        {
            *it = descriptor; // replace in place (order preserved)
            return;
        }
        m_manifests.insert(it, descriptor);
    }

    void InMemoryPluginSource::Remove(const PluginId id) noexcept
    {
        const auto it = LowerBoundById(m_manifests, id);
        if (it != m_manifests.end() && it->id == id)
        {
            m_manifests.erase(it);
        }
    }

    std::vector<PluginDescriptor> InMemoryPluginSource::Enumerate() const
    {
        return m_manifests; // already ascending by id
    }

    core::Expected<PluginDescriptor> InMemoryPluginSource::ReadManifest(const PluginId id) const
    {
        const auto it = LowerBoundById(m_manifests, id);
        if (it == m_manifests.end() || it->id != id)
        {
            return core::MakeError(core::ErrorCode::NotFound, "plugin manifest not found in in-memory source");
        }
        return *it;
    }

    bool InMemoryPluginSource::Exists(const PluginId id) const
    {
        const auto it = LowerBoundById(m_manifests, id);
        return it != m_manifests.end() && it->id == id;
    }
} // namespace stalkermp::plugin

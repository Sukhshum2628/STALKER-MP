// STALKER-MP — Platform plugin store / static registration (Sprint-014, Step 8)
//
// The single plugin-side platform-boundary TU (ADR-009). Per [AR-P1] Option B, this
// performs STATIC IN-PROCESS REGISTRATION ONLY — it holds a process-global table of
// registered plugin manifests and produces a read-only discovery source over them. It
// contains NO OS/loader/filesystem code in Option B (dynamic library loading is deferred
// to a future sprint; this is the designated TU where it would later live). Discovery is
// metadata-only: no library is opened, no plugin is instantiated, nothing is validated,
// and no lifecycle is managed. Engine-free; no exceptions, no RTTI, no iostream.

#include "stalkermp/adapters/PlatformPluginStore.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "stalkermp/core/Error.h" // core::MakeError / ErrorCode

namespace stalkermp::adapters
{
    namespace
    {
        // The process-global static registration table. Function-local static so it has
        // no dynamic-init order dependency; owned here, in the single platform TU.
        std::vector<plugin::PluginDescriptor>& StaticTable() noexcept
        {
            static std::vector<plugin::PluginDescriptor> table;
            return table;
        }

        template <typename Vec>
        auto LowerBoundById(Vec& v, const plugin::PluginId id)
        {
            return std::lower_bound(
                v.begin(), v.end(), id,
                [](const plugin::PluginDescriptor& d, const plugin::PluginId key) { return d.id < key; });
        }

        // Read-only discovery source over a snapshot of the static registrations. Snapshot
        // is taken at construction and never mutated (read-only after construction).
        class StaticPluginStore final : public plugin::IPluginSource
        {
        public:
            explicit StaticPluginStore(std::vector<plugin::PluginDescriptor> snapshot)
                : m_manifests(std::move(snapshot))
            {
            }

            [[nodiscard]] std::vector<plugin::PluginDescriptor> Enumerate() const override
            {
                return m_manifests; // already ascending by id
            }

            [[nodiscard]] core::Expected<plugin::PluginDescriptor> ReadManifest(const plugin::PluginId id) const override
            {
                const auto it = LowerBoundById(m_manifests, id);
                if (it == m_manifests.end() || it->id != id)
                {
                    return core::MakeError(core::ErrorCode::NotFound, "static plugin manifest not found");
                }
                return *it;
            }

            [[nodiscard]] bool Exists(const plugin::PluginId id) const override
            {
                const auto it = LowerBoundById(m_manifests, id);
                return it != m_manifests.end() && it->id == id;
            }

        private:
            const std::vector<plugin::PluginDescriptor> m_manifests; // read-only snapshot (ascending id)
        };
    } // namespace

    plugin::PluginOutcome RegisterStaticPlugin(const plugin::PluginDescriptor& manifest)
    {
        if (manifest.id.IsNone())
        {
            return plugin::PluginOutcome::NotFound; // an addressable id is required
        }
        std::vector<plugin::PluginDescriptor>& table = StaticTable();
        const auto it = LowerBoundById(table, manifest.id);
        if (it != table.end() && it->id == manifest.id)
        {
            return plugin::PluginOutcome::DuplicatePlugin;
        }
        table.insert(it, manifest); // keep sorted ascending by id (deterministic)
        return plugin::PluginOutcome::Ok;
    }

    void ClearStaticPlugins() noexcept { StaticTable().clear(); }

    std::size_t StaticPluginCount() noexcept { return StaticTable().size(); }

    std::unique_ptr<plugin::IPluginSource> CreatePlatformPluginSource(const plugin::PluginConfiguration& config)
    {
        (void)config; // directory token unused in static-registration mode ([AR-P1] Option B)
        return std::unique_ptr<plugin::IPluginSource>(std::make_unique<StaticPluginStore>(StaticTable()));
    }
} // namespace stalkermp::adapters

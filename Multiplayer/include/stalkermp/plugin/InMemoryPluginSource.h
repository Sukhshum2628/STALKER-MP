// STALKER-MP — In-memory plugin source (Sprint-014, Step 7)
//
// An IPluginSource that holds plugin manifests in memory (no OS, no library, no load).
// It is the default/test discovery backend: tests populate it with plugin descriptors and
// observe enumerate/read/exists. The real platform backend is Step-08. Enumeration is
// deterministic (ascending plugin id). Engine-free / loader-free / OS-free. ADR-007.
//
// `FakePluginSource` is a convenience alias for this populatable double.

#pragma once

#include <cstddef>
#include <vector>

#include "stalkermp/plugin/IPluginSource.h"

namespace stalkermp::plugin
{
    class InMemoryPluginSource final : public IPluginSource
    {
    public:
        // Store (or replace) a plugin manifest. Kept ordered by plugin id.
        void Store(const PluginDescriptor& descriptor);

        // Remove a manifest by id (benign if absent).
        void Remove(PluginId id) noexcept;

        [[nodiscard]] std::size_t Count() const noexcept { return m_manifests.size(); }

        [[nodiscard]] std::vector<PluginDescriptor> Enumerate() const override;
        [[nodiscard]] core::Expected<PluginDescriptor> ReadManifest(PluginId id) const override;
        [[nodiscard]] bool Exists(PluginId id) const override;

    private:
        std::vector<PluginDescriptor> m_manifests; // kept sorted ascending by id
    };

    // Convenience alias: the in-memory source is the populatable "fake" discovery double.
    using FakePluginSource = InMemoryPluginSource;
} // namespace stalkermp::plugin

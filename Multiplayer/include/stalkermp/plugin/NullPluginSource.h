// STALKER-MP — Null plugin source (Sprint-014, Step 7)
//
// An inert IPluginSource that discovers no plugins: Enumerate is empty, ReadManifest
// fails with NotFound, Exists is false. The "no plugin source configured" discovery
// backend for builds without a source. Engine-free / loader-free / OS-free. ADR-007.

#pragma once

#include <vector>

#include "stalkermp/core/Error.h"           // core::MakeError / ErrorCode
#include "stalkermp/plugin/IPluginSource.h"

namespace stalkermp::plugin
{
    class NullPluginSource final : public IPluginSource
    {
    public:
        [[nodiscard]] std::vector<PluginDescriptor> Enumerate() const override { return {}; }

        [[nodiscard]] core::Expected<PluginDescriptor> ReadManifest(PluginId) const override
        {
            return core::MakeError(core::ErrorCode::NotFound, "no plugin source configured");
        }

        [[nodiscard]] bool Exists(PluginId) const override { return false; }
    };
} // namespace stalkermp::plugin

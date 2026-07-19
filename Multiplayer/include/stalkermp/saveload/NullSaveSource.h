// STALKER-MP — Null save source (Sprint-012, Step 8)
//
// An inert ISaveSource that holds no saves: Enumerate is empty, Read fails with
// NotFound, Exists is false. The "no storage configured" read backend for builds
// without a save source. Engine-free / OS-free. ADR-007.

#pragma once

#include <cstdint>
#include <vector>

#include "stalkermp/core/Error.h"              // core::MakeError / ErrorCode
#include "stalkermp/saveload/ISaveSource.h"

namespace stalkermp::saveload
{
    class NullSaveSource final : public ISaveSource
    {
    public:
        [[nodiscard]] std::vector<SaveDescriptor> Enumerate() const override { return {}; }

        [[nodiscard]] core::Expected<std::vector<std::uint8_t>> Read(std::uint64_t) const override
        {
            return core::MakeError(core::ErrorCode::NotFound, "no save source configured");
        }

        [[nodiscard]] bool Exists(std::uint64_t) const override { return false; }
    };
} // namespace stalkermp::saveload

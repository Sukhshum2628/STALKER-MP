// STALKER-MP — Base interfaces (Sprint-001, §7.8)
//
// ISerializable: anything convertible to and from a byte buffer.
// Sprint-001 defines only the contract; concrete stream/packet formats
// belong to the Replication (Sprint-008+) and Persistence (Sprint-011+)
// subsystems and must not be invented here.
//
// PROVISIONAL until Sprint-008 / Sprint-011: the serialization format is
// not finalized, so this signature may be revised by those sprints without
// a deprecation period (recorded in docs/Decisions.md). Do not build
// load-bearing abstractions on it before then.

#pragma once

#include <cstddef>
#include <vector>

#include "stalkermp/core/Expected.h"

namespace stalkermp::core
{
    class ISerializable
    {
    public:
        virtual ~ISerializable() = default;

        // Appends this object's serialized representation to buffer.
        [[nodiscard]] virtual Expected<void> Serialize(std::vector<std::byte>& buffer) const = 0;

        // Reconstructs this object from exactly [data, data + size).
        [[nodiscard]] virtual Expected<void> Deserialize(const std::byte* data, std::size_t size) = 0;
    };
} // namespace stalkermp::core

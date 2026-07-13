// STALKER-MP — Snapshot read-only view interface (Sprint-008, Step 3)
//
// The single CONSUMER-FACING surface for a snapshot: const-only accessors over
// the captured metadata, entities (ascending EntityId), players (ascending
// PlayerId), and environment (Architecture §9/§10). Consumers depend ONLY on
// this interface and therefore CANNOT mutate a snapshot — immutability is
// structural at the type level (there is no non-const method here).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI on the consumer path.
// No pool, builder, queue, manager, or diagnostics.

#pragma once

#include <vector>

#include "stalkermp/snapshot/SnapshotTypes.h"

namespace stalkermp::snapshot
{
    class ISnapshotView
    {
    public:
        virtual ~ISnapshotView() = default;

        [[nodiscard]] virtual const SnapshotMetadata& Metadata() const = 0;
        [[nodiscard]] virtual const std::vector<EntitySnapshot>& Entities() const = 0;   // ascending EntityId
        [[nodiscard]] virtual const std::vector<PlayerSnapshot>& Players() const = 0;     // ascending PlayerId
        [[nodiscard]] virtual const EnvironmentSnapshot& Environment() const = 0;
    };
} // namespace stalkermp::snapshot

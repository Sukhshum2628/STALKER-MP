// STALKER-MP — Replication update view (Sprint-009, Step 3)
//
// The const-only consumer interface over an immutable per-client ReplicationUpdate
// — the "ReplicationSnapshot" projection reserved in the Sprint-008 architecture
// (§22). Consumers hold a `const IReplicationView&` and cannot mutate the update
// at all (structural immutability). Entities are exposed ascending by EntityId,
// players ascending by PlayerId.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <vector>

#include "stalkermp/replication/ReplicationTypes.h"

namespace stalkermp::replication
{
    class IReplicationView
    {
    public:
        virtual ~IReplicationView() = default;

        [[nodiscard]] virtual const ReplicationMetadata& Metadata() const = 0;
        [[nodiscard]] virtual const std::vector<EntityReplicationState>& Entities() const = 0; // ascending EntityId
        [[nodiscard]] virtual const std::vector<PlayerReplicationState>& Players() const = 0;   // ascending PlayerId
    };
} // namespace stalkermp::replication

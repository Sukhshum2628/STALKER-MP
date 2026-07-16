// STALKER-MP — Interest policy seam (Sprint-009, Step 5)
//
// The engine-free seam that selects the entities relevant to a client for
// replication. Consumed ONLY by the replication worker (Step 10). It reads an
// immutable snapshot (Sprint-008 ISnapshotView) and the client's focus/baseline
// (Sprint-009 ClientRecord) and APPENDS the relevant EntityIds — ascending,
// unique — to `out` (append-only: it never clears pre-existing output). Values
// only; no engine access, no mutation.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <vector>

#include "stalkermp/replication/ReplicationClientRegistry.h" // replication::ClientRecord
#include "stalkermp/snapshot/ISnapshotView.h"                // snapshot::ISnapshotView
#include "stalkermp/world/EntityView.h"                      // world::EntityId

namespace stalkermp::replication
{
    class IInterestPolicy
    {
    public:
        virtual ~IInterestPolicy() = default;

        // Append the EntityIds relevant to `client` for `snapshot`, ascending and
        // unique. Append-only: pre-existing entries in `out` are preserved.
        virtual void SelectRelevant(const ClientRecord& client, const snapshot::ISnapshotView& snapshot,
                                    std::vector<world::EntityId>& out) const = 0;
    };
} // namespace stalkermp::replication

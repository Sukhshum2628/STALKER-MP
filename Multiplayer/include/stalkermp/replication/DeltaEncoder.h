// STALKER-MP — Replication delta generation (Sprint-009, Step 6)
//
// Deterministic, pure, value-in/value-out delta: given a client's previous
// baseline and the current relevant states, produce the minimal change set
// (added / changed / removed for entities; changed for players) with ascending
// ordering and per-entity version bumps. Unchanged entities are omitted
// (bandwidth minimization). Bounded by maxEntitiesPerUpdate (Overflow value
// outcome). No engine access, no simulation mutation, no I/O.
//
// Inputs are assumed ascending-unique by id (as produced by the Step-4 registry
// baseline and the Step-5 interest selection); a two-pointer merge keeps the
// output deterministic and O(n).
//
// This step introduces the encoder ONLY — no classifier, queue, packet, worker,
// manager, service/tick, thread, or wire behavior.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; Expected<T>.

#pragma once

#include <cstdint>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/replication/ReplicationTypes.h"
#include "stalkermp/world/EntityView.h" // world::EntityId

namespace stalkermp::replication
{
    // The minimal per-client change set. All lists ascending by id.
    struct ReplicationChangeSet
    {
        std::vector<EntityReplicationState> added;    // present in current, absent in baseline (version = 1)
        std::vector<EntityReplicationState> changed;  // present in both, tracked fields differ (version bumped)
        std::vector<world::EntityId> removed;         // present in baseline, absent in current
        std::vector<PlayerReplicationState> playersChanged; // added or tracked-field-changed players
    };

    // Encode the entity delta between a client's baseline and the current relevant
    // states. Tracked fields for change detection: position, velocity, stateFlags.
    // Added entities are stamped version 1; changed entities baselineVersion + 1;
    // unchanged entities are omitted. Fails with Overflow (value outcome) when
    // added + changed exceeds maxEntitiesPerUpdate.
    [[nodiscard]] core::Expected<ReplicationChangeSet>
    EncodeEntityDelta(const std::vector<EntityReplicationState>& baseline,
                      const std::vector<EntityReplicationState>& current, std::uint32_t maxEntitiesPerUpdate);

    // Encode the player delta: added or tracked-field-changed players (position,
    // connectionState, authorityFlags), ascending by PlayerId. Added players are
    // stamped version 1; changed players baselineVersion + 1.
    [[nodiscard]] std::vector<PlayerReplicationState>
    EncodePlayerDelta(const std::vector<PlayerReplicationState>& baseline,
                      const std::vector<PlayerReplicationState>& current);

    // Compute the baseline to store after this update: the current entity set with
    // each entity stamped with its emitted version (new = 1, changed =
    // baselineVersion + 1, unchanged = baselineVersion). Ascending by EntityId.
    [[nodiscard]] std::vector<EntityReplicationState>
    NextEntityBaseline(const std::vector<EntityReplicationState>& baseline,
                       const std::vector<EntityReplicationState>& current);
} // namespace stalkermp::replication

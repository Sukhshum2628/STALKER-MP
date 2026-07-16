// STALKER-MP — Immutable replication update (Sprint-009, Step 3)
//
// The immutable per-client aggregate implementing IReplicationView. Built once
// into its own storage (BeginBuild -> AddEntity/AddPlayer/SetReliability ->
// Finalize) and IMMUTABLE at Finalize: every mutating operation is gated by the
// Pending/Built -> ... state and returns a value outcome once finalized (E-G1-R).
// Consumers hold a `const IReplicationView&` / `const ReplicationUpdate&` and thus
// cannot mutate it at all (structural immutability). Deterministic ordering is
// enforced: entities ascending by EntityId, players ascending by PlayerId;
// duplicates/out-of-order/id-0 are rejected.
//
// This step introduces the aggregate + view + immutability gate ONLY — no client
// registry, interest, delta, classifier, queue, packet, worker, manager,
// diagnostics, service/tick, thread, or wire integration.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; Expected<T>.

#pragma once

#include <cstdint>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/replication/IReplicationView.h"
#include "stalkermp/replication/ReplicationTypes.h"

namespace stalkermp::replication
{
    class ReplicationUpdate final : public IReplicationView
    {
    public:
        ReplicationUpdate() = default;

        // --- Build API (valid ONLY while Building, i.e. state == Built) --------

        // Start (or restart, for buffer reuse) a build cycle: resets content and
        // enters the building state with the given identity/client/provenance.
        void BeginBuild(ReplicationId id, ClientId client, std::uint64_t sourceSnapshotId,
                        std::uint64_t sourceSnapshotTick) noexcept;

        // Append an entity state. Requires Building; enforces ascending, unique
        // EntityId. Out-of-order/duplicate/none-id or non-Building => InvalidArgument.
        [[nodiscard]] core::Expected<void> AddEntity(const EntityReplicationState& entity);

        // Append a player state. Requires Building; ascending, unique PlayerId.
        [[nodiscard]] core::Expected<void> AddPlayer(const PlayerReplicationState& player);

        // Set the update's reliability class. Requires Building.
        [[nodiscard]] core::Expected<void> SetReliability(ReplicationReliability reliability);

        // Seal the update: Building -> Finalized. Stamps entity/player counts.
        // After this, all mutating operations are rejected (immutable). A finalize
        // when not Building => InvalidArgument.
        [[nodiscard]] core::Expected<void> Finalize();

        [[nodiscard]] ReplicationState State() const noexcept { return m_metadata.state; }
        [[nodiscard]] bool IsFinalized() const noexcept { return m_finalized; }

        // --- IReplicationView (const-only consumer surface) --------------------
        [[nodiscard]] const ReplicationMetadata& Metadata() const override { return m_metadata; }
        [[nodiscard]] const std::vector<EntityReplicationState>& Entities() const override { return m_entities; }
        [[nodiscard]] const std::vector<PlayerReplicationState>& Players() const override { return m_players; }

    private:
        ReplicationMetadata m_metadata{};
        std::vector<EntityReplicationState> m_entities; // ascending by EntityId
        std::vector<PlayerReplicationState> m_players;  // ascending by PlayerId
        bool m_finalized = false;
    };
} // namespace stalkermp::replication

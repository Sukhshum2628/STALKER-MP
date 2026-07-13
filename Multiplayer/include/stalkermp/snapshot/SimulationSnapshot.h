// STALKER-MP — Immutable simulation snapshot (Sprint-008, Step 3)
//
// The immutable per-tick aggregate implementing ISnapshotView. It is built once
// into its own storage (BeginBuild -> Add* / SetEnvironment -> Finalize) and
// becomes IMMUTABLE at Finalize: every mutating operation is gated by the
// Building -> Finalized state transition and returns a value outcome once the
// snapshot is finalized (E-G1-S). Consumers hold a `const ISnapshotView&` /
// `const SimulationSnapshot&` and thus cannot mutate it at all (structural
// immutability). Deterministic ordering is enforced: entities ascending by
// EntityId, players ascending by PlayerId; duplicates/out-of-order are rejected.
//
// This step introduces the aggregate + view + immutability gate ONLY — no pool,
// builder, queue, manager, engine seam, Bootstrap wiring, or diagnostics.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; Expected<T>.

#pragma once

#include <cstdint>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/snapshot/ISnapshotView.h"
#include "stalkermp/snapshot/SnapshotTypes.h"

namespace stalkermp::snapshot
{
    class SimulationSnapshot final : public ISnapshotView
    {
    public:
        SimulationSnapshot() = default;

        // --- Build API (valid ONLY while Building) ----------------------------

        // Start (or restart, for buffer reuse) a build cycle: resets content and
        // enters the Building state with the given identity/tick/version.
        void BeginBuild(SnapshotId id, std::uint64_t simulationTick, std::uint32_t version) noexcept;

        // Append an entity capture. Requires Building; enforces ascending, unique
        // EntityId. Out-of-order/duplicate/none-id or non-Building => InvalidArgument.
        [[nodiscard]] core::Expected<void> AddEntity(const EntitySnapshot& entity);

        // Append a player capture. Requires Building; ascending, unique PlayerId.
        [[nodiscard]] core::Expected<void> AddPlayer(const PlayerSnapshot& player);

        // Set the captured environment. Requires Building.
        [[nodiscard]] core::Expected<void> SetEnvironment(const EnvironmentSnapshot& environment);

        // Seal the snapshot: Building -> Finalized. Stamps entity/player counts.
        // After this, all mutating operations are rejected (immutable). A finalize
        // when not Building => InvalidArgument.
        [[nodiscard]] core::Expected<void> Finalize();

        [[nodiscard]] SnapshotState State() const noexcept { return m_metadata.state; }
        [[nodiscard]] bool IsFinalized() const noexcept { return m_metadata.state != SnapshotState::Building; }

        // --- ISnapshotView (const-only consumer surface) ----------------------
        [[nodiscard]] const SnapshotMetadata& Metadata() const override { return m_metadata; }
        [[nodiscard]] const std::vector<EntitySnapshot>& Entities() const override { return m_entities; }
        [[nodiscard]] const std::vector<PlayerSnapshot>& Players() const override { return m_players; }
        [[nodiscard]] const EnvironmentSnapshot& Environment() const override { return m_environment; }

    private:
        SnapshotMetadata m_metadata{};
        std::vector<EntitySnapshot> m_entities; // ascending by EntityId
        std::vector<PlayerSnapshot> m_players;  // ascending by PlayerId
        EnvironmentSnapshot m_environment{};
    };
} // namespace stalkermp::snapshot

// STALKER-MP — Snapshot manager (Sprint-008, Step 8)
//
// The per-tick snapshot lifecycle service: an `IService` + `ITickable` that owns
// the SnapshotPool, SnapshotBuilder, and SnapshotQueue and, once per simulation
// tick at the reserved `tick_order::kReplication = 400` slot, runs the full
// capture pipeline: BeginBuild -> Capture -> Finalize -> Publish. Exactly one
// snapshot is published per tick. On any value outcome from the builder/pool
// (e.g. PoolExhausted or Overflow) the tick is skipped and the previously
// published snapshot remains valid (Implementation Plan §Step-8 invariant).
//
// Ordering-only dependencies (registration order = init order): World,
// EntityRegistry, BubbleManager, TransitionManager, PlayerManager — all
// registered earlier so the manager initializes after every simulation producer.
// The capture surfaces (IEntitySnapshotSource, the Sprint-007 read-only
// PlayerManager, the Sprint-002 IEnvironmentSource) are injected by reference.
//
// This step introduces the manager ONLY — no engine adapter, no Bootstrap /
// FrameDispatcher wiring (Step 10), and no diagnostics content (Step 12).
//
// Engine-free / OS-free. ADR-007 (no exceptions/RTTI; value outcomes);
// ADR-011 (single-threaded; no thread created).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/FrameDispatcher.h"  // core::tick_order::kReplication
#include "stalkermp/core/interfaces/IService.h"  // core::IService
#include "stalkermp/core/interfaces/ITickable.h"
#include "stalkermp/player/PlayerManager.h"  // Sprint-007 read-only surface
#include "stalkermp/snapshot/SnapshotBuilder.h"
#include "stalkermp/snapshot/SnapshotConfiguration.h"
#include "stalkermp/snapshot/SnapshotPool.h"
#include "stalkermp/snapshot/SnapshotQueue.h"
#include "stalkermp/snapshot/SnapshotTypes.h"
#include "stalkermp/world/IEntitySnapshotSource.h"
#include "stalkermp/world/IEnvironmentSource.h"

namespace stalkermp::snapshot
{
    class SnapshotManager final : public core::IService, public core::ITickable
    {
    public:
        SnapshotManager(const SnapshotConfiguration& config,
                        const world::IEntitySnapshotSource& entitySource,
                        const player::PlayerManager& playerSurface,
                        const world::IEnvironmentSource& environmentSource)
            : m_config(config), m_entitySource(entitySource), m_playerSurface(playerSurface),
              m_environmentSource(environmentSource), m_builder(config), m_queue(m_pool)
        {
        }

        // --- core::IService / ISubsystem -------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "SnapshotManager"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override
        {
            // Ordering-only edges: every simulation producer must be registered
            // earlier so the manager ticks at kReplication = 400 after them.
            return {"World", "EntityRegistry", "BubbleManager", "TransitionManager", "PlayerManager"};
        }
        [[nodiscard]] core::Expected<void> Initialize() override; // reserves the pool
        void Shutdown() noexcept override;

        // --- core::ITickable --------------------------------------------------
        // Invoked once per frame at tick_order::kReplication = 400 (wired in
        // Step 10). Runs BeginBuild -> Capture -> Finalize -> Publish exactly once.
        void Tick(double deltaSeconds) override;

        // --- Read-only surface ------------------------------------------------
        [[nodiscard]] const SnapshotMetadata& LatestMetadata() const noexcept { return m_lastMetadata; }
        [[nodiscard]] SnapshotStatistics Statistics() const noexcept;
        [[nodiscard]] SnapshotConsistency ValidateConsistency() const noexcept;
        [[nodiscard]] const SnapshotQueue& Queue() const noexcept { return m_queue; }
        [[nodiscard]] std::uint64_t LastTick() const noexcept { return m_tick; }

        // The frozen ordering slot this manager ticks at (Bootstrap subscribes here
        // in Step 10). Exposed as a compile-time constant for placement assertions.
        static constexpr std::uint32_t TickOrder() noexcept { return core::tick_order::kReplication; }

    private:
        SnapshotConfiguration m_config;
        const world::IEntitySnapshotSource& m_entitySource;   // capture seam (Step 5)
        const player::PlayerManager& m_playerSurface;         // Sprint-007 read-only
        const world::IEnvironmentSource& m_environmentSource; // Sprint-002

        // Declaration order matters: the pool is constructed before the queue
        // (which borrows it) and before/after the builder (which takes the pool
        // per-call at BeginBuild).
        SnapshotPool m_pool;
        SnapshotBuilder m_builder;
        SnapshotQueue m_queue;

        std::uint64_t m_tick = 0;            // monotonic simulation tick
        std::uint64_t m_built = 0;           // snapshots finalized
        std::uint64_t m_publishedCount = 0;  // snapshots published (one per successful tick)
        SnapshotMetadata m_lastMetadata{};   // metadata of the last published snapshot
    };
} // namespace stalkermp::snapshot

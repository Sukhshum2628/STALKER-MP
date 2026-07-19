// STALKER-MP — Persistence manager (Sprint-011, Step 11)
//
// Owns the persistence subsystem and orchestrates it once per authoritative frame:
// it owns the SaveScheduler, PersistenceQueue, PersistenceWorker, and VersionManager,
// and holds references to the Sprint-008 SnapshotQueue (read-only) and the Step-08
// IPersistenceStore seam. Each Tick runs one deterministic, SYNCHRONOUS pass —
// schedule → enqueue → Acquire the current snapshot → worker Flush → Release — with
// no thread and no new tick_order key (it will be wired at kPersistence = 500 in
// Step 14). Read-only snapshot consumption; never mutates authoritative state
// (Host Authority; ADR-008; ADR-011).
//
// This step introduces the manager ONLY — no Bootstrap wiring (Step 14) and no
// diagnostics collector (Step 13).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (PersistenceOutcome).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/FrameDispatcher.h"          // core::tick_order::kPersistence
#include "stalkermp/core/interfaces/IService.h"
#include "stalkermp/core/interfaces/ITickable.h"
#include "stalkermp/persistence/IPersistenceStore.h"
#include "stalkermp/persistence/PersistenceConfiguration.h"
#include "stalkermp/persistence/PersistenceQueue.h"
#include "stalkermp/persistence/PersistenceTypes.h"
#include "stalkermp/persistence/PersistenceWorker.h"
#include "stalkermp/persistence/SaveScheduler.h"
#include "stalkermp/persistence/VersionManager.h"
#include "stalkermp/snapshot/SnapshotQueue.h"        // snapshot::SnapshotQueue (read-only source)

namespace stalkermp::persistence
{
    class PersistenceManager final : public core::IService, public core::ITickable
    {
    public:
        PersistenceManager(const PersistenceConfiguration& config, snapshot::SnapshotQueue& snapshotSource,
                           IPersistenceStore& store, const VersionManager& versions = VersionManager{})
            : m_config(config), m_snapshotSource(snapshotSource), m_store(store), m_versions(versions),
              m_scheduler(config), m_queue(config)
        {
        }

        // --- core::IService / ISubsystem -------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "Persistence"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override
        {
            // Ordering-only edges: persistence ticks after every producer and the
            // snapshot/replication consumers (kPersistence = 500 > 450).
            return {"World",         "EntityRegistry", "BubbleManager", "TransitionManager", "PlayerManager",
                    "Networking",    "SnapshotManager", "Replication"};
        }
        [[nodiscard]] core::Expected<void> Initialize() override;
        void Shutdown() noexcept override;

        // --- core::ITickable --------------------------------------------------
        // One synchronous pass per frame (wired at kPersistence = 500 in Step 14).
        // `deltaSeconds` is intentionally NOT used in control flow (tick-driven).
        void Tick(double deltaSeconds) override
        {
            (void)deltaSeconds;
            ++m_tick;
            Update(m_tick);
        }

        // Request a save through the scheduler (serviced on the next Tick, in priority
        // order). Supports Manual/Administrative/Shutdown; other triggers are not
        // caller-initiated (Autosave is periodic; Deferred uses DeferSave).
        [[nodiscard]] PersistenceOutcome RequestSave(SaveTrigger trigger) noexcept;

        // Schedule a deferred save to fire on the first Tick at/after `untilTick`.
        void DeferSave(std::uint64_t untilTick) noexcept { m_scheduler.Defer(untilTick); }

        // --- Read-only surface ------------------------------------------------
        [[nodiscard]] PersistenceStatistics Statistics() const noexcept;
        [[nodiscard]] PersistenceConsistency Consistency() const noexcept { return PersistenceConsistency{}; }
        [[nodiscard]] std::uint64_t Ticks() const noexcept { return m_tick; }
        [[nodiscard]] std::size_t QueueDepth() const noexcept { return m_queue.Size(); }
        [[nodiscard]] const PersistenceWorker& Worker() const noexcept { return m_worker; }

        static constexpr std::uint32_t TickOrder() noexcept { return core::tick_order::kPersistence; }

    private:
        // One deterministic pass: scheduler → enqueue → acquire snapshot → flush →
        // release. Read-only snapshot consumption.
        void Update(std::uint64_t tick) noexcept;

        PersistenceConfiguration m_config;
        snapshot::SnapshotQueue& m_snapshotSource; // Sprint-008 published snapshots (read-only)
        IPersistenceStore& m_store;                // Step-08 storage seam
        VersionManager m_versions;
        SaveScheduler m_scheduler;
        PersistenceQueue m_queue;
        PersistenceWorker m_worker;

        std::uint64_t m_tick = 0;
        std::uint64_t m_autosaves = 0;      // autosaves emitted by the scheduler
        std::uint64_t m_scheduledRequests = 0; // total requests the scheduler emitted
    };
} // namespace stalkermp::persistence

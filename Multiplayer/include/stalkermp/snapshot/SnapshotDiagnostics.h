// STALKER-MP — Snapshot diagnostics (Sprint-008, Step 11)
//
// A READ-ONLY, replay-stable inspector over the SnapshotManager. It derives every
// report from the manager's existing const surface (Statistics / LatestMetadata /
// ValidateConsistency / Queue) plus the immutable current published snapshot —
// it never mutates the pool, queue, builder, manager, or any simulation state,
// and it captures no live object. iostream-free: textual descriptions use
// common::Format (Architecture §20; ADR-007).
//
// Reserved (unpopulated) per Architecture §22: bandwidth / latency / worker
// fields, and a multi-entry build-history ring — the manager retains only the
// latest published snapshot (latest-wins), so BuildHistory reports the latest
// build; a deeper ring is a later-sprint extension.
//
// Engine-free / OS-free.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "stalkermp/snapshot/SnapshotManager.h"
#include "stalkermp/snapshot/SnapshotTypes.h"

namespace stalkermp::snapshot
{
    // Current queue occupancy + publication tally (read-only view).
    struct QueueReport
    {
        std::uint32_t depth = 0;       // current queue depth (0 or 1; latest-wins)
        std::uint64_t published = 0;   // monotonic count of publications
        SnapshotId currentId{};        // id of the current published snapshot (0 = none)
        std::uint64_t currentTick = 0; // simulation tick of the current snapshot
        bool hasCurrent = false;       // whether a snapshot is currently published
    };

    // Pool utilization (bounded-memory model). Byte-level accounting is reserved.
    struct MemoryReport
    {
        std::uint32_t poolInUse = 0;
        std::uint32_t poolCapacity = 0;
        std::uint32_t poolFree = 0; // capacity - inUse
    };

    // A single build summary, tick-indexed (deterministic; no wall-clock).
    struct BuildRecord
    {
        std::uint64_t tick = 0;
        SnapshotId id{};
        std::uint32_t entityCount = 0;
        std::uint32_t playerCount = 0;
        std::uint64_t buildDurationTicks = 0;
    };

    // A read-only dump of a specific snapshot (the current published one when its
    // id matches). `available` is false when the requested id is not retained.
    struct SnapshotDump
    {
        bool available = false;
        SnapshotMetadata metadata{};
        std::uint32_t entityCount = 0;
        std::uint32_t playerCount = 0;
        std::string text; // compact deterministic description (common::Format)
    };

    class SnapshotDiagnostics
    {
    public:
        explicit SnapshotDiagnostics(const SnapshotManager& manager) noexcept : m_manager(manager) {}

        // Aggregate tallies (built/published/retired, queue depth, pool in-use/
        // capacity, last build duration, entity/player counts).
        [[nodiscard]] SnapshotStatistics Statistics() const;

        // Human-readable one-line-ish summary of the current snapshot state.
        [[nodiscard]] std::string DescribeState() const;

        // Dump the snapshot with `id` when it is the current published snapshot;
        // otherwise SnapshotDump::available == false.
        [[nodiscard]] SnapshotDump DumpSnapshot(SnapshotId id) const;

        // Recent builds, tick-indexed. Latest build only (see header note); empty
        // when nothing has been published.
        [[nodiscard]] std::vector<BuildRecord> BuildHistory() const;

        // Current queue occupancy + publication tally.
        [[nodiscard]] QueueReport QueueStatus() const;

        // Pool utilization.
        [[nodiscard]] MemoryReport MemoryUsage() const;

        // Read-only consistency audit (ordering, id monotonicity, no duplicate ids,
        // counts match, no retired-buffer aliasing, immutability flag set).
        [[nodiscard]] SnapshotConsistency ValidateConsistency() const;

    private:
        const SnapshotManager& m_manager; // non-owning; read-only
    };
} // namespace stalkermp::snapshot

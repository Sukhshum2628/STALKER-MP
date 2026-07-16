// STALKER-MP — Snapshot diagnostics (Sprint-008, Step 11)
//
// See SnapshotDiagnostics.h. Every report is derived from the SnapshotManager's
// const surface and the immutable current published snapshot; nothing is mutated
// and no live object is captured. iostream-free (common::Format).

#include "stalkermp/snapshot/SnapshotDiagnostics.h"

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::snapshot
{
    SnapshotStatistics SnapshotDiagnostics::Statistics() const
    {
        return m_manager.Statistics();
    }

    QueueReport SnapshotDiagnostics::QueueStatus() const
    {
        const SnapshotQueue& queue = m_manager.Queue();
        QueueReport report{};
        report.depth = static_cast<std::uint32_t>(queue.Depth());
        report.published = queue.Published();
        if (const SimulationSnapshot* const current = queue.Current())
        {
            report.hasCurrent = true;
            report.currentId = current->Metadata().id;
            report.currentTick = current->Metadata().simulationTick;
        }
        return report;
    }

    MemoryReport SnapshotDiagnostics::MemoryUsage() const
    {
        const SnapshotStatistics stats = m_manager.Statistics();
        MemoryReport report{};
        report.poolInUse = stats.poolInUse;
        report.poolCapacity = stats.poolCapacity;
        report.poolFree = stats.poolCapacity >= stats.poolInUse ? (stats.poolCapacity - stats.poolInUse) : 0u;
        return report;
    }

    std::vector<BuildRecord> SnapshotDiagnostics::BuildHistory() const
    {
        std::vector<BuildRecord> history;
        const SnapshotQueue& queue = m_manager.Queue();
        if (queue.Current() != nullptr) // something has been published
        {
            const SnapshotMetadata& meta = m_manager.LatestMetadata();
            BuildRecord record{};
            record.tick = meta.simulationTick;
            record.id = meta.id;
            record.entityCount = meta.entityCount;
            record.playerCount = meta.playerCount;
            record.buildDurationTicks = meta.buildDurationTicks;
            history.push_back(record);
        }
        return history;
    }

    SnapshotDump SnapshotDiagnostics::DumpSnapshot(const SnapshotId id) const
    {
        SnapshotDump dump{};
        const SimulationSnapshot* const current = m_manager.Queue().Current();
        if (current == nullptr || !(current->Metadata().id == id))
        {
            dump.text = common::Format("snapshot {}: not available", id.value);
            return dump; // available == false
        }

        const ISnapshotView& view = *current;
        dump.available = true;
        dump.metadata = view.Metadata();
        dump.entityCount = static_cast<std::uint32_t>(view.Entities().size());
        dump.playerCount = static_cast<std::uint32_t>(view.Players().size());
        dump.text = common::Format(
            "snapshot {} tick {} v{} state {}: {} entities, {} players, weather {}",
            view.Metadata().id.value, view.Metadata().simulationTick, view.Metadata().version,
            SnapshotStateName(view.Metadata().state), dump.entityCount, dump.playerCount,
            view.Environment().weatherId);
        return dump;
    }

    std::string SnapshotDiagnostics::DescribeState() const
    {
        const SnapshotStatistics stats = m_manager.Statistics();
        const QueueReport queue = QueueStatus();
        return common::Format(
            "SnapshotManager: built {} published {} retired {} | queue depth {} published {} "
            "current {} (tick {}) | pool {}/{} in use | last build {} ticks | {} entities {} players",
            stats.built, stats.published, stats.retired, queue.depth, queue.published,
            queue.currentId.value, queue.currentTick, stats.poolInUse, stats.poolCapacity,
            stats.lastBuildDurationTicks, stats.entityCount, stats.playerCount);
    }

    SnapshotConsistency SnapshotDiagnostics::ValidateConsistency() const
    {
        return m_manager.ValidateConsistency();
    }
} // namespace stalkermp::snapshot

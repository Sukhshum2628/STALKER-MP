// STALKER-MP — Recording restore sinks (Sprint-012, Step 10)
//
// The engine-free, OS-free in-memory counterpart of the restore-sink seams. It
// implements all six IRestoreSinks interfaces, RECORDING every applied record into
// public vectors so restorers/tests can observe what would have been written, and
// returning a configurable outcome (Ok by default; a forced value outcome lets
// failure paths be exercised). It performs NO engine access and NO authoritative
// mutation — it is the test/default double for the real EngineAdapters sinks (Step
// 17).
//
// This is a test/default double introduced with the seams — no restorers,
// orchestration, recovery, or wiring.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <vector>

#include "stalkermp/saveload/IRestoreSinks.h"

namespace stalkermp::saveload
{
    class RecordingRestoreSinks final : public IWorldRestoreSink,
                                        public IEnvironmentRestoreSink,
                                        public IEntityRestoreSink,
                                        public IPlayerRestoreSink,
                                        public IAlifeRestoreSink,
                                        public ISchedulerRestoreSink
    {
    public:
        [[nodiscard]] SaveLoadOutcome Apply(const WorldRestoreRecord& record) override
        {
            worlds.push_back(record);
            return m_outcome;
        }
        [[nodiscard]] SaveLoadOutcome Apply(const EnvironmentRestoreRecord& record) override
        {
            environments.push_back(record);
            return m_outcome;
        }
        [[nodiscard]] SaveLoadOutcome Apply(const EntityRestoreRecord& record) override
        {
            entities.push_back(record);
            return m_outcome;
        }
        [[nodiscard]] SaveLoadOutcome Apply(const PlayerRestoreRecord& record) override
        {
            players.push_back(record);
            return m_outcome;
        }
        [[nodiscard]] SaveLoadOutcome Apply(const AlifeRestoreRecord& record) override
        {
            alife.push_back(record);
            return m_outcome;
        }
        [[nodiscard]] SaveLoadOutcome Apply(const SchedulerRestoreRecord& record) override
        {
            schedulers.push_back(record);
            return m_outcome;
        }

        // Force the outcome returned by every Apply (default Ok) to exercise failure
        // paths in later restorer tests.
        void SetOutcome(SaveLoadOutcome outcome) noexcept { m_outcome = outcome; }

        void Clear() noexcept
        {
            worlds.clear();
            environments.clear();
            entities.clear();
            players.clear();
            alife.clear();
            schedulers.clear();
        }

        // Recorded applications (observation only).
        std::vector<WorldRestoreRecord> worlds;
        std::vector<EnvironmentRestoreRecord> environments;
        std::vector<EntityRestoreRecord> entities;
        std::vector<PlayerRestoreRecord> players;
        std::vector<AlifeRestoreRecord> alife;
        std::vector<SchedulerRestoreRecord> schedulers;

    private:
        SaveLoadOutcome m_outcome = SaveLoadOutcome::Ok;
    };
} // namespace stalkermp::saveload

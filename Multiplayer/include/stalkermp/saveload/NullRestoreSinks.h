// STALKER-MP — Null restore sinks + bundle (Sprint-012, Step 17)
//
// The engine-free, inert implementation of the six restore-sink seams and the
// IRestoreSinkBundle. Every Apply accepts its record and writes nothing: it is the
// test/default write boundary (no engine). Recovery therefore runs its full
// Load → Validate → Migrate → Restore pipeline deterministically without any engine
// dependency; the real authoritative writes live only in EngineAdapters.cpp.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include "stalkermp/saveload/IRestoreSinkBundle.h"
#include "stalkermp/saveload/IRestoreSinks.h"

namespace stalkermp::saveload
{
    // Implements all six seams; every Apply is an accepting no-op (returns Ok).
    class NullRestoreSinks final : public IWorldRestoreSink,
                                   public IEnvironmentRestoreSink,
                                   public IEntityRestoreSink,
                                   public IPlayerRestoreSink,
                                   public IAlifeRestoreSink,
                                   public ISchedulerRestoreSink
    {
    public:
        [[nodiscard]] SaveLoadOutcome Apply(const WorldRestoreRecord&) override { return SaveLoadOutcome::Ok; }
        [[nodiscard]] SaveLoadOutcome Apply(const EnvironmentRestoreRecord&) override { return SaveLoadOutcome::Ok; }
        [[nodiscard]] SaveLoadOutcome Apply(const EntityRestoreRecord&) override { return SaveLoadOutcome::Ok; }
        [[nodiscard]] SaveLoadOutcome Apply(const PlayerRestoreRecord&) override { return SaveLoadOutcome::Ok; }
        [[nodiscard]] SaveLoadOutcome Apply(const AlifeRestoreRecord&) override { return SaveLoadOutcome::Ok; }
        [[nodiscard]] SaveLoadOutcome Apply(const SchedulerRestoreRecord&) override { return SaveLoadOutcome::Ok; }
    };

    // A bundle whose six seams are all the same inert NullRestoreSinks instance.
    class NullRestoreSinkBundle final : public IRestoreSinkBundle
    {
    public:
        [[nodiscard]] RestoreSinkSet Sinks() noexcept override
        {
            return RestoreSinkSet{m_sinks, m_sinks, m_sinks, m_sinks, m_sinks, m_sinks};
        }

    private:
        NullRestoreSinks m_sinks;
    };
} // namespace stalkermp::saveload

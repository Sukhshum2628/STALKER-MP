// STALKER-MP — Restoration write-boundary seams (Sprint-012, Step 10)
//
// The engine-free interfaces through which restoration applies authoritative state.
// This is the ADR-008 ENGINE WRITE BOUNDARY for save/load: the engine-free core
// parses a save into value-only restoration records (Step 1) and applies them ONLY
// through these seams. The REAL implementations live in EngineAdapters.cpp (Step 17)
// and write authoritative state through the sanctioned Sprint-003 Entity Registry,
// Sprint-005 ALife gateway, and Sprint-007 player seams — the engine-free core never
// includes an engine header and never mutates authoritative state directly.
//
// Each sink applies ONE value record and returns a SaveLoadOutcome (value outcome,
// ADR-007). Records cross the seam by value; no engine type is exposed. Restoration
// is host-side at startup (Host Authority; One World; One ALife Simulation).
//
// This step introduces the SEAMS ONLY — no engine implementation (Step 17), no
// restorers or orchestration (Steps 11-14), no recovery, diagnostics, or wiring.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include "stalkermp/saveload/SaveLoadTypes.h" // restoration records + SaveLoadOutcome

namespace stalkermp::saveload
{
    // Applies the World / global-state record.
    class IWorldRestoreSink
    {
    public:
        virtual ~IWorldRestoreSink() = default;
        [[nodiscard]] virtual SaveLoadOutcome Apply(const WorldRestoreRecord& record) = 0;
    };

    // Applies the Environment record.
    class IEnvironmentRestoreSink
    {
    public:
        virtual ~IEnvironmentRestoreSink() = default;
        [[nodiscard]] virtual SaveLoadOutcome Apply(const EnvironmentRestoreRecord& record) = 0;
    };

    // Applies one entity (via the Sprint-003 Entity Registry seam in the engine build).
    class IEntityRestoreSink
    {
    public:
        virtual ~IEntityRestoreSink() = default;
        [[nodiscard]] virtual SaveLoadOutcome Apply(const EntityRestoreRecord& record) = 0;
    };

    // Applies one player (offline; via the Sprint-007 player seam in the engine build).
    class IPlayerRestoreSink
    {
    public:
        virtual ~IPlayerRestoreSink() = default;
        [[nodiscard]] virtual SaveLoadOutcome Apply(const PlayerRestoreRecord& record) = 0;
    };

    // Applies one ALife record (via the Sprint-005 sanctioned gateway in the engine
    // build). Never mutates ALife directly from the core.
    class IAlifeRestoreSink
    {
    public:
        virtual ~IAlifeRestoreSink() = default;
        [[nodiscard]] virtual SaveLoadOutcome Apply(const AlifeRestoreRecord& record) = 0;
    };

    // Applies the Scheduler record.
    class ISchedulerRestoreSink
    {
    public:
        virtual ~ISchedulerRestoreSink() = default;
        [[nodiscard]] virtual SaveLoadOutcome Apply(const SchedulerRestoreRecord& record) = 0;
    };
} // namespace stalkermp::saveload

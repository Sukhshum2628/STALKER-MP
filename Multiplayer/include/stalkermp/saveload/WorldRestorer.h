// STALKER-MP — World + Environment restorer (Sprint-012, Step 11)
//
// Engine-free orchestration that applies the parsed World and Environment records
// (Step 5) to the authoritative simulation THROUGH the Step-10 restore-sink seams,
// in the fixed deterministic order World → Environment. It writes nothing directly:
// the sinks (fake/recording in tests; the real EngineAdapters implementations in
// Step 17) perform the sanctioned authoritative writes (ADR-008). The first sink
// failure short-circuits and is returned as a value outcome.
//
// This step introduces the World/Environment restorer ONLY — no entity/player
// restore (Step 12), ALife/scheduler restore (Step 13), recovery orchestration
// (Step 14), diagnostics, or wiring.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (SaveLoadOutcome).

#pragma once

#include "stalkermp/saveload/IRestoreSinks.h" // IWorldRestoreSink / IEnvironmentRestoreSink
#include "stalkermp/saveload/SaveLoadTypes.h" // SaveLoadOutcome
#include "stalkermp/saveload/SaveReader.h"    // LoadedSave

namespace stalkermp::saveload
{
    class WorldRestorer
    {
    public:
        // Apply save.world then save.environment through the sinks (fixed order).
        // Returns the first failure, or Ok. Deterministic; reads the save read-only.
        [[nodiscard]] static SaveLoadOutcome Restore(const LoadedSave& save, IWorldRestoreSink& worldSink,
                                                     IEnvironmentRestoreSink& environmentSink);
    };
} // namespace stalkermp::saveload

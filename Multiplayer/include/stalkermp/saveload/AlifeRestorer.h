// STALKER-MP — ALife restorer (Sprint-012, Step 13)
//
// Engine-free orchestration that applies the parsed ALife records (Step 5) THROUGH
// the Step-10 IAlifeRestoreSink seam, in the save's deterministic order. It writes
// nothing directly and NEVER mutates ALife from the core: the sink (fake/recording
// in tests; the real EngineAdapters implementation over the Sprint-005 sanctioned
// ALife gateway in Step 17) performs the authoritative writes (ADR-008; One ALife
// Simulation). ALife behavior resumes after restoration completes. The first sink
// failure short-circuits and is returned as a value outcome.
//
// This step introduces the ALife (and Scheduler) restorers ONLY — no restore
// orchestration, recovery pipeline, engine adapters, diagnostics, or wiring.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (SaveLoadOutcome).

#pragma once

#include "stalkermp/saveload/IRestoreSinks.h" // IAlifeRestoreSink
#include "stalkermp/saveload/SaveLoadTypes.h" // SaveLoadOutcome
#include "stalkermp/saveload/SaveReader.h"    // LoadedSave

namespace stalkermp::saveload
{
    class AlifeRestorer
    {
    public:
        // Apply every ALife record in the save's order through the sink. Returns the
        // first failure, or Ok. Deterministic; reads the save read-only.
        [[nodiscard]] static SaveLoadOutcome Restore(const LoadedSave& save, IAlifeRestoreSink& sink);
    };
} // namespace stalkermp::saveload

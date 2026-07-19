// STALKER-MP — Scheduler restorer (Sprint-012, Step 13)
//
// Engine-free orchestration that applies the parsed Scheduler record (Step 5)
// THROUGH the Step-10 ISchedulerRestoreSink seam so simulation resumes
// deterministically. It writes nothing directly: the sink (fake/recording in tests;
// the real EngineAdapters implementation in Step 17) performs the sanctioned
// authoritative write (ADR-008). Returns the sink's value outcome.
//
// This step introduces the Scheduler (and ALife) restorers ONLY — no restore
// orchestration, recovery pipeline, engine adapters, diagnostics, or wiring.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (SaveLoadOutcome).

#pragma once

#include "stalkermp/saveload/IRestoreSinks.h" // ISchedulerRestoreSink
#include "stalkermp/saveload/SaveLoadTypes.h" // SaveLoadOutcome
#include "stalkermp/saveload/SaveReader.h"    // LoadedSave

namespace stalkermp::saveload
{
    class SchedulerRestorer
    {
    public:
        // Apply the save's single scheduler record through the sink. Returns the sink
        // outcome. Deterministic; reads the save read-only.
        [[nodiscard]] static SaveLoadOutcome Restore(const LoadedSave& save, ISchedulerRestoreSink& sink);
    };
} // namespace stalkermp::saveload

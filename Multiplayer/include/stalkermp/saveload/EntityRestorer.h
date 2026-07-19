// STALKER-MP — Entity restorer (Sprint-012, Step 12)
//
// Engine-free orchestration that applies the parsed entity records (Step 5) to the
// Entity Registry THROUGH the Step-10 IEntityRestoreSink seam, in the deterministic
// ascending-EntityId order the save carries. It writes nothing directly — the sink
// (fake/recording in tests; the real EngineAdapters implementation over the
// Sprint-003 Entity Registry in Step 17) performs the sanctioned authoritative
// writes (ADR-008). The first sink failure short-circuits.
//
// This step introduces the entity restorer ONLY — no ALife/scheduler restore, no
// recovery orchestration, diagnostics, or wiring.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (SaveLoadOutcome).

#pragma once

#include "stalkermp/saveload/IRestoreSinks.h" // IEntityRestoreSink
#include "stalkermp/saveload/SaveLoadTypes.h" // SaveLoadOutcome
#include "stalkermp/saveload/SaveReader.h"    // LoadedSave

namespace stalkermp::saveload
{
    class EntityRestorer
    {
    public:
        // Apply every entity record in the save's (ascending) order through the sink.
        // Returns the first failure, or Ok. Deterministic; reads the save read-only.
        [[nodiscard]] static SaveLoadOutcome Restore(const LoadedSave& save, IEntityRestoreSink& sink);
    };
} // namespace stalkermp::saveload

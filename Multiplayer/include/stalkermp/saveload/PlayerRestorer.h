// STALKER-MP — Player restorer (Sprint-012, Step 12)
//
// Engine-free orchestration that applies the parsed player records (Step 5) THROUGH
// the Step-10 IPlayerRestoreSink seam, in the save's deterministic order. Players are
// restored OFFLINE — they reconnect later; the restorer never opens connections. It
// writes nothing directly: the sink (fake/recording in tests; the real EngineAdapters
// implementation over the Sprint-007 player seam in Step 17) performs the sanctioned
// authoritative writes (ADR-008). The first sink failure short-circuits.
//
// This step introduces the player restorer ONLY — no ALife/scheduler restore, no
// recovery orchestration, diagnostics, or wiring.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (SaveLoadOutcome).

#pragma once

#include "stalkermp/saveload/IRestoreSinks.h" // IPlayerRestoreSink
#include "stalkermp/saveload/SaveLoadTypes.h" // SaveLoadOutcome
#include "stalkermp/saveload/SaveReader.h"    // LoadedSave

namespace stalkermp::saveload
{
    class PlayerRestorer
    {
    public:
        // Apply every player record (offline) in the save's order through the sink.
        // Returns the first failure, or Ok. Deterministic; reads the save read-only.
        [[nodiscard]] static SaveLoadOutcome Restore(const LoadedSave& save, IPlayerRestoreSink& sink);
    };
} // namespace stalkermp::saveload

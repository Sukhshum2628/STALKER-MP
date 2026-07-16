// STALKER-MP — Null entity snapshot source (test build only, Sprint-008, Step 5)
//
// Test-build counterpart of the engine adapter (EngineEntitySnapshotSource,
// Step 9): provides the CreateEngineEntitySnapshotSource factory WITHOUT any
// engine or OS dependency (One Engine Boundary). The null source is an inert,
// deterministic engine-free stand-in — it captures no engine entities and, being
// append-only, leaves `out` unchanged. This lets the engine-free test build run
// the whole snapshot pipeline without an engine; tests that need controlled entity
// data use a local fake source.

#include "stalkermp/adapters/EntitySnapshotSource.h"

#include <memory>
#include <vector>

#include "stalkermp/snapshot/SnapshotTypes.h"
#include "stalkermp/world/IEntitySnapshotSource.h"

namespace stalkermp::adapters
{
    namespace
    {
        class NullEntitySnapshotSource final : public world::IEntitySnapshotSource
        {
        public:
            // Inert + deterministic: no engine -> no entities captured. Append-only,
            // so `out` is left exactly as received.
            void Capture(std::vector<snapshot::EntitySnapshot>& /*out*/) const override
            {
                // intentionally empty (no engine entities in the test build)
            }
        };
    } // namespace

    std::unique_ptr<world::IEntitySnapshotSource> CreateEngineEntitySnapshotSource()
    {
        return std::make_unique<NullEntitySnapshotSource>();
    }
} // namespace stalkermp::adapters

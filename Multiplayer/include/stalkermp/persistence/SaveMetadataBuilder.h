// STALKER-MP — Save metadata builder (Sprint-011, Step 4)
//
// Builds a SaveMetadata deterministically from an immutable snapshot view. It
// reads the captured tick, the player/entity counts, and the world/build versions,
// and computes a deterministic 64-bit checksum over the snapshot's CONTENT values.
//
// Determinism (E-G2-PF): identical snapshot content always yields the identical
// checksum. Diagnostic-only fields — the snapshot's wall-clock timestamp and build
// duration — are EXCLUDED from the checksum; the wall-clock is passed through into
// SaveMetadata.timestampWallClock as a diagnostic value only and never influences a
// decision or the checksum. The builder reads no clock and performs no I/O.
//
// `SaveTrigger` is accepted for call-site provenance (it travels with the
// SaveRequest); it is intentionally NOT folded into the metadata or checksum, so a
// manual save and an autosave of the same snapshot produce identical checksums.
//
// This step introduces the builder ONLY — no snapshot projection, validation,
// queue, worker, scheduler, or manager. Read-only over the snapshot (Host
// Authority; ADR-008).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>

#include "stalkermp/persistence/PersistenceTypes.h" // SaveMetadata, SaveTrigger
#include "stalkermp/snapshot/ISnapshotView.h"        // snapshot::ISnapshotView

namespace stalkermp::persistence
{
    class SaveMetadataBuilder
    {
    public:
        // Build metadata for `saveId` from the immutable `view`. Reads tick, counts,
        // and world/build versions; computes the content checksum; passes through the
        // snapshot's diagnostic wall-clock. `trigger` is provenance only (unused in
        // the output). Deterministic; read-only; never throws.
        [[nodiscard]] static SaveMetadata Build(const snapshot::ISnapshotView& view, SaveTrigger trigger,
                                                std::uint64_t saveId) noexcept;

        // The deterministic 64-bit content checksum over the snapshot (the same digest
        // Build stamps into SaveMetadata.checksum). Excludes diagnostic wall-clock and
        // build-duration. Exposed for validation reuse and direct testing.
        [[nodiscard]] static std::uint64_t Checksum(const snapshot::ISnapshotView& view) noexcept;
    };
} // namespace stalkermp::persistence

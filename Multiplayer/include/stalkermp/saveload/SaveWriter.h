// STALKER-MP — Save writer (Sprint-012, Step 4)
//
// Deterministically serializes an immutable authoritative snapshot into the save
// byte artifact, in the fixed section order World → Environment → Registry →
// Players → ALife → Scheduler → Story → Metadata, framed by the Step-3 SaveFormat
// primitives and closed with a content checksum trailer. Reads the snapshot
// READ-ONLY (Host Authority; ADR-008) and performs NO I/O — it produces bytes only;
// writing them to storage is the platform backend's job (Step 9).
//
// Determinism (E-G1-SL): identical snapshot + metadata content yield byte-identical
// output. The diagnostic wall-clock is excluded from the serialized identity.
//
// This step introduces the WRITER only — no reader (Step 5), integrity validation
// (Step 6), migration (Step 7), filesystem, or restoration.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>
#include <vector>

#include "stalkermp/persistence/PersistenceTypes.h" // persistence::SaveMetadata (reused, unchanged)
#include "stalkermp/snapshot/ISnapshotView.h"        // snapshot::ISnapshotView

namespace stalkermp::saveload
{
    class SaveWriter
    {
    public:
        // Serialize `view` (with the caller-built `metadata`) into a save byte
        // artifact. Deterministic; read-only; never throws.
        [[nodiscard]] static std::vector<std::uint8_t> Serialize(const snapshot::ISnapshotView& view,
                                                                 const persistence::SaveMetadata& metadata);
    };
} // namespace stalkermp::saveload

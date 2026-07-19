// STALKER-MP — Save reader (Sprint-012, Step 5)
//
// Deterministically parses a save byte artifact (produced by the Step-4 SaveWriter)
// back into the engine-free restoration record set + SaveMetadata, validating the
// file magic, format version, section framing, and content checksum. It performs NO
// I/O and NO engine access, and round-trips the writer exactly. Malformed input is a
// value outcome (CorruptedSave / VersionMismatch / ChecksumFailure / MissingData)
// that leaves the parsed value in its default (nothing applied).
//
// Step-5 validates only STRUCTURAL correctness (magic / version / framing /
// checksum). Deeper integrity (duplicate ids, missing references, count/version
// policy) is Step 6; migration is Step 7 — neither is performed here.
//
// This step introduces the READER only — no restoration, filesystem, or wiring.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (SaveLoadOutcome).

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stalkermp/persistence/PersistenceTypes.h" // persistence::SaveMetadata (reused, unchanged)
#include "stalkermp/saveload/SaveLoadTypes.h"        // SaveLoadOutcome + restoration records

namespace stalkermp::saveload
{
    // The engine-free result of parsing a save (record set + metadata). Values only.
    struct LoadedSave
    {
        persistence::SaveMetadata metadata{};
        WorldRestoreRecord world{};
        EnvironmentRestoreRecord environment{};
        std::vector<EntityRestoreRecord> entities;
        std::vector<PlayerRestoreRecord> players;
        std::vector<AlifeRestoreRecord> alife;
        SchedulerRestoreRecord scheduler{};
    };

    // A parse result: `outcome` == Ok means `save` is valid; otherwise `save` is
    // default/partial and `outcome` names the structural failure.
    struct ParseResult
    {
        SaveLoadOutcome outcome = SaveLoadOutcome::Ok;
        LoadedSave save{};
    };

    class SaveReader
    {
    public:
        [[nodiscard]] static ParseResult Parse(const std::uint8_t* data, std::size_t size);
        [[nodiscard]] static ParseResult Parse(const std::vector<std::uint8_t>& bytes)
        {
            return Parse(bytes.data(), bytes.size());
        }
    };
} // namespace stalkermp::saveload

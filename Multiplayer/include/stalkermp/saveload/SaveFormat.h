// STALKER-MP — Save file format vocabulary & framing (Sprint-012, Step 3)
//
// The deterministic, engine-free on-disk format vocabulary: the file magic and
// format version, the section identifiers, the section framing helpers (id +
// length + payload), the header read/write helpers, and a deterministic content
// checksum. This is serialization INFRASTRUCTURE only — it defines HOW bytes are
// framed, not WHAT the World/Environment/Entities/etc. contain (that is the
// SaveWriter, Step 4) nor how they are parsed into records (SaveReader, Step 5).
//
// Independent of the wire protocol (ADR-010 untouched). No filesystem, loading,
// restoration, or migration.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (SaveLoadOutcome).

#pragma once

#include <cstddef>
#include <cstdint>

#include "stalkermp/saveload/SaveByteCursor.h" // ByteWriter / ByteReader
#include "stalkermp/saveload/SaveLoadTypes.h"  // SaveLoadOutcome

namespace stalkermp::saveload
{
    // File magic ("SMPS" = STALKER-MP Save) and the on-disk format version. The
    // format version is distinct from the [saveload] policy version (Step 2).
    inline constexpr std::uint32_t kSaveMagic = 0x53504D53u; // 'S''M''P''S' little-endian
    inline constexpr std::uint32_t kSaveFormatVersion = 1u;

    // Section identifiers (fixed std::uint16_t layout; reserved 0 = Header;
    // appended, never renumbered). Ordering matches the deterministic serialization
    // order (World → Environment → Registry → Players → ALife → Scheduler → Story →
    // Metadata), framed and consumed by later steps.
    enum class SaveSection : std::uint16_t
    {
        Header = 0,
        World,
        Environment,
        Registry,
        Players,
        ALife,
        Scheduler,
        Story,
        Metadata,
        Trailer,
    };

    [[nodiscard]] constexpr const char* SaveSectionName(const SaveSection s) noexcept
    {
        switch (s)
        {
        case SaveSection::Header:      return "Header";
        case SaveSection::World:       return "World";
        case SaveSection::Environment: return "Environment";
        case SaveSection::Registry:    return "Registry";
        case SaveSection::Players:     return "Players";
        case SaveSection::ALife:       return "ALife";
        case SaveSection::Scheduler:   return "Scheduler";
        case SaveSection::Story:       return "Story";
        case SaveSection::Metadata:    return "Metadata";
        case SaveSection::Trailer:     return "Trailer";
        }
        return "Unknown";
    }

    // Write the file header (magic + format version).
    inline void WriteHeader(ByteWriter& writer)
    {
        writer.PutU32(kSaveMagic);
        writer.PutU32(kSaveFormatVersion);
    }

    // Read + validate the file header. Wrong magic => CorruptedSave; wrong format
    // version => VersionMismatch; a truncated header => CorruptedSave.
    [[nodiscard]] inline SaveLoadOutcome ReadHeader(ByteReader& reader) noexcept
    {
        std::uint32_t magic = 0;
        std::uint32_t formatVersion = 0;
        if (!reader.GetU32(magic) || !reader.GetU32(formatVersion))
        {
            return SaveLoadOutcome::CorruptedSave;
        }
        if (magic != kSaveMagic)
        {
            return SaveLoadOutcome::CorruptedSave;
        }
        if (formatVersion != kSaveFormatVersion)
        {
            return SaveLoadOutcome::VersionMismatch;
        }
        return SaveLoadOutcome::Ok;
    }

    // Frame a section: id (u16) + payload length (u32) + payload bytes.
    inline void WriteSection(ByteWriter& writer, const SaveSection id, const std::uint8_t* payload,
                             const std::size_t length)
    {
        writer.PutU16(static_cast<std::uint16_t>(id));
        writer.PutU32(static_cast<std::uint32_t>(length));
        writer.PutBytes(payload, length);
    }

    // Read a section header (id + payload length). The reader is positioned at the
    // payload on success. Returns false on truncation (reader latches its error).
    [[nodiscard]] inline bool ReadSectionHeader(ByteReader& reader, SaveSection& outId,
                                                std::uint32_t& outLength) noexcept
    {
        std::uint16_t rawId = 0;
        std::uint32_t length = 0;
        if (!reader.GetU16(rawId) || !reader.GetU32(length))
        {
            return false;
        }
        outId = static_cast<SaveSection>(rawId);
        outLength = length;
        return true;
    }

    // Deterministic 64-bit content checksum (FNV-1a) over a byte range. Used to
    // detect corruption; identical bytes always yield the identical checksum.
    [[nodiscard]] inline std::uint64_t Checksum(const std::uint8_t* data, const std::size_t length) noexcept
    {
        constexpr std::uint64_t kOffset = 1469598103934665603ull;
        constexpr std::uint64_t kPrime = 1099511628211ull;
        std::uint64_t h = kOffset;
        for (std::size_t i = 0; i < length; ++i)
        {
            h ^= static_cast<std::uint64_t>(data[i]);
            h *= kPrime;
        }
        return h;
    }
} // namespace stalkermp::saveload

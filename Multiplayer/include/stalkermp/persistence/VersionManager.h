// STALKER-MP — Persistence version manager (Sprint-011, Step 3)
//
// Tracks and compares the four persistence-related versions and reports whether a
// candidate version set is Equal, needs migration, or is Incompatible with the
// current build. Pure and deterministic — no wall-clock, no allocation, no I/O.
// Migration EXECUTION is out of scope (Sprint-012); this step only classifies the
// requirement.
//
// Compatibility model: `compatibility` is the hard gate — a differing
// compatibility version is Incompatible (cannot be migrated within this scheme).
// When the compatibility versions match but any of persistence/world/schema
// differ, migration is required. Otherwise the sets are Equal.
//
// This step introduces the version value + comparison ONLY — no metadata builder,
// snapshot projection, validation, queue, worker, scheduler, or manager.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>

namespace stalkermp::persistence
{
    // The four versions that describe a save's compatibility surface. All default to
    // 1 (the initial version). `0` is reserved as "unset".
    struct VersionSet
    {
        std::uint32_t persistence = 1;   // persistence framework format version
        std::uint32_t world = 1;         // world/content version
        std::uint32_t schema = 1;        // save schema version
        std::uint32_t compatibility = 1; // hard compatibility boundary

        [[nodiscard]] constexpr bool operator==(const VersionSet& o) const noexcept
        {
            return persistence == o.persistence && world == o.world && schema == o.schema &&
                   compatibility == o.compatibility;
        }
        [[nodiscard]] constexpr bool operator!=(const VersionSet& o) const noexcept { return !(*this == o); }
    };

    // The result of comparing a candidate version set against the current build.
    enum class VersionCompatibility : std::uint8_t
    {
        Equal = 0,         // identical version sets
        MigrationRequired, // compatible boundary, but one or more versions differ
        Incompatible,      // compatibility boundary differs — cannot be reconciled here
    };

    [[nodiscard]] constexpr const char* VersionCompatibilityName(const VersionCompatibility c) noexcept
    {
        switch (c)
        {
        case VersionCompatibility::Equal:             return "Equal";
        case VersionCompatibility::MigrationRequired: return "MigrationRequired";
        case VersionCompatibility::Incompatible:      return "Incompatible";
        }
        return "Unknown";
    }

    // Holds the current build's version set and classifies candidates against it.
    // Pure and deterministic; identical inputs always yield the identical result.
    class VersionManager
    {
    public:
        constexpr VersionManager() noexcept = default;
        constexpr explicit VersionManager(const VersionSet& current) noexcept : m_current(current) {}

        // The current build's version set.
        [[nodiscard]] constexpr const VersionSet& Current() const noexcept { return m_current; }

        // Classify `candidate` against the current build.
        [[nodiscard]] constexpr VersionCompatibility Compare(const VersionSet& candidate) const noexcept
        {
            if (candidate.compatibility != m_current.compatibility)
            {
                return VersionCompatibility::Incompatible;
            }
            if (candidate != m_current)
            {
                return VersionCompatibility::MigrationRequired;
            }
            return VersionCompatibility::Equal;
        }

        // Whether `candidate` can be reconciled with the current build (Equal or
        // MigrationRequired). Incompatible candidates return false.
        [[nodiscard]] constexpr bool IsCompatible(const VersionSet& candidate) const noexcept
        {
            return Compare(candidate) != VersionCompatibility::Incompatible;
        }

        // Whether reconciling `candidate` would require migration.
        [[nodiscard]] constexpr bool MigrationRequired(const VersionSet& candidate) const noexcept
        {
            return Compare(candidate) == VersionCompatibility::MigrationRequired;
        }

    private:
        VersionSet m_current{};
    };
} // namespace stalkermp::persistence

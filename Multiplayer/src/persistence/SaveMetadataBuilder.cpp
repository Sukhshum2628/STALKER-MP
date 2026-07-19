// STALKER-MP — Save metadata builder (Sprint-011, Step 4)
//
// See SaveMetadataBuilder.h. Deterministic content checksum (FNV-1a-style 64-bit
// fold) over the snapshot's value fields, in the snapshot's fixed ascending order
// (EntityId / PlayerId). Engine-free / OS-free; no exceptions, no RTTI, no
// iostream.

#include "stalkermp/persistence/SaveMetadataBuilder.h"

#include <cstddef>
#include <cstring> // std::memcpy (deterministic float bit access)

#include "stalkermp/snapshot/SnapshotTypes.h" // EntitySnapshot, PlayerSnapshot, EnvironmentSnapshot

namespace stalkermp::persistence
{
    namespace
    {
        constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
        constexpr std::uint64_t kFnvPrime = 1099511628211ull;

        // Fold one 64-bit value (byte-wise, little-endian order) into the digest.
        void Mix(std::uint64_t& h, std::uint64_t value) noexcept
        {
            for (int i = 0; i < 8; ++i)
            {
                h ^= (value & 0xFFull);
                h *= kFnvPrime;
                value >>= 8;
            }
        }

        // Deterministic IEEE-754 bit pattern of a float (no aliasing UB).
        [[nodiscard]] std::uint64_t FloatBits(const float f) noexcept
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &f, sizeof(bits));
            return static_cast<std::uint64_t>(bits);
        }

        void MixVec3(std::uint64_t& h, const world::Vec3& v) noexcept
        {
            Mix(h, FloatBits(v.x));
            Mix(h, FloatBits(v.y));
            Mix(h, FloatBits(v.z));
        }
    } // namespace

    std::uint64_t SaveMetadataBuilder::Checksum(const snapshot::ISnapshotView& view) noexcept
    {
        const snapshot::SnapshotMetadata& meta = view.Metadata();

        std::uint64_t h = kFnvOffset;

        // Header content (diagnostic wall-clock and build duration deliberately
        // excluded — they must not affect replay identity).
        Mix(h, meta.simulationTick);
        Mix(h, static_cast<std::uint64_t>(view.Entities().size()));
        Mix(h, static_cast<std::uint64_t>(view.Players().size()));
        Mix(h, static_cast<std::uint64_t>(view.Environment().environmentVersion)); // world version
        Mix(h, static_cast<std::uint64_t>(meta.version));                          // build/format version

        // Entities (already ascending by EntityId — fixed deterministic order).
        for (const snapshot::EntitySnapshot& e : view.Entities())
        {
            Mix(h, static_cast<std::uint64_t>(e.id.value));
            MixVec3(h, e.position);
            MixVec3(h, e.velocity);
            Mix(h, static_cast<std::uint64_t>(e.state));
            Mix(h, static_cast<std::uint64_t>(e.flags));
            Mix(h, static_cast<std::uint64_t>(e.inventoryRef.value));
            Mix(h, e.runtimeState);
        }

        // Players (already ascending by PlayerId — fixed deterministic order).
        for (const snapshot::PlayerSnapshot& p : view.Players())
        {
            Mix(h, static_cast<std::uint64_t>(p.id.value));
            Mix(h, static_cast<std::uint64_t>(p.entity.value));
            Mix(h, static_cast<std::uint64_t>(p.connectionState));
            MixVec3(h, p.position);
            Mix(h, static_cast<std::uint64_t>(p.simulationState));
            Mix(h, static_cast<std::uint64_t>(p.authorityFlags));
        }

        // Environment content (excluding its own tick echo which duplicates header).
        const snapshot::EnvironmentSnapshot& env = view.Environment();
        Mix(h, static_cast<std::uint64_t>(env.weatherId));
        Mix(h, static_cast<std::uint64_t>(env.timeOfDaySeconds));
        Mix(h, static_cast<std::uint64_t>(env.emissionState));
        Mix(h, static_cast<std::uint64_t>(env.lighting));
        Mix(h, static_cast<std::uint64_t>(env.environmentVersion));

        return h;
    }

    SaveMetadata SaveMetadataBuilder::Build(const snapshot::ISnapshotView& view, const SaveTrigger trigger,
                                            const std::uint64_t saveId) noexcept
    {
        (void)trigger; // provenance travels with the SaveRequest; not part of metadata

        const snapshot::SnapshotMetadata& meta = view.Metadata();

        SaveMetadata out{};
        out.saveId = saveId;
        out.simulationTick = meta.simulationTick;
        out.playerCount = static_cast<std::uint32_t>(view.Players().size());
        out.entityCount = static_cast<std::uint32_t>(view.Entities().size());
        out.worldVersion = view.Environment().environmentVersion;
        out.buildVersion = meta.version;
        out.checksum = Checksum(view);
        // Diagnostic passthrough ONLY — never used in a decision or the checksum.
        out.timestampWallClock = meta.timestampWallClock;
        return out;
    }
} // namespace stalkermp::persistence

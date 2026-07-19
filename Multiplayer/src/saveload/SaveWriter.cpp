// STALKER-MP — Save writer (Sprint-012, Step 4)
//
// See SaveWriter.h. Deterministic serialization into the Step-3 framing.
// Engine-free / OS-free; no exceptions, no RTTI, no iostream.

#include "stalkermp/saveload/SaveWriter.h"

#include "stalkermp/saveload/SaveByteCursor.h"
#include "stalkermp/saveload/SaveFormat.h"
#include "stalkermp/snapshot/SnapshotTypes.h" // EntitySnapshot / PlayerSnapshot / EnvironmentSnapshot

namespace stalkermp::saveload
{
    namespace
    {
        void PutVec3(ByteWriter& w, const world::Vec3& v)
        {
            w.PutF32(v.x);
            w.PutF32(v.y);
            w.PutF32(v.z);
        }

        // --- Per-section payload encoders (deterministic, fixed layout) ---------

        std::vector<std::uint8_t> EncodeWorld(const snapshot::ISnapshotView& view)
        {
            ByteWriter w;
            const snapshot::EnvironmentSnapshot& env = view.Environment();
            w.PutU64(view.Metadata().simulationTick);
            w.PutU64(static_cast<std::uint64_t>(env.timeOfDaySeconds)); // world time (derived)
            w.PutU32(env.weatherId);
            w.PutU32(0u); // globalStateFlags (no snapshot source)
            w.PutU32(0u); // zoneConfigId (no snapshot source)
            return w.Bytes();
        }

        std::vector<std::uint8_t> EncodeEnvironment(const snapshot::ISnapshotView& view)
        {
            ByteWriter w;
            const snapshot::EnvironmentSnapshot& env = view.Environment();
            w.PutU32(env.weatherId);
            w.PutU32(env.timeOfDaySeconds);
            w.PutU32(env.emissionState);
            w.PutU32(env.lighting);
            w.PutU32(env.environmentVersion);
            return w.Bytes();
        }

        std::vector<std::uint8_t> EncodeRegistry(const snapshot::ISnapshotView& view)
        {
            ByteWriter w;
            w.PutU32(static_cast<std::uint32_t>(view.Entities().size()));
            for (const snapshot::EntitySnapshot& e : view.Entities()) // ascending EntityId
            {
                w.PutU32(e.id.value);
                PutVec3(w, e.position);
                PutVec3(w, e.velocity);
                w.PutU32(e.state);          // -> stateFlags
                w.PutU32(e.flags);          // -> runtimeFlags
                w.PutU32(e.inventoryRef.value);
                w.PutU32(0u);               // owner (no snapshot source)
                w.PutU64(e.runtimeState);
                w.PutU32(0u);               // relationship (no snapshot source)
            }
            return w.Bytes();
        }

        std::vector<std::uint8_t> EncodePlayers(const snapshot::ISnapshotView& view)
        {
            ByteWriter w;
            w.PutU32(static_cast<std::uint32_t>(view.Players().size()));
            for (const snapshot::PlayerSnapshot& p : view.Players()) // ascending PlayerId
            {
                w.PutU32(p.id.value);
                w.PutU32(p.entity.value);
                PutVec3(w, p.position);
                w.PutU32(p.simulationState); // -> statistics
                w.PutU32(0u);                // questProgress (no snapshot source)
                w.PutU32(p.authorityFlags);  // -> factionId slot (opaque carry)
                w.PutU8(static_cast<std::uint8_t>(p.connectionState));
            }
            return w.Bytes();
        }

        std::vector<std::uint8_t> EncodeAlife()
        {
            ByteWriter w;
            w.PutU32(0u); // count 0 — ALife capture is not carried by the Sprint-008 snapshot
            return w.Bytes();
        }

        std::vector<std::uint8_t> EncodeScheduler(const snapshot::ISnapshotView& view)
        {
            ByteWriter w;
            w.PutU64(view.Metadata().simulationTick);
            w.PutU32(0u); // pendingUpdates
            w.PutU32(0u); // queueDepth
            w.PutU32(0u); // deferredTasks
            w.PutU64(0u); // executionOrder
            return w.Bytes();
        }

        std::vector<std::uint8_t> EncodeMetadata(const persistence::SaveMetadata& m)
        {
            ByteWriter w;
            w.PutU64(m.saveId);
            w.PutU64(m.simulationTick);
            w.PutU32(m.playerCount);
            w.PutU32(m.entityCount);
            w.PutU32(m.worldVersion);
            w.PutU32(m.buildVersion);
            w.PutU64(m.checksum);
            // timestampWallClock DELIBERATELY excluded (diagnostic; not replay identity).
            return w.Bytes();
        }

        void Frame(ByteWriter& out, SaveSection id, const std::vector<std::uint8_t>& payload)
        {
            WriteSection(out, id, payload.data(), payload.size());
        }
    } // namespace

    std::vector<std::uint8_t> SaveWriter::Serialize(const snapshot::ISnapshotView& view,
                                                    const persistence::SaveMetadata& metadata)
    {
        ByteWriter out;
        WriteHeader(out);

        Frame(out, SaveSection::World, EncodeWorld(view));
        Frame(out, SaveSection::Environment, EncodeEnvironment(view));
        Frame(out, SaveSection::Registry, EncodeRegistry(view));
        Frame(out, SaveSection::Players, EncodePlayers(view));
        Frame(out, SaveSection::ALife, EncodeAlife());
        Frame(out, SaveSection::Scheduler, EncodeScheduler(view));
        Frame(out, SaveSection::Story, std::vector<std::uint8_t>{}); // empty (no snapshot source)
        Frame(out, SaveSection::Metadata, EncodeMetadata(metadata));

        // Content checksum trailer over every preceding byte (header .. metadata).
        const std::uint64_t checksum = Checksum(out.Bytes().data(), out.Size());
        out.PutU64(checksum);

        return out.Bytes();
    }
} // namespace stalkermp::saveload

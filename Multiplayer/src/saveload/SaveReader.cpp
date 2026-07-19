// STALKER-MP — Save reader (Sprint-012, Step 5)
//
// See SaveReader.h. Deterministic parse + structural validation (magic / version /
// framing / checksum). Engine-free / OS-free; no exceptions, no RTTI, no iostream;
// value outcomes.

#include "stalkermp/saveload/SaveReader.h"

#include "stalkermp/saveload/SaveByteCursor.h"
#include "stalkermp/saveload/SaveFormat.h"

namespace stalkermp::saveload
{
    namespace
    {
        bool GetVec3(ByteReader& r, world::Vec3& v)
        {
            return r.GetF32(v.x) && r.GetF32(v.y) && r.GetF32(v.z);
        }

        // Read + verify one section header equals the expected id. Sets `length`.
        bool ExpectSection(ByteReader& r, SaveSection expected, std::uint32_t& length)
        {
            SaveSection id = SaveSection::Header;
            if (!ReadSectionHeader(r, id, length))
            {
                return false;
            }
            return id == expected;
        }

        bool DecodeWorld(ByteReader& r, WorldRestoreRecord& w)
        {
            return r.GetU64(w.simulationTick) && r.GetU64(w.worldTimeSeconds) && r.GetU32(w.weatherId) &&
                   r.GetU32(w.globalStateFlags) && r.GetU32(w.zoneConfigId);
        }

        bool DecodeEnvironment(ByteReader& r, EnvironmentRestoreRecord& e)
        {
            return r.GetU32(e.weatherId) && r.GetU32(e.timeOfDaySeconds) && r.GetU32(e.emissionState) &&
                   r.GetU32(e.lighting) && r.GetU32(e.environmentVersion);
        }

        bool DecodeEntity(ByteReader& r, EntityRestoreRecord& e)
        {
            std::uint32_t id = 0;
            std::uint32_t inv = 0;
            std::uint32_t owner = 0;
            if (!r.GetU32(id) || !GetVec3(r, e.position) || !GetVec3(r, e.velocity) || !r.GetU32(e.stateFlags) ||
                !r.GetU32(e.runtimeFlags) || !r.GetU32(inv) || !r.GetU32(owner) || !r.GetU64(e.runtimeState) ||
                !r.GetU32(e.relationship))
            {
                return false;
            }
            e.id = world::EntityId{id};
            e.inventoryRef = world::EntityId{inv};
            e.owner = world::EntityId{owner};
            return true;
        }

        bool DecodePlayer(ByteReader& r, PlayerRestoreRecord& p)
        {
            std::uint32_t id = 0;
            std::uint32_t entity = 0;
            std::uint8_t conn = 0;
            if (!r.GetU32(id) || !r.GetU32(entity) || !GetVec3(r, p.position) || !r.GetU32(p.statistics) ||
                !r.GetU32(p.questProgress) || !r.GetU32(p.factionId) || !r.GetU8(conn))
            {
                return false;
            }
            p.id = player::PlayerId{id};
            p.entity = world::EntityId{entity};
            p.connectionState = static_cast<player::PlayerConnectionState>(conn);
            return true;
        }

        bool DecodeAlife(ByteReader& r, AlifeRestoreRecord& a)
        {
            std::uint32_t offline = 0;
            if (!r.GetU32(a.smartTerrainId) || !r.GetU32(a.taskManagerState) || !r.GetU32(a.simulationState) ||
                !r.GetU32(offline) || !r.GetU32(a.npcSchedule) || !r.GetU32(a.factionPlannerState) ||
                !r.GetU32(a.storyLinkId))
            {
                return false;
            }
            a.offlineObject = world::EntityId{offline};
            return true;
        }

        bool DecodeScheduler(ByteReader& r, SchedulerRestoreRecord& s)
        {
            return r.GetU64(s.simulationTick) && r.GetU32(s.pendingUpdates) && r.GetU32(s.queueDepth) &&
                   r.GetU32(s.deferredTasks) && r.GetU64(s.executionOrder);
        }

        bool DecodeMetadata(ByteReader& r, persistence::SaveMetadata& m)
        {
            return r.GetU64(m.saveId) && r.GetU64(m.simulationTick) && r.GetU32(m.playerCount) &&
                   r.GetU32(m.entityCount) && r.GetU32(m.worldVersion) && r.GetU32(m.buildVersion) &&
                   r.GetU64(m.checksum);
            // timestampWallClock is not serialized; it defaults to 0 (diagnostic).
        }

        ParseResult Fail(SaveLoadOutcome outcome)
        {
            ParseResult result;
            result.outcome = outcome;
            return result;
        }
    } // namespace

    ParseResult SaveReader::Parse(const std::uint8_t* data, const std::size_t size)
    {
        ByteReader r{data, size};

        if (const SaveLoadOutcome header = ReadHeader(r); header != SaveLoadOutcome::Ok)
        {
            return Fail(header); // CorruptedSave (magic/truncation) or VersionMismatch
        }

        LoadedSave save;
        std::uint32_t length = 0;

        if (!ExpectSection(r, SaveSection::World, length) || !DecodeWorld(r, save.world))
        {
            return Fail(SaveLoadOutcome::CorruptedSave);
        }
        if (!ExpectSection(r, SaveSection::Environment, length) || !DecodeEnvironment(r, save.environment))
        {
            return Fail(SaveLoadOutcome::CorruptedSave);
        }

        if (!ExpectSection(r, SaveSection::Registry, length))
        {
            return Fail(SaveLoadOutcome::CorruptedSave);
        }
        std::uint32_t entityCount = 0;
        if (!r.GetU32(entityCount))
        {
            return Fail(SaveLoadOutcome::CorruptedSave);
        }
        save.entities.reserve(entityCount);
        for (std::uint32_t i = 0; i < entityCount; ++i)
        {
            EntityRestoreRecord e{};
            if (!DecodeEntity(r, e))
            {
                return Fail(SaveLoadOutcome::CorruptedSave);
            }
            save.entities.push_back(e);
        }

        if (!ExpectSection(r, SaveSection::Players, length))
        {
            return Fail(SaveLoadOutcome::CorruptedSave);
        }
        std::uint32_t playerCount = 0;
        if (!r.GetU32(playerCount))
        {
            return Fail(SaveLoadOutcome::CorruptedSave);
        }
        save.players.reserve(playerCount);
        for (std::uint32_t i = 0; i < playerCount; ++i)
        {
            PlayerRestoreRecord p{};
            if (!DecodePlayer(r, p))
            {
                return Fail(SaveLoadOutcome::CorruptedSave);
            }
            save.players.push_back(p);
        }

        if (!ExpectSection(r, SaveSection::ALife, length))
        {
            return Fail(SaveLoadOutcome::CorruptedSave);
        }
        std::uint32_t alifeCount = 0;
        if (!r.GetU32(alifeCount))
        {
            return Fail(SaveLoadOutcome::CorruptedSave);
        }
        save.alife.reserve(alifeCount);
        for (std::uint32_t i = 0; i < alifeCount; ++i)
        {
            AlifeRestoreRecord a{};
            if (!DecodeAlife(r, a))
            {
                return Fail(SaveLoadOutcome::CorruptedSave);
            }
            save.alife.push_back(a);
        }

        if (!ExpectSection(r, SaveSection::Scheduler, length) || !DecodeScheduler(r, save.scheduler))
        {
            return Fail(SaveLoadOutcome::CorruptedSave);
        }

        // Story section — present in framing; carries no records in this format version.
        if (!ExpectSection(r, SaveSection::Story, length) || !r.Skip(length))
        {
            return Fail(SaveLoadOutcome::CorruptedSave);
        }

        if (!ExpectSection(r, SaveSection::Metadata, length) || !DecodeMetadata(r, save.metadata))
        {
            return Fail(SaveLoadOutcome::CorruptedSave);
        }

        // Content checksum trailer: recompute over everything parsed so far and
        // compare to the stored trailer.
        const std::size_t bodyLength = r.Position();
        std::uint64_t storedChecksum = 0;
        if (!r.GetU64(storedChecksum))
        {
            return Fail(SaveLoadOutcome::CorruptedSave); // missing trailer
        }
        if (Checksum(data, bodyLength) != storedChecksum)
        {
            return Fail(SaveLoadOutcome::ChecksumFailure);
        }

        ParseResult result;
        result.outcome = SaveLoadOutcome::Ok;
        result.save = std::move(save);
        return result;
    }
} // namespace stalkermp::saveload

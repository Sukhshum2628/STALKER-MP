// STALKER-MP — Immutable simulation snapshot (Sprint-008, Step 3)
//
// See SimulationSnapshot.h. Building -> Finalized immutability gate; deterministic
// ascending capture; value outcomes (ADR-007). Engine-free / OS-free.

#include "stalkermp/snapshot/SimulationSnapshot.h"

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::snapshot
{
    void SimulationSnapshot::BeginBuild(const SnapshotId id, const std::uint64_t simulationTick,
                                        const std::uint32_t version) noexcept
    {
        m_entities.clear();
        m_players.clear();
        m_environment = EnvironmentSnapshot{};

        m_metadata = SnapshotMetadata{};
        m_metadata.id = id;
        m_metadata.simulationTick = simulationTick;
        m_metadata.version = version;
        m_metadata.kind = SnapshotKind::Simulation;
        m_metadata.state = SnapshotState::Building;
    }

    core::Expected<void> SimulationSnapshot::AddEntity(const EntitySnapshot& entity)
    {
        if (m_metadata.state != SnapshotState::Building)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "SimulationSnapshot: AddEntity after Finalize (immutable)");
        }
        if (entity.id.value == 0)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "SimulationSnapshot: entity id 0 (none)");
        }
        if (!m_entities.empty() && entity.id.value <= m_entities.back().id.value)
        {
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                common::Format("SimulationSnapshot: entity {} not ascending/unique", entity.id.value));
        }
        m_entities.push_back(entity);
        return core::Success();
    }

    core::Expected<void> SimulationSnapshot::AddPlayer(const PlayerSnapshot& player)
    {
        if (m_metadata.state != SnapshotState::Building)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "SimulationSnapshot: AddPlayer after Finalize (immutable)");
        }
        if (player.id.value == 0)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "SimulationSnapshot: player id 0 (none)");
        }
        if (!m_players.empty() && player.id.value <= m_players.back().id.value)
        {
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                common::Format("SimulationSnapshot: player {} not ascending/unique", player.id.value));
        }
        m_players.push_back(player);
        return core::Success();
    }

    core::Expected<void> SimulationSnapshot::SetEnvironment(const EnvironmentSnapshot& environment)
    {
        if (m_metadata.state != SnapshotState::Building)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "SimulationSnapshot: SetEnvironment after Finalize (immutable)");
        }
        m_environment = environment;
        return core::Success();
    }

    core::Expected<void> SimulationSnapshot::Finalize()
    {
        if (m_metadata.state != SnapshotState::Building)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "SimulationSnapshot: Finalize when not Building");
        }
        m_metadata.entityCount = static_cast<std::uint32_t>(m_entities.size());
        m_metadata.playerCount = static_cast<std::uint32_t>(m_players.size());
        m_metadata.state = SnapshotState::Finalized; // sealed — immutable
        return core::Success();
    }
} // namespace stalkermp::snapshot

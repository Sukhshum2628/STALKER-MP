// STALKER-MP — Immutable replication update (Sprint-009, Step 3)
//
// See ReplicationUpdate.h. Building (state Pending) -> Finalized (state Built)
// immutability gate; deterministic ascending capture; value outcomes (ADR-007).
// Engine-free / OS-free.

#include "stalkermp/replication/ReplicationUpdate.h"

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::replication
{
    void ReplicationUpdate::BeginBuild(const ReplicationId id, const ClientId client,
                                       const std::uint64_t sourceSnapshotId,
                                       const std::uint64_t sourceSnapshotTick) noexcept
    {
        m_entities.clear();
        m_players.clear();

        m_metadata = ReplicationMetadata{};
        m_metadata.id = id;
        m_metadata.client = client;
        m_metadata.sourceSnapshotId = sourceSnapshotId;
        m_metadata.sourceSnapshotTick = sourceSnapshotTick;
        m_metadata.state = ReplicationState::Pending; // building
        m_finalized = false;
    }

    core::Expected<void> ReplicationUpdate::AddEntity(const EntityReplicationState& entity)
    {
        if (m_finalized)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "ReplicationUpdate: AddEntity after Finalize (immutable)");
        }
        if (entity.id.value == 0)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "ReplicationUpdate: entity id 0 (none)");
        }
        if (!m_entities.empty() && entity.id.value <= m_entities.back().id.value)
        {
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                common::Format("ReplicationUpdate: entity {} not ascending/unique", entity.id.value));
        }
        m_entities.push_back(entity);
        return core::Success();
    }

    core::Expected<void> ReplicationUpdate::AddPlayer(const PlayerReplicationState& player)
    {
        if (m_finalized)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "ReplicationUpdate: AddPlayer after Finalize (immutable)");
        }
        if (player.id.value == 0)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "ReplicationUpdate: player id 0 (none)");
        }
        if (!m_players.empty() && player.id.value <= m_players.back().id.value)
        {
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                common::Format("ReplicationUpdate: player {} not ascending/unique", player.id.value));
        }
        m_players.push_back(player);
        return core::Success();
    }

    core::Expected<void> ReplicationUpdate::SetReliability(const ReplicationReliability reliability)
    {
        if (m_finalized)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "ReplicationUpdate: SetReliability after Finalize (immutable)");
        }
        m_metadata.reliability = reliability;
        return core::Success();
    }

    core::Expected<void> ReplicationUpdate::Finalize()
    {
        if (m_finalized)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "ReplicationUpdate: Finalize when already finalized");
        }
        m_metadata.entityCount = static_cast<std::uint32_t>(m_entities.size());
        m_metadata.playerCount = static_cast<std::uint32_t>(m_players.size());
        m_metadata.state = ReplicationState::Built; // sealed — immutable
        m_finalized = true;
        return core::Success();
    }
} // namespace stalkermp::replication

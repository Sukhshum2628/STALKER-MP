// STALKER-MP — Player manager (Sprint-007, Step 7)
//
// See PlayerManager.h. Transactional join/reclaim; deterministic queue draining;
// PlayerRegistry + PlayerLifecycle integration; abstract IPlayerSpawnGateway only.
// Engine-free / OS-free; no exceptions. Online/offline switching is NOT performed
// here (reused from Sprint-005).

#include "stalkermp/player/PlayerManager.h"

#include "stalkermp/player/PlayerLifecycle.h"

namespace stalkermp::player
{
    std::optional<PlayerRecord> PlayerManager::GetRecord(const PlayerId id) const
    {
        for (const PlayerRecord& r : m_registry.Records())
        {
            if (r.id.value == id.value)
            {
                return r;
            }
        }
        return std::nullopt;
    }

    void PlayerManager::UpdateRecord(const PlayerRecord& record)
    {
        // Mutate a stored record by removing and re-inserting it (same PlayerId).
        // The slot is freed by Retire, so Insert cannot hit capacity or duplicate.
        m_registry.Retire(record.id);
        const auto inserted = m_registry.Insert(record);
        (void)inserted; // guaranteed to succeed after Retire of the same id
    }

    world::PlayerPosition PlayerManager::DefaultSpawnPosition(const PlayerId id, const std::uint64_t tick) const
    {
        // Deterministic default spawn (spawn-location policy resolution is Step 8/10).
        world::PlayerPosition p{};
        p.id = id;
        p.position = world::Vec3{};
        p.lastUpdateTick = tick;
        return p;
    }

    JoinOutcome PlayerManager::RequestJoin(const net::ConnectionId connection, const PlayerProfile& profile,
                                           const std::uint64_t tick)
    {
        if (connection.value == 0)
        {
            return JoinOutcome::RejectedInvalid;
        }
        if (m_registry.FindByConnection(connection).has_value())
        {
            return JoinOutcome::RejectedDuplicate;
        }
        if (m_registry.size() >= m_config.maxPlayers)
        {
            return JoinOutcome::RejectedCapacity;
        }

        const PlayerId id = m_registry.Allocate();
        const world::PlayerPosition spawnPos = DefaultSpawnPosition(id, tick);

        // Transactional: materialize the entity first; on failure insert nothing
        // (no orphan record/mapping).
        auto spawned = m_gateway.Spawn(profile, spawnPos);
        if (!spawned.HasValue())
        {
            return JoinOutcome::RejectedSpawnFailed;
        }

        PlayerRecord record{};
        record.id = id;
        record.connection = connection;
        record.sessionMember = connection.value; // stable membership key for reclaim
        record.entity = spawned.Value();
        record.lifecycle = PlayerLifecycleState::Joining;
        record.connectionState = PlayerConnectionState::Connected;
        record.lastPosition = spawnPos;
        record.joinTick = tick;

        // Complete the join through the lifecycle rule (Joining -> Active).
        const auto next = ApplyJoin(record, tick);
        if (!next.HasValue())
        {
            (void)m_gateway.Despawn(record.entity); // roll back the entity
            return JoinOutcome::RejectedInvalid;
        }
        record.lifecycle = next.Value();

        const auto inserted = m_registry.Insert(record);
        if (!inserted.HasValue())
        {
            (void)m_gateway.Despawn(record.entity); // roll back the entity, no orphan
            return JoinOutcome::RejectedCapacity;
        }

        m_lastJoinTick = tick;
        return JoinOutcome::Accepted;
    }

    void PlayerManager::SuspendOnLeave(const PlayerSessionDelta& delta)
    {
        const auto view = m_registry.FindByConnection(delta.connection);
        if (!view.has_value())
        {
            return; // connection was never a player; nothing to suspend
        }
        auto recordOpt = GetRecord(view->id);
        if (!recordOpt.has_value())
        {
            return;
        }
        PlayerRecord record = recordOpt.value();

        // Disconnect always RETAINS (persistent): suspend, keep the record/entity.
        const auto next = ApplySuspend(record, DisconnectDisposition::Retain);
        if (!next.HasValue())
        {
            return; // not in a suspendable state; leave unchanged
        }
        record.lifecycle = next.Value();
        record.connectionState = PlayerConnectionState::Suspended;
        record.reconnectToken = delta.reconnectToken; // for later reclaim
        record.connection = net::ConnectionId{0};     // clear the network binding
        // sessionMember retained (old connection value) so reclaim can resolve it.
        UpdateRecord(record);
    }

    void PlayerManager::ApplyPendingDeltas(PlayerDeltaQueue& queue, const std::uint64_t tick)
    {
        // Deterministic order (ascending ConnectionId) is guaranteed by Drain().
        const std::vector<PlayerSessionDelta> deltas = queue.Drain();
        for (const PlayerSessionDelta& delta : deltas)
        {
            switch (delta.kind)
            {
            case PlayerSessionEventKind::Joined:
                // Session-level join creates a player with the default profile
                // (the profile-carrying / reconnect-token path is RequestJoin,
                // message-driven in Step 11). Result intentionally not surfaced
                // here (duplicate/capacity rejections are benign no-ops).
                (void)RequestJoin(delta.connection, PlayerProfile{}, delta.tick);
                break;
            case PlayerSessionEventKind::Left:
                SuspendOnLeave(delta);
                break;
            }
        }
        (void)tick; // deltas carry their own event ticks
    }

    ReconnectOutcome PlayerManager::ApplyReclaim(const net::ConnectionId newConnection,
                                                 const std::uint64_t reconnectToken, const std::uint64_t tick)
    {
        if (newConnection.value == 0)
        {
            return ReconnectOutcome::RejectedInvalid;
        }

        // Reconnect authority is the Sprint-006 session token seam. (Sprint-006's
        // TryReclaim is a reserved stub returning NotFound; full reclaim lands in
        // Sprint-012. The rebind path below is exercised once the seam succeeds.)
        auto reclaimed = m_session.TryReclaim(reconnectToken);
        if (!reclaimed.HasValue())
        {
            return ReconnectOutcome::TokenUnknown;
        }
        const net::ConnectionId oldConnection = reclaimed.Value();

        const auto view = m_registry.FindBySession(oldConnection.value);
        if (!view.has_value())
        {
            return ReconnectOutcome::RejectedInvalid;
        }
        auto recordOpt = GetRecord(view->id);
        if (!recordOpt.has_value())
        {
            return ReconnectOutcome::RejectedInvalid;
        }
        PlayerRecord record = recordOpt.value();

        if (record.lifecycle != PlayerLifecycleState::Suspended)
        {
            return ReconnectOutcome::AlreadyActive;
        }

        const auto next = player::ApplyReclaim(record, tick); // Suspended -> Active (free lifecycle fn)
        if (!next.HasValue())
        {
            return ReconnectOutcome::RejectedInvalid;
        }

        // Rebind to the new connection, keeping the SAME PlayerId/EntityId.
        record.lifecycle = next.Value();
        record.connectionState = PlayerConnectionState::Reclaimed;
        record.connection = newConnection;
        record.sessionMember = newConnection.value;
        record.reconnectToken = 0;
        UpdateRecord(record);

        ++m_reconnects;
        return ReconnectOutcome::Reclaimed;
    }

    SpawnOutcome PlayerManager::RequestRespawn(const PlayerId id, const std::uint64_t tick)
    {
        auto recordOpt = GetRecord(id);
        if (!recordOpt.has_value())
        {
            return SpawnOutcome::RejectedInvalid;
        }
        PlayerRecord record = recordOpt.value();

        const auto next = player::ApplyRespawn(record, tick, m_config);
        if (!next.HasValue())
        {
            return SpawnOutcome::RejectedInvalid; // illegal from the current state
        }
        if (next.Value() != PlayerLifecycleState::Active)
        {
            // Not yet eligible: the request is DENIED and the record is left
            // unchanged (Step-13 hardening: a rejection never mutates state —
            // Architecture §18). The player may request again once eligible.
            return SpawnOutcome::RejectedInvalid;
        }

        // Eligible: the persistent entity survives death, so respawn re-activates
        // and repositions — no new Spawn call.
        record.lifecycle = PlayerLifecycleState::Active;
        record.lastPosition = DefaultSpawnPosition(id, tick);
        record.respawnEligibleTick = 0;
        UpdateRecord(record);
        ++m_respawns;
        return SpawnOutcome::Spawned;
    }

    JoinOutcome PlayerManager::NotifyDeath(const PlayerId id, const std::uint64_t tick)
    {
        auto recordOpt = GetRecord(id);
        if (!recordOpt.has_value())
        {
            return JoinOutcome::RejectedInvalid;
        }
        PlayerRecord record = recordOpt.value();

        const auto next = ApplyDeath(record, tick);
        if (!next.HasValue())
        {
            return JoinOutcome::RejectedInvalid;
        }
        record.lifecycle = next.Value(); // Dead
        record.respawnEligibleTick = ComputeRespawnEligibleTick(tick, m_config);
        UpdateRecord(record);
        ++m_deaths;
        return JoinOutcome::Accepted;
    }

    PlayerStatistics PlayerManager::Statistics() const
    {
        PlayerStatistics s{};
        s.respawns = m_respawns;
        s.deaths = m_deaths;
        s.reconnects = m_reconnects;
        s.joinTick = m_lastJoinTick;
        s.averageSessionDurationTicks = 0; // populated by Step-12 diagnostics
        for (const PlayerRecord& r : m_registry.Records())
        {
            switch (r.lifecycle)
            {
            case PlayerLifecycleState::Active:
                ++s.connected;
                break;
            case PlayerLifecycleState::Suspended:
                ++s.suspended;
                break;
            case PlayerLifecycleState::Dead:
            case PlayerLifecycleState::AwaitingRespawn:
                ++s.dead;
                break;
            case PlayerLifecycleState::Joining:
            case PlayerLifecycleState::Removed:
                break;
            }
        }
        return s;
    }

    std::vector<world::PlayerPosition> PlayerManager::ActivePlayerPositions() const
    {
        // Ascending PlayerId (Records() is ascending), positions only, Active only.
        std::vector<world::PlayerPosition> out;
        for (const PlayerRecord& r : m_registry.Records())
        {
            if (r.lifecycle == PlayerLifecycleState::Active)
            {
                out.push_back(r.lastPosition);
            }
        }
        return out;
    }
} // namespace stalkermp::player

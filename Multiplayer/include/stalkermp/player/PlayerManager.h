// STALKER-MP — Player manager (Sprint-007, Step 7)
//
// The engine-free core: drains the PlayerDeltaQueue at a tick and applies
// join/suspend/reclaim/death/respawn transactionally against the PlayerRegistry
// (via the Step-5 lifecycle rules), requesting entity materialization/removal
// through the abstract IPlayerSpawnGateway. ALL mutation happens here at the
// manager's tick — never in the observer (Architecture AD-3). Host Authority:
// client input is request-only; every transition is decided here. Online/offline
// switching is NOT performed here (reused from Sprint-005, FR-4/AD-5).
//
// Engine-free / OS-free. ADR-007; ADR-011 (single-threaded). Depends only on the
// abstract IPlayerSpawnGateway (Clarifications §7.1).

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "stalkermp/net/NetTypes.h"    // net::ConnectionId
#include "stalkermp/net/Session.h"     // net::Session
#include "stalkermp/player/IPlayerSpawnGateway.h"
#include "stalkermp/player/PlayerConfiguration.h"
#include "stalkermp/player/PlayerDeltaQueue.h"
#include "stalkermp/player/PlayerRegistry.h"
#include "stalkermp/player/PlayerTypes.h"
#include "stalkermp/world/BubbleTypes.h" // world::PlayerPosition

namespace stalkermp::player
{
    class PlayerManager
    {
    public:
        PlayerManager(const PlayerConfiguration& config, IPlayerSpawnGateway& gateway, net::Session& session)
            : m_config(config), m_gateway(gateway), m_session(session), m_registry(config.maxPlayers)
        {
        }

        // Host-decided join with an explicit profile (message-driven entry, Step 11).
        [[nodiscard]] JoinOutcome RequestJoin(net::ConnectionId connection, const PlayerProfile& profile,
                                              std::uint64_t tick);

        // Drain queued session deltas and apply them deterministically at `tick`.
        void ApplyPendingDeltas(PlayerDeltaQueue& queue, std::uint64_t tick);

        // Reconnect: reclaim a retained player via the Sprint-006 token seam.
        [[nodiscard]] ReconnectOutcome ApplyReclaim(net::ConnectionId newConnection, std::uint64_t reconnectToken,
                                                    std::uint64_t tick);

        // Respawn an eligible dead player (entity persists; no new spawn).
        [[nodiscard]] SpawnOutcome RequestRespawn(PlayerId id, std::uint64_t tick);

        // Record a player's death (no auto-destruction; schedules respawn).
        [[nodiscard]] JoinOutcome NotifyDeath(PlayerId id, std::uint64_t tick);

        // --- Read-only surface -----------------------------------------------
        [[nodiscard]] std::optional<PlayerMappingView> FindByPlayerId(PlayerId id) const
        {
            return m_registry.FindByPlayerId(id);
        }
        [[nodiscard]] std::optional<PlayerMappingView> FindByConnection(net::ConnectionId c) const
        {
            return m_registry.FindByConnection(c);
        }
        [[nodiscard]] std::optional<PlayerMappingView> FindBySession(std::uint64_t s) const
        {
            return m_registry.FindBySession(s);
        }
        [[nodiscard]] std::optional<PlayerMappingView> FindByEntity(world::EntityId e) const
        {
            return m_registry.FindByEntity(e);
        }

        [[nodiscard]] PlayerStatistics Statistics() const;
        [[nodiscard]] PlayerRegistryConsistency ValidateConsistency() const { return m_registry.ValidateConsistency(); }

        // Positions-only ascending snapshot backing the Step-8 position feed.
        [[nodiscard]] std::vector<world::PlayerPosition> ActivePlayerPositions() const;

        [[nodiscard]] std::size_t PlayerCount() const noexcept { return m_registry.size(); }

        // Read-only registry access for diagnostics (Step 12; Architecture §20,
        // "read-only access to PlayerRegistry"). Exposes the const registry only —
        // no mutation path is added.
        [[nodiscard]] const PlayerRegistry& Registry() const noexcept { return m_registry; }

    private:
        [[nodiscard]] std::optional<PlayerRecord> GetRecord(PlayerId id) const;
        void UpdateRecord(const PlayerRecord& record); // Retire + re-Insert (same id)
        [[nodiscard]] world::PlayerPosition DefaultSpawnPosition(PlayerId id, std::uint64_t tick) const;
        void SuspendOnLeave(const PlayerSessionDelta& delta);

        PlayerConfiguration m_config;
        IPlayerSpawnGateway& m_gateway; // non-owning
        net::Session& m_session;        // non-owning (Sprint-006)
        PlayerRegistry m_registry;      // owned

        // Cumulative counters (respawns/deaths/reconnects/last join tick);
        // connected/suspended/dead are computed live in Statistics().
        std::uint32_t m_respawns = 0;
        std::uint32_t m_deaths = 0;
        std::uint32_t m_reconnects = 0;
        std::uint64_t m_lastJoinTick = 0;
    };
} // namespace stalkermp::player

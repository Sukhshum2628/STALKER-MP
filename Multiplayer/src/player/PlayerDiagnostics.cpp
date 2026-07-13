// STALKER-MP — Player diagnostics (Sprint-007, Step 12)
//
// See PlayerDiagnostics.h. Strictly read-only; iostream-free (common::Format);
// tick-derived (replay-stable). Engine-free / OS-free.

#include "stalkermp/player/PlayerDiagnostics.h"

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::player
{
    bool PlayerDiagnostics::PositionFeedAgrees() const
    {
        std::size_t activeRecords = 0;
        for (const PlayerRecord& r : m_manager.Registry().Records())
        {
            if (r.lifecycle == PlayerLifecycleState::Active)
            {
                ++activeRecords;
            }
        }
        return m_manager.ActivePlayerPositions().size() == activeRecords;
    }

    PlayerRegistryConsistency PlayerDiagnostics::ValidateConsistency() const
    {
        // Delegate the registry-level checks (ordering, accelerators, bijection,
        // no-zero-id, capacity), then extend with the position-feed agreement.
        PlayerRegistryConsistency report = m_manager.ValidateConsistency();
        if (!PositionFeedAgrees())
        {
            // Fold the extra check into the health signal without inventing a new
            // report field (PlayerRegistryConsistency is frozen at Step 4).
            report.mappingBijective = report.mappingBijective && false;
        }
        return report;
    }

    std::string PlayerDiagnostics::DescribeRecord(const PlayerRecord& r) const
    {
        return common::Format(
            "player {} conn={} session={} entity={} lifecycle={} connState={} joinTick={} respawnTick={}",
            r.id.value, r.connection.value, r.sessionMember, r.entity.value,
            PlayerLifecycleStateName(r.lifecycle), PlayerConnectionStateName(r.connectionState), r.joinTick,
            r.respawnEligibleTick);
    }

    std::string PlayerDiagnostics::DescribeState() const
    {
        const PlayerStatistics s = m_manager.Statistics();
        const bool healthy = ValidateConsistency().IsHealthy();
        return common::Format(
            "Players: {} (connected {}, suspended {}, dead {}); respawns {}, deaths {}, reconnects {}; "
            "consistency: {}",
            m_manager.PlayerCount(), s.connected, s.suspended, s.dead, s.respawns, s.deaths, s.reconnects,
            healthy ? "healthy" : "unhealthy");
    }

    std::string PlayerDiagnostics::DescribePlayer(const PlayerId id) const
    {
        for (const PlayerRecord& r : m_manager.Registry().Records())
        {
            if (r.id.value == id.value)
            {
                return DescribeRecord(r);
            }
        }
        return common::Format("player {} absent", id.value);
    }

    std::string PlayerDiagnostics::DumpPlayers() const
    {
        // Ascending by PlayerId (Records() is the ascending canonical vector).
        std::string out = common::Format("PlayerRegistry ({} players):", m_manager.PlayerCount());
        for (const PlayerRecord& r : m_manager.Registry().Records())
        {
            out += "\n  ";
            out += DescribeRecord(r);
        }
        return out;
    }
} // namespace stalkermp::player

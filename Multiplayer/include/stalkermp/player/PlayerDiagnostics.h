// STALKER-MP — Player diagnostics (Sprint-007, Step 12)
//
// Read-only, replay-stable observability over the PlayerManager / PlayerRegistry
// (Architecture §20). Statistics tallies, human-readable state/player dumps
// (ascending by PlayerId), and a delegated + extended consistency report. STRICTLY
// READ-ONLY — never mutates runtime state. All facts are tick-derived, so the
// output is deterministic and replay-stable. iostream-free (common::Format).
//
// Engine-free / OS-free. ADR-007. Non-owning (holds a const PlayerManager&).

#pragma once

#include <string>

#include "stalkermp/player/PlayerManager.h"
#include "stalkermp/player/PlayerRegistry.h" // PlayerRegistryConsistency
#include "stalkermp/player/PlayerTypes.h"    // PlayerStatistics, PlayerId

namespace stalkermp::player
{
    class PlayerDiagnostics
    {
    public:
        explicit PlayerDiagnostics(const PlayerManager& manager) : m_manager(manager) {}

        // Tick-derived tallies (connected/suspended/dead + cumulative
        // respawns/deaths/reconnects + last join tick). averageSessionDurationTicks
        // is reserved (0) — no per-session end tick is tracked in Sprint-007.
        [[nodiscard]] PlayerStatistics Statistics() const { return m_manager.Statistics(); }

        // One-line subsystem summary (counts + consistency health).
        [[nodiscard]] std::string DescribeState() const;

        // Full description of one player (mapping + lifecycle/connection state +
        // tick fields). Absent id -> a stable "absent" line.
        [[nodiscard]] std::string DescribePlayer(PlayerId id) const;

        // All players, ascending by PlayerId, one per line (empty -> a header only).
        [[nodiscard]] std::string DumpPlayers() const;

        // Delegates the registry consistency (ordering, accelerators, bijection)
        // and additionally confirms the position feed agrees with the registry
        // (active-record count == ActivePlayerPositions size). The returned report
        // reflects both (feed mismatch clears acceleratorsConsistent-independent
        // health via the extra check folded into IsHealthy()).
        [[nodiscard]] PlayerRegistryConsistency ValidateConsistency() const;

        // The extra Step-12 check surfaced explicitly: the active position feed
        // matches the count of Active records (read-only, deterministic).
        [[nodiscard]] bool PositionFeedAgrees() const;

    private:
        [[nodiscard]] std::string DescribeRecord(const PlayerRecord& record) const;

        const PlayerManager& m_manager; // non-owning
    };
} // namespace stalkermp::player

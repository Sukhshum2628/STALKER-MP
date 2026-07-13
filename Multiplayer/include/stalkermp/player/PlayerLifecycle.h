// STALKER-MP — Player lifecycle state machine (Sprint-007, Step 5)
//
// Pure, host-authoritative transition rules over a single PlayerRecord: legal
// transitions over PlayerLifecycleState, tick-derived respawn scheduling, and
// per-transition value outcomes. STATELESS and PURE — these functions never
// mutate the passed record and perform no I/O; the caller (Step 7 manager)
// applies the returned state and persists fields. NO registry, session, gateway,
// tick subscription, or spawn/despawn here.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI; illegal transitions are
// value outcomes (never asserts on external input).

#pragma once

#include <cstdint>

#include "stalkermp/core/Expected.h"
#include "stalkermp/player/PlayerConfiguration.h"
#include "stalkermp/player/PlayerTypes.h"

namespace stalkermp::player
{
    // Legal transition: Joining -> Active (join pipeline completes). The tick is
    // recorded by the caller; the transition rule itself does not use it.
    [[nodiscard]] core::Expected<PlayerLifecycleState> ApplyJoin(const PlayerRecord& record, std::uint64_t tick);

    // Legal transition: Active -> Dead. Death never auto-destroys the record.
    [[nodiscard]] core::Expected<PlayerLifecycleState> ApplyDeath(const PlayerRecord& record, std::uint64_t tick);

    // Legal transition: Dead|AwaitingRespawn -> (AwaitingRespawn while waiting,
    // Active once the tick-derived respawn delay has elapsed). Eligibility is the
    // record's stored respawnEligibleTick when set, else computed from `tick` and
    // the configured delay.
    [[nodiscard]] core::Expected<PlayerLifecycleState>
    ApplyRespawn(const PlayerRecord& record, std::uint64_t tick, const PlayerConfiguration& config);

    // Legal transition on disconnect: Retain suspends (Active|Dead|AwaitingRespawn
    // -> Suspended, keeping the record/entity persistent); Remove releases
    // (any -> Removed).
    [[nodiscard]] core::Expected<PlayerLifecycleState>
    ApplySuspend(const PlayerRecord& record, DisconnectDisposition disposition);

    // Legal transition on reconnect: Suspended -> Active (reclaim rebinds).
    [[nodiscard]] core::Expected<PlayerLifecycleState> ApplyReclaim(const PlayerRecord& record, std::uint64_t tick);

    // Legal transition: any -> Removed (explicit removal / release). Always legal.
    [[nodiscard]] core::Expected<PlayerLifecycleState> ApplyRemove(const PlayerRecord& record);

    // Tick-derived respawn schedule: deathTick + respawnDelayTicks (no wall-clock).
    [[nodiscard]] std::uint64_t ComputeRespawnEligibleTick(std::uint64_t deathTick,
                                                           const PlayerConfiguration& config) noexcept;
} // namespace stalkermp::player

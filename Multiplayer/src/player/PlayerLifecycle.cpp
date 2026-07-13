// STALKER-MP — Player lifecycle state machine (Sprint-007, Step 5)
//
// See PlayerLifecycle.h. Pure, stateless single-record transition rules. Illegal
// transitions are value outcomes (InvalidArgument + message). Tick-derived
// respawn timing. Engine-free / OS-free; no exceptions.

#include "stalkermp/player/PlayerLifecycle.h"

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::player
{
    namespace
    {
        [[nodiscard]] core::Error IllegalTransition(const char* op, const PlayerLifecycleState from)
        {
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                common::Format("PlayerLifecycle: illegal {} from state '{}'", op, PlayerLifecycleStateName(from)));
        }
    } // namespace

    core::Expected<PlayerLifecycleState> ApplyJoin(const PlayerRecord& record, std::uint64_t /*tick*/)
    {
        if (record.lifecycle != PlayerLifecycleState::Joining)
        {
            return IllegalTransition("Join", record.lifecycle);
        }
        return PlayerLifecycleState::Active;
    }

    core::Expected<PlayerLifecycleState> ApplyDeath(const PlayerRecord& record, std::uint64_t /*tick*/)
    {
        if (record.lifecycle != PlayerLifecycleState::Active)
        {
            return IllegalTransition("Death", record.lifecycle);
        }
        return PlayerLifecycleState::Dead;
    }

    core::Expected<PlayerLifecycleState>
    ApplyRespawn(const PlayerRecord& record, const std::uint64_t tick, const PlayerConfiguration& config)
    {
        if (record.lifecycle != PlayerLifecycleState::Dead &&
            record.lifecycle != PlayerLifecycleState::AwaitingRespawn)
        {
            return IllegalTransition("Respawn", record.lifecycle);
        }

        // Eligibility: use the record's scheduled tick when set (populated at death
        // by the caller via ComputeRespawnEligibleTick); otherwise derive it now
        // from the current tick and the configured delay.
        const std::uint64_t eligible = (record.respawnEligibleTick != 0)
                                           ? record.respawnEligibleTick
                                           : ComputeRespawnEligibleTick(tick, config);

        return (tick >= eligible) ? PlayerLifecycleState::Active : PlayerLifecycleState::AwaitingRespawn;
    }

    core::Expected<PlayerLifecycleState>
    ApplySuspend(const PlayerRecord& record, const DisconnectDisposition disposition)
    {
        if (disposition == DisconnectDisposition::Remove)
        {
            return PlayerLifecycleState::Removed; // explicit release, any state
        }
        // Retain: suspend a live-ish record, keeping it persistent.
        switch (record.lifecycle)
        {
        case PlayerLifecycleState::Active:
        case PlayerLifecycleState::Dead:
        case PlayerLifecycleState::AwaitingRespawn:
            return PlayerLifecycleState::Suspended;
        case PlayerLifecycleState::Joining:
        case PlayerLifecycleState::Suspended:
        case PlayerLifecycleState::Removed:
            break;
        }
        return IllegalTransition("Suspend(Retain)", record.lifecycle);
    }

    core::Expected<PlayerLifecycleState> ApplyReclaim(const PlayerRecord& record, std::uint64_t /*tick*/)
    {
        if (record.lifecycle != PlayerLifecycleState::Suspended)
        {
            return IllegalTransition("Reclaim", record.lifecycle);
        }
        return PlayerLifecycleState::Active;
    }

    core::Expected<PlayerLifecycleState> ApplyRemove(const PlayerRecord& /*record*/)
    {
        return PlayerLifecycleState::Removed; // any -> Removed, always legal
    }

    std::uint64_t ComputeRespawnEligibleTick(const std::uint64_t deathTick,
                                             const PlayerConfiguration& config) noexcept
    {
        return deathTick + static_cast<std::uint64_t>(config.respawnDelayTicks);
    }
} // namespace stalkermp::player

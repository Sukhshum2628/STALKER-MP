// STALKER-MP — Bubble Manager core implementation (Sprint-004, §7.1/§7.7)
//
// See BubbleManager.h. State and query surface only — no computation, no sources,
// no tick logic. Engine-agnostic; no exceptions, no throwing STL (ADR-007).

#include "stalkermp/world/BubbleManager.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "stalkermp/common/StringUtil.h"           // common::Format (ADR-007-safe)
#include "stalkermp/world/IPlayerPositionSource.h" // completes the forward-declared seam
#include "stalkermp/world/ISpatialQueries.h"       // completes the forward-declared seam

namespace stalkermp::world
{
    BubbleManager::BubbleManager(BubbleConfiguration configuration,
                                 const ISpatialQueries* spatialQueries,
                                 const IPlayerPositionSource* playerSource)
        : m_configuration(std::move(configuration))
        , m_spatialQueries(spatialQueries)
        , m_playerSource(playerSource)
    {
    }

    void BubbleManager::Update(const std::uint64_t currentTick)
    {
        const std::vector<PlayerPosition> players =
            m_playerSource != nullptr ? m_playerSource->ActivePlayers() : std::vector<PlayerPosition>{};

        // Effective radii (frozen §3C). Float math throughout to match Vec3 and
        // avoid narrowing; squared comparisons avoid sqrt and are exact.
        const float rOn = static_cast<float>(m_configuration.ActivationRadius());
        const float rOff = static_cast<float>(m_configuration.DeactivationRadius());
        const float rOn2 = rOn * rOn;

        // Candidate entities = union of QueryRadius(player, R_off) over all players
        // (no materialized merge). Deduped by EntityId; position kept for the
        // min-distance test. Empty when there is no player/spatial source.
        std::unordered_map<std::uint32_t, Vec3> candidates;
        if (m_spatialQueries != nullptr)
        {
            for (const PlayerPosition& player : players) // ascending PlayerId (B9)
            {
                const std::vector<EntityView> found = m_spatialQueries->QueryRadius(player.position, rOff);
                for (const EntityView& view : found)
                {
                    candidates.emplace(view.id.value, view.position); // dedup; positions equal per id
                }
            }
        }

        // Deterministic evaluation order: candidate ids ascending (B9).
        std::vector<std::uint32_t> candidateIds;
        candidateIds.reserve(candidates.size());
        for (const auto& entry : candidates)
        {
            candidateIds.push_back(entry.first);
        }
        std::sort(candidateIds.begin(), candidateIds.end());

        // New online set, applying hysteresis against the prior online set
        // (m_online, still holding last tick's result) — B10.
        std::vector<std::uint32_t> newOnline;
        newOnline.reserve(candidateIds.size());
        for (const std::uint32_t id : candidateIds)
        {
            const auto found = candidates.find(id);
            if (found == candidates.end())
            {
                continue; // unreachable; ids come from candidates
            }
            const Vec3& position = found->second;

            // Min squared distance to any player. A candidate is within R_off of
            // at least one player, so at least one player exists here.
            float bestSquared = 0.0f;
            bool hasDistance = false;
            for (const PlayerPosition& player : players)
            {
                const float squared = DistanceSquared(position, player.position);
                if (!hasDistance || squared < bestSquared)
                {
                    bestSquared = squared;
                    hasDistance = true;
                }
            }

            const bool wasOnline = std::binary_search(m_online.begin(), m_online.end(), id);
            if (wasOnline)
            {
                // Online stays online while within R_off (candidate ⇒ within R_off).
                newOnline.push_back(id);
            }
            else if (hasDistance && bestSquared <= rOn2)
            {
                // Offline turns online only inside the tighter R_on (entering).
                newOnline.push_back(id);
            }
            // Offline within the [R_on, R_off] hysteresis band stays offline.
        }
        // newOnline is ascending (candidateIds ascending; order-preserving pushes).

        m_previousOnline = std::move(m_online);
        m_online = std::move(newOnline);

        // Transitions this Update, derived strictly from the two sorted sets (B9,
        // ascending EntityId). Activations = online - previous; deactivations =
        // previous - online. std::set_difference requires sorted inputs, which
        // both sets are. No external state is consulted.
        std::vector<std::uint32_t> activatedIds;
        std::set_difference(m_online.begin(), m_online.end(),
                            m_previousOnline.begin(), m_previousOnline.end(),
                            std::back_inserter(activatedIds));

        std::vector<std::uint32_t> deactivatedIds;
        std::set_difference(m_previousOnline.begin(), m_previousOnline.end(),
                            m_online.begin(), m_online.end(),
                            std::back_inserter(deactivatedIds));

        m_activations.clear();
        m_activations.reserve(activatedIds.size());
        for (const std::uint32_t id : activatedIds)
        {
            m_activations.push_back(EntityId{id});
        }

        m_deactivations.clear();
        m_deactivations.reserve(deactivatedIds.size());
        for (const std::uint32_t id : deactivatedIds)
        {
            m_deactivations.push_back(EntityId{id});
        }

        m_state.activePlayerCount = players.size();
        m_state.onlineEntityCount = m_online.size();
        m_state.updateTick = currentTick;
    }

    BubbleMembership BubbleManager::MembershipOf(const EntityId id) const noexcept
    {
        const bool inCurrent = std::binary_search(m_online.begin(), m_online.end(), id.value);
        const bool inPrevious = std::binary_search(m_previousOnline.begin(), m_previousOnline.end(), id.value);

        if (inCurrent)
        {
            return inPrevious ? BubbleMembership::Inside : BubbleMembership::PendingActivation;
        }
        return inPrevious ? BubbleMembership::PendingDeactivation : BubbleMembership::Outside;
    }

    bool BubbleManager::IsEntityInside(const EntityId id) const noexcept
    {
        // m_online is sorted ascending by EntityId value. Empty until the compute
        // step populates it, so this returns false for now.
        return std::binary_search(m_online.begin(), m_online.end(), id.value);
    }

    bool BubbleManager::IsPositionInside(const Vec3& /*position*/) const noexcept
    {
        // Requires player positions and the activation algorithm (later steps).
        return false;
    }

    std::optional<PlayerPosition> BubbleManager::NearestBubble(const Vec3& /*position*/) const noexcept
    {
        // Requires the player-position source (later step).
        return std::nullopt;
    }

    double BubbleManager::ActiveBubbleRadius() const noexcept
    {
        return m_state.IsActive() ? m_configuration.ActivationRadius() : 0.0;
    }

    // ---------------------------------------------------------- Diagnostics

    std::vector<PlayerPosition> BubbleManager::ActivePlayers() const
    {
        return m_playerSource != nullptr ? m_playerSource->ActivePlayers()
                                         : std::vector<PlayerPosition>{};
    }

    std::vector<EntityId> BubbleManager::OnlineEntities() const
    {
        std::vector<EntityId> result;
        result.reserve(m_online.size());
        for (const std::uint32_t id : m_online) // ascending (B9)
        {
            result.push_back(EntityId{id});
        }
        return result;
    }

    std::string BubbleManager::DescribeState() const
    {
        return common::Format(
            "Bubble tick={} players={} online={} activations={} deactivations={} R_on={} R_off={}",
            m_state.updateTick, m_state.activePlayerCount, m_state.onlineEntityCount,
            m_activations.size(), m_deactivations.size(),
            m_configuration.ActivationRadius(), m_configuration.DeactivationRadius());
    }

    std::string BubbleManager::DumpOnline() const
    {
        std::string out = common::Format("Online entities ({}):", m_online.size());
        for (const std::uint32_t id : m_online) // ascending (B9)
        {
            out += ' ';
            out += std::to_string(id);
        }
        return out;
    }

    std::string BubbleManager::DumpTransitions() const
    {
        std::string out = common::Format("Activations ({}):", m_activations.size());
        for (const EntityId id : m_activations) // ascending (B9)
        {
            out += ' ';
            out += std::to_string(id.value);
        }
        out += common::Format(" | Deactivations ({}):", m_deactivations.size());
        for (const EntityId id : m_deactivations) // ascending (B9)
        {
            out += ' ';
            out += std::to_string(id.value);
        }
        return out;
    }

    BubbleConsistencyReport BubbleManager::ValidateConsistency() const
    {
        BubbleConsistencyReport report;

        // Online / previous sets must be strictly ascending and unique (B9).
        report.onlineSorted =
            std::is_sorted(m_online.begin(), m_online.end()) &&
            std::adjacent_find(m_online.begin(), m_online.end()) == m_online.end();
        report.previousSorted =
            std::is_sorted(m_previousOnline.begin(), m_previousOnline.end()) &&
            std::adjacent_find(m_previousOnline.begin(), m_previousOnline.end()) == m_previousOnline.end();

        // Effective radii must satisfy the hysteresis ordering (B10).
        report.radiiValid = m_configuration.ActivationRadius() <= m_configuration.DeactivationRadius();

        // Stored transitions must equal the set differences of the two sets (Step 6).
        std::vector<std::uint32_t> expectedActivated;
        std::set_difference(m_online.begin(), m_online.end(),
                            m_previousOnline.begin(), m_previousOnline.end(),
                            std::back_inserter(expectedActivated));
        std::vector<std::uint32_t> expectedDeactivated;
        std::set_difference(m_previousOnline.begin(), m_previousOnline.end(),
                            m_online.begin(), m_online.end(),
                            std::back_inserter(expectedDeactivated));

        bool consistent = expectedActivated.size() == m_activations.size() &&
                          expectedDeactivated.size() == m_deactivations.size();
        for (std::size_t i = 0; consistent && i < m_activations.size(); ++i)
        {
            consistent = m_activations[i].value == expectedActivated[i];
        }
        for (std::size_t i = 0; consistent && i < m_deactivations.size(); ++i)
        {
            consistent = m_deactivations[i].value == expectedDeactivated[i];
        }
        report.transitionsConsistent = consistent;

        return report;
    }
} // namespace stalkermp::world

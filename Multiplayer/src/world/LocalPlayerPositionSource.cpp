// STALKER-MP — Local player position source (Sprint-004, §7.2)
//
// See LocalPlayerPositionSource.h. Engine-free; no exceptions, no throwing STL
// (ADR-007). Storage is kept sorted ascending by PlayerId so ActivePlayers()
// returns a deterministic snapshot (B9).

#include "stalkermp/world/LocalPlayerPositionSource.h"

#include <algorithm>

#include "stalkermp/core/Error.h"

namespace stalkermp::world
{
    std::size_t LocalPlayerPositionSource::IndexOf(const PlayerId id) const noexcept
    {
        const auto it = std::lower_bound(
            m_players.begin(), m_players.end(), id.value,
            [](const PlayerPosition& player, const std::uint32_t value) noexcept {
                return player.id.value < value;
            });
        if (it != m_players.end() && it->id.value == id.value)
        {
            return static_cast<std::size_t>(it - m_players.begin());
        }
        return kInvalidIndex;
    }

    std::vector<PlayerPosition> LocalPlayerPositionSource::ActivePlayers() const
    {
        // Already sorted ascending by PlayerId (B9); returned by value (immutable
        // snapshot semantics).
        return m_players;
    }

    core::Expected<void>
    LocalPlayerPositionSource::SetPlayer(const PlayerId id, const Vec3& position, const std::uint64_t tick)
    {
        if (id.value == 0)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "LocalPlayerPositionSource::SetPlayer: null PlayerId");
        }

        const auto slot = std::lower_bound(
            m_players.begin(), m_players.end(), id.value,
            [](const PlayerPosition& player, const std::uint32_t value) noexcept {
                return player.id.value < value;
            });

        if (slot != m_players.end() && slot->id.value == id.value)
        {
            slot->position = position;
            slot->lastUpdateTick = tick;
            return core::Success();
        }

        PlayerPosition entry;
        entry.id = id;
        entry.position = position;
        entry.lastUpdateTick = tick;
        m_players.insert(slot, entry); // preserves ascending-PlayerId order (B9)
        return core::Success();
    }

    core::Expected<void> LocalPlayerPositionSource::RemovePlayer(const PlayerId id)
    {
        const std::size_t index = IndexOf(id);
        if (index == kInvalidIndex)
        {
            return core::MakeError(core::ErrorCode::NotFound,
                                   "LocalPlayerPositionSource::RemovePlayer: PlayerId not present");
        }
        m_players.erase(m_players.begin() + static_cast<std::ptrdiff_t>(index));
        return core::Success();
    }

    void LocalPlayerPositionSource::Clear() noexcept
    {
        m_players.clear();
    }
} // namespace stalkermp::world

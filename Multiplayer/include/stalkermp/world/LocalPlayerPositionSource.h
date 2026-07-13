// STALKER-MP — Local player position source (Sprint-004, §7.2; Design Review §3A)
//
// Engine-free, in-memory stub of IPlayerPositionSource for Sprint-004 and for
// testing. Holds simple PlayerPosition data; contains NO networking, session, or
// connection management (invariant B11). Replaced by a networked source in
// Sprint-006/007 behind the same interface, with the Bubble Manager unchanged.
//
// Single-writer: mutated only on the host simulation thread (B8). Positions are
// stored sorted ascending by PlayerId so ActivePlayers() is a deterministic
// snapshot without re-sorting (B9). ADR-007: no exceptions, Expected<T> for
// fallible mutators.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/world/BubbleTypes.h"
#include "stalkermp/world/IPlayerPositionSource.h"

namespace stalkermp::world
{
    class LocalPlayerPositionSource final : public IPlayerPositionSource
    {
    public:
        // Immutable snapshot in ascending PlayerId order (B9).
        [[nodiscard]] std::vector<PlayerPosition> ActivePlayers() const override;

        // --- Local host-side data feed (not networking; test/host use) -------

        // Insert or update a player's position. Rejects a null PlayerId (value 0).
        [[nodiscard]] core::Expected<void>
        SetPlayer(PlayerId id, const Vec3& position, std::uint64_t tick);

        // Remove a player. NotFound if absent.
        [[nodiscard]] core::Expected<void> RemovePlayer(PlayerId id);

        void Clear() noexcept;
        [[nodiscard]] std::size_t Count() const noexcept { return m_players.size(); }

    private:
        static constexpr std::size_t kInvalidIndex = static_cast<std::size_t>(-1);

        // Sorted ascending by PlayerId value; positions only (B11).
        std::vector<PlayerPosition> m_players;

        [[nodiscard]] std::size_t IndexOf(PlayerId id) const noexcept;
    };
} // namespace stalkermp::world

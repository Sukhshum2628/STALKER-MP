// STALKER-MP — Session (Sprint-006, Step 8)
//
// Host-authoritative membership registry: the admitted connections, keyed by
// ConnectionId (ascending; sorted-vector + hash accelerator, BC-005). Transient
// and reconstructable (never persisted). Fires join/leave observers exactly once.
// Reserved player/reconnect slots (null in Sprint-006). NO transport, no
// persistence, no reconnect logic. Engine-free / OS-free. ADR-007.

#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/net/ISessionObserver.h"
#include "stalkermp/net/NetTypes.h"

namespace stalkermp::net
{
    struct SessionMember
    {
        ConnectionId id{};
        std::uint64_t joinTick = 0;
        std::uint32_t playerSlot = 0;      // reserved (Sprint-007)
        std::uint64_t reconnectToken = 0;  // reserved (Sprint-012)
    };

    struct SessionReport
    {
        bool sortedUnique = true;
        bool acceleratorConsistent = true;
        bool withinCapacity = true;
        bool noZeroId = true;

        [[nodiscard]] bool IsHealthy() const noexcept
        {
            return sortedUnique && acceleratorConsistent && withinCapacity && noZeroId;
        }
    };

    class Session
    {
    public:
        explicit Session(std::size_t maxMembers) : m_maxMembers(maxMembers) {}

        // Register a non-owning observer (fired in registration order).
        void Subscribe(ISessionObserver* observer);

        // Admit a connection at handshake Established. Capacity-checked (call site
        // maps to CapacityFull); duplicate id -> AlreadyExists. Fires OnMemberJoined.
        [[nodiscard]] core::Expected<void> Admit(ConnectionId id, std::uint64_t joinTick);

        // Remove a member on disconnect. Fires OnMemberLeft exactly once with the
        // reason and a minted (opaque) reconnect token. Absent id -> no-op (no fire).
        void Remove(ConnectionId id, DisconnectReason reason);

        // Reconnect reclaim seam (Sprint-012). Sprint-006: always NotFound.
        [[nodiscard]] core::Expected<ConnectionId> TryReclaim(std::uint64_t reconnectToken);

        [[nodiscard]] bool Contains(ConnectionId id) const;
        [[nodiscard]] std::size_t Count() const noexcept { return m_members.size(); }
        [[nodiscard]] std::size_t Capacity() const noexcept { return m_maxMembers; }
        [[nodiscard]] const std::vector<SessionMember>& Members() const noexcept { return m_members; } // ascending

        [[nodiscard]] SessionReport ValidateConsistency() const;

    private:
        void RebuildAccelerator();
        [[nodiscard]] static std::uint64_t MintToken(ConnectionId id, std::uint64_t joinTick) noexcept;

        std::size_t m_maxMembers;
        std::vector<SessionMember> m_members;                // canonical, ascending, unique by id
        std::unordered_map<std::uint32_t, std::size_t> m_index; // id.value -> index
        std::vector<ISessionObserver*> m_observers;          // non-owning, registration order
    };
} // namespace stalkermp::net

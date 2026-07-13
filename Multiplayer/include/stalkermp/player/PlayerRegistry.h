// STALKER-MP — Player registry storage + lookups (Sprint-007, Steps 3–4)
//
// The canonical PlayerRecord store: a capacity-bounded sorted std::vector keyed
// ascending by PlayerId (Step 3), with strictly-ascending non-reused PlayerId
// allocation and transactional insert/retire, PLUS (Step 4) the four read-only
// secondary lookups over hash accelerators keyed to the ascending vector
// (BC-005) and a storage-level ValidateConsistency. No lifecycle, no session,
// no manager. Engine-free / OS-free. ADR-007: no exceptions, no RTTI, Expected<T>.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/net/NetTypes.h"       // net::ConnectionId
#include "stalkermp/player/PlayerTypes.h" // PlayerRecord, PlayerMappingView, PlayerId
#include "stalkermp/world/EntityView.h"   // world::EntityId

namespace stalkermp::player
{
    // Read-only storage consistency report (Step 4). Mirrors the Sprint-006
    // ConnectionTableReport precedent.
    struct PlayerRegistryConsistency
    {
        bool sortedUnique = true;          // canonical vector strictly ascending, unique PlayerIds
        bool acceleratorsConsistent = true;// every accelerator entry matches the canonical vector
        bool mappingBijective = true;      // each live secondary key maps to at most one record
        bool withinCapacity = true;        // size() <= capacity()
        bool noZeroId = true;              // no record with id == 0

        [[nodiscard]] bool IsHealthy() const noexcept
        {
            return sortedUnique && acceleratorsConsistent && mappingBijective && withinCapacity && noZeroId;
        }
    };

    class PlayerRegistry
    {
    public:
        explicit PlayerRegistry(std::size_t capacity) : m_capacity(capacity) {}

        // --- Step 3: allocation + storage ------------------------------------
        [[nodiscard]] PlayerId Allocate() noexcept;
        [[nodiscard]] core::Expected<void> Insert(const PlayerRecord& record);
        void Retire(PlayerId id) noexcept;

        [[nodiscard]] std::size_t size() const noexcept { return m_records.size(); }
        [[nodiscard]] std::size_t capacity() const noexcept { return m_capacity; }
        [[nodiscard]] const std::vector<PlayerRecord>& Records() const noexcept { return m_records; }

        // --- Step 4: read-only secondary lookups -----------------------------
        // Each resolves a secondary key to a PlayerMappingView via its accelerator
        // (O(1) amortized). A 0/none key resolves to nullopt (unmapped).
        [[nodiscard]] std::optional<PlayerMappingView> FindByPlayerId(PlayerId id) const;
        [[nodiscard]] std::optional<PlayerMappingView> FindByConnection(net::ConnectionId connection) const;
        [[nodiscard]] std::optional<PlayerMappingView> FindBySession(std::uint64_t sessionMember) const;
        [[nodiscard]] std::optional<PlayerMappingView> FindByEntity(world::EntityId entity) const;

        // Storage-level consistency (ordering, accelerator agreement, bijection).
        [[nodiscard]] PlayerRegistryConsistency ValidateConsistency() const;

    private:
        [[nodiscard]] std::size_t LowerBoundIndex(PlayerId id) const noexcept;

        // Rebuild all accelerators from the canonical vector (called after every
        // mutation; keeps index-valued accelerators consistent with m_records).
        void RebuildAccelerators();

        [[nodiscard]] static PlayerMappingView MakeView(const PlayerRecord& r) noexcept;

        std::vector<PlayerRecord> m_records; // canonical, ascending, unique by PlayerId
        std::size_t m_capacity;
        std::uint32_t m_nextId = 1; // ascending, non-reused; 0 reserved as "none"

        // Secondary accelerators -> index into m_records. 0/none keys are never
        // inserted (unmapped). Rebuilt on each mutation (BC-005).
        std::unordered_map<std::uint32_t, std::size_t> m_byPlayerId;
        std::unordered_map<std::uint32_t, std::size_t> m_byConnection;
        std::unordered_map<std::uint64_t, std::size_t> m_bySession;
        std::unordered_map<std::uint32_t, std::size_t> m_byEntity;
    };
} // namespace stalkermp::player

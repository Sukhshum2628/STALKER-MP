// STALKER-MP — Connection table (Sprint-006, Step 5)
//
// Capacity-bounded store of Connection records. Canonical sorted vector keyed
// ascending by ConnectionId + hash accelerator (BC-005). Host-assigned ascending
// ids, never reused within a session; on-demand resolution (callers hold
// ConnectionId, never a retained Connection* across ticks). NO transport,
// handshake, timeout, or queue logic. Engine-free / OS-free. ADR-007.

#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/net/Connection.h"

namespace stalkermp::net
{
    struct ConnectionTableReport
    {
        bool sortedUnique = true;          // canonical vector strictly ascending, unique ids
        bool acceleratorConsistent = true; // hash index matches the canonical vector
        bool withinCapacity = true;        // Count() <= maxConnections
        bool noZeroId = true;              // no id == 0

        [[nodiscard]] bool IsHealthy() const noexcept
        {
            return sortedUnique && acceleratorConsistent && withinCapacity && noZeroId;
        }
    };

    // Convert a millisecond duration to a tick count against a ms-per-tick frame
    // period (Step 7). Ceil, minimum 1 (a timeout can always eventually fire).
    [[nodiscard]] constexpr std::uint64_t TicksFromMs(const std::uint32_t ms, const std::uint32_t msPerTick) noexcept
    {
        if (msPerTick == 0)
        {
            return 1; // guard; callers supply a positive period
        }
        const std::uint64_t ticks = (static_cast<std::uint64_t>(ms) + msPerTick - 1) / msPerTick;
        return ticks == 0 ? 1u : ticks;
    }

    // Result of a unified disconnect. `performed` is false when the id was absent
    // (idempotent no-op). `endpoint`/`reason` feed the best-effort Bye the host
    // sends (Step 10).
    struct DisconnectIntent
    {
        bool performed = false;
        ConnectionId id{};
        TransportEndpoint endpoint{};
        DisconnectReason reason = DisconnectReason::None;
    };

    class ConnectionTable
    {
    public:
        explicit ConnectionTable(std::size_t maxConnections) : m_maxConnections(maxConnections) {}

        // Assign the next ascending, non-reused ConnectionId for a new endpoint.
        // Fails when the table is at capacity (call site maps to CapacityFull).
        [[nodiscard]] core::Expected<ConnectionId> Add(TransportEndpoint endpoint);

        // On-demand resolution; nullptr if absent. FindMutable is the only
        // mutation entry (state transitions are driven by later steps through it).
        [[nodiscard]] const Connection* Find(ConnectionId id) const;
        [[nodiscard]] Connection* FindMutable(ConnectionId id);

        void Remove(ConnectionId id);

        // Tick-derived timeout scan (Step 7), ascending ConnectionId. Flags:
        //  (a) handshake not complete and (currentTick - createdTick) > handshake budget;
        //  (b) Connected and (currentTick - lastRecvTick) > connection budget.
        // Comparison is strict '>'. Returns the flagged ids (caller disconnects
        // each with DisconnectReason::Timeout). Reads only; mutates nothing.
        [[nodiscard]] std::vector<ConnectionId> ScanTimeouts(std::uint64_t currentTick,
                                                             std::uint64_t handshakeTickBudget,
                                                             std::uint64_t connectionTickBudget) const;

        // Unified disconnect path (Step 7): set Disconnecting + record the reason,
        // capture the endpoint for a best-effort Bye, then remove the connection.
        // Idempotent: a second call (or an absent id) is a no-op (performed=false).
        [[nodiscard]] DisconnectIntent BeginDisconnect(ConnectionId id, DisconnectReason reason);

        [[nodiscard]] std::size_t Count() const noexcept { return m_entries.size(); }
        [[nodiscard]] std::size_t Capacity() const noexcept { return m_maxConnections; }
        [[nodiscard]] std::vector<ConnectionId> Ids() const; // ascending
        [[nodiscard]] const std::vector<Connection>& Entries() const noexcept { return m_entries; }

        [[nodiscard]] ConnectionTableReport ValidateConsistency() const;

    private:
        void RebuildAccelerator();

        std::size_t m_maxConnections;
        std::uint32_t m_nextId = 1; // monotonic; skips 0; never reused within a session
        std::vector<Connection> m_entries;                   // canonical, ascending, unique by id
        std::unordered_map<std::uint32_t, std::size_t> m_index; // id.value -> index
    };
} // namespace stalkermp::net

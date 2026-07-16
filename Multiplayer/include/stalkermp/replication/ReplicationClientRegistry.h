// STALKER-MP — Replication client registry (Sprint-009, Step 4)
//
// The engine-free per-client replication state store. It owns each client's
// identity, its Sprint-006 connection handle, its focus position (for interest,
// Step 5), and its last-acknowledged baseline (for delta, Step 6). Identity is
// allocated ascending and never reused; capacity is bounded by the configured
// maxClients; acknowledgement updates are monotonic; records are kept ascending
// by ClientId (deterministic).
//
// This step introduces the registry ONLY — no interest, delta, classifier,
// queue, packet, worker, manager, diagnostics, service/tick, thread, or wire
// behavior. It performs no networking; it stores the connection handle by value.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; Expected<T>.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/net/NetTypes.h"        // net::ConnectionId (value handle only)
#include "stalkermp/replication/ReplicationTypes.h"
#include "stalkermp/world/BubbleTypes.h"   // world::PlayerPosition

namespace stalkermp::replication
{
    // The canonical per-client replication record. Values only.
    struct ClientRecord
    {
        ClientId id{};                       // owning identity (0 = none); ascending, non-reused
        net::ConnectionId connection{};      // Sprint-006 link handle (value)
        world::PlayerPosition focus{};       // interest focus (positions only)
        std::uint64_t lastAckedReplicationId = 0; // baseline: last update the client acknowledged
        std::uint64_t lastAckedSnapshotTick = 0;  // provenance of that baseline
        bool active = true;                  // registered and eligible for replication
    };

    class ReplicationClientRegistry
    {
    public:
        // Capacity is bounded by the configured maxClients (>= 1 by config).
        explicit ReplicationClientRegistry(std::uint32_t maxClients) noexcept : m_maxClients(maxClients) {}

        // Register a new client for a connection with an initial focus. Allocates
        // the next ascending, non-reused ClientId. Fails (value outcome) when at
        // capacity or when the connection is already registered.
        [[nodiscard]] core::Expected<ClientId> Register(net::ConnectionId connection,
                                                        const world::PlayerPosition& focus);

        // Remove a client. The ClientId is never reused. Unknown id => value outcome.
        [[nodiscard]] core::Expected<void> Unregister(ClientId id);

        // Read-only lookups (nullptr when absent).
        [[nodiscard]] const ClientRecord* FindById(ClientId id) const noexcept;
        [[nodiscard]] const ClientRecord* FindByConnection(net::ConnectionId connection) const noexcept;

        // Update a registered client's interest focus. Unknown id => value outcome.
        [[nodiscard]] core::Expected<void> UpdateFocus(ClientId id, const world::PlayerPosition& focus);

        // Record a client acknowledgement. Monotonic: an ack older than or equal to
        // the current baseline is ignored (benign). Unknown id => value outcome.
        [[nodiscard]] core::Expected<void> RecordAck(ClientId id, std::uint64_t ackedReplicationId,
                                                     std::uint64_t ackedSnapshotTick);

        // Active clients, ascending by ClientId (deterministic; value copies).
        [[nodiscard]] std::vector<ClientRecord> ActiveClients() const;

        // Ordering + uniqueness + mapping integrity.
        [[nodiscard]] bool ValidateConsistency() const noexcept;

        [[nodiscard]] std::size_t Count() const noexcept { return m_records.size(); }
        [[nodiscard]] std::uint32_t Capacity() const noexcept { return m_maxClients; }
        [[nodiscard]] bool Full() const noexcept { return m_records.size() >= m_maxClients; }

    private:
        [[nodiscard]] std::size_t IndexOf(ClientId id) const noexcept;

        std::uint32_t m_maxClients;
        std::uint64_t m_nextClientId = 1;    // ascending, never reused (never 0)
        std::vector<ClientRecord> m_records; // kept ascending by ClientId
    };
} // namespace stalkermp::replication

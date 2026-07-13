// STALKER-MP — Fragmentation / reassembly (Sprint-006, Step 9)
//
// Splits a large message into indexed fragments (carried on the reliable channel,
// guarded by kFlagFragment) and reassembles them per (connection, messageId),
// bounded and time-boxed by reassemblyBudgetTicks. NO transport, no host loop.
// Engine-free / OS-free. ADR-007/010.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/net/ByteCursor.h"
#include "stalkermp/net/NetTypes.h" // MessageId, ConnectionId

namespace stalkermp::net
{
    // A single fragment: the sub-header (messageId, index, count) + its bytes.
    struct Fragment
    {
        MessageId messageId{};
        std::uint16_t index = 0;
        std::uint16_t count = 0;
        std::vector<std::uint8_t> bytes;
    };

    struct CompletedMessage
    {
        MessageId messageId{};
        std::vector<std::uint8_t> bytes;
    };

    enum class AcceptResult : std::uint8_t
    {
        Incomplete = 0, // stored; more fragments needed
        Completed,      // all fragments present; `out` filled
        Rejected,       // invalid (index >= count, or count mismatch)
    };

    // Fragment sub-header codec (u16 messageId, u16 index, u16 count). The fragment
    // bytes follow the sub-header on the wire.
    [[nodiscard]] core::Expected<void> SerializeFragmentHeader(const Fragment& fragment, ByteWriter& writer);
    [[nodiscard]] core::Expected<Fragment> DeserializeFragmentHeader(ByteReader& reader);

    // Sub-header wire size (bytes).
    inline constexpr std::size_t kFragmentHeaderWireSize = 6;

    class Fragmentation
    {
    public:
        Fragmentation(std::size_t maxFragmentPayload, std::uint64_t reassemblyBudgetTicks)
            : m_maxFragmentPayload(maxFragmentPayload == 0 ? 1 : maxFragmentPayload)
            , m_budgetTicks(reassemblyBudgetTicks)
        {
        }

        // Split a payload into ceil(size / maxFragmentPayload) fragments (>= 1).
        [[nodiscard]] std::vector<Fragment> Split(MessageId messageId, const std::vector<std::uint8_t>& payload) const;

        // Accept a fragment for a connection. Completed -> `out` filled and the
        // reassembly reclaimed. Rejected -> index >= count or count mismatch.
        [[nodiscard]] AcceptResult Accept(ConnectionId connection, const Fragment& fragment,
                                          std::uint64_t currentTick, CompletedMessage& out);

        // Drop reassemblies older than the budget; returns the number dropped.
        std::size_t PruneExpired(std::uint64_t currentTick);

        [[nodiscard]] std::size_t PendingCount() const noexcept { return m_pending.size(); }
        [[nodiscard]] std::uint64_t DroppedIncomplete() const noexcept { return m_droppedIncomplete; }
        void Clear() { m_pending.clear(); }

        // True when no pending reassembly is older than the budget.
        [[nodiscard]] bool ValidateConsistency(std::uint64_t currentTick) const;

    private:
        struct Reassembly
        {
            std::uint16_t count = 0;
            std::uint64_t firstTick = 0;
            std::size_t have = 0;
            std::vector<std::optional<std::vector<std::uint8_t>>> parts;
        };

        [[nodiscard]] static std::uint64_t Key(ConnectionId connection, MessageId messageId) noexcept
        {
            return (static_cast<std::uint64_t>(connection.value) << 16) | messageId.value;
        }

        std::size_t m_maxFragmentPayload;
        std::uint64_t m_budgetTicks;
        std::uint64_t m_droppedIncomplete = 0;
        std::unordered_map<std::uint64_t, Reassembly> m_pending;
    };
} // namespace stalkermp::net

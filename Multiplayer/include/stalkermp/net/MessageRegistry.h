// STALKER-MP — Message registry (Sprint-006, Step 4)
//
// Deterministic MessageId -> handler routing for the CONTROL range. Engine-free /
// OS-free. No transport, connection, session, or wiring. ADR-007 (no exceptions,
// core::Expected); ADR-010 (id ranges; additive ids; id-ordered dispatch).

#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/net/ByteCursor.h"        // net::ByteReader
#include "stalkermp/net/NetTypes.h"          // net::MessageId, net::ConnectionId

namespace stalkermp::net
{
    // Per-message dispatch context. Carries the source connection + tick ONLY —
    // no simulation handle (networking never owns simulation).
    struct DispatchContext
    {
        ConnectionId source{};
        std::uint64_t tick = 0;
    };

    enum class DispatchResult : std::uint8_t
    {
        Handled = 0,
        Unknown,   // no handler registered for the id (benign, counted)
        Failed,    // handler returned an error
    };

    [[nodiscard]] constexpr const char* DispatchResultName(const DispatchResult r) noexcept
    {
        switch (r)
        {
        case DispatchResult::Handled: return "Handled";
        case DispatchResult::Unknown: return "Unknown";
        case DispatchResult::Failed:  return "Failed";
        }
        return "Unknown";
    }

    // Engine-free handler seam. Implementations parse the payload (later steps);
    // Step-4 handlers are inert stubs returning Success().
    class IMessageHandler
    {
    public:
        virtual ~IMessageHandler() = default;
        [[nodiscard]] virtual core::Expected<void> Handle(ByteReader& payload, DispatchContext& context) = 0;
    };

    struct MessageRegistryReport
    {
        bool sortedUnique = true;          // canonical vector strictly ascending, unique ids
        bool acceleratorConsistent = true; // hash index matches the canonical vector

        [[nodiscard]] bool IsHealthy() const noexcept { return sortedUnique && acceleratorConsistent; }
    };

    class MessageRegistry
    {
    public:
        struct Entry
        {
            MessageId id{};
            IMessageHandler* handler = nullptr; // non-owning
        };

        MessageRegistry() = default;

        // Register a CONTROL-range id (Sprint-006). Non-control id -> InvalidArgument;
        // duplicate id -> AlreadyExists.
        [[nodiscard]] core::Expected<void> Register(MessageId id, IMessageHandler* handler);

        // Register a DATA-range id (seam for later sprints). Non-data id ->
        // InvalidArgument; duplicate id -> AlreadyExists.
        [[nodiscard]] core::Expected<void> RegisterData(MessageId id, IMessageHandler* handler);

        // Route a message to its handler. Unknown id -> Unknown (counted, benign).
        [[nodiscard]] DispatchResult Dispatch(MessageId id, ByteReader& payload, DispatchContext& context);

        [[nodiscard]] bool Contains(MessageId id) const;
        [[nodiscard]] std::size_t Count() const noexcept { return m_entries.size(); }
        [[nodiscard]] std::uint64_t UnknownCount() const noexcept { return m_unknownCount; }
        [[nodiscard]] std::vector<MessageId> Ids() const; // ascending
        [[nodiscard]] const std::vector<Entry>& Entries() const noexcept { return m_entries; }

        void Clear();

        [[nodiscard]] MessageRegistryReport ValidateConsistency() const;

    private:
        [[nodiscard]] core::Expected<void> Insert(MessageId id, IMessageHandler* handler);
        void RebuildAccelerator();

        std::vector<Entry> m_entries;                        // canonical, ascending, unique
        std::unordered_map<std::uint16_t, std::size_t> m_index; // id.value -> index
        std::uint64_t m_unknownCount = 0;
    };
} // namespace stalkermp::net

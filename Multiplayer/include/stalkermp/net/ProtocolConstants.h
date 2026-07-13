// STALKER-MP — Networking protocol constants (Sprint-006, Step 1)
//
// Engine-free / OS-free wire and protocol constants (ADR-010). Constants only —
// no serialization (Step 2), no registration (Step 4), no handling (Steps 6-10).

#pragma once

#include <cstdint>

#include "stalkermp/net/NetTypes.h" // MessageId

namespace stalkermp::net
{
    // Fixed protocol sentinel (first header field). Distinctive non-zero value.
    inline constexpr std::uint16_t kProtocolMagic = 0x5350; // 'S''P' (STALKer-mP)

    // Wire protocol version (ADR-010). Monotonically increasing; Sprint-006
    // compatibility rule is EXACT-MATCH (no negotiation).
    inline constexpr std::uint16_t kProtocolVersion = 1;

    // Message-id partitioning (ADR-010). Control range reserved to Sprint-006;
    // data range reserved for later gameplay sprints.
    inline constexpr MessageId kControlIdMin{0x0000};
    inline constexpr MessageId kControlIdMax{0x00FF};
    inline constexpr MessageId kDataIdMin{0x0100};

    // Named control message ids (defined here; registered/handled in later steps).
    inline constexpr MessageId kMsgHello{0x0001};
    inline constexpr MessageId kMsgChallenge{0x0002};
    inline constexpr MessageId kMsgResponse{0x0003};
    inline constexpr MessageId kMsgEstablished{0x0004};
    inline constexpr MessageId kMsgPing{0x0005};
    inline constexpr MessageId kMsgPong{0x0006};
    inline constexpr MessageId kMsgBye{0x0007};

    [[nodiscard]] constexpr bool IsControlId(const MessageId id) noexcept
    {
        return id.value <= kControlIdMax.value; // [0x0000 .. 0x00FF]
    }

    [[nodiscard]] constexpr bool IsDataId(const MessageId id) noexcept
    {
        return id.value >= kDataIdMin.value; // [0x0100 .. ]
    }
} // namespace stalkermp::net

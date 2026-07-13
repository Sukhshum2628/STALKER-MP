// STALKER-MP — Player request message ids (Sprint-007, Step 11)
//
// Single-source-of-truth for the Sprint-007 player-request message-id block
// 0x0100–0x010F (frozen Player Request Message-ID Allocation). These are additive
// DATA-range ids (ADR-010: additive, never renumbered); Sprint-006
// ProtocolConstants.h is NOT modified. Client -> host REQUESTS only; the host owns
// every resulting transition (Host Authority).
//
// Used by Sprint-007:
//   kMsgPlayerJoinRequest    = 0x0100
//   kMsgPlayerRespawnRequest = 0x0101
// Reserved (0x0102–0x010F): not registered (see the frozen allocation).

#pragma once

#include "stalkermp/net/NetTypes.h" // net::MessageId

namespace stalkermp::player
{
    inline constexpr net::MessageId kMsgPlayerJoinRequest{0x0100};
    inline constexpr net::MessageId kMsgPlayerRespawnRequest{0x0101};
} // namespace stalkermp::player

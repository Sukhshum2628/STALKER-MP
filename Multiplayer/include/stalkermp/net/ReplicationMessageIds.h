// STALKER-MP — Replication message ids (Sprint-009, Step 9)
//
// Single-source-of-truth for the Sprint-009 replication message-id block
// 0x0200-0x020F. These are ADDITIVE DATA-range ids (ADR-010: additive, never
// renumbered); Sprint-006 ProtocolConstants.h and the Sprint-007 player block
// (0x0100-0x010F) are NOT modified. The update is host -> client (authoritative
// state); the ack is client -> host (baseline advance only; never mutates
// simulation — Host Authority).
//
//   kMsgReplicationUpdate = 0x0200   (host -> client)
//   kMsgReplicationAck    = 0x0201   (client -> host)
// Reserved (0x0202-0x020F): not registered.

#pragma once

#include "stalkermp/net/NetTypes.h" // net::MessageId

namespace stalkermp::net
{
    inline constexpr net::MessageId kMsgReplicationUpdate{0x0200};
    inline constexpr net::MessageId kMsgReplicationAck{0x0201};
} // namespace stalkermp::net

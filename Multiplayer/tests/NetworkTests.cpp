// STALKER-MP — Networking tests (Sprint-006)
//
// Step 1: value types, enums, protocol constants, config parsing.
// Step 2: wire codec (ByteWriter/ByteReader + PacketHeader serialize/deserialize/
//         validate), including explicit expected-byte arrays (E-G2-N).
// Engine-free and OS-free.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "stalkermp/adapters/PlatformTransport.h"
#include "stalkermp/core/Config.h"
#include "stalkermp/net/ByteCursor.h"
#include "stalkermp/net/Connection.h"
#include "stalkermp/net/ConnectionTable.h"
#include "stalkermp/net/Fragmentation.h"
#include "stalkermp/net/Handshake.h"
#include "stalkermp/net/HostServer.h"
#include "stalkermp/net/PacketQueues.h"
#include "stalkermp/net/Reliability.h"
#include "stalkermp/net/IConnectionAuthenticator.h"
#include "stalkermp/net/ITransport.h"
#include "stalkermp/net/LoopbackTransport.h"
#include "stalkermp/net/MessageRegistry.h"
#include "stalkermp/net/MessageRegistryService.h"
#include "stalkermp/net/MockTransport.h"
#include "stalkermp/net/NetworkDiagnostics.h"
#include "stalkermp/net/NetworkingService.h"
#include "stalkermp/net/TransportService.h"
#include "stalkermp/net/ISessionObserver.h"
#include "stalkermp/net/NetTypes.h"
#include "stalkermp/net/Session.h"
#include "stalkermp/net/SessionService.h"
#include "stalkermp/net/NetworkingConfig.h"
#include "stalkermp/net/PacketHeader.h"
#include "stalkermp/net/ProtocolConstants.h"
#include "stalkermp/net/TransportConfig.h"

using namespace stalkermp;
using namespace stalkermp::net;

// ============================================================================
// Step 1 — value types, enums, protocol constants, config parsing.
// ============================================================================

TEST(NetTypesStep1, IdentifierDefaultsAndEquality)
{
    EXPECT_EQ(ConnectionId{}.value, 0u);
    EXPECT_TRUE(ConnectionId{}.IsNone());
    EXPECT_EQ(MessageId{}.value, 0u);
    EXPECT_FALSE(TransportEndpoint{}.IsValid());

    EXPECT_EQ((ConnectionId{5}), (ConnectionId{5}));
    EXPECT_NE((ConnectionId{5}), (ConnectionId{6}));
    EXPECT_EQ((MessageId{7}), (MessageId{7}));
    EXPECT_NE((MessageId{7}), (MessageId{8}));
    EXPECT_TRUE((TransportEndpoint{1}).IsValid());
}

TEST(NetTypesStep1, IdentifiersSortAscendingByValue)
{
    std::vector<ConnectionId> ids{{9}, {2}, {5}};
    std::sort(ids.begin(), ids.end(), [](ConnectionId a, ConnectionId b) { return a.value < b.value; });
    EXPECT_EQ(ids[0].value, 2u);
    EXPECT_EQ(ids[1].value, 5u);
    EXPECT_EQ(ids[2].value, 9u);
}

TEST(NetTypesStep1, TriviallyCopyable)
{
    static_assert(std::is_trivially_copyable<ConnectionId>::value, "ConnectionId");
    static_assert(std::is_trivially_copyable<MessageId>::value, "MessageId");
    static_assert(std::is_trivially_copyable<TransportEndpoint>::value, "TransportEndpoint");
    static_assert(std::is_trivially_copyable<PacketHeader>::value, "PacketHeader");
    static_assert(kPacketHeaderWireSize == 18, "wire size");
    SUCCEED();
}

TEST(NetTypesStep1, EnumDefaultsAreZeroNeutral)
{
    EXPECT_EQ(static_cast<std::uint8_t>(Channel::Reliable), 0u);
    EXPECT_EQ(static_cast<std::uint8_t>(HandshakeState::None), 0u);
    EXPECT_EQ(static_cast<std::uint8_t>(ConnectionState::Disconnected), 0u);
    EXPECT_EQ(static_cast<std::uint8_t>(DisconnectReason::None), 0u);
    EXPECT_EQ(static_cast<std::uint8_t>(TransportOutcome::Ok), 0u);
}

TEST(NetTypesStep1, EnumNames)
{
    EXPECT_STREQ(ChannelName(Channel::Reliable), "Reliable");
    EXPECT_STREQ(ChannelName(Channel::Unreliable), "Unreliable");
    EXPECT_STREQ(HandshakeStateName(HandshakeState::Established), "Established");
    EXPECT_STREQ(ConnectionStateName(ConnectionState::Connected), "Connected");
    EXPECT_STREQ(DisconnectReasonName(DisconnectReason::Timeout), "Timeout");
    EXPECT_STREQ(TransportOutcomeName(TransportOutcome::EndpointMissing), "EndpointMissing");
    EXPECT_STREQ(ChannelName(static_cast<Channel>(200)), "Unknown");
}

TEST(NetTypesStep1, ProtocolConstantsAndIdRanges)
{
    EXPECT_EQ(kProtocolVersion, 1u);
    EXPECT_NE(kProtocolMagic, 0u);
    EXPECT_EQ(kControlIdMin.value, 0x0000u);
    EXPECT_EQ(kControlIdMax.value, 0x00FFu);
    EXPECT_EQ(kDataIdMin.value, 0x0100u);

    EXPECT_TRUE(IsControlId(MessageId{0x00FF}));
    EXPECT_FALSE(IsControlId(MessageId{0x0100}));
    EXPECT_TRUE(IsDataId(MessageId{0x0100}));
    EXPECT_FALSE(IsDataId(MessageId{0x00FF}));

    for (const MessageId id : {kMsgHello, kMsgChallenge, kMsgResponse, kMsgEstablished, kMsgPing, kMsgPong, kMsgBye})
    {
        EXPECT_TRUE(IsControlId(id));
        EXPECT_FALSE(IsDataId(id));
    }
}

TEST(NetTypesStep1, PacketHeaderFlagBitsAreDistinct)
{
    EXPECT_EQ(kFlagFragment, 0x01u);
    EXPECT_EQ(kFlagAck, 0x02u);
    EXPECT_EQ(kFlagControl, 0x04u);
    EXPECT_EQ(kDefinedFlagsMask, 0x07u);
    PacketHeader h;
    EXPECT_EQ(h.magic, 0u);
    EXPECT_EQ(h.flags, 0u);
}

// --- NetworkingConfig ---------------------------------------------------------
TEST(NetworkingConfigStep1, MissingSectionDefaults)
{
    core::ConfigStore store;
    const auto r = NetworkingConfig::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const NetworkingConfig c = r.Value();
    EXPECT_FALSE(c.enabled);
    EXPECT_EQ(c.maxConnections, 16u);
    EXPECT_EQ(c.connectionTimeoutMs, 15000u);
    EXPECT_EQ(c.handshakeTimeoutMs, 5000u);
    EXPECT_EQ(c.heartbeatIntervalMs, 2000u);
    EXPECT_EQ(c.debugFlags, 0u);
}

TEST(NetworkingConfigStep1, ValidOverrides)
{
    core::ConfigStore store;
    store.Set("networking", "enabled", "true");
    store.Set("networking", "max_connections", "64");
    store.Set("networking", "connection_timeout_ms", "20000");
    store.Set("networking", "handshake_timeout_ms", "3000");
    store.Set("networking", "heartbeat_interval_ms", "1000");
    const auto r = NetworkingConfig::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const NetworkingConfig c = r.Value();
    EXPECT_TRUE(c.enabled);
    EXPECT_EQ(c.maxConnections, 64u);
    EXPECT_EQ(c.connectionTimeoutMs, 20000u);
    EXPECT_EQ(c.handshakeTimeoutMs, 3000u);
    EXPECT_EQ(c.heartbeatIntervalMs, 1000u);
}

TEST(NetworkingConfigStep1, NegativeCases)
{
    auto fails = [](const char* key, const char* val) {
        core::ConfigStore store;
        store.Set("networking", key, val);
        return !NetworkingConfig::FromConfig(store).HasValue();
    };
    EXPECT_TRUE(fails("max_connections", "0"));
    EXPECT_TRUE(fails("max_connections", "5000"));
    EXPECT_TRUE(fails("connection_timeout_ms", "500"));
    EXPECT_TRUE(fails("handshake_timeout_ms", "0"));

    // Cross-field: handshake > connection.
    {
        core::ConfigStore store;
        store.Set("networking", "connection_timeout_ms", "2000");
        store.Set("networking", "handshake_timeout_ms", "3000");
        EXPECT_FALSE(NetworkingConfig::FromConfig(store).HasValue());
    }
    // Cross-field: heartbeat >= connection.
    {
        core::ConfigStore store;
        store.Set("networking", "connection_timeout_ms", "2000");
        store.Set("networking", "heartbeat_interval_ms", "2000");
        EXPECT_FALSE(NetworkingConfig::FromConfig(store).HasValue());
    }
}

TEST(NetworkingConfigStep1, Boundaries)
{
    auto ok = [](const char* key, const char* val) {
        core::ConfigStore store;
        store.Set("networking", key, val);
        return NetworkingConfig::FromConfig(store).HasValue();
    };
    EXPECT_TRUE(ok("max_connections", "1"));
    EXPECT_TRUE(ok("max_connections", "4096"));
    core::ConfigStore store;
    store.Set("networking", "connection_timeout_ms", "1000");
    store.Set("networking", "handshake_timeout_ms", "500");
    store.Set("networking", "heartbeat_interval_ms", "100");
    EXPECT_TRUE(NetworkingConfig::FromConfig(store).HasValue());
}

// --- TransportConfig ----------------------------------------------------------
TEST(TransportConfigStep1, MissingSectionDefaults)
{
    core::ConfigStore store;
    const auto r = TransportConfig::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const TransportConfig c = r.Value();
    EXPECT_EQ(c.listenPort, 27015u);
    EXPECT_EQ(c.bindAddress, "0.0.0.0");
    EXPECT_EQ(c.mtuPayloadBytes, 1200u);
    EXPECT_EQ(c.maxBytesPerTick, 262144u);
    EXPECT_EQ(c.reliableWindow, 256u);
}

TEST(TransportConfigStep1, NegativeCases)
{
    auto fails = [](const char* key, const char* val) {
        core::ConfigStore store;
        store.Set("transport", key, val);
        return !TransportConfig::FromConfig(store).HasValue();
    };
    EXPECT_TRUE(fails("listen_port", "0"));
    EXPECT_TRUE(fails("bind_address", ""));
    EXPECT_TRUE(fails("mtu_payload_bytes", "100"));  // <= header size / below min
    EXPECT_TRUE(fails("max_outgoing_queue", "0"));
    EXPECT_TRUE(fails("reliable_window", "0"));
    EXPECT_TRUE(fails("reassembly_budget_ticks", "0"));

    // Cross-field: max_bytes_per_tick < mtu.
    core::ConfigStore store;
    store.Set("transport", "mtu_payload_bytes", "1200");
    store.Set("transport", "max_bytes_per_tick", "500");
    EXPECT_FALSE(TransportConfig::FromConfig(store).HasValue());
}

TEST(TransportConfigStep1, Boundaries)
{
    auto ok = [](const char* key, const char* val) {
        core::ConfigStore store;
        store.Set("transport", key, val);
        return TransportConfig::FromConfig(store).HasValue();
    };
    EXPECT_TRUE(ok("listen_port", "65535"));
    EXPECT_TRUE(ok("mtu_payload_bytes", "256"));
    EXPECT_TRUE(ok("mtu_payload_bytes", "9000"));

    core::ConfigStore store; // max_bytes_per_tick == mtu accepted
    store.Set("transport", "mtu_payload_bytes", "1200");
    store.Set("transport", "max_bytes_per_tick", "1200");
    EXPECT_TRUE(TransportConfig::FromConfig(store).HasValue());
}

TEST(ConfigStep1, SectionsAreIndependent)
{
    core::ConfigStore store;
    store.Set("transport", "listen_port", "0"); // invalid transport
    EXPECT_TRUE(NetworkingConfig::FromConfig(store).HasValue()); // networking unaffected
    EXPECT_FALSE(TransportConfig::FromConfig(store).HasValue());
}

// ============================================================================
// Step 2 — wire codec.
// ============================================================================

TEST(ByteCursorStep2, WriteReadLittleEndian)
{
    std::array<std::uint8_t, 8> buf{};
    ByteWriter w(buf.data(), buf.size());
    ASSERT_TRUE(w.WriteU16(0x0102).HasValue());
    ASSERT_TRUE(w.WriteU32(0x01020304).HasValue());
    ASSERT_TRUE(w.WriteU8(0xAB).HasValue());
    EXPECT_EQ(w.BytesWritten(), 7u);
    // Little-endian byte layout.
    EXPECT_EQ(buf[0], 0x02); EXPECT_EQ(buf[1], 0x01);
    EXPECT_EQ(buf[2], 0x04); EXPECT_EQ(buf[3], 0x03); EXPECT_EQ(buf[4], 0x02); EXPECT_EQ(buf[5], 0x01);
    EXPECT_EQ(buf[6], 0xAB);

    ByteReader r(buf.data(), 7);
    EXPECT_EQ(r.ReadU16().Value(), 0x0102u);
    EXPECT_EQ(r.ReadU32().Value(), 0x01020304u);
    EXPECT_EQ(r.ReadU8().Value(), 0xABu);
    EXPECT_EQ(r.BytesRead(), 7u);
}

TEST(ByteCursorStep2, OverflowUnderflowLeavePositionUnchanged)
{
    std::array<std::uint8_t, 1> buf{};
    ByteWriter w(buf.data(), buf.size());
    EXPECT_FALSE(w.WriteU16(0x1234).HasValue()); // needs 2, has 1
    EXPECT_EQ(w.BytesWritten(), 0u);

    ByteReader r(buf.data(), 1);
    EXPECT_FALSE(r.ReadU16().HasValue());
    EXPECT_EQ(r.BytesRead(), 0u);
    EXPECT_TRUE(r.ReadU8().HasValue()); // 1 byte ok
}

TEST(ByteCursorStep2, TriviallyCopyable)
{
    static_assert(std::is_trivially_copyable<ByteWriter>::value, "ByteWriter");
    static_assert(std::is_trivially_copyable<ByteReader>::value, "ByteReader");
    SUCCEED();
}

namespace
{
    PacketHeader SampleHeader()
    {
        PacketHeader h;
        h.magic = kProtocolMagic;
        h.protocolVersion = kProtocolVersion;
        h.channel = static_cast<std::uint8_t>(Channel::Unreliable);
        h.flags = kFlagControl;
        h.sequence = 0x1122;
        h.ack = 0x3344;
        h.ackBits = 0x55667788;
        h.messageId = kMsgPing.value;
        h.payloadLength = 0x0009;
        return h;
    }
}

TEST(PacketHeaderStep2, RoundTripFieldByField)
{
    std::array<std::uint8_t, kPacketHeaderWireSize> buf{};
    ByteWriter w(buf.data(), buf.size());
    const PacketHeader in = SampleHeader();
    ASSERT_TRUE(SerializeHeader(in, w).HasValue());
    EXPECT_EQ(w.BytesWritten(), kPacketHeaderWireSize);

    ByteReader r(buf.data(), buf.size());
    const auto out = DeserializeHeader(r);
    ASSERT_TRUE(out.HasValue());
    const PacketHeader h = out.Value();
    EXPECT_EQ(h.magic, in.magic);
    EXPECT_EQ(h.protocolVersion, in.protocolVersion);
    EXPECT_EQ(h.channel, in.channel);
    EXPECT_EQ(h.flags, in.flags);
    EXPECT_EQ(h.sequence, in.sequence);
    EXPECT_EQ(h.ack, in.ack);
    EXPECT_EQ(h.ackBits, in.ackBits);
    EXPECT_EQ(h.messageId, in.messageId);
    EXPECT_EQ(h.payloadLength, in.payloadLength);
    EXPECT_EQ(r.BytesRead(), kPacketHeaderWireSize);
}

// Explicit expected-byte array — the cross-toolchain determinism anchor (E-G2-N).
TEST(PacketHeaderStep2, ExplicitWireBytes)
{
    std::array<std::uint8_t, kPacketHeaderWireSize> buf{};
    ByteWriter w(buf.data(), buf.size());
    ASSERT_TRUE(SerializeHeader(SampleHeader(), w).HasValue());

    const std::array<std::uint8_t, kPacketHeaderWireSize> expected{
        0x50, 0x53,             // magic 0x5350 LE
        0x01, 0x00,             // version 1 LE
        0x01,                   // channel Unreliable
        0x04,                   // flags kFlagControl
        0x22, 0x11,             // sequence 0x1122 LE
        0x44, 0x33,             // ack 0x3344 LE
        0x88, 0x77, 0x66, 0x55, // ackBits 0x55667788 LE
        0x05, 0x00,             // messageId 0x0005 LE
        0x09, 0x00,             // payloadLength 0x0009 LE
    };
    EXPECT_EQ(buf, expected);
}

TEST(PacketHeaderStep2, WireSizeBoundaries)
{
    // Exactly 18 bytes serializes; 17 fails without OOB write.
    std::array<std::uint8_t, 17> small{};
    ByteWriter w(small.data(), small.size());
    EXPECT_FALSE(SerializeHeader(SampleHeader(), w).HasValue());

    // Deserialize from 17 bytes => Truncated.
    std::array<std::uint8_t, kPacketHeaderWireSize> full{};
    ByteWriter w2(full.data(), full.size());
    ASSERT_TRUE(SerializeHeader(SampleHeader(), w2).HasValue());
    ByteReader r(full.data(), 17);
    EXPECT_FALSE(DeserializeHeader(r).HasValue());
}

TEST(PacketHeaderStep2, ValidationFailurePaths)
{
    auto serialize = [](const PacketHeader& h, std::array<std::uint8_t, kPacketHeaderWireSize>& buf) {
        ByteWriter w(buf.data(), buf.size());
        return SerializeHeader(h, w).HasValue();
    };
    auto deser = [](std::array<std::uint8_t, kPacketHeaderWireSize>& buf) {
        ByteReader r(buf.data(), buf.size());
        return DeserializeHeader(r);
    };

    { PacketHeader h = SampleHeader(); h.magic = 0x0000; std::array<std::uint8_t, kPacketHeaderWireSize> b{}; ASSERT_TRUE(serialize(h, b)); EXPECT_FALSE(deser(b).HasValue()); }        // BadMagic
    { PacketHeader h = SampleHeader(); h.protocolVersion = kProtocolVersion + 1; std::array<std::uint8_t, kPacketHeaderWireSize> b{}; ASSERT_TRUE(serialize(h, b)); EXPECT_FALSE(deser(b).HasValue()); } // VersionMismatch
    { PacketHeader h = SampleHeader(); h.channel = 2; std::array<std::uint8_t, kPacketHeaderWireSize> b{}; ASSERT_TRUE(serialize(h, b)); EXPECT_FALSE(deser(b).HasValue()); }             // BadChannel
    { PacketHeader h = SampleHeader(); h.flags = 0x80; std::array<std::uint8_t, kPacketHeaderWireSize> b{}; ASSERT_TRUE(serialize(h, b)); EXPECT_FALSE(deser(b).HasValue()); }             // ReservedFlag
}

TEST(PacketHeaderStep2, ValidateHeaderPayloadLength)
{
    PacketHeader h = SampleHeader();
    h.payloadLength = 10;
    EXPECT_TRUE(ValidateHeader(h, 10).HasValue());
    EXPECT_TRUE(ValidateHeader(h, 20).HasValue());
    EXPECT_FALSE(ValidateHeader(h, 9).HasValue()); // BadLength
}

TEST(PacketHeaderStep2, DeterministicSerialization)
{
    std::array<std::uint8_t, kPacketHeaderWireSize> a{};
    std::array<std::uint8_t, kPacketHeaderWireSize> b{};
    ByteWriter wa(a.data(), a.size());
    ByteWriter wb(b.data(), b.size());
    ASSERT_TRUE(SerializeHeader(SampleHeader(), wa).HasValue());
    ASSERT_TRUE(SerializeHeader(SampleHeader(), wb).HasValue());
    EXPECT_EQ(a, b);
}

// ============================================================================
// Step 3 — ITransport seam + MockTransport + LoopbackTransport.
// ============================================================================

namespace
{
    ConstByteSpan Span(const std::vector<std::uint8_t>& v)
    {
        return ConstByteSpan{v.data(), v.size()};
    }
}

// --- Mock: scripted inbound delivered on Poll; Send captured; Accepted drains. -
TEST(MockTransportStep3, InboundSendAndAccepted)
{
    MockTransport t;
    t.RegisterEndpoint(TransportEndpoint{7});
    ASSERT_TRUE(t.Bind(0, "0.0.0.0").HasValue());

    // Inbound appears after Poll.
    t.InjectInbound(TransportEndpoint{7}, Channel::Reliable, {1, 2, 3});
    EXPECT_TRUE(t.Received().empty()); // not polled yet
    t.Poll(1024);
    auto rx = t.Received();
    ASSERT_EQ(rx.size(), 1u);
    EXPECT_EQ(rx[0].from, (TransportEndpoint{7}));
    EXPECT_EQ(rx[0].channel, Channel::Reliable);
    EXPECT_EQ(rx[0].bytes, (std::vector<std::uint8_t>{1, 2, 3}));
    EXPECT_TRUE(t.Received().empty()); // drained once

    // Send captured with correct endpoint/channel/bytes.
    const std::vector<std::uint8_t> payload{9, 8, 7};
    EXPECT_EQ(t.Send(TransportEndpoint{7}, Channel::Unreliable, Span(payload)), TransportOutcome::Queued);
    ASSERT_EQ(t.Outgoing().size(), 1u);
    EXPECT_EQ(t.Outgoing()[0].to, (TransportEndpoint{7}));
    EXPECT_EQ(t.Outgoing()[0].channel, Channel::Unreliable);
    EXPECT_EQ(t.Outgoing()[0].bytes, payload);

    // Accepted drains injected accepts once.
    t.InjectAccept(TransportEndpoint{42});
    auto acc = t.Accepted();
    ASSERT_EQ(acc.size(), 1u);
    EXPECT_EQ(acc[0], (TransportEndpoint{42}));
    EXPECT_TRUE(t.Accepted().empty());
}

// --- Knobs: drop / duplicate / corrupt / reorder (deterministic). --------------
TEST(MockTransportStep3, Knobs)
{
    const std::vector<std::uint8_t> p{0x10, 0x20};

    // dropNextN drops exactly N.
    {
        MockTransport t; t.RegisterEndpoint(TransportEndpoint{1}); ASSERT_TRUE(t.Bind(0, "a").HasValue());
        t.dropNextN = 2;
        (void)t.Send(TransportEndpoint{1}, Channel::Reliable, Span(p));
        (void)t.Send(TransportEndpoint{1}, Channel::Reliable, Span(p));
        (void)t.Send(TransportEndpoint{1}, Channel::Reliable, Span(p));
        EXPECT_EQ(t.Outgoing().size(), 1u); // 2 dropped, 1 captured
    }
    // duplicateNext yields two copies.
    {
        MockTransport t; t.RegisterEndpoint(TransportEndpoint{1}); ASSERT_TRUE(t.Bind(0, "a").HasValue());
        t.duplicateNext = true;
        (void)t.Send(TransportEndpoint{1}, Channel::Reliable, Span(p));
        ASSERT_EQ(t.Outgoing().size(), 2u);
        EXPECT_EQ(t.Outgoing()[0].bytes, p);
        EXPECT_EQ(t.Outgoing()[1].bytes, p);
    }
    // corruptNext flips byte[0] (still delivered).
    {
        MockTransport t; t.RegisterEndpoint(TransportEndpoint{1}); ASSERT_TRUE(t.Bind(0, "a").HasValue());
        t.corruptNext = true;
        (void)t.Send(TransportEndpoint{1}, Channel::Reliable, Span(p));
        ASSERT_EQ(t.Outgoing().size(), 1u);
        EXPECT_NE(t.Outgoing()[0].bytes[0], p[0]); // corrupted
        EXPECT_EQ(t.Outgoing()[0].bytes[1], p[1]); // rest intact
    }
    // reorderWindow >= 2 swaps a consecutive pair.
    {
        MockTransport t; t.RegisterEndpoint(TransportEndpoint{1}); ASSERT_TRUE(t.Bind(0, "a").HasValue());
        t.reorderWindow = 2;
        const std::vector<std::uint8_t> a{0xAA};
        const std::vector<std::uint8_t> b{0xBB};
        (void)t.Send(TransportEndpoint{1}, Channel::Reliable, Span(a));
        (void)t.Send(TransportEndpoint{1}, Channel::Reliable, Span(b));
        ASSERT_EQ(t.Outgoing().size(), 2u);
        EXPECT_EQ(t.Outgoing()[0].bytes, b); // swapped: B before A
        EXPECT_EQ(t.Outgoing()[1].bytes, a);
    }
}

// --- Buffer ownership: value copy, not aliased. --------------------------------
TEST(MockTransportStep3, BufferOwnershipValueCopy)
{
    MockTransport t; t.RegisterEndpoint(TransportEndpoint{1}); ASSERT_TRUE(t.Bind(0, "a").HasValue());
    std::vector<std::uint8_t> src{1, 2, 3};
    EXPECT_EQ(t.Send(TransportEndpoint{1}, Channel::Reliable, Span(src)), TransportOutcome::Queued);
    src[0] = 99; // mutate after send
    EXPECT_EQ(t.Outgoing()[0].bytes, (std::vector<std::uint8_t>{1, 2, 3})); // delivered copy unchanged
}

// --- Poll budget: partial drain, remainder persists. ---------------------------
TEST(MockTransportStep3, PollBudget)
{
    MockTransport t; ASSERT_TRUE(t.Bind(0, "a").HasValue());
    t.InjectInbound(TransportEndpoint{1}, Channel::Reliable, {1, 2, 3});     // 3 bytes
    t.InjectInbound(TransportEndpoint{1}, Channel::Reliable, {4, 5, 6, 7});  // 4 bytes

    t.Poll(0);
    EXPECT_TRUE(t.Received().empty()); // budget 0 moves nothing

    t.Poll(3); // exactly one datagram (3 bytes)
    auto rx = t.Received();
    ASSERT_EQ(rx.size(), 1u);
    EXPECT_EQ(rx[0].bytes.size(), 3u);

    t.Poll(1024); // remainder
    auto rx2 = t.Received();
    ASSERT_EQ(rx2.size(), 1u);
    EXPECT_EQ(rx2[0].bytes.size(), 4u);
}

// --- Negative: unknown endpoint, over-capacity, before-Bind. -------------------
TEST(MockTransportStep3, NegativePaths)
{
    const std::vector<std::uint8_t> p{1};

    MockTransport t;
    // Before Bind: Send -> Error, Poll -> no-op (no crash).
    EXPECT_EQ(t.Send(TransportEndpoint{1}, Channel::Reliable, Span(p)), TransportOutcome::Error);
    t.Poll(1024);
    EXPECT_TRUE(t.Received().empty());

    ASSERT_TRUE(t.Bind(0, "a").HasValue());
    // Unknown endpoint -> EndpointMissing.
    EXPECT_EQ(t.Send(TransportEndpoint{99}, Channel::Reliable, Span(p)), TransportOutcome::EndpointMissing);

    // Over-capacity -> WouldBlock.
    t.RegisterEndpoint(TransportEndpoint{1});
    t.SetOutgoingCapacity(1);
    EXPECT_EQ(t.Send(TransportEndpoint{1}, Channel::Reliable, Span(p)), TransportOutcome::Queued);
    EXPECT_EQ(t.Send(TransportEndpoint{1}, Channel::Reliable, Span(p)), TransportOutcome::WouldBlock);
}

// --- Loopback: bidirectional delivery after Poll. ------------------------------
TEST(LoopbackTransportStep3, Bidirectional)
{
    auto pair = LoopbackTransport::MakePair();
    LoopbackTransport& a = *pair.first;
    LoopbackTransport& b = *pair.second;
    ASSERT_TRUE(a.Bind(0, "a").HasValue());
    ASSERT_TRUE(b.Bind(0, "b").HasValue());

    // A -> B.
    const std::vector<std::uint8_t> m1{1, 2, 3};
    EXPECT_EQ(a.Send(a.Peer(), Channel::Reliable, Span(m1)), TransportOutcome::Ok);
    b.Poll(1024);
    auto rxB = b.Received();
    ASSERT_EQ(rxB.size(), 1u);
    EXPECT_EQ(rxB[0].from, a.Self());
    EXPECT_EQ(rxB[0].bytes, m1);

    // B -> A.
    const std::vector<std::uint8_t> m2{9, 9};
    EXPECT_EQ(b.Send(b.Peer(), Channel::Unreliable, Span(m2)), TransportOutcome::Ok);
    a.Poll(1024);
    auto rxA = a.Received();
    ASSERT_EQ(rxA.size(), 1u);
    EXPECT_EQ(rxA[0].from, b.Self());
    EXPECT_EQ(rxA[0].bytes, m2);

    // Send to an unknown endpoint -> EndpointMissing.
    EXPECT_EQ(a.Send(TransportEndpoint{99}, Channel::Reliable, Span(m1)), TransportOutcome::EndpointMissing);

    // Accepted() returns the single peer once.
    auto acc = a.Accepted();
    ASSERT_EQ(acc.size(), 1u);
    EXPECT_EQ(acc[0], a.Peer());
    EXPECT_TRUE(a.Accepted().empty());
}

// ============================================================================
// Step 4 — MessageRegistry + MessageRegistryService.
// ============================================================================

namespace
{
    // Counting handler for dispatch tests.
    class CountingHandler final : public IMessageHandler
    {
    public:
        int calls = 0;
        bool fail = false;
        [[nodiscard]] core::Expected<void> Handle(ByteReader&, DispatchContext&) override
        {
            ++calls;
            if (fail)
            {
                return core::MakeError(core::ErrorCode::InvalidArgument, "handler failed");
            }
            return core::Success();
        }
    };

    DispatchResult DispatchId(MessageRegistry& reg, MessageId id)
    {
        std::uint8_t buf[1] = {0};
        ByteReader r(buf, 0); // empty payload
        DispatchContext ctx{ConnectionId{1}, 5};
        return reg.Dispatch(id, r, ctx);
    }
}

// --- Service registers the seven control ids; ascending; consistent. -----------
TEST(MessageRegistryStep4, ServiceRegistersControlIds)
{
    MessageRegistryService svc;
    ASSERT_TRUE(svc.Initialize().HasValue());
    EXPECT_EQ(svc.Name(), std::string_view{"MessageRegistry"});
    EXPECT_TRUE(svc.Dependencies().empty());

    const MessageRegistry& reg = svc.Registry();
    EXPECT_EQ(reg.Count(), 7u);
    for (const MessageId id : {kMsgHello, kMsgChallenge, kMsgResponse, kMsgEstablished, kMsgPing, kMsgPong, kMsgBye})
    {
        EXPECT_TRUE(reg.Contains(id));
    }

    const std::vector<MessageId> ids = reg.Ids();
    for (std::size_t i = 1; i < ids.size(); ++i)
    {
        EXPECT_LT(ids[i - 1].value, ids[i].value); // ascending
    }
    EXPECT_TRUE(reg.ValidateConsistency().IsHealthy());

    svc.Shutdown();
    EXPECT_EQ(svc.Registry().Count(), 0u);
}

// --- Dispatch to a registered id calls the handler exactly once. ---------------
TEST(MessageRegistryStep4, DispatchCallsHandlerOnce)
{
    MessageRegistry reg;
    CountingHandler h;
    ASSERT_TRUE(reg.Register(kMsgPing, &h).HasValue());

    EXPECT_EQ(DispatchId(reg, kMsgPing), DispatchResult::Handled);
    EXPECT_EQ(h.calls, 1);
    EXPECT_EQ(DispatchId(reg, kMsgPing), DispatchResult::Handled);
    EXPECT_EQ(h.calls, 2);
}

// --- Handler error surfaces as Failed. -----------------------------------------
TEST(MessageRegistryStep4, HandlerErrorIsFailed)
{
    MessageRegistry reg;
    CountingHandler h;
    h.fail = true;
    ASSERT_TRUE(reg.Register(kMsgPing, &h).HasValue());
    EXPECT_EQ(DispatchId(reg, kMsgPing), DispatchResult::Failed);
}

// --- Unknown id -> Unknown, counted, no crash. ---------------------------------
TEST(MessageRegistryStep4, UnknownIdTolerated)
{
    MessageRegistry reg;
    EXPECT_EQ(reg.UnknownCount(), 0u);
    EXPECT_EQ(DispatchId(reg, MessageId{0x00AB}), DispatchResult::Unknown);
    EXPECT_EQ(DispatchId(reg, MessageId{0x00AB}), DispatchResult::Unknown);
    EXPECT_EQ(reg.UnknownCount(), 2u);
}

// --- Negative: duplicate + data-range via control registration. ----------------
TEST(MessageRegistryStep4, NegativeRegistration)
{
    MessageRegistry reg;
    NullMessageHandler stub;
    ASSERT_TRUE(reg.Register(kMsgHello, &stub).HasValue());
    EXPECT_FALSE(reg.Register(kMsgHello, &stub).HasValue());        // duplicate -> AlreadyExists
    EXPECT_FALSE(reg.Register(MessageId{0x0100}, &stub).HasValue()); // data id via control -> InvalidArgument
    EXPECT_TRUE(reg.RegisterData(MessageId{0x0100}, &stub).HasValue()); // data seam accepts it
    EXPECT_FALSE(reg.RegisterData(MessageId{0x00FF}, &stub).HasValue()); // control id via data -> InvalidArgument
}

// --- Boundary: control range ends; 0x0100 rejected by control registration. ----
TEST(MessageRegistryStep4, BoundaryRegistration)
{
    MessageRegistry reg;
    NullMessageHandler stub;
    EXPECT_TRUE(reg.Register(MessageId{0x0000}, &stub).HasValue());
    EXPECT_TRUE(reg.Register(MessageId{0x00FF}, &stub).HasValue());
    EXPECT_FALSE(reg.Register(MessageId{0x0100}, &stub).HasValue());
    EXPECT_TRUE(reg.ValidateConsistency().IsHealthy());

    // Empty registry dispatch -> Unknown.
    MessageRegistry empty;
    EXPECT_EQ(DispatchId(empty, kMsgPing), DispatchResult::Unknown);
}

// ============================================================================
// Step 5 — ConnectionTable + Connection.
// ============================================================================

// --- Add assigns ascending ids (1,2,3...), never 0; defaults neutral. ----------
TEST(ConnectionTableStep5, AddAssignsAscendingIds)
{
    ConnectionTable table(8);
    const auto a = table.Add(TransportEndpoint{10});
    const auto b = table.Add(TransportEndpoint{11});
    const auto c = table.Add(TransportEndpoint{12});
    ASSERT_TRUE(a.HasValue() && b.HasValue() && c.HasValue());
    EXPECT_EQ(a.Value().value, 1u);
    EXPECT_EQ(b.Value().value, 2u);
    EXPECT_EQ(c.Value().value, 3u);
    EXPECT_EQ(table.Count(), 3u);

    const Connection* conn = table.Find(a.Value());
    ASSERT_NE(conn, nullptr);
    EXPECT_EQ(conn->endpoint, (TransportEndpoint{10}));
    EXPECT_EQ(conn->state, ConnectionState::Disconnected);
    EXPECT_EQ(conn->handshake, HandshakeState::None);
    EXPECT_EQ(conn->lastReason, DisconnectReason::None);
    EXPECT_EQ(conn->playerSlot, 0u);
    EXPECT_EQ(conn->reconnectToken, 0u);
    EXPECT_EQ(conn->nextSequence, 0u);
}

// --- Find / FindMutable resolve present ids; nullptr for absent. ---------------
TEST(ConnectionTableStep5, FindResolution)
{
    ConnectionTable table(8);
    const auto id = table.Add(TransportEndpoint{5});
    ASSERT_TRUE(id.HasValue());
    EXPECT_NE(table.Find(id.Value()), nullptr);
    EXPECT_NE(table.FindMutable(id.Value()), nullptr);
    EXPECT_EQ(table.Find(ConnectionId{999}), nullptr);
    EXPECT_EQ(table.FindMutable(ConnectionId{999}), nullptr);

    // FindMutable is the mutation entry.
    Connection* m = table.FindMutable(id.Value());
    m->state = ConnectionState::Connected;
    EXPECT_EQ(table.Find(id.Value())->state, ConnectionState::Connected);
}

// --- Remove keeps order + accelerator consistent; ids not reused. --------------
TEST(ConnectionTableStep5, RemoveAndNoReuse)
{
    ConnectionTable table(8);
    const auto a = table.Add(TransportEndpoint{1});
    const auto b = table.Add(TransportEndpoint{2});
    const auto c = table.Add(TransportEndpoint{3});
    ASSERT_TRUE(a.HasValue() && b.HasValue() && c.HasValue());

    table.Remove(b.Value()); // remove middle
    EXPECT_EQ(table.Count(), 2u);
    EXPECT_EQ(table.Find(b.Value()), nullptr);
    const std::vector<ConnectionId> ids = table.Ids();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0].value, 1u);
    EXPECT_EQ(ids[1].value, 3u); // ascending, gap closed
    EXPECT_TRUE(table.ValidateConsistency().IsHealthy());

    // New Add gets a NEW id (4), never the removed 2.
    const auto d = table.Add(TransportEndpoint{4});
    ASSERT_TRUE(d.HasValue());
    EXPECT_EQ(d.Value().value, 4u);
    EXPECT_TRUE(table.ValidateConsistency().IsHealthy());

    table.Remove(ConnectionId{999}); // absent -> benign no-op
    EXPECT_EQ(table.Count(), 3u);
}

// --- Capacity boundary: fill exactly, next fails; remove then add new id. -------
TEST(ConnectionTableStep5, CapacityBoundary)
{
    ConnectionTable table(2);
    const auto a = table.Add(TransportEndpoint{1});
    const auto b = table.Add(TransportEndpoint{2});
    ASSERT_TRUE(a.HasValue() && b.HasValue());
    EXPECT_FALSE(table.Add(TransportEndpoint{3}).HasValue()); // at capacity

    table.Remove(a.Value());
    const auto c = table.Add(TransportEndpoint{4});
    ASSERT_TRUE(c.HasValue());
    EXPECT_EQ(c.Value().value, 3u); // new id, not the removed 1
    EXPECT_TRUE(table.ValidateConsistency().IsHealthy());
}

// --- ValidateConsistency healthy across churn; empty table healthy. ------------
TEST(ConnectionTableStep5, ConsistencyAcrossChurn)
{
    ConnectionTable table(16);
    EXPECT_TRUE(table.ValidateConsistency().IsHealthy());
    std::vector<ConnectionId> live;
    for (std::uint32_t i = 0; i < 10; ++i)
    {
        const auto r = table.Add(TransportEndpoint{i + 1});
        ASSERT_TRUE(r.HasValue());
        live.push_back(r.Value());
    }
    table.Remove(live[3]);
    table.Remove(live[7]);
    table.Remove(live[0]);
    EXPECT_EQ(table.Count(), 7u);
    EXPECT_TRUE(table.ValidateConsistency().IsHealthy());
    const std::vector<ConnectionId> ids = table.Ids();
    for (std::size_t i = 1; i < ids.size(); ++i)
    {
        EXPECT_LT(ids[i - 1].value, ids[i].value); // ascending
    }
}

// ============================================================================
// Step 6 — Handshake state machine + IConnectionAuthenticator.
// ============================================================================

namespace
{
    class DenyAuthenticator final : public IConnectionAuthenticator
    {
    public:
        [[nodiscard]] AuthDecision Authorize(const Connection&, const AuthPayload&) override
        {
            return AuthDecision::Deny;
        }
    };

    HandshakeMessage Hello(std::uint16_t version, std::uint32_t nonce)
    {
        return HandshakeMessage{kMsgHello, version, nonce};
    }
    HandshakeMessage Msg(MessageId id, std::uint32_t nonce = 0)
    {
        return HandshakeMessage{id, 0, nonce};
    }
}

// --- Happy path: single step per tick to Established. --------------------------
TEST(HandshakeStep6, HappyPath)
{
    AllowAllAuthenticator auth;
    Handshake hs(auth);
    Connection c;
    c.id = ConnectionId{5};
    c.handshake = HandshakeState::None;

    // Tick 1: Hello -> Challenge out, state ChallengeSent.
    auto r1 = hs.Advance(c, Hello(kProtocolVersion, 0x1111), 1);
    EXPECT_TRUE(r1.advanced);
    EXPECT_FALSE(r1.drop.has_value());
    ASSERT_TRUE(r1.outbound.has_value());
    EXPECT_EQ(r1.outbound->id, kMsgChallenge);
    EXPECT_EQ(r1.state, HandshakeState::ChallengeSent);
    EXPECT_EQ(c.handshake, HandshakeState::ChallengeSent);
    const std::uint32_t serverNonce = r1.outbound->nonce;
    EXPECT_EQ(serverNonce, ServerNonce(c.id));

    // Tick 2: Response echoing the server nonce -> Established out.
    auto r2 = hs.Advance(c, Msg(kMsgResponse, serverNonce), 2);
    EXPECT_TRUE(r2.advanced);
    EXPECT_FALSE(r2.drop.has_value());
    ASSERT_TRUE(r2.outbound.has_value());
    EXPECT_EQ(r2.outbound->id, kMsgEstablished);
    EXPECT_EQ(c.handshake, HandshakeState::Established);
}

// --- Version mismatch at Hello -> ProtocolError, no Established. ----------------
TEST(HandshakeStep6, VersionMismatch)
{
    AllowAllAuthenticator auth;
    Handshake hs(auth);
    Connection c;
    c.id = ConnectionId{1};
    auto r = hs.Advance(c, Hello(kProtocolVersion + 1, 0), 1);
    ASSERT_TRUE(r.drop.has_value());
    EXPECT_EQ(*r.drop, DisconnectReason::ProtocolError);
    EXPECT_FALSE(r.advanced);
    EXPECT_EQ(c.handshake, HandshakeState::None); // unchanged
}

// --- Nonce mismatch at Response -> ProtocolError. ------------------------------
TEST(HandshakeStep6, NonceMismatch)
{
    AllowAllAuthenticator auth;
    Handshake hs(auth);
    Connection c;
    c.id = ConnectionId{2};
    ASSERT_TRUE(hs.Advance(c, Hello(kProtocolVersion, 1), 1).advanced);
    auto r = hs.Advance(c, Msg(kMsgResponse, ServerNonce(c.id) ^ 0xDEAD), 2);
    ASSERT_TRUE(r.drop.has_value());
    EXPECT_EQ(*r.drop, DisconnectReason::ProtocolError);
    EXPECT_NE(c.handshake, HandshakeState::Established);
}

// --- Deny authenticator -> Rejected at the Response edge. -----------------------
TEST(HandshakeStep6, DenyRejects)
{
    DenyAuthenticator auth;
    Handshake hs(auth);
    Connection c;
    c.id = ConnectionId{3};
    ASSERT_TRUE(hs.Advance(c, Hello(kProtocolVersion, 1), 1).advanced);
    auto r = hs.Advance(c, Msg(kMsgResponse, ServerNonce(c.id)), 2);
    ASSERT_TRUE(r.drop.has_value());
    EXPECT_EQ(*r.drop, DisconnectReason::Rejected);
    EXPECT_NE(c.handshake, HandshakeState::Established);
}

// --- Illegal transitions -> ProtocolError; connection not established. ----------
TEST(HandshakeStep6, IllegalTransitions)
{
    AllowAllAuthenticator auth;
    Handshake hs(auth);

    auto expectDrop = [&](HandshakeState start, const HandshakeMessage& m) {
        Connection c;
        c.id = ConnectionId{9};
        c.handshake = start;
        auto r = hs.Advance(c, m, 1);
        EXPECT_TRUE(r.drop.has_value());
        EXPECT_FALSE(r.advanced);
    };

    // Response before Challenge (in None).
    expectDrop(HandshakeState::None, Msg(kMsgResponse, 0));
    // Challenge received in None (server->client msg, out of order).
    expectDrop(HandshakeState::None, Msg(kMsgChallenge, 0));
    // Established-id received early (in None).
    expectDrop(HandshakeState::None, Msg(kMsgEstablished, 0));
    // Duplicate Hello (in ChallengeSent).
    expectDrop(HandshakeState::ChallengeSent, Hello(kProtocolVersion, 0));
    // Any handshake message after Established.
    expectDrop(HandshakeState::Established, Msg(kMsgResponse, 0));
}

// --- Non-handshake control id (Ping) mid-handshake is ignored. ------------------
TEST(HandshakeStep6, PingIgnoredDuringHandshake)
{
    AllowAllAuthenticator auth;
    Handshake hs(auth);
    Connection c;
    c.id = ConnectionId{4};
    c.handshake = HandshakeState::ChallengeSent;
    auto r = hs.Advance(c, Msg(kMsgPing, 0), 1);
    EXPECT_FALSE(r.drop.has_value());
    EXPECT_FALSE(r.advanced);
    EXPECT_EQ(c.handshake, HandshakeState::ChallengeSent); // unchanged
}

// --- No incoming message -> no state change. -----------------------------------
TEST(HandshakeStep6, NoMessageNoChange)
{
    AllowAllAuthenticator auth;
    Handshake hs(auth);
    Connection c;
    c.id = ConnectionId{6};
    c.handshake = HandshakeState::ChallengeSent;
    auto r = hs.Advance(c, std::nullopt, 1);
    EXPECT_FALSE(r.advanced);
    EXPECT_FALSE(r.drop.has_value());
    EXPECT_EQ(c.handshake, HandshakeState::ChallengeSent);
}

// --- Handshake message payload round-trips through bytes (mock delivery). -------
TEST(HandshakeStep6, PayloadRoundTrip)
{
    std::array<std::uint8_t, 8> buf{};
    ByteWriter w(buf.data(), buf.size());
    ASSERT_TRUE(SerializeHandshake(Hello(kProtocolVersion, 0xABCDEF01), w).HasValue());

    ByteReader r(buf.data(), w.BytesWritten());
    auto parsed = DeserializeHandshake(kMsgHello, r);
    ASSERT_TRUE(parsed.HasValue());
    EXPECT_EQ(parsed.Value().id, kMsgHello);
    EXPECT_EQ(parsed.Value().version, kProtocolVersion);
    EXPECT_EQ(parsed.Value().nonce, 0xABCDEF01u);
}

// ============================================================================
// Step 7 — Timeout + unified disconnect flow.
// ============================================================================

// --- TicksFromMs: ceil, min 1, monotonic. --------------------------------------
TEST(TimeoutStep7, TicksFromMs)
{
    EXPECT_EQ(TicksFromMs(1000, 16), 63u); // ceil(62.5)
    EXPECT_EQ(TicksFromMs(32, 16), 2u);
    EXPECT_EQ(TicksFromMs(16, 16), 1u);
    EXPECT_EQ(TicksFromMs(5, 16), 1u);   // sub-tick -> 1
    EXPECT_EQ(TicksFromMs(0, 16), 1u);   // never 0
    EXPECT_GE(TicksFromMs(2000, 16), TicksFromMs(1000, 16)); // monotonic
}

// --- Idle timeout: stale Connected flagged; fresh not. -------------------------
TEST(TimeoutStep7, IdleTimeout)
{
    ConnectionTable table(8);
    const auto stale = table.Add(TransportEndpoint{1});
    const auto fresh = table.Add(TransportEndpoint{2});
    ASSERT_TRUE(stale.HasValue() && fresh.HasValue());

    Connection* s = table.FindMutable(stale.Value());
    s->handshake = HandshakeState::Established;
    s->state = ConnectionState::Connected;
    s->lastRecvTick = 10;
    Connection* f = table.FindMutable(fresh.Value());
    f->handshake = HandshakeState::Established;
    f->state = ConnectionState::Connected;
    f->lastRecvTick = 150;

    const auto flagged = table.ScanTimeouts(/*tick*/ 200, /*handshake*/ 1000, /*connection*/ 100);
    ASSERT_EQ(flagged.size(), 1u);
    EXPECT_EQ(flagged[0], stale.Value()); // 200-10=190 > 100; fresh 200-150=50 not
}

// --- Handshake timeout: non-Established past budget flagged. --------------------
TEST(TimeoutStep7, HandshakeTimeout)
{
    ConnectionTable table(8);
    const auto slow = table.Add(TransportEndpoint{1});
    const auto done = table.Add(TransportEndpoint{2});
    ASSERT_TRUE(slow.HasValue() && done.HasValue());

    Connection* a = table.FindMutable(slow.Value());
    a->handshake = HandshakeState::ChallengeSent; // not complete
    a->createdTick = 10;
    Connection* b = table.FindMutable(done.Value());
    b->handshake = HandshakeState::Established;    // complete in time
    b->createdTick = 10;

    const auto flagged = table.ScanTimeouts(/*tick*/ 200, /*handshake*/ 50, /*connection*/ 1000);
    ASSERT_EQ(flagged.size(), 1u);
    EXPECT_EQ(flagged[0], slow.Value()); // 200-10=190 > 50; established not flagged
}

// --- Unified disconnect: intent + removal + idempotent. ------------------------
TEST(TimeoutStep7, UnifiedDisconnect)
{
    ConnectionTable table(8);
    const auto id = table.Add(TransportEndpoint{77});
    ASSERT_TRUE(id.HasValue());

    const DisconnectIntent intent = table.BeginDisconnect(id.Value(), DisconnectReason::Timeout);
    EXPECT_TRUE(intent.performed);
    EXPECT_EQ(intent.reason, DisconnectReason::Timeout);
    EXPECT_EQ(intent.endpoint, (TransportEndpoint{77})); // captured for Bye
    EXPECT_EQ(table.Find(id.Value()), nullptr);          // removed
    EXPECT_EQ(table.Count(), 0u);

    // Idempotent: second call is a no-op.
    const DisconnectIntent again = table.BeginDisconnect(id.Value(), DisconnectReason::Timeout);
    EXPECT_FALSE(again.performed);
    // Absent id: no-op.
    EXPECT_FALSE(table.BeginDisconnect(ConnectionId{999}, DisconnectReason::Graceful).performed);
}

// --- Multiple simultaneous timeouts processed in ascending id order. -----------
TEST(TimeoutStep7, MultipleTimeoutsAscending)
{
    ConnectionTable table(8);
    std::vector<ConnectionId> ids;
    for (std::uint32_t i = 0; i < 4; ++i)
    {
        const auto r = table.Add(TransportEndpoint{i + 1});
        ASSERT_TRUE(r.HasValue());
        Connection* c = table.FindMutable(r.Value());
        c->handshake = HandshakeState::Established;
        c->state = ConnectionState::Connected;
        c->lastRecvTick = 0; // all stale
        ids.push_back(r.Value());
    }
    const auto flagged = table.ScanTimeouts(/*tick*/ 500, /*handshake*/ 1000, /*connection*/ 100);
    ASSERT_EQ(flagged.size(), 4u);
    for (std::size_t i = 1; i < flagged.size(); ++i)
    {
        EXPECT_LT(flagged[i - 1].value, flagged[i].value); // ascending
    }

    // Disconnect all; table empties.
    for (const ConnectionId cid : flagged)
    {
        EXPECT_TRUE(table.BeginDisconnect(cid, DisconnectReason::Timeout).performed);
    }
    EXPECT_EQ(table.Count(), 0u);
}

// --- Boundary: strict '>' budget. ----------------------------------------------
TEST(TimeoutStep7, BoundaryStrictGreater)
{
    ConnectionTable table(8);
    const auto id = table.Add(TransportEndpoint{1});
    ASSERT_TRUE(id.HasValue());
    Connection* c = table.FindMutable(id.Value());
    c->handshake = HandshakeState::Established;
    c->state = ConnectionState::Connected;
    c->lastRecvTick = 0;

    // Exactly at budget (elapsed == budget) -> not flagged.
    EXPECT_TRUE(table.ScanTimeouts(/*tick*/ 100, /*hs*/ 1000, /*conn*/ 100).empty());
    // One past budget -> flagged.
    EXPECT_EQ(table.ScanTimeouts(/*tick*/ 101, /*hs*/ 1000, /*conn*/ 100).size(), 1u);

    // Empty table scan -> empty.
    ConnectionTable empty(4);
    EXPECT_TRUE(empty.ScanTimeouts(999, 100, 100).empty());
}

// ============================================================================
// Step 8 — Session + SessionService + ISessionObserver.
// ============================================================================

namespace
{
    struct RecordingObserver final : public ISessionObserver
    {
        struct Joined { ConnectionId id; std::uint64_t tick; };
        struct Left { ConnectionId id; DisconnectReason reason; std::uint64_t token; };
        std::vector<Joined> joined;
        std::vector<Left> left;
        int tag = 0;
        std::vector<int>* order = nullptr; // shared fire-order log

        void OnMemberJoined(ConnectionId id, std::uint64_t tick) override
        {
            joined.push_back({id, tick});
            if (order) order->push_back(tag);
        }
        void OnMemberLeft(ConnectionId id, DisconnectReason reason, std::uint64_t token) override
        {
            left.push_back({id, reason, token});
        }
    };
}

// --- Admit fires OnMemberJoined once; ascending; consistent. -------------------
TEST(SessionStep8, AdmitFiresJoined)
{
    Session session(8);
    RecordingObserver obs;
    session.Subscribe(&obs);

    ASSERT_TRUE(session.Admit(ConnectionId{5}, 100).HasValue());
    ASSERT_TRUE(session.Admit(ConnectionId{2}, 101).HasValue());
    ASSERT_TRUE(session.Admit(ConnectionId{9}, 102).HasValue());

    EXPECT_EQ(session.Count(), 3u);
    EXPECT_EQ(obs.joined.size(), 3u);
    EXPECT_TRUE(session.Contains(ConnectionId{2}));
    // Members ascending.
    const auto& m = session.Members();
    ASSERT_EQ(m.size(), 3u);
    EXPECT_EQ(m[0].id.value, 2u);
    EXPECT_EQ(m[1].id.value, 5u);
    EXPECT_EQ(m[2].id.value, 9u);
    EXPECT_EQ(m[0].playerSlot, 0u);
    EXPECT_EQ(m[0].reconnectToken, 0u);
    EXPECT_TRUE(session.ValidateConsistency().IsHealthy());
}

// --- Remove fires OnMemberLeft once (reason + token); second is no-op. ----------
TEST(SessionStep8, RemoveFiresLeftOnce)
{
    Session session(8);
    RecordingObserver obs;
    session.Subscribe(&obs);
    ASSERT_TRUE(session.Admit(ConnectionId{3}, 50).HasValue());

    session.Remove(ConnectionId{3}, DisconnectReason::Timeout);
    EXPECT_EQ(session.Count(), 0u);
    ASSERT_EQ(obs.left.size(), 1u);
    EXPECT_EQ(obs.left[0].id.value, 3u);
    EXPECT_EQ(obs.left[0].reason, DisconnectReason::Timeout);
    EXPECT_NE(obs.left[0].token, 0u); // minted, opaque, non-zero

    session.Remove(ConnectionId{3}, DisconnectReason::Graceful); // absent -> no-op
    EXPECT_EQ(obs.left.size(), 1u); // no second fire
}

// --- Multiple observers fire in registration order. ----------------------------
TEST(SessionStep8, ObserversFireInRegistrationOrder)
{
    Session session(8);
    std::vector<int> fireOrder;
    RecordingObserver a; a.tag = 1; a.order = &fireOrder;
    RecordingObserver b; b.tag = 2; b.order = &fireOrder;
    session.Subscribe(&a);
    session.Subscribe(&b);
    ASSERT_TRUE(session.Admit(ConnectionId{1}, 0).HasValue());
    ASSERT_EQ(fireOrder.size(), 2u);
    EXPECT_EQ(fireOrder[0], 1);
    EXPECT_EQ(fireOrder[1], 2);
}

// --- Negative: duplicate admit; capacity; TryReclaim. --------------------------
TEST(SessionStep8, NegativePaths)
{
    Session session(2);
    ASSERT_TRUE(session.Admit(ConnectionId{1}, 0).HasValue());
    EXPECT_FALSE(session.Admit(ConnectionId{1}, 0).HasValue()); // duplicate -> AlreadyExists
    ASSERT_TRUE(session.Admit(ConnectionId{2}, 0).HasValue());
    EXPECT_FALSE(session.Admit(ConnectionId{3}, 0).HasValue()); // capacity

    EXPECT_FALSE(session.TryReclaim(12345).HasValue()); // not supported
}

// --- Boundary: fill exactly, remove one, admit new; empty members. -------------
TEST(SessionStep8, Boundary)
{
    Session session(2);
    ASSERT_TRUE(session.Admit(ConnectionId{1}, 0).HasValue());
    ASSERT_TRUE(session.Admit(ConnectionId{2}, 0).HasValue());
    EXPECT_FALSE(session.Admit(ConnectionId{3}, 0).HasValue());
    session.Remove(ConnectionId{1}, DisconnectReason::Graceful);
    EXPECT_TRUE(session.Admit(ConnectionId{3}, 0).HasValue());
    EXPECT_TRUE(session.ValidateConsistency().IsHealthy());

    Session empty(4);
    EXPECT_TRUE(empty.Members().empty());
}

// --- SessionService: IService contract + drives the owned Session. -------------
TEST(SessionServiceStep8, ServiceContract)
{
    SessionService svc(8);
    EXPECT_EQ(svc.Name(), std::string_view{"Session"});
    EXPECT_TRUE(svc.Dependencies().empty());
    EXPECT_TRUE(svc.Initialize().HasValue());

    RecordingObserver obs;
    svc.Subscribe(&obs);
    ASSERT_TRUE(svc.Get().Admit(ConnectionId{7}, 10).HasValue());
    EXPECT_EQ(svc.Get().Count(), 1u);
    EXPECT_EQ(obs.joined.size(), 1u);
    svc.Get().Remove(ConnectionId{7}, DisconnectReason::Graceful);
    EXPECT_EQ(obs.left.size(), 1u);

    svc.Shutdown(); // idempotent, safe
    svc.Shutdown();
    SUCCEED();
}

// ============================================================================
// Step 9 — PacketQueues + Reliability + Fragmentation.
// ============================================================================

namespace
{
    std::vector<std::uint8_t> Bytes(std::initializer_list<std::uint8_t> b) { return std::vector<std::uint8_t>(b); }
}

// --- Queues: FIFO within bound; overflow policies. -----------------------------
TEST(PacketQueuesStep9, FifoAndOverflow)
{
    PacketQueues q(/*maxOut*/ 2, /*maxIn*/ 2);
    // FIFO.
    EXPECT_EQ(q.EnqueueOutgoing(Channel::Reliable, Bytes({1})), EnqueueResult::Enqueued);
    EXPECT_EQ(q.EnqueueOutgoing(Channel::Reliable, Bytes({2})), EnqueueResult::Enqueued);
    EXPECT_EQ(q.OutgoingCount(Channel::Reliable), 2u);
    EXPECT_EQ(q.DequeueOutgoing(Channel::Reliable).value(), Bytes({1}));
    EXPECT_EQ(q.DequeueOutgoing(Channel::Reliable).value(), Bytes({2}));
    EXPECT_FALSE(q.DequeueOutgoing(Channel::Reliable).has_value());

    // Reliable overflow -> ReliableOverflow (not enqueued).
    ASSERT_EQ(q.EnqueueOutgoing(Channel::Reliable, Bytes({1})), EnqueueResult::Enqueued);
    ASSERT_EQ(q.EnqueueOutgoing(Channel::Reliable, Bytes({2})), EnqueueResult::Enqueued);
    EXPECT_EQ(q.EnqueueOutgoing(Channel::Reliable, Bytes({3})), EnqueueResult::ReliableOverflow);
    EXPECT_EQ(q.OutgoingCount(Channel::Reliable), 2u); // unchanged

    // Unreliable overflow -> drop-oldest.
    ASSERT_EQ(q.EnqueueOutgoing(Channel::Unreliable, Bytes({10})), EnqueueResult::Enqueued);
    ASSERT_EQ(q.EnqueueOutgoing(Channel::Unreliable, Bytes({11})), EnqueueResult::Enqueued);
    EXPECT_EQ(q.EnqueueOutgoing(Channel::Unreliable, Bytes({12})), EnqueueResult::DroppedOldest);
    EXPECT_EQ(q.OutgoingCount(Channel::Unreliable), 2u);
    EXPECT_EQ(q.DequeueOutgoing(Channel::Unreliable).value(), Bytes({11})); // 10 dropped
    EXPECT_TRUE(q.ValidateConsistency());
}

// --- Reliability: ack removes; unacked retransmit past interval. ----------------
TEST(ReliabilityStep9, AckAndRetransmit)
{
    Reliability rel(/*window*/ 8, /*interval*/ 10);
    std::uint16_t s0 = 0, s1 = 0, s2 = 0;
    ASSERT_EQ(rel.RegisterSend(Bytes({0}), /*tick*/ 0, s0), SendResult::Sent);
    ASSERT_EQ(rel.RegisterSend(Bytes({1}), 0, s1), SendResult::Sent);
    ASSERT_EQ(rel.RegisterSend(Bytes({2}), 0, s2), SendResult::Sent);
    EXPECT_EQ(rel.InFlightCount(), 3u);

    // Ack s1 (latest) with ackBits bit0 => s0 also acked.
    rel.ProcessAck(s1, /*ackBits*/ 0x1);
    EXPECT_EQ(rel.InFlightCount(), 1u); // only s2 remains

    // Before interval: no retransmit.
    EXPECT_TRUE(rel.CollectRetransmits(/*tick*/ 5).empty());
    // Past interval: s2 retransmits.
    auto rtx = rel.CollectRetransmits(/*tick*/ 10);
    ASSERT_EQ(rtx.size(), 1u);
    EXPECT_EQ(rtx[0].sequence, s2);
    // Just collected -> not immediately again.
    EXPECT_TRUE(rel.CollectRetransmits(/*tick*/ 15).empty());
    EXPECT_TRUE(rel.ValidateConsistency());
}

// --- Reliability: window backpressure; unknown ack ignored. --------------------
TEST(ReliabilityStep9, WindowAndUnknownAck)
{
    Reliability rel(/*window*/ 2, /*interval*/ 10);
    std::uint16_t s = 0;
    EXPECT_EQ(rel.RegisterSend(Bytes({0}), 0, s), SendResult::Sent);
    EXPECT_EQ(rel.RegisterSend(Bytes({1}), 0, s), SendResult::Sent);
    EXPECT_EQ(rel.RegisterSend(Bytes({2}), 0, s), SendResult::WindowFull); // window full
    EXPECT_EQ(rel.InFlightCount(), 2u);

    rel.ProcessAck(/*ack*/ 999, /*ackBits*/ 0); // unknown -> ignored
    EXPECT_EQ(rel.InFlightCount(), 2u);
}

// --- Unreliable newest-wins. ----------------------------------------------------
TEST(ReliabilityStep9, UnreliableNewestWins)
{
    UnreliableReceiver rx;
    EXPECT_TRUE(rx.Accept(5));
    EXPECT_TRUE(rx.Accept(6));
    EXPECT_FALSE(rx.Accept(4)); // older -> discarded
    EXPECT_FALSE(rx.Accept(6)); // equal -> discarded
    EXPECT_TRUE(rx.Accept(7));
    // Wraparound: 0xFFFF then 0 is newer.
    UnreliableReceiver rx2;
    EXPECT_TRUE(rx2.Accept(0xFFFF));
    EXPECT_TRUE(rx2.Accept(0));
    EXPECT_FALSE(rx2.Accept(0xFFFE));
}

// --- Fragmentation: split + reassemble under reorder/duplicate. -----------------
TEST(FragmentationStep9, SplitAndReassemble)
{
    Fragmentation frag(/*maxFragmentPayload*/ 4, /*budget*/ 100);
    const std::vector<std::uint8_t> payload{1, 2, 3, 4, 5, 6, 7, 8, 9}; // 9 bytes -> ceil(9/4)=3
    auto fragments = frag.Split(kMsgHello, payload);
    ASSERT_EQ(fragments.size(), 3u);
    EXPECT_EQ(fragments[0].count, 3u);

    // Deliver reordered + duplicate.
    CompletedMessage out;
    EXPECT_EQ(frag.Accept(ConnectionId{1}, fragments[2], /*tick*/ 0, out), AcceptResult::Incomplete);
    EXPECT_EQ(frag.Accept(ConnectionId{1}, fragments[2], 0, out), AcceptResult::Incomplete); // duplicate idempotent
    EXPECT_EQ(frag.Accept(ConnectionId{1}, fragments[0], 0, out), AcceptResult::Incomplete);
    EXPECT_EQ(frag.Accept(ConnectionId{1}, fragments[1], 0, out), AcceptResult::Completed);
    EXPECT_EQ(out.messageId, kMsgHello);
    EXPECT_EQ(out.bytes, payload);
    EXPECT_EQ(frag.PendingCount(), 0u);
}

// --- Fragmentation: single-fragment message; MTU boundary. ---------------------
TEST(FragmentationStep9, BoundaryFragmentCount)
{
    Fragmentation frag(/*maxFragmentPayload*/ 4, /*budget*/ 100);
    EXPECT_EQ(frag.Split(kMsgPing, std::vector<std::uint8_t>(4)).size(), 1u);  // exactly one
    EXPECT_EQ(frag.Split(kMsgPing, std::vector<std::uint8_t>(5)).size(), 2u);  // one over
    EXPECT_EQ(frag.Split(kMsgPing, std::vector<std::uint8_t>{}).size(), 1u);   // empty -> 1

    CompletedMessage out;
    auto one = frag.Split(kMsgPing, std::vector<std::uint8_t>{7, 8, 9});
    ASSERT_EQ(one.size(), 1u);
    EXPECT_EQ(frag.Accept(ConnectionId{1}, one[0], 0, out), AcceptResult::Completed);
    EXPECT_EQ(out.bytes, (std::vector<std::uint8_t>{7, 8, 9}));
}

// --- Fragmentation: index>=count rejected; expiry drops + reclaims. -------------
TEST(FragmentationStep9, RejectAndExpiry)
{
    Fragmentation frag(/*maxFragmentPayload*/ 4, /*budget*/ 50);
    CompletedMessage out;

    Fragment bad; bad.messageId = kMsgHello; bad.index = 3; bad.count = 2; // index >= count
    EXPECT_EQ(frag.Accept(ConnectionId{1}, bad, 0, out), AcceptResult::Rejected);

    // Start a reassembly, then let it expire.
    auto fragments = frag.Split(kMsgHello, std::vector<std::uint8_t>{1, 2, 3, 4, 5}); // 2 fragments
    EXPECT_EQ(frag.Accept(ConnectionId{2}, fragments[0], /*tick*/ 10, out), AcceptResult::Incomplete);
    EXPECT_EQ(frag.PendingCount(), 1u);
    EXPECT_TRUE(frag.ValidateConsistency(/*tick*/ 60));  // 60-10=50, not > 50
    EXPECT_EQ(frag.PruneExpired(/*tick*/ 60), 0u);       // exactly at budget, not dropped
    EXPECT_EQ(frag.PruneExpired(/*tick*/ 61), 1u);       // one past -> dropped
    EXPECT_EQ(frag.PendingCount(), 0u);
    EXPECT_EQ(frag.DroppedIncomplete(), 1u);
}

// --- Fragment sub-header round-trips. ------------------------------------------
TEST(FragmentationStep9, SubHeaderRoundTrip)
{
    Fragment f; f.messageId = MessageId{0x0102}; f.index = 3; f.count = 7;
    std::array<std::uint8_t, kFragmentHeaderWireSize> buf{};
    ByteWriter w(buf.data(), buf.size());
    ASSERT_TRUE(SerializeFragmentHeader(f, w).HasValue());
    EXPECT_EQ(w.BytesWritten(), kFragmentHeaderWireSize);
    ByteReader r(buf.data(), buf.size());
    auto parsed = DeserializeFragmentHeader(r);
    ASSERT_TRUE(parsed.HasValue());
    EXPECT_EQ(parsed.Value().messageId.value, 0x0102u);
    EXPECT_EQ(parsed.Value().index, 3u);
    EXPECT_EQ(parsed.Value().count, 7u);
}

// ============================================================================
// Step 10 — HostServer (+ fake client over loopback / mock).
// ============================================================================

namespace
{
    // Build a control packet (header + payload) as a client would.
    std::vector<std::uint8_t> ClientPacket(MessageId id, Channel ch, std::uint16_t seq,
                                           const std::vector<std::uint8_t>& payload)
    {
        PacketHeader h;
        h.magic = kProtocolMagic;
        h.protocolVersion = kProtocolVersion;
        h.channel = static_cast<std::uint8_t>(ch);
        h.flags = kFlagControl;
        h.sequence = seq;
        h.messageId = id.value;
        h.payloadLength = static_cast<std::uint16_t>(payload.size());
        std::vector<std::uint8_t> pkt(kPacketHeaderWireSize + payload.size());
        ByteWriter w(pkt.data(), pkt.size());
        (void)SerializeHeader(h, w);
        std::copy(payload.begin(), payload.end(),
                  pkt.begin() + static_cast<std::ptrdiff_t>(kPacketHeaderWireSize));
        return pkt;
    }

    std::vector<std::uint8_t> HsPayload(std::uint16_t version, std::uint32_t nonce)
    {
        std::vector<std::uint8_t> p(6);
        ByteWriter w(p.data(), p.size());
        (void)w.WriteU16(version);
        (void)w.WriteU32(nonce);
        return p;
    }

    NetworkingConfig MakeNet(std::uint32_t maxConns)
    {
        NetworkingConfig n;
        n.enabled = true;
        n.maxConnections = maxConns;
        n.handshakeTimeoutMs = 100;   // small -> few ticks (msPerTick 16)
        n.connectionTimeoutMs = 1000;
        n.heartbeatIntervalMs = 500;
        return n;
    }

    // Drains a loopback client's inbox into parsed (messageId, serverNonce) pairs.
    struct ClientMsg { MessageId id; std::uint32_t nonce; };
    std::vector<ClientMsg> DrainClient(LoopbackTransport& io)
    {
        io.Poll(1u << 20);
        std::vector<ClientMsg> out;
        for (const auto& dg : io.Received())
        {
            ByteReader r(dg.bytes.data(), dg.bytes.size());
            auto h = DeserializeHeader(r);
            if (!h) continue;
            ClientMsg m;
            m.id = MessageId{h.Value().messageId};
            m.nonce = 0;
            if (IsControlId(m.id))
            {
                auto hs = DeserializeHandshake(m.id, r); // version+nonce (best-effort)
                if (hs) m.nonce = hs.Value().nonce;
            }
            out.push_back(m);
        }
        return out;
    }
    bool HasMsg(const std::vector<ClientMsg>& v, MessageId id) {
        return std::any_of(v.begin(), v.end(), [id](const ClientMsg& m){ return m.id == id; });
    }
}

// --- Full handshake -> connected -> ping/pong -> graceful bye (loopback). -------
TEST(HostServerStep10, FullFlowLoopback)
{
    auto pair = LoopbackTransport::MakePair();
    LoopbackTransport& hostIo = *pair.first;
    LoopbackTransport& clientIo = *pair.second;
    ASSERT_TRUE(clientIo.Bind(0, "c").HasValue());

    AllowAllAuthenticator auth;
    HostServer host(auth);
    host.SetMsPerTick(16);
    const NetworkingConfig net = MakeNet(4);
    TransportConfig tc; // defaults (maxBytesPerTick large)
    ConnectionTable table(net.maxConnections);
    Session session(net.maxConnections);
    MessageRegistry reg;

    ASSERT_TRUE(host.Start(net, tc, hostIo).HasValue());
    std::uint16_t cseq = 0;

    // Tick 1: host accepts the loopback peer.
    host.Tick(1, hostIo, table, session, reg);
    ASSERT_EQ(table.Count(), 1u);

    // Client -> Hello.
    {
        auto pkt = ClientPacket(kMsgHello, Channel::Reliable, cseq++, HsPayload(kProtocolVersion, 0x1234));
        (void)clientIo.Send(clientIo.Peer(), Channel::Reliable, ConstByteSpan{pkt.data(), pkt.size()});
    }
    host.Tick(2, hostIo, table, session, reg); // Hello -> Challenge
    auto m1 = DrainClient(clientIo);
    ASSERT_TRUE(HasMsg(m1, kMsgChallenge));
    std::uint32_t serverNonce = 0;
    for (const auto& m : m1) if (m.id == kMsgChallenge) serverNonce = m.nonce;

    // Client -> Response(serverNonce).
    {
        auto pkt = ClientPacket(kMsgResponse, Channel::Reliable, cseq++, HsPayload(0, serverNonce));
        (void)clientIo.Send(clientIo.Peer(), Channel::Reliable, ConstByteSpan{pkt.data(), pkt.size()});
    }
    host.Tick(3, hostIo, table, session, reg); // Response -> Established + Admit
    EXPECT_EQ(session.Count(), 1u);
    auto m2 = DrainClient(clientIo);
    EXPECT_TRUE(HasMsg(m2, kMsgEstablished));

    // Client -> Ping ; host -> Pong.
    {
        auto pkt = ClientPacket(kMsgPing, Channel::Reliable, cseq++, {});
        (void)clientIo.Send(clientIo.Peer(), Channel::Reliable, ConstByteSpan{pkt.data(), pkt.size()});
    }
    host.Tick(4, hostIo, table, session, reg);
    auto m3 = DrainClient(clientIo);
    EXPECT_TRUE(HasMsg(m3, kMsgPong));

    // Client -> Bye ; connection removed.
    {
        auto pkt = ClientPacket(kMsgBye, Channel::Reliable, cseq++, {});
        (void)clientIo.Send(clientIo.Peer(), Channel::Reliable, ConstByteSpan{pkt.data(), pkt.size()});
    }
    host.Tick(5, hostIo, table, session, reg);
    EXPECT_EQ(session.Count(), 0u);
    EXPECT_EQ(table.Count(), 0u);
}

// --- Capacity rejection (mock). ------------------------------------------------
TEST(HostServerStep10, CapacityRejection)
{
    MockTransport io;
    AllowAllAuthenticator auth;
    HostServer host(auth);
    const NetworkingConfig net = MakeNet(1);
    TransportConfig tc;
    ConnectionTable table(net.maxConnections);
    Session session(net.maxConnections);
    MessageRegistry reg;
    ASSERT_TRUE(host.Start(net, tc, io).HasValue());

    io.InjectAccept(TransportEndpoint{10});
    io.InjectAccept(TransportEndpoint{11});
    host.Tick(1, io, table, session, reg);
    EXPECT_EQ(table.Count(), 1u);              // only one admitted
    EXPECT_EQ(host.RejectedCapacity(), 1u);    // the other rejected (CapacityFull)
}

// --- Version-mismatch rejection (mock). ----------------------------------------
TEST(HostServerStep10, VersionMismatchRejected)
{
    MockTransport io;
    AllowAllAuthenticator auth;
    HostServer host(auth);
    const NetworkingConfig net = MakeNet(4);
    TransportConfig tc;
    ConnectionTable table(net.maxConnections);
    Session session(net.maxConnections);
    MessageRegistry reg;
    ASSERT_TRUE(host.Start(net, tc, io).HasValue());

    io.InjectAccept(TransportEndpoint{10});
    host.Tick(1, io, table, session, reg);
    ASSERT_EQ(table.Count(), 1u);

    auto bad = ClientPacket(kMsgHello, Channel::Reliable, 0, HsPayload(kProtocolVersion + 1, 0));
    io.InjectInbound(TransportEndpoint{10}, Channel::Reliable, bad);
    host.Tick(2, io, table, session, reg);
    EXPECT_EQ(table.Count(), 0u);   // disconnected on ProtocolError
    EXPECT_EQ(session.Count(), 0u); // never admitted
}

// --- Malformed datagram dropped; host survives. --------------------------------
TEST(HostServerStep10, MalformedDropped)
{
    MockTransport io;
    AllowAllAuthenticator auth;
    HostServer host(auth);
    const NetworkingConfig net = MakeNet(4);
    TransportConfig tc;
    ConnectionTable table(net.maxConnections);
    Session session(net.maxConnections);
    MessageRegistry reg;
    ASSERT_TRUE(host.Start(net, tc, io).HasValue());

    io.InjectAccept(TransportEndpoint{10});
    host.Tick(1, io, table, session, reg);
    io.InjectInbound(TransportEndpoint{10}, Channel::Reliable, {0x00, 0x01, 0x02}); // garbage
    host.Tick(2, io, table, session, reg);
    EXPECT_EQ(host.DroppedDatagrams(), 1u);
    EXPECT_EQ(table.Count(), 1u); // connection unaffected
}

// --- E-G3-N replay: identical inputs -> identical outgoing + state. -------------
TEST(HostServerStep10, ReplayDeterminism)
{
    auto run = [](MockTransport& io, ConnectionTable& table, Session& session) {
        AllowAllAuthenticator auth;
        HostServer host(auth);
        host.SetMsPerTick(16);
        const NetworkingConfig net = MakeNet(4);
        TransportConfig tc;
        MessageRegistry reg;
        (void)host.Start(net, tc, io);

        io.InjectAccept(TransportEndpoint{10});
        host.Tick(1, io, table, session, reg);
        auto hello = ClientPacket(kMsgHello, Channel::Reliable, 0, HsPayload(kProtocolVersion, 0xAAAA));
        io.InjectInbound(TransportEndpoint{10}, Channel::Reliable, hello);
        host.Tick(2, io, table, session, reg);
        // Respond with the deterministic server nonce for id 1.
        auto resp = ClientPacket(kMsgResponse, Channel::Reliable, 1, HsPayload(0, ServerNonce(ConnectionId{1})));
        io.InjectInbound(TransportEndpoint{10}, Channel::Reliable, resp);
        host.Tick(3, io, table, session, reg);
    };

    MockTransport ioA; ConnectionTable tableA(4); Session sessionA(4);
    MockTransport ioB; ConnectionTable tableB(4); Session sessionB(4);
    run(ioA, tableA, sessionA);
    run(ioB, tableB, sessionB);

    // Identical session/table state.
    EXPECT_EQ(tableA.Count(), tableB.Count());
    EXPECT_EQ(sessionA.Count(), sessionB.Count());
    EXPECT_EQ(sessionA.Count(), 1u);
    // Identical outgoing bytes.
    ASSERT_EQ(ioA.Outgoing().size(), ioB.Outgoing().size());
    for (std::size_t i = 0; i < ioA.Outgoing().size(); ++i)
    {
        EXPECT_EQ(ioA.Outgoing()[i].bytes, ioB.Outgoing()[i].bytes);
        EXPECT_EQ(ioA.Outgoing()[i].to, ioB.Outgoing()[i].to);
    }
}

// ============================================================================
// Step 11 — NetworkingService + TransportService + NetworkDiagnostics.
// ============================================================================

namespace
{
    // A transport whose Bind fails (for the enabled-bind-failure path). Implements
    // ITransport directly (MockTransport is final).
    class FailBindTransport final : public ITransport
    {
    public:
        [[nodiscard]] core::Expected<void> Bind(std::uint16_t, std::string_view) override
        {
            return core::MakeError(core::ErrorCode::IoError, "bind failed");
        }
        [[nodiscard]] core::Expected<void> Listen(std::uint32_t) override { return core::Success(); }
        void Poll(std::size_t) override {}
        [[nodiscard]] TransportOutcome Send(TransportEndpoint, Channel, ConstByteSpan) override
        {
            return TransportOutcome::Ok;
        }
        void CloseListen() override {}
        void CloseEndpoint(TransportEndpoint) override {}
        [[nodiscard]] std::vector<TransportEndpoint> Accepted() override { return {}; }
        [[nodiscard]] std::vector<ReceivedDatagram> Received() override { return {}; }
    };
}

// --- TransportService owns and exposes a transport. ----------------------------
TEST(TransportServiceStep11, OwnsTransport)
{
    TransportService svc(std::make_unique<MockTransport>());
    EXPECT_EQ(svc.Name(), std::string_view{"Transport"});
    EXPECT_TRUE(svc.Dependencies().empty());
    EXPECT_TRUE(svc.Initialize().HasValue());
    ASSERT_TRUE(svc.Transport().Bind(0, "x").HasValue()); // usable
    svc.Shutdown();
    SUCCEED();
}

// --- NetworkingService: IService/ITickable contract; deps; lifecycle. ----------
TEST(NetworkingServiceStep11, ContractAndLifecycle)
{
    MockTransport io;
    AllowAllAuthenticator auth;
    Session session(4);
    MessageRegistry reg;
    NetworkingService svc(MakeNet(4), TransportConfig{}, io, session, reg, auth);

    EXPECT_EQ(svc.Name(), std::string_view{"Networking"});
    const auto deps = svc.Dependencies();
    ASSERT_EQ(deps.size(), 4u);
    EXPECT_EQ(deps[0], "World");
    EXPECT_EQ(deps[1], "EntityRegistry");
    EXPECT_EQ(deps[2], "BubbleManager");
    EXPECT_EQ(deps[3], "TransitionManager");

    ASSERT_TRUE(svc.Initialize().HasValue());
    EXPECT_TRUE(svc.Host().IsRunning());

    svc.Tick(0.016);
    svc.Tick(0.016);
    EXPECT_EQ(svc.LastTick(), 2u); // monotonic

    svc.Shutdown();
    svc.Shutdown(); // idempotent
    SUCCEED();
}

// --- Disabled: Initialize success, ticks are no-ops. ---------------------------
TEST(NetworkingServiceStep11, DisabledNoOp)
{
    MockTransport io;
    AllowAllAuthenticator auth;
    Session session(4);
    MessageRegistry reg;
    NetworkingConfig net = MakeNet(4);
    net.enabled = false;
    NetworkingService svc(net, TransportConfig{}, io, session, reg, auth);

    ASSERT_TRUE(svc.Initialize().HasValue());
    svc.Tick(0.016);
    EXPECT_EQ(svc.Table().Count(), 0u); // nothing happens
    EXPECT_EQ(session.Count(), 0u);
}

// --- Enabled + bind failure -> Initialize error. -------------------------------
TEST(NetworkingServiceStep11, BindFailure)
{
    FailBindTransport io;
    AllowAllAuthenticator auth;
    Session session(4);
    MessageRegistry reg;
    NetworkingService svc(MakeNet(4), TransportConfig{}, io, session, reg, auth);
    EXPECT_FALSE(svc.Initialize().HasValue()); // enabled + bind fails
}

// --- End-to-end handshake/admit/disconnect through NetworkingService::Tick. -----
TEST(NetworkingServiceStep11, EndToEndViaTick)
{
    auto pair = LoopbackTransport::MakePair();
    LoopbackTransport& hostIo = *pair.first;
    LoopbackTransport& clientIo = *pair.second;
    ASSERT_TRUE(clientIo.Bind(0, "c").HasValue());

    AllowAllAuthenticator auth;
    Session session(4);
    MessageRegistry reg;
    NetworkingService svc(MakeNet(4), TransportConfig{}, hostIo, session, reg, auth);
    ASSERT_TRUE(svc.Initialize().HasValue());

    std::uint16_t cs = 0;
    svc.Tick(0.016); // accept
    EXPECT_EQ(svc.Table().Count(), 1u);

    { auto p = ClientPacket(kMsgHello, Channel::Reliable, cs++, HsPayload(kProtocolVersion, 0x22));
      (void)clientIo.Send(clientIo.Peer(), Channel::Reliable, ConstByteSpan{p.data(), p.size()}); }
    svc.Tick(0.016); // Hello -> Challenge
    auto m1 = DrainClient(clientIo);
    ASSERT_TRUE(HasMsg(m1, kMsgChallenge));
    std::uint32_t sn = 0; for (const auto& m : m1) if (m.id == kMsgChallenge) sn = m.nonce;

    { auto p = ClientPacket(kMsgResponse, Channel::Reliable, cs++, HsPayload(0, sn));
      (void)clientIo.Send(clientIo.Peer(), Channel::Reliable, ConstByteSpan{p.data(), p.size()}); }
    svc.Tick(0.016); // Response -> Established + Admit
    EXPECT_EQ(session.Count(), 1u);

    // Diagnostics reflect the connected session.
    NetworkDiagnostics diag(svc.Table(), session, svc.Host());
    const NetworkStatistics st = diag.Statistics();
    EXPECT_EQ(st.connectionCount, 1u);
    EXPECT_EQ(st.sessionCount, 1u);
    EXPECT_EQ(st.connectedCount, 1u);
    EXPECT_NE(diag.DescribeState().find("connected=1"), std::string::npos);
    EXPECT_NE(diag.DumpConnections().find("1"), std::string::npos);
    EXPECT_TRUE(diag.ValidateConsistency().IsHealthy());

    // Diagnostics are read-only: state unchanged after calling them.
    const std::size_t before = svc.Table().Count();
    (void)diag.Statistics(); (void)diag.DescribeState(); (void)diag.DumpConnections();
    EXPECT_EQ(svc.Table().Count(), before);
}

// --- Diagnostics on an empty module: zeroed, healthy, no crash. ----------------
TEST(NetworkDiagnosticsStep11, EmptyModule)
{
    MockTransport io;
    AllowAllAuthenticator auth;
    Session session(4);
    MessageRegistry reg;
    NetworkingService svc(MakeNet(4), TransportConfig{}, io, session, reg, auth);
    ASSERT_TRUE(svc.Initialize().HasValue());

    NetworkDiagnostics diag(svc.Table(), session, svc.Host());
    const NetworkStatistics st = diag.Statistics();
    EXPECT_EQ(st.connectionCount, 0u);
    EXPECT_EQ(st.sessionCount, 0u);
    EXPECT_EQ(st.droppedDatagrams, 0u);
    EXPECT_TRUE(diag.ValidateConsistency().IsHealthy());
    EXPECT_NE(diag.DescribeConnection(ConnectionId{99}).find("absent"), std::string::npos); // absent formatting
}

// ============================================================================
// Step 13 — Platform transport factory (test build links the null/loopback).
// ============================================================================

// --- One Platform Boundary: the test build's CreatePlatformTransport yields an
//     OS-free transport (no real socket), usable through the ITransport seam. -----
TEST(PlatformTransportStep13, TestBuildUsesNullFactory)
{
    std::unique_ptr<ITransport> transport = adapters::CreatePlatformTransport(TransportConfig{});
    ASSERT_NE(transport, nullptr);

    // Usable without any OS socket: bind succeeds trivially (null/mock), and the
    // seam behaves as specified.
    ASSERT_TRUE(transport->Bind(0, "127.0.0.1").HasValue());
    ASSERT_TRUE(transport->Listen(16).HasValue());

    // Send to an unknown endpoint -> EndpointMissing (benign), never a crash.
    const std::vector<std::uint8_t> payload{1, 2, 3};
    EXPECT_EQ(transport->Send(TransportEndpoint{404}, Channel::Reliable, ConstByteSpan{payload.data(), payload.size()}),
              TransportOutcome::EndpointMissing);

    // Poll and drains are safe and empty on a fresh transport.
    transport->Poll(1024);
    EXPECT_TRUE(transport->Received().empty());
    EXPECT_TRUE(transport->Accepted().empty());
}

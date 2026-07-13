// STALKER-MP — Transport seam (Sprint-006, Step 3)
//
// The ONLY abstraction over the OS (ADR-009: One Platform Boundary). The module
// above depends solely on this engine-free / OS-free interface; the real socket
// implementation (PlatformSockets.cpp, Step 13) and the in-memory mock/loopback
// transports (this step) live behind it. NO socket/address/OS type crosses this
// seam — only opaque TransportEndpoint handles and owned byte buffers.
//
// ADR-007: no exceptions; fallible ops return core::Expected / TransportOutcome.
// ADR-011: Poll is non-blocking; no threads. Engine-free and OS-free.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/net/NetTypes.h" // TransportEndpoint, Channel, TransportOutcome

namespace stalkermp::net
{
    // Non-owning read-only view of bytes handed to Send. The transport copies out
    // of it before returning (owned-buffer value transfer; no aliasing).
    struct ConstByteSpan
    {
        const std::uint8_t* data = nullptr;
        std::size_t size = 0;
    };

    // A received datagram, owned by value (ADR-009: no OS type here).
    struct ReceivedDatagram
    {
        TransportEndpoint from{};
        Channel channel = Channel::Reliable;
        std::vector<std::uint8_t> bytes;
    };

    class ITransport
    {
    public:
        virtual ~ITransport() = default;

        // Bring the transport up. addressView is a textual address (validated by
        // the concrete transport; the mock/loopback accept any non-empty value).
        [[nodiscard]] virtual core::Expected<void> Bind(std::uint16_t port, std::string_view addressView) = 0;
        [[nodiscard]] virtual core::Expected<void> Listen(std::uint32_t backlog) = 0;

        // Non-blocking pump. Moves at most maxBytesBudget bytes of inbound data
        // into the Received() drain; leftover stays queued for the next tick.
        virtual void Poll(std::size_t maxBytesBudget) = 0;

        // Enqueue/deliver bytes to a peer. Returns a value outcome, never throws.
        [[nodiscard]] virtual TransportOutcome Send(TransportEndpoint to, Channel channel, ConstByteSpan bytes) = 0;

        virtual void CloseListen() = 0;
        virtual void CloseEndpoint(TransportEndpoint endpoint) = 0;

        // Drain (by value) newly accepted endpoints and received datagrams.
        [[nodiscard]] virtual std::vector<TransportEndpoint> Accepted() = 0;
        [[nodiscard]] virtual std::vector<ReceivedDatagram> Received() = 0;
    };
} // namespace stalkermp::net

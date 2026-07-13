// STALKER-MP — Platform sockets (Sprint-006, Step 13)
//
// The ONLY translation unit in the module that includes OS socket headers
// (Winsock). It implements a non-blocking UDP net::ITransport and defines the
// CreatePlatformTransport factory for the engine build. No socket/address/OS type
// crosses the engine-free net::ITransport seam (One Platform Boundary, ADR-009):
// remote (address, port) is mapped to an opaque net::TransportEndpoint handle held
// privately here. ADR-007: no exceptions; every fallible op returns
// core::Expected / net::TransportOutcome. Non-blocking; Poll is byte-budgeted.
//
// This TU compiles ONLY in the engine build (Windows). The test build links
// tests/support/NullPlatformTransport.cpp instead.

#include "stalkermp/adapters/PlatformTransport.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// --- OS socket headers (confined to this TU) --------------------------------
#include <winsock2.h>
#include <ws2tcpip.h>
#if defined(_MSC_VER)
#pragma comment(lib, "ws2_32.lib") // TU-scoped Winsock link (One Platform Boundary)
#endif
// -----------------------------------------------------------------------------

#include "stalkermp/core/Error.h"
#include "stalkermp/core/Expected.h"
#include "stalkermp/net/ITransport.h"
#include "stalkermp/net/NetTypes.h"
#include "stalkermp/net/TransportConfig.h"

namespace stalkermp::adapters
{
    namespace
    {
        // Non-blocking UDP transport. UDP has no accept; a connection is modeled
        // from the first datagram seen from a remote address, minting an endpoint.
        class UdpTransport final : public net::ITransport
        {
        public:
            explicit UdpTransport(const net::TransportConfig& config) : m_config(config) {}

            ~UdpTransport() override
            {
                CloseListen();
                if (m_wsaStarted)
                {
                    ::WSACleanup();
                }
            }

            [[nodiscard]] core::Expected<void> Bind(const std::uint16_t port,
                                                    const std::string_view addressView) override
            {
                if (!m_wsaStarted)
                {
                    WSADATA data;
                    if (::WSAStartup(MAKEWORD(2, 2), &data) != 0)
                    {
                        return core::MakeError(core::ErrorCode::IoError, "WSAStartup failed");
                    }
                    m_wsaStarted = true;
                }

                m_socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                if (m_socket == INVALID_SOCKET)
                {
                    return core::MakeError(core::ErrorCode::IoError, "UDP socket creation failed");
                }

                u_long nonBlocking = 1;
                if (::ioctlsocket(m_socket, FIONBIO, &nonBlocking) == SOCKET_ERROR)
                {
                    CloseListen();
                    return core::MakeError(core::ErrorCode::IoError, "failed to set socket non-blocking");
                }

                sockaddr_in addr;
                std::memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_port = ::htons(port);
                const std::string address(addressView);
                if (::inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1)
                {
                    CloseListen();
                    return core::MakeError(core::ErrorCode::InvalidArgument, "invalid bind address");
                }

                if (::bind(m_socket, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
                {
                    CloseListen();
                    return core::MakeError(core::ErrorCode::IoError, "UDP bind failed (port in use?)");
                }

                m_bound = true;
                return core::Success();
            }

            [[nodiscard]] core::Expected<void> Listen(std::uint32_t /*backlog*/) override
            {
                return core::Success(); // UDP: no listen backlog
            }

            void Poll(const std::size_t maxBytesBudget) override
            {
                if (!m_bound)
                {
                    return;
                }
                std::vector<std::uint8_t> buffer(m_config.mtuPayloadBytes);
                std::size_t used = 0;
                while (used < maxBytesBudget)
                {
                    sockaddr_in from;
                    int fromLen = static_cast<int>(sizeof(from));
                    std::memset(&from, 0, sizeof(from));
                    const int received = ::recvfrom(m_socket, reinterpret_cast<char*>(buffer.data()),
                                                    static_cast<int>(buffer.size()), 0,
                                                    reinterpret_cast<sockaddr*>(&from), &fromLen);
                    if (received == SOCKET_ERROR)
                    {
                        break; // WSAEWOULDBLOCK (no more data) or an error -> stop this tick
                    }
                    if (received <= 0)
                    {
                        break;
                    }
                    const net::TransportEndpoint endpoint = ResolveOrCreateEndpoint(from);
                    net::ReceivedDatagram datagram;
                    datagram.from = endpoint;
                    datagram.channel = net::Channel::Reliable; // real channel is in the packet header
                    datagram.bytes.assign(buffer.begin(),
                                          buffer.begin() + static_cast<std::ptrdiff_t>(received));
                    m_received.push_back(std::move(datagram));
                    used += static_cast<std::size_t>(received);
                }
            }

            [[nodiscard]] net::TransportOutcome Send(const net::TransportEndpoint to, net::Channel /*channel*/,
                                                     const net::ConstByteSpan bytes) override
            {
                if (!m_bound)
                {
                    return net::TransportOutcome::Error;
                }
                const auto it = m_endpointToAddr.find(to.value);
                if (it == m_endpointToAddr.end())
                {
                    return net::TransportOutcome::EndpointMissing;
                }
                const int sent = ::sendto(m_socket, reinterpret_cast<const char*>(bytes.data),
                                          static_cast<int>(bytes.size), 0,
                                          reinterpret_cast<const sockaddr*>(&it->second), sizeof(sockaddr_in));
                if (sent == SOCKET_ERROR)
                {
                    return (::WSAGetLastError() == WSAEWOULDBLOCK) ? net::TransportOutcome::WouldBlock
                                                                  : net::TransportOutcome::Error;
                }
                return net::TransportOutcome::Ok;
            }

            void CloseListen() override
            {
                if (m_socket != INVALID_SOCKET)
                {
                    ::closesocket(m_socket);
                    m_socket = INVALID_SOCKET;
                }
                m_bound = false;
            }

            void CloseEndpoint(const net::TransportEndpoint endpoint) override
            {
                const auto it = m_endpointToAddr.find(endpoint.value);
                if (it != m_endpointToAddr.end())
                {
                    m_addrToEndpoint.erase(AddrKey(it->second));
                    m_endpointToAddr.erase(it);
                }
            }

            [[nodiscard]] std::vector<net::TransportEndpoint> Accepted() override
            {
                return std::exchange(m_accepted, {});
            }
            [[nodiscard]] std::vector<net::ReceivedDatagram> Received() override
            {
                return std::exchange(m_received, {});
            }

        private:
            [[nodiscard]] static std::uint64_t AddrKey(const sockaddr_in& addr) noexcept
            {
                return (static_cast<std::uint64_t>(addr.sin_addr.s_addr) << 16) |
                       static_cast<std::uint64_t>(addr.sin_port);
            }

            [[nodiscard]] net::TransportEndpoint ResolveOrCreateEndpoint(const sockaddr_in& from)
            {
                const std::uint64_t key = AddrKey(from);
                const auto existing = m_addrToEndpoint.find(key);
                if (existing != m_addrToEndpoint.end())
                {
                    return net::TransportEndpoint{existing->second};
                }
                const std::uint32_t value = m_nextEndpoint++;
                m_addrToEndpoint.emplace(key, value);
                m_endpointToAddr.emplace(value, from);
                m_accepted.push_back(net::TransportEndpoint{value}); // "accept" on first datagram
                return net::TransportEndpoint{value};
            }

            net::TransportConfig m_config;
            SOCKET m_socket = INVALID_SOCKET;
            bool m_bound = false;
            bool m_wsaStarted = false;
            std::uint32_t m_nextEndpoint = 1;
            std::unordered_map<std::uint32_t, sockaddr_in> m_endpointToAddr;
            std::unordered_map<std::uint64_t, std::uint32_t> m_addrToEndpoint;
            std::vector<net::TransportEndpoint> m_accepted;
            std::vector<net::ReceivedDatagram> m_received;
        };
    } // namespace

    std::unique_ptr<net::ITransport> CreatePlatformTransport(const net::TransportConfig& config)
    {
        return std::make_unique<UdpTransport>(config);
    }
} // namespace stalkermp::adapters

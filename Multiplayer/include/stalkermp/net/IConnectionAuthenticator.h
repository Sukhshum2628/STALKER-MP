// STALKER-MP — Connection authenticator seam (Sprint-006, Step 6)
//
// The auth seam called at the handshake Response->Established edge. Sprint-006
// ships only AllowAllAuthenticator (no auth logic, no crypto). Engine-free /
// OS-free. ADR-007.

#pragma once

#include <cstdint>

#include "stalkermp/net/Connection.h"

namespace stalkermp::net
{
    // Payload presented to the authenticator (the echoed server nonce + reserved
    // fields). No credentials/crypto in Sprint-006.
    struct AuthPayload
    {
        std::uint32_t nonce = 0;
    };

    enum class AuthDecision : std::uint8_t
    {
        Allow = 0,
        Deny,
    };

    class IConnectionAuthenticator
    {
    public:
        virtual ~IConnectionAuthenticator() = default;
        [[nodiscard]] virtual AuthDecision Authorize(const Connection& connection, const AuthPayload& payload) = 0;
    };

    // Default Sprint-006 authenticator: allows every connection.
    class AllowAllAuthenticator final : public IConnectionAuthenticator
    {
    public:
        [[nodiscard]] AuthDecision Authorize(const Connection&, const AuthPayload&) override
        {
            return AuthDecision::Allow;
        }
    };
} // namespace stalkermp::net

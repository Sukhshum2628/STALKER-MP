// STALKER-MP — Platform transport factory (Sprint-006, Step 13)
//
// Engine-free / OS-free header. The real UDP implementation lives in
// PlatformSockets.cpp (the ONLY OS-header TU — One Platform Boundary, ADR-009);
// the test build links NullPlatformTransport.cpp (a loopback/mock counterpart).
// No socket/address/OS type crosses the engine-free net::ITransport seam.

#pragma once

#include <memory>

#include "stalkermp/net/ITransport.h"
#include "stalkermp/net/TransportConfig.h"

namespace stalkermp::adapters
{
    [[nodiscard]] std::unique_ptr<net::ITransport> CreatePlatformTransport(const net::TransportConfig& config);
} // namespace stalkermp::adapters

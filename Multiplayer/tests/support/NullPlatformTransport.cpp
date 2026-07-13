// STALKER-MP — Null platform transport (test build only, Sprint-006, Step 13)
//
// Test-build counterpart of src/adapters/PlatformSockets.cpp: provides the
// CreatePlatformTransport factory WITHOUT any OS socket dependency, so the test
// build links no OS transport (One Platform Boundary). Returns an inert in-memory
// transport; networking is disabled by default (never bound), and when exercised
// it behaves like the mock.

#include "stalkermp/adapters/PlatformTransport.h"

#include "stalkermp/net/MockTransport.h"

namespace stalkermp::adapters
{
    std::unique_ptr<net::ITransport> CreatePlatformTransport(const net::TransportConfig&)
    {
        return std::make_unique<net::MockTransport>();
    }
} // namespace stalkermp::adapters

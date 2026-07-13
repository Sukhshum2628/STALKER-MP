// STALKER-MP — Transport service (Sprint-006, Step 11)
//
// See TransportService.h. Engine-free / OS-free; no exceptions.

#include "stalkermp/net/TransportService.h"

namespace stalkermp::net
{
    core::Expected<void> TransportService::Initialize()
    {
        return core::Success(); // the transport is bound by the host at NetworkingService::Initialize
    }

    void TransportService::Shutdown() noexcept
    {
        // The transport is closed via the host's Stop path; nothing to release here.
    }
} // namespace stalkermp::net

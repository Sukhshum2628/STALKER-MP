// STALKER-MP — Session service (Sprint-006, Step 8)
//
// See SessionService.h. Engine-free / OS-free; no exceptions, core::Expected.
// Lifecycle only; the host drives Admit/Remove on the owned Session.

#include "stalkermp/net/SessionService.h"

namespace stalkermp::net
{
    core::Expected<void> SessionService::Initialize()
    {
        // No resources to acquire; the Session is constructed with the service.
        return core::Success();
    }

    void SessionService::Shutdown() noexcept
    {
        // Session membership is transient; destroyed with the service. Nothing to
        // release here.
    }
} // namespace stalkermp::net

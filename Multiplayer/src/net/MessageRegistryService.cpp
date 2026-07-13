// STALKER-MP — Message registry service (Sprint-006, Step 4)
//
// See MessageRegistryService.h. Engine-free / OS-free; no exceptions,
// core::Expected. Registers the seven control-message ids at Initialize.

#include "stalkermp/net/MessageRegistryService.h"

#include "stalkermp/net/ProtocolConstants.h" // control message ids

namespace stalkermp::net
{
    core::Expected<void> MessageRegistryService::Initialize()
    {
        const MessageId controlIds[] = {
            kMsgHello, kMsgChallenge, kMsgResponse, kMsgEstablished, kMsgPing, kMsgPong, kMsgBye,
        };
        for (const MessageId id : controlIds)
        {
            if (auto r = m_registry.Register(id, &m_controlStub); !r)
            {
                return r;
            }
        }
        return core::Success();
    }

    void MessageRegistryService::Shutdown() noexcept
    {
        m_registry.Clear();
    }
} // namespace stalkermp::net

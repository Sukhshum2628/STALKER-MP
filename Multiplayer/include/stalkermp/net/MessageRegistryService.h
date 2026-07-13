// STALKER-MP — Message registry service (Sprint-006, Step 4)
//
// core::IService (NOT ITickable) owning a MessageRegistry and registering the
// seven control-message ids at Initialize. Engine-free / OS-free. ADR-007.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/interfaces/IService.h"
#include "stalkermp/net/MessageRegistry.h"

namespace stalkermp::net
{
    // Inert control-message handler stub (Step 4). Parses nothing; returns
    // Success(). Real handlers arrive in Steps 6-10.
    class NullMessageHandler final : public IMessageHandler
    {
    public:
        [[nodiscard]] core::Expected<void> Handle(ByteReader&, DispatchContext&) override
        {
            return core::Success();
        }
    };

    class MessageRegistryService final : public core::IService
    {
    public:
        // --- core::IService -------------------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "MessageRegistry"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override { return {}; }
        [[nodiscard]] core::Expected<void> Initialize() override;
        void Shutdown() noexcept override;

        // --- Registry access ------------------------------------------------
        [[nodiscard]] MessageRegistry& Registry() noexcept { return m_registry; }
        [[nodiscard]] const MessageRegistry& Registry() const noexcept { return m_registry; }

        // Data-range registration passthrough for later sprints (validates the
        // id is in the data range). Sprint-006 registers no data ids.
        [[nodiscard]] core::Expected<void> Register(MessageId id, IMessageHandler* handler)
        {
            return m_registry.RegisterData(id, handler);
        }

    private:
        MessageRegistry m_registry;
        NullMessageHandler m_controlStub; // shared inert handler for the control ids
    };
} // namespace stalkermp::net

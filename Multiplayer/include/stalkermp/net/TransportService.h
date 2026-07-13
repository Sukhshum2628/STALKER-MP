// STALKER-MP — Transport service (Sprint-006, Step 11)
//
// core::IService (NO tick) owning the ITransport. Kept separate so tests inject a
// transport without building the whole module; the real build gets its transport
// from adapters::CreatePlatformTransport (Step 13). Engine-free / OS-free. ADR-007.

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/interfaces/IService.h"
#include "stalkermp/net/ITransport.h"

namespace stalkermp::net
{
    class TransportService final : public core::IService
    {
    public:
        explicit TransportService(std::unique_ptr<ITransport> transport) : m_transport(std::move(transport)) {}

        // --- core::IService -------------------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "Transport"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override { return {}; }
        [[nodiscard]] core::Expected<void> Initialize() override;
        void Shutdown() noexcept override;

        // Non-owning access to the owned transport (must outlive the host that uses it).
        [[nodiscard]] ITransport& Transport() noexcept { return *m_transport; }
        [[nodiscard]] const ITransport& Transport() const noexcept { return *m_transport; }

    private:
        std::unique_ptr<ITransport> m_transport;
    };
} // namespace stalkermp::net

// STALKER-MP — Session service (Sprint-006, Step 8)
//
// core::IService (NOT ITickable) owning the Session + its observer registry.
// Host-authoritative membership; the host (Step 10) drives Admit/Remove.
// Engine-free / OS-free. ADR-007.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/interfaces/IService.h"
#include "stalkermp/net/Session.h"

namespace stalkermp::net
{
    class SessionService final : public core::IService
    {
    public:
        explicit SessionService(std::size_t maxMembers) : m_session(maxMembers) {}

        // --- core::IService -------------------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "Session"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override { return {}; }
        [[nodiscard]] core::Expected<void> Initialize() override;
        void Shutdown() noexcept override;

        // --- Session access -------------------------------------------------
        // Named Get() (not Session()) to avoid a member-named-as-type hazard.
        [[nodiscard]] Session& Get() noexcept { return m_session; }
        [[nodiscard]] const Session& Get() const noexcept { return m_session; }

        // Observer subscription passthrough for later sprints.
        void Subscribe(ISessionObserver* observer) { m_session.Subscribe(observer); }

    private:
        Session m_session;
    };
} // namespace stalkermp::net

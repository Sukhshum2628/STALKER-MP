// STALKER-MP — Bubble Manager service (Sprint-004, §7.1; Design Review §5)
//
// Owns the BubbleManager and exposes it to the composition root as a
// ServiceRegistry-managed service, mirroring EntityRegistryService (Sprint-003).
// This step provides lifecycle and ownership ONLY — it does not subscribe to the
// frame dispatcher and performs no per-frame update (a later step).
//
// Ownership: created at the Bootstrap composition root, owned by the
// ServiceRegistry; registered after its dependencies (World, EntityRegistry) and
// after WorldQueryService (which supplies the ISpatialQueries seam), so it
// initializes after them and shuts down before them.
//
// Engine-agnostic: no engine headers, no engine pointers (One Engine Boundary,
// B6). ADR-007-clean.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/interfaces/IService.h"
#include "stalkermp/core/interfaces/ITickable.h"
#include "stalkermp/world/BubbleConfiguration.h"
#include "stalkermp/world/BubbleManager.h" // also forward-declares the seam types

namespace stalkermp::world
{
    class BubbleManagerService final : public core::IService,
                                       public core::ITickable
    {
    public:
        // The seams (both non-owning, engine-free) are injected here and forwarded
        // to the owned BubbleManager. They must outlive the service.
        BubbleManagerService(BubbleConfiguration configuration,
                             const ISpatialQueries* spatialQueries,
                             const IPlayerPositionSource* playerSource);

        // --- core::IService -------------------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "BubbleManager"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override { return {"World", "EntityRegistry"}; }
        [[nodiscard]] core::Expected<void> Initialize() override;
        void Shutdown() noexcept override;

        // --- core::ITickable ------------------------------------------------
        // Runs the bubble activation computation once per simulation frame
        // (subscribed at tick_order::kBubble). Advances a monotonic tick counter
        // and delegates to BubbleManager::Update. No other work.
        void Tick(double deltaSeconds) override;

        // --- Bubble access --------------------------------------------------
        [[nodiscard]] BubbleManager& Manager() noexcept { return m_manager; }
        [[nodiscard]] const BubbleManager& Manager() const noexcept { return m_manager; }
        [[nodiscard]] const BubbleConfiguration& Configuration() const noexcept { return m_manager.Configuration(); }

    private:
        BubbleManager m_manager;

        // Monotonic simulation-frame counter supplied to BubbleManager::Update as
        // the current tick (deterministic; engine-free, no global state).
        std::uint64_t m_tick = 0;
    };
} // namespace stalkermp::world

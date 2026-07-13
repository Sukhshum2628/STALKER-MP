// STALKER-MP — ALife Transition Manager service (Sprint-005, Architecture §6/§14)
//
// Owns the TransitionManager and exposes it to the composition root as a
// ServiceRegistry-managed service, mirroring BubbleManagerService (Sprint-004).
// This step (Step 8) provides lifecycle, ownership, and the per-frame tick ONLY.
// It does NOT subscribe to the FrameDispatcher and is NOT wired into Bootstrap
// (that is Step 10), and it does not construct the real engine gateway (Step 9).
//
// Ownership: created at the Bootstrap composition root (a later step), owned by
// the ServiceRegistry; registered after its dependencies (World, EntityRegistry,
// BubbleManager) so it initializes after them and shuts down before them.
//
// Engine-agnostic: no engine headers, no engine pointers (One Engine Boundary).
// The injected switch gateway is reached only through the engine-free
// IAlifeSwitchGateway seam (ADR-008). ADR-007-clean (no exceptions, Expected<T>).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/interfaces/IService.h"
#include "stalkermp/core/interfaces/ITickable.h"
#include "stalkermp/world/TransitionManager.h" // also forward-declares Bubble/registry

namespace stalkermp::world
{
    class TransitionManagerService final : public core::IService,
                                           public core::ITickable
    {
    public:
        // Dependencies (all non-owning, must outlive the service):
        //   bubble   — Sprint-004 Bubble Manager, read-only transition source.
        //   registry — Sprint-003 Entity Registry, read-only liveness/lookup.
        //   gateway  — engine-free switch seam (the sanctioned mutation path).
        // Forwarded to the owned TransitionManager. Defaulted to nullptr so the
        // service constructs validly before the composition root supplies them.
        TransitionManagerService(const BubbleManager* bubble = nullptr,
                                 const EntityRegistry* registry = nullptr,
                                 IAlifeSwitchGateway* gateway = nullptr);

        // --- core::IService -------------------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "TransitionManager"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override
        {
            return {"World", "EntityRegistry", "BubbleManager"};
        }
        [[nodiscard]] core::Expected<void> Initialize() override;
        void Shutdown() noexcept override;

        // --- core::ITickable ------------------------------------------------
        // Runs one transition-pipeline tick (reconcile -> ingest/dedup -> apply).
        // Advances a monotonic tick counter and delegates to TransitionManager::
        // Update. No other work. Subscribed at tick_order::kAlifeTransition by the
        // composition root in a later step.
        void Tick(double deltaSeconds) override;

        // --- Transition access ----------------------------------------------
        [[nodiscard]] TransitionManager& Manager() noexcept { return m_manager; }
        [[nodiscard]] const TransitionManager& Manager() const noexcept { return m_manager; }

    private:
        TransitionManager m_manager;

        // Monotonic simulation-frame counter passed to TransitionManager::Update
        // as the current tick (deterministic; engine-free, no global state).
        std::uint64_t m_tick = 0;
    };
} // namespace stalkermp::world

// STALKER-MP — World manager (Sprint-002, §7.1/7.2)
//
// Why this exists:
//   Central coordinator of the World subsystem (04_World.md §7): owns the
//   lifecycle state machine, the simulation clock, and the per-tick
//   pipeline that publishes the immutable WorldContext.
//
// Responsibilities:
//   - Lifecycle: Initialize -> Start -> (Pause <-> Resume) -> Shutdown,
//     deterministic, illegal transitions rejected.
//   - Per-tick update (via FrameDispatcher, order tick_order::kWorld):
//     advance clock -> derive time of day -> publish context.
//   - Expose IWorld and IClockService read surfaces.
//
// Ownership / lifetime:
//   Owned by the ServiceRegistry (registered at the Bootstrap composition
//   root). Owns SimulationClock and the current context. Owns no entities,
//   no player state, no engine state.
//
// Threading:
//   Tick and lifecycle run on the engine main thread. Context() returns by
//   value; cross-thread consumption arrives with the snapshot
//   architecture (RFC-0003), not before.

#pragma once

#include <string>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/interfaces/IService.h"
#include "stalkermp/core/interfaces/ITickable.h"
#include "stalkermp/world/IClockService.h"
#include "stalkermp/world/IEnvironmentSource.h"
#include "stalkermp/world/IWorld.h"
#include "stalkermp/world/SimulationClock.h"
#include "stalkermp/world/WorldConfiguration.h"
#include "stalkermp/world/WorldContext.h"

namespace stalkermp::world
{
    // Tick diagnostics (Sprint-002 §7.11/7.13).
    struct WorldTickStatistics
    {
        std::uint64_t ticks = 0;
        double lastTickMilliseconds = 0.0;
        double maxTickMilliseconds = 0.0;
    };

    class WorldManager final : public core::IService,
                               public core::ITickable,
                               public IWorld,
                               public IClockService
    {
    public:
        // environmentSource: optional (nullptr = weather/emission stay at
        // defaults); non-owning, must outlive the manager (both owned by
        // the Bootstrap runtime).
        explicit WorldManager(WorldConfiguration configuration,
                              const IEnvironmentSource* environmentSource = nullptr);

        // --- core::IService -------------------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "World"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override { return {}; }
        [[nodiscard]] core::Expected<void> Initialize() override;
        void Shutdown() noexcept override;

        // --- Lifecycle (composition root only, Sprint-002 §7.2) -------------
        [[nodiscard]] core::Expected<void> Start();
        [[nodiscard]] core::Expected<void> Pause();
        [[nodiscard]] core::Expected<void> Resume();

        // --- core::ITickable (invoked by FrameDispatcher) --------------------
        void Tick(double deltaSeconds) override;

        // --- IWorld ----------------------------------------------------------
        [[nodiscard]] WorldLifecycleState State() const noexcept override { return m_state; }
        [[nodiscard]] WorldContext Context() const noexcept override { return m_context; }

        // --- IClockService ---------------------------------------------------
        [[nodiscard]] std::uint64_t CurrentTick() const noexcept override { return m_clock.Tick(); }
        [[nodiscard]] double SimulationSeconds() const noexcept override { return m_clock.SimulationSeconds(); }
        [[nodiscard]] WorldTimeOfDay TimeOfDay() const noexcept override { return m_context.timeOfDay; }

        [[nodiscard]] const WorldConfiguration& Configuration() const noexcept { return m_configuration; }
        [[nodiscard]] WorldTickStatistics Statistics() const noexcept { return m_statistics; }

    private:
        [[nodiscard]] core::Expected<void> TransitionTo(WorldLifecycleState target);
        void SampleEnvironment();
        void PublishContext(double deltaSeconds);

        WorldConfiguration m_configuration;
        SimulationClock m_clock;
        WorldContext m_context{};
        WorldLifecycleState m_state = WorldLifecycleState::Created;
        const IEnvironmentSource* m_environmentSource = nullptr; // Non-owning.
        WorldTickStatistics m_statistics{};
        WeatherId m_weather{};
        bool m_emissionActive = false;
    };
} // namespace stalkermp::world

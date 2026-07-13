// STALKER-MP — Simulation clock (Sprint-002, §7.3)
//
// Deterministic simulation time: advances only through Advance(dt), scaled
// by the configured simulation speed. Identical dt sequences always
// produce identical simulation time — the clock never reads wall time.
//
// Ownership: owned by WorldManager; other systems observe it through
// IClockService / WorldContext.

#pragma once

#include <cstdint>

#include "stalkermp/core/Assert.h"

namespace stalkermp::world
{
    class SimulationClock
    {
    public:
        explicit SimulationClock(const double simulationSpeed = 1.0) noexcept
            : m_speed(simulationSpeed)
        {
            SMP_ASSERT_MSG(simulationSpeed > 0.0, "SimulationClock: speed must be positive");
        }

        // Advances simulation time by wall-clock deltaSeconds * speed and
        // increments the tick index. deltaSeconds must be >= 0.
        void Advance(const double deltaSeconds) noexcept
        {
            SMP_ASSERT_MSG(deltaSeconds >= 0.0, "SimulationClock::Advance: negative delta");
            m_simulationSeconds += deltaSeconds * m_speed;
            ++m_tick;
        }

        // Increments the tick index without advancing simulation time
        // (used while the world is paused so contexts stay fresh).
        void AdvancePausedTick() noexcept { ++m_tick; }

        [[nodiscard]] std::uint64_t Tick() const noexcept { return m_tick; }
        [[nodiscard]] double SimulationSeconds() const noexcept { return m_simulationSeconds; }
        [[nodiscard]] double Speed() const noexcept { return m_speed; }

    private:
        double m_speed = 1.0;
        double m_simulationSeconds = 0.0;
        std::uint64_t m_tick = 0;
    };
} // namespace stalkermp::world

// STALKER-MP — Clock service interface (Sprint-002, §7.10)
//
// Read-only access to deterministic world time. Backed by WorldManager's
// SimulationClock; no consumer may advance time.

#pragma once

#include <cstdint>

#include "stalkermp/world/WorldTypes.h"

namespace stalkermp::world
{
    class IClockService
    {
    public:
        virtual ~IClockService() = default;

        [[nodiscard]] virtual std::uint64_t CurrentTick() const noexcept = 0;
        [[nodiscard]] virtual double SimulationSeconds() const noexcept = 0;
        [[nodiscard]] virtual WorldTimeOfDay TimeOfDay() const noexcept = 0;
    };
} // namespace stalkermp::world

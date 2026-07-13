// STALKER-MP — World time derivation (Sprint-002, §7.3)
//
// Pure functions deriving in-world time of day from simulation time and
// the configured day length. Stateless and deterministic.

#pragma once

#include <cmath>

#include "stalkermp/core/Assert.h"
#include "stalkermp/world/WorldTypes.h"

namespace stalkermp::world
{
    // dayLengthSeconds: duration of one full in-world day, expressed in
    // simulation seconds. Must be positive.
    [[nodiscard]] inline WorldTimeOfDay DeriveTimeOfDay(const double simulationSeconds,
                                                        const double dayLengthSeconds) noexcept
    {
        SMP_ASSERT_MSG(dayLengthSeconds > 0.0, "DeriveTimeOfDay: day length must be positive");
        const double wrapped = std::fmod(simulationSeconds, dayLengthSeconds);
        return WorldTimeOfDay{wrapped / dayLengthSeconds};
    }
} // namespace stalkermp::world

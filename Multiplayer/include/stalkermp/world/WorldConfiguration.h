// STALKER-MP — World configuration (Sprint-002, §7.9)
//
// Values parsed from the [world] section of server.cfg (the host owns the
// world; RFC-0001). Only keys consumed by current code exist; new keys
// arrive with the sprints that read them.
//
// Ownership: value type, parsed once at the composition root and injected
// into WorldManager.

#pragma once

#include "stalkermp/core/Config.h"
#include "stalkermp/core/Expected.h"

namespace stalkermp::world
{
    struct WorldConfiguration
    {
        // Simulation time multiplier (> 0). 1.0 = real time.
        double simulationSpeed = 1.0;

        // Real-time minutes of one full in-world day at speed 1.0 (> 0).
        double dayLengthMinutes = 90.0;

        // Verbose per-tick world logging (development aid).
        bool debugLogging = false;

        [[nodiscard]] double DayLengthSeconds() const noexcept { return dayLengthMinutes * 60.0; }

        // Parses the [world] section. Missing keys keep defaults; present
        // but invalid values fail with InvalidArgument.
        [[nodiscard]] static core::Expected<WorldConfiguration> FromConfig(const core::ConfigStore& config);
    };
} // namespace stalkermp::world

// STALKER-MP — Bubble Manager configuration (Sprint-004, §7.5; Design Review §3C/§5)
//
// Values parsed from the [bubble] section of server.cfg (the host owns the
// bubble; RFC-0001). Only keys consumed by current code exist; new keys arrive
// with the sprints that read them (WorldConfiguration convention).
//
// The effective activation/deactivation radii encode the frozen hysteresis rule
// (Design Review §3C, invariant B10): an entity turns online inside the smaller
// ActivationRadius() and only turns offline outside the larger DeactivationRadius(),
// so boundary entities do not oscillate.
//
// Ownership: value type, parsed once at the composition root and injected into the
// Bubble Manager. Engine-agnostic; no engine headers; ADR-007-clean.

#pragma once

#include <algorithm>
#include <cstdint>

#include "stalkermp/core/Config.h"
#include "stalkermp/core/Expected.h"

namespace stalkermp::world
{
    struct BubbleConfiguration
    {
        // Base activation radius contributed by each player (world units, > 0).
        double simulationRadius = 150.0;

        // Hysteresis band (>= 0, and deactivationMargin >= activationMargin):
        //   ActivationRadius()   = simulationRadius + activationMargin   (R_on)
        //   DeactivationRadius() = simulationRadius + deactivationMargin (R_off)
        double activationMargin = 10.0;
        double deactivationMargin = 30.0;

        // Upper clamp on the effective radii (world units, > 0).
        double maximumRadius = 500.0;

        // Opaque bubble debug flag bits (§7.5). Bit semantics assigned later.
        std::uint32_t debugFlags = 0;

        // Frozen effective radii (Design Review §3C). Clamped to maximumRadius.
        [[nodiscard]] double ActivationRadius() const noexcept
        {
            return std::min(simulationRadius + activationMargin, maximumRadius);
        }

        [[nodiscard]] double DeactivationRadius() const noexcept
        {
            return std::min(simulationRadius + deactivationMargin, maximumRadius);
        }

        // Parses the [bubble] section. Missing keys keep defaults; present but
        // invalid values (non-positive radii, negative margins, or
        // deactivationMargin < activationMargin) fail with InvalidArgument.
        [[nodiscard]] static core::Expected<BubbleConfiguration> FromConfig(const core::ConfigStore& config);
    };
} // namespace stalkermp::world

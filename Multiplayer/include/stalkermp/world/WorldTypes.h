// STALKER-MP — World value types (Sprint-002, §7.3)
//
// Small deterministic value types shared across the World subsystem.
// No player, networking, or session state may ever appear here
// (Sprint-002 §7.4 note; 04_World.md §8).

#pragma once

#include <cstdint>
#include <string_view>

#include "stalkermp/common/Hash.h"

namespace stalkermp::world
{
    // Lifecycle of the World subsystem (Sprint-002 §7.2).
    enum class WorldLifecycleState : std::uint8_t
    {
        Created = 0,
        Initialized,
        Running,
        Paused,
        ShutDown,
    };

    [[nodiscard]] constexpr const char* WorldLifecycleStateName(const WorldLifecycleState state) noexcept
    {
        switch (state)
        {
        case WorldLifecycleState::Created:     return "Created";
        case WorldLifecycleState::Initialized: return "Initialized";
        case WorldLifecycleState::Running:     return "Running";
        case WorldLifecycleState::Paused:      return "Paused";
        case WorldLifecycleState::ShutDown:    return "ShutDown";
        }
        return "Unknown";
    }

    // Identifies a weather profile by name hash. Profiles are engine data;
    // the world tracks identity only.
    struct WeatherId
    {
        std::uint32_t value = 0; // 0 = unknown / none.

        [[nodiscard]] static constexpr WeatherId FromName(const std::string_view name) noexcept
        {
            return WeatherId{common::Fnv1a32(name)};
        }

        [[nodiscard]] constexpr bool IsUnknown() const noexcept { return value == 0; }
        [[nodiscard]] constexpr bool operator==(const WeatherId& other) const noexcept { return value == other.value; }
        [[nodiscard]] constexpr bool operator!=(const WeatherId& other) const noexcept { return value != other.value; }
    };

    inline constexpr WeatherId kUnknownWeather{};

    // In-world time of day as a fraction of one day [0, 1).
    // 0.0 = 00:00, 0.5 = 12:00.
    struct WorldTimeOfDay
    {
        double dayFraction = 0.0;

        [[nodiscard]] std::uint32_t Hour() const noexcept
        {
            return static_cast<std::uint32_t>(dayFraction * 24.0) % 24u;
        }

        [[nodiscard]] std::uint32_t Minute() const noexcept
        {
            return static_cast<std::uint32_t>(dayFraction * 24.0 * 60.0) % 60u;
        }
    };
} // namespace stalkermp::world

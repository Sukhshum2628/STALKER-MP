// STALKER-MP — Math helpers (Sprint-001, §7.11)
//
// Only helpers with an immediate consumer are defined; geometry and vector
// math remain the engine's responsibility (02_Engine.md — reuse before
// replacement). Clamping uses std::clamp directly (Revision-1 R6 removed
// the redundant local implementation).

#pragma once

#include <cmath>
#include <type_traits>

namespace stalkermp::common
{
    template <typename T>
    [[nodiscard]] constexpr T Lerp(const T a, const T b, const T t) noexcept
    {
        static_assert(std::is_floating_point_v<T>, "Lerp requires a floating-point type");
        return a + (b - a) * t;
    }

    template <typename T>
    [[nodiscard]] bool ApproxEqual(const T a, const T b, const T epsilon = static_cast<T>(1e-6)) noexcept
    {
        static_assert(std::is_floating_point_v<T>, "ApproxEqual requires a floating-point type");
        return std::fabs(a - b) <= epsilon;
    }
} // namespace stalkermp::common

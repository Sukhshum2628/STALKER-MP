// STALKER-MP — Version system (Sprint-001, §7.13)
//
// Why this exists:
//   The module must identify itself (project version), the engine it was
//   built for (compatibility version), and the concrete build (date, git
//   revision) in logs, saves and future network handshakes.
//
// Responsibilities:
//   - SemanticVersion value type.
//   - Compile-time project and compatibility constants.
//   - Runtime engine-compatibility verification.
//
// Ownership / lifetime:
//   Constants only; no runtime state.
//
// Git revision:
//   The build may stamp the revision by defining SMP_GIT_REVISION
//   (scripts/print_git_revision.* emit the value for build integration).
//   Unstamped builds report "unstamped".

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "stalkermp/core/Expected.h"

namespace stalkermp::core
{
    struct SemanticVersion
    {
        std::uint16_t major = 0;
        std::uint16_t minor = 0;
        std::uint16_t patch = 0;

        [[nodiscard]] std::string ToString() const
        {
            return std::to_string(major) + '.' + std::to_string(minor) + '.' + std::to_string(patch);
        }

        [[nodiscard]] constexpr bool operator==(const SemanticVersion& other) const noexcept
        {
            return major == other.major && minor == other.minor && patch == other.patch;
        }

        [[nodiscard]] constexpr bool operator!=(const SemanticVersion& other) const noexcept
        {
            return !(*this == other);
        }
    };

    namespace version
    {
        // STALKER-MP project version.
        inline constexpr SemanticVersion kProject{0, 1, 0};

        // Engine builds whose version string starts with this prefix are
        // considered compatible (see x_ray.cpp: XRAY_MONOLITH_VERSION).
        inline constexpr std::string_view kCompatibleEnginePrefix = "X-Ray Monolith v1.5";

        // Compatibility version for future persistence / network handshakes.
        // Incremented only by an approved RFC change.
        inline constexpr std::uint32_t kCompatibility = 1;

        // Compiler-provided build timestamp.
        [[nodiscard]] std::string_view BuildTimestamp() noexcept;

        // Git revision stamped at build time, or "unstamped".
        [[nodiscard]] std::string_view GitRevision() noexcept;

        // Single-line banner for logs:
        // "STALKER-MP 0.1.0 (compat 1, built Jul  3 2026 12:00:00, rev unstamped)"
        [[nodiscard]] std::string Banner();

        // Verifies that the running engine matches kCompatibleEnginePrefix.
        [[nodiscard]] Expected<void> VerifyEngineCompatibility(std::string_view engineVersion);
    } // namespace version
} // namespace stalkermp::core

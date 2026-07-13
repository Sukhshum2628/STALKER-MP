// STALKER-MP — Bubble Manager configuration parsing (Sprint-004, §7.5)
//
// See BubbleConfiguration.h. Engine-agnostic; no exceptions, no throwing STL.
// Mirrors the WorldConfiguration parsing convention.

#include "stalkermp/world/BubbleConfiguration.h"

#include "stalkermp/common/StringUtil.h" // common::Format (ADR-007-safe)

namespace stalkermp::world
{
    namespace
    {
        constexpr const char* kSection = "bubble";

        // Reads a present key as a double satisfying `valid`; returns an error on a
        // present-but-invalid value. Missing key leaves `target` unchanged.
        [[nodiscard]] core::Expected<void>
        ReadDouble(const core::ConfigStore& config, const char* key, double& target,
                   bool (*valid)(double), const char* requirement)
        {
            if (const auto raw = config.GetString(kSection, key))
            {
                const auto value = config.GetDouble(kSection, key);
                if (!value || !valid(*value))
                {
                    return core::MakeError(core::ErrorCode::InvalidArgument,
                                           common::Format("[bubble] {} '{}' {}", key, *raw, requirement));
                }
                target = *value;
            }
            return core::Success();
        }
    } // namespace

    core::Expected<BubbleConfiguration> BubbleConfiguration::FromConfig(const core::ConfigStore& config)
    {
        BubbleConfiguration result;

        if (!config.HasSection(kSection))
        {
            return result; // all defaults
        }

        if (auto r = ReadDouble(config, "simulation_radius", result.simulationRadius,
                                [](double v) { return v > 0.0; }, "must be a positive number");
            !r)
        {
            return r.GetError();
        }
        if (auto r = ReadDouble(config, "activation_margin", result.activationMargin,
                                [](double v) { return v >= 0.0; }, "must be zero or positive");
            !r)
        {
            return r.GetError();
        }
        if (auto r = ReadDouble(config, "deactivation_margin", result.deactivationMargin,
                                [](double v) { return v >= 0.0; }, "must be zero or positive");
            !r)
        {
            return r.GetError();
        }
        if (auto r = ReadDouble(config, "maximum_radius", result.maximumRadius,
                                [](double v) { return v > 0.0; }, "must be a positive number");
            !r)
        {
            return r.GetError();
        }

        if (const auto flags = config.GetInt(kSection, "debug_flags"))
        {
            if (*flags < 0)
            {
                return core::MakeError(core::ErrorCode::InvalidArgument,
                                       "[bubble] debug_flags must be zero or positive");
            }
            result.debugFlags = static_cast<std::uint32_t>(*flags);
        }

        // Cross-field hysteresis rule (B10): the deactivation band must not be
        // tighter than the activation band, or entities could oscillate.
        if (result.deactivationMargin < result.activationMargin)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "[bubble] deactivation_margin must be >= activation_margin");
        }

        return result;
    }
} // namespace stalkermp::world

// STALKER-MP — Prediction configuration parsing (Sprint-010, Step 2)
//
// See PredictionConfiguration.h. Engine-free / OS-free; no exceptions,
// Expected<T>. Mirrors the Sprint-008/009 *Config::FromConfig parse/validate style.

#include "stalkermp/prediction/PredictionConfiguration.h"

#include <cstdint>

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::prediction
{
    namespace
    {
        constexpr const char* kSection = "prediction";

        [[nodiscard]] core::Expected<void>
        ReadU32(const core::ConfigStore& config, const char* key, std::uint32_t& target, std::int64_t min,
                std::int64_t max)
        {
            if (const auto raw = config.GetInt(kSection, key))
            {
                if (*raw < min || *raw > max)
                {
                    return core::MakeError(
                        core::ErrorCode::InvalidArgument,
                        common::Format("[prediction] {} '{}' must be in [{}, {}]", key, *raw, min, max));
                }
                target = static_cast<std::uint32_t>(*raw);
            }
            return core::Success();
        }
    } // namespace

    core::Expected<PredictionConfiguration> PredictionConfiguration::FromConfig(const core::ConfigStore& config)
    {
        PredictionConfiguration result;

        if (!config.HasSection(kSection))
        {
            return result; // all defaults
        }

        if (auto r = ReadU32(config, "input_buffer_depth", result.inputBufferDepth, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "state_buffer_depth", result.stateBufferDepth, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "interpolation_delay_ticks", result.interpolationDelayTicks, 0, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "max_prediction_ticks", result.maxPredictionTicks, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "position_correction_threshold_mm", result.positionCorrectionThresholdMm, 0,
                             0xFFFFFFFFll);
            !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "rotation_correction_threshold_mrad", result.rotationCorrectionThresholdMrad, 0,
                             0xFFFFFFFFll);
            !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "velocity_correction_threshold_mm", result.velocityCorrectionThresholdMm, 0,
                             0xFFFFFFFFll);
            !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "version", result.version, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }

        // Cross-field rules (defensive; the per-field minimums above already enforce
        // these, but the explicit checks document the invariants).
        if (result.inputBufferDepth < 1 || result.stateBufferDepth < 1 || result.maxPredictionTicks < 1 ||
            result.version < 1)
        {
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                "[prediction] inputBufferDepth/stateBufferDepth/maxPredictionTicks/version must be >= 1");
        }

        return result;
    }
} // namespace stalkermp::prediction

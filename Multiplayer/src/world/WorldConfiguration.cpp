#include "stalkermp/world/WorldConfiguration.h"

#include "stalkermp/common/StringUtil.h"

namespace stalkermp::world
{
    core::Expected<WorldConfiguration> WorldConfiguration::FromConfig(const core::ConfigStore& config)
    {
        WorldConfiguration result;

        if (config.HasSection("world"))
        {
            if (const auto raw = config.GetString("world", "simulation_speed"))
            {
                const auto speed = config.GetDouble("world", "simulation_speed");
                if (!speed || *speed <= 0.0)
                {
                    return core::MakeError(core::ErrorCode::InvalidArgument,
                                           common::Format("[world] simulation_speed '{}' must be a positive number",
                                                          *raw));
                }
                result.simulationSpeed = *speed;
            }

            if (const auto raw = config.GetString("world", "day_length_minutes"))
            {
                const auto dayLength = config.GetDouble("world", "day_length_minutes");
                if (!dayLength || *dayLength <= 0.0)
                {
                    return core::MakeError(core::ErrorCode::InvalidArgument,
                                           common::Format("[world] day_length_minutes '{}' must be a positive number",
                                                          *raw));
                }
                result.dayLengthMinutes = *dayLength;
            }

            if (const auto raw = config.GetString("world", "debug_logging"))
            {
                const auto debug = config.GetBool("world", "debug_logging");
                if (!debug)
                {
                    return core::MakeError(core::ErrorCode::InvalidArgument,
                                           common::Format("[world] debug_logging '{}' must be a boolean", *raw));
                }
                result.debugLogging = *debug;
            }
        }

        return result;
    }
} // namespace stalkermp::world

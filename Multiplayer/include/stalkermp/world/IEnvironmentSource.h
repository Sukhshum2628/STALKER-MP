// STALKER-MP — Environment source seam (Sprint-002, §7.8)
//
// Adapter interface supplying raw environmental state:
//   Sprint-002: EngineEnvironmentSource reads the engine's CEnvironment.
//   Tests: FakeEnvironmentSource.
//
// Known limitation (documented): the engine core exposes the weather
// profile directly; emission (blowout) state is script-driven in Anomaly
// and becomes available with Lua integration (Sprint-013). Until then the
// engine source reports emissionActive = false.

#pragma once

#include <optional>
#include <string>

namespace stalkermp::world
{
    struct EnvironmentSample
    {
        std::string weatherName;    // Engine weather profile name; empty = unknown.
        bool emissionActive = false;
    };

    class IEnvironmentSource
    {
    public:
        virtual ~IEnvironmentSource() = default;

        // nullopt while the environment is unavailable (e.g. main menu,
        // no level loaded). Never throws.
        [[nodiscard]] virtual std::optional<EnvironmentSample> Sample() const = 0;
    };
} // namespace stalkermp::world

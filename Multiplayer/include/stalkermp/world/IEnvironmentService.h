// STALKER-MP — Environment service interface (Sprint-002, §7.8/7.10)
//
// Read-only environmental state derived from the world context.
// Lighting and wind exposure (spec §7.8) is deferred until a consumer
// exists — extending a read-only interface later is non-breaking, whereas
// speculative surface would violate the API-first rule (RFC-0007 §7).

#pragma once

#include "stalkermp/world/WorldTypes.h"

namespace stalkermp::world
{
    class IEnvironmentService
    {
    public:
        virtual ~IEnvironmentService() = default;

        [[nodiscard]] virtual WeatherId Weather() const noexcept = 0;
        [[nodiscard]] virtual WorldTimeOfDay TimeOfDay() const noexcept = 0;
        [[nodiscard]] virtual bool EmissionActive() const noexcept = 0;
    };
} // namespace stalkermp::world

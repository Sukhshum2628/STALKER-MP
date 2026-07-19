// STALKER-MP — Save/Load configuration (Sprint-012, Step 2)
//
// Parsed from the [saveload] section of server.cfg. Missing section => all
// defaults; present-but-invalid value => InvalidArgument (value outcome). The save
// directory is an OPAQUE token — the engine-free core performs NO path logic; only
// the platform backend (Step 9) resolves it. Engine-free and OS-free. ADR-007: no
// exceptions, no RTTI, no iostream; Expected<T> / common::Format.
//
// This file introduces the configuration value + FromConfig only — no serialization,
// loading, restoration, migration, filesystem, or wiring.
//
// Cross-field: maxGenerations >= 1; version >= 1; saveDirectoryToken non-empty.

#pragma once

#include <cstdint>
#include <string>

#include "stalkermp/core/Config.h"   // core::ConfigStore
#include "stalkermp/core/Expected.h" // core::Expected

namespace stalkermp::saveload
{
    struct SaveLoadConfiguration
    {
        // Opaque save-location token; resolved ONLY by the platform backend (no path
        // logic in the engine-free core). Must be non-empty.
        std::string saveDirectoryToken = "saves";

        // Maximum retained save generations per slot (bounds storage). Must be >= 1.
        std::uint32_t maxGenerations = 8;

        // Whether the host attempts recovery from the newest save at startup.
        bool loadOnStartup = true;

        // Whether integrity validation is strict (any failure aborts the load) vs
        // advisory. Policy flag only; consumed by later steps.
        bool strictIntegrity = true;

        // Whether version migration is attempted for compatible-but-older saves.
        bool migrationEnabled = true;

        // Save/load format-policy version stamped into diagnostics (forward-compatible).
        // Must be >= 1. (Distinct from the on-disk SaveFormat version, Step 3.)
        std::uint32_t version = 1;

        // Parses [saveload]. Missing section => defaults; present-but-invalid =>
        // InvalidArgument. Enforces the cross-field rules documented above.
        [[nodiscard]] static core::Expected<SaveLoadConfiguration> FromConfig(const core::ConfigStore& config);
    };
} // namespace stalkermp::saveload

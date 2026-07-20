// STALKER-MP — Lua configuration (Sprint-013, Step 2)
//
// Parsed from the [lua] section of server.cfg. Missing section => all defaults;
// present-but-invalid value => InvalidArgument (value outcome). The script directory
// is an OPAQUE token — the engine-free core performs NO path logic; only the platform
// source backend (later step) resolves it. Engine-free, VM-free, and OS-free.
// ADR-007: no exceptions, no RTTI, no iostream; Expected<T> / common::Format.
//
// This file introduces the configuration value + FromConfig only — no runtime,
// loading, validation orchestration, dispatch, filesystem, or wiring.
//
// Cross-field: maxScripts >= 1; executionBudgetMicros >= 1; recursionLimit >= 1;
// version >= 1; scriptDirectoryToken non-empty.

#pragma once

#include <cstdint>
#include <string>

#include "stalkermp/core/Config.h"   // core::ConfigStore
#include "stalkermp/core/Expected.h" // core::Expected

namespace stalkermp::lua
{
    struct LuaConfiguration
    {
        // Opaque script-location token; resolved ONLY by the platform source backend
        // (no path logic in the engine-free core). Must be non-empty.
        std::string scriptDirectoryToken = "scripts";

        // Maximum number of scripts the subsystem will load (bounds memory). Must be >= 1.
        std::uint32_t maxScripts = 256;

        // Per-invocation execution budget in microseconds (execution-timeout guard,
        // §7.9). Diagnostic/enforcement bound; must be >= 1.
        std::uint32_t executionBudgetMicros = 2000;

        // Maximum script call recursion depth (infinite-recursion guard, §7.9).
        // Must be >= 1.
        std::uint32_t recursionLimit = 64;

        // Whether the scripting subsystem is enabled at all.
        bool enabled = true;

        // Whether each script is validated when loaded (§7.7). Policy flag only.
        bool validateOnLoad = true;

        // Whether script API-version compatibility is enforced strictly. Policy flag only.
        bool strictApiVersion = true;

        // Scripting config-policy version stamped into diagnostics (forward-compatible).
        // Must be >= 1.
        std::uint32_t version = 1;

        // Parses [lua]. Missing section => defaults; present-but-invalid =>
        // InvalidArgument. Enforces the cross-field rules documented above.
        [[nodiscard]] static core::Expected<LuaConfiguration> FromConfig(const core::ConfigStore& config);
    };
} // namespace stalkermp::lua

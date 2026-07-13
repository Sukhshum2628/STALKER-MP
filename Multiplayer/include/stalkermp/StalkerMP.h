// STALKER-MP — Engine boundary (Sprint-001, §7.7)
//
// Why this exists:
//   This is the single header the X-Ray Engine includes. It exposes the
//   module's initialization pipeline behind a minimal, exception-safe
//   surface so engine code (which has no exception discipline of its own)
//   never observes a throw from STALKER-MP.
//
// Responsibilities:
//   - Initialize: logging -> configuration -> engine compatibility check ->
//     service registry -> module registration.
//   - Shutdown: reverse order, always completes.
//
// Ownership / lifetime:
//   Initialize creates the module runtime; Shutdown destroys it. The engine
//   calls Initialize once after InitEngine() and Shutdown once before
//   destroyEngine() (xrEngine/x_ray.cpp).
//
// Failure policy:
//   A failed Initialize leaves the engine fully functional as single-player
//   Anomaly: the failure is logged and false is returned; the engine
//   continues without multiplayer infrastructure. This mirrors the
//   isolation principle of 02_Engine.md §19.4.

#pragma once

#include <string>

namespace stalkermp
{
    struct InitializeParams
    {
        // Directory for STALKER-MP configuration files (server.cfg,
        // client.cfg, development.cfg). Created if missing.
        std::string configDirectory;

        // Directory for the module log file. Created if missing.
        std::string logDirectory;

        // Engine version string (XRAY_MONOLITH_VERSION) used for the
        // compatibility check.
        std::string engineVersion;
    };

    // Initializes the STALKER-MP runtime. Returns true on success.
    // On failure the reason is written to the module log (and to stderr if
    // the log could not be created); the engine must continue running.
    // Calling Initialize when already initialized logs a warning and
    // returns true (the running instance is preserved).
    bool Initialize(const InitializeParams& params) noexcept;

    // Destroys the STALKER-MP runtime. Safe to call when not initialized.
    void Shutdown() noexcept;

    [[nodiscard]] bool IsInitialized() noexcept;

    // "STALKER-MP 0.1.0 (compat 1, ...)" — valid for the process lifetime.
    [[nodiscard]] const char* VersionString() noexcept;
} // namespace stalkermp

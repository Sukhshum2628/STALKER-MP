// STALKER-MP — Platform script-source factory (Sprint-013, Step 8)
//
// The engine-free factory for the real filesystem-backed script source. The concrete
// implementation lives in the single sanctioned platform TU (PlatformScriptStore.cpp),
// the ONLY script-side place that performs filesystem I/O (ADR-009 / Singular Platform
// Boundary), mirroring PlatformSaveStore.cpp / PlatformSockets.cpp. No OS/file header
// is exposed here; callers see only the engine-free `lua::IScriptSource` seam.
//
// This header declares the factory ONLY — no VM, no engine, no loading orchestration.
//
// Engine-free / VM-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <memory>
#include <string>

#include "stalkermp/lua/IScriptSource.h" // lua::IScriptSource

namespace stalkermp::adapters
{
    // A read-only filesystem-backed `IScriptSource` over the opaque
    // `scriptDirectoryToken`. Enumeration is deterministic (ascending by derived id).
    // A missing/empty directory yields an inert source that enumerates nothing (a
    // read source never creates or mutates the directory). Never throws.
    [[nodiscard]] std::unique_ptr<lua::IScriptSource>
    CreatePlatformScriptSource(const std::string& scriptDirectoryToken);
} // namespace stalkermp::adapters

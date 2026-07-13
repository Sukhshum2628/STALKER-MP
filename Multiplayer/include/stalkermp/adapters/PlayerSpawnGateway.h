// STALKER-MP — Player spawn gateway factory (Sprint-007, Step 9)
//
// Engine-free / OS-free header declaring the factory for the player spawn gateway.
// The engine implementation lives in EngineAdapters.cpp (the ONLY engine-header TU
// — One Engine Boundary; delivered in Step 10). The test build links
// tests/support/NullPlayerSpawnGateway.cpp (an inert, deterministic counterpart).
// No engine type crosses the engine-free player::IPlayerSpawnGateway seam.

#pragma once

#include <memory>

#include "stalkermp/player/IPlayerSpawnGateway.h"

namespace stalkermp::adapters
{
    [[nodiscard]] std::unique_ptr<player::IPlayerSpawnGateway> CreatePlayerSpawnGateway();
} // namespace stalkermp::adapters

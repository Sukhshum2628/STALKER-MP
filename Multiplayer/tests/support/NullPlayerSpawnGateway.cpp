// STALKER-MP — Null player spawn gateway (test build only, Sprint-007, Step 9)
//
// Test-build counterpart of the engine adapter (EnginePlayerSpawnGateway, Step 10):
// provides the CreatePlayerSpawnGateway factory WITHOUT any engine or OS
// dependency (One Engine Boundary). Spawn mints deterministic ascending synthetic
// EntityIds (replay-stable); Despawn is a benign value outcome. Every engine-free
// step links this; the engine adapter is never compiled into the test build.

#include "stalkermp/adapters/PlayerSpawnGateway.h"

#include <cstdint>
#include <memory>

#include "stalkermp/core/Expected.h"
#include "stalkermp/player/IPlayerSpawnGateway.h"

namespace stalkermp::adapters
{
    namespace
    {
        // Inert, deterministic spawn gateway. No engine, no OS.
        class NullPlayerSpawnGateway final : public player::IPlayerSpawnGateway
        {
        public:
            [[nodiscard]] core::Expected<world::EntityId> Spawn(const player::PlayerProfile& /*profile*/,
                                                                const world::PlayerPosition& /*position*/) override
            {
                // Deterministic ascending synthetic id (never 0), preserving replay
                // determinism in the engine-free test build.
                return world::EntityId{m_nextEntity++};
            }

            [[nodiscard]] player::SpawnOutcome Despawn(world::EntityId /*entity*/) override
            {
                return player::SpawnOutcome::Spawned; // benign success; no engine object to remove
            }

        private:
            std::uint32_t m_nextEntity = 1; // ascending, non-zero
        };
    } // namespace

    std::unique_ptr<player::IPlayerSpawnGateway> CreatePlayerSpawnGateway()
    {
        return std::make_unique<NullPlayerSpawnGateway>();
    }
} // namespace stalkermp::adapters

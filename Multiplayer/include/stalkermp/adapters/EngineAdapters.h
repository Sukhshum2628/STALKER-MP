// STALKER-MP — Engine adapter factories (Sprint-002, adapter layer)
//
// The single engine-free header through which the Bootstrap composition
// root obtains engine-facing adapters. Two implementations of this header
// exist, selected by build target:
//
//   src/adapters/EngineAdapters.cpp   — real engine adapters; compiles
//                                       with engine headers; xrMP only.
//   tests/support/NullEngineAdapters.cpp — inert stand-ins; test build
//                                       only (no engine available).
//
// This seam keeps Bootstrap.cpp and every module header engine-free while
// letting the module run inside the engine and inside the test runner
// from identical composition-root code.

#pragma once

#include <memory>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/FrameDispatcher.h"
#include "stalkermp/core/interfaces/ITickable.h"
#include "stalkermp/adapters/PlatformTransport.h" // adapters::CreatePlatformTransport (Sprint-006)
#include "stalkermp/world/EntityRegistry.h"
#include "stalkermp/world/IAlifeSwitchGateway.h"
#include "stalkermp/world/IEntitySource.h"
#include "stalkermp/world/IEnvironmentSource.h"

namespace stalkermp::adapters
{
    // Owns the module's single engine frame registration
    // (Device.seqFrame, REG_PRIORITY_LOW). Destruction unregisters; the
    // Bootstrap destroys the bridge FIRST during shutdown so no engine
    // callback can reach a shutting-down module.
    class IFrameBridge
    {
    public:
        virtual ~IFrameBridge() = default;
    };

    // Entity view over the engine's live object list (empty while no
    // level is loaded). Never fails; availability is dynamic.
    [[nodiscard]] std::unique_ptr<world::IEntitySource> CreateEngineEntitySource();

    // Environment sampler over the engine's CEnvironment (nullopt samples
    // while unavailable).
    [[nodiscard]] std::unique_ptr<world::IEnvironmentSource> CreateEngineEnvironmentSource();

    // Registers the frame observer feeding dispatcher. Fails if the engine
    // frame sequence is unavailable.
    [[nodiscard]] core::Expected<std::unique_ptr<IFrameBridge>>
    CreateEngineFrameBridge(core::FrameDispatcher& dispatcher);

    // Feeds the EntityRegistry from the engine's live object list, ticked once
    // per frame at tick_order::kEntityRegistry (Sprint-003 engine feed; Design
    // Review §5; constraints C1-C4). This step implements the creation side:
    // reconciliation registers engine objects not yet in the registry
    // (delta-only, C3), applies registrations deterministically in ascending
    // EntityId order (C1), captures only ids/names by value and never stores
    // CObject* (C2), and no-ops while no level is loaded (C4). Destruction via
    // relcase is a later step.
    class IEntityFeed : public core::ITickable
    {
    public:
        ~IEntityFeed() override = default;
    };

    [[nodiscard]] std::unique_ptr<IEntityFeed> CreateEngineEntityFeed(world::EntityRegistry& registry);

    // ALife switch gateway behind the engine-free IAlifeSwitchGateway seam
    // (Sprint-005, Architecture §15). The real implementation (EngineAdapters.cpp)
    // drives vanilla ALife via the frozen Cooperative Flag Override
    // (CALifeUpdateManager::set_switch_online/offline), resolving EntityId to the
    // ALife object on demand with no retained pointer or cache (ADR-008, E-G2).
    // The test build (NullEngineAdapters.cpp) returns an inert stand-in.
    [[nodiscard]] std::unique_ptr<world::IAlifeSwitchGateway> CreateEngineAlifeSwitchGateway();

    // CreatePlatformTransport is declared in PlatformTransport.h (included above).
} // namespace stalkermp::adapters

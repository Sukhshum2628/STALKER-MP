// STALKER-MP — Null engine adapters (test build only)
//
// Test-build counterpart of src/adapters/EngineAdapters.cpp: provides the
// same factory symbols without any engine dependency so the Bootstrap
// composition root is identical in both builds. Bootstrap-level tests
// drive frames manually through detail::GetModuleFrameDispatcher();
// query/environment tests inject fakes directly.

#include "stalkermp/adapters/EngineAdapters.h"

#include "stalkermp/adapters/NullAlifeSwitchGateway.h"

namespace stalkermp::adapters
{
    namespace
    {
        class NullEntitySource final : public world::IEntitySource
        {
        public:
            [[nodiscard]] std::size_t Count() const override { return 0; }
            void Enumerate(const std::function<void(const world::EntityView&)>&) const override {}
            [[nodiscard]] std::optional<world::EntityView> FindById(world::EntityId) const override
            {
                return std::nullopt;
            }
            [[nodiscard]] std::optional<world::EntityView> FindByName(std::string_view) const override
            {
                return std::nullopt;
            }
        };

        class NullEnvironmentSource final : public world::IEnvironmentSource
        {
        public:
            [[nodiscard]] std::optional<world::EnvironmentSample> Sample() const override
            {
                return std::nullopt;
            }
        };

        class NullFrameBridge final : public IFrameBridge
        {
        };

        class NullEntityFeed final : public IEntityFeed
        {
        public:
            void Tick(double) override {}
        };
    } // namespace

    std::unique_ptr<world::IEntitySource> CreateEngineEntitySource()
    {
        return std::make_unique<NullEntitySource>();
    }

    std::unique_ptr<world::IEnvironmentSource> CreateEngineEnvironmentSource()
    {
        return std::make_unique<NullEnvironmentSource>();
    }

    core::Expected<std::unique_ptr<IFrameBridge>>
    CreateEngineFrameBridge(core::FrameDispatcher&)
    {
        return std::unique_ptr<IFrameBridge>(std::make_unique<NullFrameBridge>());
    }

    std::unique_ptr<IEntityFeed> CreateEngineEntityFeed(world::EntityRegistry&)
    {
        return std::make_unique<NullEntityFeed>();
    }

    std::unique_ptr<world::IAlifeSwitchGateway> CreateEngineAlifeSwitchGateway()
    {
        // Inert stand-in for the engine-free test build (no ALife available).
        return std::make_unique<NullAlifeSwitchGateway>();
    }

    // CreatePlatformTransport is defined in tests/support/NullPlatformTransport.cpp
    // for the test build (Step 13).
} // namespace stalkermp::adapters

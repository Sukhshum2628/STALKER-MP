#include "stalkermp/world/WorldManager.h"

#include "stalkermp/core/Assert.h"
#include "stalkermp/core/Log.h"
#include "stalkermp/common/StringUtil.h"
#include "stalkermp/common/Time.h"
#include "stalkermp/world/WorldTime.h"

namespace stalkermp::world
{
    namespace
    {
        constexpr std::string_view kLogCategory = "World";

        // The manager is testable standalone (no Bootstrap, no module
        // logger), so every log call is availability-guarded.
        void LogInfo(const std::string& message)
        {
            if (core::IsLogAvailable())
            {
                core::Log().Info(kLogCategory, message);
            }
        }

        void LogVerbose(const std::string& message)
        {
            if (core::IsLogAvailable())
            {
                core::Log().Verbose(kLogCategory, message);
            }
        }

        [[nodiscard]] bool IsLegalTransition(const WorldLifecycleState from,
                                             const WorldLifecycleState to) noexcept
        {
            switch (to)
            {
            case WorldLifecycleState::Initialized:
                return from == WorldLifecycleState::Created;
            case WorldLifecycleState::Running:
                return from == WorldLifecycleState::Initialized || from == WorldLifecycleState::Paused;
            case WorldLifecycleState::Paused:
                return from == WorldLifecycleState::Running;
            case WorldLifecycleState::ShutDown:
                // Shutdown is legal from any state except ShutDown itself;
                // ShutdownAll() may run against a never-initialized service.
                return from != WorldLifecycleState::ShutDown;
            case WorldLifecycleState::Created:
                return false;
            }
            return false;
        }
    } // namespace

    WorldManager::WorldManager(WorldConfiguration configuration,
                               const IEnvironmentSource* environmentSource)
        : m_configuration(configuration)
        , m_clock(configuration.simulationSpeed)
        , m_environmentSource(environmentSource)
    {
    }

    core::Expected<void> WorldManager::TransitionTo(const WorldLifecycleState target)
    {
        if (!IsLegalTransition(m_state, target))
        {
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                common::Format("World lifecycle: illegal transition {} -> {}",
                               WorldLifecycleStateName(m_state), WorldLifecycleStateName(target)));
        }

        if (core::IsLogAvailable())
        {
            core::Log().Debug(kLogCategory,
                              common::Format("Lifecycle: {} -> {}",
                                             WorldLifecycleStateName(m_state),
                                             WorldLifecycleStateName(target)));
        }
        m_state = target;
        return core::Success();
    }

    core::Expected<void> WorldManager::Initialize()
    {
        if (auto transition = TransitionTo(WorldLifecycleState::Initialized); !transition)
        {
            return transition;
        }

        LogInfo(common::Format("World initialized (speed {}, day length {} min)",
                                m_configuration.simulationSpeed,
                                m_configuration.dayLengthMinutes));
        return core::Success();
    }

    core::Expected<void> WorldManager::Start()
    {
        // Start and Resume both target Running; each is legal from exactly
        // one source state, which the transition table alone cannot
        // distinguish — enforce the source state here.
        if (m_state != WorldLifecycleState::Initialized)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   common::Format("World lifecycle: Start requires Initialized, state is {}",
                                                  WorldLifecycleStateName(m_state)));
        }
        if (auto transition = TransitionTo(WorldLifecycleState::Running); !transition)
        {
            return transition;
        }
        LogInfo("World simulation running");
        return core::Success();
    }

    core::Expected<void> WorldManager::Pause()
    {
        return TransitionTo(WorldLifecycleState::Paused);
    }

    core::Expected<void> WorldManager::Resume()
    {
        if (m_state != WorldLifecycleState::Paused)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   common::Format("World lifecycle: Resume requires Paused, state is {}",
                                                  WorldLifecycleStateName(m_state)));
        }
        return TransitionTo(WorldLifecycleState::Running);
    }

    void WorldManager::Shutdown() noexcept
    {
        if (m_state == WorldLifecycleState::ShutDown)
        {
            return;
        }
        m_state = WorldLifecycleState::ShutDown;
        LogInfo(common::Format("World shut down ({} ticks, max tick {} ms)",
                               m_statistics.ticks, m_statistics.maxTickMilliseconds));
    }

    void WorldManager::Tick(const double deltaSeconds)
    {
        SMP_ASSERT_MSG(m_state == WorldLifecycleState::Running || m_state == WorldLifecycleState::Paused,
                       "WorldManager::Tick outside Running/Paused");
        if (m_state != WorldLifecycleState::Running && m_state != WorldLifecycleState::Paused)
        {
            return; // Release builds: ignore ticks in illegal states, reported above.
        }

        const common::Stopwatch tickTimer;

        if (m_state == WorldLifecycleState::Running)
        {
            m_clock.Advance(deltaSeconds);
        }
        else
        {
            m_clock.AdvancePausedTick(); // Contexts stay fresh; time stands still.
        }

        SampleEnvironment();
        PublishContext(deltaSeconds);

        ++m_statistics.ticks;
        m_statistics.lastTickMilliseconds = tickTimer.ElapsedMilliseconds();
        if (m_statistics.lastTickMilliseconds > m_statistics.maxTickMilliseconds)
        {
            m_statistics.maxTickMilliseconds = m_statistics.lastTickMilliseconds;
        }

        if (m_configuration.debugLogging)
        {
            LogVerbose(common::Format("tick {} dt {} sim {}", m_context.tick,
                                      m_context.deltaSeconds, m_context.simulationSeconds));
        }
    }

    void WorldManager::SampleEnvironment()
    {
        if (m_environmentSource == nullptr)
        {
            return; // Defaults remain (standalone/test operation).
        }

        const auto sample = m_environmentSource->Sample();
        if (!sample)
        {
            return; // Environment unavailable (e.g. main menu); keep last state.
        }

        m_weather = sample->weatherName.empty() ? kUnknownWeather
                                                : WeatherId::FromName(sample->weatherName);
        m_emissionActive = sample->emissionActive;
    }

    void WorldManager::PublishContext(const double deltaSeconds)
    {
        WorldContext context;
        context.tick = m_clock.Tick();
        context.deltaSeconds = deltaSeconds;
        context.simulationSeconds = m_clock.SimulationSeconds();
        context.timeOfDay = DeriveTimeOfDay(m_clock.SimulationSeconds(), m_configuration.DayLengthSeconds());
        context.weather = m_weather;
        context.emissionActive = m_emissionActive;
        m_context = context;
    }
} // namespace stalkermp::world

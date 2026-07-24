// STALKER-MP — Engine bootstrap (Sprint-001, §7.7)
//
// Why this exists:
//   One place owns module startup and teardown. The pipeline order is:
//     1. Logging            (everything after this is observable)
//     2. Configuration      (server.cfg, client.cfg, development.cfg)
//     3. Engine compatibility check
//     4. Service registry   (created, validated, initialized)
//     5. Module registration (declared future modules, §7.10)
//   Shutdown runs strictly in reverse.
//
// Ownership / lifetime:
//   Runtime aggregates every module-lifetime object (logger, configs,
//   registries). Exactly one Runtime exists between Initialize and
//   Shutdown; it is the single deliberate piece of module-global state,
//   created and destroyed only here.
//
// Failure policy:
//   Initialize never throws across the engine boundary. On failure the
//   partially constructed runtime is destroyed, the reason is logged (or
//   written to stderr if logging never came up), and false is returned.
//   The engine continues as single-player Anomaly (02_Engine.md §19.4).

#include "stalkermp/StalkerMP.h"

#include <cstdint>
#include <cstdio>
#include <exception>
#include <memory>
#include <string_view>
#include <vector>

#include "stalkermp/core/Config.h"
#include "stalkermp/core/Expected.h"
#include "stalkermp/core/FrameDispatcher.h"
#include "stalkermp/core/Log.h"
#include "stalkermp/core/ModuleRegistry.h"
#include "stalkermp/core/ServiceRegistry.h"
#include "stalkermp/core/Version.h"
#include "stalkermp/adapters/EngineAdapters.h"
#include "stalkermp/common/FileSystemUtil.h"
#include "stalkermp/common/StringUtil.h"
#include "stalkermp/world/BubbleConfiguration.h"
#include "stalkermp/world/BubbleManagerService.h"
#include "stalkermp/world/EntityRegistryService.h"
#include "stalkermp/world/EnvironmentService.h"
#include "stalkermp/world/IAlifeSwitchGateway.h"
#include "stalkermp/world/LocalPlayerPositionSource.h"
#include "stalkermp/world/TransitionManagerService.h"
#include "stalkermp/net/IConnectionAuthenticator.h"
#include "stalkermp/net/MessageRegistryService.h"
#include "stalkermp/net/NetworkingConfig.h"
#include "stalkermp/net/NetworkingService.h"
#include "stalkermp/net/SessionService.h"
#include "stalkermp/net/TransportConfig.h"
#include "stalkermp/net/TransportService.h"
#include "stalkermp/adapters/PlayerSpawnGateway.h"
#include "stalkermp/player/IPlayerSpawnGateway.h"
#include "stalkermp/player/NetworkedPlayerPositionSource.h"
#include "stalkermp/player/PlayerConfiguration.h"
#include "stalkermp/player/PlayerDeltaQueue.h"
#include "stalkermp/player/PlayerManager.h"
#include "stalkermp/player/PlayerManagerService.h"
#include "stalkermp/adapters/EntitySnapshotSource.h"     // engine-free factory decl (Sprint-008)
#include "stalkermp/snapshot/SnapshotConfiguration.h"
#include "stalkermp/snapshot/SnapshotManager.h"
#include "stalkermp/world/IEntitySnapshotSource.h"
#include "stalkermp/replication/BubbleInterestPolicy.h"    // Sprint-009 replication wiring
#include "stalkermp/replication/BubbleMembershipAdapter.h"
#include "stalkermp/replication/IBubbleMembershipSource.h"
#include "stalkermp/replication/IInterestPolicy.h"
#include "stalkermp/replication/ReplicationClientRegistry.h"
#include "stalkermp/replication/ReplicationConfiguration.h"
#include "stalkermp/replication/ReplicationManager.h"
#include "stalkermp/persistence/IPersistenceStore.h"
#include "stalkermp/persistence/PersistenceConfiguration.h"
#include "stalkermp/persistence/PersistenceManager.h"
// Save/Load System (Sprint-012, Step 17) — composition-root wiring: build-selected
// save backend + restoration write boundary + startup recovery (before networking).
#include "stalkermp/adapters/PlatformSaveStore.h"          // adapters::ISaveBackend
#include "stalkermp/adapters/SaveLoadSeams.h"              // CreateEngineSaveBackend / CreateEngineRestoreSinks
#include "stalkermp/saveload/IRestoreSinkBundle.h"         // saveload::IRestoreSinkBundle
#include "stalkermp/saveload/RecoveryPipeline.h"           // saveload::RecoveryPipeline
#include "stalkermp/saveload/SaveFormat.h"                 // saveload::kSaveFormatVersion (recovery target)
#include "stalkermp/saveload/SaveLoadConfiguration.h"      // saveload::SaveLoadConfiguration
#include "stalkermp/saveload/SaveMigrator.h"               // saveload::SaveMigrator
// Lua Integration (Sprint-013, Step 17) — composition-root wiring: build-selected Lua
// runtime + Public API facades + script source; ScriptManager at kScripting = 375.
#include "stalkermp/adapters/LuaSeams.h"                   // CreateEngineLuaRuntime / ScriptApi / ScriptSource
#include "stalkermp/lua/ILuaApiBundle.h"                   // lua::ILuaApiBundle
#include "stalkermp/lua/IScriptSource.h"                   // lua::IScriptSource
#include "stalkermp/lua/LuaConfiguration.h"                // lua::LuaConfiguration
#include "stalkermp/lua/LuaDiagnostics.h"                  // lua::LuaDiagnostics
#include "stalkermp/lua/LuaManager.h"                      // lua::LuaManager
#include "stalkermp/lua/ScriptManager.h"                   // lua::ScriptManager
#include "stalkermp/lua/ScriptThreadGuard.h"               // lua::ScriptThreadGuard
// Plugin Framework (Sprint-014, Step 17) — composition-root wiring: static-registration
// discovery backend ([AR-P1] Option B) + reused Sprint-013 gameplay host facades ([AR-P3]
// Option A); PluginManager at the reserved kPlugins = 700.
#include "stalkermp/adapters/PlatformPluginStore.h"        // CreatePlatformPluginSource ([AR-P1] Option B)
#include "stalkermp/plugin/IPluginSource.h"                // plugin::IPluginSource
#include "stalkermp/plugin/PluginConfiguration.h"          // plugin::PluginConfiguration
#include "stalkermp/plugin/PluginDiagnostics.h"            // plugin::PluginDiagnostics
#include "stalkermp/plugin/PluginHostSurface.h"            // plugin::GameplayHostSurface ([AR-P3] Option A)
#include "stalkermp/plugin/PluginManager.h"                // plugin::PluginManager
#include "stalkermp/plugin/PluginThreadGuard.h"            // plugin::PluginThreadGuard
#include "stalkermp/world/WorldConfiguration.h"
#include "stalkermp/world/WorldManager.h"
#include "stalkermp/world/WorldModule.h"
#include "stalkermp/world/WorldQueryService.h"
#include "stalkermp/adapters/PredictionSeams.h"                 // Sprint-010 prediction seam factories
#include "stalkermp/prediction/ClientPresentationDriver.h"      // Sprint-010 client-presentation driver
#include "stalkermp/prediction/ClientPresentationPhase.h"       // Sprint-010 post-dispatch phase hook
#include "stalkermp/prediction/IAuthoritativeStateSource.h"
#include "stalkermp/prediction/PredictionConfiguration.h"

namespace stalkermp
{
    namespace
    {
        using namespace core;

        constexpr std::string_view kLogCategory = "Bootstrap";
        constexpr std::string_view kLogFileName = "stalkermp.log";

        // Default configuration file contents written on first run.
        // Every key is read by current code; new keys arrive with the
        // sprints that consume them.
        constexpr std::string_view kDefaultDevelopmentConfig =
            "; STALKER-MP development configuration\n"
            "; Generated with defaults on first run. Safe to edit.\n"
            "\n"
            "[meta]\n"
            "config_version = 1\n"
            "\n"
            "[log]\n"
            "; minimum level written to sinks: verbose, debug, info, warning, error\n"
            "level = info\n";

        constexpr std::string_view kDefaultServerConfig =
            "; STALKER-MP host (server) configuration\n"
            "; Generated with defaults on first run. Safe to edit.\n"
            "; Host gameplay settings arrive with Sprint-006 (Host Networking).\n"
            "\n"
            "[meta]\n"
            "config_version = 1\n"
            "\n"
            "[world]\n"
            "; simulation time multiplier (positive; 1.0 = real time)\n"
            "simulation_speed = 1.0\n"
            "; real-time minutes per in-world day at speed 1.0\n"
            "day_length_minutes = 90\n"
            "; verbose per-tick world logging (development aid)\n"
            "debug_logging = false\n"
            "\n"
            "; Persistence Framework (Sprint-011). Queue/interval/backoff are tick or\n"
            "; entry counts; autosave 0 = disabled. All optional (defaults shown).\n"
            "[persistence]\n"
            "queue_depth = 16\n"
            "autosave_interval_ticks = 0\n"
            "max_retries = 3\n"
            "retry_backoff_ticks = 30\n"
            "backpressure_high_watermark = 12\n"
            "backpressure_low_watermark = 4\n"
            "version = 1\n"
            "\n"
            "; Plugin Framework (Sprint-014). Static in-process registration only ([AR-P1]\n"
            "; Option B); dynamic library loading is deferred. max_plugins >= 1. All optional\n"
            "; (defaults shown).\n"
            "[plugins]\n"
            "plugin_directory = plugins\n"
            "max_plugins = 64\n"
            "enabled = true\n"
            "validate_on_load = true\n"
            "strict_api_version = true\n"
            "version = 1\n";

        constexpr std::string_view kDefaultClientConfig =
            "; STALKER-MP client configuration\n"
            "; Generated with defaults on first run. Safe to edit.\n"
            "; Client settings arrive with Sprint-007 (Player Connections).\n"
            "\n"
            "[meta]\n"
            "config_version = 1\n"
            "\n"
            "; Client prediction & interpolation (Sprint-010). Depths/delays are tick\n"
            "; counts; correction thresholds are fixed-point (mm / mrad). All optional\n"
            "; (defaults shown). Ignored on the host (identity mode).\n"
            "[prediction]\n"
            "input_buffer_depth = 128\n"
            "state_buffer_depth = 128\n"
            "interpolation_delay_ticks = 2\n"
            "max_prediction_ticks = 8\n"
            "position_correction_threshold_mm = 250\n"
            "rotation_correction_threshold_mrad = 100\n"
            "velocity_correction_threshold_mm = 500\n"
            "version = 1\n";

        struct Runtime
        {
            Logger logger;
            ConfigStore developmentConfig;
            ConfigStore serverConfig;
            ConfigStore clientConfig;
            FrameDispatcher frameDispatcher;
            ServiceRegistry services;
            ModuleRegistry modules;
            std::string versionString;

            // Engine adapters (real in the engine build, null in tests).
            // Declared before the registries so they outlive every service
            // referencing them (members destroy in reverse order).
            std::unique_ptr<world::IEntitySource> entitySource;
            std::unique_ptr<world::IEnvironmentSource> environmentSource;

            // Entity snapshot capture source (Sprint-008, Step 9): the engine
            // adapter in the engine build (CreateEngineEntitySnapshotSource) and the
            // null source in tests. Owned here so it outlives the SnapshotManager
            // (owned by the ServiceRegistry) that references it by const reference.
            std::unique_ptr<world::IEntitySnapshotSource> entitySnapshotSource;

            // ALife switch gateway (Sprint-005). Engine-free seam; the real gateway
            // in the engine build (CreateEngineAlifeSwitchGateway) and the null
            // adapter in tests. Owned here so it outlives the TransitionManagerService,
            // which references it through the TransitionManager (One Engine Boundary:
            // the module only ever holds the engine-free interface).
            std::unique_ptr<world::IAlifeSwitchGateway> transitionGateway;

            // Local player-position source (Sprint-004). Positions-only; owned
            // here and injected into the Bubble Manager. Replaced by a networked
            // source in a later sprint behind the same seam.
            std::unique_ptr<world::LocalPlayerPositionSource> playerPositionSource;

            // The module's single engine frame registration. Created last
            // during Initialize, destroyed first during Shutdown.
            std::unique_ptr<adapters::IFrameBridge> frameBridge;

            // Engine feed for the Entity Registry (Sprint-003). Owned here;
            // subscribed to the dispatcher at tick_order::kEntityRegistry. Torn
            // down (unsubscribe + reset) after the frame bridge and before the
            // ServiceRegistry that owns the registry it references.
            std::unique_ptr<adapters::IEntityFeed> entityFeed;

            // Non-owning; the ServiceRegistry owns the manager. Cached for
            // dispatcher wiring and lifecycle control at the composition root.
            world::WorldManager* worldManager = nullptr;

            // Non-owning; the ServiceRegistry owns the service. Cached so it can be
            // subscribed to / unsubscribed from the frame dispatcher (Sprint-004).
            world::BubbleManagerService* bubbleManagerService = nullptr;

            // Non-owning; the ServiceRegistry owns the service. Cached so it can be
            // subscribed to / unsubscribed from the frame dispatcher (Sprint-005).
            world::TransitionManagerService* transitionManagerService = nullptr;

            // Host networking (Sprint-006). The authenticator is Runtime-owned so
            // it outlives the NetworkingService that references it. The transport
            // is owned by the TransportService (in the ServiceRegistry); the four
            // networking services are owned by the ServiceRegistry. Only the
            // NetworkingService ticks (subscribed at tick_order::kNetworking).
            net::AllowAllAuthenticator networkAuthenticator;
            net::NetworkingService* networkingService = nullptr;

            // Player Lifecycle (Sprint-007). The spawn gateway (engine or null),
            // the PlayerManager, its delta queue, and the networked position source
            // are Runtime-owned so the Bubble can bind the networked source at
            // construction (it precedes the late-registered PlayerManagerService).
            // The service (owned by the ServiceRegistry) is cached here for frame
            // subscription/teardown. All engine-free.
            std::unique_ptr<player::IPlayerSpawnGateway> playerSpawnGateway;
            std::unique_ptr<player::PlayerDeltaQueue> playerDeltaQueue;
            std::unique_ptr<player::PlayerManager> playerManager;
            std::unique_ptr<player::NetworkedPlayerPositionSource> networkedPlayerSource;
            player::PlayerManagerService* playerManagerService = nullptr;

            // Snapshot System (Sprint-008). The SnapshotManager (IService +
            // ITickable) is owned by the ServiceRegistry; cached here so it can be
            // subscribed to / unsubscribed from the frame dispatcher at
            // tick_order::kReplication = 400.
            snapshot::SnapshotManager* snapshotManager = nullptr;

            // Replication Pipeline (Sprint-009). The client registry, the Bubble
            // membership adapter, and the interest policy are Runtime-owned so they
            // outlive the ServiceRegistry-owned ReplicationManager that references
            // them. Only the ReplicationManager ticks (at kReplicationPipeline = 450);
            // cached here for frame subscription/teardown. All engine-free; no transport.
            std::unique_ptr<replication::ReplicationClientRegistry> replicationClients;
            std::unique_ptr<replication::IBubbleMembershipSource> bubbleMembership;
            std::unique_ptr<replication::IInterestPolicy> interestPolicy;
            replication::ReplicationManager* replicationManager = nullptr;

            // Client Prediction & Interpolation (Sprint-010). The three engine-free
            // seams (engine adapters in the engine build, null/empty in tests) are
            // Runtime-owned and declared BEFORE the driver so the driver — which holds
            // references to them — is destroyed first (reverse member-destruction).
            // The driver is NOT a FrameDispatcher subscriber; it runs in the
            // client-presentation phase after Dispatch (no new tick_order key;
            // networking-last preserved). `clientPresentationTick` is the monotonic
            // per-frame counter passed to Advance.
            std::unique_ptr<prediction::ILocalInputSource> localInputSource;
            std::unique_ptr<prediction::IAuthoritativeStateSource> authoritativeStateSource;
            std::unique_ptr<prediction::IPresentationSink> presentationSink;
            std::unique_ptr<prediction::ClientPresentationDriver> clientPresentationDriver;
            std::uint64_t clientPresentationTick = 0;

            // Persistence Framework (Sprint-011) + Save/Load System (Sprint-012). The
            // save backend is the Runtime-owned storage seam so it outlives the
            // ServiceRegistry-owned PersistenceManager that references its Store() (the
            // manager never dereferences it at destruction). Sprint-012, Step 17 swaps
            // the default in-memory persistence store for the build-selected save
            // backend: the real filesystem backend in the engine build, the in-memory
            // backend in tests (adapters::CreateEngineSaveBackend). The write seam
            // (Store(), IPersistenceStore) is the frozen Sprint-011 one, unchanged; the
            // paired read seam (Source(), ISaveSource) feeds startup recovery. Only the
            // PersistenceManager ticks (at kPersistence = 500); cached here for frame
            // subscription/teardown. Read-only snapshot consumption; no thread.
            std::unique_ptr<adapters::ISaveBackend> saveBackend;
            saveload::SaveLoadConfiguration saveLoadConfig;
            persistence::PersistenceManager* persistenceManager = nullptr;

            // Restoration write boundary (Sprint-012, Step 17). Owned here (outlives the
            // one-shot startup RecoveryPipeline). Engine build: the real EngineAdapters
            // sinks writing authoritative state through the sanctioned Sprint-003/005/007
            // seams. Test build: inert null sinks. NOT a FrameDispatcher subscriber —
            // recovery runs ONCE at Initialize, before the frame bridge and networking.
            std::unique_ptr<saveload::IRestoreSinkBundle> restoreSinks;

            // Lua Integration (Sprint-013, Step 17). Runtime-owned seams: the concrete Lua
            // runtime (real VM binding / null in tests), the Public API facade bundle (real
            // engine facades / recording in tests), and the script source (real filesystem
            // via PlatformScriptStore / in-memory in tests). The LuaManager owns runtime
            // init + API registration. The ScriptManager is ServiceRegistry-owned (cached
            // here as a raw pointer) and subscribed to the FrameDispatcher at the reserved
            // tick_order::kScripting = 375 (Gameplay phase). The diagnostics collector and
            // thread guard are non-invasive Runtime-owned surfaces (the guard is bound to
            // the simulation thread at Initialize). These seams are declared AFTER the
            // ServiceRegistry-owning members are used and are destroyed with the Runtime;
            // the ScriptManager never dereferences them at destruction.
            lua::LuaConfiguration luaConfig;
            std::unique_ptr<lua::ILuaRuntime> luaRuntime;
            std::unique_ptr<lua::ILuaApiBundle> luaApis;
            std::unique_ptr<lua::IScriptSource> scriptSource;
            std::unique_ptr<lua::LuaManager> luaManager;
            lua::ScriptManager* scriptManager = nullptr; // owned by ServiceRegistry
            lua::LuaDiagnostics luaDiagnostics;          // non-invasive collector
            lua::ScriptThreadGuard scriptThreadGuard;    // single-thread enforcement (bound at Init)

            // Plugin Framework (Sprint-014, Step 17). Runtime-owned seams: the plugin config;
            // the host-exposed API-id set; the discovery source (static-registration backend
            // per [AR-P1] Option B — PlatformPluginStore; in-memory snapshot in tests); and the
            // host-service surface (the reused Sprint-013 gameplay facades per [AR-P3] Option A,
            // wrapping the SAME Public API bundle `luaApis` the ScriptManager uses — declared
            // AFTER luaApis so it is destroyed BEFORE it). The PluginManager is ServiceRegistry-
            // owned (cached here as a raw pointer) and subscribed to the FrameDispatcher at the
            // reserved tick_order::kPlugins = 700 (after Persistence 500, before Networking 900).
            // The diagnostics collector and thread guard are non-invasive Runtime-owned surfaces
            // (the guard is bound to the simulation thread at Initialize). These seams outlive
            // ShutdownAll and are destroyed with the Runtime; the manager never dereferences them
            // at destruction.
            plugin::PluginConfiguration pluginConfig;
            std::vector<std::uint32_t> pluginAvailableApis;
            std::unique_ptr<plugin::IPluginSource> pluginSource;
            std::unique_ptr<plugin::IPluginHostSurface> pluginHostSurface;
            plugin::PluginManager* pluginManager = nullptr; // owned by ServiceRegistry
            plugin::PluginDiagnostics pluginDiagnostics;    // non-invasive collector
            plugin::PluginThreadGuard pluginThreadGuard;    // single-thread enforcement (bound at Init)
        };

        // The single module-global. See file header for justification.
        std::unique_ptr<Runtime> g_runtime;

        [[nodiscard]] Expected<LogLevel> ParseLogLevel(const std::string_view name)
        {
            using common::EqualsIgnoreCase;
            if (EqualsIgnoreCase(name, "verbose")) { return LogLevel::Verbose; }
            if (EqualsIgnoreCase(name, "debug")) { return LogLevel::Debug; }
            if (EqualsIgnoreCase(name, "info")) { return LogLevel::Info; }
            if (EqualsIgnoreCase(name, "warning")) { return LogLevel::Warning; }
            if (EqualsIgnoreCase(name, "error")) { return LogLevel::Error; }
            return MakeError(ErrorCode::InvalidArgument,
                             common::Format("Unknown log level '{}'", name));
        }

        [[nodiscard]] Expected<void> SetupLogging(Runtime& runtime, const InitializeParams& params)
        {
            if (auto directory = common::EnsureDirectory(params.logDirectory); !directory)
            {
                return directory;
            }

            runtime.logger.AddSink(std::make_unique<ConsoleLogSink>());

            const std::string logPath = common::JoinPath(params.logDirectory, kLogFileName);
            auto fileSinkResult = FileLogSink::Create(logPath);
            if (!fileSinkResult)
            {
                return fileSinkResult.GetError();
            }
            runtime.logger.AddSink(std::move(fileSinkResult).Value());

            core::detail::SetModuleLogger(&runtime.logger);
            return Success();
        }

        [[nodiscard]] Expected<void> LoadConfiguration(Runtime& runtime, const InitializeParams& params)
        {
            if (auto directory = common::EnsureDirectory(params.configDirectory); !directory)
            {
                return directory;
            }

            struct FileSpec
            {
                std::string_view name;
                std::string_view defaults;
                ConfigStore* target;
            };

            const FileSpec files[] = {
                {config_files::kDevelopment, kDefaultDevelopmentConfig, &runtime.developmentConfig},
                {config_files::kServer, kDefaultServerConfig, &runtime.serverConfig},
                {config_files::kClient, kDefaultClientConfig, &runtime.clientConfig},
            };

            for (const FileSpec& file : files)
            {
                const std::string path = common::JoinPath(params.configDirectory, file.name);
                const bool existed = common::FileExists(path);

                auto loaded = config::LoadOrCreate(path, file.defaults);
                if (!loaded)
                {
                    return loaded.GetError();
                }
                *file.target = std::move(loaded).Value();

                runtime.logger.Info(kLogCategory,
                                    common::Format(existed ? "Loaded configuration '{}' ({} keys)"
                                                           : "Created default configuration '{}' ({} keys)",
                                                   path, file.target->KeyCount()));

                // Version policy: missing -> assume 1 and warn; newer than
                // this build -> refuse (VersionMismatch); older -> accept
                // (the migration chain arrives with Sprint-012).
                const auto fileVersion = file.target->GetInt("meta", "config_version");
                if (!fileVersion)
                {
                    runtime.logger.Warning(
                        kLogCategory,
                        common::Format("'{}' has no [meta] config_version; assuming version 1", path));
                }
                else if (*fileVersion > config::kCurrentVersion)
                {
                    return MakeError(ErrorCode::VersionMismatch,
                                     common::Format("'{}' has config_version {} but this build supports "
                                                    "up to {}",
                                                    path, *fileVersion, config::kCurrentVersion));
                }
                else if (*fileVersion < config::kCurrentVersion)
                {
                    runtime.logger.Info(
                        kLogCategory,
                        common::Format("'{}' has config_version {} (current {}); accepted, migration "
                                       "support arrives with Sprint-012",
                                       path, *fileVersion, config::kCurrentVersion));
                }
            }

            // Apply the one setting Sprint-001 consumes.
            if (const auto levelName = runtime.developmentConfig.GetString("log", "level"))
            {
                auto level = ParseLogLevel(*levelName);
                if (!level)
                {
                    runtime.logger.Warning(kLogCategory,
                                           common::Format("{}; keeping level '{}'",
                                                          level.GetError().Message(),
                                                          LogLevelName(runtime.logger.MinimumLevel())));
                }
                else
                {
                    runtime.logger.SetMinimumLevel(level.Value());
                }
            }

            return Success();
        }

        // Sprint-002: construct and wire world services (composition root;
        // constructor injection, registration order = initialization order).
        [[nodiscard]] Expected<void> RegisterWorldServices(Runtime& runtime)
        {
            auto worldConfiguration = world::WorldConfiguration::FromConfig(runtime.serverConfig);
            if (!worldConfiguration)
            {
                return worldConfiguration.GetError();
            }

            auto registryConfiguration = world::EntityRegistryConfig::FromConfig(runtime.serverConfig);
            if (!registryConfiguration)
            {
                return registryConfiguration.GetError();
            }

            auto bubbleConfiguration = world::BubbleConfiguration::FromConfig(runtime.serverConfig);
            if (!bubbleConfiguration)
            {
                return bubbleConfiguration.GetError();
            }

            // Engine adapters (null implementations in the test build).
            runtime.entitySource = adapters::CreateEngineEntitySource();
            runtime.environmentSource = adapters::CreateEngineEnvironmentSource();

            auto manager = std::make_unique<world::WorldManager>(worldConfiguration.Value(),
                                                                 runtime.environmentSource.get());
            world::WorldManager* managerPtr = manager.get();

            if (auto registered = runtime.services.Register(std::move(manager)); !registered)
            {
                return registered;
            }

            // Entity Registry (Sprint-003 §7.1). Registered before WorldQueryService
            // so it initializes first (Design Review §5). Owned by the
            // ServiceRegistry; the raw pointer is cached only to wire the engine
            // feed below (no service-locator lookups elsewhere).
            auto registryService = std::make_unique<world::EntityRegistryService>(registryConfiguration.Value());
            world::EntityRegistryService* registryServicePtr = registryService.get();
            if (auto registry = runtime.services.Register(std::move(registryService)); !registry)
            {
                return registry;
            }

            // Engine feed (Sprint-003 engine feed; creation side). Reconciles the
            // engine object list into the registry once per frame at
            // tick_order::kEntityRegistry (after World). Null in the test build.
            runtime.entityFeed = adapters::CreateEngineEntityFeed(registryServicePtr->Registry());
            if (auto subscribed = runtime.frameDispatcher.Subscribe(
                    *runtime.entityFeed, tick_order::kEntityRegistry, "EntityRegistry");
                !subscribed)
            {
                return subscribed;
            }

            // WorldQueryService implements ISpatialQueries; cache the raw pointer to
            // wire it into the Bubble Manager below (no service-locator lookups).
            auto worldQueryService = std::make_unique<world::WorldQueryService>(*runtime.entitySource);
            world::WorldQueryService* worldQueryServicePtr = worldQueryService.get();
            if (auto queries = runtime.services.Register(std::move(worldQueryService)); !queries)
            {
                return queries;
            }

            if (auto environment = runtime.services.Register(
                    std::make_unique<world::EnvironmentService>(*managerPtr));
                !environment)
            {
                return environment;
            }

            // Bubble Manager (Sprint-004 §7.1). Owns the BubbleManager; consumes the
            // ISpatialQueries seam (WorldQueryService) and the local player-position
            // source, both read-only. Registered after its dependencies so it
            // initializes after them and shuts down before them. Not yet
            // frame-subscribed (a later step performs the per-tick update).
            // Sprint-007 Player Lifecycle foundation. Constructed BEFORE the Bubble
            // because the Bubble binds the networked player-position source at
            // construction (no setter; Bubble Manager code unchanged). The Session
            // is created here (its only input is maxConnections) so the manager can
            // reference it; the umbrella PlayerManagerService is registered later
            // (after Networking) so it initializes after all its dependencies.
            auto networkingConfiguration = net::NetworkingConfig::FromConfig(runtime.serverConfig);
            if (!networkingConfiguration)
            {
                return networkingConfiguration.GetError();
            }
            auto playerConfiguration = player::PlayerConfiguration::FromConfig(runtime.serverConfig);
            if (!playerConfiguration)
            {
                return playerConfiguration.GetError();
            }
            auto sessionService =
                std::make_unique<net::SessionService>(networkingConfiguration.Value().maxConnections);
            net::SessionService* sessionServicePtr = sessionService.get();
            if (auto registered = runtime.services.Register(std::move(sessionService)); !registered)
            {
                return registered;
            }
            runtime.playerSpawnGateway = adapters::CreatePlayerSpawnGateway();
            runtime.playerDeltaQueue = std::make_unique<player::PlayerDeltaQueue>();
            runtime.playerManager = std::make_unique<player::PlayerManager>(
                playerConfiguration.Value(), *runtime.playerSpawnGateway, sessionServicePtr->Get());
            runtime.networkedPlayerSource =
                std::make_unique<player::NetworkedPlayerPositionSource>(*runtime.playerManager);

            // Bubble binds the networked player-position source in place of the
            // Sprint-004 LocalPlayerPositionSource (only the injected instance
            // differs; positions-only, invariant B11 preserved).
            auto bubbleService = std::make_unique<world::BubbleManagerService>(
                bubbleConfiguration.Value(), worldQueryServicePtr, runtime.networkedPlayerSource.get());
            world::BubbleManagerService* bubbleServicePtr = bubbleService.get();
            if (auto bubble = runtime.services.Register(std::move(bubbleService)); !bubble)
            {
                return bubble;
            }

            // Run the bubble computation once per frame, after World and the
            // Entity Registry feed (tick_order::kBubble = 300). Uses the single
            // existing FrameDispatcher; no new engine frame registration.
            if (auto subscribed = runtime.frameDispatcher.Subscribe(
                    *bubbleServicePtr, tick_order::kBubble, "BubbleManager");
                !subscribed)
            {
                return subscribed;
            }
            runtime.bubbleManagerService = bubbleServicePtr;

            // ALife Transition Layer (Sprint-005 §14). Owns the TransitionManager;
            // consumes the BubbleManager (transitions) and EntityRegistry (liveness)
            // read-only, and drives the vanilla switch through the engine-free
            // IAlifeSwitchGateway seam. The gateway is created via the sanctioned
            // factory (real in the engine build, null in tests) and owned by the
            // Runtime so it outlives the service. Registered after its dependencies
            // (World, EntityRegistry, BubbleManager) so it initializes after them
            // and shuts down before them.
            runtime.transitionGateway = adapters::CreateEngineAlifeSwitchGateway();
            auto transitionService = std::make_unique<world::TransitionManagerService>(
                &bubbleServicePtr->Manager(), &registryServicePtr->Registry(),
                runtime.transitionGateway.get());
            world::TransitionManagerService* transitionServicePtr = transitionService.get();
            if (auto transition = runtime.services.Register(std::move(transitionService)); !transition)
            {
                return transition;
            }

            // Run the transition pipeline once per frame, strictly after the Bubble
            // has produced this tick's transitions (tick_order::kAlifeTransition = 350,
            // between Bubble = 300 and Replication = 400). Uses the single existing
            // FrameDispatcher; no new engine frame registration.
            if (auto subscribed = runtime.frameDispatcher.Subscribe(
                    *transitionServicePtr, tick_order::kAlifeTransition, "TransitionManager");
                !subscribed)
            {
                return subscribed;
            }
            runtime.transitionManagerService = transitionServicePtr;

            // Host Networking Infrastructure (Sprint-006). Registered AFTER the
            // simulation pipeline (World, EntityRegistry, Bubble, Transition) so it
            // initializes after and shuts down before them. Networking consumes
            // simulation output and transports information — it OWNS no simulation.
            // The transport is created via the sanctioned factory (real UDP in the
            // engine build, loopback/mock in tests) and owned by the TransportService.
            // networkingConfiguration and the SessionService were created earlier
            // (before the Bubble) for the Sprint-007 player foundation.
            auto transportConfiguration = net::TransportConfig::FromConfig(runtime.serverConfig);
            if (!transportConfiguration)
            {
                return transportConfiguration.GetError();
            }

            auto transportService =
                std::make_unique<net::TransportService>(adapters::CreatePlatformTransport(transportConfiguration.Value()));
            net::TransportService* transportServicePtr = transportService.get();
            if (auto registered = runtime.services.Register(std::move(transportService)); !registered)
            {
                return registered;
            }

            auto messageRegistryService = std::make_unique<net::MessageRegistryService>();
            net::MessageRegistryService* messageRegistryServicePtr = messageRegistryService.get();
            if (auto registered = runtime.services.Register(std::move(messageRegistryService)); !registered)
            {
                return registered;
            }

            // SessionService was created earlier (before the Bubble); reuse it.
            auto networkingService = std::make_unique<net::NetworkingService>(
                networkingConfiguration.Value(), transportConfiguration.Value(),
                transportServicePtr->Transport(), sessionServicePtr->Get(),
                messageRegistryServicePtr->Registry(), runtime.networkAuthenticator);
            net::NetworkingService* networkingServicePtr = networkingService.get();
            if (auto registered = runtime.services.Register(std::move(networkingService)); !registered)
            {
                return registered;
            }

            // The single networking tick — the highest key, strictly after every
            // producer stage (networking-last invariant). Only the umbrella ticks.
            if (auto subscribed = runtime.frameDispatcher.Subscribe(
                    *networkingServicePtr, tick_order::kNetworking, "Networking");
                !subscribed)
            {
                return subscribed;
            }
            runtime.networkingService = networkingServicePtr;

            // Sprint-007 PlayerManagerService. Registered AFTER its dependencies
            // (World, EntityRegistry, BubbleManager, TransitionManager, Networking)
            // so it initializes after them (registration-order init) and shuts down
            // before them. References the Runtime-owned PlayerManager + delta queue,
            // the Session, and the MessageRegistry (for the additive player-request
            // id registration performed in Initialize).
            auto playerManagerService = std::make_unique<player::PlayerManagerService>(
                *runtime.playerManager, *runtime.playerDeltaQueue, sessionServicePtr->Get(),
                messageRegistryServicePtr->Registry());
            player::PlayerManagerService* playerManagerServicePtr = playerManagerService.get();
            if (auto registered = runtime.services.Register(std::move(playerManagerService)); !registered)
            {
                return registered;
            }
            runtime.playerManagerService = playerManagerServicePtr;

            // Single player-lifecycle tick at tick_order::kPlayerLifecycle = 250,
            // strictly between EntityRegistry (200) and Bubble (300): a joined
            // player is visible to activation the same frame (Architecture AD-2).
            if (auto subscribed = runtime.frameDispatcher.Subscribe(
                    *playerManagerServicePtr, tick_order::kPlayerLifecycle, "PlayerLifecycle");
                !subscribed)
            {
                return subscribed;
            }

            if (auto subscribed = runtime.frameDispatcher.Subscribe(
                    *managerPtr, tick_order::kWorld, "World");
                !subscribed)
            {
                return subscribed;
            }

            // Snapshot System (Sprint-008 §Step-10). Registered LAST — after every
            // simulation producer (World, EntityRegistry, Bubble, Transition,
            // PlayerManager) — so its declared ordering-only dependencies are all
            // registered earlier and registration-order InitializeAll initializes it
            // after them (Initialize reserves the pool). Constructed with the
            // SnapshotConfiguration, the engine-free entity snapshot source (real in
            // the engine build, null in tests; Runtime-owned), and const references
            // to the Sprint-007 player surface and the Sprint-002 environment source.
            auto snapshotConfiguration = snapshot::SnapshotConfiguration::FromConfig(runtime.serverConfig);
            if (!snapshotConfiguration)
            {
                return snapshotConfiguration.GetError();
            }
            runtime.entitySnapshotSource = adapters::CreateEngineEntitySnapshotSource();
            auto snapshotManager = std::make_unique<snapshot::SnapshotManager>(
                snapshotConfiguration.Value(), *runtime.entitySnapshotSource, *runtime.playerManager,
                *runtime.environmentSource);
            snapshot::SnapshotManager* snapshotManagerPtr = snapshotManager.get();
            if (auto registered = runtime.services.Register(std::move(snapshotManager)); !registered)
            {
                return registered;
            }

            // Single snapshot tick at the reserved tick_order::kReplication = 400,
            // strictly after the Transition stage (350) and before Networking (900):
            // one immutable snapshot is published per frame from the completed
            // simulation state. Uses the single existing FrameDispatcher; no new
            // engine frame registration.
            if (auto subscribed = runtime.frameDispatcher.Subscribe(
                    *snapshotManagerPtr, tick_order::kReplication, "Snapshot");
                !subscribed)
            {
                return subscribed;
            }
            runtime.snapshotManager = snapshotManagerPtr;

            // Replication Pipeline (Sprint-009). Registered AFTER the SnapshotManager
            // (its snapshot source) and every simulation producer, so registration-
            // order InitializeAll initializes it last. Consumes the Sprint-008
            // snapshot queue read-only, the Sprint-004 Bubble activation (via the
            // engine-free membership adapter) for interest, and its own client
            // registry; it produces packets but OWNS no transport (send is future).
            auto replicationConfiguration = replication::ReplicationConfiguration::FromConfig(runtime.serverConfig);
            if (!replicationConfiguration)
            {
                return replicationConfiguration.GetError();
            }
            runtime.replicationClients =
                std::make_unique<replication::ReplicationClientRegistry>(replicationConfiguration.Value().maxClients);
            runtime.bubbleMembership =
                std::make_unique<replication::BubbleMembershipAdapter>(bubbleServicePtr->Manager());
            runtime.interestPolicy = std::make_unique<replication::BubbleInterestPolicy>(
                *runtime.bubbleMembership, replicationConfiguration.Value().interestRadiusMeters);

            auto replicationManager = std::make_unique<replication::ReplicationManager>(
                replicationConfiguration.Value(), *runtime.replicationClients, *runtime.interestPolicy,
                snapshotManagerPtr->Queue());
            replication::ReplicationManager* replicationManagerPtr = replicationManager.get();
            if (auto registered = runtime.services.Register(std::move(replicationManager)); !registered)
            {
                return registered;
            }

            // Single replication tick at the reserved tick_order::kReplicationPipeline
            // = 450, strictly after Snapshot (400) and before Persistence (500)/
            // Networking (900): consumes the just-published snapshot and assembles
            // per-client packets. Uses the single existing FrameDispatcher.
            if (auto subscribed = runtime.frameDispatcher.Subscribe(
                    *replicationManagerPtr, tick_order::kReplicationPipeline, "Replication");
                !subscribed)
            {
                return subscribed;
            }
            runtime.replicationManager = replicationManagerPtr;

            // Persistence Framework (Sprint-011). Registered AFTER the SnapshotManager
            // (its read-only snapshot source) and the Replication consumer, so
            // registration-order InitializeAll initializes it last among the consumers.
            // It consumes the Sprint-008 snapshot queue read-only, owns its
            // scheduler/queue/worker/version-manager, and hands validated saves to the
            // engine-free IPersistenceStore seam (the in-memory backend here; the real
            // filesystem backend is Sprint-012). No thread; no OS/file code.
            auto persistenceConfiguration = persistence::PersistenceConfiguration::FromConfig(runtime.serverConfig);
            if (!persistenceConfiguration)
            {
                return persistenceConfiguration.GetError();
            }

            // Save/Load System (Sprint-012, Step 17). Parse the [saveload] config block
            // and bind the build-selected save backend. The backend's write store
            // (Store(), the frozen IPersistenceStore seam) replaces the Sprint-011
            // in-memory store behind the SAME seam — the PersistenceManager and the
            // kPersistence = 500 tick are otherwise unchanged. The backend's read source
            // (Source(), ISaveSource) feeds startup recovery below. Filesystem I/O stays
            // confined to PlatformSaveStore.cpp (engine build); tests use in-memory.
            auto saveLoadConfiguration = saveload::SaveLoadConfiguration::FromConfig(runtime.serverConfig);
            if (!saveLoadConfiguration)
            {
                return saveLoadConfiguration.GetError();
            }
            runtime.saveLoadConfig = saveLoadConfiguration.Value();
            auto saveBackend = adapters::CreateEngineSaveBackend(runtime.saveLoadConfig);
            if (!saveBackend)
            {
                return saveBackend.GetError();
            }
            runtime.saveBackend = std::move(saveBackend).Value();

            auto persistenceManager = std::make_unique<persistence::PersistenceManager>(
                persistenceConfiguration.Value(), snapshotManagerPtr->Queue(), runtime.saveBackend->Store());
            persistence::PersistenceManager* persistenceManagerPtr = persistenceManager.get();
            if (auto registered = runtime.services.Register(std::move(persistenceManager)); !registered)
            {
                return registered;
            }

            // Single persistence tick at the reserved tick_order::kPersistence = 500,
            // strictly after Replication (450) and before Networking (900): consumes the
            // just-published snapshot and runs one synchronous persistence pass. Uses the
            // single existing FrameDispatcher; no new tick_order key.
            if (auto subscribed = runtime.frameDispatcher.Subscribe(
                    *persistenceManagerPtr, tick_order::kPersistence, "Persistence");
                !subscribed)
            {
                return subscribed;
            }
            runtime.persistenceManager = persistenceManagerPtr;

            // Lua Integration (Sprint-013, Step 17). Parse [lua]; bind the build-selected
            // Lua runtime, Public API facade bundle, and script source behind engine-free
            // factories (real LuaJIT + engine facades + PlatformScriptStore in the engine
            // build; null/recording/in-memory in tests). The ScriptManager is registered as
            // an IService and subscribed at the reserved tick_order::kScripting = 375 — the
            // Gameplay phase, strictly after ALifeTransition (350) and before Snapshot
            // (kReplication = 400) — so authoritative effects a script applies through the
            // sanctioned seams are captured in the same-frame snapshot. LuaManager.Init +
            // ScriptManager.LoadAll run once at Initialize (before networking). Engine code
            // is confined to EngineAdapters.cpp; script-file I/O to PlatformScriptStore.cpp.
            auto luaConfiguration = lua::LuaConfiguration::FromConfig(runtime.serverConfig);
            if (!luaConfiguration)
            {
                return luaConfiguration.GetError();
            }
            runtime.luaConfig = luaConfiguration.Value();
            runtime.luaRuntime = adapters::CreateEngineLuaRuntime();
            runtime.luaApis = adapters::CreateEngineScriptApi();
            runtime.scriptSource = adapters::CreateEngineScriptSource(runtime.luaConfig);
            runtime.luaManager = std::make_unique<lua::LuaManager>(*runtime.luaRuntime, runtime.luaApis->Apis());

            auto scriptManager = std::make_unique<lua::ScriptManager>(
                *runtime.luaRuntime, *runtime.scriptSource, runtime.luaConfig.version,
                runtime.luaConfig.strictApiVersion);
            lua::ScriptManager* scriptManagerPtr = scriptManager.get();
            if (auto registered = runtime.services.Register(std::move(scriptManager)); !registered)
            {
                return registered;
            }

            // Single scripting tick at the reserved tick_order::kScripting = 375 (Gameplay
            // phase; after ALifeTransition 350, before Snapshot 400). Uses the single
            // existing FrameDispatcher; networking-last (kNetworking = 900) preserved.
            if (auto subscribed = runtime.frameDispatcher.Subscribe(
                    *scriptManagerPtr, tick_order::kScripting, "Scripting");
                !subscribed)
            {
                return subscribed;
            }
            runtime.scriptManager = scriptManagerPtr;

            // Plugin Framework (Sprint-014, Step 17). Parse [plugins]; bind the discovery
            // source (static-registration backend per [AR-P1] Option B — PlatformPluginStore;
            // an in-memory snapshot in tests) and the host-service surface (the reused
            // Sprint-013 gameplay facades per [AR-P3] Option A, wrapping the SAME Public API
            // bundle `luaApis` the ScriptManager consumes — no new engine facade). The
            // PluginManager is registered as an IService (after every simulation producer and
            // after Scripting) and subscribed at the reserved tick_order::kPlugins = 700 —
            // strictly after Persistence (500) and before Networking (900) — so a plugin's
            // authoritative effects (applied through the sanctioned seams) are captured in the
            // subsequent frame's snapshot. PluginManager.Startup() runs once at Initialize
            // (before networking). No new tick_order key; networking-last preserved. Module-
            // loading code stays confined to PlatformPluginStore.cpp (Option B: no OS loader).
            auto pluginConfiguration = plugin::PluginConfiguration::FromConfig(runtime.serverConfig);
            if (!pluginConfiguration)
            {
                return pluginConfiguration.GetError();
            }
            runtime.pluginConfig = pluginConfiguration.Value();
            runtime.pluginSource = adapters::CreatePlatformPluginSource(runtime.pluginConfig);
            runtime.pluginHostSurface =
                std::make_unique<plugin::GameplayHostSurface>(runtime.luaApis->Apis());
            runtime.pluginAvailableApis = {}; // host-advertised API ids (none in the Option A baseline)

            auto pluginManager = std::make_unique<plugin::PluginManager>(
                *runtime.pluginSource, *runtime.pluginHostSurface, runtime.pluginAvailableApis,
                runtime.pluginConfig);
            plugin::PluginManager* pluginManagerPtr = pluginManager.get();
            if (auto registered = runtime.services.Register(std::move(pluginManager)); !registered)
            {
                return registered;
            }

            // Single plugin tick at the reserved tick_order::kPlugins = 700 (after Persistence
            // 500, before Networking 900). Uses the single existing FrameDispatcher; no new
            // tick_order key; networking-last (kNetworking = 900) preserved.
            if (auto subscribed = runtime.frameDispatcher.Subscribe(
                    *pluginManagerPtr, tick_order::kPlugins, "Plugins");
                !subscribed)
            {
                return subscribed;
            }
            runtime.pluginManager = pluginManagerPtr;

            runtime.worldManager = managerPtr;
            return Success();
        }

        // Engine-free authoritative source that yields no frames. The client receive
        // + decode pipeline (a future sprint) will replace this behind the same seam;
        // until then the driver has no authoritative frames to consume, and the host
        // renders authoritative state directly (identity mode).
        class EmptyAuthoritativeStateSource final : public prediction::IAuthoritativeStateSource
        {
        public:
            [[nodiscard]] bool NextFrame(prediction::SnapshotFrame&) const override { return false; }
        };

        // Sprint-010: construct + wire the client-presentation driver (prediction +
        // interpolation). It is NOT a FrameDispatcher subscriber — the frame bridge
        // invokes Advance in the client-presentation phase AFTER Dispatch (no new
        // tick_order key; networking-last preserved). Identity mode is selected
        // because this composition IS the host (it owns the authoritative simulation
        // and replication); a client build would set identity mode false and supply a
        // receive-decoded authoritative source behind the same seam.
        [[nodiscard]] Expected<void> RegisterClientPresentation(Runtime& runtime)
        {
            auto predictionConfiguration = prediction::PredictionConfiguration::FromConfig(runtime.clientConfig);
            if (!predictionConfiguration)
            {
                return predictionConfiguration.GetError();
            }

            // Engine-free seams (real engine adapters in the engine build, null/empty
            // in tests). Owned by the Runtime so they outlive the driver.
            runtime.localInputSource = adapters::CreateEngineLocalInputSource();
            runtime.presentationSink = adapters::CreateEnginePresentationSink();
            runtime.authoritativeStateSource = std::make_unique<EmptyAuthoritativeStateSource>();

            runtime.clientPresentationDriver = std::make_unique<prediction::ClientPresentationDriver>(
                predictionConfiguration.Value(), *runtime.localInputSource, *runtime.authoritativeStateSource,
                *runtime.presentationSink);

            // Host authority: render authoritative state directly (no prediction/
            // interpolation on the host).
            runtime.clientPresentationDriver->SetIdentityMode(true);

            runtime.logger.Info(kLogCategory, "Client-presentation driver wired (host identity mode)");
            return Success();
        }

        [[nodiscard]] Expected<void> RegisterModules(Runtime& runtime)
        {
            // Sprint-001 §7.10 — declared now, implemented by later sprints.
            const ModuleDescriptor descriptors[] = {
                {"World", "World abstraction layer — Sprint-002"},
                {"Player", "Player connections and identity — Sprint-007"},
                {"Replication", "Snapshot and delta replication — Sprint-008/009"},
                {"Persistence", "Persistent world storage — Sprint-011"},
                {"Networking", "Host networking and transport — Sprint-006"},
                {"Plugins", "Plugin and extension system — Sprint-014"},
                {"Diagnostics", "Runtime diagnostics and metrics — Sprint-015"},
            };

            for (const ModuleDescriptor& descriptor : descriptors)
            {
                if (auto declared = runtime.modules.Declare(descriptor); !declared)
                {
                    return declared;
                }
                runtime.logger.Debug(kLogCategory,
                                     common::Format("Declared module '{}'", descriptor.name));
            }

            // Sprint-002: the World module is the first implemented module.
            if (auto attached = runtime.modules.AttachImplementation(
                    "World", std::make_unique<world::WorldModule>());
                !attached)
            {
                return attached;
            }

            return Success();
        }

        [[nodiscard]] Expected<void> InitializeRuntime(Runtime& runtime, const InitializeParams& params)
        {
            // 1. Logging.
            if (auto logging = SetupLogging(runtime, params); !logging)
            {
                return logging;
            }

            runtime.versionString = version::Banner();
            runtime.logger.Info(kLogCategory, runtime.versionString);
            runtime.logger.Info(kLogCategory,
                                common::Format("Engine reports version '{}'", params.engineVersion));

            // 2. Configuration.
            if (auto configuration = LoadConfiguration(runtime, params); !configuration)
            {
                return configuration;
            }

            // 3. Engine compatibility.
            if (auto compatibility = version::VerifyEngineCompatibility(params.engineVersion); !compatibility)
            {
                return compatibility;
            }
            runtime.logger.Info(kLogCategory, "Engine compatibility verified");

            // 4. Service construction and wiring (Sprint-002: World).
            if (auto worldServices = RegisterWorldServices(runtime); !worldServices)
            {
                return worldServices;
            }

            // 4b. Client Prediction & Interpolation (Sprint-010). Constructed after
            //     the simulation/replication services it presents; NOT a dispatcher
            //     subscriber — invoked in the client-presentation phase after Dispatch.
            if (auto presentation = RegisterClientPresentation(runtime); !presentation)
            {
                return presentation;
            }

            // 5. Service initialization in registration order.
            if (auto services = runtime.services.InitializeAll(); !services)
            {
                return services;
            }
            runtime.logger.Info(kLogCategory,
                                common::Format("Service registry initialized ({} services)",
                                               runtime.services.Count()));

            // 6. Modules.
            if (auto modules = RegisterModules(runtime); !modules)
            {
                return modules;
            }
            runtime.logger.Info(kLogCategory,
                                common::Format("Module registry populated ({} modules declared)",
                                               runtime.modules.Count()));

            // 7. Start the world simulation.
            if (auto started = runtime.worldManager->Start(); !started)
            {
                return started;
            }

            // 7b. Save/Load startup recovery (Sprint-012, Step 17). Runs ONCE here —
            //     AFTER world Start groundwork, BEFORE the engine frame bridge and before
            //     networking accepts connections — guaranteeing "simulation starts before
            //     networking". This is NOT a FrameDispatcher subscriber and introduces NO
            //     new tick_order key. Restoration crosses the engine boundary ONLY through
            //     the Step-10 restore-sink seams (the Runtime-owned bundle). Recovery is
            //     isolated (E-G5-SL): a missing/corrupted/partial save is a logged value
            //     outcome that leaves the freshly-started world intact — never fatal.
            runtime.restoreSinks = adapters::CreateEngineRestoreSinks();
            if (runtime.saveLoadConfig.loadOnStartup)
            {
                const std::vector<saveload::SaveDescriptor> saves = runtime.saveBackend->Source().Enumerate();
                if (saves.empty())
                {
                    runtime.logger.Info(kLogCategory, "Save/Load: no save to recover; starting a fresh world");
                }
                else
                {
                    // Enumerate() is ascending; the last descriptor is the newest save.
                    const std::uint64_t newest = saves.back().saveId;
                    saveload::SaveMigrator migrator;
                    saveload::RecoveryPipeline pipeline(runtime.saveBackend->Source(), persistence::VersionManager{},
                                                        migrator, saveload::kSaveFormatVersion);
                    const saveload::RecoveryReport report =
                        pipeline.Recover(newest, runtime.restoreSinks->Sinks());
                    if (report.outcome == saveload::SaveLoadOutcome::Ok)
                    {
                        runtime.logger.Info(
                            kLogCategory,
                            common::Format("Save/Load: recovered save {} ({} entities, {} players) before networking",
                                           report.saveId, report.entitiesRestored, report.playersRestored));
                    }
                    else
                    {
                        // Non-fatal: the world remains as freshly started (recovery isolation).
                        runtime.logger.Warning(
                            kLogCategory,
                            common::Format("Save/Load: recovery of save {} did not complete (outcome {}); "
                                           "continuing with the freshly-started world",
                                           newest, static_cast<int>(report.outcome)));
                    }
                }
            }

            // 7c. Lua Integration startup (Sprint-013, Step 17). Runs ONCE here — before
            //     the engine frame bridge and before networking. Bind the thread guard to
            //     the simulation thread (single-thread enforcement), then LuaManager.Init
            //     (create the VM state, load libraries, register the Public API facades as
            //     host functions) and ScriptManager.LoadAll (discover + load host-side
            //     scripts). A runtime-init or load failure is NON-fatal: scripting is left
            //     disabled and the engine continues (fault isolation). The per-frame OnTick
            //     runs at kScripting = 375.
            runtime.scriptThreadGuard.Bind(1); // sanctioned simulation-thread token
            if (const lua::ScriptOutcome o = runtime.luaManager->Init(); o != lua::ScriptOutcome::Ok)
            {
                runtime.logger.Warning(
                    kLogCategory,
                    common::Format("Lua: runtime init failed (outcome {}); scripting disabled",
                                   static_cast<int>(o)));
            }
            else
            {
                const lua::ScriptLoadReport report = runtime.scriptManager->LoadAll();
                runtime.logger.Info(kLogCategory,
                                    common::Format("Lua: loaded {}/{} scripts before networking",
                                                   report.loaded, report.attempted));
            }

            // 7d. Plugin Framework startup (Sprint-014, Step 17). Runs ONCE here — before the
            //     engine frame bridge and before networking. Bind the thread guard to the
            //     simulation thread (single-thread enforcement), then PluginManager.Startup():
            //     load every statically-registered plugin (PluginLoader), then initialize and
            //     activate the loaded-and-bound plugins through the PluginHost. A load or init
            //     failure isolates ONLY the offending plugin (fault isolation); the engine
            //     continues. The per-frame OnTick dispatch runs at kPlugins = 700. (In the
            //     engine build, statically-linked plugins register their manifest and bind
            //     their instance via EngineAdapters; the engine-free composition here loads the
            //     registered manifests deterministically.)
            runtime.pluginThreadGuard.Bind(1); // sanctioned simulation-thread token (as scripting)
            const plugin::PluginStartupReport pluginReport = runtime.pluginManager->Startup();
            runtime.pluginDiagnostics.Reset();
            runtime.logger.Info(
                kLogCategory,
                common::Format("Plugins: loaded {}/{} and activated {} before networking ({} isolated)",
                               pluginReport.loaded, pluginReport.discovered, pluginReport.initialized,
                               pluginReport.isolated));

            // 8. Engine frame registration — strictly LAST: no engine
            //    callback may arrive before the module is fully ready
            //    (register-last / remove-first, Design Review §5).
            auto bridge = adapters::CreateEngineFrameBridge(runtime.frameDispatcher);
            if (!bridge)
            {
                return bridge.GetError();
            }
            runtime.frameBridge = std::move(bridge).Value();
            runtime.logger.Info(kLogCategory, "Engine frame bridge registered");

            runtime.logger.Info(kLogCategory, "STALKER-MP initialized");
            runtime.logger.Flush();
            return Success();
        }

        void DestroyRuntime() noexcept
        {
            if (!g_runtime)
            {
                return;
            }

            // Reverse of initialization: the engine frame registration is
            // removed FIRST so no engine callback can reach a
            // shutting-down module (remove-first, Design Review §5).
            g_runtime->frameBridge.reset();

            // Client-presentation driver (Sprint-010) next: the frame bridge that
            // drove it (via the post-Dispatch phase) is gone, so no further Advance
            // can run. Destroyed before its Runtime-owned seams (which follow with the
            // Runtime) since it holds references to them.
            g_runtime->clientPresentationDriver.reset();

            // Engine feed next: unsubscribe from the (no-longer-driven) dispatcher
            // and destroy it before the ServiceRegistry that owns the registry it
            // references.
            if (g_runtime->entityFeed)
            {
                if (auto unsubscribed = g_runtime->frameDispatcher.Unsubscribe(*g_runtime->entityFeed);
                    !unsubscribed && IsLogAvailable())
                {
                    Log().Warning(kLogCategory, unsubscribed.GetError().Describe());
                }
                g_runtime->entityFeed.reset();
            }

            // Plugin Framework runtime (Sprint-014): unsubscribe the PluginManager from the
            // dispatcher (subscribed LAST, at kPlugins = 700) — reverse-order teardown, so it
            // is removed before the Persistence/Lua consumers it followed in registration —
            // then let ShutdownAll run its Shutdown (host.ShutdownAll + fault clear). The
            // Runtime-owned plugin source / host surface / available-api set / config outlive
            // ShutdownAll and are destroyed with the Runtime; the manager never dereferences
            // them at destruction (the host surface is destroyed before `luaApis`, whose
            // facades it wraps).
            if (g_runtime->pluginManager != nullptr)
            {
                if (auto unsubscribed = g_runtime->frameDispatcher.Unsubscribe(*g_runtime->pluginManager);
                    !unsubscribed && IsLogAvailable())
                {
                    Log().Warning(kLogCategory, unsubscribed.GetError().Describe());
                }
                g_runtime->pluginManager = nullptr;
            }

            // Persistence Framework runtime (Sprint-011) + Save/Load System (Sprint-012):
            // unsubscribe FIRST among the consumers (persistence subscribed last, at
            // kPersistence = 500), before ShutdownAll. The Runtime-owned save backend
            // (whose Store() the manager references) and the one-shot restore-sink bundle
            // outlive ShutdownAll and are destroyed with the Runtime; the manager never
            // dereferences the store at destruction. The restore-sink bundle is not a
            // subscriber (recovery ran once at Initialize) — nothing to unsubscribe.
            if (g_runtime->persistenceManager != nullptr)
            {
                if (auto unsubscribed = g_runtime->frameDispatcher.Unsubscribe(*g_runtime->persistenceManager);
                    !unsubscribed && IsLogAvailable())
                {
                    Log().Warning(kLogCategory, unsubscribed.GetError().Describe());
                }
                g_runtime->persistenceManager = nullptr;
            }

            // Lua Integration runtime (Sprint-013): unsubscribe the ScriptManager from the
            // dispatcher (subscribed at kScripting = 375), then shut down the runtime state
            // (LuaManager.Shutdown -> DestroyState), before ShutdownAll. The Runtime-owned
            // Lua runtime / API bundle / script source outlive ShutdownAll and are destroyed
            // with the Runtime; the ScriptManager never dereferences them at destruction.
            if (g_runtime->scriptManager != nullptr)
            {
                if (auto unsubscribed = g_runtime->frameDispatcher.Unsubscribe(*g_runtime->scriptManager);
                    !unsubscribed && IsLogAvailable())
                {
                    Log().Warning(kLogCategory, unsubscribed.GetError().Describe());
                }
                g_runtime->scriptManager = nullptr;
            }
            if (g_runtime->luaManager)
            {
                g_runtime->luaManager->Shutdown();
            }

            // Replication Pipeline runtime (Sprint-009): unsubscribe from the
            // (no-longer-driven) dispatcher before ShutdownAll. Removed before the
            // SnapshotManager it consumes (reverse of subscription order — it was
            // subscribed last, at kReplicationPipeline = 450). The Runtime-owned
            // client registry / membership adapter / interest policy outlive
            // ShutdownAll and are destroyed with the Runtime; the manager never
            // dereferences them at destruction.
            if (g_runtime->replicationManager != nullptr)
            {
                if (auto unsubscribed = g_runtime->frameDispatcher.Unsubscribe(*g_runtime->replicationManager);
                    !unsubscribed && IsLogAvailable())
                {
                    Log().Warning(kLogCategory, unsubscribed.GetError().Describe());
                }
                g_runtime->replicationManager = nullptr;
            }

            // Snapshot System runtime (Sprint-008): unsubscribe from the
            // (no-longer-driven) dispatcher before ShutdownAll. Removed before the
            // producer subsystems it observes (reverse of subscription order — it was
            // subscribed last). The SnapshotManager owns its pool/builder/queue, so
            // ShutdownAll's reverse-order Shutdown + destruction returns every pooled
            // buffer; the Runtime-owned entity snapshot source outlives ShutdownAll
            // and is destroyed with the Runtime (same pattern as the other sources).
            if (g_runtime->snapshotManager != nullptr)
            {
                if (auto unsubscribed = g_runtime->frameDispatcher.Unsubscribe(*g_runtime->snapshotManager);
                    !unsubscribed && IsLogAvailable())
                {
                    Log().Warning(kLogCategory, unsubscribed.GetError().Describe());
                }
                g_runtime->snapshotManager = nullptr;
            }

            // Host Networking runtime (Sprint-006): unsubscribe FIRST among the
            // subsystems (networking subscribed last, at the highest key), before
            // ShutdownAll. The transport (owned by the TransportService) is closed
            // via the host's Stop during ShutdownAll and destroyed with the
            // ServiceRegistry; no service dereferences it at destruction.
            if (g_runtime->networkingService != nullptr)
            {
                if (auto unsubscribed = g_runtime->frameDispatcher.Unsubscribe(*g_runtime->networkingService);
                    !unsubscribed && IsLogAvailable())
                {
                    Log().Warning(kLogCategory, unsubscribed.GetError().Describe());
                }
                g_runtime->networkingService = nullptr;
            }

            // ALife Transition runtime (Sprint-005): unsubscribe from the
            // (no-longer-driven) dispatcher before the ServiceRegistry that owns the
            // service is shut down. Removed before the Bubble it depends on (reverse
            // of subscription order). The Runtime-owned gateway is destroyed with the
            // Runtime; the service never dereferences it at destruction (same pattern
            // as the entity/environment sources).
            if (g_runtime->transitionManagerService != nullptr)
            {
                if (auto unsubscribed = g_runtime->frameDispatcher.Unsubscribe(*g_runtime->transitionManagerService);
                    !unsubscribed && IsLogAvailable())
                {
                    Log().Warning(kLogCategory, unsubscribed.GetError().Describe());
                }
                g_runtime->transitionManagerService = nullptr;
            }

            // Bubble runtime: unsubscribe from the (no-longer-driven) dispatcher
            // before the ServiceRegistry that owns the service is shut down.
            if (g_runtime->bubbleManagerService != nullptr)
            {
                if (auto unsubscribed = g_runtime->frameDispatcher.Unsubscribe(*g_runtime->bubbleManagerService);
                    !unsubscribed && IsLogAvailable())
                {
                    Log().Warning(kLogCategory, unsubscribed.GetError().Describe());
                }
                g_runtime->bubbleManagerService = nullptr;
            }

            // Player Lifecycle runtime (Sprint-007): unsubscribe from the
            // (no-longer-driven) dispatcher before ShutdownAll. Networking is
            // already frame-unsubscribed above, so no session callback fires into
            // the observer during teardown; the Runtime-owned player objects
            // (manager/queue/observer via the service) outlive ShutdownAll and are
            // destroyed with the Runtime.
            if (g_runtime->playerManagerService != nullptr)
            {
                if (auto unsubscribed = g_runtime->frameDispatcher.Unsubscribe(*g_runtime->playerManagerService);
                    !unsubscribed && IsLogAvailable())
                {
                    Log().Warning(kLogCategory, unsubscribed.GetError().Describe());
                }
                g_runtime->playerManagerService = nullptr;
            }

            if (g_runtime->worldManager != nullptr)
            {
                if (auto unsubscribed = g_runtime->frameDispatcher.Unsubscribe(*g_runtime->worldManager);
                    !unsubscribed && IsLogAvailable())
                {
                    Log().Warning(kLogCategory, unsubscribed.GetError().Describe());
                }
                g_runtime->worldManager = nullptr;
            }
            g_runtime->services.ShutdownAll();

            if (IsLogAvailable())
            {
                g_runtime->logger.Info(kLogCategory, "STALKER-MP shut down");
                g_runtime->logger.Flush();
            }

            core::detail::SetModuleLogger(nullptr);
            g_runtime.reset();
        }

        void ReportInitializeFailure(const Error& error) noexcept
        {
            const std::string description = "STALKER-MP initialization failed: " + error.Describe();
            if (IsLogAvailable())
            {
                Log().Error(kLogCategory, description);
                Log().Flush();
            }
            else
            {
                std::fputs(description.c_str(), stderr);
                std::fputc('\n', stderr);
                std::fflush(stderr);
            }
        }
    } // namespace

    bool Initialize(const InitializeParams& params) noexcept
    {
        if (g_runtime)
        {
            core::Log().Warning(kLogCategory,
                                "Initialize called while already initialized; keeping running instance");
            return true;
        }

        auto runtime = std::make_unique<Runtime>();
        g_runtime = std::move(runtime);

        if (auto result = InitializeRuntime(*g_runtime, params); !result)
        {
            ReportInitializeFailure(result.GetError());
            DestroyRuntime();
            return false;
        }
        return true;
    }

    void Shutdown() noexcept
    {
        DestroyRuntime();
    }

    bool IsInitialized() noexcept
    {
        return g_runtime != nullptr;
    }

    const char* VersionString() noexcept
    {
        // Valid independently of runtime lifetime so the engine can log the
        // module version at any point.
        static const std::string banner = core::version::Banner();
        return banner.c_str();
    }
} // namespace stalkermp

namespace stalkermp::detail
{
    core::FrameDispatcher* GetModuleFrameDispatcher() noexcept
    {
        return g_runtime ? &g_runtime->frameDispatcher : nullptr;
    }

    // Client-presentation phase (Sprint-010): runs the driver once per frame, after
    // FrameDispatcher::Dispatch. No-op when uninitialized or no driver is wired.
    // Deterministic (tick-driven); deltaSeconds is not used in control flow.
    void AdvanceClientPresentation(double deltaSeconds) noexcept
    {
        if (!g_runtime || !g_runtime->clientPresentationDriver)
        {
            return;
        }
        ++g_runtime->clientPresentationTick;
        (void)g_runtime->clientPresentationDriver->Advance(g_runtime->clientPresentationTick, deltaSeconds);
    }

    const prediction::ClientPresentationDriver* GetModuleClientPresentationDriver() noexcept
    {
        return g_runtime ? g_runtime->clientPresentationDriver.get() : nullptr;
    }
} // namespace stalkermp::detail

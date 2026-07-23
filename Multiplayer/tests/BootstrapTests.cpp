#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

#include "stalkermp/StalkerMP.h"
#include "stalkermp/core/Config.h"
#include "stalkermp/core/FrameDispatcher.h"
#include "stalkermp/common/FileSystemUtil.h"
#include "stalkermp/prediction/ClientPresentationDriver.h"
#include "stalkermp/prediction/ClientPresentationPhase.h"

namespace
{
    namespace fs = std::filesystem;

    // Unique temp root per process run so repeated runs never collide.
    std::string TestRoot()
    {
        static const std::string root =
            (fs::temp_directory_path() / ("stalkermp_test_" + std::to_string(
                                              static_cast<unsigned long long>(
                                                  std::chrono::steady_clock::now().time_since_epoch().count()))))
                .string();
        return root;
    }

    stalkermp::InitializeParams MakeParams(const std::string& suffix)
    {
        stalkermp::InitializeParams params;
        params.configDirectory = stalkermp::common::JoinPath(TestRoot(), suffix + "/config");
        params.logDirectory = stalkermp::common::JoinPath(TestRoot(), suffix + "/logs");
        params.engineVersion = "X-Ray Monolith v1.5.3";
        return params;
    }
} // namespace

TEST(Bootstrap, InitializeAndShutdown)
{
    const auto params = MakeParams("basic");

    EXPECT_FALSE(stalkermp::IsInitialized());
    ASSERT_TRUE(stalkermp::Initialize(params));
    EXPECT_TRUE(stalkermp::IsInitialized());

    // Default config files were created.
    EXPECT_TRUE(stalkermp::common::FileExists(
        stalkermp::common::JoinPath(params.configDirectory, "development.cfg")));
    EXPECT_TRUE(stalkermp::common::FileExists(
        stalkermp::common::JoinPath(params.configDirectory, "server.cfg")));
    EXPECT_TRUE(stalkermp::common::FileExists(
        stalkermp::common::JoinPath(params.configDirectory, "client.cfg")));

    // Log file was created.
    EXPECT_TRUE(stalkermp::common::FileExists(
        stalkermp::common::JoinPath(params.logDirectory, "stalkermp.log")));

    stalkermp::Shutdown();
    EXPECT_FALSE(stalkermp::IsInitialized());
}

TEST(Bootstrap, SecondInitializeKeepsRunningInstance)
{
    const auto params = MakeParams("double");

    ASSERT_TRUE(stalkermp::Initialize(params));
    EXPECT_TRUE(stalkermp::Initialize(params)); // Warns, returns true.
    EXPECT_TRUE(stalkermp::IsInitialized());

    stalkermp::Shutdown();
    EXPECT_FALSE(stalkermp::IsInitialized());
}

TEST(Bootstrap, RejectsIncompatibleEngine)
{
    auto params = MakeParams("badengine");
    params.engineVersion = "Some Other Engine 2.0";

    EXPECT_FALSE(stalkermp::Initialize(params));
    EXPECT_FALSE(stalkermp::IsInitialized());
}

TEST(Bootstrap, ShutdownWithoutInitializeIsSafe)
{
    EXPECT_FALSE(stalkermp::IsInitialized());
    stalkermp::Shutdown();
    EXPECT_FALSE(stalkermp::IsInitialized());
}

TEST(Bootstrap, VersionStringIsAlwaysAvailable)
{
    const char* version = stalkermp::VersionString();
    ASSERT_NE(version, nullptr);
    EXPECT_TRUE(std::string(version).find("STALKER-MP") != std::string::npos);
}

TEST(Bootstrap, GeneratedConfigsCarryCurrentVersion)
{
    const auto params = MakeParams("cfgversion");

    ASSERT_TRUE(stalkermp::Initialize(params));
    stalkermp::Shutdown();

    for (const char* name : {"development.cfg", "server.cfg", "client.cfg"})
    {
        const auto loaded = stalkermp::core::config::LoadFile(
            stalkermp::common::JoinPath(params.configDirectory, name));
        ASSERT_TRUE(loaded.HasValue());
        EXPECT_EQ(loaded.Value().GetInt("meta", "config_version").value_or(0),
                  stalkermp::core::config::kCurrentVersion);
    }
}

TEST(Bootstrap, RejectsNewerConfigVersion)
{
    const auto params = MakeParams("cfgnewer");

    // Pre-create a development.cfg claiming a future schema version.
    ASSERT_TRUE(stalkermp::common::EnsureDirectory(params.configDirectory).HasValue());
    ASSERT_TRUE(stalkermp::common::WriteTextFile(
                    stalkermp::common::JoinPath(params.configDirectory, "development.cfg"),
                    "[meta]\nconfig_version = 99\n")
                    .HasValue());

    EXPECT_FALSE(stalkermp::Initialize(params));
    EXPECT_FALSE(stalkermp::IsInitialized());
}

TEST(Bootstrap, WorldSubsystemWiredThroughCompositionRoot)
{
    const auto params = MakeParams("world");

    EXPECT_EQ(stalkermp::detail::GetModuleFrameDispatcher(), nullptr);

    ASSERT_TRUE(stalkermp::Initialize(params));

    // Sprint-013: exactly ten frame subscribers are wired — the nine below plus
    // Scripting (kScripting = 375, Gameplay phase). Sprint-011 wired — World (100),
    // EntityRegistry feed (200), PlayerLifecycle (250), BubbleManager (300),
    // TransitionManager (350), Snapshot (400), Replication (450), Persistence (500),
    // Networking (900, the highest key, networking-last).
    static_assert(stalkermp::core::tick_order::kNetworking > stalkermp::core::tick_order::kPlugins,
                  "networking must be the highest tick_order key");
    // PlayerLifecycle ticks strictly between EntityRegistry and Bubble so a joined
    // player is visible to activation the same frame (Sprint-007, AD-2).
    static_assert(stalkermp::core::tick_order::kEntityRegistry < stalkermp::core::tick_order::kPlayerLifecycle &&
                      stalkermp::core::tick_order::kPlayerLifecycle < stalkermp::core::tick_order::kBubble,
                  "PlayerLifecycle must tick between EntityRegistry and Bubble");
    // Snapshot ticks strictly between Transition (350) and Persistence (500), at the
    // reserved replication slot, after every simulation producer (Sprint-008).
    static_assert(stalkermp::core::tick_order::kAlifeTransition < stalkermp::core::tick_order::kReplication &&
                      stalkermp::core::tick_order::kReplication < stalkermp::core::tick_order::kPersistence,
                  "Snapshot must tick between Transition and Persistence");
    // Replication consumes the snapshot: it ticks strictly between Snapshot (400)
    // and Persistence (500), and before Networking (900) (Sprint-009).
    static_assert(stalkermp::core::tick_order::kReplication < stalkermp::core::tick_order::kReplicationPipeline &&
                      stalkermp::core::tick_order::kReplicationPipeline < stalkermp::core::tick_order::kPersistence &&
                      stalkermp::core::tick_order::kReplicationPipeline < stalkermp::core::tick_order::kNetworking,
                  "Replication must tick between Snapshot and Persistence, before Networking");
    auto* dispatcher = stalkermp::detail::GetModuleFrameDispatcher();
    ASSERT_NE(dispatcher, nullptr);
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(10));

    // Driving frames through the dispatcher is safe and repeatable
    // (stands in for the engine bridge, Milestone W2). Networking ticks last each
    // frame; with the loopback/mock transport and networking disabled by default
    // this is a deterministic no-op.
    dispatcher->Dispatch(0.016);
    dispatcher->Dispatch(0.016);
    dispatcher->Dispatch(0.0);

    // Sprint-009 integration: the composed subsystem (incl. the snapshot manager
    // at 400 and the replication manager at 450) is stable across repeated frames —
    // the wiring is unchanged (still nine subscribers) and driving many frames is a
    // deterministic no-op with no active clients; the replication manager consumes
    // the published snapshot and assembles packets (none, empty world) without error.
    for (int i = 0; i < 8; ++i)
    {
        dispatcher->Dispatch(0.016);
    }
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(10));

    stalkermp::Shutdown();
    EXPECT_EQ(stalkermp::detail::GetModuleFrameDispatcher(), nullptr);
}

// --- Sprint-013 Step-17: Lua ScriptManager wired at the reserved kScripting slot ---
TEST(Bootstrap, ScriptManagerSubscribedAtScriptingTickSlot)
{
    const auto params = MakeParams("lua");

    ASSERT_TRUE(stalkermp::Initialize(params));

    // The reserved scripting slot sits in the Gameplay phase: after ALifeTransition
    // (350) and before Snapshot (kReplication = 400), so authoritative script effects
    // are captured in the same-frame snapshot; networking-last (900) preserved.
    static_assert(stalkermp::core::tick_order::kAlifeTransition < stalkermp::core::tick_order::kScripting &&
                      stalkermp::core::tick_order::kScripting < stalkermp::core::tick_order::kReplication,
                  "Scripting must tick between ALifeTransition and Snapshot");
    static_assert(stalkermp::core::tick_order::kScripting < stalkermp::core::tick_order::kNetworking,
                  "networking-last must be preserved");

    auto* dispatcher = stalkermp::detail::GetModuleFrameDispatcher();
    ASSERT_NE(dispatcher, nullptr);
    // Ten subscribers now: the nine prior + Scripting at kScripting = 375.
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(10));

    // Driving frames dispatches OnTick deterministically (no scripts loaded from the
    // in-memory test source, so it is a stable no-op); the wiring is unchanged.
    for (int i = 0; i < 4; ++i)
    {
        dispatcher->Dispatch(0.016);
    }
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(10));

    stalkermp::Shutdown();
    EXPECT_EQ(stalkermp::detail::GetModuleFrameDispatcher(), nullptr);
}

// --- Sprint-012 Step-17: Save/Load wiring — startup recovery, not a subscriber ---
TEST(Bootstrap, SaveLoadRecoveryIsStartupPhaseNotASubscriber)
{
    const auto params = MakeParams("saveload");

    ASSERT_TRUE(stalkermp::Initialize(params));

    // Step-17 swaps the persistence store for the build-selected save backend (the
    // in-memory backend in tests) and runs RecoveryPipeline ONCE during Initialize —
    // BEFORE the frame bridge and networking. With no prior save it is a non-fatal
    // "fresh world" path, so Initialize still succeeds. Crucially, recovery is NOT a
    // FrameDispatcher subscriber and introduces NO new tick_order key: the subscriber
    // count is UNCHANGED from the Sprint-011 baseline of nine.
    auto* dispatcher = stalkermp::detail::GetModuleFrameDispatcher();
    ASSERT_NE(dispatcher, nullptr);
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(10));

    // The swapped save backend still ticks persistence at kPersistence = 500 (a save
    // path), and driving frames remains a deterministic no-op on an empty world.
    for (int i = 0; i < 4; ++i)
    {
        dispatcher->Dispatch(0.016);
    }
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(10));

    stalkermp::Shutdown();
    EXPECT_EQ(stalkermp::detail::GetModuleFrameDispatcher(), nullptr);
}

// --- Sprint-010 Step-16: ClientPresentationDriver composition-root wiring ------
TEST(Bootstrap, ClientPresentationDriverWiredInIdentityMode)
{
    const auto params = MakeParams("prediction_wiring");

    ASSERT_TRUE(stalkermp::Initialize(params));

    // The driver is wired and, on the host, runs in identity mode.
    const auto* driver = stalkermp::detail::GetModuleClientPresentationDriver();
    ASSERT_NE(driver, nullptr);
    EXPECT_TRUE(driver->IsIdentityMode());

    // It is NOT a FrameDispatcher subscriber (no new tick_order key; networking-last
    // preserved) — the subscriber count is unchanged (nine).
    auto* dispatcher = stalkermp::detail::GetModuleFrameDispatcher();
    ASSERT_NE(dispatcher, nullptr);
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(10));

    // The client-presentation phase runs after Dispatch (as the engine frame bridge
    // does), deterministically and without error, adding no subscribers.
    for (int i = 0; i < 4; ++i)
    {
        dispatcher->Dispatch(0.016);
        stalkermp::detail::AdvanceClientPresentation(0.016);
    }
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(10));
    // Identity mode on the host: no local prediction ran.
    EXPECT_EQ(driver->Diagnostics().Snapshot().predictionsRun, 0u);

    stalkermp::Shutdown();
    EXPECT_EQ(stalkermp::detail::GetModuleClientPresentationDriver(), nullptr);
}

// --- Sprint-008 Step-10: SnapshotManager composition-root wiring --------------
TEST(Bootstrap, SnapshotManagerWiredAtReplicationSlot)
{
    const auto params = MakeParams("snapshot_wiring");

    ASSERT_TRUE(stalkermp::Initialize(params));
    auto* dispatcher = stalkermp::detail::GetModuleFrameDispatcher();
    ASSERT_NE(dispatcher, nullptr);

    // Registration + initialization: the snapshot + replication + persistence
    // managers brought the subscriber count to nine (World, EntityRegistry,
    // PlayerLifecycle, Bubble, Transition, Snapshot, Replication, Persistence,
    // Networking) and Initialize succeeded.
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(10));

    // Tick subscription at kReplication = 400 (after Transition, before Networking):
    // driving frames dispatches the snapshot tick deterministically with no error.
    dispatcher->Dispatch(0.016);
    dispatcher->Dispatch(0.016);
    dispatcher->Dispatch(0.0);
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(10)); // wiring stable

    // Reverse-order shutdown + unsubscription during teardown: the managers are
    // unsubscribed before ShutdownAll and the whole runtime (dispatcher included)
    // is torn down cleanly.
    stalkermp::Shutdown();
    EXPECT_FALSE(stalkermp::IsInitialized());
    EXPECT_EQ(stalkermp::detail::GetModuleFrameDispatcher(), nullptr);

    // Re-initialize + shut down again: teardown fully unsubscribed the managers
    // (no stale subscriber) so a fresh composition wires exactly nine again.
    ASSERT_TRUE(stalkermp::Initialize(params));
    auto* dispatcher2 = stalkermp::detail::GetModuleFrameDispatcher();
    ASSERT_NE(dispatcher2, nullptr);
    EXPECT_EQ(dispatcher2->SubscriberCount(), static_cast<std::size_t>(10));
    stalkermp::Shutdown();
    EXPECT_EQ(stalkermp::detail::GetModuleFrameDispatcher(), nullptr);
}

// --- Sprint-011 Step-14: PersistenceManager composition-root wiring -----------
TEST(Bootstrap, PersistenceManagerWiredAtPersistenceSlot)
{
    const auto params = MakeParams("persistence_wiring");

    // Ordering: persistence ticks strictly after Replication (450) and before
    // Networking (900), at the reserved kPersistence = 500.
    static_assert(stalkermp::core::tick_order::kReplicationPipeline < stalkermp::core::tick_order::kPersistence &&
                      stalkermp::core::tick_order::kPersistence < stalkermp::core::tick_order::kNetworking,
                  "Persistence must tick between Replication and Networking");

    ASSERT_TRUE(stalkermp::Initialize(params));

    // The default [persistence] config block was written to server.cfg.
    EXPECT_TRUE(stalkermp::common::FileExists(
        stalkermp::common::JoinPath(params.configDirectory, "server.cfg")));

    auto* dispatcher = stalkermp::detail::GetModuleFrameDispatcher();
    ASSERT_NE(dispatcher, nullptr);
    // Nine subscribers now (Persistence added at 500); Initialize succeeded.
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(10));

    // Driving frames dispatches the persistence tick deterministically with no error
    // (autosave disabled by default; no snapshot consumer errors), wiring stable.
    for (int i = 0; i < 8; ++i)
    {
        dispatcher->Dispatch(0.016);
    }
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(10));

    // Reverse-order teardown: persistence is unsubscribed before ShutdownAll and the
    // runtime is destroyed cleanly (store outlives the manager).
    stalkermp::Shutdown();
    EXPECT_FALSE(stalkermp::IsInitialized());
    EXPECT_EQ(stalkermp::detail::GetModuleFrameDispatcher(), nullptr);

    // A fresh composition re-wires exactly nine (no stale persistence subscriber).
    ASSERT_TRUE(stalkermp::Initialize(params));
    auto* dispatcher2 = stalkermp::detail::GetModuleFrameDispatcher();
    ASSERT_NE(dispatcher2, nullptr);
    EXPECT_EQ(dispatcher2->SubscriberCount(), static_cast<std::size_t>(10));
    stalkermp::Shutdown();
    EXPECT_EQ(stalkermp::detail::GetModuleFrameDispatcher(), nullptr);
}

// --- Sprint-009 Step-12: ReplicationManager composition-root wiring -----------
TEST(Bootstrap, ReplicationManagerWiredAtPipelineSlot)
{
    const auto params = MakeParams("replication_wiring");

    ASSERT_TRUE(stalkermp::Initialize(params));
    auto* dispatcher = stalkermp::detail::GetModuleFrameDispatcher();
    ASSERT_NE(dispatcher, nullptr);

    // Registration + initialization: the replication manager brought the subscriber
    // count to eight and Initialize succeeded (single instance; no duplicate reg).
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(10));

    // Placement: Replication ticks at kReplicationPipeline = 450, after Snapshot
    // (400) and before Persistence (500)/Networking (900).
    static_assert(stalkermp::core::tick_order::kReplication <
                          stalkermp::core::tick_order::kReplicationPipeline &&
                      stalkermp::core::tick_order::kReplicationPipeline <
                          stalkermp::core::tick_order::kNetworking,
                  "Replication ticks between Snapshot and Networking");

    // Driving frames dispatches Snapshot (400) then Replication (450) each frame,
    // deterministically, with no error and stable wiring.
    dispatcher->Dispatch(0.016);
    dispatcher->Dispatch(0.016);
    dispatcher->Dispatch(0.0);
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(10));

    // Reverse-order teardown fully unsubscribes the replication manager.
    stalkermp::Shutdown();
    EXPECT_FALSE(stalkermp::IsInitialized());
    EXPECT_EQ(stalkermp::detail::GetModuleFrameDispatcher(), nullptr);

    ASSERT_TRUE(stalkermp::Initialize(params));
    auto* dispatcher2 = stalkermp::detail::GetModuleFrameDispatcher();
    ASSERT_NE(dispatcher2, nullptr);
    EXPECT_EQ(dispatcher2->SubscriberCount(), static_cast<std::size_t>(10)); // no stale subscriber
    stalkermp::Shutdown();
    EXPECT_EQ(stalkermp::detail::GetModuleFrameDispatcher(), nullptr);
}

TEST(Bootstrap, AcceptsConfigWithoutVersionWithWarning)
{
    const auto params = MakeParams("cfgmissing");

    // Pre-create a version-less development.cfg (pre-versioning layout).
    ASSERT_TRUE(stalkermp::common::EnsureDirectory(params.configDirectory).HasValue());
    ASSERT_TRUE(stalkermp::common::WriteTextFile(
                    stalkermp::common::JoinPath(params.configDirectory, "development.cfg"),
                    "[log]\nlevel = info\n")
                    .HasValue());

    EXPECT_TRUE(stalkermp::Initialize(params)); // Warns, assumes version 1.
    stalkermp::Shutdown();
}

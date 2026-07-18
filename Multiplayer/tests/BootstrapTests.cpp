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

    // Sprint-009: exactly eight frame subscribers are wired — World (100),
    // EntityRegistry feed (200), PlayerLifecycle (250), BubbleManager (300),
    // TransitionManager (350), Snapshot (400), Replication (450), Networking (900,
    // the highest key, networking-last).
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
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(8));

    // Driving frames through the dispatcher is safe and repeatable
    // (stands in for the engine bridge, Milestone W2). Networking ticks last each
    // frame; with the loopback/mock transport and networking disabled by default
    // this is a deterministic no-op.
    dispatcher->Dispatch(0.016);
    dispatcher->Dispatch(0.016);
    dispatcher->Dispatch(0.0);

    // Sprint-009 integration: the composed subsystem (incl. the snapshot manager
    // at 400 and the replication manager at 450) is stable across repeated frames —
    // the wiring is unchanged (still eight subscribers) and driving many frames is a
    // deterministic no-op with no active clients; the replication manager consumes
    // the published snapshot and assembles packets (none, empty world) without error.
    for (int i = 0; i < 8; ++i)
    {
        dispatcher->Dispatch(0.016);
    }
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(8));

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
    // preserved) — the subscriber count is unchanged (eight).
    auto* dispatcher = stalkermp::detail::GetModuleFrameDispatcher();
    ASSERT_NE(dispatcher, nullptr);
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(8));

    // The client-presentation phase runs after Dispatch (as the engine frame bridge
    // does), deterministically and without error, adding no subscribers.
    for (int i = 0; i < 4; ++i)
    {
        dispatcher->Dispatch(0.016);
        stalkermp::detail::AdvanceClientPresentation(0.016);
    }
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(8));
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

    // Registration + initialization: the snapshot + replication managers brought
    // the subscriber count to eight (World, EntityRegistry, PlayerLifecycle, Bubble,
    // Transition, Snapshot, Replication, Networking) and Initialize succeeded.
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(8));

    // Tick subscription at kReplication = 400 (after Transition, before Networking):
    // driving frames dispatches the snapshot tick deterministically with no error.
    dispatcher->Dispatch(0.016);
    dispatcher->Dispatch(0.016);
    dispatcher->Dispatch(0.0);
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(8)); // wiring stable

    // Reverse-order shutdown + unsubscription during teardown: the managers are
    // unsubscribed before ShutdownAll and the whole runtime (dispatcher included)
    // is torn down cleanly.
    stalkermp::Shutdown();
    EXPECT_FALSE(stalkermp::IsInitialized());
    EXPECT_EQ(stalkermp::detail::GetModuleFrameDispatcher(), nullptr);

    // Re-initialize + shut down again: teardown fully unsubscribed both managers
    // (no stale subscriber) so a fresh composition wires exactly eight again.
    ASSERT_TRUE(stalkermp::Initialize(params));
    auto* dispatcher2 = stalkermp::detail::GetModuleFrameDispatcher();
    ASSERT_NE(dispatcher2, nullptr);
    EXPECT_EQ(dispatcher2->SubscriberCount(), static_cast<std::size_t>(8));
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
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(8));

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
    EXPECT_EQ(dispatcher->SubscriberCount(), static_cast<std::size_t>(8));

    // Reverse-order teardown fully unsubscribes the replication manager.
    stalkermp::Shutdown();
    EXPECT_FALSE(stalkermp::IsInitialized());
    EXPECT_EQ(stalkermp::detail::GetModuleFrameDispatcher(), nullptr);

    ASSERT_TRUE(stalkermp::Initialize(params));
    auto* dispatcher2 = stalkermp::detail::GetModuleFrameDispatcher();
    ASSERT_NE(dispatcher2, nullptr);
    EXPECT_EQ(dispatcher2->SubscriberCount(), static_cast<std::size_t>(8)); // no stale subscriber
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

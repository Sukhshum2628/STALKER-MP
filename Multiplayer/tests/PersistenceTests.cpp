// STALKER-MP — Persistence framework tests (Sprint-011)
//
// Step 1: value types, enumerations, POD structures, and const char* name
//         helpers. Engine-free and OS-free. Types only — no configuration,
//         queue, worker, scheduler, manager, storage, or thread.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include "stalkermp/core/Config.h"
#include "stalkermp/persistence/PersistenceConfiguration.h"
#include "stalkermp/persistence/PersistenceTypes.h"

using namespace stalkermp;

// --- Enum layout: fixed std::uint8_t underlying type (deterministic ABI) -----
TEST(PersistenceTypesStep1, EnumsHaveUint8UnderlyingType)
{
    static_assert(std::is_same_v<std::underlying_type_t<persistence::PersistenceOutcome>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<persistence::SaveTrigger>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<persistence::SaveState>, std::uint8_t>);
    SUCCEED();
}

// --- Reserved 0 carries the neutral/safe meaning ------------------------------
TEST(PersistenceTypesStep1, ReservedZeroIsNeutral)
{
    static_assert(static_cast<std::uint8_t>(persistence::PersistenceOutcome::Ok) == 0u);
    static_assert(static_cast<std::uint8_t>(persistence::SaveTrigger::Manual) == 0u);
    static_assert(static_cast<std::uint8_t>(persistence::SaveState::Pending) == 0u);
    SUCCEED();
}

// --- POD value captures: trivially copyable + documented defaults -------------
TEST(PersistenceTypesStep1, PodDefaults)
{
    static_assert(std::is_trivially_copyable_v<persistence::SaveRequest>);
    static_assert(std::is_trivially_copyable_v<persistence::SaveMetadata>);
    static_assert(std::is_trivially_copyable_v<persistence::PersistenceStatistics>);
    static_assert(std::is_trivially_copyable_v<persistence::PersistenceConsistency>);

    const persistence::SaveRequest req{};
    EXPECT_EQ(req.id, 0u);
    EXPECT_EQ(req.trigger, persistence::SaveTrigger::Manual);
    EXPECT_EQ(req.requestTick, 0u);

    const persistence::SaveMetadata meta{};
    EXPECT_EQ(meta.saveId, 0u);
    EXPECT_EQ(meta.simulationTick, 0u);
    EXPECT_EQ(meta.playerCount, 0u);
    EXPECT_EQ(meta.entityCount, 0u);
    EXPECT_EQ(meta.worldVersion, 0u);
    EXPECT_EQ(meta.buildVersion, 0u);
    EXPECT_EQ(meta.checksum, 0u);
    EXPECT_EQ(meta.timestampWallClock, 0u);

    const persistence::PersistenceStatistics stats{};
    EXPECT_EQ(stats.saveRequests, 0u);
    EXPECT_EQ(stats.autosaves, 0u);
    EXPECT_EQ(stats.completedSaves, 0u);
    EXPECT_EQ(stats.failedSaves, 0u);
    EXPECT_EQ(stats.retries, 0u);
    EXPECT_EQ(stats.queueOverflows, 0u);
    EXPECT_EQ(stats.currentQueueDepth, 0u);
    EXPECT_EQ(stats.maxQueueDepth, 0u);
    EXPECT_EQ(stats.lastSaveDurationMicros, 0u);
    EXPECT_EQ(stats.maxSaveDurationMicros, 0u);
    EXPECT_EQ(stats.timestampWallClock, 0u);
}

// --- Consistency defaults healthy; any false flag makes it unhealthy ----------
TEST(PersistenceTypesStep1, ConsistencyHealthyByDefault)
{
    persistence::PersistenceConsistency c{};
    EXPECT_TRUE(c.readOnly);
    EXPECT_TRUE(c.boundedQueue);
    EXPECT_TRUE(c.versionTracked);
    EXPECT_TRUE(c.failureIsolated);
    EXPECT_TRUE(c.snapshotImmutable);
    EXPECT_TRUE(c.IsHealthy());

    c.readOnly = false;
    EXPECT_FALSE(c.IsHealthy());

    persistence::PersistenceConsistency d{};
    d.failureIsolated = false;
    EXPECT_FALSE(d.IsHealthy());
}

// --- PersistenceOutcome names: every enumerator + Unknown fallback ------------
TEST(PersistenceTypesStep1, OutcomeNamesTotal)
{
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::Ok), "Ok");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::QueueFull), "QueueFull");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::VersionMismatch),
                 "VersionMismatch");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::IntegrityFailure),
                 "IntegrityFailure");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::IncompleteSnapshot),
                 "IncompleteSnapshot");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::WorkerUnavailable),
                 "WorkerUnavailable");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::StorageUnavailable),
                 "StorageUnavailable");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::NothingToPersist),
                 "NothingToPersist");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(static_cast<persistence::PersistenceOutcome>(200)), "Unknown");
}

// --- SaveTrigger names: every enumerator + Unknown fallback -------------------
TEST(PersistenceTypesStep1, TriggerNamesTotal)
{
    EXPECT_STREQ(persistence::SaveTriggerName(persistence::SaveTrigger::Manual), "Manual");
    EXPECT_STREQ(persistence::SaveTriggerName(persistence::SaveTrigger::Autosave), "Autosave");
    EXPECT_STREQ(persistence::SaveTriggerName(persistence::SaveTrigger::Administrative), "Administrative");
    EXPECT_STREQ(persistence::SaveTriggerName(persistence::SaveTrigger::Shutdown), "Shutdown");
    EXPECT_STREQ(persistence::SaveTriggerName(persistence::SaveTrigger::Deferred), "Deferred");
    EXPECT_STREQ(persistence::SaveTriggerName(static_cast<persistence::SaveTrigger>(200)), "Unknown");
}

// --- SaveState names: every enumerator + Unknown fallback --------------------
TEST(PersistenceTypesStep1, StateNamesTotal)
{
    EXPECT_STREQ(persistence::SaveStateName(persistence::SaveState::Pending), "Pending");
    EXPECT_STREQ(persistence::SaveStateName(persistence::SaveState::Validating), "Validating");
    EXPECT_STREQ(persistence::SaveStateName(persistence::SaveState::Persisting), "Persisting");
    EXPECT_STREQ(persistence::SaveStateName(persistence::SaveState::Completed), "Completed");
    EXPECT_STREQ(persistence::SaveStateName(persistence::SaveState::Failed), "Failed");
    EXPECT_STREQ(persistence::SaveStateName(static_cast<persistence::SaveState>(200)), "Unknown");
}

// --- Name helpers are constexpr (compile-time evaluable) ----------------------
TEST(PersistenceTypesStep1, NameHelpersAreConstexpr)
{
    static_assert(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::Ok)[0] == 'O');
    static_assert(persistence::SaveTriggerName(persistence::SaveTrigger::Autosave)[0] == 'A');
    static_assert(persistence::SaveStateName(persistence::SaveState::Failed)[0] == 'F');
    SUCCEED();
}

// ============================================================================
// Step 2 — PersistenceConfiguration::FromConfig
// ============================================================================

// --- Missing [persistence] section => all documented defaults -----------------
TEST(PersistenceConfigStep2, DefaultsWhenSectionAbsent)
{
    core::ConfigStore store;
    const auto r = persistence::PersistenceConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.queueDepth, 16u);
    EXPECT_EQ(c.autosaveIntervalTicks, 0u);
    EXPECT_EQ(c.maxRetries, 3u);
    EXPECT_EQ(c.retryBackoffTicks, 30u);
    EXPECT_EQ(c.backpressureHighWatermark, 12u);
    EXPECT_EQ(c.backpressureLowWatermark, 4u);
    EXPECT_EQ(c.version, 1u);
}

// --- Each field parses a valid supplied value (override) ----------------------
TEST(PersistenceConfigStep2, OverridesParsed)
{
    core::ConfigStore store;
    store.Set("persistence", "queue_depth", "64");
    store.Set("persistence", "autosave_interval_ticks", "1800");
    store.Set("persistence", "max_retries", "5");
    store.Set("persistence", "retry_backoff_ticks", "60");
    store.Set("persistence", "backpressure_high_watermark", "50");
    store.Set("persistence", "backpressure_low_watermark", "10");
    store.Set("persistence", "version", "2");
    const auto r = persistence::PersistenceConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.queueDepth, 64u);
    EXPECT_EQ(c.autosaveIntervalTicks, 1800u);
    EXPECT_EQ(c.maxRetries, 5u);
    EXPECT_EQ(c.retryBackoffTicks, 60u);
    EXPECT_EQ(c.backpressureHighWatermark, 50u);
    EXPECT_EQ(c.backpressureLowWatermark, 10u);
    EXPECT_EQ(c.version, 2u);
}

// --- Zeroable fields accept 0 (autosave disabled; no retry/backoff; wm off) ----
TEST(PersistenceConfigStep2, ZeroableFieldsAcceptZero)
{
    core::ConfigStore store;
    store.Set("persistence", "autosave_interval_ticks", "0");
    store.Set("persistence", "max_retries", "0");
    store.Set("persistence", "retry_backoff_ticks", "0");
    store.Set("persistence", "backpressure_high_watermark", "0");
    store.Set("persistence", "backpressure_low_watermark", "0");
    const auto r = persistence::PersistenceConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.autosaveIntervalTicks, 0u);
    EXPECT_EQ(c.maxRetries, 0u);
    EXPECT_EQ(c.retryBackoffTicks, 0u);
    EXPECT_EQ(c.backpressureHighWatermark, 0u);
    EXPECT_EQ(c.backpressureLowWatermark, 0u);
}

// --- Boundary minimums accepted (queueDepth = version = 1; wm = queueDepth) ----
TEST(PersistenceConfigStep2, BoundaryMinimumsAccepted)
{
    core::ConfigStore store;
    store.Set("persistence", "queue_depth", "1");
    store.Set("persistence", "backpressure_high_watermark", "1"); // == queueDepth
    store.Set("persistence", "backpressure_low_watermark", "1");  // == high
    store.Set("persistence", "version", "1");
    const auto r = persistence::PersistenceConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.queueDepth, 1u);
    EXPECT_EQ(c.backpressureHighWatermark, 1u);
    EXPECT_EQ(c.backpressureLowWatermark, 1u);
    EXPECT_EQ(c.version, 1u);
}

// --- Invalid per-field values are rejected (value outcome) --------------------
TEST(PersistenceConfigStep2, InvalidPerFieldValuesRejected)
{
    {
        core::ConfigStore s;
        s.Set("persistence", "queue_depth", "0"); // below min 1
        EXPECT_FALSE(persistence::PersistenceConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("persistence", "version", "0"); // below min 1
        EXPECT_FALSE(persistence::PersistenceConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("persistence", "queue_depth", "-5"); // negative
        EXPECT_FALSE(persistence::PersistenceConfiguration::FromConfig(s).HasValue());
    }
}

// --- Cross-field watermark ordering enforced: low <= high <= queueDepth --------
TEST(PersistenceConfigStep2, WatermarkOrderingEnforced)
{
    {
        // low > high
        core::ConfigStore s;
        s.Set("persistence", "backpressure_high_watermark", "4");
        s.Set("persistence", "backpressure_low_watermark", "8");
        EXPECT_FALSE(persistence::PersistenceConfiguration::FromConfig(s).HasValue());
    }
    {
        // high > queueDepth
        core::ConfigStore s;
        s.Set("persistence", "queue_depth", "8");
        s.Set("persistence", "backpressure_high_watermark", "16");
        s.Set("persistence", "backpressure_low_watermark", "2");
        EXPECT_FALSE(persistence::PersistenceConfiguration::FromConfig(s).HasValue());
    }
    {
        // valid ordering accepted
        core::ConfigStore s;
        s.Set("persistence", "queue_depth", "8");
        s.Set("persistence", "backpressure_high_watermark", "8");
        s.Set("persistence", "backpressure_low_watermark", "8");
        EXPECT_TRUE(persistence::PersistenceConfiguration::FromConfig(s).HasValue());
    }
}

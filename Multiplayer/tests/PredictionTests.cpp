// STALKER-MP — Client prediction & interpolation tests (Sprint-010)
//
// Step 1: value types, enumerations, POD structures, and const char* name
//         helpers. Engine-free and OS-free. Types only — no configuration,
//         buffers, prediction/interpolation logic, seams, driver, or diagnostics.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include "stalkermp/core/Config.h"
#include "stalkermp/prediction/PredictionConfiguration.h"
#include "stalkermp/prediction/PredictionTypes.h"

using namespace stalkermp;

// --- Enum layout: fixed std::uint8_t underlying type (deterministic ABI) -----
TEST(PredictionTypesStep1, EnumsHaveUint8UnderlyingType)
{
    static_assert(std::is_same_v<std::underlying_type_t<prediction::PredictionOutcome>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<prediction::CorrectionKind>, std::uint8_t>);
    SUCCEED();
}

// --- POD value captures: trivially copyable + documented defaults -------------
TEST(PredictionTypesStep1, PodDefaults)
{
    static_assert(std::is_trivially_copyable_v<prediction::InputCommand>);
    static_assert(std::is_trivially_copyable_v<prediction::PredictedState>);
    static_assert(std::is_trivially_copyable_v<prediction::InterpolatedState>);
    static_assert(std::is_trivially_copyable_v<prediction::PredictionStatistics>);
    static_assert(std::is_trivially_copyable_v<prediction::PredictionConsistency>);

    const prediction::InputCommand in{};
    EXPECT_EQ(in.sequence, 0u);
    EXPECT_EQ(in.tick, 0u);
    EXPECT_EQ(in.yaw, 0.0f);
    EXPECT_EQ(in.actionBits, 0u);

    const prediction::PredictedState ps{};
    EXPECT_EQ(ps.id.value, 0u);
    EXPECT_EQ(ps.tick, 0u);
    EXPECT_EQ(ps.yaw, 0.0f);
    EXPECT_EQ(ps.stateFlags, 0u);

    const prediction::InterpolatedState is{};
    EXPECT_EQ(is.id.value, 0u);
    EXPECT_EQ(is.yaw, 0.0f);

    const prediction::PredictionStatistics st{};
    EXPECT_EQ(st.predictionsRun, 0u);
    EXPECT_EQ(st.corrections, 0u);
    EXPECT_EQ(st.snaps, 0u);
    EXPECT_EQ(st.interpolations, 0u);
    EXPECT_EQ(st.overflows, 0u);
    EXPECT_EQ(st.inputsRecorded, 0u);
    EXPECT_EQ(st.maxCorrectionMagnitude, 0u);
    EXPECT_EQ(st.timestampWallClock, 0u);
}

// --- SnapshotFrame: value-only aggregate, ordered containers, defaults --------
TEST(PredictionTypesStep1, SnapshotFrameDefaults)
{
    const prediction::SnapshotFrame f{};
    EXPECT_EQ(f.tick, 0u);
    EXPECT_EQ(f.ackedInputSequence, 0u);
    EXPECT_TRUE(f.entities.empty());
    EXPECT_TRUE(f.players.empty());
}

// --- Consistency: healthy by default; any false clears health -----------------
TEST(PredictionTypesStep1, ConsistencyHealth)
{
    prediction::PredictionConsistency c{};
    EXPECT_TRUE(c.IsHealthy());
    c.clientOnly = false;
    EXPECT_FALSE(c.IsHealthy());
    c = prediction::PredictionConsistency{};
    c.noAuthoritativeMutation = false;
    EXPECT_FALSE(c.IsHealthy());
    c = prediction::PredictionConsistency{};
    c.hostAuthorityPreserved = false;
    EXPECT_FALSE(c.IsHealthy());
}

// --- Name() helpers are total (incl. out-of-range sentinel) -------------------
TEST(PredictionTypesStep1, OutcomeNamesTotal)
{
    EXPECT_STREQ(prediction::PredictionOutcomeName(prediction::PredictionOutcome::Ok), "Ok");
    EXPECT_STREQ(prediction::PredictionOutcomeName(prediction::PredictionOutcome::ClientOnly), "ClientOnly");
    EXPECT_STREQ(prediction::PredictionOutcomeName(prediction::PredictionOutcome::BufferOverflow), "BufferOverflow");
    EXPECT_STREQ(prediction::PredictionOutcomeName(prediction::PredictionOutcome::SequenceMismatch), "SequenceMismatch");
    EXPECT_STREQ(prediction::PredictionOutcomeName(prediction::PredictionOutcome::NoAuthoritativeFrame),
                 "NoAuthoritativeFrame");
    EXPECT_STREQ(prediction::PredictionOutcomeName(prediction::PredictionOutcome::CorrectionRejected),
                 "CorrectionRejected");
    EXPECT_STREQ(prediction::PredictionOutcomeName(static_cast<prediction::PredictionOutcome>(200)), "Unknown");
}

TEST(PredictionTypesStep1, CorrectionNamesTotal)
{
    EXPECT_STREQ(prediction::CorrectionKindName(prediction::CorrectionKind::None), "None");
    EXPECT_STREQ(prediction::CorrectionKindName(prediction::CorrectionKind::Smoothed), "Smoothed");
    EXPECT_STREQ(prediction::CorrectionKindName(prediction::CorrectionKind::Snapped), "Snapped");
    EXPECT_STREQ(prediction::CorrectionKindName(static_cast<prediction::CorrectionKind>(200)), "Unknown");
}

// ============================================================================
// Step 2 — PredictionConfiguration::FromConfig
// ============================================================================

// --- Missing [prediction] section => all documented defaults ------------------
TEST(PredictionConfigStep2, DefaultsWhenSectionAbsent)
{
    core::ConfigStore store;
    const auto r = prediction::PredictionConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.inputBufferDepth, 128u);
    EXPECT_EQ(c.stateBufferDepth, 128u);
    EXPECT_EQ(c.interpolationDelayTicks, 2u);
    EXPECT_EQ(c.maxPredictionTicks, 8u);
    EXPECT_EQ(c.positionCorrectionThresholdMm, 250u);
    EXPECT_EQ(c.rotationCorrectionThresholdMrad, 100u);
    EXPECT_EQ(c.velocityCorrectionThresholdMm, 500u);
    EXPECT_EQ(c.version, 1u);
}

// --- Each field parses a valid supplied value (override) ----------------------
TEST(PredictionConfigStep2, OverridesParsed)
{
    core::ConfigStore store;
    store.Set("prediction", "input_buffer_depth", "256");
    store.Set("prediction", "state_buffer_depth", "64");
    store.Set("prediction", "interpolation_delay_ticks", "3");
    store.Set("prediction", "max_prediction_ticks", "16");
    store.Set("prediction", "position_correction_threshold_mm", "500");
    store.Set("prediction", "rotation_correction_threshold_mrad", "200");
    store.Set("prediction", "velocity_correction_threshold_mm", "750");
    store.Set("prediction", "version", "2");
    const auto r = prediction::PredictionConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.inputBufferDepth, 256u);
    EXPECT_EQ(c.stateBufferDepth, 64u);
    EXPECT_EQ(c.interpolationDelayTicks, 3u);
    EXPECT_EQ(c.maxPredictionTicks, 16u);
    EXPECT_EQ(c.positionCorrectionThresholdMm, 500u);
    EXPECT_EQ(c.rotationCorrectionThresholdMrad, 200u);
    EXPECT_EQ(c.velocityCorrectionThresholdMm, 750u);
    EXPECT_EQ(c.version, 2u);
}

// --- Threshold / delay fields accept 0 (advisory) -----------------------------
TEST(PredictionConfigStep2, ZeroableFieldsAcceptZero)
{
    core::ConfigStore store;
    store.Set("prediction", "interpolation_delay_ticks", "0");
    store.Set("prediction", "position_correction_threshold_mm", "0");
    store.Set("prediction", "rotation_correction_threshold_mrad", "0");
    store.Set("prediction", "velocity_correction_threshold_mm", "0");
    const auto r = prediction::PredictionConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(r.Value().interpolationDelayTicks, 0u);
    EXPECT_EQ(r.Value().positionCorrectionThresholdMm, 0u);
    EXPECT_EQ(r.Value().rotationCorrectionThresholdMrad, 0u);
    EXPECT_EQ(r.Value().velocityCorrectionThresholdMm, 0u);
}

// --- Invalid per-field values are rejected (value outcome) --------------------
TEST(PredictionConfigStep2, InvalidValuesRejected)
{
    {
        core::ConfigStore s;
        s.Set("prediction", "input_buffer_depth", "0"); // below min 1
        EXPECT_FALSE(prediction::PredictionConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("prediction", "state_buffer_depth", "0"); // below min 1
        EXPECT_FALSE(prediction::PredictionConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("prediction", "max_prediction_ticks", "0"); // below min 1
        EXPECT_FALSE(prediction::PredictionConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("prediction", "version", "0"); // below min 1
        EXPECT_FALSE(prediction::PredictionConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("prediction", "input_buffer_depth", "-5"); // negative
        EXPECT_FALSE(prediction::PredictionConfiguration::FromConfig(s).HasValue());
    }
}

// --- Boundary minimums (== 1) are accepted ------------------------------------
TEST(PredictionConfigStep2, BoundaryMinimumsAccepted)
{
    core::ConfigStore store;
    store.Set("prediction", "input_buffer_depth", "1");
    store.Set("prediction", "state_buffer_depth", "1");
    store.Set("prediction", "max_prediction_ticks", "1");
    store.Set("prediction", "version", "1");
    const auto r = prediction::PredictionConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(r.Value().inputBufferDepth, 1u);
    EXPECT_EQ(r.Value().maxPredictionTicks, 1u);
    EXPECT_EQ(r.Value().version, 1u);
}

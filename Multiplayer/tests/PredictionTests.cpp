// STALKER-MP — Client prediction & interpolation tests (Sprint-010)
//
// Step 1: value types, enumerations, POD structures, and const char* name
//         helpers. Engine-free and OS-free. Types only — no configuration,
//         buffers, prediction/interpolation logic, seams, driver, or diagnostics.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

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

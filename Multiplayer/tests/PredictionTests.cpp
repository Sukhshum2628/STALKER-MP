// STALKER-MP — Client prediction & interpolation tests (Sprint-010)
//
// Step 1: value types, enumerations, POD structures, and const char* name
//         helpers. Engine-free and OS-free. Types only — no configuration,
//         buffers, prediction/interpolation logic, seams, driver, or diagnostics.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include "stalkermp/core/Config.h"
#include "stalkermp/prediction/InputBuffer.h"
#include "stalkermp/prediction/PredictionConfiguration.h"
#include "stalkermp/prediction/PredictionStep.h"
#include "stalkermp/prediction/StateBuffer.h"
#include "stalkermp/prediction/PredictionTypes.h"

using namespace stalkermp;

namespace
{
    prediction::InputCommand Cmd(std::uint64_t sequence, std::uint64_t tick)
    {
        prediction::InputCommand c{};
        c.sequence = sequence;
        c.tick = tick;
        return c;
    }
    prediction::PredictedState St(std::uint32_t id, std::uint64_t tick, float x)
    {
        prediction::PredictedState s{};
        s.id = world::EntityId{id};
        s.tick = tick;
        s.position = world::Vec3{x, 0.0f, 0.0f};
        return s;
    }
} // namespace

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

// ============================================================================
// Step 3 — InputBuffer
// ============================================================================

// --- Push: ascending sequence; regress rejected; FIFO; size/capacity ----------
TEST(InputBufferStep3, PushAscendingAndSequenceMismatch)
{
    prediction::InputBuffer buffer{4};
    EXPECT_EQ(buffer.Capacity(), 4u);
    EXPECT_TRUE(buffer.Empty());

    EXPECT_EQ(buffer.Push(Cmd(1, 10)), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(buffer.Push(Cmd(2, 11)), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(buffer.Size(), 2u);
    EXPECT_EQ(buffer.LastSequence(), 2u);

    // A regress or duplicate sequence is rejected; the buffer is unchanged.
    EXPECT_EQ(buffer.Push(Cmd(2, 12)), prediction::PredictionOutcome::SequenceMismatch);
    EXPECT_EQ(buffer.Push(Cmd(1, 13)), prediction::PredictionOutcome::SequenceMismatch);
    EXPECT_EQ(buffer.Size(), 2u);
}

// --- Overflow is a value outcome; the buffer is unchanged ---------------------
TEST(InputBufferStep3, OverflowValueOutcome)
{
    prediction::InputBuffer buffer{2};
    EXPECT_EQ(buffer.Push(Cmd(1, 0)), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(buffer.Push(Cmd(2, 0)), prediction::PredictionOutcome::Ok);
    EXPECT_TRUE(buffer.Full());
    EXPECT_EQ(buffer.Push(Cmd(3, 0)), prediction::PredictionOutcome::BufferOverflow);
    EXPECT_EQ(buffer.Size(), 2u); // unchanged
}

// --- PruneUpTo discards acknowledged inputs; sequences stay monotonic ---------
TEST(InputBufferStep3, PruneUpTo)
{
    prediction::InputBuffer buffer{4};
    (void)buffer.Push(Cmd(1, 0));
    (void)buffer.Push(Cmd(2, 0));
    (void)buffer.Push(Cmd(3, 0));
    (void)buffer.Push(Cmd(4, 0));

    buffer.PruneUpTo(2); // acknowledge sequences 1 and 2
    EXPECT_EQ(buffer.Size(), 2u);

    // A freed slot is reusable (ring wrap); sequences remain strictly ascending.
    EXPECT_EQ(buffer.Push(Cmd(5, 0)), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(buffer.Size(), 3u);
    // Pruning past everything empties the buffer (benign).
    buffer.PruneUpTo(100);
    EXPECT_TRUE(buffer.Empty());
    EXPECT_EQ(buffer.LastSequence(), 5u); // monotonic guard persists after prune
    EXPECT_EQ(buffer.Push(Cmd(5, 0)), prediction::PredictionOutcome::SequenceMismatch);
}

// --- Pending returns unacknowledged inputs ascending (for replay) -------------
TEST(InputBufferStep3, PendingAscending)
{
    prediction::InputBuffer buffer{8};
    for (std::uint64_t s = 1; s <= 5; ++s)
    {
        (void)buffer.Push(Cmd(s, s * 10));
    }
    const auto pending = buffer.Pending(/*fromSequence*/ 2);
    ASSERT_EQ(pending.size(), 3u); // sequences 3, 4, 5
    EXPECT_EQ(pending[0].sequence, 3u);
    EXPECT_EQ(pending[1].sequence, 4u);
    EXPECT_EQ(pending[2].sequence, 5u);
    for (std::size_t i = 1; i < pending.size(); ++i)
    {
        EXPECT_TRUE(pending[i - 1].sequence < pending[i].sequence); // strictly ascending
    }
    // Pending does not mutate the buffer.
    EXPECT_EQ(buffer.Size(), 5u);
}

// --- Clear restores the empty initial state -----------------------------------
TEST(InputBufferStep3, Clear)
{
    prediction::InputBuffer buffer{4};
    (void)buffer.Push(Cmd(1, 0));
    (void)buffer.Push(Cmd(2, 0));
    buffer.Clear();
    EXPECT_TRUE(buffer.Empty());
    EXPECT_EQ(buffer.LastSequence(), 0u);
    EXPECT_EQ(buffer.Push(Cmd(1, 0)), prediction::PredictionOutcome::Ok); // sequence restarts
}

// ============================================================================
// Step 4 — StateBuffer
// ============================================================================

// --- Record + lookup by tick; latest; ascending guard -------------------------
TEST(StateBufferStep4, RecordLookupLatest)
{
    prediction::StateBuffer buffer{4};
    EXPECT_EQ(buffer.Capacity(), 4u);
    EXPECT_TRUE(buffer.Empty());
    EXPECT_EQ(buffer.At(1), nullptr);
    EXPECT_EQ(buffer.Latest(), nullptr);

    buffer.Record(St(7, 1, 1.0f));
    buffer.Record(St(7, 2, 2.0f));
    buffer.Record(St(7, 3, 3.0f));
    EXPECT_EQ(buffer.Size(), 3u);

    const prediction::PredictedState* at2 = buffer.At(2);
    ASSERT_NE(at2, nullptr);
    EXPECT_FLOAT_EQ(at2->position.x, 2.0f);
    EXPECT_EQ(buffer.At(99), nullptr); // absent

    const prediction::PredictedState* latest = buffer.Latest();
    ASSERT_NE(latest, nullptr);
    EXPECT_EQ(latest->tick, 3u);

    // A non-ascending tick is ignored (deterministic no-op).
    buffer.Record(St(7, 2, 99.0f));
    EXPECT_EQ(buffer.Size(), 3u);
    EXPECT_FLOAT_EQ(buffer.At(2)->position.x, 2.0f); // unchanged
}

// --- Bounded ring: oldest evicted on overflow (deterministic) -----------------
TEST(StateBufferStep4, BoundedEviction)
{
    prediction::StateBuffer buffer{2};
    buffer.Record(St(1, 1, 1.0f));
    buffer.Record(St(1, 2, 2.0f));
    EXPECT_TRUE(buffer.Full());

    buffer.Record(St(1, 3, 3.0f)); // evicts tick 1
    EXPECT_EQ(buffer.Size(), 2u);
    EXPECT_EQ(buffer.At(1), nullptr);       // evicted
    ASSERT_NE(buffer.At(2), nullptr);
    ASSERT_NE(buffer.At(3), nullptr);
    EXPECT_EQ(buffer.Latest()->tick, 3u);
}

// --- Clear restores the empty initial state -----------------------------------
TEST(StateBufferStep4, Clear)
{
    prediction::StateBuffer buffer{4};
    buffer.Record(St(1, 1, 1.0f));
    buffer.Record(St(1, 2, 2.0f));
    buffer.Clear();
    EXPECT_TRUE(buffer.Empty());
    EXPECT_EQ(buffer.At(1), nullptr);
    buffer.Record(St(1, 1, 5.0f)); // tick sequence restarts after Clear
    ASSERT_NE(buffer.At(1), nullptr);
    EXPECT_FLOAT_EQ(buffer.At(1)->position.x, 5.0f);
}

// ============================================================================
// Step 5 — Deterministic prediction integrator
// ============================================================================

namespace
{
    prediction::InputCommand MoveInput(std::uint64_t seq, std::uint64_t tick, float mx, float mz, float yaw,
                                       std::uint32_t actions = 0)
    {
        prediction::InputCommand c{};
        c.sequence = seq;
        c.tick = tick;
        c.move = world::Vec3{mx, 0.0f, mz};
        c.yaw = yaw;
        c.actionBits = actions;
        return c;
    }
    constexpr float kHalfPi = 1.5707963267948966f;
} // namespace

// --- No input (zero move, unchanged yaw) is the identity for position ---------
TEST(PredictionStepStep5, NoInputIsIdentity)
{
    const prediction::PredictionConfiguration cfg{};
    prediction::PredictedState prior{};
    prior.id = world::EntityId{7};
    prior.position = world::Vec3{5.0f, 1.0f, -3.0f};
    prior.yaw = 0.0f;

    const auto next = prediction::Integrate(prior, MoveInput(1, 10, 0.0f, 0.0f, 0.0f), cfg);
    EXPECT_EQ(next.id.value, 7u);
    EXPECT_EQ(next.tick, 10u);
    EXPECT_FLOAT_EQ(next.position.x, 5.0f);
    EXPECT_FLOAT_EQ(next.position.y, 1.0f);
    EXPECT_FLOAT_EQ(next.position.z, -3.0f);
    EXPECT_FLOAT_EQ(next.velocity.x, 0.0f);
    EXPECT_FLOAT_EQ(next.velocity.z, 0.0f);
}

// --- Movement integrates in world space; yaw rotates the local intent ---------
TEST(PredictionStepStep5, MovementAndRotation)
{
    const prediction::PredictionConfiguration cfg{};
    prediction::PredictedState origin{};

    // Facing yaw 0: local +X moves world +X.
    const auto east = prediction::Integrate(origin, MoveInput(1, 1, 1.0f, 0.0f, 0.0f), cfg);
    EXPECT_FLOAT_EQ(east.position.x, 1.0f);
    EXPECT_FLOAT_EQ(east.position.z, 0.0f);
    EXPECT_FLOAT_EQ(east.yaw, 0.0f);

    // Facing yaw +90deg: local +X rotates to world +Z (x*sin, x*cos with x=cos~0).
    const auto turned = prediction::Integrate(origin, MoveInput(2, 2, 1.0f, 0.0f, kHalfPi), cfg);
    EXPECT_NEAR(turned.position.x, 0.0f, 1e-6f);
    EXPECT_FLOAT_EQ(turned.position.z, 1.0f);
    EXPECT_FLOAT_EQ(turned.yaw, kHalfPi);
    EXPECT_FLOAT_EQ(turned.velocity.z, 1.0f); // per-tick displacement == velocity
}

// --- Facing + stance flags come from the input; tick is stamped ---------------
TEST(PredictionStepStep5, FacingAndFlags)
{
    const prediction::PredictionConfiguration cfg{};
    prediction::PredictedState prior{};
    const auto next = prediction::Integrate(prior, MoveInput(1, 42, 0.0f, 0.0f, 0.75f, /*actions*/ 0xABCDu), cfg);
    EXPECT_FLOAT_EQ(next.yaw, 0.75f);
    EXPECT_EQ(next.stateFlags, 0xABCDu);
    EXPECT_EQ(next.tick, 42u);
}

// --- Deterministic: identical inputs => identical output ----------------------
TEST(PredictionStepStep5, Deterministic)
{
    const prediction::PredictionConfiguration cfg{};
    prediction::PredictedState prior{};
    prior.position = world::Vec3{2.0f, 0.0f, 1.0f};
    const auto in = MoveInput(1, 5, 0.5f, -0.25f, 0.3f, 0x1u);

    const auto a = prediction::Integrate(prior, in, cfg);
    const auto b = prediction::Integrate(prior, in, cfg);
    EXPECT_FLOAT_EQ(a.position.x, b.position.x);
    EXPECT_FLOAT_EQ(a.position.y, b.position.y);
    EXPECT_FLOAT_EQ(a.position.z, b.position.z);
    EXPECT_FLOAT_EQ(a.velocity.x, b.velocity.x);
    EXPECT_FLOAT_EQ(a.yaw, b.yaw);
    EXPECT_EQ(a.tick, b.tick);
    EXPECT_EQ(a.stateFlags, b.stateFlags);
}

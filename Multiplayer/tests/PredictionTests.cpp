// STALKER-MP — Client prediction & interpolation tests (Sprint-010)
//
// Step 1: value types, enumerations, POD structures, and const char* name
//         helpers. Engine-free and OS-free. Types only — no configuration,
//         buffers, prediction/interpolation logic, seams, driver, or diagnostics.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <type_traits>
#include <utility>
#include <vector>

#include "stalkermp/core/Config.h"
#include "stalkermp/prediction/InputBuffer.h"
#include "stalkermp/prediction/PredictionConfiguration.h"
#include "stalkermp/prediction/PredictionManager.h"
#include "stalkermp/prediction/PredictionStep.h"
#include "stalkermp/prediction/InterpolationStep.h"
#include "stalkermp/prediction/InterpolationManager.h"
#include "stalkermp/prediction/SnapshotBuffer.h"
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

// ============================================================================
// Step 6 — PredictionManager
// ============================================================================

namespace
{
    prediction::PredictionConfiguration ManagerConfig(std::uint32_t maxPredictionTicks)
    {
        prediction::PredictionConfiguration c{};
        c.inputBufferDepth = 64;
        c.stateBufferDepth = 64;
        c.maxPredictionTicks = maxPredictionTicks;
        return c;
    }
} // namespace

// --- Record inputs, then predict forward from the confirmed baseline ----------
TEST(PredictionManagerStep6, RecordAndPredict)
{
    prediction::PredictionManager mgr{ManagerConfig(8)};

    // Three forward inputs, each moving local +X by 1 at yaw 0 (world +X).
    EXPECT_EQ(mgr.RecordInput(MoveInput(1, 1, 1.0f, 0.0f, 0.0f)), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(mgr.RecordInput(MoveInput(2, 2, 1.0f, 0.0f, 0.0f)), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(mgr.RecordInput(MoveInput(3, 3, 1.0f, 0.0f, 0.0f)), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(mgr.Statistics().inputsRecorded, 3u);

    const auto& current = mgr.PredictLocal(3);
    EXPECT_FLOAT_EQ(current.position.x, 3.0f); // three ticks of +X
    EXPECT_EQ(current.tick, 3u);
    EXPECT_EQ(mgr.Statistics().predictionsRun, 1u);
    EXPECT_EQ(mgr.RecordedStateCount(), 3u);

    // Current() mirrors the last prediction.
    EXPECT_FLOAT_EQ(mgr.Current().position.x, 3.0f);
}

// --- toTick bounds how far forward prediction runs ----------------------------
TEST(PredictionManagerStep6, PredictsOnlyUpToRequestedTick)
{
    prediction::PredictionManager mgr{ManagerConfig(8)};
    EXPECT_EQ(mgr.RecordInput(MoveInput(1, 1, 1.0f, 0.0f, 0.0f)), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(mgr.RecordInput(MoveInput(2, 2, 1.0f, 0.0f, 0.0f)), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(mgr.RecordInput(MoveInput(3, 3, 1.0f, 0.0f, 0.0f)), prediction::PredictionOutcome::Ok);

    const auto& current = mgr.PredictLocal(2); // stop at tick 2
    EXPECT_FLOAT_EQ(current.position.x, 2.0f);
    EXPECT_EQ(current.tick, 2u);
}

// --- maxPredictionTicks caps the number of integrations -----------------------
TEST(PredictionManagerStep6, CapsAtMaxPredictionTicks)
{
    prediction::PredictionManager mgr{ManagerConfig(2)}; // cap = 2

    for (std::uint64_t i = 1; i <= 5; ++i)
    {
        EXPECT_EQ(mgr.RecordInput(MoveInput(i, i, 1.0f, 0.0f, 0.0f)), prediction::PredictionOutcome::Ok);
    }

    const auto& current = mgr.PredictLocal(100); // request far ahead
    EXPECT_FLOAT_EQ(current.position.x, 2.0f);   // only 2 inputs applied (capped)
    EXPECT_EQ(current.tick, 2u);
}

// --- Deterministic: identical inputs => identical current state; idempotent ----
TEST(PredictionManagerStep6, DeterministicAndIdempotent)
{
    prediction::PredictionManager a{ManagerConfig(8)};
    prediction::PredictionManager b{ManagerConfig(8)};

    for (std::uint64_t i = 1; i <= 4; ++i)
    {
        const auto in = MoveInput(i, i, 0.5f, 0.25f, 0.1f * static_cast<float>(i), static_cast<std::uint32_t>(i));
        EXPECT_EQ(a.RecordInput(in), prediction::PredictionOutcome::Ok);
        EXPECT_EQ(b.RecordInput(in), prediction::PredictionOutcome::Ok);
    }

    const auto sa = a.PredictLocal(4);
    const auto sb = b.PredictLocal(4);
    EXPECT_FLOAT_EQ(sa.position.x, sb.position.x);
    EXPECT_FLOAT_EQ(sa.position.z, sb.position.z);
    EXPECT_FLOAT_EQ(sa.yaw, sb.yaw);
    EXPECT_EQ(sa.tick, sb.tick);

    // Re-running the same prediction is idempotent (replays from the baseline).
    const auto again = a.PredictLocal(4);
    EXPECT_FLOAT_EQ(again.position.x, sa.position.x);
    EXPECT_FLOAT_EQ(again.position.z, sa.position.z);
}

// --- Out-of-order input is rejected without disturbing prediction -------------
TEST(PredictionManagerStep6, RejectsSequenceRegression)
{
    prediction::PredictionManager mgr{ManagerConfig(8)};
    EXPECT_EQ(mgr.RecordInput(MoveInput(5, 5, 1.0f, 0.0f, 0.0f)), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(mgr.RecordInput(MoveInput(3, 3, 1.0f, 0.0f, 0.0f)), prediction::PredictionOutcome::SequenceMismatch);
    EXPECT_EQ(mgr.Statistics().inputsRecorded, 1u);
}

// ============================================================================
// Step 7 — SnapshotBuffer
// ============================================================================

namespace
{
    prediction::SnapshotFrame Frame(std::uint64_t tick, std::uint64_t acked = 0)
    {
        prediction::SnapshotFrame f{};
        f.tick = tick;
        f.ackedInputSequence = acked;
        return f;
    }
} // namespace

// --- Ascending push accepted; regress/duplicate rejected ----------------------
TEST(SnapshotBufferStep7, PushAscendingAndRejectRegression)
{
    prediction::SnapshotBuffer buf{8, /*delay*/ 0};
    EXPECT_EQ(buf.Push(Frame(10)), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(buf.Push(Frame(20)), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(buf.Push(Frame(20)), prediction::PredictionOutcome::SequenceMismatch); // duplicate
    EXPECT_EQ(buf.Push(Frame(15)), prediction::PredictionOutcome::SequenceMismatch); // regress
    EXPECT_EQ(buf.Size(), 2u);
}

// --- Pair brackets the (delayed) render tick and derives factor in [0,1] -------
TEST(SnapshotBufferStep7, PairFactorMidpoint)
{
    prediction::SnapshotBuffer buf{8, /*delay*/ 0};
    EXPECT_EQ(buf.Push(Frame(10)), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(buf.Push(Frame(20)), prediction::PredictionOutcome::Ok);

    const auto pair = buf.Pair(15); // exactly halfway between 10 and 20
    ASSERT_TRUE(pair.valid);
    ASSERT_NE(pair.from, nullptr);
    ASSERT_NE(pair.to, nullptr);
    EXPECT_EQ(pair.from->tick, 10u);
    EXPECT_EQ(pair.to->tick, 20u);
    EXPECT_FLOAT_EQ(pair.factor, 0.5f);

    // Endpoint: target equals a frame tick -> factor 0 on that bracket.
    const auto atFrom = buf.Pair(10);
    EXPECT_EQ(atFrom.from->tick, 10u);
    EXPECT_FLOAT_EQ(atFrom.factor, 0.0f);
}

// --- Interpolation delay shifts the target tick backward ----------------------
TEST(SnapshotBufferStep7, DelayShiftsTarget)
{
    prediction::SnapshotBuffer buf{8, /*delay*/ 4};
    EXPECT_EQ(buf.Push(Frame(10)), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(buf.Push(Frame(20)), prediction::PredictionOutcome::Ok);

    // renderTick 24 - delay 4 = target 20 -> newest boundary.
    const auto pair = buf.Pair(24);
    ASSERT_TRUE(pair.valid);
    EXPECT_EQ(pair.from->tick, 20u);
    EXPECT_EQ(pair.to->tick, 20u);

    // renderTick 19 - delay 4 = target 15 -> midpoint of [10,20].
    const auto mid = buf.Pair(19);
    EXPECT_EQ(mid.from->tick, 10u);
    EXPECT_EQ(mid.to->tick, 20u);
    EXPECT_FLOAT_EQ(mid.factor, 0.5f);
}

// --- No extrapolation: clamp before oldest and beyond newest ------------------
TEST(SnapshotBufferStep7, ClampsWithoutExtrapolation)
{
    prediction::SnapshotBuffer buf{8, /*delay*/ 0};
    EXPECT_EQ(buf.Push(Frame(10)), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(buf.Push(Frame(20)), prediction::PredictionOutcome::Ok);

    // Beyond newest -> clamp to newest (no extrapolation).
    const auto ahead = buf.Pair(999);
    ASSERT_TRUE(ahead.valid);
    EXPECT_EQ(ahead.from->tick, 20u);
    EXPECT_EQ(ahead.to->tick, 20u);
    EXPECT_FLOAT_EQ(ahead.factor, 0.0f);

    // Before oldest -> clamp to oldest.
    const auto behind = buf.Pair(3);
    EXPECT_EQ(behind.from->tick, 10u);
    EXPECT_EQ(behind.to->tick, 10u);
    EXPECT_FLOAT_EQ(behind.factor, 0.0f);

    // Empty buffer -> invalid.
    prediction::SnapshotBuffer empty{4, 0};
    const auto none = empty.Pair(10);
    EXPECT_FALSE(none.valid);
    EXPECT_EQ(none.from, nullptr);
}

// --- Bounded ring: oldest evicted on overflow (deterministic) -----------------
TEST(SnapshotBufferStep7, BoundedEviction)
{
    prediction::SnapshotBuffer buf{3, /*delay*/ 0};
    for (std::uint64_t t = 1; t <= 5; ++t)
    {
        EXPECT_EQ(buf.Push(Frame(t * 10)), prediction::PredictionOutcome::Ok);
    }
    EXPECT_EQ(buf.Size(), 3u);         // capacity bound respected
    EXPECT_TRUE(buf.Full());

    // Frames 10, 20 evicted; 30, 40, 50 remain. Target before oldest clamps to 30.
    const auto pair = buf.Pair(30);
    ASSERT_TRUE(pair.valid);
    EXPECT_EQ(pair.from->tick, 30u);

    // Determinism: a second identical Pair yields the same bracket + factor.
    const auto mid1 = buf.Pair(45);
    const auto mid2 = buf.Pair(45);
    EXPECT_EQ(mid1.from->tick, mid2.from->tick);
    EXPECT_EQ(mid1.to->tick, mid2.to->tick);
    EXPECT_FLOAT_EQ(mid1.factor, mid2.factor);
    EXPECT_FLOAT_EQ(mid1.factor, 0.5f); // 45 between 40 and 50
}

// ============================================================================
// Step 8 — Deterministic interpolation step
// ============================================================================

namespace
{
    replication::EntityReplicationState Ent(std::uint32_t id, float x, float y, float z)
    {
        replication::EntityReplicationState e{};
        e.id = world::EntityId{id};
        e.position = world::Vec3{x, y, z};
        return e;
    }
    replication::PlayerReplicationState Ply(std::uint32_t entityId, float x, float y, float z)
    {
        replication::PlayerReplicationState p{};
        p.entity = world::EntityId{entityId};
        p.position = world::Vec3{x, y, z};
        return p;
    }
    constexpr float kPi = 3.14159265358979323846f;
} // namespace

// --- Position lerp: endpoints exact, midpoint halfway --------------------------
TEST(InterpolationStepStep8, EntityPositionEndpointsAndMidpoint)
{
    const auto a = Ent(9, 0.0f, 0.0f, 0.0f);
    const auto b = Ent(9, 10.0f, -4.0f, 2.0f);

    const auto at0 = prediction::Interpolate(a, b, 0.0f);
    EXPECT_EQ(at0.id.value, 9u);
    EXPECT_FLOAT_EQ(at0.position.x, 0.0f);
    EXPECT_FLOAT_EQ(at0.position.z, 0.0f);

    const auto at1 = prediction::Interpolate(a, b, 1.0f);
    EXPECT_FLOAT_EQ(at1.position.x, 10.0f);
    EXPECT_FLOAT_EQ(at1.position.y, -4.0f);
    EXPECT_FLOAT_EQ(at1.position.z, 2.0f);

    const auto mid = prediction::Interpolate(a, b, 0.5f);
    EXPECT_FLOAT_EQ(mid.position.x, 5.0f);
    EXPECT_FLOAT_EQ(mid.position.y, -2.0f);
    EXPECT_FLOAT_EQ(mid.position.z, 1.0f);
}

// --- Factor clamps: no extrapolation below 0 or above 1 -----------------------
TEST(InterpolationStepStep8, FactorClampsNoExtrapolation)
{
    const auto a = Ent(1, 0.0f, 0.0f, 0.0f);
    const auto b = Ent(1, 8.0f, 0.0f, 0.0f);

    const auto below = prediction::Interpolate(a, b, -2.0f); // clamp to from
    EXPECT_FLOAT_EQ(below.position.x, 0.0f);

    const auto above = prediction::Interpolate(a, b, 5.0f); // clamp to to
    EXPECT_FLOAT_EQ(above.position.x, 8.0f);

    EXPECT_FLOAT_EQ(prediction::ClampFactor(-0.5f), 0.0f);
    EXPECT_FLOAT_EQ(prediction::ClampFactor(1.5f), 1.0f);
    EXPECT_FLOAT_EQ(prediction::ClampFactor(0.25f), 0.25f);
}

// --- Player overload lerps position and carries the entity id -----------------
TEST(InterpolationStepStep8, PlayerOverload)
{
    const auto a = Ply(42, 0.0f, 0.0f, 0.0f);
    const auto b = Ply(42, 4.0f, 4.0f, 4.0f);
    const auto mid = prediction::Interpolate(a, b, 0.5f);
    EXPECT_EQ(mid.id.value, 42u);
    EXPECT_FLOAT_EQ(mid.position.x, 2.0f);
    EXPECT_FLOAT_EQ(mid.position.y, 2.0f);
    EXPECT_FLOAT_EQ(mid.position.z, 2.0f);
}

// --- Yaw angular interpolation: shortest arc across the +/-pi seam ------------
TEST(InterpolationStepStep8, YawShortestArc)
{
    // Endpoints exact.
    EXPECT_FLOAT_EQ(prediction::InterpolateYaw(0.5f, 1.5f, 0.0f), 0.5f);
    EXPECT_FLOAT_EQ(prediction::InterpolateYaw(0.5f, 1.5f, 1.0f), 1.5f);

    // Simple midpoint (no wrap).
    EXPECT_FLOAT_EQ(prediction::InterpolateYaw(0.0f, 1.0f, 0.5f), 0.5f);

    // Across the seam: from just below +pi to just above -pi should move the SHORT
    // way (through +pi), not the long way back through 0.
    const float from = kPi - 0.1f;
    const float to = -kPi + 0.1f;   // effectively +0.2 rad the short way
    const float halfway = prediction::InterpolateYaw(from, to, 0.5f);
    // Short arc midpoint is just past +pi (which equals -pi); expect ~ +pi.
    EXPECT_NEAR(std::fabs(halfway), kPi, 0.05f);

    // Clamp: factor beyond [0,1] does not extrapolate.
    EXPECT_FLOAT_EQ(prediction::InterpolateYaw(0.0f, 1.0f, 2.0f), 1.0f);
    EXPECT_FLOAT_EQ(prediction::InterpolateYaw(0.0f, 1.0f, -1.0f), 0.0f);
}

// --- Deterministic: identical inputs => identical output ----------------------
TEST(InterpolationStepStep8, Deterministic)
{
    const auto a = Ent(3, 1.0f, 2.0f, 3.0f);
    const auto b = Ent(3, 9.0f, -2.0f, 0.0f);
    const auto x = prediction::Interpolate(a, b, 0.375f);
    const auto y = prediction::Interpolate(a, b, 0.375f);
    EXPECT_FLOAT_EQ(x.position.x, y.position.x);
    EXPECT_FLOAT_EQ(x.position.y, y.position.y);
    EXPECT_FLOAT_EQ(x.position.z, y.position.z);
    EXPECT_EQ(x.id.value, y.id.value);
    EXPECT_FLOAT_EQ(prediction::InterpolateYaw(0.2f, 2.9f, 0.6f),
                    prediction::InterpolateYaw(0.2f, 2.9f, 0.6f));
}

// ============================================================================
// Step 9 — InterpolationManager
// ============================================================================

namespace
{
    // A frame at `tick` whose entities are the given (id,x) pairs, ascending by id.
    prediction::SnapshotFrame EntFrame(std::uint64_t tick,
                                       std::initializer_list<std::pair<std::uint32_t, float>> ents)
    {
        prediction::SnapshotFrame f{};
        f.tick = tick;
        for (const auto& [id, x] : ents)
        {
            replication::EntityReplicationState e{};
            e.id = world::EntityId{id};
            e.position = world::Vec3{x, 0.0f, 0.0f};
            f.entities.push_back(e);
        }
        return f;
    }

    prediction::PredictionConfiguration InterpConfig(std::uint32_t delay)
    {
        prediction::PredictionConfiguration c{};
        c.stateBufferDepth = 16;
        c.interpolationDelayTicks = delay;
        return c;
    }
} // namespace

// --- Smoothing: entities in both frames are lerped at the bracket factor -------
TEST(InterpolationManagerStep9, SmoothsMatchedEntities)
{
    prediction::InterpolationManager mgr{InterpConfig(0)};
    EXPECT_EQ(mgr.PushFrame(EntFrame(10, {{1, 0.0f}, {2, 100.0f}})), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(mgr.PushFrame(EntFrame(20, {{1, 10.0f}, {2, 200.0f}})), prediction::PredictionOutcome::Ok);

    std::vector<prediction::InterpolatedState> out;
    EXPECT_EQ(mgr.Interpolate(15, out), prediction::PredictionOutcome::Ok); // halfway
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].id.value, 1u);
    EXPECT_FLOAT_EQ(out[0].position.x, 5.0f);
    EXPECT_EQ(out[1].id.value, 2u);
    EXPECT_FLOAT_EQ(out[1].position.x, 150.0f);
}

// --- Ordering + uniqueness; unmatched entities are skipped (no extrapolation) --
TEST(InterpolationManagerStep9, AscendingUniqueAndMatchedOnly)
{
    prediction::InterpolationManager mgr{InterpConfig(0)};
    // Entity 2 present in both; 1 only in `from`; 3 only in `to`.
    EXPECT_EQ(mgr.PushFrame(EntFrame(10, {{1, 1.0f}, {2, 20.0f}})), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(mgr.PushFrame(EntFrame(20, {{2, 40.0f}, {3, 3.0f}})), prediction::PredictionOutcome::Ok);

    std::vector<prediction::InterpolatedState> out;
    EXPECT_EQ(mgr.Interpolate(15, out), prediction::PredictionOutcome::Ok);
    ASSERT_EQ(out.size(), 1u);         // only entity 2 is in both frames
    EXPECT_EQ(out[0].id.value, 2u);
    EXPECT_FLOAT_EQ(out[0].position.x, 30.0f);
}

// --- Append-only: existing contents are preserved -----------------------------
TEST(InterpolationManagerStep9, AppendOnlyOutput)
{
    prediction::InterpolationManager mgr{InterpConfig(0)};
    EXPECT_EQ(mgr.PushFrame(EntFrame(10, {{5, 0.0f}})), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(mgr.PushFrame(EntFrame(20, {{5, 8.0f}})), prediction::PredictionOutcome::Ok);

    std::vector<prediction::InterpolatedState> out;
    prediction::InterpolatedState sentinel{};
    sentinel.id = world::EntityId{999};
    out.push_back(sentinel);

    EXPECT_EQ(mgr.Interpolate(15, out), prediction::PredictionOutcome::Ok);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].id.value, 999u); // preserved
    EXPECT_EQ(out[1].id.value, 5u);
    EXPECT_FLOAT_EQ(out[1].position.x, 4.0f);
}

// --- No extrapolation: clamp to boundary; empty buffer appends nothing ---------
TEST(InterpolationManagerStep9, NoExtrapolationAndEmpty)
{
    prediction::InterpolationManager mgr{InterpConfig(0)};

    // Empty buffer -> NoAuthoritativeFrame, nothing appended.
    std::vector<prediction::InterpolatedState> none;
    EXPECT_EQ(mgr.Interpolate(10, none), prediction::PredictionOutcome::NoAuthoritativeFrame);
    EXPECT_TRUE(none.empty());

    EXPECT_EQ(mgr.PushFrame(EntFrame(10, {{7, 1.0f}})), prediction::PredictionOutcome::Ok);
    EXPECT_EQ(mgr.PushFrame(EntFrame(20, {{7, 5.0f}})), prediction::PredictionOutcome::Ok);

    // Beyond newest -> clamp to newest frame (factor 0 -> own position, no overshoot).
    std::vector<prediction::InterpolatedState> ahead;
    EXPECT_EQ(mgr.Interpolate(999, ahead), prediction::PredictionOutcome::Ok);
    ASSERT_EQ(ahead.size(), 1u);
    EXPECT_FLOAT_EQ(ahead[0].position.x, 5.0f); // newest, not extrapolated past
}

// --- Deterministic: identical calls produce identical output ------------------
TEST(InterpolationManagerStep9, Deterministic)
{
    prediction::InterpolationManager a{InterpConfig(2)};
    prediction::InterpolationManager b{InterpConfig(2)};
    for (auto* m : {&a, &b})
    {
        EXPECT_EQ(m->PushFrame(EntFrame(10, {{1, 0.0f}, {2, 0.0f}})), prediction::PredictionOutcome::Ok);
        EXPECT_EQ(m->PushFrame(EntFrame(20, {{1, 10.0f}, {2, 20.0f}})), prediction::PredictionOutcome::Ok);
    }

    std::vector<prediction::InterpolatedState> oa;
    std::vector<prediction::InterpolatedState> ob;
    EXPECT_EQ(a.Interpolate(19, oa), prediction::PredictionOutcome::Ok); // 19 - delay 2 = 17
    EXPECT_EQ(b.Interpolate(19, ob), prediction::PredictionOutcome::Ok);
    ASSERT_EQ(oa.size(), ob.size());
    for (std::size_t k = 0; k < oa.size(); ++k)
    {
        EXPECT_EQ(oa[k].id.value, ob[k].id.value);
        EXPECT_FLOAT_EQ(oa[k].position.x, ob[k].position.x);
    }
}

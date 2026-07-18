// STALKER-MP — Client prediction manager (Sprint-010, Steps 6 + 11)
//
// See PredictionManager.h. Deterministic, client-only forward prediction from the
// last confirmed state, plus host-wins reconciliation (Step 11). Engine-free /
// OS-free; no exceptions, no RTTI, no iostream; value outcomes (PredictionOutcome).

#include "stalkermp/prediction/PredictionManager.h"

#include <cmath>
#include <vector>

#include "stalkermp/prediction/PredictionStep.h" // Integrate

namespace stalkermp::prediction
{
    namespace
    {
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kTwoPi = 6.28318530717958647692f;
        constexpr float kUnitsToMilli = 1000.0f; // 1 world unit (metre) -> millimetres

        // Euclidean distance between two positions, in millimetres (fixed-point cmp).
        [[nodiscard]] float DistanceMm(const world::Vec3& a, const world::Vec3& b) noexcept
        {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            const float dz = a.z - b.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz) * kUnitsToMilli;
        }

        // Absolute shortest-arc yaw difference, in milliradians.
        [[nodiscard]] float YawDeltaMrad(const float a, const float b) noexcept
        {
            float delta = a - b;
            while (delta > kPi)
            {
                delta -= kTwoPi;
            }
            while (delta < -kPi)
            {
                delta += kTwoPi;
            }
            return std::fabs(delta) * kUnitsToMilli;
        }
    } // namespace

    PredictionOutcome PredictionManager::RecordInput(const InputCommand& input) noexcept
    {
        const PredictionOutcome outcome = m_inputs.Push(input);
        if (outcome == PredictionOutcome::Ok)
        {
            ++m_stats.inputsRecorded;
        }
        else if (outcome == PredictionOutcome::BufferOverflow)
        {
            ++m_stats.overflows;
        }
        return outcome;
    }

    PredictedState PredictionManager::ReplayFrom(const PredictedState& baseline, const std::uint64_t fromSequence,
                                                 const std::uint64_t toTick)
    {
        PredictedState state = baseline;
        const std::vector<InputCommand> pending = m_inputs.Pending(fromSequence);

        std::uint32_t applied = 0;
        for (const InputCommand& input : pending)
        {
            if (input.tick > toTick)
            {
                break; // do not predict beyond the requested tick
            }
            if (applied >= m_config.maxPredictionTicks)
            {
                break; // capped: bound prediction ahead of the confirmed state
            }
            state = Integrate(state, input, m_config);
            m_states.Record(state); // ascending tick; deterministic
            ++applied;
        }
        return state;
    }

    const PredictedState& PredictionManager::PredictLocal(const std::uint64_t toTick)
    {
        ++m_stats.predictionsRun;
        m_current = ReplayFrom(m_confirmed, m_confirmedSequence, toTick);
        return m_current;
    }

    PredictionOutcome PredictionManager::Reconcile(const replication::PlayerReplicationState& authoritative,
                                                   const std::uint64_t ackedInputSequence, const std::uint64_t toTick)
    {
        // A regressed acknowledgement is rejected without mutating state.
        if (m_confirmedSequence > 0 && ackedInputSequence < m_confirmedSequence)
        {
            return PredictionOutcome::SequenceMismatch;
        }

        // Once a local entity is established, refuse to reconcile against a different
        // entity (host authority still wins, but not for the wrong body).
        if (m_confirmed.id.value != 0 && authoritative.entity.value != 0 &&
            authoritative.entity.value != m_confirmed.id.value)
        {
            return PredictionOutcome::CorrectionRejected;
        }

        const PredictedState previous = m_current; // where the client currently shows the local player

        // Snap the confirmed baseline to the authoritative state (host authority wins).
        // The host sends position + identity only; velocity and facing/stance are
        // client-visual (no authoritative source), so they are carried from the prior
        // prediction for continuity — the same treatment applied to yaw.
        PredictedState baseline{};
        baseline.id = authoritative.entity.value != 0 ? authoritative.entity : m_confirmed.id;
        baseline.tick = m_confirmed.tick;
        baseline.position = authoritative.position;
        baseline.velocity = previous.velocity; // carried for visual continuity (host sends none)
        baseline.yaw = previous.yaw;            // client-visual facing preserved
        baseline.stateFlags = previous.stateFlags;

        // Prune acknowledged inputs and advance the confirmed point.
        m_inputs.PruneUpTo(ackedInputSequence);
        m_confirmed = baseline;
        m_confirmedSequence = ackedInputSequence;

        // Replay the remaining pending inputs deterministically from the authoritative
        // baseline; this is the corrected current prediction (host authority preserved).
        m_current = ReplayFrom(m_confirmed, m_confirmedSequence, toTick);

        // Classify the applied correction against the configured thresholds. The
        // magnitude is how far the correction moved the rendered local player.
        const float posErrMm = DistanceMm(m_current.position, previous.position);
        const float yawErrMrad = YawDeltaMrad(m_current.yaw, previous.yaw);
        const float velErrMm = DistanceMm(m_current.velocity, previous.velocity);

        const bool beyond = posErrMm > static_cast<float>(m_config.positionCorrectionThresholdMm) ||
                            yawErrMrad > static_cast<float>(m_config.rotationCorrectionThresholdMrad) ||
                            velErrMm > static_cast<float>(m_config.velocityCorrectionThresholdMm);

        if (beyond)
        {
            m_lastCorrection = CorrectionKind::Snapped;
            ++m_stats.snaps;
        }
        else if (posErrMm > 0.0f || yawErrMrad > 0.0f || velErrMm > 0.0f)
        {
            m_lastCorrection = CorrectionKind::Smoothed;
        }
        else
        {
            m_lastCorrection = CorrectionKind::None;
        }

        if (m_lastCorrection != CorrectionKind::None)
        {
            ++m_stats.corrections;
        }

        const std::uint32_t magnitudeMm = static_cast<std::uint32_t>(posErrMm);
        m_stats.lastCorrectionMagnitude = magnitudeMm;
        if (magnitudeMm > m_stats.maxCorrectionMagnitude)
        {
            m_stats.maxCorrectionMagnitude = magnitudeMm;
        }

        return PredictionOutcome::Ok;
    }
} // namespace stalkermp::prediction

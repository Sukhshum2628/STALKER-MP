// STALKER-MP — Client presentation driver (Sprint-010, Step 14)
//
// See ClientPresentationDriver.h. One synchronous, deterministic pass per Advance;
// no thread, no FrameDispatcher subscription. Engine-free / OS-free; no exceptions,
// no RTTI, no iostream; value outcomes.

#include "stalkermp/prediction/ClientPresentationDriver.h"

namespace stalkermp::prediction
{
    const replication::PlayerReplicationState*
    ClientPresentationDriver::SelectLocalPlayer(const SnapshotFrame& frame) const
    {
        if (frame.players.empty())
        {
            return nullptr;
        }

        const std::uint32_t establishedId = m_prediction.Current().id.value;
        if (establishedId != 0)
        {
            for (const replication::PlayerReplicationState& p : frame.players)
            {
                if (p.entity.value == establishedId)
                {
                    return &p;
                }
            }
            return nullptr; // local player not present in this frame
        }

        // Not yet established: bootstrap from the first player (ascending PlayerId).
        return &frame.players.front();
    }

    PredictionOutcome ClientPresentationDriver::Advance(const std::uint64_t clientTick, const double dt)
    {
        (void)dt; // real-time delta is not part of the deterministic, tick-driven path

        // ---- Identity mode (host): render authoritative frames directly ----------
        if (m_identity)
        {
            SnapshotFrame frame{};
            while (m_authoritative.NextFrame(frame))
            {
                for (const replication::EntityReplicationState& e : frame.entities)
                {
                    InterpolatedState out{};
                    out.id = e.id;
                    out.position = e.position;
                    out.yaw = 0.0f; // authoritative carries no orientation (Preserve Before Replace)
                    m_sink.ApplyRemote(out);
                }
            }
            return PredictionOutcome::Ok;
        }

        // ---- Client mode: predict + interpolate ----------------------------------

        // 1) Poll + record the local input for this tick.
        InputCommand input{};
        if (m_input.PollInput(input))
        {
            if (m_prediction.RecordInput(input) == PredictionOutcome::BufferOverflow)
            {
                m_diagnostics.RecordOverflow();
            }
        }

        // 2) Drain authoritative frames: buffer for interpolation, reconcile locally.
        SnapshotFrame frame{};
        while (m_authoritative.NextFrame(frame))
        {
            (void)m_interpolation.PushFrame(frame); // ascending-tick buffer (bounded)

            if (const replication::PlayerReplicationState* local = SelectLocalPlayer(frame))
            {
                const PredictionOutcome r =
                    m_prediction.Reconcile(*local, frame.ackedInputSequence, clientTick);
                if (r == PredictionOutcome::Ok && m_prediction.LastCorrection() != CorrectionKind::None)
                {
                    m_diagnostics.RecordCorrection(m_prediction.Statistics().lastCorrectionMagnitude);
                }
                // SequenceMismatch / CorrectionRejected are tolerated value outcomes.
            }
        }

        // 3) Predict the local player forward and present it.
        const PredictedState& localState = m_prediction.PredictLocal(clientTick);
        m_diagnostics.RecordPrediction();
        m_sink.ApplyLocal(localState);

        // 4) Interpolate remote entities and present them (append-only, ascending).
        std::vector<InterpolatedState> remote;
        (void)m_interpolation.Interpolate(clientTick, remote);
        for (const InterpolatedState& r : remote)
        {
            m_sink.ApplyRemote(r);
        }
        m_diagnostics.RecordInterpolation(remote.size());

        return PredictionOutcome::Ok;
    }
} // namespace stalkermp::prediction

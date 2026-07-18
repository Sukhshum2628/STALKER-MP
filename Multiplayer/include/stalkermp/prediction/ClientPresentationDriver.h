// STALKER-MP — Client presentation driver (Sprint-010, Step 14)
//
// The synchronous, single-threaded per-frame orchestration of client prediction &
// interpolation. It owns the prediction + interpolation managers and the
// diagnostics collector, and references the three engine-free seams (local input,
// authoritative state, presentation sink). One `Advance` call is exactly one
// deterministic pass — there is NO thread and NO FrameDispatcher subscription; the
// composition root (Step 16) calls Advance once per client frame, after dispatch.
//
// Client pass: poll local input -> RecordInput -> drain authoritative frames
// (PushFrame for interpolation + Reconcile the local player, host authority wins)
// -> PredictLocal -> Interpolate remote entities -> apply everything through the
// presentation sink (client-visual transforms only; ADR-008). On the host,
// SetIdentityMode(true) renders the authoritative frames directly with no
// prediction/interpolation. The real-time `dt` never enters control flow — the
// deterministic path is tick-driven (Replay Determinism).
//
// This step introduces the driver ONLY — no engine binding (Step 15) and no
// composition-root wiring (Step 16).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (PredictionOutcome).

#pragma once

#include <cstdint>
#include <vector>

#include "stalkermp/prediction/IAuthoritativeStateSource.h"
#include "stalkermp/prediction/ILocalInputSource.h"
#include "stalkermp/prediction/IPresentationSink.h"
#include "stalkermp/prediction/InterpolationManager.h"
#include "stalkermp/prediction/PredictionConfiguration.h"
#include "stalkermp/prediction/PredictionDiagnostics.h"
#include "stalkermp/prediction/PredictionManager.h"
#include "stalkermp/prediction/PredictionTypes.h"

namespace stalkermp::prediction
{
    class ClientPresentationDriver
    {
    public:
        // References to the seams are borrowed for the driver's lifetime (owned by
        // the composition root). The prediction/interpolation managers + diagnostics
        // are owned and sized from the configuration.
        ClientPresentationDriver(const PredictionConfiguration& config, const ILocalInputSource& input,
                                 const IAuthoritativeStateSource& authoritative, IPresentationSink& sink)
            : m_input(input), m_authoritative(authoritative), m_sink(sink), m_prediction(config),
              m_interpolation(config)
        {
        }

        // Run one synchronous presentation pass for `clientTick`. Client mode:
        // poll+record input, drain authoritative frames (buffer + reconcile), predict
        // the local player, interpolate remote entities, and apply all results to the
        // sink. Identity mode (host): render the drained authoritative frames directly.
        // `dt` is real-time frame delta and is intentionally NOT used in control flow.
        // Deterministic; all value outcomes handled. Returns Ok.
        [[nodiscard]] PredictionOutcome Advance(std::uint64_t clientTick, double dt);

        // Host runs in identity mode: authoritative frames are rendered directly with
        // no prediction/interpolation. Clients run in prediction mode (the default).
        void SetIdentityMode(bool identity) noexcept { m_identity = identity; }
        [[nodiscard]] bool IsIdentityMode() const noexcept { return m_identity; }

        // Read-only views.
        [[nodiscard]] const PredictionManager& Prediction() const noexcept { return m_prediction; }
        [[nodiscard]] const InterpolationManager& Interpolation() const noexcept { return m_interpolation; }
        [[nodiscard]] const PredictionDiagnostics& Diagnostics() const noexcept { return m_diagnostics; }

    private:
        // Select the authoritative player state for the local player in `frame`:
        // the player whose entity matches the established local id, or (before one is
        // established) the first player. Returns nullptr when the frame has no players.
        [[nodiscard]] const replication::PlayerReplicationState* SelectLocalPlayer(const SnapshotFrame& frame) const;

        const ILocalInputSource& m_input;
        const IAuthoritativeStateSource& m_authoritative;
        IPresentationSink& m_sink;

        PredictionManager m_prediction;
        InterpolationManager m_interpolation;
        PredictionDiagnostics m_diagnostics;

        bool m_identity = false; // host = identity (no prediction/interpolation)
    };
} // namespace stalkermp::prediction

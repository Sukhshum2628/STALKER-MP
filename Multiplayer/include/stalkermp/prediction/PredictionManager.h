// STALKER-MP — Client prediction manager (Sprint-010, Steps 6 + 11)
//
// Owns the input buffer, the predicted-state buffer, and the deterministic
// integrator, and drives local-player prediction forward from the last confirmed
// state. `RecordInput` buffers one client input; `PredictLocal(toTick)` replays
// the buffered inputs from the confirmed baseline up to `toTick` — capped by the
// configured maxPredictionTicks — recording each predicted state and returning the
// resulting current state. `Current()` and `Statistics()` are read-only views.
//
// Step 11 adds `Reconcile`: deterministic, host-wins error correction. It prunes
// acknowledged inputs, snaps the confirmed baseline to the authoritative state
// (host authority always wins), replays the remaining pending inputs, and
// classifies the resulting correction as None / Smoothed (small) / Snapped (large)
// against the configured thresholds. Reconciliation reads the authoritative state
// and NEVER mutates it (Host Authority; Preserve Before Replace; ADR-008).
//
// Prediction is CLIENT-ONLY presentation: it produces a visual overlay and never
// mutates authoritative simulation state.
//
// This step introduces prediction + reconciliation — no interpolation, seams,
// driver, diagnostics, or engine binding.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (PredictionOutcome).

#pragma once

#include <cstddef>
#include <cstdint>

#include "stalkermp/prediction/InputBuffer.h"
#include "stalkermp/prediction/PredictionConfiguration.h"
#include "stalkermp/prediction/PredictionTypes.h"
#include "stalkermp/prediction/StateBuffer.h"

namespace stalkermp::prediction
{
    class PredictionManager
    {
    public:
        // Buffers are sized from the configuration (both depths are >= 1 by config).
        explicit PredictionManager(const PredictionConfiguration& config)
            : m_config(config), m_inputs(config.inputBufferDepth), m_states(config.stateBufferDepth)
        {
        }

        // Buffer one local input for prediction + (later) replay. Strictly ascending
        // sequence required; a regress is SequenceMismatch and a full buffer is
        // BufferOverflow (both leave state unchanged). Deterministic; no allocation.
        [[nodiscard]] PredictionOutcome RecordInput(const InputCommand& input) noexcept;

        // Predict the local player forward: starting from the last confirmed state,
        // deterministically integrate the buffered inputs whose tick is <= `toTick`,
        // in ascending sequence order, applying at most maxPredictionTicks of them
        // (the cap bounds prediction cost/drift). Each integrated state is recorded
        // into the state buffer. Returns the resulting current predicted state.
        // Idempotent for a given input set (replays from the confirmed baseline).
        const PredictedState& PredictLocal(std::uint64_t toTick);

        // Reconcile the local prediction against an authoritative player state that
        // acknowledges inputs through `ackedInputSequence`, then re-predict to
        // `toTick`. Host authority always wins: acknowledged inputs are pruned, the
        // confirmed baseline is snapped to the authoritative position, and the
        // remaining pending inputs are deterministically replayed through Integrate.
        // The applied correction is classified against the configured thresholds
        // (None / Smoothed / Snapped) and reflected in Statistics()/LastCorrection().
        //   - `ackedInputSequence` older than the last confirmed => SequenceMismatch.
        //   - authoritative entity conflicting with the established local entity =>
        //     CorrectionRejected.
        // Both reject without mutating state. Never mutates the authoritative input.
        [[nodiscard]] PredictionOutcome Reconcile(const replication::PlayerReplicationState& authoritative,
                                                  std::uint64_t ackedInputSequence, std::uint64_t toTick);

        // The most recent predicted state (the confirmed baseline until first predict).
        [[nodiscard]] const PredictedState& Current() const noexcept { return m_current; }

        // The kind of correction the most recent Reconcile applied (None until one runs).
        [[nodiscard]] CorrectionKind LastCorrection() const noexcept { return m_lastCorrection; }

        // Read-only aggregate tallies (diagnostic groundwork).
        [[nodiscard]] const PredictionStatistics& Statistics() const noexcept { return m_stats; }

        // Read-only structural views (bounded buffers; no mutation surface).
        [[nodiscard]] std::size_t PendingInputCount() const noexcept { return m_inputs.Size(); }
        [[nodiscard]] std::size_t RecordedStateCount() const noexcept { return m_states.Size(); }
        [[nodiscard]] std::uint64_t ConfirmedSequence() const noexcept { return m_confirmedSequence; }

    private:
        // Deterministically integrate the buffered inputs with sequence > `fromSequence`
        // and tick <= `toTick`, starting from `baseline`, applying at most
        // maxPredictionTicks of them and recording each into the state buffer. Returns
        // the resulting state. Shared by PredictLocal and Reconcile.
        [[nodiscard]] PredictedState ReplayFrom(const PredictedState& baseline, std::uint64_t fromSequence,
                                                std::uint64_t toTick);

        PredictionConfiguration m_config;
        InputBuffer m_inputs;
        StateBuffer m_states;
        PredictedState m_confirmed{};        // baseline; fed by reconciliation at Step-11
        std::uint64_t m_confirmedSequence = 0; // last confirmed input sequence (0 = none)
        PredictedState m_current{};          // latest predicted state (read-only view)
        CorrectionKind m_lastCorrection = CorrectionKind::None; // last reconcile classification
        PredictionStatistics m_stats{};      // diagnostic tallies
    };
} // namespace stalkermp::prediction

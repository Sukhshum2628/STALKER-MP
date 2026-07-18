// STALKER-MP — Client prediction manager (Sprint-010, Step 6)
//
// Owns the input buffer, the predicted-state buffer, and the deterministic
// integrator, and drives local-player prediction forward from the last confirmed
// state. `RecordInput` buffers one client input; `PredictLocal(toTick)` replays
// the buffered inputs from the confirmed baseline up to `toTick` — capped by the
// configured maxPredictionTicks — recording each predicted state and returning the
// resulting current state. `Current()` and `Statistics()` are read-only views.
//
// Prediction is CLIENT-ONLY presentation: it produces a visual overlay and never
// mutates authoritative simulation state (Host Authority; ADR-008). The confirmed
// baseline is fed by reconciliation at Step-11; until then it is the zero state.
//
// This step introduces the manager ONLY — no reconciliation, interpolation,
// seams, driver, diagnostics, or engine binding.
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

        // The most recent predicted state (the confirmed baseline until first predict).
        [[nodiscard]] const PredictedState& Current() const noexcept { return m_current; }

        // Read-only aggregate tallies (diagnostic groundwork).
        [[nodiscard]] const PredictionStatistics& Statistics() const noexcept { return m_stats; }

        // Read-only structural views (bounded buffers; no mutation surface).
        [[nodiscard]] std::size_t PendingInputCount() const noexcept { return m_inputs.Size(); }
        [[nodiscard]] std::size_t RecordedStateCount() const noexcept { return m_states.Size(); }

    private:
        PredictionConfiguration m_config;
        InputBuffer m_inputs;
        StateBuffer m_states;
        PredictedState m_confirmed{};        // baseline; fed by reconciliation at Step-11
        std::uint64_t m_confirmedSequence = 0; // last confirmed input sequence (0 = none)
        PredictedState m_current{};          // latest predicted state (read-only view)
        PredictionStatistics m_stats{};      // diagnostic tallies
    };
} // namespace stalkermp::prediction

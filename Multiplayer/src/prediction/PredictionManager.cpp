// STALKER-MP — Client prediction manager (Sprint-010, Step 6)
//
// See PredictionManager.h. Deterministic, client-only forward prediction from the
// last confirmed state. Engine-free / OS-free; no exceptions, no RTTI, no
// iostream; value outcomes (PredictionOutcome).

#include "stalkermp/prediction/PredictionManager.h"

#include <vector>

#include "stalkermp/prediction/PredictionStep.h" // Integrate

namespace stalkermp::prediction
{
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

    const PredictedState& PredictionManager::PredictLocal(const std::uint64_t toTick)
    {
        ++m_stats.predictionsRun;

        PredictedState state = m_confirmed;
        const std::vector<InputCommand> pending = m_inputs.Pending(m_confirmedSequence);

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

        m_current = state;
        return m_current;
    }
} // namespace stalkermp::prediction

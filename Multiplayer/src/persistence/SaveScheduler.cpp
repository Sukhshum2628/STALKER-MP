// STALKER-MP — Save scheduler (Sprint-011, Step 10)
//
// See SaveScheduler.h. Deterministic, tick-driven; no wall-clock in control flow.
// Engine-free / OS-free; no exceptions, no RTTI, no iostream.

#include "stalkermp/persistence/SaveScheduler.h"

namespace stalkermp::persistence
{
    std::optional<SaveRequest> SaveScheduler::Tick(const std::uint64_t currentTick) noexcept
    {
        // Arm the autosave baseline on first observation so the cadence is measured
        // from when scheduling begins (no autosave is emitted on the arming tick).
        if (!m_autosaveArmed)
        {
            m_lastAutosaveTick = currentTick;
            m_autosaveArmed = true;
        }

        // Fixed priority order: Shutdown > Administrative > Manual > Deferred > Autosave.
        if (m_shutdownPending)
        {
            m_shutdownPending = false;
            return Emit(SaveTrigger::Shutdown, currentTick);
        }
        if (m_adminPending)
        {
            m_adminPending = false;
            return Emit(SaveTrigger::Administrative, currentTick);
        }
        if (m_manualPending)
        {
            m_manualPending = false;
            return Emit(SaveTrigger::Manual, currentTick);
        }
        if (m_deferredPending && currentTick >= m_deferredTick)
        {
            m_deferredPending = false;
            return Emit(SaveTrigger::Deferred, currentTick);
        }
        if (m_autosaveIntervalTicks > 0 && currentTick >= m_lastAutosaveTick &&
            (currentTick - m_lastAutosaveTick) >= m_autosaveIntervalTicks)
        {
            m_lastAutosaveTick = currentTick;
            return Emit(SaveTrigger::Autosave, currentTick);
        }
        return std::nullopt;
    }
} // namespace stalkermp::persistence

// STALKER-MP — Save scheduler (Sprint-011, Step 10)
//
// Deterministic, TICK-DRIVEN scheduling of save requests. Callers register manual,
// administrative, shutdown, and deferred saves; a periodic autosave is emitted
// every `autosaveIntervalTicks`. `Tick(currentTick)` returns at most one SaveRequest
// per tick, in a fixed priority order (Shutdown > Administrative > Manual >
// Deferred(due) > Autosave), stamping a monotonic save id. No wall-clock enters
// control flow — scheduling is a pure function of the tick sequence and the
// registered requests (Replay Determinism).
//
// This step introduces the scheduler ONLY — it owns no queue, worker, store, or
// manager; it merely decides WHEN a save should be requested.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>
#include <optional>

#include "stalkermp/persistence/PersistenceConfiguration.h" // PersistenceConfiguration
#include "stalkermp/persistence/PersistenceTypes.h"         // SaveRequest, SaveTrigger

namespace stalkermp::persistence
{
    class SaveScheduler
    {
    public:
        explicit SaveScheduler(const PersistenceConfiguration& config) noexcept
            : m_autosaveIntervalTicks(config.autosaveIntervalTicks)
        {
        }

        // Register one-shot saves (serviced on the next Tick, in priority order).
        void RequestManual() noexcept { m_manualPending = true; }
        void RequestAdministrative() noexcept { m_adminPending = true; }
        void RequestShutdown() noexcept { m_shutdownPending = true; }

        // Register a deferred save to fire on the first Tick at/after `untilTick`.
        // A later Defer overwrites the pending deferral.
        void Defer(std::uint64_t untilTick) noexcept
        {
            m_deferredPending = true;
            m_deferredTick = untilTick;
        }

        // Advance to `currentTick`; return at most one due SaveRequest (highest
        // priority), or nullopt when nothing is due. Deterministic.
        [[nodiscard]] std::optional<SaveRequest> Tick(std::uint64_t currentTick) noexcept;

        // Read-only state.
        [[nodiscard]] bool HasPending() const noexcept
        {
            return m_shutdownPending || m_adminPending || m_manualPending || m_deferredPending;
        }
        [[nodiscard]] std::uint64_t LastAutosaveTick() const noexcept { return m_lastAutosaveTick; }
        [[nodiscard]] std::uint64_t EmittedCount() const noexcept { return m_emitted; }

    private:
        [[nodiscard]] SaveRequest Emit(SaveTrigger trigger, std::uint64_t tick) noexcept
        {
            ++m_emitted;
            SaveRequest r{};
            r.id = ++m_nextId;
            r.trigger = trigger;
            r.requestTick = tick;
            return r;
        }

        std::uint32_t m_autosaveIntervalTicks = 0;

        bool m_manualPending = false;
        bool m_adminPending = false;
        bool m_shutdownPending = false;
        bool m_deferredPending = false;
        std::uint64_t m_deferredTick = 0;

        std::uint64_t m_lastAutosaveTick = 0; // tick of the last emitted autosave
        bool m_autosaveArmed = false;         // whether the autosave baseline is set
        std::uint64_t m_nextId = 0;           // monotonic save id
        std::uint64_t m_emitted = 0;          // total requests emitted
    };
} // namespace stalkermp::persistence

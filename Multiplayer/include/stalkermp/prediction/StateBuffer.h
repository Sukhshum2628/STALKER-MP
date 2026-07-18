// STALKER-MP — Predicted state buffer (Sprint-010, Step 4)
//
// A fixed-capacity ring of PredictedState keyed by (ascending) prediction tick,
// used by reconciliation (Step 11) to look up the state the client predicted for
// the tick an authoritative frame confirms. Records are appended in ascending
// tick order; when the ring is full the oldest is evicted (bounded memory,
// deterministic). Lookup by tick is O(n) over the small ring. Storage is
// pre-reserved once at construction; Record/Clear perform NO dynamic allocation
// in steady state.
//
// This step introduces the buffer ONLY — no prediction/reconciliation logic,
// manager, seams, driver, diagnostics, or engine binding.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stalkermp/prediction/PredictionTypes.h" // PredictedState

namespace stalkermp::prediction
{
    class StateBuffer
    {
    public:
        // Capacity is bounded by the configured stateBufferDepth (>= 1 by config).
        explicit StateBuffer(std::size_t capacity) : m_buffer(capacity == 0 ? 1 : capacity) {}

        // Record the predicted state for its tick. Ticks must be strictly ascending
        // (a non-ascending tick is ignored, leaving the buffer unchanged). When the
        // ring is full the oldest record is evicted (bounded, deterministic). No
        // allocation.
        void Record(const PredictedState& state) noexcept
        {
            if (m_count > 0 && state.tick <= m_lastTick)
            {
                return; // not ascending -> ignore (deterministic no-op)
            }
            if (m_count >= m_buffer.size())
            {
                m_head = (m_head + 1) % m_buffer.size(); // evict oldest
                --m_count;
            }
            const std::size_t tail = (m_head + m_count) % m_buffer.size();
            m_buffer[tail] = state;
            m_lastTick = state.tick;
            ++m_count;
        }

        // The recorded predicted state for `tick`, or nullptr if absent/evicted.
        [[nodiscard]] const PredictedState* At(const std::uint64_t tick) const noexcept
        {
            for (std::size_t i = 0; i < m_count; ++i)
            {
                const PredictedState& s = m_buffer[(m_head + i) % m_buffer.size()];
                if (s.tick == tick)
                {
                    return &s;
                }
            }
            return nullptr;
        }

        // The most recently recorded state, or nullptr when empty.
        [[nodiscard]] const PredictedState* Latest() const noexcept
        {
            if (m_count == 0)
            {
                return nullptr;
            }
            return &m_buffer[(m_head + m_count - 1) % m_buffer.size()];
        }

        void Clear() noexcept
        {
            m_head = 0;
            m_count = 0;
            m_lastTick = 0;
        }

        [[nodiscard]] bool Empty() const noexcept { return m_count == 0; }
        [[nodiscard]] bool Full() const noexcept { return m_count >= m_buffer.size(); }
        [[nodiscard]] std::size_t Size() const noexcept { return m_count; }
        [[nodiscard]] std::size_t Capacity() const noexcept { return m_buffer.size(); }

    private:
        std::vector<PredictedState> m_buffer; // fixed after construction (ring storage)
        std::size_t m_head = 0;
        std::size_t m_count = 0;
        std::uint64_t m_lastTick = 0; // last recorded tick (ascending guard)
    };
} // namespace stalkermp::prediction

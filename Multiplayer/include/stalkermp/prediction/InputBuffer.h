// STALKER-MP — Client input buffer (Sprint-010, Step 3)
//
// A fixed-capacity, sequence-ordered ring of InputCommand retained for local
// prediction and reconciliation replay. Sequences must be strictly ascending
// (a regress is a SequenceMismatch value outcome); capacity is bounded by the
// configured inputBufferDepth (a full buffer is Overflow unless the oldest has
// been pruned). PruneUpTo discards acknowledged inputs; Pending returns the
// still-unacknowledged inputs in ascending order for deterministic replay.
// Storage is pre-reserved once at construction; Push/Prune/Clear perform NO
// dynamic allocation in steady state.
//
// This step introduces the buffer ONLY — no prediction/interpolation logic,
// manager, seams, driver, diagnostics, or engine binding.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (PredictionOutcome).

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stalkermp/prediction/PredictionTypes.h" // InputCommand, PredictionOutcome

namespace stalkermp::prediction
{
    class InputBuffer
    {
    public:
        // Capacity is bounded by the configured inputBufferDepth (>= 1 by config).
        explicit InputBuffer(std::size_t capacity) : m_buffer(capacity == 0 ? 1 : capacity) {}

        // Append an input. Requires a strictly ascending sequence (regress or
        // duplicate => SequenceMismatch, buffer unchanged). A full buffer =>
        // Overflow (buffer unchanged); prune acknowledged inputs first. No allocation.
        [[nodiscard]] PredictionOutcome Push(const InputCommand& command) noexcept
        {
            // Strictly ascending, globally monotonic per client — the guard persists
            // even after PruneUpTo empties the buffer (only Clear resets it).
            if (m_everAccepted && command.sequence <= m_lastSequence)
            {
                return PredictionOutcome::SequenceMismatch;
            }
            if (m_count >= m_buffer.size())
            {
                return PredictionOutcome::BufferOverflow;
            }
            const std::size_t tail = (m_head + m_count) % m_buffer.size();
            m_buffer[tail] = command;
            m_lastSequence = command.sequence;
            m_everAccepted = true;
            ++m_count;
            return PredictionOutcome::Ok;
        }

        // Discard every buffered input whose sequence is <= `sequence` (acknowledged
        // by the host). Deterministic; no allocation. Benign when nothing matches.
        void PruneUpTo(const std::uint64_t sequence) noexcept
        {
            while (m_count > 0 && m_buffer[m_head].sequence <= sequence)
            {
                m_head = (m_head + 1) % m_buffer.size();
                --m_count;
            }
        }

        // The still-buffered inputs with sequence > `fromSequence`, ascending, for
        // deterministic replay. Value copies; the buffer is unchanged.
        [[nodiscard]] std::vector<InputCommand> Pending(const std::uint64_t fromSequence) const
        {
            std::vector<InputCommand> out;
            out.reserve(m_count);
            for (std::size_t i = 0; i < m_count; ++i)
            {
                const InputCommand& c = m_buffer[(m_head + i) % m_buffer.size()];
                if (c.sequence > fromSequence)
                {
                    out.push_back(c); // ascending (ring holds ascending sequences)
                }
            }
            return out;
        }

        void Clear() noexcept
        {
            m_head = 0;
            m_count = 0;
            m_lastSequence = 0;
            m_everAccepted = false; // a fresh session may restart the sequence
        }

        [[nodiscard]] bool Empty() const noexcept { return m_count == 0; }
        [[nodiscard]] bool Full() const noexcept { return m_count >= m_buffer.size(); }
        [[nodiscard]] std::size_t Size() const noexcept { return m_count; }
        [[nodiscard]] std::size_t Capacity() const noexcept { return m_buffer.size(); }
        [[nodiscard]] std::uint64_t LastSequence() const noexcept { return m_lastSequence; }

    private:
        std::vector<InputCommand> m_buffer; // fixed after construction (ring storage)
        std::size_t m_head = 0;
        std::size_t m_count = 0;
        std::uint64_t m_lastSequence = 0; // last accepted sequence (for ascending check)
        bool m_everAccepted = false;      // whether any input was accepted since construction/Clear
    };
} // namespace stalkermp::prediction

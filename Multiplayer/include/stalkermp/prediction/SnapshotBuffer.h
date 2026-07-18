// STALKER-MP — Authoritative snapshot buffer (Sprint-010, Step 7)
//
// A fixed-capacity ring of decoded authoritative SnapshotFrames, kept in
// ascending tick order, from which remote-entity interpolation selects the two
// frames that bracket a delayed render tick. `Push` appends a frame (ascending
// tick; a regress is a SequenceMismatch value outcome, buffer unchanged); when the
// ring is full the oldest frame is evicted (bounded, deterministic). `Pair`
// returns the two frames bracketing `renderTick - interpolationDelayTicks` plus a
// tick-derived interpolation factor in [0,1] — never extrapolating beyond the
// oldest or newest buffered frame (it clamps to the boundary frame instead).
//
// This step introduces the buffer + frame selection ONLY — no interpolation math
// (Step 8) and no manager (Step 9). The returned pointers reference storage inside
// the ring and remain valid until the next Push/Clear.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (PredictionOutcome).

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stalkermp/prediction/PredictionTypes.h" // SnapshotFrame, PredictionOutcome

namespace stalkermp::prediction
{
    // The two authoritative frames bracketing a delayed render tick, and the factor
    // in [0,1] to interpolate from `from` toward `to`. When only one frame brackets
    // the tick (clamped to a boundary), `from == to` and `factor == 0`. `valid` is
    // false only when the buffer is empty (no frame to interpolate from).
    struct SnapshotPair
    {
        const SnapshotFrame* from = nullptr;
        const SnapshotFrame* to = nullptr;
        float factor = 0.0f;
        bool valid = false;
    };

    class SnapshotBuffer
    {
    public:
        // `capacity` bounds the number of buffered frames (>= 1 by config).
        // `interpolationDelayTicks` is the jitter-buffer delay applied in Pair.
        SnapshotBuffer(std::size_t capacity, std::uint64_t interpolationDelayTicks)
            : m_buffer(capacity == 0 ? 1 : capacity), m_delayTicks(interpolationDelayTicks)
        {
        }

        // Append an authoritative frame. Ticks must be strictly ascending (a regress
        // or duplicate => SequenceMismatch, buffer unchanged). When full, the oldest
        // frame is evicted (bounded, deterministic).
        [[nodiscard]] PredictionOutcome Push(const SnapshotFrame& frame);

        // Select the two frames bracketing (renderTick - interpolationDelayTicks) and
        // the interpolation factor in [0,1]. No extrapolation: a target before the
        // oldest or beyond the newest clamps to that boundary frame (factor 0). When
        // the buffer is empty the result is { nullptr, nullptr, 0, false }.
        [[nodiscard]] SnapshotPair Pair(std::uint64_t renderTick) const;

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
        [[nodiscard]] std::uint64_t DelayTicks() const noexcept { return m_delayTicks; }

    private:
        // The i-th frame in ascending order (0 = oldest). Caller ensures i < m_count.
        [[nodiscard]] const SnapshotFrame& FrameAt(std::size_t i) const noexcept
        {
            return m_buffer[(m_head + i) % m_buffer.size()];
        }

        std::vector<SnapshotFrame> m_buffer; // fixed after construction (ring storage)
        std::size_t m_head = 0;
        std::size_t m_count = 0;
        std::uint64_t m_lastTick = 0;    // last pushed tick (ascending guard)
        std::uint64_t m_delayTicks = 0;  // interpolation delay (jitter buffer)
    };
} // namespace stalkermp::prediction

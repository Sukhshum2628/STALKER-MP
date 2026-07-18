// STALKER-MP — Authoritative snapshot buffer (Sprint-010, Step 7)
//
// See SnapshotBuffer.h. Bounded, ascending-tick ring with deterministic frame
// selection and clamping (no extrapolation). Engine-free / OS-free; no exceptions,
// no RTTI, no iostream; value outcomes (PredictionOutcome).

#include "stalkermp/prediction/SnapshotBuffer.h"

namespace stalkermp::prediction
{
    PredictionOutcome SnapshotBuffer::Push(const SnapshotFrame& frame)
    {
        if (m_count > 0 && frame.tick <= m_lastTick)
        {
            return PredictionOutcome::SequenceMismatch; // not ascending -> reject
        }
        if (m_count >= m_buffer.size())
        {
            m_head = (m_head + 1) % m_buffer.size(); // evict oldest (bounded)
            --m_count;
        }
        const std::size_t tail = (m_head + m_count) % m_buffer.size();
        m_buffer[tail] = frame;
        m_lastTick = frame.tick;
        ++m_count;
        return PredictionOutcome::Ok;
    }

    SnapshotPair SnapshotBuffer::Pair(const std::uint64_t renderTick) const
    {
        SnapshotPair result;
        if (m_count == 0)
        {
            return result; // empty -> invalid
        }

        // Apply the interpolation delay (jitter buffer). Saturate at 0: a render tick
        // earlier than the delay targets the oldest available frame (no underflow).
        const std::uint64_t target = (renderTick > m_delayTicks) ? (renderTick - m_delayTicks) : 0;

        const SnapshotFrame& oldest = FrameAt(0);
        const SnapshotFrame& newest = FrameAt(m_count - 1);

        result.valid = true;

        // Clamp before the oldest / at-or-beyond the newest — never extrapolate.
        if (target <= oldest.tick)
        {
            result.from = &oldest;
            result.to = &oldest;
            result.factor = 0.0f;
            return result;
        }
        if (target >= newest.tick)
        {
            result.from = &newest;
            result.to = &newest;
            result.factor = 0.0f;
            return result;
        }

        // Find the bracket [from, to] with from.tick <= target < to.tick.
        for (std::size_t i = 0; i + 1 < m_count; ++i)
        {
            const SnapshotFrame& from = FrameAt(i);
            const SnapshotFrame& to = FrameAt(i + 1);
            if (from.tick <= target && target < to.tick)
            {
                const std::uint64_t span = to.tick - from.tick; // > 0 (strictly ascending)
                const std::uint64_t offset = target - from.tick;
                result.from = &from;
                result.to = &to;
                result.factor = static_cast<float>(offset) / static_cast<float>(span);
                return result;
            }
        }

        // Unreachable given the clamps above; clamp to newest defensively.
        result.from = &newest;
        result.to = &newest;
        result.factor = 0.0f;
        return result;
    }
} // namespace stalkermp::prediction

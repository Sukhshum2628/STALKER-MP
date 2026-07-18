// STALKER-MP — Remote-entity interpolation manager (Sprint-010, Step 9)
//
// Owns the authoritative SnapshotBuffer and produces smoothed remote-entity
// presentation states for a render tick. `PushFrame` buffers one decoded
// authoritative frame (ascending tick). `Interpolate(renderTick, out)` selects the
// two frames bracketing the delayed render tick (SnapshotBuffer::Pair) and blends
// the entities present in BOTH frames (InterpolationStep), APPENDING one
// InterpolatedState per entity to `out` in ascending, unique EntityId order. It
// never extrapolates: at a clamped boundary (from == to) each entity is emitted at
// its own authoritative position; an empty buffer appends nothing.
//
// Interpolation is CLIENT-ONLY presentation: it produces a visual overlay and
// never mutates authoritative simulation state (Host Authority; ADR-008).
//
// This step introduces the manager ONLY — no prediction, reconciliation, seams,
// driver, diagnostics, or engine binding.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (PredictionOutcome).

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stalkermp/prediction/PredictionConfiguration.h"
#include "stalkermp/prediction/PredictionTypes.h" // SnapshotFrame, InterpolatedState, PredictionOutcome
#include "stalkermp/prediction/SnapshotBuffer.h"

namespace stalkermp::prediction
{
    class InterpolationManager
    {
    public:
        // The owned snapshot buffer is sized from the configured historical-frame
        // depth and interpolation delay (both bounded; delay may be 0).
        explicit InterpolationManager(const PredictionConfiguration& config)
            : m_snapshots(config.stateBufferDepth, config.interpolationDelayTicks)
        {
        }

        // Buffer one authoritative frame (ascending tick; a regress is a
        // SequenceMismatch value outcome, buffer unchanged). Bounded ring.
        [[nodiscard]] PredictionOutcome PushFrame(const SnapshotFrame& frame)
        {
            return m_snapshots.Push(frame);
        }

        // Append smoothed remote-entity states for `renderTick` to `out`, ascending
        // and unique by EntityId. Only entities present in BOTH bracketing frames are
        // emitted (no extrapolation of existence); at a clamped boundary every entity
        // in the boundary frame is emitted at its own position. `out` is NOT cleared
        // (append-only). Deterministic. Returns Ok, or NoAuthoritativeFrame when the
        // buffer has no frame to interpolate from (nothing appended).
        [[nodiscard]] PredictionOutcome Interpolate(std::uint64_t renderTick, std::vector<InterpolatedState>& out);

        void Clear() noexcept { m_snapshots.Clear(); }

        // Read-only surface.
        [[nodiscard]] std::size_t BufferedFrameCount() const noexcept { return m_snapshots.Size(); }
        [[nodiscard]] std::size_t Capacity() const noexcept { return m_snapshots.Capacity(); }
        [[nodiscard]] std::uint64_t DelayTicks() const noexcept { return m_snapshots.DelayTicks(); }

    private:
        SnapshotBuffer m_snapshots;
    };
} // namespace stalkermp::prediction

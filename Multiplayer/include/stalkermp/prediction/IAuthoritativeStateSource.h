// STALKER-MP — Authoritative state seam (Sprint-010, Step 10)
//
// The engine-free, READ-ONLY seam through which the client-presentation driver
// pulls decoded authoritative frames. Each SnapshotFrame is built from the
// Sprint-009 replication value shapes (EntityReplicationState /
// PlayerReplicationState) and carries the host's last-processed input sequence
// (SnapshotFrame::ackedInputSequence) for reconciliation. Value-only and
// non-mutating: the driver observes authoritative state, it never writes it
// (Host Authority; Preserve Before Replace; ADR-008).
//
// This step introduces the seam interfaces ONLY — no engine binding (Step 15) and
// no driver (Step 14).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include "stalkermp/prediction/PredictionTypes.h" // SnapshotFrame

namespace stalkermp::prediction
{
    // Yields the next decoded authoritative frame (with ackedInputSequence).
    // Implemented by the engine binding over the replication pipeline; faked in
    // tests. Read-only — it never mutates authoritative simulation state.
    class IAuthoritativeStateSource
    {
    public:
        virtual ~IAuthoritativeStateSource() = default;

        // Fill `out` with the next authoritative frame and return true; return false
        // when no new frame is available (out left unchanged). Const / non-mutating.
        [[nodiscard]] virtual bool NextFrame(SnapshotFrame& out) const = 0;
    };
} // namespace stalkermp::prediction

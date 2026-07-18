// STALKER-MP — Local input seam (Sprint-010, Step 10)
//
// The engine-free seam through which the client-presentation driver polls the
// local player's input for a tick. The engine binding (Step 15) implements this
// over the real input device in a single TU; tests drive it with a fake. Value-
// only: PollInput fills a caller-owned InputCommand and returns whether an input
// was available. Read-only with respect to simulation — producing an input never
// mutates authoritative state (Host Authority; ADR-008).
//
// This step introduces the seam interfaces ONLY — no engine binding (Step 15) and
// no driver (Step 14).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include "stalkermp/prediction/PredictionTypes.h" // InputCommand

namespace stalkermp::prediction
{
    // Supplies the local player's input command for the current tick. Implemented
    // by the engine binding; faked in tests.
    class ILocalInputSource
    {
    public:
        virtual ~ILocalInputSource() = default;

        // Fill `out` with the next local input and return true; return false when no
        // input is available this tick (out left unchanged). Const / non-mutating.
        [[nodiscard]] virtual bool PollInput(InputCommand& out) const = 0;
    };
} // namespace stalkermp::prediction

// STALKER-MP — Presentation sink seam (Sprint-010, Step 10)
//
// The engine-free seam through which the client-presentation driver emits its
// CLIENT-VISUAL results: the locally predicted state and the smoothed remote
// interpolated states. The engine binding (Step 15) implements this by applying
// the transforms to the render/scene representation only; tests observe the
// emitted values with a fake. These are presentation transforms exclusively —
// applying them never mutates authoritative simulation state (Host Authority;
// ADR-008).
//
// This step introduces the seam interfaces ONLY — no engine binding (Step 15) and
// no driver (Step 14).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include "stalkermp/prediction/PredictionTypes.h" // PredictedState, InterpolatedState

namespace stalkermp::prediction
{
    // Receives client-visual presentation results. Implemented by the engine
    // binding (scene/render only); faked in tests. Applying a value is a visual
    // transform and never mutates authoritative simulation state.
    class IPresentationSink
    {
    public:
        virtual ~IPresentationSink() = default;

        // Apply the locally predicted state (local player's visual overlay).
        virtual void ApplyLocal(const PredictedState& state) = 0;

        // Apply one smoothed remote-entity state (remote visual overlay).
        virtual void ApplyRemote(const InterpolatedState& state) = 0;
    };
} // namespace stalkermp::prediction

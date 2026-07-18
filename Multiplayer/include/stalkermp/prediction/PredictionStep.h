// STALKER-MP — Deterministic prediction integrator (Sprint-010, Step 5)
//
// The pure, fixed-timestep client-side movement integrator: apply one input to
// one predicted state, producing the next predicted state. One call advances
// exactly one prediction tick. Deterministic (identical inputs => identical
// output) and value-in/value-out — it predicts only local movement/rotation/
// stance, never authoritative gameplay outcomes, and never touches simulation,
// the engine, the OS, or a clock. Header-only (a pure step belongs in the header).
//
// This step introduces the integrator ONLY — no buffers, manager, reconciliation,
// interpolation, seams, driver, diagnostics, or engine binding.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cmath>

#include "stalkermp/prediction/PredictionConfiguration.h"
#include "stalkermp/prediction/PredictionTypes.h"

namespace stalkermp::prediction
{
    // Integrate one input into the prior predicted state, advancing one tick.
    // The input's `move` is the local-space displacement intent for this tick; it
    // is rotated into world space about the up (Y) axis by the input's `yaw`, added
    // to the prior position, and reported as the per-tick velocity. Facing (`yaw`)
    // and opaque stance/interaction flags come from the input. Deterministic.
    [[nodiscard]] inline PredictedState Integrate(const PredictedState& prior, const InputCommand& input,
                                                  const PredictionConfiguration& config) noexcept
    {
        (void)config; // fixed-timestep (one tick); movement scaling reserved for later

        // Yaw rotation about the up axis (right-handed, Y up). Deterministic float ops.
        const float c = std::cos(input.yaw);
        const float s = std::sin(input.yaw);
        const world::Vec3 worldMove{input.move.x * c - input.move.z * s, input.move.y,
                                    input.move.x * s + input.move.z * c};

        PredictedState next{};
        next.id = prior.id;
        next.tick = input.tick;
        next.position = world::Vec3{prior.position.x + worldMove.x, prior.position.y + worldMove.y,
                                    prior.position.z + worldMove.z};
        next.velocity = worldMove;      // per-tick displacement == velocity (fixed timestep)
        next.yaw = input.yaw;           // facing set by the input
        next.stateFlags = input.actionBits; // opaque stance / basic-interaction intent
        return next;
    }
} // namespace stalkermp::prediction

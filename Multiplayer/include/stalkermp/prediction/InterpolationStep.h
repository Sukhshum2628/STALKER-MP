// STALKER-MP — Deterministic interpolation step (Sprint-010, Step 8)
//
// The pure, header-only blend between two authoritative states by a factor in
// [0,1]: position is linearly interpolated and yaw is angularly interpolated along
// the shortest arc (wrapping across the +/-pi seam). Deterministic (identical
// inputs => identical output), value-in/value-out, and NON-extrapolating — the
// factor is clamped to [0,1], so `factor=0 -> from` and `factor=1 -> to` exactly.
//
// This produces a CLIENT-ONLY presentation overlay for remote entities; it never
// mutates authoritative simulation state (Host Authority; ADR-008).
//
// Note (Preserve Before Replace): the Sprint-009 authoritative entity/player
// states carry position but not orientation, so the state overloads interpolate
// position and carry yaw as 0 (no authoritative orientation source yet). The
// angular primitive `InterpolateYaw` is provided and fully tested here for reuse
// wherever a yaw source exists (e.g. the locally predicted state, Step 9+).
//
// This step introduces the pure step ONLY — no buffer (Step 7) and no manager
// (Step 9).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include "stalkermp/replication/ReplicationTypes.h" // EntityReplicationState / PlayerReplicationState
#include "stalkermp/prediction/PredictionTypes.h"   // InterpolatedState
#include "stalkermp/world/EntityView.h"             // world::Vec3

namespace stalkermp::prediction
{
    // Clamp an interpolation factor into [0,1] (no extrapolation). Deterministic.
    [[nodiscard]] inline float ClampFactor(const float factor) noexcept
    {
        return factor < 0.0f ? 0.0f : (factor > 1.0f ? 1.0f : factor);
    }

    // Linear interpolation of a position by a clamped factor. `factor=0 -> from`,
    // `factor=1 -> to`. Pure and deterministic.
    [[nodiscard]] inline world::Vec3 InterpolatePosition(const world::Vec3& from, const world::Vec3& to,
                                                         const float factor) noexcept
    {
        const float t = ClampFactor(factor);
        return world::Vec3{from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t,
                           from.z + (to.z - from.z) * t};
    }

    // Angular interpolation of yaw (radians) along the shortest arc, wrapping the
    // delta into (-pi, pi] so a blend across the +/-pi seam takes the short way
    // round. Clamped factor; `factor=0 -> fromYaw`. Pure and deterministic.
    [[nodiscard]] inline float InterpolateYaw(const float fromYaw, const float toYaw, const float factor) noexcept
    {
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kTwoPi = 6.28318530717958647692f;

        const float t = ClampFactor(factor);
        float delta = toYaw - fromYaw;
        while (delta > kPi)
        {
            delta -= kTwoPi;
        }
        while (delta < -kPi)
        {
            delta += kTwoPi;
        }
        return fromYaw + delta * t;
    }

    // Blend two authoritative entity states into a remote presentation state.
    // Position is lerped; `id` is carried from `from`. Yaw is 0 (the authoritative
    // entity state carries no orientation yet — see file header). Pure/deterministic.
    [[nodiscard]] inline InterpolatedState Interpolate(const replication::EntityReplicationState& from,
                                                       const replication::EntityReplicationState& to,
                                                       const float factor) noexcept
    {
        InterpolatedState out{};
        out.id = from.id;
        out.position = InterpolatePosition(from.position, to.position, factor);
        out.yaw = 0.0f; // no authoritative orientation source (Preserve Before Replace)
        return out;
    }

    // Player overload: blend two authoritative player states into a remote
    // presentation state (position lerp; `id` carried from the player's entity).
    [[nodiscard]] inline InterpolatedState Interpolate(const replication::PlayerReplicationState& from,
                                                       const replication::PlayerReplicationState& to,
                                                       const float factor) noexcept
    {
        InterpolatedState out{};
        out.id = from.entity;
        out.position = InterpolatePosition(from.position, to.position, factor);
        out.yaw = 0.0f; // no authoritative orientation source (Preserve Before Replace)
        return out;
    }
} // namespace stalkermp::prediction

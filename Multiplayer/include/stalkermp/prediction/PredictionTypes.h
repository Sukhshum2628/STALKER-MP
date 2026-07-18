// STALKER-MP — Client prediction & interpolation value types (Sprint-010, Step 1)
//
// The engine-free, OS-free foundational vocabulary of the CLIENT-SIDE prediction
// & interpolation subsystem: the input command, predicted/interpolated state, the
// authoritative snapshot frame, and the statistics/consistency value shapes that
// every later Sprint-010 step builds on. Types ONLY — no logic, no buffers, no
// manager, no seams, no thread, no packets.
//
// Prediction is client-only presentation; it produces visual transforms and never
// mutates authoritative simulation state (Host Authority; ADR-008). These value
// captures never hold a live object or raw pointer. Deterministic layout, `0 =
// none`, wall-clock excluded from replay identity (diagnostic only). Reuses the
// Sprint-009 replication value shapes as the decoded authoritative input
// (Preserve Before Replace).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>
#include <vector>

#include "stalkermp/player/PlayerTypes.h"            // player::PlayerId
#include "stalkermp/replication/ReplicationTypes.h"  // replication::EntityReplicationState / PlayerReplicationState
#include "stalkermp/world/EntityView.h"              // world::EntityId, world::Vec3

namespace stalkermp::prediction
{
    // ------------------------------------------------------------- Enumerations
    // Fixed std::uint8_t underlying type (deterministic ABI); reserved 0 carries
    // the neutral/safe meaning; new enumerators are appended, never renumbered.

    // Named value outcome for the (later) fallible prediction operations (ADR-007).
    enum class PredictionOutcome : std::uint8_t
    {
        Ok = 0,
        ClientOnly,          // operation is meaningful only on a client (host = identity)
        BufferOverflow,      // a bounded buffer is full
        SequenceMismatch,    // an input sequence regressed or is out of order
        NoAuthoritativeFrame,// no authoritative frame available to reconcile/interpolate
        CorrectionRejected,  // a correction exceeded a hard limit and was rejected
    };

    [[nodiscard]] constexpr const char* PredictionOutcomeName(const PredictionOutcome o) noexcept
    {
        switch (o)
        {
        case PredictionOutcome::Ok:                   return "Ok";
        case PredictionOutcome::ClientOnly:           return "ClientOnly";
        case PredictionOutcome::BufferOverflow:       return "BufferOverflow";
        case PredictionOutcome::SequenceMismatch:     return "SequenceMismatch";
        case PredictionOutcome::NoAuthoritativeFrame: return "NoAuthoritativeFrame";
        case PredictionOutcome::CorrectionRejected:   return "CorrectionRejected";
        }
        return "Unknown";
    }

    // The correction a reconciliation applied to a predicted state.
    enum class CorrectionKind : std::uint8_t
    {
        None = 0,     // predicted state matched authoritative within thresholds
        Smoothed,     // small error corrected smoothly toward authoritative
        Snapped,      // large error snapped to authoritative (host authority wins)
    };

    [[nodiscard]] constexpr const char* CorrectionKindName(const CorrectionKind k) noexcept
    {
        switch (k)
        {
        case CorrectionKind::None:     return "None";
        case CorrectionKind::Smoothed: return "Smoothed";
        case CorrectionKind::Snapped:  return "Snapped";
        }
        return "Unknown";
    }

    // ---------------------------------------------------------- Value captures
    // Values only — never a live object, reference, or handle into simulation.

    // One client input sample. `sequence` is monotonic per client; `tick` is the
    // client prediction tick the input applies to.
    struct InputCommand
    {
        std::uint64_t sequence = 0; // monotonic client input sequence (0 = none)
        std::uint64_t tick = 0;     // client prediction tick this input applies to
        world::Vec3 move{};         // movement intent (local space)
        float yaw = 0.0f;           // facing/rotation intent (radians)
        std::uint32_t actionBits = 0; // opaque action flags (stance / basic interactions)
    };

    // A predicted local presentation state at a given tick (client-only overlay).
    struct PredictedState
    {
        world::EntityId id{};       // local entity (0 = none)
        std::uint64_t tick = 0;     // prediction tick
        world::Vec3 position{};
        world::Vec3 velocity{};
        float yaw = 0.0f;
        std::uint32_t stateFlags = 0; // opaque (stance / animation intent)
    };

    // A smoothed remote presentation state (interpolated between authoritative
    // frames). Client-only overlay.
    struct InterpolatedState
    {
        world::EntityId id{}; // 0 = none
        world::Vec3 position{};
        float yaw = 0.0f;
    };

    // One decoded authoritative frame (from the replication pipeline). Values only;
    // entities ascending by EntityId, players ascending by PlayerId.
    struct SnapshotFrame
    {
        std::uint64_t tick = 0;                                       // authoritative simulation tick
        std::uint64_t ackedInputSequence = 0;                        // last client input the host processed
        std::vector<replication::EntityReplicationState> entities;   // ascending EntityId
        std::vector<replication::PlayerReplicationState> players;    // ascending PlayerId
    };

    // Read-only aggregate tallies (populated by the diagnostics collector later).
    struct PredictionStatistics
    {
        std::uint64_t predictionsRun = 0;
        std::uint64_t corrections = 0;
        std::uint64_t snaps = 0;
        std::uint64_t interpolations = 0;
        std::uint64_t overflows = 0;
        std::uint64_t inputsRecorded = 0;
        std::uint32_t lastCorrectionMagnitude = 0; // fixed-point (mm); diagnostic
        std::uint32_t maxCorrectionMagnitude = 0;  // fixed-point (mm); diagnostic
        std::uint64_t timestampWallClock = 0;      // DIAGNOSTIC ONLY — not replay identity
    };

    // Read-only consistency report; value shape fixed here, populated by later
    // steps/diagnostics. Healthy by default.
    struct PredictionConsistency
    {
        bool clientOnly = true;              // prediction never runs authoritatively
        bool noAuthoritativeMutation = true; // authoritative state is never mutated
        bool inputsMonotonic = true;         // input sequences strictly increasing
        bool statesTickOrdered = true;       // predicted states ascending by tick
        bool interpolationBounded = true;    // no extrapolation beyond the delay
        bool hostAuthorityPreserved = true;  // reconciliation snaps toward authoritative

        [[nodiscard]] bool IsHealthy() const noexcept
        {
            return clientOnly && noAuthoritativeMutation && inputsMonotonic && statesTickOrdered &&
                   interpolationBounded && hostAuthorityPreserved;
        }
    };
} // namespace stalkermp::prediction

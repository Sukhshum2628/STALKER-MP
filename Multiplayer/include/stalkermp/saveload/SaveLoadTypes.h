// STALKER-MP — Save/Load value types & vocabulary (Sprint-012, Step 1)
//
// The engine-free, OS-free foundational vocabulary of the host-side SAVE/LOAD
// subsystem: the outcome/phase/state enumerations, the save descriptor, the
// value-only restoration records, and the statistics/consistency value shapes that
// every later Sprint-012 step builds on. Types ONLY — no serialization, loading,
// restoration, migration, filesystem, or engine access.
//
// Save/Load is HOST-SIDE and reconstructs the single authoritative world at startup
// (Host Authority; One World; One ALife Simulation). These value captures hold only
// plain scalars and the Sprint-011 value shapes — never a live object, reference, or
// engine/OS handle. Deterministic layout, `0 = none/neutral`, wall-clock excluded
// from replay identity (diagnostic only). Reuses the Sprint-011 persistence value
// shapes (`SaveMetadata`, `VersionSet`) unchanged (Preserve Before Replace).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>

#include "stalkermp/persistence/PersistenceTypes.h" // persistence::SaveMetadata (reused, unchanged)
#include "stalkermp/persistence/VersionManager.h"   // persistence::VersionSet (reused, unchanged)
#include "stalkermp/player/PlayerTypes.h"           // player::PlayerId / PlayerConnectionState
#include "stalkermp/world/EntityView.h"             // world::EntityId / world::Vec3

namespace stalkermp::saveload
{
    // ------------------------------------------------------------- Enumerations
    // Fixed std::uint8_t underlying type (deterministic ABI); reserved 0 carries
    // the neutral/safe meaning; new enumerators are appended, never renumbered.

    // Named value outcome for the (later) fallible save/load operations (ADR-007).
    enum class SaveLoadOutcome : std::uint8_t
    {
        Ok = 0,
        CorruptedSave,      // the save artifact is structurally invalid
        VersionUnsupported, // the save version has no supported migration path
        VersionMismatch,    // the save version is incompatible with the build
        IntegrityFailure,   // an integrity check (counts / references / duplicates) failed
        MissingData,        // a required section/record is absent
        ChecksumFailure,    // a content checksum did not match
        InterruptedLoad,    // the load was interrupted before completion
        PartialRecovery,    // recovery completed only partially (rolled back)
        StorageUnavailable, // the storage backend could not be read
        NothingToLoad,      // no save is present to load
    };

    [[nodiscard]] constexpr const char* SaveLoadOutcomeName(const SaveLoadOutcome o) noexcept
    {
        switch (o)
        {
        case SaveLoadOutcome::Ok:                 return "Ok";
        case SaveLoadOutcome::CorruptedSave:      return "CorruptedSave";
        case SaveLoadOutcome::VersionUnsupported: return "VersionUnsupported";
        case SaveLoadOutcome::VersionMismatch:    return "VersionMismatch";
        case SaveLoadOutcome::IntegrityFailure:   return "IntegrityFailure";
        case SaveLoadOutcome::MissingData:        return "MissingData";
        case SaveLoadOutcome::ChecksumFailure:    return "ChecksumFailure";
        case SaveLoadOutcome::InterruptedLoad:    return "InterruptedLoad";
        case SaveLoadOutcome::PartialRecovery:    return "PartialRecovery";
        case SaveLoadOutcome::StorageUnavailable: return "StorageUnavailable";
        case SaveLoadOutcome::NothingToLoad:      return "NothingToLoad";
        }
        return "Unknown";
    }

    // The world-restoration phase currently in progress (deterministic order).
    enum class RestorePhase : std::uint8_t
    {
        World = 0,
        Environment,
        Registry,
        Players,
        ALife,
        Scheduler,
        Story,
        Complete,
    };

    [[nodiscard]] constexpr const char* RestorePhaseName(const RestorePhase p) noexcept
    {
        switch (p)
        {
        case RestorePhase::World:       return "World";
        case RestorePhase::Environment: return "Environment";
        case RestorePhase::Registry:    return "Registry";
        case RestorePhase::Players:     return "Players";
        case RestorePhase::ALife:       return "ALife";
        case RestorePhase::Scheduler:   return "Scheduler";
        case RestorePhase::Story:       return "Story";
        case RestorePhase::Complete:    return "Complete";
        }
        return "Unknown";
    }

    // The lifecycle state of a load/recovery operation.
    enum class LoadState : std::uint8_t
    {
        Idle = 0,
        Reading,
        Validating,
        Migrating,
        Restoring,
        Completed,
        Failed,
    };

    [[nodiscard]] constexpr const char* LoadStateName(const LoadState s) noexcept
    {
        switch (s)
        {
        case LoadState::Idle:       return "Idle";
        case LoadState::Reading:    return "Reading";
        case LoadState::Validating: return "Validating";
        case LoadState::Migrating:  return "Migrating";
        case LoadState::Restoring:  return "Restoring";
        case LoadState::Completed:  return "Completed";
        case LoadState::Failed:     return "Failed";
        }
        return "Unknown";
    }

    // ---------------------------------------------------------- Value captures
    // Values only — never a live object, reference, or handle into simulation.

    // Identifies one persisted save. `metadata` is the Sprint-011 value shape,
    // reused unchanged; `generation` orders successive saves of a slot.
    struct SaveDescriptor
    {
        std::uint64_t saveId = 0;            // 0 = none
        persistence::SaveMetadata metadata{}; // Sprint-011 metadata (reused)
        std::uint32_t generation = 0;
    };

    // --- Restoration records (value-only; applied through the restore seams later).

    // World + global state (Architecture §7.4).
    struct WorldRestoreRecord
    {
        std::uint64_t simulationTick = 0;
        std::uint64_t worldTimeSeconds = 0;
        std::uint32_t weatherId = 0;
        std::uint32_t globalStateFlags = 0;
        std::uint32_t zoneConfigId = 0;
    };

    // Environment (Architecture §7.4).
    struct EnvironmentRestoreRecord
    {
        std::uint32_t weatherId = 0;
        std::uint32_t timeOfDaySeconds = 0;
        std::uint32_t emissionState = 0;
        std::uint32_t lighting = 0;
        std::uint32_t environmentVersion = 0;
    };

    // One entity (Architecture §7.5). `id`/`inventoryRef`/`owner` are value ids.
    struct EntityRestoreRecord
    {
        world::EntityId id{};            // 0 = none
        world::Vec3 position{};
        world::Vec3 velocity{};
        std::uint32_t stateFlags = 0;    // opaque runtime state
        std::uint32_t runtimeFlags = 0;  // opaque flag bits
        world::EntityId inventoryRef{};  // value reference (0 = none)
        world::EntityId owner{};         // ownership (0 = none)
        std::uint64_t runtimeState = 0;  // opaque runtime state word
        std::uint32_t relationship = 0;  // opaque relationship word
    };

    // One player (Architecture §7.6). Players are restored offline; they reconnect.
    struct PlayerRestoreRecord
    {
        player::PlayerId id{};           // 0 = none
        world::EntityId entity{};        // owning entity (0 = none)
        world::Vec3 position{};
        std::uint32_t statistics = 0;    // opaque
        std::uint32_t questProgress = 0; // opaque
        std::uint32_t factionId = 0;
        player::PlayerConnectionState connectionState = player::PlayerConnectionState::Connected;
    };

    // ALife (Architecture §7.7). Opaque value words applied via the sanctioned seam.
    struct AlifeRestoreRecord
    {
        std::uint32_t smartTerrainId = 0;
        std::uint32_t taskManagerState = 0;
        std::uint32_t simulationState = 0;
        world::EntityId offlineObject{};   // 0 = none
        std::uint32_t npcSchedule = 0;
        std::uint32_t factionPlannerState = 0;
        std::uint32_t storyLinkId = 0;
    };

    // Scheduler (Architecture §7.8).
    struct SchedulerRestoreRecord
    {
        std::uint64_t simulationTick = 0;
        std::uint32_t pendingUpdates = 0;
        std::uint32_t queueDepth = 0;
        std::uint32_t deferredTasks = 0;
        std::uint64_t executionOrder = 0; // deterministic ordering key
    };

    // Read-only aggregate tallies (populated by the diagnostics collector later).
    struct RecoveryStatistics
    {
        std::uint64_t saveDurationMicros = 0;   // DIAGNOSTIC ONLY (timing)
        std::uint64_t loadDurationMicros = 0;   // DIAGNOSTIC ONLY (timing)
        std::uint64_t entitiesRestored = 0;
        std::uint64_t playersRestored = 0;
        std::uint64_t migrationTimeMicros = 0;  // DIAGNOSTIC ONLY (timing)
        std::uint64_t validationFailures = 0;
        std::uint64_t recoveryAttempts = 0;
        std::uint64_t recoverySuccesses = 0;
        std::uint64_t timestampWallClock = 0;   // DIAGNOSTIC ONLY — not replay identity
    };

    // Read-only consistency report; value shape fixed here, populated by later
    // steps/diagnostics. Healthy by default.
    struct RecoveryConsistency
    {
        bool deterministicRestore = true;      // restoration is order-fixed & deterministic
        bool restoredBeforeNetworking = true;  // simulation restored before networking starts
        bool authoritativeOnly = true;         // only the host restores authoritative state
        bool versionValidated = true;          // save version validated before restore
        bool integrityValidated = true;        // integrity validated before restore
        bool engineWritesSanctioned = true;    // authoritative writes only via sanctioned seams

        [[nodiscard]] bool IsHealthy() const noexcept
        {
            return deterministicRestore && restoredBeforeNetworking && authoritativeOnly && versionValidated &&
                   integrityValidated && engineWritesSanctioned;
        }
    };
} // namespace stalkermp::saveload

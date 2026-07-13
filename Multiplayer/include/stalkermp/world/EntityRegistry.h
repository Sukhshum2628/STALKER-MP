// STALKER-MP — Entity Registry core (Sprint-003, §7.1/§7.3/§7.4; invariants I3/I4)
//
// The authoritative record store for persistent entity identity and metadata.
// This is the registry ITSELF — no engine interaction, no engine pointers, no
// frame synchronization, no callbacks, no feed/reconciliation, no relcase, no
// events, no statistics, no spatial queries, no threading, no activation state.
// Those belong to later Sprint-003 steps.
//
// Storage (invariant I3): the canonical primary store is a std::vector kept
// sorted ascending by EntityId, which owns the records and defines the canonical
// iteration order. A std::unordered_map (Evidence Gate 1, approved) is a
// secondary generation accelerator only and never defines iteration order.
//
// ADR-007: engine-agnostic; no engine headers (One Engine Boundary / I2); no
// CObject / engine pointers (C2); no exceptions, no throwing STL, no iostream /
// filesystem (invariants 4/6, I10). Fallible operations return core::Expected<T>.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/world/EntityHandle.h"
#include "stalkermp/world/EntityRecord.h"
#include "stalkermp/world/EntityView.h" // world::EntityId

namespace stalkermp::world
{
    // Metadata-only registry statistics (§7.11; invariant I7). Every field is
    // derived from entity-category / spawn-source METADATA or from a count of
    // registry operations. No session, connection, or gameplay state appears here
    // (the WorldContext Player Count Leak lesson, AI_CONTEXT.md): "playerCount" is
    // the number of records whose category is Player, not a count of connections.
    struct RegistryStatistics
    {
        std::size_t entityCount = 0;       // total live records
        std::size_t npcCount = 0;
        std::size_t monsterCount = 0;
        std::size_t itemCount = 0;
        std::size_t vehicleCount = 0;
        std::size_t playerCount = 0;       // category == Player (metadata only, I7)
        std::size_t dynamicSpawnCount = 0; // spawnSource == Dynamic
        std::uint64_t destroyedCount = 0;  // total unregistrations this session
    };

    // Classification of a runtime handle against the registry (§7.12 Invalid
    // Handle Detection).
    enum class HandleValidity : std::uint8_t
    {
        Valid = 0,        // live record, matching generation
        NullHandle,       // null or malformed handle (id == 0 or generation == 0)
        UnknownId,        // well-formed handle, but no live record for the id
        StaleGeneration,  // live record exists, but the generation does not match
    };

    [[nodiscard]] constexpr const char* HandleValidityName(const HandleValidity validity) noexcept
    {
        switch (validity)
        {
        case HandleValidity::Valid:           return "Valid";
        case HandleValidity::NullHandle:      return "NullHandle";
        case HandleValidity::UnknownId:       return "UnknownId";
        case HandleValidity::StaleGeneration: return "StaleGeneration";
        }
        return "Unknown";
    }

    // Result of a read-only registry integrity scan (§7.12 Duplicate Detection /
    // Missing & Invalid Entity Validation). Every list is empty on a healthy
    // registry. Reports (never repairs).
    struct RegistryIntegrityReport
    {
        std::vector<EntityId> duplicateIds;         // adjacent equal ids (I3 violation)
        std::vector<EntityId> outOfOrder;           // id < previous (I3 violation)
        std::vector<EntityId> invalidRecords;       // id == 0 or generation == 0
        std::vector<EntityId> generationMismatches; // record generation exceeds the counter

        [[nodiscard]] bool IsHealthy() const noexcept
        {
            return duplicateIds.empty() && outOfOrder.empty()
                && invalidRecords.empty() && generationMismatches.empty();
        }
    };

    class EntityRegistry
    {
    public:
        using Visitor = std::function<void(const EntityRecord&)>;

        EntityRegistry() = default;

        // Register a new persistent entity. Rejects a null EntityId
        // (InvalidArgument) and a currently-live duplicate id (AlreadyExists).
        // Assigns a session-local generation (I4): the first registration of an
        // id yields generation 1; each later reuse of the same id increments the
        // generation so prior handles become detectably stale. Returns the runtime
        // handle on success.
        [[nodiscard]] core::Expected<EntityHandle>
        RegisterEntity(EntityId id, std::string name, EntityMetadata metadata);

        // Pre-allocate a deterministic EntityId (host-authoritative).
        [[nodiscard]] core::Expected<EntityId> ReserveEntity();

        // Register a pre-assigned identity + metadata.
        [[nodiscard]] core::Expected<EntityHandle>
        RestoreEntity(EntityId id, EntityGeneration generation, std::string name, EntityMetadata metadata);

        // Remove a live entity by runtime handle. The handle must be valid and its
        // generation must match the live record; an unknown id or a stale
        // generation returns NotFound. (The engine feed's benign-no-op policy, C3,
        // is applied by the feed layer in a later step, not by this core API.)
        [[nodiscard]] core::Expected<void> UnregisterEntity(EntityHandle handle);

        // Pure identity lookup (ignores generation). Returns nullptr if not live.
        // The returned pointer is valid only until the next mutating call.
        [[nodiscard]] const EntityRecord* FindByEntityId(EntityId id) const noexcept;

        // Validated lookup: returns the record only if the handle's generation
        // matches the live record; nullptr otherwise. Valid until next mutation.
        [[nodiscard]] const EntityRecord* FindByHandle(EntityHandle handle) const noexcept;

        // Safe accessor: returns a copy, or NotFound instead of nullptr.
        [[nodiscard]] core::Expected<EntityRecord> GetRecord(EntityHandle handle) const;

        [[nodiscard]] bool Contains(EntityId id) const noexcept;
        [[nodiscard]] std::size_t Size() const noexcept;

        // Remove all records and reset generation tracking.
        void Clear() noexcept;

        // Visit every live record in canonical ascending-EntityId order (I3).
        void ForEach(const Visitor& visitor) const;

        // --- Extended lookup (§7.4) and typed iteration (§7.8) --------------
        // All scan the canonical store, so every result is in ascending-EntityId
        // order (I3). No secondary name/type/section indices are introduced yet;
        // hash acceleration (§7.9) is deferred to profiling-driven work (§7.13).

        // First live record whose name matches, in canonical order (names are not
        // guaranteed unique). nullptr if none; valid until the next mutating call.
        [[nodiscard]] const EntityRecord* FindByName(std::string_view name) const noexcept;

        // Handles of all live records of the given category / section, in
        // canonical ascending-EntityId order.
        [[nodiscard]] std::vector<EntityHandle> FindByType(EntityCategory category) const;
        [[nodiscard]] std::vector<EntityHandle> FindBySection(std::string_view section) const;

        // Visit every live record of the given category in canonical order (I3).
        void ForEachOfType(EntityCategory category, const Visitor& visitor) const;

        // --- Registration lifecycle (§7.6) ----------------------------------
        // The deterministic REGISTRATION state machine. These are registration
        // states, NOT the ALife online/offline state (I5, owned by Sprint-004/005).
        // Legal transitions:
        //   Registered  -> Initialized
        //   Initialized -> Active
        //   {Registered, Initialized, Active} -> PendingRemoval
        // RegisterEntity enters Registered; UnregisterEntity removes a record
        // outright; PendingRemoval is the marked pre-removal state.

        // Move a live entity to `target` if the transition is legal. Invalid handle
        // -> InvalidArgument; unknown/stale handle -> NotFound; illegal transition
        // -> InvalidArgument. Registration state is not the sort key, so canonical
        // ordering (I3) is unaffected.
        [[nodiscard]] core::Expected<void>
        SetRegistrationState(EntityHandle handle, EntityRegistrationState target);

        // Current registration state of a live entity (validated by handle).
        [[nodiscard]] core::Expected<EntityRegistrationState>
        GetRegistrationState(EntityHandle handle) const;

        // Visit every live record whose registration state is Active, in canonical
        // ascending-EntityId order (§7.8; I3).
        void ForEachActive(const Visitor& visitor) const;

        // --- Registration lifecycle events (§7.7) ----------------------------
        // Non-fallible void dispatch, deterministic registration order, non-reentrant.
        // Subscription lifetime is owned by the subscriber; cleared at shutdown.

        using SubscriptionId = std::uint32_t;
        inline static constexpr SubscriptionId kInvalidSubscriptionId = 0;
        using EntityEventCallback = std::function<void(const EntityRecord&)>;

        [[nodiscard]] core::Expected<SubscriptionId> SubscribeOnRegistered(EntityEventCallback callback);
        [[nodiscard]] core::Expected<SubscriptionId> SubscribeOnUnregistered(EntityEventCallback callback);
        [[nodiscard]] core::Expected<SubscriptionId> SubscribeOnRestored(EntityEventCallback callback);

        core::Expected<void> UnsubscribeOnRegistered(SubscriptionId id) noexcept;
        core::Expected<void> UnsubscribeOnUnregistered(SubscriptionId id) noexcept;
        core::Expected<void> UnsubscribeOnRestored(SubscriptionId id) noexcept;

        // --- Registry statistics (§7.11; metadata-only, I7) ------------------
        // Computed on demand by a single deterministic scan of the canonical store
        // (plus the retained destroyed-operation counter). Read-only; safe to call
        // during event dispatch.
        [[nodiscard]] RegistryStatistics ComputeStatistics() const noexcept;

        // --- Debug utilities (§7.12; read-only, engine-free) -----------------
        // All are non-mutating and safe to call during event dispatch.

        // Classify a runtime handle against the registry (Invalid Handle Detection).
        [[nodiscard]] HandleValidity ClassifyHandle(EntityHandle handle) const noexcept;

        // Human-readable description of one live entity (Entity Inspector).
        // NotFound if the handle does not resolve to a live record.
        [[nodiscard]] core::Expected<std::string> InspectEntity(EntityHandle handle) const;

        // Human-readable dump of every live record in canonical order (Registry Dump).
        [[nodiscard]] std::string DumpRegistry() const;

        // Read-only integrity scan (Duplicate Detection / Missing & Invalid Entity
        // Validation). A healthy registry returns an all-empty report.
        [[nodiscard]] RegistryIntegrityReport ValidateIntegrity() const;

    private:
        static constexpr std::size_t kInvalidIndex = static_cast<std::size_t>(-1);

        // Canonical primary storage: always sorted ascending by handle.id.value.
        // Owns records and defines canonical iteration order (I3).
        std::vector<EntityRecord> m_records;

        // Secondary accelerator (Gate 1): id.value -> last generation assigned.
        // Persists across unregister so id reuse increments generation. Never
        // defines iteration order.
        std::unordered_map<std::uint32_t, EntityGeneration> m_generationById;

        // Binary search the sorted canonical store; kInvalidIndex if absent.
        [[nodiscard]] std::size_t IndexOf(EntityId id) const noexcept;

        // Non-const access to a live record validated by handle; nullptr otherwise.
        [[nodiscard]] EntityRecord* MutableRecord(EntityHandle handle) noexcept;

        // Legality of a registration-state transition (§7.6 deterministic machine).
        [[nodiscard]] static bool
        IsLegalRegistrationTransition(EntityRegistrationState from, EntityRegistrationState to) noexcept;

        // Human-readable one-line description of a record (§7.12 helper).
        [[nodiscard]] static std::string DescribeRecord(const EntityRecord& record);

        struct EventSubscription
        {
            SubscriptionId id = kInvalidSubscriptionId;
            EntityEventCallback callback;
        };

        std::vector<EventSubscription> m_onRegisteredSubscribers;
        std::vector<EventSubscription> m_onUnregisteredSubscribers;
        std::vector<EventSubscription> m_onRestoredSubscribers;

        SubscriptionId m_nextSubscriptionId = 1;
        std::uint32_t m_nextReservedId = 0x10000;
        bool m_dispatchingEvents = false;

        // Running total of successful unregistrations this session (§7.11
        // "Destroyed Count"). Not derivable from live records, so it is retained
        // here; reset by Clear().
        std::uint64_t m_destroyedCount = 0;
    };
} // namespace stalkermp::world

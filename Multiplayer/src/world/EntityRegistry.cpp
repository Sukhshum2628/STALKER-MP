// STALKER-MP — Entity Registry core implementation (Sprint-003, §7.1/§7.3/§7.4)
//
// See EntityRegistry.h. Engine-agnostic; no engine headers, no exceptions, no
// throwing STL (ADR-007). The canonical store (m_records) is kept sorted
// ascending by EntityId so iteration and lookup are deterministic (invariant I3).

#include "stalkermp/world/EntityRegistry.h"

#include <algorithm>
#include <utility>

#include "stalkermp/core/Error.h"
#include "stalkermp/common/StringUtil.h" // common::Format (ADR-007-safe, no iostream)

namespace stalkermp::world
{
    struct DispatchGuard
    {
        bool& flag;
        explicit DispatchGuard(bool& f) noexcept : flag(f) { flag = true; }
        ~DispatchGuard() noexcept { flag = false; }
    };

    std::size_t EntityRegistry::IndexOf(const EntityId id) const noexcept
    {
        // Sorted-by-id invariant lets us binary search the canonical store.
        const auto it = std::lower_bound(
            m_records.begin(), m_records.end(), id.value,
            [](const EntityRecord& record, const std::uint32_t value) noexcept {
                return record.handle.id.value < value;
            });

        if (it != m_records.end() && it->handle.id.value == id.value)
        {
            return static_cast<std::size_t>(it - m_records.begin());
        }
        return kInvalidIndex;
    }

    core::Expected<EntityHandle>
    EntityRegistry::RegisterEntity(const EntityId id, std::string name, EntityMetadata metadata)
    {
        if (m_dispatchingEvents)
        {
            return core::MakeError(core::ErrorCode::Internal,
                                   "EntityRegistry::RegisterEntity: mutation during event dispatch is forbidden");
        }

        if (id.value == 0)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "EntityRegistry::RegisterEntity: null EntityId");
        }

        // Duplicate live id is rejected (uniqueness).
        const auto position = std::lower_bound(
            m_records.begin(), m_records.end(), id.value,
            [](const EntityRecord& record, const std::uint32_t value) noexcept {
                return record.handle.id.value < value;
            });

        if (position != m_records.end() && position->handle.id.value == id.value)
        {
            return core::MakeError(core::ErrorCode::AlreadyExists,
                                   "EntityRegistry::RegisterEntity: EntityId already registered");
        }

        // Session-local generation (I4): first use = 1, each reuse increments.
        const EntityGeneration previous = m_generationById[id.value]; // 0 if absent
        const EntityGeneration generation = previous + 1;
        m_generationById[id.value] = generation;

        EntityRecord record;
        record.handle = EntityHandle{id, generation};
        record.name = std::move(name);
        record.metadata = std::move(metadata);
        record.metadata.state = EntityRegistrationState::Registered;

        // Insert at the sorted position to preserve the ascending-EntityId
        // canonical order (I3). `position` remains valid (no prior mutation).
        const auto insertedIt = m_records.insert(position, std::move(record));

        // Dispatch registered event in deterministic registration order, non-reentrant
        if (!m_onRegisteredSubscribers.empty())
        {
            DispatchGuard guard(m_dispatchingEvents);
            for (const auto& sub : m_onRegisteredSubscribers)
            {
                if (sub.callback)
                {
                    sub.callback(*insertedIt);
                }
            }
        }

        return EntityHandle{id, generation};
    }

    core::Expected<void> EntityRegistry::UnregisterEntity(const EntityHandle handle)
    {
        if (m_dispatchingEvents)
        {
            return core::MakeError(core::ErrorCode::Internal,
                                   "EntityRegistry::UnregisterEntity: mutation during event dispatch is forbidden");
        }

        if (!handle.IsValid())
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "EntityRegistry::UnregisterEntity: invalid handle");
        }

        const std::size_t index = IndexOf(handle.id);
        if (index == kInvalidIndex)
        {
            return core::MakeError(core::ErrorCode::NotFound,
                                   "EntityRegistry::UnregisterEntity: EntityId not registered");
        }

        if (m_records[index].handle.generation != handle.generation)
        {
            return core::MakeError(core::ErrorCode::NotFound,
                                   "EntityRegistry::UnregisterEntity: stale handle generation");
        }

        // Dispatch unregistered event in deterministic registration order, non-reentrant
        if (!m_onUnregisteredSubscribers.empty())
        {
            DispatchGuard guard(m_dispatchingEvents);
            for (const auto& sub : m_onUnregisteredSubscribers)
            {
                if (sub.callback)
                {
                    sub.callback(m_records[index]);
                }
            }
        }

        // Erase preserves the sorted order of the remaining records. The
        // generation counter is intentionally retained so a future reuse of this
        // id increments the generation.
        m_records.erase(m_records.begin() + static_cast<std::ptrdiff_t>(index));
        ++m_destroyedCount; // §7.11 Destroyed Count (not derivable from live records).
        return core::Success();
    }

    const EntityRecord* EntityRegistry::FindByEntityId(const EntityId id) const noexcept
    {
        const std::size_t index = IndexOf(id);
        return index == kInvalidIndex ? nullptr : &m_records[index];
    }

    const EntityRecord* EntityRegistry::FindByHandle(const EntityHandle handle) const noexcept
    {
        const EntityRecord* record = FindByEntityId(handle.id);
        if (record != nullptr && record->handle.generation == handle.generation)
        {
            return record;
        }
        return nullptr;
    }

    core::Expected<EntityRecord> EntityRegistry::GetRecord(const EntityHandle handle) const
    {
        if (const EntityRecord* record = FindByHandle(handle); record != nullptr)
        {
            return *record;
        }
        return core::MakeError(core::ErrorCode::NotFound,
                               "EntityRegistry::GetRecord: no live record for handle");
    }

    bool EntityRegistry::Contains(const EntityId id) const noexcept
    {
        return IndexOf(id) != kInvalidIndex;
    }

    std::size_t EntityRegistry::Size() const noexcept
    {
        return m_records.size();
    }

    void EntityRegistry::Clear() noexcept
    {
        m_records.clear();
        m_generationById.clear();
        m_onRegisteredSubscribers.clear();
        m_onUnregisteredSubscribers.clear();
        m_onRestoredSubscribers.clear();
        m_nextSubscriptionId = 1;
        m_nextReservedId = 0x10000;
        m_dispatchingEvents = false;
        m_destroyedCount = 0;
    }

    void EntityRegistry::ForEach(const Visitor& visitor) const
    {
        if (!visitor)
        {
            return;
        }
        // m_records is sorted ascending by EntityId -> canonical order (I3).
        for (const EntityRecord& record : m_records)
        {
            visitor(record);
        }
    }

    const EntityRecord* EntityRegistry::FindByName(const std::string_view name) const noexcept
    {
        // Canonical order: first match is the lowest EntityId (deterministic).
        for (const EntityRecord& record : m_records)
        {
            if (std::string_view{record.name} == name)
            {
                return &record;
            }
        }
        return nullptr;
    }

    std::vector<EntityHandle> EntityRegistry::FindByType(const EntityCategory category) const
    {
        std::vector<EntityHandle> result;
        for (const EntityRecord& record : m_records) // ascending EntityId order
        {
            if (record.metadata.category == category)
            {
                result.push_back(record.handle);
            }
        }
        return result;
    }

    std::vector<EntityHandle> EntityRegistry::FindBySection(const std::string_view section) const
    {
        std::vector<EntityHandle> result;
        for (const EntityRecord& record : m_records) // ascending EntityId order
        {
            if (std::string_view{record.metadata.section} == section)
            {
                result.push_back(record.handle);
            }
        }
        return result;
    }

    void EntityRegistry::ForEachOfType(const EntityCategory category, const Visitor& visitor) const
    {
        if (!visitor)
        {
            return;
        }
        for (const EntityRecord& record : m_records) // ascending EntityId order
        {
            if (record.metadata.category == category)
            {
                visitor(record);
            }
        }
    }

    EntityRecord* EntityRegistry::MutableRecord(const EntityHandle handle) noexcept
    {
        if (!handle.IsValid())
        {
            return nullptr;
        }
        const std::size_t index = IndexOf(handle.id);
        if (index == kInvalidIndex)
        {
            return nullptr;
        }
        EntityRecord& record = m_records[index];
        return record.handle.generation == handle.generation ? &record : nullptr;
    }

    bool EntityRegistry::IsLegalRegistrationTransition(const EntityRegistrationState from,
                                                       const EntityRegistrationState to) noexcept
    {
        switch (from)
        {
        case EntityRegistrationState::Registered:
            return to == EntityRegistrationState::Initialized
                || to == EntityRegistrationState::PendingRemoval;
        case EntityRegistrationState::Initialized:
            return to == EntityRegistrationState::Active
                || to == EntityRegistrationState::PendingRemoval;
        case EntityRegistrationState::Active:
            return to == EntityRegistrationState::PendingRemoval;
        case EntityRegistrationState::Constructed:
        case EntityRegistrationState::PendingRemoval:
        case EntityRegistrationState::Unregistered:
            return false;
        }
        return false;
    }

    core::Expected<void> EntityRegistry::SetRegistrationState(const EntityHandle handle,
                                                              const EntityRegistrationState target)
    {
        if (m_dispatchingEvents)
        {
            return core::MakeError(core::ErrorCode::Internal,
                                   "EntityRegistry::SetRegistrationState: mutation during event dispatch is forbidden");
        }

        if (!handle.IsValid())
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "EntityRegistry::SetRegistrationState: invalid handle");
        }

        EntityRecord* record = MutableRecord(handle);
        if (record == nullptr)
        {
            return core::MakeError(core::ErrorCode::NotFound,
                                   "EntityRegistry::SetRegistrationState: no live record for handle");
        }

        if (!IsLegalRegistrationTransition(record->metadata.state, target))
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "EntityRegistry::SetRegistrationState: illegal registration transition");
        }

        record->metadata.state = target;
        return core::Success();
    }

    core::Expected<EntityRegistrationState>
    EntityRegistry::GetRegistrationState(const EntityHandle handle) const
    {
        if (const EntityRecord* record = FindByHandle(handle); record != nullptr)
        {
            return record->metadata.state;
        }
        return core::MakeError(core::ErrorCode::NotFound,
                               "EntityRegistry::GetRegistrationState: no live record for handle");
    }

    void EntityRegistry::ForEachActive(const Visitor& visitor) const
    {
        if (!visitor)
        {
            return;
        }
        for (const EntityRecord& record : m_records) // ascending EntityId order
        {
            if (record.metadata.state == EntityRegistrationState::Active)
            {
                visitor(record);
            }
        }
    }

    RegistryStatistics EntityRegistry::ComputeStatistics() const noexcept
    {
        RegistryStatistics stats;
        stats.entityCount = m_records.size();
        stats.destroyedCount = m_destroyedCount;

        for (const EntityRecord& record : m_records)
        {
            switch (record.metadata.category)
            {
            case EntityCategory::Npc:     ++stats.npcCount;     break;
            case EntityCategory::Monster: ++stats.monsterCount; break;
            case EntityCategory::Item:    ++stats.itemCount;    break;
            case EntityCategory::Vehicle: ++stats.vehicleCount; break;
            case EntityCategory::Player:  ++stats.playerCount;  break;
            case EntityCategory::Unknown: break;
            }

            if (record.metadata.spawnSource == EntitySpawnSource::Dynamic)
            {
                ++stats.dynamicSpawnCount;
            }
        }

        return stats;
    }

    std::string EntityRegistry::DescribeRecord(const EntityRecord& record)
    {
        return common::Format(
            "Entity id={} gen={} name='{}' category={} section='{}' state={} spawn={} flags={} createdTick={}",
            record.handle.id.value, record.handle.generation, record.name,
            EntityCategoryName(record.metadata.category), record.metadata.section,
            EntityRegistrationStateName(record.metadata.state),
            EntitySpawnSourceName(record.metadata.spawnSource),
            record.metadata.flags, record.metadata.creationTick);
    }

    HandleValidity EntityRegistry::ClassifyHandle(const EntityHandle handle) const noexcept
    {
        if (!handle.IsValid()) // id == 0 or generation == 0
        {
            return HandleValidity::NullHandle;
        }
        const EntityRecord* record = FindByEntityId(handle.id);
        if (record == nullptr)
        {
            return HandleValidity::UnknownId;
        }
        if (record->handle.generation != handle.generation)
        {
            return HandleValidity::StaleGeneration;
        }
        return HandleValidity::Valid;
    }

    core::Expected<std::string> EntityRegistry::InspectEntity(const EntityHandle handle) const
    {
        if (const EntityRecord* record = FindByHandle(handle); record != nullptr)
        {
            return DescribeRecord(*record);
        }
        return core::MakeError(core::ErrorCode::NotFound,
                               "EntityRegistry::InspectEntity: no live record for handle");
    }

    std::string EntityRegistry::DumpRegistry() const
    {
        std::string out = common::Format("EntityRegistry: {} live, {} destroyed\n",
                                         m_records.size(), m_destroyedCount);
        for (const EntityRecord& record : m_records) // canonical ascending-EntityId order (I3)
        {
            out += DescribeRecord(record);
            out += '\n';
        }
        return out;
    }

    RegistryIntegrityReport EntityRegistry::ValidateIntegrity() const
    {
        RegistryIntegrityReport report;

        std::uint32_t previousId = 0;
        bool first = true;
        for (const EntityRecord& record : m_records)
        {
            const EntityId id = record.handle.id;

            // Malformed record: null id or invalid generation.
            if (id.value == 0 || record.handle.generation == kInvalidGeneration)
            {
                report.invalidRecords.push_back(id);
            }

            // Canonical store must be strictly ascending and unique (I3).
            if (!first)
            {
                if (id.value == previousId)
                {
                    report.duplicateIds.push_back(id);
                }
                else if (id.value < previousId)
                {
                    report.outOfOrder.push_back(id);
                }
            }
            previousId = id.value;
            first = false;

            // No live record may carry a generation beyond the last-assigned
            // high-water mark for its id. (A restored older generation is legal,
            // so this is an upper-bound check, not equality.)
            const auto it = m_generationById.find(id.value);
            if (it == m_generationById.end() || record.handle.generation > it->second)
            {
                report.generationMismatches.push_back(id);
            }
        }

        return report;
    }

    core::Expected<EntityId> EntityRegistry::ReserveEntity()
    {
        if (m_dispatchingEvents)
        {
            return core::MakeError(core::ErrorCode::Internal,
                                   "EntityRegistry::ReserveEntity: mutation during event dispatch is forbidden");
        }

        while (Contains(EntityId{m_nextReservedId}) || m_generationById.count(m_nextReservedId) > 0)
        {
            m_nextReservedId++;
        }

        return EntityId{m_nextReservedId++};
    }

    core::Expected<EntityHandle>
    EntityRegistry::RestoreEntity(const EntityId id, const EntityGeneration generation, std::string name, EntityMetadata metadata)
    {
        if (m_dispatchingEvents)
        {
            return core::MakeError(core::ErrorCode::Internal,
                                   "EntityRegistry::RestoreEntity: mutation during event dispatch is forbidden");
        }

        if (id.value == 0)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "EntityRegistry::RestoreEntity: null EntityId");
        }

        if (generation == kInvalidGeneration)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "EntityRegistry::RestoreEntity: invalid generation");
        }

        // Duplicate live id is rejected.
        const auto position = std::lower_bound(
            m_records.begin(), m_records.end(), id.value,
            [](const EntityRecord& record, const std::uint32_t value) noexcept {
                return record.handle.id.value < value;
            });

        if (position != m_records.end() && position->handle.id.value == id.value)
        {
            return core::MakeError(core::ErrorCode::AlreadyExists,
                                   "EntityRegistry::RestoreEntity: EntityId already registered");
        }

        // Ensure generation sequence is updated.
        const EntityGeneration currentLast = m_generationById[id.value];
        if (generation > currentLast)
        {
            m_generationById[id.value] = generation;
        }

        EntityRecord record;
        record.handle = EntityHandle{id, generation};
        record.name = std::move(name);
        record.metadata = std::move(metadata);

        const auto insertedIt = m_records.insert(position, std::move(record));

        // Dispatch restored event in deterministic registration order, non-reentrant
        if (!m_onRestoredSubscribers.empty())
        {
            DispatchGuard guard(m_dispatchingEvents);
            for (const auto& sub : m_onRestoredSubscribers)
            {
                if (sub.callback)
                {
                    sub.callback(*insertedIt);
                }
            }
        }

        return EntityHandle{id, generation};
    }

    core::Expected<EntityRegistry::SubscriptionId>
    EntityRegistry::SubscribeOnRegistered(EntityEventCallback callback)
    {
        if (m_dispatchingEvents)
        {
            return core::MakeError(core::ErrorCode::Internal,
                                   "EntityRegistry::SubscribeOnRegistered: subscription changes during event dispatch are forbidden");
        }
        if (!callback)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "EntityRegistry::SubscribeOnRegistered: null callback");
        }
        const SubscriptionId id = m_nextSubscriptionId++;
        m_onRegisteredSubscribers.push_back({id, std::move(callback)});
        return id;
    }

    core::Expected<EntityRegistry::SubscriptionId>
    EntityRegistry::SubscribeOnUnregistered(EntityEventCallback callback)
    {
        if (m_dispatchingEvents)
        {
            return core::MakeError(core::ErrorCode::Internal,
                                   "EntityRegistry::SubscribeOnUnregistered: subscription changes during event dispatch are forbidden");
        }
        if (!callback)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "EntityRegistry::SubscribeOnUnregistered: null callback");
        }
        const SubscriptionId id = m_nextSubscriptionId++;
        m_onUnregisteredSubscribers.push_back({id, std::move(callback)});
        return id;
    }

    core::Expected<EntityRegistry::SubscriptionId>
    EntityRegistry::SubscribeOnRestored(EntityEventCallback callback)
    {
        if (m_dispatchingEvents)
        {
            return core::MakeError(core::ErrorCode::Internal,
                                   "EntityRegistry::SubscribeOnRestored: subscription changes during event dispatch are forbidden");
        }
        if (!callback)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "EntityRegistry::SubscribeOnRestored: null callback");
        }
        const SubscriptionId id = m_nextSubscriptionId++;
        m_onRestoredSubscribers.push_back({id, std::move(callback)});
        return id;
    }

    core::Expected<void> EntityRegistry::UnsubscribeOnRegistered(const SubscriptionId id) noexcept
    {
        if (m_dispatchingEvents)
        {
            return core::MakeError(core::ErrorCode::Internal,
                                   "EntityRegistry::UnsubscribeOnRegistered: subscription changes during event dispatch are forbidden");
        }
        const auto it = std::find_if(
            m_onRegisteredSubscribers.begin(), m_onRegisteredSubscribers.end(),
            [id](const EventSubscription& sub) noexcept { return sub.id == id; });

        if (it != m_onRegisteredSubscribers.end())
        {
            m_onRegisteredSubscribers.erase(it);
            return core::Success();
        }
        return core::MakeError(core::ErrorCode::NotFound,
                               "EntityRegistry::UnsubscribeOnRegistered: subscription id not found");
    }

    core::Expected<void> EntityRegistry::UnsubscribeOnUnregistered(const SubscriptionId id) noexcept
    {
        if (m_dispatchingEvents)
        {
            return core::MakeError(core::ErrorCode::Internal,
                                   "EntityRegistry::UnsubscribeOnUnregistered: subscription changes during event dispatch are forbidden");
        }
        const auto it = std::find_if(
            m_onUnregisteredSubscribers.begin(), m_onUnregisteredSubscribers.end(),
            [id](const EventSubscription& sub) noexcept { return sub.id == id; });

        if (it != m_onUnregisteredSubscribers.end())
        {
            m_onUnregisteredSubscribers.erase(it);
            return core::Success();
        }
        return core::MakeError(core::ErrorCode::NotFound,
                               "EntityRegistry::UnsubscribeOnUnregistered: subscription id not found");
    }

    core::Expected<void> EntityRegistry::UnsubscribeOnRestored(const SubscriptionId id) noexcept
    {
        if (m_dispatchingEvents)
        {
            return core::MakeError(core::ErrorCode::Internal,
                                   "EntityRegistry::UnsubscribeOnRestored: subscription changes during event dispatch are forbidden");
        }
        const auto it = std::find_if(
            m_onRestoredSubscribers.begin(), m_onRestoredSubscribers.end(),
            [id](const EventSubscription& sub) noexcept { return sub.id == id; });

        if (it != m_onRestoredSubscribers.end())
        {
            m_onRestoredSubscribers.erase(it);
            return core::Success();
        }
        return core::MakeError(core::ErrorCode::NotFound,
                               "EntityRegistry::UnsubscribeOnRestored: subscription id not found");
    }
} // namespace stalkermp::world

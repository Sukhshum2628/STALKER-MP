// STALKER-MP — In-memory persistence store (Sprint-011, Step 8)
//
// See InMemoryPersistenceStore.h. Engine-free / OS-free; no exceptions, no RTTI,
// no iostream; value outcomes.

#include "stalkermp/persistence/InMemoryPersistenceStore.h"

#include "stalkermp/core/Error.h" // core::MakeError, core::ErrorCode

namespace stalkermp::persistence
{
    InMemoryPersistenceStore::Pending* InMemoryPersistenceStore::Find(const StoreHandle handle) noexcept
    {
        if (!handle.IsValid())
        {
            return nullptr;
        }
        for (Pending& p : m_pending)
        {
            if (p.handle == handle)
            {
                return &p;
            }
        }
        return nullptr;
    }

    void InMemoryPersistenceStore::Erase(const StoreHandle handle) noexcept
    {
        for (std::size_t i = 0; i < m_pending.size(); ++i)
        {
            if (m_pending[i].handle == handle)
            {
                m_pending.erase(m_pending.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
        }
    }

    core::Expected<StoreHandle> InMemoryPersistenceStore::Begin(const SaveMetadata& metadata)
    {
        if (!m_available)
        {
            return core::MakeError(core::ErrorCode::IoError, "persistence store unavailable");
        }
        const StoreHandle handle{++m_nextHandle};
        m_pending.push_back(Pending{handle, metadata, /*written*/ false});
        return handle;
    }

    PersistenceOutcome InMemoryPersistenceStore::Write(const StoreHandle handle, const PersistenceSnapshot&)
    {
        Pending* pending = Find(handle);
        if (pending == nullptr)
        {
            return PersistenceOutcome::StorageUnavailable; // unknown/invalid handle
        }
        if (m_failWrites)
        {
            return PersistenceOutcome::StorageUnavailable; // simulated backend write failure
        }
        pending->written = true;
        return PersistenceOutcome::Ok;
    }

    PersistenceOutcome InMemoryPersistenceStore::Commit(const StoreHandle handle)
    {
        Pending* pending = Find(handle);
        if (pending == nullptr)
        {
            return PersistenceOutcome::StorageUnavailable; // unknown/invalid handle
        }
        if (!pending->written)
        {
            return PersistenceOutcome::IncompleteSnapshot; // nothing written to commit
        }
        m_committed.push_back(pending->metadata); // becomes the current save
        Erase(handle);
        return PersistenceOutcome::Ok;
    }

    void InMemoryPersistenceStore::Abort(const StoreHandle handle) noexcept
    {
        Erase(handle); // committed history (previous save) is retained
    }
} // namespace stalkermp::persistence

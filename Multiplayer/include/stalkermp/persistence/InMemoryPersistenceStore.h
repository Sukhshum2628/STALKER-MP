// STALKER-MP — In-memory persistence store (Sprint-011, Step 8)
//
// An IPersistenceStore that records committed saves in memory (no OS, no file, no
// serialization). It is the default/test backend: it lets the framework be
// exercised end-to-end and lets tests observe what would have been persisted, and
// it can simulate backend failure (unavailable / write failure) to exercise the
// worker's failure-isolation path. The real filesystem backend is Sprint-012.
//
// Atomicity: Begin opens a pending transaction; Write marks it written; Commit
// appends its metadata to the committed history (this becomes the current save);
// Abort discards the pending transaction, leaving the committed history — the
// previous save — intact.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stalkermp/persistence/IPersistenceStore.h"

namespace stalkermp::persistence
{
    class InMemoryPersistenceStore final : public IPersistenceStore
    {
    public:
        [[nodiscard]] core::Expected<StoreHandle> Begin(const SaveMetadata& metadata) override;
        [[nodiscard]] PersistenceOutcome Write(StoreHandle handle, const PersistenceSnapshot& snapshot) override;
        [[nodiscard]] PersistenceOutcome Commit(StoreHandle handle) override;
        void Abort(StoreHandle handle) noexcept override;

        // Failure simulation (test control): when unavailable, Begin fails; when write
        // failure is set, Write fails. Both leave committed history intact.
        void SetAvailable(bool available) noexcept { m_available = available; }
        void SetFailWrites(bool fail) noexcept { m_failWrites = fail; }

        // Read-only observation of what has been persisted.
        [[nodiscard]] std::size_t CommittedCount() const noexcept { return m_committed.size(); }
        [[nodiscard]] std::size_t PendingCount() const noexcept { return m_pending.size(); }
        [[nodiscard]] bool HasCommitted() const noexcept { return !m_committed.empty(); }
        [[nodiscard]] const SaveMetadata& LastCommitted() const noexcept { return m_committed.back(); }

    private:
        struct Pending
        {
            StoreHandle handle{};
            SaveMetadata metadata{};
            bool written = false;
        };

        [[nodiscard]] Pending* Find(StoreHandle handle) noexcept;
        void Erase(StoreHandle handle) noexcept;

        std::vector<Pending> m_pending;      // open transactions
        std::vector<SaveMetadata> m_committed; // committed history (last = current)
        std::uint64_t m_nextHandle = 0;
        bool m_available = true;
        bool m_failWrites = false;
    };
} // namespace stalkermp::persistence

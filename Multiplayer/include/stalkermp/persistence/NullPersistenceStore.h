// STALKER-MP — Null persistence store (Sprint-011, Step 8)
//
// An inert IPersistenceStore that accepts every transaction and discards it. It
// records nothing and never fails — the "/dev/null" backend for builds where no
// storage is configured. Engine-free / OS-free. ADR-007.

#pragma once

#include <cstdint>

#include "stalkermp/persistence/IPersistenceStore.h"

namespace stalkermp::persistence
{
    class NullPersistenceStore final : public IPersistenceStore
    {
    public:
        [[nodiscard]] core::Expected<StoreHandle> Begin(const SaveMetadata&) override
        {
            return StoreHandle{++m_nextHandle};
        }
        [[nodiscard]] PersistenceOutcome Write(StoreHandle, const PersistenceSnapshot&) override
        {
            return PersistenceOutcome::Ok;
        }
        [[nodiscard]] PersistenceOutcome Commit(StoreHandle) override { return PersistenceOutcome::Ok; }
        void Abort(StoreHandle) noexcept override {}

    private:
        std::uint64_t m_nextHandle = 0;
    };
} // namespace stalkermp::persistence

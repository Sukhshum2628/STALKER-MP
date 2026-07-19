// STALKER-MP — In-memory save backend (Sprint-012, Step 9)
//
// The engine-free, OS-free counterpart of the platform filesystem backend. Committed
// writes are held in memory and are readable through the same object's ISaveSource,
// so the composition root can run save + recovery end-to-end without a filesystem.
// No OS/file code lives here (that is PlatformSaveStore.cpp).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes.

#include <cstdint>
#include <memory>
#include <vector>

#include "stalkermp/adapters/PlatformSaveStore.h"
#include "stalkermp/core/Error.h"
#include "stalkermp/persistence/PersistenceSnapshot.h" // persistence::PersistenceSnapshot
#include "stalkermp/saveload/SaveWriter.h"              // saveload::SaveWriter

namespace stalkermp::adapters
{
    namespace
    {
        class InMemorySaveBackend final : public ISaveBackend,
                                          public persistence::IPersistenceStore,
                                          public saveload::ISaveSource
        {
        public:
            // --- ISaveBackend ----------------------------------------------------
            [[nodiscard]] persistence::IPersistenceStore& Store() noexcept override { return *this; }
            [[nodiscard]] saveload::ISaveSource& Source() noexcept override { return *this; }
            [[nodiscard]] saveload::SaveLoadOutcome Delete(const std::uint64_t saveId) override
            {
                for (std::size_t i = 0; i < m_saves.size(); ++i)
                {
                    if (m_saves[i].descriptor.saveId == saveId)
                    {
                        m_saves.erase(m_saves.begin() + static_cast<std::ptrdiff_t>(i));
                        return saveload::SaveLoadOutcome::Ok;
                    }
                }
                return saveload::SaveLoadOutcome::Ok; // absent -> benign
            }

            // --- persistence::IPersistenceStore (write) --------------------------
            [[nodiscard]] core::Expected<persistence::StoreHandle>
            Begin(const persistence::SaveMetadata& metadata) override
            {
                const persistence::StoreHandle handle{++m_nextHandle};
                m_pending.push_back(Pending{handle, metadata, {}, false});
                return handle;
            }

            [[nodiscard]] persistence::PersistenceOutcome
            Write(const persistence::StoreHandle handle, const persistence::PersistenceSnapshot& snapshot) override
            {
                Pending* p = FindPending(handle);
                if (p == nullptr)
                {
                    return persistence::PersistenceOutcome::StorageUnavailable;
                }
                p->bytes = saveload::SaveWriter::Serialize(snapshot.View(), p->metadata);
                p->written = true;
                return persistence::PersistenceOutcome::Ok;
            }

            [[nodiscard]] persistence::PersistenceOutcome Commit(const persistence::StoreHandle handle) override
            {
                Pending* p = FindPending(handle);
                if (p == nullptr)
                {
                    return persistence::PersistenceOutcome::StorageUnavailable;
                }
                if (!p->written)
                {
                    return persistence::PersistenceOutcome::IncompleteSnapshot;
                }
                StoreSave(p->metadata, p->bytes);
                ErasePending(handle);
                return persistence::PersistenceOutcome::Ok;
            }

            void Abort(const persistence::StoreHandle handle) noexcept override { ErasePending(handle); }

            // --- saveload::ISaveSource (read) ------------------------------------
            [[nodiscard]] std::vector<saveload::SaveDescriptor> Enumerate() const override
            {
                std::vector<saveload::SaveDescriptor> out;
                out.reserve(m_saves.size());
                for (const Save& s : m_saves) // kept ascending by saveId
                {
                    out.push_back(s.descriptor);
                }
                return out;
            }

            [[nodiscard]] core::Expected<std::vector<std::uint8_t>> Read(const std::uint64_t saveId) const override
            {
                for (const Save& s : m_saves)
                {
                    if (s.descriptor.saveId == saveId)
                    {
                        return s.bytes;
                    }
                }
                return core::MakeError(core::ErrorCode::NotFound, "save not found");
            }

            [[nodiscard]] bool Exists(const std::uint64_t saveId) const override
            {
                for (const Save& s : m_saves)
                {
                    if (s.descriptor.saveId == saveId)
                    {
                        return true;
                    }
                }
                return false;
            }

        private:
            struct Pending
            {
                persistence::StoreHandle handle{};
                persistence::SaveMetadata metadata{};
                std::vector<std::uint8_t> bytes;
                bool written = false;
            };
            struct Save
            {
                saveload::SaveDescriptor descriptor{};
                std::vector<std::uint8_t> bytes;
            };

            [[nodiscard]] Pending* FindPending(const persistence::StoreHandle h) noexcept
            {
                for (Pending& p : m_pending)
                {
                    if (p.handle == h)
                    {
                        return &p;
                    }
                }
                return nullptr;
            }

            void ErasePending(const persistence::StoreHandle h) noexcept
            {
                for (std::size_t i = 0; i < m_pending.size(); ++i)
                {
                    if (m_pending[i].handle == h)
                    {
                        m_pending.erase(m_pending.begin() + static_cast<std::ptrdiff_t>(i));
                        return;
                    }
                }
            }

            void StoreSave(const persistence::SaveMetadata& metadata, const std::vector<std::uint8_t>& bytes)
            {
                saveload::SaveDescriptor descriptor{};
                descriptor.saveId = metadata.saveId;
                descriptor.metadata = metadata;

                for (Save& s : m_saves) // replace existing (same saveId)
                {
                    if (s.descriptor.saveId == metadata.saveId)
                    {
                        s.descriptor = descriptor;
                        s.bytes = bytes;
                        return;
                    }
                }
                std::size_t insertAt = m_saves.size(); // insert ascending by saveId
                for (std::size_t i = 0; i < m_saves.size(); ++i)
                {
                    if (metadata.saveId < m_saves[i].descriptor.saveId)
                    {
                        insertAt = i;
                        break;
                    }
                }
                m_saves.insert(m_saves.begin() + static_cast<std::ptrdiff_t>(insertAt), Save{descriptor, bytes});
            }

            std::vector<Pending> m_pending;
            std::vector<Save> m_saves; // ascending by saveId
            std::uint64_t m_nextHandle = 0;
        };
    } // namespace

    std::unique_ptr<ISaveBackend> CreateInMemorySaveBackend()
    {
        return std::make_unique<InMemorySaveBackend>();
    }
} // namespace stalkermp::adapters

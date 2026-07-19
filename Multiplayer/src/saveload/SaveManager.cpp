// STALKER-MP — Save manager (Sprint-012, Step 14)
//
// See SaveManager.h. Engine-free / OS-free; no exceptions, no RTTI, no iostream;
// value outcomes.

#include "stalkermp/saveload/SaveManager.h"

#include "stalkermp/persistence/PersistenceSnapshot.h" // persistence::PersistenceSnapshot
#include "stalkermp/saveload/SaveIntegrityValidator.h" // SaveIntegrityValidator (reused, Step 6)
#include "stalkermp/saveload/SaveReader.h"             // SaveReader (reused, Step 5)

namespace stalkermp::saveload
{
    namespace
    {
        [[nodiscard]] SaveLoadOutcome MapStoreOutcome(persistence::PersistenceOutcome o) noexcept
        {
            switch (o)
            {
            case persistence::PersistenceOutcome::Ok:                 return SaveLoadOutcome::Ok;
            case persistence::PersistenceOutcome::StorageUnavailable: return SaveLoadOutcome::StorageUnavailable;
            case persistence::PersistenceOutcome::IncompleteSnapshot: return SaveLoadOutcome::MissingData;
            case persistence::PersistenceOutcome::VersionMismatch:    return SaveLoadOutcome::VersionMismatch;
            default:                                                  return SaveLoadOutcome::IntegrityFailure;
            }
        }
    } // namespace

    SaveLoadOutcome SaveManager::CreateSave(const snapshot::ISnapshotView& view,
                                            const persistence::SaveMetadata& metadata)
    {
        persistence::IPersistenceStore& store = m_backend.Store();

        auto begin = store.Begin(metadata);
        if (!begin.HasValue())
        {
            return SaveLoadOutcome::StorageUnavailable;
        }
        const persistence::StoreHandle handle = begin.Value();

        const persistence::PersistenceSnapshot snapshot{view};
        if (const persistence::PersistenceOutcome w = store.Write(handle, snapshot);
            w != persistence::PersistenceOutcome::Ok)
        {
            store.Abort(handle); // retain previous
            return MapStoreOutcome(w);
        }
        if (const persistence::PersistenceOutcome c = store.Commit(handle);
            c != persistence::PersistenceOutcome::Ok)
        {
            store.Abort(handle); // retain previous
            return MapStoreOutcome(c);
        }
        return SaveLoadOutcome::Ok;
    }

    SaveLoadOutcome SaveManager::ValidateSave(const std::uint64_t saveId) const
    {
        auto read = m_backend.Source().Read(saveId);
        if (!read.HasValue())
        {
            return SaveLoadOutcome::NothingToLoad;
        }
        const ParseResult parsed = SaveReader::Parse(read.Value());
        if (parsed.outcome != SaveLoadOutcome::Ok)
        {
            return parsed.outcome; // CorruptedSave / VersionMismatch / ChecksumFailure
        }
        return SaveIntegrityValidator::Validate(m_build, parsed.save);
    }
} // namespace stalkermp::saveload

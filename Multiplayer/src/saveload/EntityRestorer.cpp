// STALKER-MP — Entity restorer (Sprint-012, Step 12)
//
// See EntityRestorer.h. Engine-free / OS-free; no exceptions, no RTTI, no iostream;
// value outcomes.

#include "stalkermp/saveload/EntityRestorer.h"

namespace stalkermp::saveload
{
    SaveLoadOutcome EntityRestorer::Restore(const LoadedSave& save, IEntityRestoreSink& sink)
    {
        for (const EntityRestoreRecord& entity : save.entities) // ascending EntityId order
        {
            if (const SaveLoadOutcome o = sink.Apply(entity); o != SaveLoadOutcome::Ok)
            {
                return o;
            }
        }
        return SaveLoadOutcome::Ok;
    }
} // namespace stalkermp::saveload

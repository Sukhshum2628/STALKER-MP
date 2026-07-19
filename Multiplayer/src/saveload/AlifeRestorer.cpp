// STALKER-MP — ALife restorer (Sprint-012, Step 13)
//
// See AlifeRestorer.h. Engine-free / OS-free; no exceptions, no RTTI, no iostream;
// value outcomes.

#include "stalkermp/saveload/AlifeRestorer.h"

namespace stalkermp::saveload
{
    SaveLoadOutcome AlifeRestorer::Restore(const LoadedSave& save, IAlifeRestoreSink& sink)
    {
        for (const AlifeRestoreRecord& record : save.alife) // save order (deterministic)
        {
            if (const SaveLoadOutcome o = sink.Apply(record); o != SaveLoadOutcome::Ok)
            {
                return o;
            }
        }
        return SaveLoadOutcome::Ok;
    }
} // namespace stalkermp::saveload

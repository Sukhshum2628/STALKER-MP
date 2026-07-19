// STALKER-MP — World + Environment restorer (Sprint-012, Step 11)
//
// See WorldRestorer.h. Engine-free / OS-free; no exceptions, no RTTI, no iostream;
// value outcomes.

#include "stalkermp/saveload/WorldRestorer.h"

namespace stalkermp::saveload
{
    SaveLoadOutcome WorldRestorer::Restore(const LoadedSave& save, IWorldRestoreSink& worldSink,
                                           IEnvironmentRestoreSink& environmentSink)
    {
        if (const SaveLoadOutcome o = worldSink.Apply(save.world); o != SaveLoadOutcome::Ok)
        {
            return o;
        }
        if (const SaveLoadOutcome o = environmentSink.Apply(save.environment); o != SaveLoadOutcome::Ok)
        {
            return o;
        }
        return SaveLoadOutcome::Ok;
    }
} // namespace stalkermp::saveload

// STALKER-MP — Player restorer (Sprint-012, Step 12)
//
// See PlayerRestorer.h. Engine-free / OS-free; no exceptions, no RTTI, no iostream;
// value outcomes.

#include "stalkermp/saveload/PlayerRestorer.h"

namespace stalkermp::saveload
{
    SaveLoadOutcome PlayerRestorer::Restore(const LoadedSave& save, IPlayerRestoreSink& sink)
    {
        for (const PlayerRestoreRecord& player : save.players)
        {
            if (const SaveLoadOutcome o = sink.Apply(player); o != SaveLoadOutcome::Ok)
            {
                return o;
            }
        }
        return SaveLoadOutcome::Ok;
    }
} // namespace stalkermp::saveload

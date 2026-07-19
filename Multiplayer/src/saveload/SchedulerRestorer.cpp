// STALKER-MP — Scheduler restorer (Sprint-012, Step 13)
//
// See SchedulerRestorer.h. Engine-free / OS-free; no exceptions, no RTTI, no
// iostream; value outcomes.

#include "stalkermp/saveload/SchedulerRestorer.h"

namespace stalkermp::saveload
{
    SaveLoadOutcome SchedulerRestorer::Restore(const LoadedSave& save, ISchedulerRestoreSink& sink)
    {
        return sink.Apply(save.scheduler);
    }
} // namespace stalkermp::saveload

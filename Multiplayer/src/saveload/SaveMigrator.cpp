// STALKER-MP — Save migration framework (Sprint-012, Step 7)
//
// See SaveMigrator.h. Deterministic, engine-free migration pipeline.
// Engine-free / OS-free; no exceptions, no RTTI, no iostream; value outcomes.

#include "stalkermp/saveload/SaveMigrator.h"

namespace stalkermp::saveload
{
    void SaveMigrator::Register(const std::uint32_t from, const std::uint32_t to, const StepFn apply)
    {
        if (apply == nullptr || to <= from)
        {
            return; // reject a null or non-advancing step (deterministic no-op)
        }
        // Replace an existing step for the same `from` (last registration wins,
        // deterministic) rather than allowing ambiguous duplicates.
        for (Step& s : m_steps)
        {
            if (s.from == from)
            {
                s.to = to;
                s.apply = apply;
                return;
            }
        }
        m_steps.push_back(Step{from, to, apply});
    }

    const SaveMigrator::Step* SaveMigrator::FindStep(const std::uint32_t from) const noexcept
    {
        for (const Step& s : m_steps)
        {
            if (s.from == from)
            {
                return &s;
            }
        }
        return nullptr;
    }

    bool SaveMigrator::CanMigrate(const std::uint32_t fromVersion, const std::uint32_t toVersion) const noexcept
    {
        if (fromVersion == toVersion)
        {
            return true;
        }
        if (fromVersion > toVersion)
        {
            return false; // no downgrade
        }
        std::uint32_t v = fromVersion;
        while (v < toVersion)
        {
            const Step* s = FindStep(v);
            if (s == nullptr || s->to <= v)
            {
                return false;
            }
            v = s->to;
        }
        return v == toVersion;
    }

    SaveLoadOutcome SaveMigrator::Migrate(LoadedSave& save, const std::uint32_t fromVersion,
                                          const std::uint32_t toVersion)
    {
        m_appliedSteps = 0;

        if (fromVersion == toVersion)
        {
            return SaveLoadOutcome::Ok; // nothing to migrate
        }
        if (fromVersion > toVersion)
        {
            return SaveLoadOutcome::VersionUnsupported; // downgrade unsupported
        }

        std::uint32_t v = fromVersion;
        while (v < toVersion)
        {
            const Step* s = FindStep(v);
            if (s == nullptr || s->to <= v || s->to > toVersion)
            {
                return SaveLoadOutcome::VersionUnsupported; // missing / non-advancing / overshoot
            }
            if (const SaveLoadOutcome o = s->apply(save); o != SaveLoadOutcome::Ok)
            {
                return o; // a step's own failure is propagated
            }
            v = s->to;
            ++m_appliedSteps;
        }
        return SaveLoadOutcome::Ok;
    }
} // namespace stalkermp::saveload

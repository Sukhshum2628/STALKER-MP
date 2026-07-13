// STALKER-MP — ALife Transition Manager core (Sprint-005, Step 3)
//
// Out-of-line definitions for the engine-free TransitionManager core. This TU
// holds only the read-only surface for Step 3 (StateOf, ValidateConsistency);
// the Update pipeline (Bubble ingestion, dedup, gateway apply, reconciliation)
// is added in later steps and will live here.
//
// ADR-007: engine-free, no exceptions, no iostream. ADR-008: no engine call is
// named here; the sanctioned mutation lives behind the gateway's concrete
// adapter (a later step). One Engine Boundary: this TU includes no engine header.

#include "stalkermp/world/TransitionManager.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "stalkermp/common/StringUtil.h"     // common::Format (ADR-007-safe, no iostream)
#include "stalkermp/world/BubbleManager.h"   // Activations() / Deactivations()
#include "stalkermp/world/EntityRegistry.h"  // Contains()

namespace stalkermp::world
{
    namespace
    {
        // Dedup predicate (Architecture §13): emit an Activate only when the
        // entity is not already Online or heading Online.
        [[nodiscard]] bool NeedsActivate(const TransitionState state) noexcept
        {
            return state != TransitionState::Online && state != TransitionState::PendingOnline;
        }

        // Emit a Deactivate only when the entity is not already Offline or
        // heading Offline.
        [[nodiscard]] bool NeedsDeactivate(const TransitionState state) noexcept
        {
            return state != TransitionState::Offline && state != TransitionState::PendingOffline;
        }
    } // namespace

    void TransitionManager::Reconcile()
    {
        // Reconciliation queries engine truth ONLY through IsOnline(); it issues no
        // Apply, stages nothing, and adds no retry/timeout logic. Process entities
        // in canonical ascending EntityId order (the store is already ascending).
        if (m_gateway == nullptr)
        {
            return; // no engine truth available -> leave all Pending* unchanged
        }

        // Snapshot the ids to reconcile first (ascending). Confirming an entity is
        // an in-place state update (no structural change), but snapshotting keeps
        // the pass obviously safe and order-stable.
        std::vector<EntityId> pending;
        for (const TransitionIntentStore::Entry& entry : m_intent.Entries())
        {
            if (entry.state == TransitionState::PendingOnline ||
                entry.state == TransitionState::PendingOffline)
            {
                pending.push_back(entry.id);
            }
        }

        for (const EntityId id : pending)
        {
            const std::optional<bool> online = m_gateway->IsOnline(id);
            if (!online.has_value())
            {
                continue; // nullopt: unknown to engine -> leave in Pending*
            }

            const TransitionState state = m_intent.Get(id);
            if (state == TransitionState::PendingOnline && *online)
            {
                m_intent.Set(id, TransitionState::Online); // confirmed online
            }
            else if (state == TransitionState::PendingOffline && !*online)
            {
                m_intent.Set(id, TransitionState::Offline); // confirmed offline
            }
            // Mismatch (PendingOnline & false, PendingOffline & true): leave as-is.
        }
    }

    void TransitionManager::Update(const std::uint64_t tick)
    {
        m_lastTick = tick;

        // Step 6: reconcile prior-tick Pending* intents against engine truth before
        // this tick's ingestion. This keeps confirmation one deterministic step per
        // tick and tolerant of asynchronous engine switching.
        Reconcile();

        m_stagedBatch.clear();

        // Missing Bubble dependency => nothing to ingest (deterministic no-op).
        if (m_bubble == nullptr)
        {
            return;
        }

        // Step 1–4: read Bubble transitions, validate against the registry, dedup
        // against intent, and stage commands. The Bubble emits each list ascending
        // by EntityId (B9); we sort the combined batch below for a single canonical
        // ascending order across both kinds.

        // Activations.
        for (const EntityId id : m_bubble->Activations())
        {
            if (m_registry != nullptr && !m_registry->Contains(id))
            {
                continue; // entity no longer registered — drop (benign)
            }
            if (!NeedsActivate(m_intent.Get(id)))
            {
                continue; // dedup: already Online / PendingOnline
            }
            m_stagedBatch.push_back(TransitionCommand{id, TransitionKind::Activate});
            m_intent.Set(id, TransitionState::PendingOnline); // internal bookkeeping only
        }

        // Deactivations.
        for (const EntityId id : m_bubble->Deactivations())
        {
            if (m_registry != nullptr && !m_registry->Contains(id))
            {
                continue;
            }
            if (!NeedsDeactivate(m_intent.Get(id)))
            {
                continue; // dedup: already Offline / PendingOffline
            }
            m_stagedBatch.push_back(TransitionCommand{id, TransitionKind::Deactivate});
            m_intent.Set(id, TransitionState::PendingOffline);
        }

        // One canonical ascending batch (ascending EntityId, then kind).
        std::sort(m_stagedBatch.begin(), m_stagedBatch.end());

        // Step 5: apply the staged batch through the engine-free gateway seam in a
        // single phase, record one outcome per staged command (in staged order),
        // and build the read-only TransitionResult for this tick. NO reconciliation
        // here: intent is left as staged (Pending*); confirmation is Step 6.
        m_lastRecords.clear();
        m_lastResult = TransitionResult{};
        m_lastResult.tick = tick;

        if (m_gateway == nullptr)
        {
            return; // no gateway => nothing applied; empty result at this tick
        }

        const std::vector<TransitionOutcome> outcomes = m_gateway->Apply(m_stagedBatch);

        // Defensive: pair up to the common length (a well-behaved gateway returns
        // exactly one outcome per command, preserving order).
        const std::size_t count = std::min(outcomes.size(), m_stagedBatch.size());
        m_lastRecords.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const TransitionCommand& command = m_stagedBatch[i];
            const TransitionOutcome outcome = outcomes[i];
            m_lastRecords.push_back(TransitionRecord{command.id, command.kind, outcome});

            // Only a switch actually performed this tick is a delta. AlreadyInState
            // (idempotent no-op), EntityMissing, and Failed are not deltas and are
            // handled (reconciled/retried) in Step 6.
            if (outcome == TransitionOutcome::Applied)
            {
                if (command.kind == TransitionKind::Activate)
                {
                    m_lastResult.broughtOnline.push_back(command.id); // ascending (batch order)
                }
                else
                {
                    m_lastResult.broughtOffline.push_back(command.id); // ascending (batch order)
                }
            }
        }
    }

    TransitionState TransitionManager::StateOf(const EntityId id) const
    {
        return m_intent.Get(id);
    }

    namespace
    {
        [[nodiscard]] bool IsLegalState(const TransitionState state) noexcept
        {
            switch (state)
            {
            case TransitionState::Offline:
            case TransitionState::PendingOnline:
            case TransitionState::Online:
            case TransitionState::PendingOffline:
                return true;
            }
            return false;
        }

        [[nodiscard]] bool AscendingUnique(const std::vector<EntityId>& ids) noexcept
        {
            for (std::size_t i = 1; i < ids.size(); ++i)
            {
                if (!(ids[i - 1].value < ids[i].value))
                {
                    return false;
                }
            }
            return true;
        }
    } // namespace

    TransitionStatistics TransitionManager::Statistics() const
    {
        TransitionStatistics stats;

        for (const TransitionIntentStore::Entry& entry : m_intent.Entries())
        {
            switch (entry.state)
            {
            case TransitionState::Online:         ++stats.online; break;
            case TransitionState::PendingOnline:  ++stats.pendingOnline; break;
            case TransitionState::PendingOffline: ++stats.pendingOffline; break;
            case TransitionState::Offline:        ++stats.offlineTracked; break;
            }
        }

        for (const TransitionRecord& record : m_lastRecords)
        {
            switch (record.outcome)
            {
            case TransitionOutcome::Applied:        ++stats.appliedThisTick; break;
            case TransitionOutcome::AlreadyInState:
            case TransitionOutcome::EntityMissing:  ++stats.skippedThisTick; break;
            case TransitionOutcome::Failed:         ++stats.failedThisTick; break;
            }
        }

        return stats;
    }

    std::string TransitionManager::DescribeState() const
    {
        const TransitionStatistics stats = Statistics();
        return common::Format(
            "Transition tick={} online={} pendingOnline={} pendingOffline={} offline={} "
            "staged={} applied={} skipped={} failed={}",
            m_lastTick, stats.online, stats.pendingOnline, stats.pendingOffline,
            stats.offlineTracked, m_stagedBatch.size(),
            stats.appliedThisTick, stats.skippedThisTick, stats.failedThisTick);
    }

    std::string TransitionManager::DumpPending() const
    {
        std::string out = "Pending:";
        for (const TransitionIntentStore::Entry& entry : m_intent.Entries()) // ascending
        {
            if (entry.state == TransitionState::PendingOnline ||
                entry.state == TransitionState::PendingOffline)
            {
                out += ' ';
                out += std::to_string(entry.id.value);
                out += '=';
                out += TransitionStateName(entry.state);
            }
        }
        return out;
    }

    TransitionConsistencyReport TransitionManager::ValidateConsistency() const
    {
        TransitionConsistencyReport report;

        // 1) Intent store: ordering + accelerator (Step 3 invariant).
        report.store = m_intent.ValidateConsistency();

        // 2) State legality: every tracked intent is a valid enum value.
        for (const TransitionIntentStore::Entry& entry : m_intent.Entries())
        {
            if (!IsLegalState(entry.state))
            {
                report.statesValid = false;
                break;
            }
        }

        // 3) Staged batch: strictly ascending, unique ids (one command per entity).
        for (std::size_t i = 1; i < m_stagedBatch.size(); ++i)
        {
            if (!(m_stagedBatch[i - 1] < m_stagedBatch[i]) ||
                m_stagedBatch[i - 1].id.value == m_stagedBatch[i].id.value)
            {
                report.stagedBatchOrdered = false;
                break;
            }
        }

        // 4) Staged/intent consistency: a staged Activate implies PendingOnline
        //    intent; a staged Deactivate implies PendingOffline (the state Step 4
        //    set when staging, undisturbed by this tick's start-of-tick reconcile).
        for (const TransitionCommand& command : m_stagedBatch)
        {
            const TransitionState state = m_intent.Get(command.id);
            const bool ok =
                (command.kind == TransitionKind::Activate && state == TransitionState::PendingOnline) ||
                (command.kind == TransitionKind::Deactivate && state == TransitionState::PendingOffline);
            if (!ok)
            {
                report.stagedIntentConsistent = false;
                break;
            }
        }

        // 5) Records/batch consistency (gateway outcome consistency where
        //    applicable): records are either empty (no gateway applied) or exactly
        //    parallel to the staged batch in id and kind.
        if (!m_lastRecords.empty())
        {
            if (m_lastRecords.size() != m_stagedBatch.size())
            {
                report.recordsMatchBatch = false;
            }
            else
            {
                for (std::size_t i = 0; i < m_lastRecords.size(); ++i)
                {
                    if (m_lastRecords[i].id.value != m_stagedBatch[i].id.value ||
                        m_lastRecords[i].kind != m_stagedBatch[i].kind)
                    {
                        report.recordsMatchBatch = false;
                        break;
                    }
                }
            }
        }

        // 6) Result consistency: delta lists ascending & unique, tick matches the
        //    last Update, and every listed id is backed by an Applied record of the
        //    matching kind.
        if (!AscendingUnique(m_lastResult.broughtOnline) ||
            !AscendingUnique(m_lastResult.broughtOffline) ||
            m_lastResult.tick != m_lastTick)
        {
            report.resultConsistent = false;
        }
        else
        {
            for (const EntityId id : m_lastResult.broughtOnline)
            {
                if (!BackedByAppliedRecord(id, TransitionKind::Activate))
                {
                    report.resultConsistent = false;
                    break;
                }
            }
            if (report.resultConsistent)
            {
                for (const EntityId id : m_lastResult.broughtOffline)
                {
                    if (!BackedByAppliedRecord(id, TransitionKind::Deactivate))
                    {
                        report.resultConsistent = false;
                        break;
                    }
                }
            }
        }

        return report;
    }

    bool TransitionManager::BackedByAppliedRecord(const EntityId id, const TransitionKind kind) const
    {
        for (const TransitionRecord& record : m_lastRecords)
        {
            if (record.id.value == id.value && record.kind == kind &&
                record.outcome == TransitionOutcome::Applied)
            {
                return true;
            }
        }
        return false;
    }
} // namespace stalkermp::world

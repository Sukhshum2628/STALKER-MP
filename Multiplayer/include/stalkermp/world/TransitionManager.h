// STALKER-MP — ALife Transition Manager core (Sprint-005, Architecture §6/§8/§11)
//
// The engine-free core of the ALife Transition Layer. It owns the per-entity
// transition INTENT (via TransitionIntentStore) and, in later steps, will consume
// the Bubble's per-tick transitions, drive the vanilla switch through the
// IAlifeSwitchGateway seam, and reconcile intent against engine truth.
//
// THIS STEP (Step 3) establishes ONLY: construction/injection, the intent store,
// and the read-only surface (StateOf, TrackedCount, LastResult, ValidateConsistency).
// There is NO Update pipeline yet — no Bubble reads, no dedup, no gateway apply,
// no confirmation. Those are Steps 4–6.
//
// Ownership (Architecture §11): the manager owns only intent; the ENGINE owns the
// real online/offline status. Injected dependencies are NON-OWNING and read-only
// for Bubble/registry; the gateway is the sanctioned mutation seam (used later).
//
// ADR-007: engine-free, no exceptions, no throwing STL surface. ADR-008: this
// core names no engine call; the Cooperative Flag Override lives behind the
// gateway's concrete adapter (a later step).

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "stalkermp/world/EntityView.h"           // world::EntityId
#include "stalkermp/world/IAlifeSwitchGateway.h"   // world::IAlifeSwitchGateway
#include "stalkermp/world/TransitionIntentStore.h" // world::TransitionIntentStore
#include "stalkermp/world/TransitionTypes.h"       // world::TransitionState, world::TransitionCommand, world::TransitionResult

namespace stalkermp::world
{
    class BubbleManager;   // Sprint-004 — read-only transition source (injected)
    class EntityRegistry;  // Sprint-003 — read-only liveness/lookup (injected)

    // Read-only consistency scan of the manager (Step 7). Reports, never repairs.
    struct TransitionConsistencyReport
    {
        TransitionStoreReport store{};       // intent store ordering/accelerator health
        bool statesValid = true;             // every tracked intent is a legal enum value
        bool stagedBatchOrdered = true;      // staged batch strictly ascending, unique ids
        bool stagedIntentConsistent = true;  // each staged command matches its Pending* intent
        bool recordsMatchBatch = true;       // records parallel the staged batch (or none)
        bool resultConsistent = true;        // result ascending/unique, tick matches, backed by Applied records

        [[nodiscard]] bool IsHealthy() const noexcept
        {
            return store.IsHealthy() && statesValid && stagedBatchOrdered &&
                   stagedIntentConsistent && recordsMatchBatch && resultConsistent;
        }
    };

    class TransitionManager
    {
    public:
        // bubble:   Sprint-004 Bubble Manager — read-only transition source.
        // registry: Sprint-003 Entity Registry — read-only liveness/lookup.
        // gateway:  the engine-free switch seam (sanctioned mutation, used later).
        // All non-owning; must outlive the manager. Defaulted to nullptr so early
        // construction stays valid; the (later) Update pipeline treats missing
        // dependencies as "nothing to do".
        explicit TransitionManager(const BubbleManager* bubble = nullptr,
                                   const EntityRegistry* registry = nullptr,
                                   IAlifeSwitchGateway* gateway = nullptr) noexcept
            : m_bubble(bubble)
            , m_registry(registry)
            , m_gateway(gateway)
        {
        }

        // --- Pipeline (Architecture §7 steps 1–6; Steps 4–5) ----------------
        // One tick of the transition pipeline:
        //   Step 4 (ingestion + dedup): read the Bubble's Activations() /
        //     Deactivations(), drop entities no longer registered, dedup against
        //     intent, stage ONE ordered batch (ascending EntityId), and mark the
        //     corresponding Pending* intent (internal bookkeeping only).
        //   Step 5 (apply): apply the staged batch through the engine-free
        //     IAlifeSwitchGateway seam in one phase, record one TransitionOutcome
        //     per staged command (in staged order), and build the read-only
        //     TransitionResult for this tick.
        //
        // This method does NOT confirm transitions, does NOT query IsOnline(), and
        // does NOT advance Pending* to final states — reconciliation is Step 6.
        void Update(std::uint64_t tick);

        // The batch staged by the last Update, ascending by EntityId. Read-only.
        [[nodiscard]] const std::vector<TransitionCommand>& StagedBatch() const noexcept
        {
            return m_stagedBatch;
        }

        // Per-command outcomes recorded by the last apply, parallel to the staged
        // batch order (records[i] corresponds to StagedBatch()[i]). Read-only.
        [[nodiscard]] const std::vector<TransitionRecord>& LastRecords() const noexcept
        {
            return m_lastRecords;
        }

        // The tick of the last Update (0 before the first Update).
        [[nodiscard]] std::uint64_t LastTick() const noexcept { return m_lastTick; }

        // --- Read-only surface (Architecture §9) ------------------------------

        // Tracked intent for an entity; Offline if untracked.
        [[nodiscard]] TransitionState StateOf(EntityId id) const;

        // Number of entities with tracked (non-Offline-implicit) intent.
        [[nodiscard]] std::size_t TrackedCount() const noexcept { return m_intent.Count(); }

        // The last tick's read-only result. Empty (tick 0) until Update runs
        // (a later step).
        [[nodiscard]] const TransitionResult& LastResult() const noexcept { return m_lastResult; }

        // --- Diagnostics (Architecture §9; Step 7). All read-only, deterministic,
        //     allocation-light, iostream-free (common::Format + C stdio). None of
        //     these mutate state, call Apply()/IsOnline(), reconcile, or stage.

        // Snapshot of transition bookkeeping: intent-state tallies plus this tick's
        // applied / skipped (AlreadyInState + EntityMissing) / failed outcomes.
        [[nodiscard]] TransitionStatistics Statistics() const;

        // One-line human-readable summary of the current state (for logs/tools).
        [[nodiscard]] std::string DescribeState() const;

        // Ascending list of entities currently in a Pending* state, with their state.
        [[nodiscard]] std::string DumpPending() const;

        // Read-only consistency scan (Architecture §16). Healthy on an empty
        // manager and whenever every Step-7 invariant holds.
        [[nodiscard]] TransitionConsistencyReport ValidateConsistency() const;

        // --- Dependency accessors (read-only; for later-step wiring/tests) -----
        [[nodiscard]] const BubbleManager* Bubble() const noexcept { return m_bubble; }
        [[nodiscard]] const EntityRegistry* Registry() const noexcept { return m_registry; }
        [[nodiscard]] const IAlifeSwitchGateway* Gateway() const noexcept { return m_gateway; }

    private:
        // Step 6: reconcile prior-tick Pending* intents against engine truth using
        // only IsOnline() read-back, in canonical ascending EntityId order.
        // PendingOnline -> Online when IsOnline()==true; PendingOffline -> Offline
        // when IsOnline()==false; false/true mismatch and nullopt leave the entity
        // in its existing Pending* state. Issues no Apply, stages nothing, and
        // adds no retry/timeout logic.
        void Reconcile();

        // Diagnostics helper (Step 7): true if this tick's records contain an
        // Applied outcome for (id, kind). Read-only; mutates nothing.
        [[nodiscard]] bool BackedByAppliedRecord(EntityId id, TransitionKind kind) const;

        const BubbleManager* m_bubble = nullptr;   // non-owning (B3-equivalent)
        const EntityRegistry* m_registry = nullptr; // non-owning
        IAlifeSwitchGateway* m_gateway = nullptr;   // non-owning; sanctioned seam

        TransitionIntentStore m_intent;             // owned intent state (canonical)
        std::vector<TransitionCommand> m_stagedBatch; // last tick's ordered batch (ascending)
        std::vector<TransitionRecord> m_lastRecords; // per-command outcomes (staged order)
        std::uint64_t m_lastTick = 0;               // tick of the last Update
        TransitionResult m_lastResult;              // last tick's applied delta (Step 5)
    };
} // namespace stalkermp::world

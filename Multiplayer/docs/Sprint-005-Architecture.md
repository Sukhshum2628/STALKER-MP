# Sprint-005 — ALife Transition Layer — Architecture (FROZEN)

- **Status:** **FROZEN** — architecture frozen 2026-07-09. All blocking evidence gates
  resolved (E-G1 PASSED, E-G2 CLOSED); ADR-008 accepted. Ready to enter implementation.
- **Date:** 2026-07-06 (proposed); **2026-07-09 (frozen)**.
- **Subordinate to:** ADR-007 (Engine ABI Contract), **ADR-008 (Engine State Mutation
  Boundary — accepted, §19)**, the architecture documents (`04_World.md`, `05_ALife.md`,
  `06_Multiplayer.md`), and RFC-0001/0002/0003.
- **Author role:** Principal Architect.
- **Production switch mechanism (frozen):** **Cooperative Flag Override** via
  `CALifeUpdateManager::set_switch_online(id, bool)` / `set_switch_offline(id, bool)`.

---

## 1. Sprint overview

Sprint-005 introduces the **ALife Transition Layer**: the subsystem that consumes the
Bubble Manager's per-tick activation/deactivation transitions (Sprint-004) and drives the
engine's **existing, vanilla** ALife online/offline switching so that each decided entity
moves between offline (strategic) and online (full) simulation — without duplicating
simulation, without modifying ALife logic, and preserving one authoritative world.

This is the **first behavioral sprint** and, more importantly, the first sprint that
**writes** engine simulation state. Sprints 1–4 only observed the engine (frame bridge,
entity feed reconciliation, position reads). Sprint-005 crosses a new boundary — mutating
engine ALife state — which is why it warrants its own ADR (§19).

---

## 2. Objectives

- Consume Bubble transitions (`Activations()` / `Deactivations()`) each frame.
- Drive the engine's vanilla ALife switch machinery to bring decided entities online/offline.
- Guarantee exactly one simulation of an entity at any time (no duplicate activation).
- Preserve vanilla ALife behavior — reuse engine switching; never reimplement or edit it.
- Keep all engine access confined to `EngineAdapters.cpp` (One Engine Boundary).
- Produce a deterministic, idempotent, replay-safe transition result that later sprints
  (replication, persistence) can consume.

---

## 3. Responsibilities

The Transition Layer:

- **Owns** the module's per-entity *simulation-intent* state and the in-flight transition
  bookkeeping (the "we have asked the engine to bring X online" record).
- **Consumes** Bubble transition lists (input) and the Entity Registry (validation of
  identity/liveness), both read-only.
- **Produces** ordered, deduplicated transition *commands* to the engine and a read-only
  per-tick *transition result* (what actually changed) for downstream sprints.
- **Delegates** the actual online/offline switch to the engine's own ALife functions via a
  single engine-facing gateway.

It does **not** own the actual engine object online/offline status (the engine owns that),
entity identity (the registry owns that), or the activation decision (the Bubble owns that).

---

## 4. Non-goals

No networking, replication, snapshots, save/load, threading changes, prediction, or
interpolation (all later sprints). **No ALife logic** — no AI, pathfinding, scheduling,
smart-terrain, or squad behavior. No engine-source edits. No new `seqFrame` object. No
gameplay. The layer decides *when* to switch and asks the engine to do it; it never decides
*how* an online entity behaves.

---

## 5. High-level architecture

```
Bubble Manager (Sprint-004)  ──Activations()/Deactivations() (read-only)──┐
Entity Registry (Sprint-003) ──identity/liveness (read-only)──────────────┤
                                                                          ▼
                                        world::TransitionManager  (engine-free)
                                          - intent state machine per EntityId
                                          - dedup / idempotency / ordering
                                          - deferred, batched command queue
                                          - per-tick transition result (read-only out)
                                                     │  IAlifeSwitchGateway (engine-free seam)
                                                     ▼
                                    adapters::EngineAlifeSwitchGateway  (EngineAdapters.cpp ONLY)
                                          - resolve EntityId -> ALife object (on demand, no cache)
                                          - set vanilla override flags via set_switch_online/offline(id,bool)
                                                     │
                                                     ▼
                                    Engine CALifeSwitchManager / CALifeUpdateManager
                                          (vanilla ALife switching — unchanged)
```

The `TransitionManager` and the `IAlifeSwitchGateway` interface are engine-free (world
subsystem). Only the concrete `EngineAlifeSwitchGateway` touches engine headers, and it
lives in `EngineAdapters.cpp` (with a `NullAlifeSwitchGateway` for the test build).

---

## 6. Component diagram (text)

- `world::TransitionManager` — core, engine-free. Owns intent state + queue.
- `world::TransitionState` (enum), `world::TransitionCommand`, `world::TransitionRecord`,
  `world::TransitionResult`, `world::TransitionStatistics` — value types.
- `world::IAlifeSwitchGateway` — engine-free seam: apply a batch of switch commands, report
  per-command outcome; query current engine online state (read-only).
- `world::TransitionManagerService` — `core::IService` + `core::ITickable`; owns the manager;
  composition-root lifecycle; ticked at `tick_order::kAlifeTransition`.
- `adapters::EngineAlifeSwitchGateway` — concrete gateway (EngineAdapters.cpp).
- `adapters::NullAlifeSwitchGateway` — inert test-build counterpart (NullEngineAdapters.cpp).
- Factory `adapters::CreateEngineAlifeSwitchGateway()` (mirrors the entity-feed factory).

---

## 7. Runtime sequence (per frame)

```
FrameBridge → FrameDispatcher.Dispatch(dt)
  kWorld (100)            WorldManager                 (advance clock / context)
  kEntityRegistry (200)   EngineEntityFeed             (reconcile registry from engine)
  kBubble (300)           BubbleManagerService         (compute activation + transitions)
  kAlifeTransition (350)  TransitionManagerService     (THIS SPRINT)
        1. read Bubble Activations()/Deactivations() (this tick)
        2. validate each against the Entity Registry (still registered? live?)
        3. reconcile against intent state; drop no-ops (idempotency)
        4. enqueue ordered commands (ascending EntityId)
        5. apply the batch via IAlifeSwitchGateway (single deterministic phase)
        6. record outcomes -> intent state + TransitionResult (read-only out)
  kReplication (400)      (future) consumes TransitionResult
  kPersistence (500)      (future)
```

`kAlifeTransition = 350` is a new, additive `tick_order` constant between Bubble (300) and
Replication (400) — see §14. Runs strictly after the Bubble has produced this tick's
transitions.

---

## 8. Class responsibilities

- **TransitionManager** — pure logic. Given this tick's Bubble transitions and a registry
  view, compute the ordered set of engine switch commands, apply them through the gateway,
  update intent state, and expose the tick's result and statistics. Sole writer of intent
  state. Engine-free.
- **TransitionManagerService** — IService + ITickable wrapper; owns the manager; injects the
  Bubble, registry, and gateway; ticks the pipeline; composition-root ownership.
- **IAlifeSwitchGateway** — abstract boundary. `Apply(const std::vector<TransitionCommand>&)
  -> per-command outcomes`; `IsOnline(EntityId) const` (read-only engine query for
  reconciliation). Engine-free interface.
- **EngineAlifeSwitchGateway** — maps `EntityId` → ALife object id and invokes the vanilla
  engine switch; reports outcome; never edits ALife. EngineAdapters.cpp only.

---

## 9. Interfaces (shapes, not code)

- `IAlifeSwitchGateway`
  - `Apply(commands) -> std::vector<TransitionOutcome>` (parallel to input; per-command
    Applied / AlreadyInState / EntityMissing / Failed).
  - `IsOnline(EntityId) const -> std::optional<bool>` (nullopt if the entity is unknown to
    the engine).
- `TransitionManager` (public surface)
  - `Update(currentTick)` — the pipeline (§7 steps 1–6).
  - Read-only: `StateOf(EntityId) -> TransitionState`; `LastResult() -> const TransitionResult&`;
    `Statistics() -> TransitionStatistics`; diagnostics (`DescribeState`, `DumpPending`,
    `ValidateConsistency`) mirroring Sprint-004.
- All fallible operations return `core::Expected<T>`; no exceptions (ADR-007).

The Transition Layer **consumes**: Bubble `Activations()`/`Deactivations()` (by `EntityId`),
registry liveness (`Contains`/`FindByEntityId`), and gateway online reads. It **produces**:
engine switch commands (to the gateway) and a read-only `TransitionResult` per tick.

---

## 10. Data structures (value types)

- `enum class TransitionState { Offline, PendingOnline, Online, PendingOffline }`.
- `enum class TransitionKind { Activate, Deactivate }`.
- `struct TransitionCommand { EntityId id; TransitionKind kind; }` (ordered ascending EntityId).
- `enum class TransitionOutcome { Applied, AlreadyInState, EntityMissing, Failed }`.
- `struct TransitionRecord { EntityId id; TransitionKind kind; TransitionOutcome outcome; }`.
- `struct TransitionResult { std::vector<EntityId> broughtOnline; std::vector<EntityId> broughtOffline; std::uint64_t tick; }` (ascending).
- `struct TransitionStatistics { std::size_t online, pendingOnline, pendingOffline, offlineTracked, appliedThisTick, skippedThisTick, failedThisTick; }`.

Intent state is stored as a deterministic, sorted structure keyed by `EntityId` (sorted
vector + secondary hash accelerator, exactly like the registry — canonical ascending order,
BC-005-approved containers).

---

## 11. Ownership rules

| Data | Owner |
|---|---|
| Activation decision (which entities should be online) | Bubble Manager (Sprint-004) |
| Entity identity / registration | Entity Registry (Sprint-003) |
| **Per-entity transition intent + in-flight bookkeeping** | **TransitionManager (Sprint-005)** |
| Actual object online/offline status | **Engine ALife (vanilla, unchanged)** |
| World tick / context | WorldManager (Sprint-002) |
| Engine access to perform the switch | `EngineAdapters.cpp` gateway only |

The single most important boundary: **the engine owns the real online/offline status; the
Transition Layer owns only the *intent* and drives the vanilla switch.** This preserves
Preserve Before Replace and Host Authority.

---

## 12. State machine (per entity)

```
Offline ──Bubble activate & set_switch_online(id,true)──► PendingOnline ──confirmed online──► Online
   ▲                                                                                              │
   │                                                                                              │
   └────── confirmed offline ◄── PendingOffline ◄── Bubble deactivate & set_switch_offline(id,true) ┘
```

- `Offline → PendingOnline`: Bubble activation received, command applied to the gateway.
- `PendingOnline → Online`: gateway/engine confirms online (outcome `Applied` or
  `AlreadyInState`, or `IsOnline` read-back true).
- `Online → PendingOffline`: Bubble deactivation received, command applied.
- `PendingOffline → Offline`: confirmed offline.
- Illegal/duplicate transitions (e.g. activate while already Online/PendingOnline) are
  **no-ops** (idempotency, §13). `EntityMissing` (entity left the registry mid-flight)
  drops the entry to `Offline`/untracked.

Transitions are **single-step per tick** and confirmed by the next tick's reconciliation —
this keeps the pipeline deterministic and tolerant of asynchronous engine switching.

---

## 13. Transition flow — timing, dedup, idempotency, replay

- **Timing: deferred + batched + queued.** Transitions are **not** applied the instant the
  Bubble decides them. Each `kAlifeTransition` tick, the manager reads the Bubble's
  transitions, builds one ordered batch, and applies it in a **single deterministic phase**
  (ascending `EntityId`) — mirroring the registry's C1 and the Bubble's evaluation. No
  mid-frame engine mutation from callbacks.
- **Duplicate prevention:** the intent-state map is consulted before enqueueing. A command
  is emitted only when the desired state differs from the tracked state (activate only if
  not Online/PendingOnline; deactivate only if not Offline/PendingOffline). Bubble may
  re-report membership across ticks; only genuine edges produce commands.
- **Idempotency:** the gateway treats a switch to the current engine state as
  `AlreadyInState` (a no-op). Applying the same command twice yields the same engine state.
- **Replay safety:** re-running `Update` with the same Bubble transitions and the same
  engine state produces **no additional** switches (all resolve to `AlreadyInState`/no-op)
  and an empty `TransitionResult`. Deterministic ordering makes the result reproducible.

---

## 14. Composition-root integration

- `TransitionManagerService` (IService + ITickable) is created in `Bootstrap.cpp` after the
  Bubble service, owned by the `ServiceRegistry`. `Dependencies()` = `{ "World",
  "EntityRegistry", "BubbleManager" }`.
- Injected read-only references: the `BubbleManager` (for transitions), the `EntityRegistry`
  (validation), and an `IAlifeSwitchGateway` (created via
  `adapters::CreateEngineAlifeSwitchGateway()`, owned by `Runtime`, null in the test build).
- Subscribed to the single existing `FrameDispatcher` at **`tick_order::kAlifeTransition =
  350`** — the one minimal, additive change to a frozen file (`FrameDispatcher.h`). It adds a
  constant to the `tick_order` namespace, which already reserves future-sprint slots
  (`kReplication`, `kPersistence`, `kPlugins`); this is the intended extension mechanism, not
  a behavioral change. No new `seqFrame`.
- Teardown mirrors the Bubble/feed rule: `frameBridge.reset()` → unsubscribe the transition
  service → unsubscribe Bubble/others → `services.ShutdownAll()`. The gateway (Runtime-owned)
  outlives operational use; no dtor dereferences an injected reference.

---

## 15. Engine boundary analysis

- **Only** `adapters::EngineAlifeSwitchGateway` (in `EngineAdapters.cpp`) includes engine
  headers and calls engine functions. `TransitionManager`, `TransitionManagerService`, the
  interface, and all value types are engine-free.
- The gateway resolves `EntityId.value` (== ALife object id, `ALife::_OBJECT_ID`, the `u16`
  from `object->ID()` already used by the Sprint-003 feed) to the ALife object **on demand,
  via the engine's own authoritative lookup, and retains no engine pointer or id→object cache
  across calls** (see E-G2 in §21) — the engine stays the single owner of object existence and
  lifetime — then drives the **vanilla** switch via the **Cooperative Flag Override** pattern
  (frozen by E-G1): `CALifeUpdateManager::set_switch_online(id, bool)` /
  `set_switch_offline(id, bool)`, which set the object's `can_switch_online` / `can_switch_offline`
  override flags that the vanilla `try_switch_*` path already evaluates. The gateway therefore
  **overrides the predicates the vanilla ALife update loop consumes** rather than racing it;
  the engine's own scheduler performs the actual state change. Direct
  `switch_online/switch_offline` (flip-flop against the single-actor distance gate),
  `try_switch_online/try_switch_offline` (single-actor distance gate cannot serve remote
  multiplayer bubbles), and configuration-only band-widening (single-actor-centred, cannot
  serve remote players) were evaluated and **rejected** by E-G1.
- **Preserve Before Replace:** no ALife source is edited; the module drives existing
  behavior. **One Engine Boundary** and **ADR-007** hold: the gateway TU compiles under the
  engine ABI (exceptions off) and returns outcomes as values, never throwing.

---

## 16. Determinism analysis

- Input is deterministic: the Bubble produces ascending-`EntityId` transition lists (B9).
- The manager evaluates and applies in **ascending `EntityId`** order, in one phase per tick.
- Intent state is a canonically ordered structure; `TransitionResult` is ascending.
- No engine callback mutates intent state; all mutation is in `Update` on the host thread.
- Confirmation is reconciled deterministically next tick.
- Therefore: same Bubble transitions + same engine state + same registry ⇒ identical commands,
  identical result, identical intent state (B1-equivalent).
- **Resolved (E-G1):** contention with vanilla distance-based switching is eliminated by
  construction — Cooperative Flag Override sets the override flags the vanilla loop reads, so
  the two authorities cooperate rather than fight (flip-flop measured at zero). The module's
  determinism is over its own decisions and command ordering, with read-back reconciliation
  absorbing engine-side timing.

---

## 17. Failure modes

| Failure | Handling |
|---|---|
| `EntityId` not registered at apply time (entity left) | Skip; drop intent to untracked. Benign. |
| `EntityId` maps to no live engine object | Gateway reports `EntityMissing`; skip; no throw. |
| Engine switch reports failure | `Failed`; keep prior intent; retry next tick; log via `core::Log`. |
| Entity destroyed mid-transition (relcase during frame) | Registry unregisters it; next tick validation drops it. |
| Gateway unavailable (null / no level) | Commands no-op; intent stays; guarded like the feed's `g_pGameLevel == nullptr`. |
| Bubble and engine disagree on online state | Reconciliation via `IsOnline` read-back each tick converges intent to engine truth. |

No exceptions anywhere; every fallible step returns `core::Expected<T>` or a value outcome.

---

## 18. Risk analysis

1. **Vanilla distance-based switching contends with Bubble-driven switching — RESOLVED (E-G1).**
   The engine's `CALifeUpdateManager` normally switches objects online/offline by distance to
   the single actor. In MP the Bubble (multi-player union) is the authority. E-G1 proved that
   direct `switch_*` flip-flops against the single-actor distance gate, and `try_switch_*`
   cannot serve remote bubbles at all. **Frozen resolution:** the gateway uses
   `set_switch_online/offline(id, bool)` (Cooperative Flag Override), setting the
   `can_switch_*` override flags that the vanilla `try_switch_*` path already honours. The
   vanilla loop then performs the switch in agreement with the Bubble decision — cooperation,
   not contention; flip-flop measured at zero. No ALife source or configuration change required.
2. **EntityId ↔ ALife object mapping (E-G2).** Confirm `EntityId.value` is the ALife
   `_OBJECT_ID` for all persistent entities and that a live `CSE_ALifeDynamicObject` (or the
   flag API) is reachable from `EngineAdapters.cpp` for arbitrary ids. **Mitigation is
   on-demand resolution via the engine's authoritative lookup with no retained pointers — a
   persistent gateway-owned id→object cache is rejected** (it would be a second lifetime
   tracker outside the engine, dangling on engine destroy and violating C2 / One Engine
   Boundary / Preserve Before Replace). See §21 E-G2.
3. **Asynchronous switch timing.** Engine switching may complete over multiple frames; the
   PendingOnline/PendingOffline states + read-back reconciliation absorb this.
4. **Oscillation.** Already damped upstream by the Bubble's hysteresis (B10); the intent
   machine adds a second guard (single-step + confirmation).
5. **Save/load interaction.** The engine persists ALife online/offline itself; the module's
   intent state is transient and reconstructed — no conflict (see §20).

---

## 19. ADRs introduced

**ADR-008 — Engine State Mutation Boundary — ACCEPTED (2026-07-09).** Sprints 1–4 only read
engine state; Sprint-005 is the first to write it. ADR-008 establishes the normative invariant
for the read→write boundary, under the ADR-007 registry.

**Context.** The Transition Layer must drive vanilla ALife online/offline state for
host-decided entities across a multi-player Bubble, without editing or reimplementing ALife
and without a second authority fighting the engine's own switching.

**Decision.** Engine simulation state may be mutated **only** through the **Cooperative Flag
Override** pattern, subject to these binding rules:

1. **Sanctioned vanilla entry point only.** Mutation is performed exclusively via
   `CALifeUpdateManager::set_switch_online(id, bool)` / `set_switch_offline(id, bool)`, which
   set the object's `can_switch_online` / `can_switch_offline` override flags. The module
   **sets the predicates the vanilla ALife update loop already consumes** and lets the engine's
   own scheduler perform the switch. The module never calls the forced
   `switch_online/switch_offline` and never edits or reimplements ALife (Preserve Before Replace).
2. **Single gateway, one boundary.** All such mutation is confined to one gateway
   (`adapters::EngineAlifeSwitchGateway`) in `EngineAdapters.cpp`. No other TU includes engine
   headers or writes engine state (One Engine Boundary).
3. **Stateless, on-demand resolution.** The gateway resolves `EntityId.value == ALife::_OBJECT_ID`
   to the live object through the engine's authoritative lookup at the instant of the call,
   retains no engine pointer and no id→object cache, and remains the engine's dependent — never
   a second owner of object lifetime (inherits E-G2).
4. **Engine-free, exception-free interface.** The gateway is reached through the engine-free
   `IAlifeSwitchGateway` seam and returns `core::Expected<T>` / value outcomes; it never throws
   (ADR-007). `NullAlifeSwitchGateway` preserves the engine-free test build.
5. **No presumed synchrony.** The module never assumes a mutation completes in the same frame;
   PendingOnline/PendingOffline plus read-back reconciliation converge intent to engine truth
   (reconcile, don't presume).

**Consequences.** Host authority and determinism are preserved (the host is the sole flag
setter; decisions are ordered by ascending `EntityId`); vanilla single-player behaviour is
unchanged when only the local actor is present; contention/flip-flop is eliminated by
construction rather than by tuning; and the single gateway becomes the one seam for any future
host-authority transfer. This is a genuinely new invariant and is binding on all later sprints
that write engine state.

---

## 20. Future-sprint dependencies

- **Sprint-006/007 (networking/players):** the Bubble already abstracts players via
  `IPlayerPositionSource`; the Transition Layer consumes Bubble output, so networking slots
  in beneath the Bubble with no change here. Host authority (RFC-0001) is preserved — the
  host is the sole switcher.
- **Sprint-008 (replication):** `TransitionResult` (per-tick online/offline by `EntityId`) is
  exactly the activation delta replication needs to tell clients which entities are active;
  it is produced read-only and ordered for that purpose.
- **Sprint-011/012 (persistence):** intent state is **transient and reconstructable** from
  Bubble + registry + engine online reads; it is **not** persisted. Engine ALife state is
  saved by the engine's own save system (Preserve Before Replace). No new persistence format.
- **Future ALife ownership/authority:** the single gateway is the one place engine ALife is
  driven — the natural seam for future host-authority transfer or ownership arbitration.

---

## 21. Evidence gates (resolve before / during implementation)

- **E-G1 — vanilla switch entry point & contention — RESOLVED / PASSED (2026-07-09).** The
  spike selected the production switch mechanism: **Cooperative Flag Override** via
  `CALifeUpdateManager::set_switch_online(id, bool)` / `set_switch_offline(id, bool)`. Direct
  `switch_online/switch_offline` was rejected (flip-flop against the single-actor distance
  gate); `try_switch_online/try_switch_offline` was rejected as the production path (its
  single-actor distance gate cannot serve remote multiplayer bubbles); configuration-only
  approaches were rejected (single-actor-centred, cannot serve remote players). Cooperative
  Flag Override sets the `can_switch_*` override flags the vanilla `try_switch_*` path already
  evaluates, so Bubble-driven switching cooperates with the vanilla loop instead of contending
  — flip-flop measured at zero — while preserving Preserve Before Replace, Host Authority, One
  Engine Boundary, deterministic simulation, and ADR-007. The gateway stays stateless with
  on-demand `EntityId` resolution per E-G2. Frozen into §15 and ADR-008 (§19).
- **E-G2 — EntityId ↔ ALife object reachability — RESOLVED (verified, 2026-07-06).** The
  architectural decision is closed: the gateway resolves `EntityId.value == ALife::_OBJECT_ID`
  to a live object **on demand at apply time** through the engine's own authoritative lookup
  (ALife simulator object map / server id→entity), retains no pointer and no cache, and
  prefers the id-based flag API where it avoids pointer handling. Verification confirmed the
  gateway is stateless w.r.t. engine object lifetime; no regression in Sprint-003 registry or
  Sprint-004 Bubble ownership; and Determinism, Host Authority, Preserve Before Replace,
  ADR-007, and One Engine Boundary are preserved. The rejected-cache rationale is retained
  below as the frozen decision record. Remaining for implementation (not architectural):
  confirm the concrete engine lookup/flag call — this folds into the E-G1 spike (same gateway,
  same engine session) and does not gate the architecture. **The gateway must NOT maintain
  its own id→object cache and must NOT retain engine object pointers across calls** — the
  engine remains the sole owner of object existence and lifetime; resolution is a per-apply
  read of current engine state (a pure function of it). Prefer the id-based flag API
  (`set_switch_*(id, bool)`) where it avoids pointer handling entirely. A stale/absent id is
  the designed `EntityMissing` benign skip (§17). **Only** if direct lookup proves too costly,
  the bounded fallback is an *ephemeral, per-tick* index rebuilt each apply from the
  authoritative object list, holding **ids, never long-lived pointers** — this is not a
  persistent cache and not a second source of truth. A persistent id→object pointer cache is
  explicitly rejected: it would be a second lifetime/ownership tracker outside the engine
  (dangling on engine destroy, requiring its own relcase sync) and violates C2 / One Engine
  Boundary / Preserve Before Replace / determinism.
- **E-G3 — switch timing (non-blocking).** Measure whether switching completes within one
  frame or spans frames; sizes the Pending* confirmation window. Fallback already designed
  (multi-tick reconciliation).

**All blocking gates are resolved.** **E-G1 is RESOLVED (PASSED)** and **E-G2 is CLOSED
(verified)**. **ADR-008** (§19) is **accepted**. **E-G3** remains non-blocking (tuning of the
Pending* confirmation window) and does not gate freeze or implementation. There are no
outstanding blocking prerequisites — the architecture is frozen (§25).

---

## 22. Design review checklist (for the freeze gate)

- [x] Ownership boundaries unambiguous (engine owns online/offline; module owns intent).
- [x] Transitions deferred + batched + single deterministic phase at `kAlifeTransition`.
- [x] Dedup, idempotency, replay safety specified and testable.
- [x] Deterministic ascending-`EntityId` ordering throughout.
- [x] One Engine Boundary: only the gateway in `EngineAdapters.cpp` touches the engine.
- [x] ADR-007 preserved (no exceptions, `Expected<T>`, C4530-clean); ADR-008 accepted.
- [x] `TransitionResult` defined as the read-only output for replication (Sprint-008).
- [x] Intent state transient / reconstructable (no persistence coupling).
- [x] `NullAlifeSwitchGateway` keeps the engine-free test build intact.
- [x] E-G2 resolved (verified) — on-demand resolution, no cache/retained pointers.
- [x] E-G1 resolved (PASSED) — Cooperative Flag Override (`set_switch_*`) frozen; no
      contention/flip-flop; no reopening of the architecture.
- [x] The single additive `tick_order::kAlifeTransition = 350` constant is the only touch to
      a frozen file, and is non-behavioral.

## 23. Verification requirements

- Engine-free unit tests (fake `IAlifeSwitchGateway`, fake Bubble transitions, fake
  registry): activation/deactivation edges, idempotency (repeat with no new commands),
  replay safety (empty result on re-apply), duplicate prevention, `EntityMissing` handling,
  deterministic ordering, single-step state machine, and stress (hundreds of transitions).
- MSVC Release clean under `EngineAbi.props` (no C4530/C2220; exceptions off); One Engine
  Boundary preserved (only the gateway TU includes engine headers).
- Runtime verification (Antigravity): entities within the bubble come online and leave
  offline in-engine, exactly once each, with no flip-flop, no duplicate simulation, and
  vanilla single-player behavior unchanged when a single local player is present.

## 24. Sprint acceptance criteria

- [ ] Bubble transitions drive engine online/offline via the vanilla switch, entities
      activating/deactivating exactly once per decided edge.
- [ ] No duplicate simulation; one simulation per entity at all times.
- [ ] Idempotent and replay-safe; deterministic ascending ordering.
- [ ] All engine access confined to the `EngineAdapters.cpp` gateway; ALife source unmodified.
- [ ] `TransitionManagerService` wired at `tick_order::kAlifeTransition`, correct teardown.
- [ ] Every transition-layer TU conforms to ADR-007 and ADR-008 (accepted).
- [ ] `TransitionResult` produced read-only for Sprint-008.
- [ ] Unit, idempotency, replay, and stress tests pass. (E-G1 resolved/PASSED and E-G2
      closed at architecture time and recorded.)
- [ ] Documentation written; Definition of Done met; ready for Sprint-006.

---

## 25. Sprint-005 Architecture Freeze

**Frozen: 2026-07-09.** All conditions for freeze are met:

- **E-G1 — RESOLVED (PASSED).** Production switch mechanism selected: **Cooperative Flag
  Override** via `CALifeUpdateManager::set_switch_online/offline(id, bool)`. Direct
  `switch_*`, `try_switch_*`, and configuration-only approaches evaluated and rejected.
- **E-G2 — CLOSED (verified).** On-demand `EntityId → object` resolution through the engine's
  authoritative lookup; no retained pointers; no gateway cache.
- **ADR-008 — ACCEPTED.** Engine State Mutation Boundary formalized on the Cooperative Flag
  Override pattern (§19).
- **E-G3 — non-blocking** (Pending* window tuning); does not gate freeze or implementation.

**Frozen decisions (binding on implementation):**

1. Engine online/offline is driven **only** through `set_switch_online/offline(id, bool)`
   from the single `EngineAlifeSwitchGateway` in `EngineAdapters.cpp`.
2. The gateway is **stateless** with on-demand `EntityId` resolution; no pointers or cache retained.
3. `TransitionManagerService` runs at `tick_order::kAlifeTransition = 350`; transitions are
   deferred, batched, and applied in one deterministic ascending-`EntityId` phase per tick.
4. Intent state is transient and reconstructable; ALife online/offline is persisted by the
   engine's own save system (no new persistence format).
5. `TransitionResult` is the read-only per-tick activation delta consumed by Sprint-008.
6. The only touch to a frozen file is the additive, non-behavioral
   `tick_order::kAlifeTransition = 350` constant.

**Preserved invariants (verified at freeze):** Preserve Before Replace, One Engine Boundary,
Host Authority, Deterministic simulation, ADR-007, ADR-008, Sprint-003 Entity Registry
ownership, Sprint-004 BubbleManager ownership.

This freeze is final. Any change to a frozen decision requires a new ADR or a formally
reopened gate; ordinary implementation may not alter it.

---

## 26. Sprint-005 Definition of Done

Sprint-005 is **Done** when all of the following hold:

1. **Transition consumption.** The layer consumes the Bubble's per-tick `Activations()` /
   `Deactivations()` and drives engine online/offline for each decided entity exactly once
   per edge.
2. **Cooperative Flag Override implemented.** Switching is performed **solely** through
   `set_switch_online/offline(id, bool)` from the single gateway; no forced `switch_*`, no
   `try_switch_*`, no ALife source edit, no configuration workaround.
3. **One Engine Boundary.** Only `EngineAlifeSwitchGateway` in `EngineAdapters.cpp` includes
   engine headers or writes engine state; `TransitionManager`, service, interface, and value
   types remain engine-free; `NullAlifeSwitchGateway` keeps the test build engine-free.
4. **Stateless resolution (E-G2).** The gateway resolves `EntityId` on demand via the engine's
   authoritative lookup, retaining no pointer and no cache.
5. **Exactly-once, no duplicate simulation.** One simulation per entity at all times; no
   flip-flop; vanilla single-player behavior unchanged with only the local actor present.
6. **Determinism, idempotency, replay safety.** Deterministic ascending-`EntityId` ordering;
   re-applying with no new transitions yields no commands; replay yields an empty result.
7. **ADR-007 + ADR-008 conformance.** Every TU compiles under `EngineAbi.props` (exceptions
   off, C4530/C2220-clean); all fallible operations return `core::Expected<T>` / value
   outcomes; no engine mutation outside the sanctioned entry point and single gateway.
8. **Replication readiness.** `TransitionResult` is produced read-only as the Sprint-008
   activation delta.
9. **Composition-root integration.** `TransitionManagerService` is wired at
   `tick_order::kAlifeTransition = 350` with correct reverse-order teardown.
10. **Tests green.** Engine-free unit, idempotency, replay, duplicate-prevention,
    `EntityMissing`, ordering, single-step state-machine, and stress tests pass on GCC and
    MSVC; MSVC Release clean under the Engine ABI Contract.
11. **Runtime verification (Antigravity).** In-engine, entities within the bubble come online
    and leave offline exactly once each, with no flip-flop and no duplicate simulation.
12. **Documentation & closure.** Subsystem documentation written; this architecture and its
    freeze recorded; status docs updated; ready for Sprint-006.

---

## 27. Readiness confirmation

**Sprint-005 is ready to enter implementation.** The architecture is frozen with all blocking
evidence gates resolved (E-G1 PASSED, E-G2 CLOSED), ADR-008 accepted, and every core
invariant preserved. The remaining E-G3 tuning is non-blocking and folds into implementation.
Implementation may proceed against the frozen decisions in §25 and the Definition of Done in §26.

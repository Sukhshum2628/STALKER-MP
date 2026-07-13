# Sprint-005 — ALife Transition Layer — Implementation Plan (AUTHORITATIVE)

- **Status:** Active implementation plan. Architecture is **FROZEN** — this plan implements it verbatim and introduces no new architectural decisions.
- **Role:** Sprint-005 Implementation Lead.
- **Source of truth:** `Multiplayer/docs/Sprint-005-Architecture.md` (frozen 2026-07-09), ADR-007, ADR-008.
- **Frozen invariants (immutable here):** Cooperative Flag Override via `CALifeUpdateManager::set_switch_online/offline(id, bool)`; One Engine Boundary; ADR-007; ADR-008; deterministic ascending-`EntityId` ordering; stateless on-demand `EntityId` resolution (no pointer/cache); intent state transient.
- **Out of scope (do not implement):** networking, replication transport, persistence, player synchronization, spawn/despawn creation, any future-sprint feature. E-G3 tuning is folded in as a config constant only.

---

## 1. Implementation roadmap

Eleven sequential, purely additive steps. Every step is independently buildable and independently verifiable; the tree compiles clean after each. Steps 1–8 and the test work in 10 are **engine-free** and verified on the GCC + MSVC test build using `NullAlifeSwitchGateway`. Step 9 introduces the single engine-touching translation unit; steps 9–11 add the runtime (Antigravity) verification surface.

| Step | Milestone | Engine-touching? | Primary verification |
|---|---|---|---|
| 1 | Value types & enums | No | Compile + type tests |
| 2 | `IAlifeSwitchGateway` seam + `NullAlifeSwitchGateway` | No | Compile + null-gateway tests |
| 3 | `TransitionManager` core (intent store, construction, read-only surface) | No | Unit: construction, `StateOf`, consistency |
| 4 | `Update` ingestion + dedup (ordered command batch) | No | Unit: edge detection, ordering, no-ops |
| 5 | Gateway apply + outcome recording + `TransitionResult` | No | Unit: apply, outcomes, result shape |
| 6 | Reconciliation / confirmation (Pending → confirmed) | No | Unit: confirmation, `EntityMissing`, replay |
| 7 | Statistics + diagnostics | No | Unit: statistics, describe/dump |
| 8 | `TransitionManagerService` (IService + ITickable) | No | Unit: lifecycle, tick, ordering |
| 9 | `tick_order::kAlifeTransition = 350` + `EngineAlifeSwitchGateway` + factories | Yes | MSVC engine-build compile; boundary check |
| 10 | Composition-root wiring + full test suite | Yes (wiring) | Full suite green; runtime smoke |
| 11 | Documentation + sprint closure | No | Docs + status sync; DoD review |

**Global dependency:** every step depends on Sprint-001 (core: `Expected<T>`, `IService`, `ITickable`, `ServiceRegistry`, logging), Sprint-002 (`FrameDispatcher`), Sprint-003 (`EntityRegistry`, `EntityId`), Sprint-004 (`BubbleManager` transitions) — all CLOSED.

---

## 2. Per-step breakdown

Each step lists: **Dependencies · Files to create · Files to modify · File responsibilities · Expected unit tests · Verification checkpoint · Antigravity verification prompt · Definition of Done.** No step combines milestones. No step reopens architecture.

---

### Step 1 — Value types & enums

**Dependencies:** Sprint-003 `EntityId` (`include/stalkermp/world/…` identity type); ADR-007 build props.

**Files to create:**
- `include/stalkermp/world/TransitionTypes.h`

**Files to modify:**
- Test project file (`.vcxproj` / build list) only if a new TU is later needed — **none this step** (header-only).

**Responsibilities:**
- `TransitionTypes.h` declares the frozen value types exactly as §10: `enum class TransitionState { Offline, PendingOnline, Online, PendingOffline }`; `enum class TransitionKind { Activate, Deactivate }`; `enum class TransitionOutcome { Applied, AlreadyInState, EntityMissing, Failed }`; `struct TransitionCommand { EntityId id; TransitionKind kind; }`; `struct TransitionRecord { EntityId id; TransitionKind kind; TransitionOutcome outcome; }`; `struct TransitionResult { std::vector<EntityId> broughtOnline; std::vector<EntityId> broughtOffline; std::uint64_t tick; }`; `struct TransitionStatistics { … }` (all seven counters from §10). Pure POD/enum; engine-free; no logic.

**Expected unit tests** (`tests/TransitionTests.cpp`, new):
- Enum value stability (default = `Offline` / `Activate`).
- `TransitionCommand` orders by ascending `EntityId` under the project comparator.
- `TransitionResult` default-constructs empty with tick 0.
- Trivial-copy / no-throw static asserts (ADR-007 posture).

**Verification checkpoint:** header compiles standalone under `EngineAbi.props`; test TU links; no engine header pulled in.

**Antigravity verification prompt:**
> Build the test target after Step 1. Confirm `TransitionTypes.h` compiles standalone under EngineAbi.props (exceptions off, /W4 /WX, C4530-clean), that no engine header is transitively included, and that the new TransitionTests type-stability tests pass on GCC and MSVC. Report test counts before/after.

**Definition of Done:** all seven value types present and matching §10; header engine-free; type tests green on GCC + MSVC; clean compile.

---

### Step 2 — `IAlifeSwitchGateway` seam + `NullAlifeSwitchGateway`

**Dependencies:** Step 1.

**Files to create:**
- `include/stalkermp/world/IAlifeSwitchGateway.h`
- `include/stalkermp/adapters/NullAlifeSwitchGateway.h` (+ inert impl in the test-support TU)

**Files to modify:**
- `tests/support/NullEngineAdapters.cpp` — add the inert `NullAlifeSwitchGateway` implementation and a `CreateNullAlifeSwitchGateway()` (or equivalent) test factory.

**Responsibilities:**
- `IAlifeSwitchGateway.h` — engine-free abstract seam (§9): `Apply(const std::vector<TransitionCommand>&) -> std::vector<TransitionOutcome>` (parallel to input); `IsOnline(EntityId) const -> std::optional<bool>` (nullopt if unknown). No engine types in the signature.
- `NullAlifeSwitchGateway` — inert counterpart: `Apply` returns `AlreadyInState` (or a configurable script for tests) per command; `IsOnline` returns a test-controlled map/nullopt. Keeps the engine-free test build intact.

**Expected unit tests** (`tests/TransitionTests.cpp`):
- Null gateway returns one outcome per command, order-preserving.
- `IsOnline` returns injected values / nullopt for unknown ids.
- Interface is abstract (cannot instantiate) and non-throwing.

**Verification checkpoint:** interface + null impl compile; test build has zero engine headers; null factory usable from tests.

**Antigravity verification prompt:**
> Build the test target after Step 2. Confirm `IAlifeSwitchGateway` is engine-free (no engine header in the interface or the null adapter), that `NullAlifeSwitchGateway` returns one outcome per command in input order, and that the null-gateway tests pass on GCC and MSVC. Confirm the test build still links with no engine TU.

**Definition of Done:** seam matches §9 exactly; null adapter inert and injectable; engine-free test build preserved; tests green; clean compile.

---

### Step 3 — `TransitionManager` core (intent store, construction, read-only surface)

**Dependencies:** Steps 1–2; Sprint-003 `EntityRegistry`; Sprint-004 `BubbleManager` (types only, injected read-only).

**Files to create:**
- `include/stalkermp/world/TransitionManager.h`
- `src/world/TransitionManager.cpp`

**Files to modify:**
- Test/library build lists to include the new `TransitionManager.cpp` TU.

**Responsibilities:**
- Intent-state store: deterministic **sorted vector keyed by `EntityId` + secondary hash accelerator** (BC-005-approved containers, exactly like the registry, canonical ascending order). Sole writer of intent state.
- Construction / injection: `TransitionManager(const BubbleManager*, const EntityRegistry*, IAlifeSwitchGateway*)` (read-only refs for the first two, per §14). No `Update` yet.
- Read-only surface stubs: `StateOf(EntityId) -> TransitionState` (Offline if untracked); `ValidateConsistency()` skeleton; `LastResult()` returns empty. No engine header.

**Expected unit tests:**
- Constructs with fake Bubble, fake registry, null gateway.
- `StateOf` returns `Offline` for unknown ids.
- Intent store maintains ascending order under insertion (white-box or via `StateOf` sweep).
- `ValidateConsistency` passes on an empty manager.

**Verification checkpoint:** manager compiles and links engine-free; constructs deterministically; no `Update` behavior yet.

**Antigravity verification prompt:**
> Build the test target after Step 3. Confirm `TransitionManager` compiles engine-free, constructs against fake Bubble/registry and the null gateway, that `StateOf` returns Offline for unknown ids, and that intent-store ordering is ascending by EntityId. Run the Step-3 unit tests on GCC and MSVC and report counts.

**Definition of Done:** manager core builds engine-free; intent store is sorted-vector + hash (BC-005); construction and read-only stubs deterministic; tests green; clean compile.

---

### Step 4 — `Update` ingestion + dedup (ordered command batch)

**Dependencies:** Step 3.

**Files to create:** none.

**Files to modify:**
- `include/stalkermp/world/TransitionManager.h`, `src/world/TransitionManager.cpp` — add ingestion + dedup (pipeline §7 steps 1–4).

**Responsibilities:**
- Read this tick's Bubble `Activations()` / `Deactivations()` (by `EntityId`).
- Validate each against the registry (`Contains` / `FindByEntityId`): drop entries whose entity is no longer registered/live.
- Reconcile against intent state and **emit a command only on a genuine edge** (activate only if not Online/PendingOnline; deactivate only if not Offline/PendingOffline) — duplicate prevention per §13.
- Produce **one ordered batch** (ascending `EntityId`) staged for apply — no gateway call yet, no engine mutation. Expose the staged batch for test inspection.

**Expected unit tests:**
- Activation edge for an Offline entity produces exactly one `Activate` command.
- Re-reported membership across ticks produces no duplicate command (idempotent ingestion).
- Deactivation of an untracked/Offline entity is a no-op.
- Unregistered entity is dropped (no command).
- Batch is ascending by `EntityId` regardless of Bubble input order.

**Verification checkpoint:** ingestion deterministic; dedup correct; batch ordered; still no engine interaction.

**Antigravity verification prompt:**
> Build the test target after Step 4. Confirm ingestion reads Bubble transitions, validates against the registry, emits commands only on genuine edges, drops unregistered entities, and produces an ascending-EntityId batch. Verify no gateway/engine call occurs yet. Run Step-4 unit tests on GCC and MSVC; report counts.

**Definition of Done:** pipeline steps 1–4 implemented; duplicate prevention and ordering proven by tests; no engine mutation; tests green; clean compile.

---

### Step 5 — Gateway apply + outcome recording + `TransitionResult`

**Dependencies:** Step 4.

**Files to create:** none.

**Files to modify:**
- `TransitionManager.h/.cpp` — apply the staged batch (§7 steps 5–6).

**Responsibilities:**
- Apply the ordered batch through `IAlifeSwitchGateway::Apply` in a **single deterministic phase**.
- Record per-command `TransitionOutcome`; advance intent state Offline→PendingOnline / Online→PendingOffline on `Applied`/`AlreadyInState`.
- Build the read-only `TransitionResult` for the tick (`broughtOnline` / `broughtOffline`, ascending; `tick`). `LastResult()` returns it.
- All fallible paths return `core::Expected<T>` / value outcomes; no throw (ADR-007). No presumed synchrony — confirmation deferred to Step 6.

**Expected unit tests:**
- Batch applied once, outcomes parallel to input order.
- Offline→PendingOnline and Online→PendingOffline transitions occur on apply.
- `TransitionResult` lists only newly-actioned ids, ascending.
- `Failed` outcome keeps prior intent (retry next tick).
- Null-gateway `AlreadyInState` yields no state churn.

**Verification checkpoint:** apply path deterministic; result read-only and ordered; single apply phase (no per-command mid-frame side effects).

**Antigravity verification prompt:**
> Build the test target after Step 5. Confirm the batch is applied through the gateway in one phase, outcomes are recorded parallel to input, intent advances to Pending* correctly, `TransitionResult` is ascending and read-only, and `Failed` preserves prior intent. Confirm no exceptions and ADR-007 posture. Run Step-5 tests on GCC and MSVC; report counts.

**Definition of Done:** apply + outcome recording + result production match §7/§13; Expected/value outcomes only; deterministic single-phase apply; tests green; clean compile.

---

### Step 6 — Reconciliation / confirmation (Pending → confirmed)

**Dependencies:** Step 5.

**Files to create:** none.

**Files to modify:**
- `TransitionManager.h/.cpp` — next-tick confirmation and drop logic (§12 state machine completion).

**Responsibilities:**
- On each tick, reconcile Pending* entities: `PendingOnline → Online` and `PendingOffline → Offline` when confirmed (outcome `Applied`/`AlreadyInState` or `IsOnline` read-back agrees).
- Single-step-per-tick guarantee: at most one state advance per entity per tick.
- `EntityMissing` (entity left the registry mid-flight) drops the entry to Offline/untracked.
- **Replay safety:** re-running `Update` with identical Bubble transitions and identical engine state yields no new commands and an empty `TransitionResult`.

**Expected unit tests:**
- PendingOnline confirms to Online on read-back true; PendingOffline to Offline on read-back false.
- `EntityMissing` drops in-flight entry cleanly.
- Replay: second identical `Update` produces empty result, no churn.
- No entity advances more than one state in a single tick.

**Verification checkpoint:** full state machine (§12) closed; replay-safe; deterministic convergence to engine truth.

**Antigravity verification prompt:**
> Build the test target after Step 6. Confirm Pending* entities converge to Online/Offline via read-back, that a mid-flight `EntityMissing` drops cleanly, that single-step-per-tick holds, and that a replayed identical `Update` yields an empty `TransitionResult` with no state churn. Run Step-6 tests on GCC and MSVC; report counts.

**Definition of Done:** §12 state machine complete; idempotency and replay safety proven; deterministic; tests green; clean compile.

---

### Step 7 — Statistics + diagnostics

**Dependencies:** Step 6.

**Files to create:** none.

**Files to modify:**
- `TransitionManager.h/.cpp` — `Statistics()`, `DescribeState`, `DumpPending`, and completed `ValidateConsistency` (mirroring Sprint-004 diagnostics, §9).

**Responsibilities:**
- `TransitionStatistics` counters populated from intent state + this tick (online, pendingOnline, pendingOffline, offlineTracked, appliedThisTick, skippedThisTick, failedThisTick).
- Diagnostics are read-only, allocation-light, and use `common::Format` + C stdio (no iostream; ADR-007). `ValidateConsistency` asserts intent-store ordering and state legality.

**Expected unit tests:**
- Counter arithmetic matches a scripted transition sequence.
- `DescribeState` / `DumpPending` produce stable strings for a known state.
- `ValidateConsistency` detects an artificially corrupted ordering (white-box) and passes on valid state.

**Verification checkpoint:** diagnostics read-only and deterministic; no iostream; counters accurate.

**Antigravity verification prompt:**
> Build the test target after Step 7. Confirm statistics counters match a scripted sequence, diagnostics are read-only and iostream-free (common::Format + C stdio), and `ValidateConsistency` passes on valid state and flags corruption. Run Step-7 tests on GCC and MSVC; report counts.

**Definition of Done:** statistics + diagnostics complete and accurate; ADR-007 output posture; tests green; clean compile.

---

### Step 8 — `TransitionManagerService` (IService + ITickable)

**Dependencies:** Step 7.

**Files to create:**
- `include/stalkermp/world/TransitionManagerService.h`
- `src/world/TransitionManagerService.cpp`

**Files to modify:**
- Build lists to include the new service TU.

**Responsibilities:**
- `core::IService` + `core::ITickable` wrapper owning the `TransitionManager`.
- Injects read-only `BubbleManager` + `EntityRegistry` and an `IAlifeSwitchGateway`.
- `Tick` calls `m_manager.Update(++m_tick)` (single monotonic tick, mirroring `BubbleManagerService`).
- `Dependencies()` = `{ "World", "EntityRegistry", "BubbleManager" }`. `Initialize`/`Shutdown` deterministic and non-reentrant. **No `FrameDispatcher` subscription yet** (wired in Step 10).

**Expected unit tests:**
- Service constructs with fakes + null gateway; reports correct dependencies.
- `Tick` advances the manager once per call (monotonic tick).
- Initialize/Shutdown idempotent; no work on an empty tick.

**Verification checkpoint:** service builds engine-free; ticks the pipeline; dependency contract correct.

**Antigravity verification prompt:**
> Build the test target after Step 8. Confirm `TransitionManagerService` is IService + ITickable, declares dependencies {World, EntityRegistry, BubbleManager}, advances the manager once per Tick with a monotonic tick, and constructs against fakes + null gateway. Confirm it does not yet subscribe to FrameDispatcher. Run Step-8 tests on GCC and MSVC; report counts.

**Definition of Done:** service matches §6/§14 (minus wiring); engine-free; monotonic tick; dependency contract correct; tests green; clean compile.

---

### Step 9 — `tick_order::kAlifeTransition = 350` + `EngineAlifeSwitchGateway` + factories

**Dependencies:** Step 8. **First engine-touching step.**

**Files to create:** none (concrete gateway lives inside the existing engine adapter TU).

**Files to modify:**
- `include/stalkermp/core/FrameDispatcher.h` — add the single additive constant `tick_order::kAlifeTransition = 350` (between `kBubble = 300` and `kReplication = 400`). Non-behavioral; the only touch to a frozen file.
- `include/stalkermp/adapters/EngineAdapters.h` — declare `adapters::CreateEngineAlifeSwitchGateway()` factory (mirrors the entity-feed factory).
- `src/adapters/EngineAdapters.cpp` — implement `EngineAlifeSwitchGateway` and the factory.
- `tests/support/NullEngineAdapters.cpp` — ensure the null factory symbol matches the new declaration for the test build.

**Responsibilities:**
- `EngineAlifeSwitchGateway` is the **only** TU that includes engine headers for this sprint. It resolves `EntityId.value == ALife::_OBJECT_ID` **on demand** via the engine's authoritative lookup (`objects().object(id)`), retains **no** pointer and **no** cache, and drives the switch **only** through `CALifeUpdateManager::set_switch_online(id, bool)` / `set_switch_offline(id, bool)` (Cooperative Flag Override, ADR-008). `IsOnline` is a read-only engine query. Returns `Applied` / `AlreadyInState` / `EntityMissing` / `Failed` as values; never throws; never edits ALife; never calls forced `switch_*` or `try_switch_*`.
- Guarded like the feed: if `g_pGameLevel == nullptr` or no ALife simulator, commands no-op and report benign outcomes.

**Expected unit tests:**
- Engine-free build: `FrameDispatcher` ordering test confirms `kBubble(300) < kAlifeTransition(350) < kReplication(400)`.
- The engine gateway TU is excluded from the test build (One Engine Boundary); the null factory is used instead — asserted by a build-configuration check.

**Verification checkpoint:** engine build compiles the gateway under `EngineAbi.props`; test build excludes it; tick-order constant additive and correctly placed.

**Antigravity verification prompt:**
> Build BOTH targets after Step 9. (1) Test build: confirm `kAlifeTransition = 350` sits between Bubble(300) and Replication(400), the ordering test passes, and only the null gateway is linked (no engine TU). (2) MSVC engine build (xrMP → AnomalyDX9): confirm `EngineAdapters.cpp` compiles under EngineAbi.props with the new `EngineAlifeSwitchGateway`, that it calls only `set_switch_online/offline(id,bool)` (grep confirms no forced `switch_*`/`try_switch_*`), resolves ids on demand with no retained pointer/cache, and returns value outcomes with no exceptions. Report both build results.

**Definition of Done:** tick-order constant added (only frozen-file touch, non-behavioral); concrete gateway implements ADR-008 exactly (set_switch_* only, on-demand resolution, no cache, no throw); One Engine Boundary intact (single engine TU); both builds clean.

---

### Step 10 — Composition-root wiring + full test suite

**Dependencies:** Step 9.

**Files to create:** none.

**Files to modify:**
- `src/core/Bootstrap.cpp` — create `TransitionManagerService` **after** the Bubble service, owned by `ServiceRegistry`; inject `BubbleManager`, `EntityRegistry`, and the gateway from `CreateEngineAlifeSwitchGateway()` (Runtime-owned; null in test build); subscribe to the existing `FrameDispatcher` at `tick_order::kAlifeTransition = 350`; teardown order: `frameBridge.reset()` → unsubscribe transition service → unsubscribe Bubble/others → `services.ShutdownAll()`; gateway released with Runtime.
- `tests/TransitionTests.cpp` and `tests/BootstrapTests.cpp` — complete the suite: full pipeline integration with fakes, idempotency, replay, duplicate-prevention, `EntityMissing`, deterministic ordering, single-step machine, and a stress test (hundreds of transitions), plus composition-root wiring/teardown-order assertions.

**Responsibilities:**
- Single deterministic per-frame dispatch at 350, strictly after Bubble(300); no new `seqFrame`; correct reverse-order teardown (no dangling callback).

**Expected unit tests:**
- Bootstrap registers the service with correct dependencies and tick order; teardown reverses subscription order.
- End-to-end (fakes): Bubble activation → gateway apply → confirmation → offline, exactly once each.
- Stress: hundreds of transitions remain ordered, deterministic, leak-free.

**Verification checkpoint:** full suite green on GCC + MSVC; MSVC Release clean under `EngineAbi.props`; runtime smoke shows entities activating/deactivating once with no flip-flop.

**Antigravity verification prompt:**
> Build and run the FULL suite after Step 10 on GCC and MSVC; confirm all Sprint-005 tests pass and MSVC Release is clean under EngineAbi.props (no C4530/C2220). Confirm Bootstrap wires `TransitionManagerService` after Bubble at tick_order 350 with correct reverse-order teardown and no new seqFrame. Then run the game (AnomalyDX9): with a single local player, confirm vanilla single-player behavior is unchanged; confirm entities within the bubble come online and leave offline exactly once each, with no flip-flop and no duplicate simulation. Report full test counts and runtime observations.

**Definition of Done:** service wired at 350 with correct teardown; full suite green on both toolchains; MSVC Release clean; runtime smoke passes (exactly-once, no flip-flop, vanilla SP unchanged); One Engine Boundary preserved.

---

### Step 11 — Documentation + sprint closure

**Dependencies:** Step 10.

**Files to create:**
- `Multiplayer/docs/TransitionLayer.md` (§7.16-style subsystem doc: ownership, seams, pipeline, state machine, diagnostics, ADR-008 mapping).

**Files to modify:**
- `Documentation/SPRINTS/Sprint-005-Online-Offline-Transitions.md` — Status → **Implemented & Verified (Closed)**.
- `Documentation/AI/CURRENT_STATUS.md` — Sprint-005 → Closed/Verified; roadmap row updated; test totals.
- `Documentation/AI/SESSION_LOG.md` — append the Sprint-005 implementation session.

**Responsibilities:**
- Document the delivered subsystem and record closure consistent with Sprint-003/004 style; no architectural change.

**Expected unit tests:** none (documentation step).

**Verification checkpoint:** docs match delivered code; status docs internally consistent; no stale "Ready for Implementation" left where "Closed" belongs.

**Antigravity verification prompt:**
> Review the Step-11 documentation. Confirm `TransitionLayer.md` matches the delivered code (seams, pipeline, state machine, ADR-008 mechanism), that Sprint-005 status is Closed/Verified across the sprint doc, CURRENT_STATUS, and SESSION_LOG, and that final test totals are recorded. Confirm no contradictions remain.

**Definition of Done:** subsystem doc written; all status docs synchronized to Closed/Verified with final counts; Sprint-005 Definition of Done (Architecture §26) fully satisfied and recorded.

---

## 3. Cross-cutting requirements (enforced every step)

- **Independently buildable & verifiable:** each step compiles clean and has its own tests / verification prompt; no step depends on a later step to build.
- **One Engine Boundary:** only `src/adapters/EngineAdapters.cpp` (Step 9+) includes engine headers; steps 1–8 are engine-free and test-verified via `NullAlifeSwitchGateway`.
- **ADR-007:** exceptions off, `/W4 /WX`, C4530/C2220-clean, `core::Expected<T>` / value outcomes, no iostream (`common::Format` + C stdio).
- **ADR-008:** engine state written only via `set_switch_online/offline(id, bool)` from the single gateway; on-demand resolution, no retained pointer/cache; no presumed synchrony.
- **Determinism:** ascending-`EntityId` ordering throughout; single deterministic apply phase per tick; single-step-per-tick confirmation.
- **Additive only:** the sole touch to a frozen file is `tick_order::kAlifeTransition = 350` (Step 9), non-behavioral.
- **Exclusions:** no networking, replication transport, persistence, player sync, or spawn/despawn creation; no future-sprint features; no combined milestones.

---

## 4. Expected final project state after Sprint-005

- **New engine-free module:** `TransitionTypes.h`, `IAlifeSwitchGateway.h`, `NullAlifeSwitchGateway`, `TransitionManager` (.h/.cpp), `TransitionManagerService` (.h/.cpp) under `include/stalkermp/world` + `src/world`.
- **One engine-touching addition:** `EngineAlifeSwitchGateway` + `CreateEngineAlifeSwitchGateway()` inside `src/adapters/EngineAdapters.cpp`; `EngineAdapters.h` factory declaration; null counterpart in `tests/support/NullEngineAdapters.cpp`.
- **One additive frozen-file change:** `tick_order::kAlifeTransition = 350` in `FrameDispatcher.h`.
- **Composition root:** `TransitionManagerService` owned by `ServiceRegistry`, subscribed at 350, correct reverse-order teardown, gateway Runtime-owned.
- **Behavior:** host-authoritative, deterministic online/offline driven by Bubble membership through Cooperative Flag Override; vanilla single-player behavior unchanged; exactly-once activation/deactivation; no flip-flop; no duplicate simulation; `TransitionResult` produced read-only for Sprint-008.
- **Tests:** full Sprint-005 suite green on GCC + MSVC; MSVC Release clean under `EngineAbi.props`; runtime smoke verified by Antigravity.
- **Docs:** `TransitionLayer.md` written; sprint doc, CURRENT_STATUS, and SESSION_LOG all show Sprint-005 **Closed & Verified**; Sprint-006 (Host Networking) unblocked.

---

## 5. Execution protocol

Implement **one step at a time**, purely additive. After each step: build, run that step's tests, and issue the step's Antigravity verification prompt. **Do not begin the next step until the current step is verified.** Any architectural question is out of scope — the architecture is frozen; this plan is executed verbatim.

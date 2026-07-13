# Sprint-008 — Snapshot System — Implementation Plan (AUTHORITATIVE)

- **Status:** Active implementation roadmap. Architecture **FROZEN** (`Multiplayer/docs/Sprint-008-Architecture.md`, frozen 2026-07-11 with the review-approved `IEntitySnapshotSource` clarification). This plan implements that architecture verbatim and introduces no new architectural decision, ADR, scope change, or ownership change.
- **Roles:** Principal Architect authored this plan; the Implementation Engineer converts each step into one implementation prompt; Antigravity (Verification Engineer) independently verifies each step before the next begins.
- **Frozen invariants (immutable here):** Preserve Before Replace, Host Authority, One World, One ALife Simulation, One Engine Boundary, One Platform Boundary (ADR-009), Wire Protocol Contract (ADR-010 — untouched; snapshots are not packets), Single-Threaded / Deterministic boundary (ADR-011 — no thread created), Engine State Mutation Boundary (ADR-008 — read-only observation only), ADR-007, Deterministic Simulation, Replay Determinism, and the existing ownership of Sprints 002–007 (World, Entity Registry, Bubble, Transition, Session, Player). The existing `tick_order` table is preserved; Sprint-008 occupies the already-reserved `kReplication = 400` slot (no new key, no relational change).

---

## 1. Sprint Overview

**Objective.** Build the engine-free **immutable Snapshot System**: the deterministic, sim-thread-only mechanism that captures a per-tick, immutable, value-only copy of entity/player/environment state and hands it to a bounded, exception-free, multi-consumer-safe queue — the single foundation on which Replication (Sprint-009), Persistence (Sprints 011–012), Replay, and future workers will build. Snapshots only: no replication, persistence, delta, packets/message-ids, or worker threads.

**Implementation boundaries (in scope).** `snapshot::SnapshotTypes`; `snapshot::SnapshotConfiguration`; the immutable `snapshot::SimulationSnapshot` + read-only `snapshot::ISnapshotView` base; the exception-free `snapshot::SnapshotPool`; the new engine-free `world::IEntitySnapshotSource` seam (+ `NullEntitySnapshotSource`); `snapshot::SnapshotBuilder`; `snapshot::SnapshotQueue`; `snapshot::SnapshotManager` (`IService` + `ITickable`) at `tick_order::kReplication = 400`; `adapters::EngineEntitySnapshotSource` (the one engine TU) + factory; Bootstrap composition-root wiring; `snapshot::SnapshotDiagnostics`; validation hardening; integration + documentation; sprint closure.

**Explicit non-goals (do NOT implement).** Replication logic/prioritization/bandwidth (Sprint-009); delta/interest/compression (Sprints 009–010); prediction/interpolation (Sprint-010); save-game/durable serialization or a persistence format (Sprints 011–012); **any network packet, wire encoding, or `MessageId`** (ADR-010 untouched); **any thread creation or scheduling** (ADR-011 — the queue is concurrency-*ready* behind a documented single-producer/multi-consumer contract but no thread is spawned and it is exercised single-threaded); any engine-state mutation (ADR-008); materialized `ReplicationSnapshot`/`PersistenceSnapshot`/`ThreadSnapshot` content (reserved projections, architecture §22); repurposing `IEntitySnapshotSource` as a general-purpose entity-access API (architecture §15 clarification — it is consumed only by `SnapshotBuilder`); assignment of, or dependency on, any future producer `tick_order` value.

---

## 2. Implementation Strategy

**Philosophy — engine-free core first, single engine TU late, wiring last (the Sprints 005/006/007 shape).** Steps 1–8 and 11–14 are entirely engine-free, verified on GCC + MSVC against `NullEntitySnapshotSource`, fake player/environment surfaces, and the real pool/queue — no engine, no OS, no threads. Only Step 9 introduces engine headers, confined to `adapters::EngineEntitySnapshotSource` inside `EngineAdapters.cpp` behind the engine-free `world::IEntitySnapshotSource` (One Engine Boundary). Composition-root wiring (Step 10) touches the frozen `Bootstrap.cpp` exactly once, after the subsystem is proven in isolation.

**Why this order minimizes integration risk.** The highest-risk surfaces (engine entity capture, composition-root lifetime, the frame slot at 400, and exception-free bounded memory) are isolated and deferred; the bulk of the logic (immutable aggregate, pool reuse, builder, queue ref-counting) is proven with in-memory fakes first. Every step is purely additive and leaves the tree buildable; no step depends on a later step to compile. The pool + immutable snapshot (Steps 3–4) exist before the builder (Step 6), so the deterministic build-and-seal is testable the moment it lands. The capture seam (Step 5) is a value-only interface, so the builder is written and tested against the null before any engine code exists.

**Subsystem dependency order (strict).** value types → config → immutable snapshot + view → pool → capture seam + null → builder → queue → manager → engine adapter → Bootstrap wiring → diagnostics → validation hardening → integration + docs → closure. Each capability is introduced once, in dependency order, so no partial-ownership or partially-mutable-snapshot state ever exists.

**Namespace / layout conventions (mapping only, not new decisions).** New subsystem namespace `stalkermp::snapshot`; headers under `include/stalkermp/snapshot/`; sources under `src/snapshot/`; the capture seam under `include/stalkermp/world/IEntitySnapshotSource.h` (it is a `world::` seam, like `IEntitySource`); the engine adapter in the existing `src/adapters/EngineAdapters.cpp` with its null counterpart under `tests/support/NullEntitySnapshotSource.cpp`; the factory header `include/stalkermp/adapters/EntitySnapshotSource.h`; tests in a new `tests/SnapshotTests.cpp` (+ `tests/BootstrapTests.cpp` for the wiring assertion). This mirrors the `world`/`net`/`player`/`adapters` split already in the tree.

---

## 3. Implementation Steps

> Every step: engine-free unless explicitly noted (only Step 9 touches engine headers); single-threaded (no thread created); ADR-007 (no exceptions, no RTTI, `Expected<T>`/value outcomes, no throwing allocation on the hot path, C4530-clean, no iostream — `common::Format`); deterministic and replay-safe; additive; leaves the project buildable on GCC + MSVC. Each step is independently verified by Antigravity before the next begins.

### Step 1 — Snapshot value types
- **Objective.** The engine-free vocabulary for the subsystem.
- **Scope.** Value types/enums only; no logic, no storage.
- **Files to create.** `include/stalkermp/snapshot/SnapshotTypes.h` (`SnapshotId`, `SnapshotKind`, `SnapshotState`, `SnapshotMetadata`, `EntitySnapshot`, `PlayerSnapshot`, `EnvironmentSnapshot`, `SnapshotStatistics`, `SnapshotConsistency`, `Name()` helpers); create `tests/SnapshotTests.cpp` wired into the test project.
- **Files to modify.** `xrMP.vcxproj` / `tests/xrMP_Tests.vcxproj`.
- **Dependencies.** Sprint-002 `world::EntityId`/`Vec3`, Sprint-007 `player::PlayerId`, Sprint-002 environment value shape (reused by value).
- **Constraints.** Engine-free / OS-free; POD-style; enums `enum class : std::uint8_t`; ids "0 = none"; no logic.
- **Determinism.** Fixed enum layout; tick-derived id/metadata fields; no wall-clock in content (a diagnostic `timestampWallClock` field is marked reserved/diagnostic).
- **Definition of Done.** Types match architecture §9/§10; engine/OS-free; ADR-007-clean; GCC + MSVC green.

### Step 2 — `SnapshotConfiguration` + `FromConfig`
- **Objective.** Parse/validate `[snapshot]` config.
- **Scope.** `SnapshotConfiguration` + `FromConfig` only.
- **Files to create.** `include/stalkermp/snapshot/SnapshotConfiguration.h` (+ `src/snapshot/SnapshotConfiguration.cpp`).
- **Responsibilities.** Pool capacity (buffers), max entities/players per snapshot, snapshot format version, queue depth, build-budget (diagnostic). Missing → defaults; invalid → `InvalidArgument` (`common::Format`); cross-field (e.g., `poolCapacity >= 2`, `queueDepth <= poolCapacity`).
- **Dependencies.** Step 1; Sprint-001 `ConfigStore`/`Expected`/`common::Format`.
- **Determinism.** All budgets are fixed integers; no wall-clock.
- **Definition of Done.** Matches §9; `FromConfig` validated (default/override/invalid/cross-field); engine/OS-free; both toolchains green.

### Step 3 — Immutable `SimulationSnapshot` + `ISnapshotView`
- **Objective.** The immutable aggregate + read-only base + the `Building→Finalized` immutability gate.
- **Scope.** `ISnapshotView` (const-only accessors), `SimulationSnapshot` implementing it; a state gate so no public API mutates a finalized snapshot; **no** pool/builder/queue yet (construction is direct for this step's tests).
- **Files to create.** `include/stalkermp/snapshot/ISnapshotView.h`, `include/stalkermp/snapshot/SimulationSnapshot.h` (+ `.cpp`).
- **Interfaces.** `ISnapshotView`: `Metadata()`, `Entities()` (ascending), `Players()` (ascending), `Environment()`. `SimulationSnapshot`: internal mutable build API (BeginBuild/append/Finalize) callable only pre-finalize; const-only view after.
- **Invariants (E-G1-S).** After `Finalize`, the public surface is const-only; a mutate attempt does not compile; `SnapshotState` transitions Building→Finalized once, monotonically.
- **Determinism.** Ascending-id ordering enforced at build; identical inputs → identical view.
- **Definition of Done.** Immutability enforced structurally + a validation flag; engine/OS-free; ADR-007-clean; both toolchains green (E-G1-S groundwork).

### Step 4 — `SnapshotPool`
- **Objective.** Exception-free fixed-capacity buffer reuse with intrusive ref-count.
- **Scope.** `SnapshotPool`: `Reserve(capacity)`, `Acquire() -> Expected<buffer>`, `ReturnToPool(buffer)`, `InUse()/Capacity()`; per-buffer intrusive ref-count; capacity exhaustion → `PoolExhausted` value outcome. No `shared_ptr`/throwing `new` on the hot path (pre-reserved at `Reserve`).
- **Files to create.** `include/stalkermp/snapshot/SnapshotPool.h` (+ `.cpp`).
- **Invariants (E-G3-S).** Steady-state reuse (no allocation after `Reserve`); `Acquire` past capacity → `PoolExhausted` (never throw/UB); ref-count returns a buffer exactly once at zero.
- **Determinism.** Deterministic buffer selection (e.g., ascending free-index); no address-dependent behavior in outcomes.
- **Definition of Done.** Reuse proven; exhaustion is a value outcome; ADR-007 (no throwing allocation); both toolchains green.

### Step 5 — `world::IEntitySnapshotSource` seam + `NullEntitySnapshotSource`
- **Objective.** The additive, snapshot-specific, engine-free capture seam + inert test source. **No engine code.**
- **Scope.** `world::IEntitySnapshotSource` (value-only `Capture(std::vector<EntitySnapshot>& out)`, append-only, ascending); `adapters::NullEntitySnapshotSource` (deterministic synthetic/empty capture); `adapters::CreateEngineEntitySnapshotSource()` factory declaration. Per architecture §15: consumed **only** by `SnapshotBuilder`; supplements (never replaces/broadens) the Sprint-003 Entity Registry / Sprint-002 read-only entity interfaces.
- **Files to create.** `include/stalkermp/world/IEntitySnapshotSource.h`, `include/stalkermp/adapters/EntitySnapshotSource.h` (factory decl), `tests/support/NullEntitySnapshotSource.cpp`.
- **Files to modify.** `tests/xrMP_Tests.vcxproj` (null TU); `xrMP.vcxproj` ClInclude (factory header; the engine impl is Step 9).
- **Invariants.** The seam names no engine call/type; the null is deterministic and engine/OS-free; the test build links only the null.
- **Definition of Done.** Seam + null + factory decl exist and back the test build; no engine code; both toolchains green.

### Step 6 — `SnapshotBuilder`
- **Objective.** Deterministic per-tick capture into a pooled buffer, sealed at finalize.
- **Scope.** `SnapshotBuilder`: `BeginBuild(pool, tick) -> Expected<handle>` (acquire buffer, stamp metadata), `Capture(entitySource, playerSurface, environmentSource)` (append entities ascending via `IEntitySnapshotSource`, players via the Sprint-007 read-only surface, environment via Sprint-002 `IEnvironmentSource`), `Finalize() -> Expected<const SimulationSnapshot*>` (seal). Engine-free; sim-thread only; consumes only value seams.
- **Files to create.** `include/stalkermp/snapshot/SnapshotBuilder.h` (+ `.cpp`).
- **Dependencies.** Steps 1–5; Sprint-007 `PlayerManager` read-only surface; Sprint-002 `IEnvironmentSource`.
- **Invariants (E-G2-S).** No live object crosses the seam (values only); ascending capture; identical inputs+tick → identical finalized content + metadata (excluding wall-clock); `PoolExhausted`/`Overflow` handled as outcomes (build skipped, no partial publish).
- **Definition of Done.** Full build→finalize against the null/fakes; determinism/replay covered; engine/OS-free; both toolchains green.

### Step 7 — `SnapshotQueue`
- **Objective.** Multi-consumer-safe publish/acquire/release with ref-counted retire.
- **Scope.** `SnapshotQueue`: `Publish(const SimulationSnapshot*)`, `Acquire() -> const SimulationSnapshot*` (ref++), `Release(const*)` (ref--), `Depth()/Published()`; retire a superseded snapshot to the pool when ref-count hits zero. Single-producer/multi-consumer contract documented; exercised single-threaded (no thread created).
- **Files to create.** `include/stalkermp/snapshot/SnapshotQueue.h` (+ `.cpp`).
- **Invariants (E-G4-S).** Multiple synchronous consumers read the same published snapshot without corruption; a snapshot is retired exactly once after its last release + supersession; `Release` of an unknown/retired handle is benign.
- **Definition of Done.** Publish/acquire/release + retire proven with several synchronous consumers; ADR-007 (exception-free); both toolchains green.

### Step 8 — `SnapshotManager`
- **Objective.** The per-tick lifecycle service (not yet Bootstrap-wired).
- **Scope.** `SnapshotManager` (`IService` + `ITickable`): owns the builder/pool/queue; `Initialize` reserves the pool; `Tick` runs BeginBuild→Capture→Finalize→Publish once; read-only `LatestMetadata()`/`Statistics()`/`ValidateConsistency()`/`Queue()`. `Dependencies() = {World, EntityRegistry, BubbleManager, TransitionManager, PlayerManager}` (ordering-only).
- **Files to create.** `include/stalkermp/snapshot/SnapshotManager.h` (+ `.cpp`).
- **Dependencies.** Steps 1–7; the capture seam (Step 5) + player/environment surfaces injected as references.
- **Invariants.** One snapshot published per tick; on `PoolExhausted` the tick skips (previous published snapshot remains valid); single-threaded.
- **Definition of Done.** Full per-tick lifecycle against the null/fakes; engine/OS-free; both toolchains green.

### Step 9 — Engine adapter (`EngineEntitySnapshotSource`)
- **Objective.** The one engine TU marshaling per-entity value data.
- **Scope.** `adapters::EngineEntitySnapshotSource` + engine-build `CreateEngineEntitySnapshotSource()` in `src/adapters/EngineAdapters.cpp`: read engine object transform/velocity/state/flags on demand (no retained pointer, no mutation — ADR-008 read-only observation) and marshal into `EntitySnapshot` values, ascending by `EntityId`. Test build keeps `NullEntitySnapshotSource`.
- **Files to modify.** `src/adapters/EngineAdapters.cpp`; `xrMP.vcxproj` (engine build only; the TU is already listed).
- **Invariants.** Engine headers only in `EngineAdapters.cpp` (One Engine Boundary); no engine mutation (ADR-008); values only cross the seam; consumed only by `SnapshotBuilder`.
- **Definition of Done.** Engine build compiles under `EngineAbi.props`; test build unaffected (null path green); no engine header leaks outside `EngineAdapters.cpp`. Engine capture smoke is Antigravity's on Windows.

### Step 10 — Bootstrap wiring (`kReplication = 400`)
- **Objective.** Integrate the manager as one `ITickable` at the reserved slot.
- **Scope.** Register `SnapshotManager` in `Bootstrap.cpp` after the Transition service; construct with `SnapshotConfiguration`, the `IEntitySnapshotSource` from `CreateEngineEntitySnapshotSource()` (Runtime-owned; null in tests), and const references to the player/environment surfaces; subscribe to the `FrameDispatcher` at `tick_order::kReplication = 400`; reverse-order teardown; `BootstrapTests` subscriber-count (5/6→next) + a `static_assert(kAlifeTransition < kReplication && kReplication < kPersistence)` placement assertion.
- **Files to modify.** `src/core/Bootstrap.cpp`, `tests/BootstrapTests.cpp`; the two vcxprojs (manager + service TUs).
- **Invariants.** Existing `tick_order` table preserved (occupies the frozen `kReplication = 400`; no new key); frame order …→ Transition (350) → **Snapshot (400)** → … → Networking (900); reverse-order teardown drains the queue and returns buffers to the pool; Bootstrap includes only the engine-free factory header (no engine header).
- **Definition of Done.** Manager ticks at 400; subscriber count + placement asserted; teardown leaves no orphan buffer; engine/OS-free wiring; suite green.

### Step 11 — `SnapshotDiagnostics`
- **Objective.** Read-only, replay-stable observability.
- **Scope.** `SnapshotDiagnostics`: `Statistics`, `DescribeState`, `DumpSnapshot(SnapshotId)`, `BuildHistory`, `QueueStatus`, `MemoryUsage`, `ValidateConsistency` (ordering, id monotonicity, no duplicate ids, counts match, no retired-buffer aliasing, immutability flag set). Bandwidth/latency/worker fields reserved.
- **Files to create.** `include/stalkermp/snapshot/SnapshotDiagnostics.h` (+ `.cpp`).
- **Invariants.** Read-only; deterministic (except explicitly-diagnostic timing/wall-clock); iostream-free.
- **Definition of Done.** All accessors read-only; consistency negatives covered; engine/OS-free; suite green.

### Step 12 — Validation hardening
- **Objective.** Prove the integrity/immutability negative surface (test-and-harden; no new behavior).
- **Scope.** Extend `SnapshotTests.cpp`: missing/duplicate/invalid-handle capture, version mismatch, corruption detection, immutable-access rejection, pool exhaustion, queue over-depth, large-world + churn stress, replay identity; tighten an outcome/message only if a proven gap surfaces (no interface change).
- **Files to modify.** `tests/SnapshotTests.cpp`; minimal `snapshot::*` `.cpp` hardening only if a gap is found.
- **Invariants.** Every rejection is a value outcome leaving state unchanged; no live object ever captured; determinism preserved under stress.
- **Definition of Done.** Full negative surface green; no interface/behavior change for valid inputs; suite green.

### Step 13 — Integration + documentation
- **Objective.** End-to-end integration behind seams + subsystem doc.
- **Scope.** Integration tests via the composed stack (World/Registry/Player captured correctly through the null-source + fakes; queue operational; multiple synchronous consumers read the same published snapshot safely; full-tick replay identity); author `Multiplayer/docs/Snapshots.md` (purpose, ownership, components, per-tick pass at 400, immutability/lifecycle, memory model, boundaries, ADR-007/008/009/010/011, worker-consumer contract, extension points). No behavior change.
- **Files to create/modify.** `Multiplayer/docs/Snapshots.md`; `tests/SnapshotTests.cpp`/`tests/BootstrapTests.cpp`.
- **Definition of Done.** Integration green; doc matches delivered code; consistency sweep clean; suite green.

### Step 14 — Documentation + sprint closure (no code)
- **Objective.** Confirm Sprint-008 is implemented/verified/closeable; synchronize status docs.
- **Scope.** Cross-check the Definition of Done (§4/§31 of the architecture; sprint doc §12) against delivered, Antigravity-verified artifacts; record final test counts, build status, boundary/determinism/gate outcomes; update `Documentation/SPRINTS/Sprint-008-Replication-Snapshots.md`, `Documentation/AI/CURRENT_STATUS.md`, `Documentation/AI/SESSION_LOG.md` to Closed/Verified; declare readiness for Sprint-009. A consistency scan.
- **Files to modify.** The three status docs (status/closure only).
- **Invariants.** Documentation only; record only verified facts.
- **Definition of Done.** DoD satisfied and recorded; status consistent; Sprint-008 declared Implemented / Verified / Closed / Frozen; Sprint-009 readiness stated.

---

## 4. Step-to-architecture / evidence-gate mapping

| Plan step | Architecture reference | Evidence gate |
|---|---|---|
| 1 Snapshot value types | §9/§10, §13 | — |
| 2 Configuration | §9 | — |
| 3 Immutable snapshot + view | §9/§10, §18, FR-2 | **E-G1-S** (immutability) groundwork |
| 4 SnapshotPool | §9/§19/§24, FR-6 | **E-G3-S** (exception-free bounded memory) |
| 5 IEntitySnapshotSource + null | §9/§15 (clarification) | — |
| 6 SnapshotBuilder | §8/§10/§13, FR-1/3/7/9 | **E-G2-S** (deterministic build) |
| 7 SnapshotQueue | §9/§12, FR-5 | **E-G4-S** (multi-consumer safety) |
| 8 SnapshotManager | §8/§9/§12 | — |
| 9 Engine adapter | §15, AD-5, ADR-008 | E-G* engine capture (Antigravity) |
| 10 Bootstrap wiring | §8/§12, AD-2 | — |
| 11 Diagnostics | §20 | — |
| 12 Validation hardening | §18/§24 | E-G1-S/E-G3-S negatives |
| 13 Integration + docs | §21/§22 | E-G2-S/E-G4-S composed |
| 14 Closure | §25/§31 | all gates recorded |

---

## 5. Implementation-Risk Review

- **Dependency ordering.** The strict order (types → config → immutable snapshot → pool → seam/null → builder → queue → manager → engine adapter → wiring → diagnostics → validation → integration → closure) means every step compiles/tests against already-delivered pieces; no step needs a later step to compile.
- **Hidden coupling.** Architecture-identified couplings enforced by step boundaries: (a) builder↔live-objects — the value-only `IEntitySnapshotSource` (Step 5) is the only entity capture path, consumed only by the builder (Step 6); (b) latent threading — the queue (Step 7) exposes only `Publish`/`Acquire`/`Release` behind a documented single-producer/multi-consumer contract, and **no thread is created**; (c) wall-clock in content — kept in a diagnostic metadata field only (Step 1). The §15 clarification is enforced: `IEntitySnapshotSource` is not exposed to any subsystem other than `SnapshotBuilder`.
- **Rollback safety.** Every step is additive and leaves the tree buildable; a failed build/publish is a value outcome that skips the tick (previous snapshot remains valid) with no partial state.
- **Transactional behavior.** BeginBuild acquires a pooled buffer; a failed Capture/Finalize returns the buffer to the pool (no leak); Publish is all-or-nothing; retire happens exactly once at ref-count zero.
- **Initialization order.** `SnapshotManager` registered after the Transition service; `Dependencies()` names World/EntityRegistry/Bubble/Transition/Player (all registered earlier), so registration-order `InitializeAll` initializes it after them; `Initialize` reserves the pool.
- **Shutdown order.** Reverse order: the manager is frame-unsubscribed before `ShutdownAll`; the queue drains and returns all buffers to the pool; no snapshot outlives the pool.
- **Ownership correctness.** Live state stays with Sprints 002–007; snapshot buffers in the pool; published snapshots in the queue; consumers hold non-owning ref-counted const handles. No step writes another subsystem's state.
- **Determinism.** Fixed 400 slot after all mutation, monotonic tick-derived `SnapshotId`, ascending-id capture, wall-clock excluded from identity — guarded by the Step-6 and Step-12 replay tests.
- **Replay safety.** The Step-6 determinism test and the Step-12 stress-replay are the explicit guards; the engine adapter (Step 9) is behind the seam and does not affect test-build replay.
- **Future-sprint compatibility.** The versioned metadata, the reserved `Replication`/`Persistence`/`Thread` projections over `ISnapshotView`, and the queue's single-producer/multi-consumer contract are delivered without reopening the architecture; no future producer `tick_order` value is assigned (Sprint-008 occupies only the frozen `kReplication = 400`).

All implementation-planning risks above are resolved within this plan; none requires an architecture or ADR change.

---

## 6. Completion Criteria

Sprint-008 implementation is **complete** when:
1. All 14 steps are implemented, each independently verified by Antigravity, tree buildable after each.
2. The snapshot module runs as a single `ITickable` at `tick_order::kReplication = 400` (the frozen Sprint-008 slot), strictly after all simulation producers, integrated via Bootstrap with reverse-order teardown.
3. One Engine Boundary **and** One Platform Boundary hold: only `EngineAdapters.cpp` includes engine headers (now also `EngineEntitySnapshotSource`); no OS header is added; the test build links the null capture source.
4. ADR-007, ADR-008, ADR-009, ADR-010, ADR-011 all conformed to (008 read-only observation; 009/010 untouched — no packets/ids; 011 single-threaded, no thread created).
5. Evidence gates: **E-G1-S PASSED, E-G2-S PASSED, E-G3-S PASSED**; E-G4-S confirmed (non-blocking).
6. Full suite green on GCC + MSVC, MSVC Release clean under `EngineAbi.props`, zero new warnings, no regressions.
7. No non-goal implemented (no replication/persistence/delta/prediction/packets/message-ids/threads); `IEntitySnapshotSource` consumed only by `SnapshotBuilder`; snapshots own no live simulation state.
8. Subsystem doc (`Multiplayer/docs/Snapshots.md`) written; all status docs synchronized to Closed/Verified.

---

## 7. Sprint Closure Checklist

Before Sprint-008 is declared **Implemented / Verified / Closed / Frozen**:

- [ ] Steps 1–14 implemented and each independently Antigravity-verified.
- [ ] Project buildable after every step (GCC + MSVC), zero new warnings.
- [ ] `SnapshotManager` ticks once at `tick_order::kReplication = 400`; Bootstrap wiring + reverse-order teardown verified; subscriber count updated; placement asserted (`kAlifeTransition < kReplication < kPersistence`).
- [ ] One Engine Boundary intact (engine headers only in `EngineAdapters.cpp`; the sole new engine code is `EngineEntitySnapshotSource`).
- [ ] One Platform Boundary intact (no OS header added).
- [ ] Immutability enforced (E-G1-S); deterministic build/publication (E-G2-S); exception-free bounded memory with buffer reuse (E-G3-S); multi-consumer safety confirmed (E-G4-S).
- [ ] Snapshots own **no** live simulation state; captures are values only (no raw pointers); `IEntitySnapshotSource` consumed only by `SnapshotBuilder` (architecture §15 clarification).
- [ ] ADR-007/008/009/010/011 preserved; no new `tick_order` key, no `MessageId`, no thread, no persistence format.
- [ ] Final test counts + build status recorded; `Snapshots.md` written; the three status docs updated to Closed/Verified.
- [ ] Sprint-008 Definition of Done (§6) fully satisfied; architecture unchanged (frozen).

---

## 8. Recommended implementation sequence (authoritative execution order)

1. Snapshot value types.
2. `SnapshotConfiguration` + `FromConfig`.
3. Immutable `SimulationSnapshot` + `ISnapshotView` — **E-G1-S** groundwork.
4. `SnapshotPool` — **E-G3-S**.
5. `world::IEntitySnapshotSource` seam + `NullEntitySnapshotSource`.
6. `SnapshotBuilder` — **E-G2-S**.
7. `SnapshotQueue` — **E-G4-S**.
8. `SnapshotManager`.
9. `EngineEntitySnapshotSource` (the one engine TU).
10. `tick_order::kReplication` Bootstrap wiring.
11. `SnapshotDiagnostics`.
12. Validation hardening.
13. Integration + documentation.
14. Documentation + sprint closure.

Steps 1–8 (and the test work in 10) are engine-free and OS-free, verified with the null capture source and fakes; Step 9 is the single engine-touching translation unit; Steps 10–14 add wiring, diagnostics, hardening, integration, and closure. Execute strictly in this order, verifying each step before beginning the next.

---

## 9. Implementation Plan Freeze

The Sprint-008 Implementation Plan is **FROZEN** as of 2026-07-11.

- **Internally consistent.** The 14 steps form a strict, additive dependency chain; each is independently verifiable, leaves the tree buildable, and maps to the frozen architecture (§4 mapping) and its evidence gates.
- **Conforms to the frozen Sprint-008 Architecture.** Every step derives from `Multiplayer/docs/Sprint-008-Architecture.md`; the engine-free core, single-threaded execution (no thread created), immutable value-only snapshots, exception-free bounded memory, `kReplication = 400` placement, One Engine Boundary (sole new engine TU `EngineEntitySnapshotSource`), and the `IEntitySnapshotSource` §15 clarification (consumed only by `SnapshotBuilder`) are preserved. No step introduces replication/persistence/delta/prediction/packets/message-ids/threads or engine mutation.
- **No architecture modified.** This plan changes no architectural decision, no ADR (ADR-007…ADR-011 preserved), and no previously frozen sprint architecture, plan, message allocation, verification rules, or step specification.
- **Ready for the next phase.** Sprint-008 is ready to begin **Step Specifications**, one step at a time, each frozen before implementation and independently verified by Antigravity before the next begins.

No step specifications, implementation, or verification are produced here — the implementation-planning phase ends at this freeze.

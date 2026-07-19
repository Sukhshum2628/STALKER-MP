# Sprint-011 — Persistence Framework · Architecture & Implementation Plan

| Field | Value |
|---|---|
| Sprint | 011 — Persistence Framework |
| Status | **Plan — EXECUTED & CLOSED** (all 17 steps implemented, verified, frozen — 2026-07-19) |
| Scope authority | `Documentation/SPRINTS/Sprint-011/Sprint-011-Persistence.md` (FROZEN) |
| Baseline | Sprint-010 CLOSED & FROZEN — **519 / 519** tests passing (GCC + MSVC) |
| Governing ADRs | ADR-007, ADR-008, ADR-009, ADR-010, ADR-011 (all preserved; none modified) |
| Architecture refs | `08_Persistence.md`, `09_Threading.md`; RFC-0003, RFC-0005, RFC-0006 |
| Steps | 17 (Step-01 … Step-17), strict additive dependency chain |

---

## 0. Governance & preserved invariants (apply to every step)

This plan is **purely additive** and modifies no prior sprint's frozen documents, code, or ADRs. Every step preserves, without exception:

- **Preserve Before Replace** — persistence consumes the **existing** Sprint-008 immutable `SimulationSnapshot` through the frozen `snapshot::SnapshotQueue` (read-only `ISnapshotView`). It defines **no new simulation capture** and reimplements none of the world, ALife, snapshot, or replication logic. `PersistenceSnapshot` is a read-only *projection* over the acquired snapshot view, not a second capture.
- **Host Authority** — persistence is a **read-only consumer** of authoritative snapshots. It never writes to, mutates, or feeds back into the authoritative simulation, ALife, world, or replication. Save failures never affect the running World (retain-previous, continue).
- **One World / One ALife Simulation** — no second world or simulation; persistence observes the single authoritative snapshot stream.
- **Deterministic Simulation & Replay Determinism** — scheduling, metadata (incl. checksum), validation, versioning, and queue ordering are pure and tick-derived: identical snapshots + identical configuration + identical tick sequence yield identical persistence decisions and identical metadata. Wall-clock appears **only** in explicitly-diagnostic fields, never in control flow.
- **One Engine Boundary** — the persistence subsystem is **engine-free**. It introduces **no engine translation unit** and includes no engine headers; it consumes the engine-free snapshot seam.
- **One Platform Boundary** — Sprint-011 introduces **no OS/file/socket/serialization code**. Actual storage writing, save-file format, and serialization are **out of scope** (§4 of the scope authority) and belong to Sprint-012. Persistence hands a validated snapshot + metadata to an **engine-free `IPersistenceStore` seam**; the real backend is a Sprint-012 adapter, and the test/default build links an in-memory/null store. This preserves the platform boundary exactly as Sprint-006's `PlatformTransport` seam did for sockets.
- **ADR-007** — no exceptions, no RTTI, no iostream; all fallible operations are `core::Expected<T>` / `PersistenceOutcome` value outcomes; bounded memory (pre-reserved queues; no unbounded growth).
- **ADR-008** — persistence performs **read-only observation** of the authoritative snapshot; it never mutates authoritative state.
- **ADR-009** — the platform boundary is untouched (no OS code; storage behind an engine-free seam).
- **ADR-010** — the wire protocol is **not** touched; persistence is host-local and sends nothing.
- **ADR-011 (decisive)** — Sprint-011 creates **no thread**. Although `09_Threading.md` §11 describes a conceptual "Persistence Worker thread," the Version-1 realization is **synchronous**, exactly as the Sprint-009 `ReplicationWorker` was ("*NEVER creates a thread … designed to run on a future worker thread behind the snapshot seam (ADR-011), but Sprint-009 exercises it synchronously*"). The `PersistenceManager` advances the worker synchronously once per authoritative frame at the **reserved** `tick_order::kPersistence = 500` (no new key is allocated; the slot already exists between Replication 450 and Diagnostics 600). Networking-last is preserved (500 < 900).

**Compatibility.** Every step is additive and leaves the Sprint-010 **519/519** baseline intact; new tests only add to the count. No prior public API changes.

---

## 0.A Binding architectural decisions (resolved during design verification)

Three readings are load-bearing and are frozen here so no step drifts:

- **D-PF1 — Synchronous worker (ADR-011).** The `PersistenceWorker` is synchronous and creates no thread. The scope doc's "asynchronous persistence workers … independent from the Simulation Thread" is the *conceptual* model of `09_Threading.md` (which frames the already-implemented `ReplicationWorker` as a "thread" too); V1 realizes it synchronously behind the immutable snapshot seam. Independence is achieved by consuming an **immutable** snapshot, not by concurrency.
- **D-PF2 — Storage is an engine-free seam; no serialization/OS code.** `PersistSnapshot` validates and hands off to `IPersistenceStore` (an engine-free interface). Sprint-011 ships the interface + an in-memory/null store used by the default and test builds. Save-file format, world serialization, compression, and the real filesystem backend are **Sprint-012** (§4 out-of-scope). This keeps One Platform Boundary intact and lets Sprint-012 add the real backend behind the frozen seam.
- **D-PF3 — Reuse the Sprint-008 snapshot (Preserve Before Replace).** `PersistenceSnapshot` is a read-only value projection built from the acquired `ISnapshotView`; persistence becomes a **second consumer** of the multi-consumer `SnapshotQueue` (Acquire/Release, reference-counted), alongside replication. No new capture path is introduced.

---

## 1. Sprint objective

Deliver the **Persistence Framework** — the host-side infrastructure that coordinates *when* and *how safely* authoritative world snapshots are handed off for saving — without implementing any save format or serialization. It provides scheduling, worker management, a bounded queue with back-pressure, snapshot consumption, version tracking, validation, and diagnostics, so that Sprint-012 can implement full save/load on top of a stable, deterministic, failure-isolated foundation.

## 2. Scope

**In scope.** Engine-free host subsystem in a new `persistence::` namespace: value types & configuration; a `VersionManager`; a deterministic `SaveMetadata` builder; a `PersistenceSnapshot` read-only projection over `ISnapshotView`; a pure `ValidationFramework`; a bounded `PersistenceQueue` with back-pressure; the engine-free `IPersistenceStore` seam + in-memory/null store; a **synchronous** `PersistenceWorker`; a deterministic `SaveScheduler`; the `PersistenceManager` (`IService` + `ITickable`) at `tick_order::kPersistence = 500`; error-handling/recovery hardening; `PersistenceDiagnostics` + statistics; composition-root wiring; integration tests; and the subsystem doc `Multiplayer/docs/Persistence.md`.

## 3. Out-of-scope items

Save file format, world serialization, load/restore pipeline, compression, cloud saves, backup management (§4 of the scope authority); any thread creation; any new `tick_order` key (the reserved `kPersistence = 500` is used); any OS/filesystem code (behind the storage seam, realized in Sprint-012); any mutation of the authoritative simulation, ALife, snapshot, replication, or networking; any wire-protocol change.

## 4. Design goals

Persistence runs independently of simulation (immutable handoff, no blocking); configurable scheduling; workers consume immutable snapshots only; deterministic, replay-safe decisions and metadata; bounded memory with value-outcome error handling; failure isolation (a failed save never corrupts the World and retains the previous save); engine-free, unit-testable core; zero impact on the authoritative pipeline and the Sprint-010 baseline.

## 5. Architectural overview

Persistence is a **read-only consumer** of the Sprint-008 snapshot stream, running once per authoritative frame after replication. The `PersistenceManager` ticks at `kPersistence = 500`:

```
snapshot::SnapshotQueue ──Acquire()──► PersistenceSnapshot (read-only projection)
   (Sprint-008, multi-consumer)                     │
SaveScheduler ──SaveRequest(trigger)──► PersistenceQueue ──► PersistenceWorker (synchronous, no thread)
   (manual/autosave/admin/shutdown/deferred)   (bounded,        │  Consume → Validate → build SaveMetadata
                                                back-pressure)   │  → PersistSnapshot(IPersistenceStore)   → Release()
VersionManager ─(compatibility)─────────────────────────────────┘        (engine-free seam; real backend = Sprint-012)
```

The World, Sprint-008 snapshot, Sprint-009 replication, and networking tick are all unchanged. Replication (450) still runs before persistence (500); both are independent read-only consumers of the same published snapshot.

## 6. Component responsibilities

| Component | Responsibility |
|---|---|
| `persistence::PersistenceTypes` | Engine-free vocabulary: `PersistenceOutcome`, `SaveTrigger`, `SaveState`, `SaveRequest`, `SaveMetadata`, `PersistenceStatistics`, `PersistenceConsistency`, name helpers. |
| `persistence::PersistenceConfiguration` (+ `FromConfig`) | Queue depth, autosave interval (ticks), max retries, retry backoff (ticks), back-pressure high/low watermarks, versions. |
| `persistence::VersionManager` | Tracks persistence/world/schema/compatibility versions; `IsCompatible`; migration-required flag. Pure. |
| `persistence::SaveMetadataBuilder` | Deterministic `SaveMetadata` from an acquired snapshot view (save id, tick, player/entity counts, world/build version, checksum). Pure; no serialization. |
| `persistence::PersistenceSnapshot` | Read-only projection over `ISnapshotView` (Preserve Before Replace); completeness/integrity accessors. |
| `persistence::ValidationFramework` | Pure validators: snapshot integrity, version compatibility, queue ordering, completeness → value outcomes. |
| `persistence::PersistenceQueue` | Bounded FIFO of save jobs; `Publish`/`Acquire`/`Release`/`Retry`; back-pressure via watermarks; queue statistics. Exception-free, bounded. |
| `persistence::IPersistenceStore` (+ null/in-memory) | Engine-free storage seam (`Begin`/`Write`/`Commit`/`Abort` value-outcome surface). Real backend = Sprint-012. |
| `persistence::PersistenceWorker` | **Synchronous** (no thread): `Consume` → `Validate` → build metadata → `Persist` (via store) → `Flush`; `Start`/`Stop` are lifecycle toggles. Failure-isolated. |
| `persistence::SaveScheduler` | Deterministic tick-driven scheduling: manual, periodic autosave, administrative, shutdown, deferred; configurable policy; emits `SaveRequest`s. |
| `persistence::PersistenceManager` | Owns worker + queue + scheduler + version manager + store; `RequestSave(trigger)`, per-tick orchestration; `IService` + `ITickable` at `kPersistence = 500`; reads `SnapshotQueue` read-only. |
| `persistence::PersistenceDiagnostics` | Non-invasive collector + read-only inspectors (queue viewer, worker status, autosave monitor, version inspector, save history). |
| `adapters` wiring | Composition-root construction/teardown in `Bootstrap.cpp`; default `[persistence]` config block. |

## 7. Tick-order & frame placement

The `PersistenceManager` subscribes as a single `ITickable` at the **reserved** `tick_order::kPersistence = 500` — after `kReplicationPipeline = 450`, before `kDiagnostics = 600` and `kNetworking = 900`. **No new key is allocated.** Persistence is part of the authoritative frame dispatch (unlike Sprint-010's post-dispatch presentation phase) because it is a host-side consumer of the just-published snapshot, exactly like replication. Networking-last is preserved.

## 8. Storage seam (One Platform Boundary)

`IPersistenceStore` is an engine-free, value-outcome interface (`Begin(SaveMetadata) → handle`, `Write(handle, PersistenceSnapshot) `, `Commit(handle)`, `Abort(handle)`), with **no serialization semantics defined in Sprint-011** — it transfers ownership of a validated snapshot + metadata to a backend. Sprint-011 ships an in-memory store (records accepted saves for tests/diagnostics) and a null store. Sprint-012 supplies the real filesystem backend behind this frozen seam.

## 9. Testing strategy

`Multiplayer/tests/PersistenceTests.cpp` accumulates per-step unit tests plus a `PersistenceIntegrationStep15` composed suite and `Bootstrap` wiring assertions. GCC engine-free build + MSVC. Every rejection is a value outcome leaving state unchanged; determinism verified by twin-instance replay; bounded memory verified under sustained-autosave churn.

## 10. Evidence gates (must all pass by close)

- **E-G1-PF** — read-only immutable-snapshot consumption; no live simulation access; no authoritative mutation.
- **E-G2-PF** — deterministic scheduling, metadata (incl. checksum), validation, and versioning; wall-clock diagnostic-only.
- **E-G3-PF** — worker independence & failure isolation: no thread created; a worker/store failure never corrupts simulation and retains the previous save.
- **E-G4-PF** — bounded queue, back-pressure, and robust value-outcome validation (no unbounded growth; overflow/rejection are value outcomes).
- **E-G5-PF** — One Platform Boundary + One Engine Boundary preserved: no OS/file/serialization code and no engine TU; storage behind the engine-free seam.

## 11. Definition of Done (from scope authority §12)

Persistence operates independently of simulation; save scheduling is configurable; workers consume immutable snapshots only; version tracking implemented; validation framework complete; ready for Save/Load (Sprint-012). All recorded at closure (Step-17).

## 12. Step ordering (dependency chain)

`01 types → 02 config → 03 version manager → 04 metadata builder → 05 persistence snapshot projection → 06 validation framework → 07 persistence queue → 08 storage seam (IPersistenceStore + null/in-memory) → 09 persistence worker (synchronous) → 10 save scheduler → 11 PersistenceManager → 12 error handling + recovery hardening → 13 diagnostics + statistics → 14 composition-root wiring (kPersistence = 500) → 15 integration tests → 16 documentation (Persistence.md) → 17 sprint closure.`

Each step is independently buildable (clean compile, tree green after each) and independently verifiable (its own tests), strictly additive.

---

## Step specifications

### Step-01 — Persistence value types & vocabulary
- **Objective.** The engine-free vocabulary every later step builds on.
- **Scope — In.** `PersistenceTypes.h` (header-only): `enum class PersistenceOutcome : std::uint8_t { Ok, QueueFull, VersionMismatch, IntegrityFailure, IncompleteSnapshot, WorkerUnavailable, StorageUnavailable, NothingToPersist }` (+ name); `enum class SaveTrigger : std::uint8_t { Manual, Autosave, Administrative, Shutdown, Deferred }` (+ name); `enum class SaveState : std::uint8_t { Pending, Validating, Persisting, Completed, Failed }` (+ name); `struct SaveRequest { std::uint64_t id; SaveTrigger trigger; std::uint64_t requestTick; }`; `struct SaveMetadata { std::uint64_t saveId; std::uint64_t simulationTick; std::uint32_t playerCount; std::uint32_t entityCount; std::uint32_t worldVersion; std::uint32_t buildVersion; std::uint64_t checksum; std::uint64_t timestampWallClock /*diagnostic*/; }`; `struct PersistenceStatistics { ... monotonic counters ...; std::uint64_t timestampWallClock; }`; `struct PersistenceConsistency { bool readOnly; bool boundedQueue; bool versionTracked; bool failureIsolated; bool IsHealthy() const; }`. Fixed `std::uint8_t` enums, reserved-0 neutral, append-only. **Out.** Logic, buffers, manager.
- **FR.** POD/trivially-copyable value captures; deterministic layout; wall-clock isolated to diagnostic fields.
- **NFR.** ADR-007; engine/OS-free; header-only. **Evidence.** E-G1-PF/E-G2-PF groundwork. **Tests.** `PersistenceTypesStep1`: enum underlying types, name totals, POD defaults. **Commit:** `docs(persistence): freeze Sprint-011 Step-01 spec (types)`.

### Step-02 — `PersistenceConfiguration`
- **Objective.** Parsed `[persistence]` configuration.
- **Scope — In.** `PersistenceConfiguration.h`/`.cpp`: `queueDepth` (≥1), `autosaveIntervalTicks` (0 = disabled), `maxRetries`, `retryBackoffTicks`, `backpressureHighWatermark`, `backpressureLowWatermark` (low ≤ high ≤ queueDepth), `version` (≥1); `FromConfig(ConfigStore)` (missing section → defaults; invalid → `InvalidArgument`). Mirrors the Sprint-008/009/010 `*Config::FromConfig` style. **Out.** Consumption.
- **FR.** Cross-field validation; value outcomes. **Evidence.** E-G4-PF groundwork. **Tests.** `PersistenceConfigStep2`: defaults, overrides, zero-disable autosave, watermark ordering, invalid rejected. **Commit:** `docs(persistence): freeze Sprint-011 Step-02 spec (config)`.

### Step-03 — `VersionManager`
- **Objective.** Track and compare persistence/world/schema/compatibility versions.
- **Scope — In.** `VersionManager.h` (header-only or `.cpp`): value struct of the four versions + migration-required flag; `IsCompatible(other) -> bool`; `Compare(...) -> {Equal, MigrationRequired, Incompatible}`; pure/deterministic. **Out.** Migration execution (Sprint-012).
- **FR.** Deterministic comparison; no wall-clock. **Evidence.** E-G2-PF. **Tests.** `VersionManagerStep3`: equal, migration-required, incompatible, boundaries. **Commit:** `docs(persistence): freeze Sprint-011 Step-03 spec (versions)`.

### Step-04 — `SaveMetadataBuilder`
- **Objective.** Deterministic `SaveMetadata` from an acquired snapshot view.
- **Scope — In.** `SaveMetadataBuilder.h`/`.cpp`: `Build(const snapshot::ISnapshotView&, SaveTrigger, saveId) -> SaveMetadata` — reads tick, player/entity counts, world/build version; computes a deterministic `checksum` over snapshot-derived values (engine-free, order-independent-by-construction). Wall-clock stamped into the diagnostic field only. **Out.** Serialization/file writing.
- **FR.** Pure; identical view → identical metadata (checksum stable); wall-clock excluded from checksum. **Evidence.** E-G2-PF. **Tests.** `SaveMetadataStep4`: field capture, checksum determinism, checksum sensitivity to content, wall-clock excluded. **Commit:** `docs(persistence): freeze Sprint-011 Step-04 spec (metadata)`.

### Step-05 — `PersistenceSnapshot` projection
- **Objective.** A read-only, engine-free projection over the Sprint-008 snapshot (Preserve Before Replace).
- **Scope — In.** `PersistenceSnapshot.h`: wraps a `const snapshot::ISnapshotView&` (no copy of simulation state, no new capture); exposes `Tick()`, `PlayerCount()`, `EntityCount()`, `IsComplete()`, `IntegrityOk()` read-only accessors used by validation. Lifetime bound to the acquired snapshot reference. **Out.** Any capture/mutation.
- **FR.** Read-only; no ownership of simulation data; no engine type crosses. **Evidence.** E-G1-PF. **Tests.** `PersistenceSnapshotStep5`: projection accessors over a fake `ISnapshotView`; no mutation. **Commit:** `docs(persistence): freeze Sprint-011 Step-05 spec (snapshot projection)`.

### Step-06 — `ValidationFramework`
- **Objective.** Pure validators producing value outcomes.
- **Scope — In.** `ValidationFramework.h`/`.cpp`: `ValidateIntegrity(PersistenceSnapshot) -> PersistenceOutcome`; `ValidateVersion(VersionManager, metadata) -> PersistenceOutcome`; `ValidateCompleteness(PersistenceSnapshot) -> PersistenceOutcome`; `ValidateQueueOrdering(...) -> PersistenceOutcome`. Pure; each rejection leaves state unchanged. **Out.** Storage/worker.
- **FR.** Deterministic; value outcomes (`IntegrityFailure`/`VersionMismatch`/`IncompleteSnapshot`). **Evidence.** E-G2-PF/E-G4-PF. **Tests.** `ValidationStep6`: pass + each negative. **Commit:** `docs(persistence): freeze Sprint-011 Step-06 spec (validation)`.

### Step-07 — `PersistenceQueue`
- **Objective.** Bounded FIFO of save jobs with back-pressure.
- **Scope — In.** `PersistenceQueue.h`/`.cpp`: pre-reserved ring; `Publish(SaveRequest) -> PersistenceOutcome` (`QueueFull` at capacity → value outcome, unchanged); `Acquire() -> const SaveRequest*` (FIFO front); `Release()`; `Retry(SaveRequest)` (bounded re-enqueue); watermark-based back-pressure signal (`IsBackpressured()` between high/low); queue statistics (depth, published, dropped). Exception-free; no allocation in steady state. **Out.** Worker/scheduler.
- **FR.** FIFO ordering; bounded; overflow is a value outcome; back-pressure hysteresis. **Evidence.** E-G4-PF. **Tests.** `PersistenceQueueStep7`: publish/acquire/release order, overflow, retry bound, watermark hysteresis, stats. **Commit:** `docs(persistence): freeze Sprint-011 Step-07 spec (queue)`.

### Step-08 — Storage seam `IPersistenceStore` (+ null/in-memory)
- **Objective.** The engine-free storage boundary (One Platform Boundary); real backend deferred to Sprint-012.
- **Scope — In.** `IPersistenceStore.h` (engine-free interface): `Begin(const SaveMetadata&) -> Expected<StoreHandle>`, `Write(StoreHandle, const PersistenceSnapshot&) -> PersistenceOutcome`, `Commit(StoreHandle) -> PersistenceOutcome`, `Abort(StoreHandle) noexcept`. `InMemoryPersistenceStore` (records accepted saves; test/default) + `NullPersistenceStore` (accepts and discards). **No file format, no serialization, no OS calls.** **Out.** Real filesystem backend (Sprint-012).
- **FR.** Value-outcome surface; atomic begin/commit/abort semantics (a store either commits fully or aborts, retaining the previous save); no OS code. **Evidence.** E-G5-PF/E-G3-PF. **Tests.** `PersistenceStoreStep8`: in-memory begin/write/commit records a save; abort discards; storage-unavailable simulated → value outcome. **Commit:** `docs(persistence): freeze Sprint-011 Step-08 spec (storage seam)`.

### Step-09 — `PersistenceWorker` (synchronous)
- **Objective.** The synchronous worker that turns one acquired snapshot into a persisted save (no thread; ADR-011).
- **Scope — In.** `PersistenceWorker.h`/`.cpp`: `Start()`/`Stop()` (lifecycle toggles — **no OS thread**); `Consume(PersistenceSnapshot, VersionManager) -> PersistenceOutcome` (validate); `Persist(metadata, snapshot, IPersistenceStore) -> PersistenceOutcome` (Begin → Write → Commit; on failure Abort + retain previous); `Flush()`; failure isolation (a store/validation failure returns a value outcome and never corrupts state). Runs entirely on the calling thread; deterministic. Header states the ReplicationWorker-precedent rationale. **Out.** Scheduling, ownership of the queue.
- **FR.** No thread created; deterministic; failure-isolated (retain-previous); value outcomes throughout. **Evidence.** E-G3-PF. **Tests.** `PersistenceWorkerStep9`: consume+persist success; validation reject; store failure → abort + previous retained; not-started → `WorkerUnavailable`; determinism. **Commit:** `docs(persistence): freeze Sprint-011 Step-09 spec (worker)`.

### Step-10 — `SaveScheduler`
- **Objective.** Deterministic, tick-driven scheduling of save requests.
- **Scope — In.** `SaveScheduler.h`/`.cpp`: `RequestManual()`, `RequestAdministrative()`, `RequestShutdown()`, `Defer(untilTick)`; `Tick(currentTick) -> std::optional<SaveRequest>` producing periodic autosave every `autosaveIntervalTicks` and any pending manual/admin/shutdown/deferred request in a deterministic priority order; configurable policy. No wall-clock in control flow (tick-derived). **Out.** Queue/worker ownership.
- **FR.** Deterministic emission for a given tick sequence + requests; autosave cadence exact; priority total & stable. **Evidence.** E-G2-PF. **Tests.** `SaveSchedulerStep10`: autosave cadence, manual/admin/shutdown priority, deferred fires at tick, determinism. **Commit:** `docs(persistence): freeze Sprint-011 Step-10 spec (scheduler)`.

### Step-11 — `PersistenceManager`
- **Objective.** Own and orchestrate the subsystem per authoritative frame.
- **Scope — In.** `PersistenceManager.h`/`.cpp` (`core::IService` + `core::ITickable`): constructed with config, `snapshot::SnapshotQueue&` (read-only), `IPersistenceStore&`, and the owned version manager/scheduler/queue/worker; `RequestSave(SaveTrigger) -> PersistenceOutcome`; `Tick(double dt)` runs one deterministic pass — scheduler (tick) → enqueue → **Acquire** current snapshot → project → worker consume/persist → **Release** — with back-pressure honored; read-only `Statistics()`/`Consistency()`. Consumes the queue read-only as a **second consumer** (Acquire/Release). **Out.** Bootstrap wiring; diagnostics collector.
- **FR.** One synchronous pass per Tick; deterministic; read-only snapshot consumption; all value outcomes handled; never mutates authoritative state. **Evidence.** E-G1-PF/E-G3-PF. **Tests.** `PersistenceManagerStep11`: request→persist end-to-end with fakes, autosave via Tick, back-pressure, read-only (snapshot released), determinism. **Commit:** `docs(persistence): freeze Sprint-011 Step-11 spec (manager)`.

### Step-12 — Error handling + recovery hardening (negative surface)
- **Objective.** Prove the isolation/bounded/recovery negatives as value outcomes.
- **Scope — In.** Extend `PersistenceTests.cpp`: worker failure isolation (simulation-state proxy unchanged); queue overflow + back-pressure; version mismatch rejection; interrupted save → abort + previous retained; storage unavailable; validation failure; a sustained-autosave churn stress with bounded memory + determinism. Minimal `.cpp` hardening only on a proven gap. **Out.** New behavior/interfaces.
- **FR.** Every rejection is a value outcome leaving state unchanged; determinism under stress; bounded memory. **Evidence.** E-G1-PF…E-G5-PF negatives. **Tests.** `PersistenceHardeningStep12`. **Commit:** `docs(persistence): freeze Sprint-011 Step-12 spec (hardening)`.

### Step-13 — `PersistenceDiagnostics` + statistics
- **Objective.** A pure, non-invasive collector + read-only inspectors.
- **Scope — In.** `PersistenceDiagnostics.h` (header-only): `Reset()`, `Snapshot() -> PersistenceStatistics` (immutable value), `RecordSaveRequest`, `RecordAutosave`, `RecordFailure`, `RecordSaveDuration(...)` (diagnostic timing), `RecordQueueDepth`, `RecordWorkerBusy`; deterministic aggregates (avg save time, max queue depth); read-only views (queue viewer, worker status, autosave monitor, version inspector, save history). Timing explicitly diagnostic. **Out.** Pipeline reference/mutation.
- **FR.** Non-invasive; deterministic aggregates; monotonic counters; immutable `Snapshot`; `Reset` restores initial. **Evidence.** none. **Tests.** `PersistenceDiagnosticsStep13`: record/snapshot, reset, monotonic, immutable copy, deterministic average. **Commit:** `docs(persistence): freeze Sprint-011 Step-13 spec (diagnostics)`.

### Step-14 — Composition-root wiring (`kPersistence = 500`)
- **Objective.** Wire persistence into Bootstrap after replication, at the reserved slot.
- **Scope — In.** `Bootstrap.cpp`: construct `PersistenceConfiguration` (from `serverConfig`), the store seam (in-memory/null in the default+test build; real backend is Sprint-012 behind the seam), the version manager/scheduler/queue/worker, and the `PersistenceManager` (referencing the SnapshotManager's `Queue()` read-only); register the manager and subscribe it at `tick_order::kPersistence = 500`; reverse-order teardown (unsubscribe before ShutdownAll; store outlives the manager); default `[persistence]` config block written to `server.cfg`; `vcxproj` registration. **Out.** Behavior change; new features.
- **FR.** Single subscriber at 500 (after Replication 450, before Networking 900); networking-last preserved; existing ordering/services unchanged; deterministic; reverse-order teardown. **Evidence.** E-G1-PF/E-G3-PF composed. **Tests.** `Bootstrap` assertions: manager present, subscribed at 500, subscriber count increments by exactly one, deterministic across frames, torn down on shutdown. **Commit:** `docs(persistence): freeze Sprint-011 Step-14 spec (wiring)`.

### Step-15 — Integration tests
- **Objective.** Composed-stack behavior with fakes.
- **Scope — In.** `PersistenceIntegrationStep15`: snapshot consumption (manager over a fake `SnapshotQueue`); worker execution; scheduler timing (autosave cadence across many Ticks); queue stability + back-pressure under burst; version compatibility (accept/reject); failure isolation (store failure → simulation proxy unaffected, previous save retained); sustained-autosave stress; determinism/replay (twin managers → identical saves + stats). **Out.** New behavior.
- **FR.** Deterministic; bounded; failure-isolated; read-only. **Evidence.** E-G1-PF…E-G5-PF composed. **Commit:** `docs(persistence): freeze Sprint-011 Step-15 spec (integration)`.

### Step-16 — Documentation (`Persistence.md`)
- **Objective.** Author the subsystem doc.
- **Scope — In.** `Multiplayer/docs/Persistence.md`: persistence lifecycle; the **synchronous-worker / no-thread** rationale (ADR-011, ReplicationWorker precedent, immutable handoff); scheduling policy; version management; validation process; the `IPersistenceStore` extension point for Sprint-012; frame placement at `kPersistence = 500`; determinism/governance; `[persistence]` config table. **Out.** Code/behavior.
- **FR.** Accurate to the delivered subsystem. **Evidence.** — . **Commit:** `docs(persistence): freeze Sprint-011 Step-16 spec (Persistence.md)`.

### Step-17 — Sprint closure (no code)
- **Objective.** Confirm DoD; freeze status; declare Sprint-012 readiness.
- **Scope — In.** Cross-check §11 acceptance criteria and §12 DoD against delivered, verified artifacts; record the final baseline (`519 + Sprint-011 additions`); set `Sprint-011-Persistence.md` to Implemented / Verified / Closed / Frozen (status + closure section); update `CURRENT_STATUS.md`, `SESSION_LOG.md`, README textual progress; consistency scan. **Out.** Any code/test change (except a genuine closure defect); any Sprint-012 work.
- **FR.** DoD satisfied & recorded; status consistent; Sprint-011 declared Closed/Frozen; Sprint-012 readiness stated. **Evidence.** All E-G*-PF recorded. **Commit:** `docs(persistence): close Sprint-011`.

---

## Compatibility & non-regression checklist (every step)
- Sprint-010 **519/519** baseline preserved; new tests only add.
- No engine TU added; no OS/file/serialization code; no wire change; no thread created.
- Reserved `tick_order::kPersistence = 500` used; no new key; networking-last preserved.
- Read-only snapshot consumption; no authoritative mutation; ADR-007…ADR-011 preserved.

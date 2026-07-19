# Persistence Framework (Sprint-011)

STALKER-MP is host-authoritative and deterministic. The Persistence Framework is
the **host-side infrastructure that coordinates when and how safely authoritative
world snapshots are handed off for saving** — it deliberately implements *no* save
format and *no* serialization (those are Sprint-012). It consumes the immutable
Sprint-008 snapshot stream read-only, schedules saves, validates and versions them,
and hands them to a storage seam. It never mutates authoritative simulation state
(Host Authority; ADR-008), creates no thread (ADR-011), and adds no OS/file code
(One Platform Boundary).

## Where it runs in the frame

The `PersistenceManager` is a single `core::IService` + `core::ITickable`
subscribed to the one frame dispatcher at the **reserved** `tick_order::kPersistence
= 500` — strictly after Snapshot (400) and Replication (450), before Networking
(900, networking-last). No new ordering key was introduced. Each frame runs exactly
one synchronous pass; the real-time `dt` is not used in control flow (the path is
tick-driven — Replay Determinism).

```
FrameDispatcher::Dispatch(dt):
  … World(100) … Snapshot(400) → Replication(450) → Persistence(500) … Networking(900)
```

## Components (engine-free core, `persistence::`)

| Component | Role |
| --- | --- |
| `PersistenceTypes.h` | Value vocabulary: `PersistenceOutcome`, `SaveTrigger`, `SaveState`, `SaveRequest`, `SaveMetadata`, `PersistenceStatistics`, `PersistenceConsistency`. |
| `PersistenceConfiguration` (+ `FromConfig`) | `[persistence]` config: queue depth, autosave interval, retries, backoff, back-pressure watermarks, version. |
| `VersionManager` | Tracks persistence/world/schema/compatibility versions; classifies a candidate as Equal / MigrationRequired / Incompatible. |
| `SaveMetadataBuilder` | Deterministic `SaveMetadata` (tick, counts, versions, checksum) from an immutable snapshot view. |
| `PersistenceSnapshot` | Read-only projection over `snapshot::ISnapshotView` (Preserve Before Replace — no new capture). |
| `ValidationFramework` | Pure validators: integrity, completeness, version, queue ordering → value outcomes. |
| `PersistenceQueue` | Bounded FIFO of save requests with hysteresis back-pressure; publish/acquire/release/retry. |
| `IPersistenceStore` (+ `InMemory`/`Null`) | Engine-free storage seam (Begin/Write/Commit/Abort). Real filesystem backend = Sprint-012. |
| `PersistenceWorker` | **Synchronous** (no thread): Consume → build metadata → Persist; failure-isolated (Abort → retain previous). |
| `SaveScheduler` | Deterministic tick-driven scheduling: manual / autosave / administrative / shutdown / deferred. |
| `PersistenceManager` | Owns the above; one synchronous Tick pass; consumes the SnapshotQueue read-only. |
| `PersistenceDiagnostics` | Pure, non-invasive collector: monotonic counters + deterministic aggregates. |

## Persistence lifecycle (one `Tick` pass)

1. **Schedule** — `SaveScheduler::Tick(currentTick)` returns at most one due
   `SaveRequest` (priority: Shutdown > Administrative > Manual > Deferred >
   Autosave), or nothing.
2. **Enqueue** — the request is published to the bounded `PersistenceQueue` (a full
   queue is a `QueueFull` value outcome; back-pressure engages/releases on the
   watermarks).
3. **Acquire** — the manager `Acquire()`s the current published snapshot from the
   Sprint-008 `SnapshotQueue` (read-only, reference-counted). Without one, queued
   requests wait for a later tick.
4. **Persist** — `PersistenceWorker::Flush` drains the queue: for each request it
   validates (`Consume`), builds `SaveMetadata`, and persists through the store
   (`Begin → Write → Commit`). A failed request is re-enqueued for a later pass.
5. **Release** — the snapshot reference is released.

## Worker architecture — synchronous, no thread (D-PF1 / ADR-011)

Although `09_Threading.md` §11 describes a conceptual "Persistence Worker thread,"
the Version-1 realization is **synchronous and creates no thread**, exactly as the
Sprint-009 `ReplicationWorker` did. Independence from simulation is achieved by
consuming an **immutable snapshot** (immutable handoff), not by concurrency. There
are no `std::thread`, `std::mutex`, `std::atomic`, `std::async`, `std::future`, or
`condition_variable` anywhere in the subsystem. A future sprint may move the worker
onto a real thread behind the same snapshot seam without changing this design.

**Failure isolation (retain-previous).** Any `Begin`/`Write`/`Commit` failure
`Abort`s the transaction, so the previously committed save is retained untouched and
the authoritative World is never affected. Validation rejections short-circuit
before the store is touched. Every failure is a `PersistenceOutcome` value outcome.

## Scheduling policy

`SaveScheduler` is a pure function of the tick sequence and the registered requests
(no wall-clock in control flow). One-shot triggers (Manual/Administrative/Shutdown)
are serviced in priority order, one per tick; a Deferred save fires on the first
tick at/after its target; periodic autosave is emitted every
`autosave_interval_ticks` (0 disables it). A higher-priority request suppresses the
autosave for that tick; the autosave fires on the next eligible tick. The manager
exposes `RequestSave(trigger)` (Manual/Administrative/Shutdown) and
`DeferSave(untilTick)`.

## Version management

`VersionManager` compares a candidate `VersionSet` (persistence / world / schema /
compatibility) against the build. The `compatibility` field is the hard gate: a
differing compatibility boundary is `Incompatible`; a matching boundary with any
other version difference is `MigrationRequired`; identical sets are `Equal`.
`ValidationFramework::ValidateVersion` treats Equal and MigrationRequired as
compatible and Incompatible as `VersionMismatch`. At **save** time the candidate is
the current build, so version validation is trivially satisfied; the full version
gate (and migration execution) is a **load** concern for Sprint-012.

## Validation process

`ValidationFramework` provides pure validators, each returning a value outcome:
integrity (real snapshot id; header counts agree with the containers),
completeness (the snapshot is sealed — Finalized/Published), version
(reconcilable with the build), and queue ordering (strictly ascending save ids,
non-decreasing ticks). `SaveMetadata.checksum` is a deterministic 64-bit FNV-1a
fold over the snapshot's content values (entities/players in their fixed ascending
order, environment, tick, counts, versions), explicitly **excluding** the diagnostic
wall-clock and build-duration — identical content always yields the identical
checksum.

## Storage seam & extension point (One Platform Boundary)

`IPersistenceStore` is the engine-free storage boundary
(`Begin(SaveMetadata) → handle`, `Write(handle, PersistenceSnapshot)`,
`Commit(handle)`, `Abort(handle)`) with **no serialization semantics defined in
Sprint-011**. It ships with an in-memory store (records accepted saves; the
default/test backend) and a null store. **Sprint-012 adds the real filesystem
backend behind this frozen seam** — the save-file format, world serialization, the
load/restore pipeline, and compression all live there, not here. This keeps the
platform boundary intact: Sprint-011 contains no OS/file/socket code.

## Configuration (`[persistence]`, server.cfg)

| Key | Default | Meaning |
| --- | --- | --- |
| `queue_depth` | 16 | Max pending save jobs (bounded; ≥ 1). |
| `autosave_interval_ticks` | 0 | Periodic autosave interval; 0 = disabled. |
| `max_retries` | 3 | Max retries for a failed save (policy; manager/scheduler). |
| `retry_backoff_ticks` | 30 | Delay between a failed save and its retry. |
| `backpressure_high_watermark` | 12 | Depth at which back-pressure engages (0 disables). |
| `backpressure_low_watermark` | 4 | Depth at which back-pressure releases (≤ high ≤ queue_depth). |
| `version` | 1 | Persistence format version (≥ 1). |

## Determinism & governance

- **Deterministic / Replay Determinism** — scheduling, metadata, checksum,
  validation, and versioning are pure and tick-derived; identical snapshots +
  config + tick sequence yield identical decisions and identical saves; wall-clock
  appears only in explicitly-diagnostic fields.
- **Host Authority / ADR-008** — read-only snapshot consumption; a failed save
  retains the previous and never affects the World; authoritative state is never
  mutated.
- **One Engine Boundary / One Platform Boundary** — the subsystem is engine-free
  and adds no engine TU and no OS/file code; storage is behind the engine-free
  `IPersistenceStore` seam.
- **ADR-007** — no exceptions, no RTTI, no iostream; all fallible operations return
  `PersistenceOutcome` value outcomes; the queue is bounded (no unbounded growth).
- **ADR-011** — no thread created; one synchronous pass per frame at the reserved
  `tick_order::kPersistence = 500`; networking-last preserved.

## Evidence gates

E-G1-PF (read-only immutable-snapshot consumption; no authoritative mutation),
E-G2-PF (deterministic scheduling/metadata/validation/versioning), E-G3-PF (worker
independence & failure isolation; no thread), E-G4-PF (bounded queue / back-pressure
/ robust value-outcome validation), E-G5-PF (One Platform + One Engine Boundary
preserved).

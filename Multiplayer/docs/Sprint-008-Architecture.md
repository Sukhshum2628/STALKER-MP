# Sprint-008 — Snapshot System — Architecture

- **Status:** Architecture (draft — awaiting review & approval before Implementation Plan / Step Specs). Architecture phase only; no implementation, no step specifications, no code.
- **Governance:** Conforms to ADR-007, ADR-008, ADR-009, ADR-010, ADR-011 (none superseded; none modified here). Governed by ADR → Architecture → Sprint Architecture → Implementation Plan → Step Specs → Implementation → Independent Verification.
- **Sprint definition:** `Documentation/SPRINTS/Sprint-008-Replication-Snapshots.md` (scope authority). This is the *architecture* for that sprint; it introduces no decision contradicting an Accepted ADR or a frozen sprint.
- **Consumes (frozen, unchanged):** Sprint-002 World (`FrameDispatcher`, `IWorldQueries`/`ISpatialQueries`, `WorldContext`, `IEnvironmentSource`/`EnvironmentService`), Sprint-003 Entity Registry (read-only identity/metadata), Sprint-004 Bubble (activation membership), Sprint-005 ALife Transition (`TransitionResult`), Sprint-006 Host Networking (untouched — snapshots are not packets), Sprint-007 Player Lifecycle (read-only player surface + position feed).

---

## 1. Sprint objective

Introduce the **immutable Snapshot System**: the single, deterministic mechanism by which the authoritative simulation exposes a consistent, read-only copy of world state to asynchronous consumers (replication, persistence, replay, diagnostics — all future). The Simulation (frame) thread remains the sole owner of live gameplay state; whenever an external system needs world state it receives an **immutable snapshot**, never a live object or raw pointer. Sprint-008 delivers the snapshot value types, the builder (sim-thread only), the immutable aggregate, an exception-free memory pool, a multi-consumer-safe queue, the manager/service integration at the reserved `kReplication (400)` tick slot, diagnostics, and validation — **snapshots only** (no replication, persistence, delta, threads, or packets).

## 2. Problem statement

After Sprint-007 the authoritative simulation is complete (world, entities, ALife transitions, persistent players), but every consumer that wants world state must read live subsystem objects directly on the frame thread. That is unsafe for any asynchronous consumer (a future replication or persistence worker cannot touch live simulation objects without races), and it couples each consumer to each subsystem's internal representation. Replication (Sprint-009), Persistence (Sprint-011), and Replay/Diagnostics need a **stable, immutable, versioned** view produced once per tick, decoupled from live objects. Sprint-008 builds exactly that handoff — the immutable snapshot and its safe lifecycle — without yet building any consumer.

## 3. Scope

In scope: engine-free snapshot value types (`SnapshotId`, `SnapshotMetadata`, `EntitySnapshot`, `PlayerSnapshot`, `EnvironmentSnapshot`, `SnapshotStatistics`, `SnapshotConsistency`, enums); the immutable `SimulationSnapshot` aggregate + the read-only `ISnapshotView` base interface; `SnapshotConfiguration` (+ `FromConfig`); the `SnapshotBuilder` (sim-thread `BeginBuild`→`Capture`→`Finalize`); a new engine-free capture seam `world::IEntitySnapshotSource` (richer per-entity capture) with its single concrete adapter in `EngineAdapters.cpp` and a null test counterpart; the exception-free `SnapshotPool` (fixed-capacity buffer reuse, intrusive ref-count); the `SnapshotQueue` (`Publish`/`Acquire`/`Release`, multi-consumer-safe); the `SnapshotManager` (`IService` + `ITickable`) at `tick_order::kReplication = 400`; composition-root wiring; `SnapshotDiagnostics`; validation/integrity checks; unit + integration tests; the subsystem doc.

## 4. Explicit out-of-scope items (non-goals)

Not in this sprint (and not depended upon): replication logic, prioritization, or bandwidth shaping (Sprint-009); delta/interest/compression (Sprints 009–010); client prediction/interpolation (Sprint-010); save-game/durable serialization or a persistence format (Sprints 011–012); **network packets or any wire encoding** (snapshots are in-process values, not `MessageId`s — ADR-010 untouched, no new message ids); **thread creation or scheduling** (ADR-011 — the queue is *designed* multi-consumer-safe but Sprint-008 spawns no worker thread and exercises it single-threaded); any engine-state mutation (ADR-008 — snapshots are read-only); replay recording/playback; and materialized `ReplicationSnapshot`/`PersistenceSnapshot`/`ThreadSnapshot` *content* (their derived projections are reserved extension points, defined by the base interface, materialized by their owning sprints — see §22).

## 5. Functional requirements

FR-1 Once per simulation tick the `SnapshotBuilder` produces one `SimulationSnapshot` capturing entities, players, and environment for that tick. FR-2 A snapshot is **immutable after `Finalize()`**: no public API mutates a finalized/published snapshot; consumers see only `const` access. FR-3 Snapshots capture **values only** — never a raw pointer or a handle into live simulation. FR-4 Each snapshot carries metadata (`SnapshotId` monotonic, simulation tick, version, entity/player counts, build duration). FR-5 The `SnapshotQueue` publishes finalized snapshots and lets multiple consumers `Acquire`/`Release` them safely; a snapshot is retired (returned to the pool) only after its last consumer releases it. FR-6 Memory is drawn from a fixed-capacity `SnapshotPool` with buffer reuse — no per-snapshot heap allocation in steady state, and no throwing allocation (ADR-007). FR-7 Building reads the authoritative simulation through existing engine-free seams plus the new `IEntitySnapshotSource`; it never mutates any subsystem. FR-8 Validation detects missing entities, invalid handles, duplicate entries, version mismatch, corruption, and any mutable-access attempt. FR-9 The builder executes **only on the frame (simulation) thread**, at `tick_order::kReplication = 400`, strictly after all simulation producers (World/EntityRegistry/Player/Bubble/Transition) have updated for the tick. FR-10 Publication is deterministic: identical simulation state + tick yields identical snapshot content and a monotonically increasing `SnapshotId`.

## 6. Non-functional requirements

NFR-1 **Deterministic & replay-safe:** identical (simulation state + tick) → identical snapshot content and id ordering; wall-clock appears only in a diagnostic metadata field excluded from replay identity. NFR-2 **Engine-free core:** every type and the builder are engine-free; the sole engine contact is the `EngineEntitySnapshotSource` in `EngineAdapters.cpp` (One Engine Boundary). NFR-3 **Exception-free / RTTI-free** (ADR-007): value outcomes, no throwing allocation on the build/publish/acquire path, `/W4 /WX`, C4530-clean. NFR-4 **Single-threaded execution** (ADR-011): built and published on the frame thread; the queue is concurrency-*ready* (exception-free intrusive ref-count + a documented producer/consumer contract) but no thread is created in this sprint. NFR-5 **Bounded memory:** pool capacity and per-snapshot element caps are configured; steady-state allocations are reused. NFR-6 **No OS/platform header** enters the subsystem (One Platform Boundary, ADR-009). NFR-7 **Additive-only:** no frozen interface of Sprints 002–007 is modified; the new `IEntitySnapshotSource` is additive.

## 7. System responsibilities

The subsystem is responsible for: capturing a per-tick immutable copy of entity/player/environment state into a `SimulationSnapshot`; enforcing immutability after finalize; allocating snapshot storage from a bounded, exception-free pool; publishing finalized snapshots to a multi-consumer-safe queue and retiring them when released; exposing a read-only `ISnapshotView` to consumers; validating snapshot integrity; and reporting read-only diagnostics. It is **not** responsible for: producing or mutating simulation state (Sprints 002–007 own it); transforming snapshots into network updates (Sprint-009); serializing snapshots to disk (Sprints 011–012); or running any worker thread (future).

## 8. High-level architecture

A single `SnapshotManager` (`IService` + `ITickable`) is owned by the composition root and subscribed to the one `FrameDispatcher` at the reserved `tick_order::kReplication = 400`, strictly after every simulation producer (World 100 → EntityRegistry 200 → Player 250 → Bubble 300 → ALifeTransition 350 → **Snapshot 400**) and before Persistence (500)/Networking (900). Each tick the manager runs the lifecycle: `BeginBuild` (acquire a pooled buffer, stamp metadata) → `Capture` (read entities via `IEntitySnapshotSource`, players via the Sprint-007 read-only surface, environment via the Sprint-002 `IEnvironmentSource`) → `Finalize` (seal — the snapshot becomes immutable) → `Publish` (hand the finalized snapshot to the `SnapshotQueue`). Consumers (all synchronous stand-ins in Sprint-008; real workers later) `Acquire` a `const` handle, read it, and `Release`; the queue returns a snapshot to the pool when its intrusive ref-count reaches zero after it has been superseded. Networking remains last (900) and unaffected — snapshots are in-process, not packets.

Data-flow (one tick, on the frame thread):
`[400] SnapshotManager.Tick → SnapshotBuilder: BeginBuild(pool) → Capture(IEntitySnapshotSource + PlayerManager + IEnvironmentSource) → Finalize (immutable) → SnapshotQueue.Publish` → *(async, future)* `worker.Acquire → read → Release`.

## 9. Component decomposition

- `snapshot::SnapshotTypes` — value types/enums: `SnapshotId` (monotonic `u64`, 0 = none), `SnapshotKind` (Simulation / Replication / Persistence / Thread — only Simulation is materialized here; the others are reserved), `SnapshotState` (Building / Finalized / Published / Retired), `SnapshotMetadata`, `EntitySnapshot`, `PlayerSnapshot`, `EnvironmentSnapshot`, `SnapshotStatistics`, `SnapshotConsistency`. Engine-free, POD-style.
- `snapshot::ISnapshotView` — the read-only base interface (shared base, §7.3): const accessors for metadata, entities, players, environment. Consumers depend only on this.
- `snapshot::SimulationSnapshot` — the immutable aggregate implementing `ISnapshotView`; built into a pooled buffer, sealed at `Finalize` (a `Building→Finalized` state gate; public API is const-only once finalized).
- `snapshot::SnapshotConfiguration` (+ `FromConfig`) — pool capacity (number of buffers), max entities/players per snapshot, snapshot format version, build-budget (diagnostic), queue depth.
- `snapshot::SnapshotBuilder` — engine-free; `BeginBuild`/`Capture`/`Finalize`; sim-thread only; consumes the capture seams; no ownership of the buffer beyond the build (the pool owns it).
- `world::IEntitySnapshotSource` — **new engine-free seam**: richer per-entity capture (`EntitySnapshot`: `EntityId`, transform, velocity, state, flags, inventory reference, runtime state) as value data. One concrete `adapters::EngineEntitySnapshotSource` in `EngineAdapters.cpp`; `adapters::NullEntitySnapshotSource` (test build) + `CreateEngineEntitySnapshotSource()` factory. Mirrors the Sprint-002 `IEntitySource` / Sprint-007 `IPlayerSpawnGateway` seam precedent.
- `snapshot::SnapshotPool` — exception-free fixed-capacity buffer pool with an intrusive ref-count per buffer; `Acquire`/`ReturnToPool`; pre-reserved at Initialize; no throwing allocation in steady state; capacity-exhaustion is a value outcome.
- `snapshot::SnapshotQueue` — `Publish(finalized)`, `Acquire() → const handle`, `Release(handle)`; multi-consumer-safe (intrusive ref-count; single-producer/multi-consumer contract documented for future threads); retires a superseded snapshot when its ref-count hits zero. No thread created.
- `snapshot::SnapshotManager` — `IService` + `ITickable`; owns the `SnapshotBuilder`, `SnapshotPool`, and `SnapshotQueue`; ticked at `kReplication = 400`; `Dependencies() = {World, EntityRegistry, BubbleManager, TransitionManager, PlayerManager}` (ordering-only const edges).
- `snapshot::SnapshotDiagnostics` — read-only `Statistics` / `DescribeState` / `DumpSnapshot` / `BuildHistory` / `QueueStatus` / `MemoryUsage` / `ValidateConsistency` (iostream-free).

## 10. Public interfaces (conceptual only)

Conceptual contracts (no signatures frozen here):

- `ISnapshotView` (const-only): `Metadata()`, `Entities()` (ascending `EntityId`), `Players()` (ascending `PlayerId`), `Environment()`. The sole consumer-facing surface.
- `SnapshotBuilder`: `BeginBuild(pool, tick) → Expected<handle>`; `Capture(entitySource, playerSurface, environmentSource)`; `Finalize() → Expected<const SimulationSnapshot*>` (seals immutability).
- `world::IEntitySnapshotSource` (engine-free seam): `Capture(vector<EntitySnapshot>& out)` (append-only, ascending, value data; no engine type).
- `SnapshotPool`: `Acquire() → Expected<buffer>`; `ReturnToPool(buffer)`; `Reserve(capacity)`; `InUse()/Capacity()`.
- `SnapshotQueue`: `Publish(const SimulationSnapshot*)`; `Acquire() → const SimulationSnapshot*` (ref-count++); `Release(const SimulationSnapshot*)` (ref-count--); `Depth()/Published()`.
- `SnapshotManager`: `IService`/`ITickable`; read-only `LatestMetadata()`, `Statistics()`, `ValidateConsistency()`, `Queue()` (const).

## 11. Data ownership

| Data | Owner |
|---|---|
| Live gameplay/simulation state | Sprints 002–007 subsystems (unchanged) — the sole owners of live state |
| Per-entity engine object data | Engine, marshaled to `EntitySnapshot` values only inside `EngineAdapters.cpp` |
| **Snapshot buffers (storage)** | **`snapshot::SnapshotPool` (Sprint-008)** |
| **Published immutable snapshots** | **`snapshot::SnapshotQueue` (Sprint-008)** until retired to the pool |
| Snapshot metadata / statistics | The snapshot itself / `SnapshotManager` (read-only) |
| Consumer references to a snapshot | Non-owning `const` handles (ref-counted for lifetime) |

Snapshots own only *copies* of state; they never own or reference live simulation objects. The producer (frame thread) is the sole writer; consumers hold non-owning `const` handles whose lifetime is governed by the queue's ref-count.

## 12. Lifetime and ownership model

`SnapshotManager` is created in `Bootstrap.cpp` after the Transition service, owned by the `ServiceRegistry`, constructed with `SnapshotConfiguration`, const references to the World/EntityRegistry/Player read surfaces, and the `IEntitySnapshotSource` from `CreateEngineEntitySnapshotSource()` (Runtime-owned; null in tests). It subscribes to the `FrameDispatcher` at 400 in `Initialize` and reserves the pool. A snapshot's lifetime: pooled-buffer `Acquire` at `BeginBuild` → `Building` → `Finalized` (immutable) → `Published` (in the queue, ref-countable) → `Retired` (ref-count 0 and superseded → buffer returned to the pool for reuse). Teardown is reverse-order: the manager unsubscribes from the `FrameDispatcher` before `ShutdownAll`; the queue drains and returns all buffers to the pool; no snapshot outlives the pool.

## 13. Determinism considerations

Building runs at a fixed tick slot (400) after all simulation mutation (100–350), reading a consistent per-tick view; `SnapshotId` is monotonic and tick-derived; entities and players are captured in ascending id order; environment is the Sprint-002 per-tick value. Consequently identical (simulation state + tick) sequences produce byte/field-identical snapshot **content** and identical id ordering — the Sprint-008 replay property (E-G2-S). The only non-deterministic field is `SnapshotMetadata.timestampWallClock`, which is a **diagnostic** value explicitly excluded from replay identity and from any future replicated/persisted content; deterministic identity is `(SnapshotId, simulationTick, content)`.

## 14. Threading model

Single-threaded in Sprint-008: build, finalize, and publish all run on the frame thread inside the `kReplication = 400` tick; consumers in this sprint are synchronous stand-ins. No thread is created (ADR-011 preserved). The queue and pool are nonetheless designed for the future single-producer/multi-consumer model: an exception-free intrusive ref-count governs snapshot lifetime, and the producer/consumer memory-visibility contract (publish-before-acquire, release-before-retire) is documented so a future sprint can introduce worker threads *behind this seam* without reshaping the API. RFC-0006 worker threads never enter frame dispatch; they will consume only published immutable snapshots.

## 15. Engine interaction boundaries

One Engine Boundary preserved: the **only** engine-touching code added is `adapters::EngineEntitySnapshotSource` in `EngineAdapters.cpp`, reached through the engine-free `world::IEntitySnapshotSource`. It reads engine object transform/velocity/state/flags on demand and marshals them into engine-free `EntitySnapshot` values; it retains no engine pointer and mutates nothing (ADR-008 untouched — snapshots are read-only observation, the Sprint-002 observation discipline). Players are captured via the Sprint-007 read-only surface (already engine-free); environment via the Sprint-002 `IEnvironmentSource` (already engine-free). No new engine mutation path is introduced.

> **Clarification (review-approved) — `world::IEntitySnapshotSource` is a scoped, snapshot-specific seam.** It is **additive**: it supplements — and does **not** replace, widen, or deprecate — the existing Sprint-003 Entity Registry or any Sprint-002/003 read-only entity interface (`IEntitySource`, `IWorldQueries`, `ISpatialQueries`, `EntityView`), all of which remain the canonical entity-access APIs and are unchanged. Its sole purpose is to **marshal the richer per-entity value data required to construct a snapshot** (`EntitySnapshot`: transform, velocity, state, flags, inventory reference, runtime state) that the existing views do not carry. It is consumed **only** by the `SnapshotBuilder` and MUST NOT become a general-purpose entity-access API for other subsystems: future subsystems needing entity data use the established Entity Registry / query seams (or consume a published immutable snapshot), never this capture seam. If a future need arises to broaden entity access, it is addressed through those existing interfaces (or a new, separately-justified ADR), not by repurposing `IEntitySnapshotSource`.

## 16. Networking boundaries

None crossed. Snapshots are **in-process immutable values**, not packets: no transport, no wire header, no endianness, no `MessageId` (ADR-009/010 untouched). Networking remains last in the frame (900) and is not modified. Sprint-009 replication will *consume* published snapshots and encode them onto the wire — that transformation and any message ids belong to Sprint-009, not here.

## 17. Persistence boundaries

None crossed. Sprint-008 defines no save-game format, database, or durable serialization. The `PersistenceSnapshot` projection is a reserved extension point (§22) materialized by Sprints 011–012; the immutable `SimulationSnapshot` is deliberately a clean, value-only capture from which a future persistence sprint can serialize without redesign.

## 18. Failure handling

Every fallible operation degrades benignly as a value outcome, never an exception or partial publish. Pool exhaustion (no free buffer) → a `PoolExhausted` outcome and the tick's build is skipped (the previous published snapshot remains valid) — never a throw or a blocking wait. A capture over the configured entity/player cap → truncation is refused with an `Overflow` outcome (bounded memory) rather than an unbounded allocation. A finalize on an already-finalized buffer, or a mutate attempt on a finalized snapshot, is impossible via the public API and flagged by validation. A `Release` of an unknown/ already-retired handle is a benign no-op. Teardown with published snapshots drains the queue and returns all buffers to the pool (no leak).

## 19. Error model

Conforms to ADR-007: no exceptions, no RTTI. Fallible calls return `core::Expected<T>`; lifecycle stages return value outcomes (`BuildOutcome`, `PublishOutcome`, `AcquireOutcome`). No throwing allocation on the build/publish/acquire path — the pool is pre-reserved and reuses buffers; a would-be allocation failure surfaces as `PoolExhausted`. Diagnostics use `common::Format` (iostream-free). Assertions guard internal invariants only, never validate external/consumer input.

## 20. Diagnostics strategy

`SnapshotDiagnostics` is read-only and replay-stable (except the explicitly-diagnostic wall-clock/timing fields): `Statistics()` (snapshots built/published/retired, current queue depth, pool in-use/capacity, last build duration, entity/player counts), `DescribeState()`, `DumpSnapshot(SnapshotId)`, `BuildHistory()` (recent builds, tick-indexed), `QueueStatus()`, `MemoryUsage()` (pool utilization), and `ValidateConsistency()` (ordering, id monotonicity, no duplicate ids, counts match, no retired-buffer aliasing, immutability flag set on published). Bandwidth/latency/worker fields are reserved (unpopulated) for later sprints.

## 21. Testing strategy

(Strategy only; the suite is authored in the implementation phase.) The engine-free core is fully unit-testable against `NullEntitySnapshotSource`, a fake player surface, a fake environment source, and the real pool/queue — no engine, no OS, no threads. Coverage targets: snapshot creation; **immutability** (a finalized snapshot exposes no mutator; a mutate attempt fails to compile / is validation-flagged); entity/player/environment capture correctness; pool **buffer reuse** (no steady-state allocation; capacity exhaustion → `PoolExhausted`); queue publish/acquire/release with **multiple synchronous consumers** and correct ref-count retire; **determinism/replay** (identical state+tick twice → identical snapshot content + id order); large-world and churn stress; validation negatives (missing/duplicate/version-mismatch/corruption). Integration (behind seams): World/Registry/Player captured correctly through the composed stack; queue operational; multiple consumers read the same published snapshot safely. Independent verification (Antigravity) covers the engine capture path on Windows.

## 22. Extension points

Reserved, not implemented: `ReplicationSnapshot` (Sprint-009 — a prioritized, delta-able projection/view over `ISnapshotView`); `PersistenceSnapshot` (Sprints 011–012 — a durable serialization projection); `ThreadSnapshot` / worker-thread consumers (a future threading sprint — the queue's single-producer/multi-consumer contract is the seam); memory-pool growth policy; `SnapshotDiagnostics` reserved bandwidth/latency/worker fields; snapshot **versioning** (the `version` metadata field enables forward-compatible consumers). No future producer `tick_order` value is assigned or depended upon (Sprint-008 occupies the frozen `kReplication = 400` slot only), and no non-goal (replication, persistence, delta, prediction, packets, threads) is implemented.

## 23. Risks

R-1 Excessive memory allocation / unbounded snapshots. R-2 Snapshot construction overhead exceeding the frame budget at 400. R-3 Queue backlog / snapshots never retired (ref-count leak). R-4 Snapshot corruption or a mutable-access escape (immutability not truly enforced). R-5 Version mismatch between producer and a future consumer. R-6 Hidden coupling: the builder reaching into a live subsystem object instead of a value seam. R-7 A latent threading assumption baked into the API that a future worker sprint cannot satisfy. R-8 Non-determinism leaking into snapshot content (e.g., wall-clock, hash-iteration order, pointer values). R-9 ADR-007 violation via throwing allocation (`shared_ptr`/`new`) on the hot path.

## 24. Mitigations

R-1 → fixed-capacity `SnapshotPool` + configured per-snapshot element caps; over-cap → `Overflow`. R-2 → build is a bounded, value-copy pass at a fixed slot; build duration is measured (diagnostics) and the tick can skip on `PoolExhausted` without stalling. R-3 → intrusive ref-count + retire-when-zero-and-superseded; `ValidateConsistency` and diagnostics surface non-retired buffers; teardown drains the queue. R-4 → immutability enforced structurally (const-only public API after `Finalize`; a `Building→Finalized` state gate) and checked by validation. R-5 → a `version` field in metadata; consumers check it (future). R-6 → the builder consumes only engine-free value seams (`IEntitySnapshotSource`, player surface, `IEnvironmentSource`); no live object crosses the seam; enforced by the engine-free test build. R-7 → the single-producer/multi-consumer contract + exception-free ref-count are designed now and documented; the API exposes only `Publish`/`Acquire`/`Release`. R-8 → ascending-id capture, monotonic `SnapshotId`, tick-derived content; wall-clock confined to a diagnostic field excluded from replay identity. R-9 → the pool pre-reserves buffers and reuses them; no `shared_ptr`/throwing `new` on build/publish/acquire; capacity exhaustion is a value outcome (ADR-007).

## 25. Acceptance criteria

(Architecture-level.) The architecture is acceptable when it: establishes the immutable `SimulationSnapshot` + read-only `ISnapshotView` base; a sim-thread-only `SnapshotBuilder` at the frozen `kReplication = 400` slot after all simulation producers; an exception-free bounded `SnapshotPool`; a multi-consumer-safe `SnapshotQueue` with ref-counted retire; the `SnapshotManager` service + Bootstrap wiring; a single new engine seam (`EngineEntitySnapshotSource` in `EngineAdapters.cpp`) preserving One Engine Boundary; captures values only (no raw pointers, no live objects); is deterministic/replay-safe with wall-clock excluded from identity; introduces no packets/message-ids/threads/persistence; and conforms to ADR-007/008/009/010/011 with no modification to any frozen sprint. §28 records the review as passed.

## 26. Dependencies on previous sprints

Sprint-002 (World / `FrameDispatcher` / `IEnvironmentSource` / queries), Sprint-003 (Entity Registry — identity/liveness + the engine feed the new capture seam extends), Sprint-004 (Bubble — activation membership, if captured), Sprint-005 (Transition — `TransitionResult` as a per-tick activation delta available to snapshot metadata), Sprint-006 (Networking — untouched; snapshots are not packets), Sprint-007 (Player Lifecycle — the read-only player surface + position feed captured into `PlayerSnapshot`). All consumed as frozen; none modified.

## 27. Future sprint dependencies

Sprint-009 (Replication Pipeline) consumes published immutable snapshots and transforms them into prioritized, bandwidth-efficient network updates (the `ReplicationSnapshot` projection + wire encoding + message ids all belong to Sprint-009). Sprints 010 (prediction/interpolation) build on replicated snapshots. Sprints 011–012 (Persistence) serialize the `PersistenceSnapshot` projection. A future threading sprint introduces worker threads that consume the queue behind the frozen single-producer/multi-consumer contract. No future sprint is pre-committed to a signature or a `tick_order` value by this document.

## 28. Architecture decision summary

AD-1 New engine-free `snapshot::` subsystem; a single `SnapshotManager` `ITickable`. AD-2 Occupies the frozen `tick_order::kReplication = 400` slot (Sprint-008's reserved slot), strictly after all simulation producers; no new tick key. AD-3 One immutable `SimulationSnapshot` + read-only `ISnapshotView` base; `Replication`/`Persistence`/`Thread` snapshots are reserved projections (not materialized here). AD-4 Values only — no raw pointer/live object ever enters a snapshot; the builder consumes engine-free value seams. AD-5 Exactly one new engine seam, `IEntitySnapshotSource`, concrete only in `EngineAdapters.cpp` (One Engine Boundary); ADR-008 untouched (read-only observation). AD-6 Exception-free bounded `SnapshotPool` (no `shared_ptr`/throwing allocation on the hot path) + intrusive ref-counted `SnapshotQueue` retire (ADR-007). AD-7 Single-threaded in Sprint-008 (ADR-011); the queue/pool are concurrency-ready behind a documented single-producer/multi-consumer contract, but no thread is created. AD-8 Deterministic/replay-safe content; wall-clock confined to a diagnostic metadata field excluded from identity. AD-9 No packets, message ids, persistence format, or replication logic (ADR-009/010 untouched). No new ADR is required; no existing ADR is superseded.

---

## Architecture Review (self-review)

- **Architectural consistency.** Mirrors the established shape (value types → engine-free core + store/service → single engine seam with null counterpart → read-only diagnostics), consistent with Sprints 003–007. Consistent.
- **ADR compliance.** ADR-007: engine-free core, no exceptions/RTTI, value outcomes, exception-free pool (no throwing allocation) — conformant. ADR-008: read-only observation only; no engine mutation — conformant. ADR-009: no OS/platform header — conformant. ADR-010: no wire format/message id (snapshots aren't packets) — conformant. ADR-011: single-threaded; no thread created; the queue is concurrency-ready behind a documented contract — conformant. No ADR modified/superseded.
- **Preserve Before Replace / Host Authority / One World / One ALife.** Snapshots *read* the one authoritative simulation and never mutate it; the frame thread stays the sole owner of live state; captures reuse existing engine-free seams (Preserve Before Replace). Preserved.
- **Determinism & replay safety.** Fixed tick slot after all mutation, monotonic tick-derived `SnapshotId`, ascending-id capture, wall-clock excluded from identity → replayable (§13). Safe.
- **Ownership boundaries.** Live state stays with Sprints 002–007; snapshot buffers in the pool; published snapshots in the queue; consumers hold non-owning ref-counted const handles. No overlap.
- **Engine boundary preservation.** One new engine TU path (`EngineEntitySnapshotSource` in `EngineAdapters.cpp`) behind an engine-free seam; test build uses the null. Preserved.
- **Platform boundary preservation.** No OS/socket contact; Sprint-006's One Platform Boundary untouched. Preserved.
- **Hidden coupling.** Two candidate couplings identified and resolved in-architecture: (a) **builder ↔ live objects** — resolved by the value-only `IEntitySnapshotSource` seam (no live object crosses); (b) **latent threading assumption** — resolved by defining the single-producer/multi-consumer contract + exception-free ref-count now while creating no thread (AD-7). A third candidate, **wall-clock in content**, is resolved by confining it to a diagnostic field (AD-8). No residual hidden coupling.
- **Future extensibility.** Reserved `Replication`/`Persistence`/`Thread` projections over `ISnapshotView`, the versioned metadata, and the queue contract provide clean extension points for Sprints 009–012 without reopening this architecture.

**Risks discovered during review and their resolution:** RR-1 *`kReplication` naming vs snapshot purpose* — the frozen `tick_order` key at 400 is named `kReplication` but reserved (comment) for Sprint-008; **resolved** by occupying that exact frozen slot without renaming it (renaming a relational key is out of scope), and documenting that the SnapshotManager holds the Sprint-008 slot (AD-2). RR-2 *ADR-007 vs `shared_ptr`-style ownership for multi-consumer lifetime* — **resolved** by a custom exception-free intrusive ref-count + pre-reserved pool, never throwing allocation on the hot path (AD-6, R-9). RR-3 *`SimulationSnapshot`/`ReplicationSnapshot`/`PersistenceSnapshot`/`ThreadSnapshot` (§7.3) vs the no-replication/no-persistence non-goals* — **resolved** by materializing only `SimulationSnapshot` + the `ISnapshotView` base and reserving the other projections for their owning sprints (AD-3, §22). RR-4 *richer entity capture (§7.6 velocity/flags) exceeds the existing `EntityView`* — **resolved** by the additive engine-free `IEntitySnapshotSource` seam marshaled in `EngineAdapters.cpp` (AD-5), preserving One Engine Boundary rather than widening a frozen type. All discovered risks are resolved within this architecture; none requires a new ADR.

---

## 29. Proposed implementation sequencing (step boundaries)

Engine-free-first; the single engine seam late; wiring last (the Sprints 005/006/007 shape). Each step is additive, leaves the tree buildable, and is independently verifiable.

1. **Snapshot value types** — `SnapshotTypes.h` (ids, enums, metadata, `EntitySnapshot`/`PlayerSnapshot`/`EnvironmentSnapshot`, statistics/consistency). Engine-free; types only.
2. **`SnapshotConfiguration` + `FromConfig`** — pool/element caps, version, queue depth (tick-derived budgets).
3. **Immutable `SimulationSnapshot` + `ISnapshotView`** — the aggregate + read-only base + the `Building→Finalized` immutability gate.
4. **`SnapshotPool`** — exception-free fixed-capacity buffer reuse + intrusive ref-count; capacity outcomes.
5. **`world::IEntitySnapshotSource` + `NullEntitySnapshotSource`** — the engine-free capture seam + inert test source (no engine code).
6. **`SnapshotBuilder`** — `BeginBuild`/`Capture`/`Finalize` over the seams + pool; engine-free; deterministic.
7. **`SnapshotQueue`** — `Publish`/`Acquire`/`Release`, ref-counted retire, multi-consumer (single-threaded exercised).
8. **`SnapshotManager`** — `IService` + `ITickable`; per-tick lifecycle; owns builder/pool/queue.
9. **Engine adapter** — `adapters::EngineEntitySnapshotSource` in `EngineAdapters.cpp` (the sole engine TU) + factory; test build keeps the null.
10. **Bootstrap wiring** — register `SnapshotManager` after the Transition service; subscribe at `tick_order::kReplication = 400`; reverse-order teardown; `BootstrapTests` subscriber-count + tick assertions.
11. **`SnapshotDiagnostics`** — read-only inspector/dump/memory/queue/build-history.
12. **Validation hardening** — integrity/immutability negative surface + large-world/churn stress + replay identity.
13. **Integration + documentation** — composed-stack integration checks + `Multiplayer/docs/Snapshots.md`.
14. **Documentation + sprint closure** — status-doc updates; declare readiness for Sprint-009.

## 30. Evidence gates

- **E-G1-S — Immutability enforcement (blocking).** A finalized/published snapshot exposes no mutating public API; a mutate attempt fails at compile time (const-only) and any residual path is validation-flagged. Validates FR-2/§18/§24 R-4.
- **E-G2-S — Deterministic build & publication (blocking).** Identical simulation state + tick, twice, yields identical snapshot content and identical monotonic `SnapshotId` ordering; wall-clock excluded. Validates §13.
- **E-G3-S — Exception-free bounded memory (blocking).** Sustained build/publish/retire reuses pooled buffers with no steady-state allocation and no throwing allocation; capacity exhaustion returns `PoolExhausted`. Validates FR-6/§19/ADR-007.
- **E-G4-S — Multi-consumer safety (non-blocking, confirmatory).** Multiple synchronous consumers `Acquire`/`Release` the same published snapshot with correct ref-counted retire and no corruption; no consumer touches live simulation. Sizes nothing architectural; confirms the queue contract holds ahead of a future threading sprint.

**Freeze rule (proposed):** E-G1-S, E-G2-S, E-G3-S are blocking; E-G4-S is non-blocking (confirmatory). Each resolves entirely behind the subsystem's seams and cannot reopen the architecture.

## 31. Acceptance criteria (sprint)

Sprint-008 is complete when: every published snapshot is immutable (E-G1-S); build/publication is deterministic and replay-identical (E-G2-S); memory is bounded and exception-free with buffer reuse (E-G3-S); multiple consumers read published snapshots safely with correct retire (E-G4-S); the `SnapshotManager` ticks once at `kReplication = 400` after all simulation producers, wired via Bootstrap with reverse-order teardown; One Engine Boundary holds (only `EngineEntitySnapshotSource` touches the engine) and One Platform Boundary is untouched; no packet/message-id/thread/persistence is introduced; ADR-007…ADR-011 are preserved; the full suite is green on GCC + MSVC (Release x64, zero new warnings, no regressions); the subsystem doc is written and the status docs synchronized.

---

## 32. Architecture Freeze

The Sprint-008 (Snapshot System) Architecture is **FROZEN** as of 2026-07-11, approved at architecture review with the single documentation clarification incorporated (§15 — `world::IEntitySnapshotSource` is an additive, snapshot-specific capture seam that supplements and does not replace or broaden the existing Entity Registry / read-only entity interfaces, exists solely to marshal richer per-entity value data for snapshot construction, and must not become a general-purpose entity-access API). No architectural redesign was required.

The self-review (§28) passed on every dimension and resolved all discovered risks (RR-1…RR-4) within the document. It conforms to ADR-007, ADR-008, ADR-009, ADR-010, and ADR-011; modifies no frozen sprint architecture, ADR, implementation plan, message allocation, verification rules, or previous step specification; and preserves Preserve Before Replace, Host Authority, One World, One ALife Simulation, Deterministic Simulation, One Engine Boundary, and One Platform Boundary.

This architecture is ready for the next phase: the **Sprint-008 Implementation Plan**.

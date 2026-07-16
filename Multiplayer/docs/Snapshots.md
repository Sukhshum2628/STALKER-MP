# Snapshot System (Sprint-008) — Subsystem Documentation

- **Status:** Implemented & Verified — Sprint-008 (closure is Step 14). Steps 1–13 delivered and independently verified.
- **Architecture:** `Multiplayer/docs/Sprint-008-Architecture.md` (FROZEN). This document records the delivered subsystem; it introduces no new decisions.
- **Governing ADRs:** ADR-007 (Engine ABI Contract — no exceptions/RTTI/iostream; `core::Expected<T>` / value outcomes), ADR-008 (Engine State Mutation Boundary — snapshots are read-only observation; nothing is mutated), ADR-009 (One Platform Boundary — untouched; snapshots are not transported here), ADR-010 (Wire Protocol Contract — untouched; a snapshot is **not** a packet and carries no `MessageId`), ADR-011 (Single-Threaded / Deterministic — no thread is created; the queue is concurrency-*ready* behind a documented single-producer / multi-consumer contract).

---

## 1. Purpose

The Snapshot System produces an **immutable, deterministic, value-only record of the simulation state once per tick** and hands it to asynchronous, read-only consumers (replication, persistence, replay, and — in a future threading sprint — worker threads) without ever exposing or retaining a live engine object. It decouples "what the world looked like at tick N" from the live, mutating simulation: consumers read a frozen snapshot while the next tick is already being built. It adds no gameplay behavior and mutates no engine or simulation state.

## 2. Ownership boundaries

| Data | Owner |
|---|---|
| Live engine objects / transforms / ALife state | Engine (observed read-only via the capture seam; never retained) |
| Per-entity captured **values** | `snapshot::EntitySnapshot` (value copy inside a `SimulationSnapshot`) |
| Per-player captured **values** | `snapshot::PlayerSnapshot` (from the Sprint-007 read-only `PlayerManager` surface) |
| Per-tick environment **values** | `snapshot::EnvironmentSnapshot` (from the Sprint-002 `IEnvironmentSource`) |
| Reusable snapshot buffers + intrusive ref-counts | `snapshot::SnapshotPool` |
| The per-tick build pass | `snapshot::SnapshotBuilder` |
| The published-snapshot handoff | `snapshot::SnapshotQueue` |
| The per-tick lifecycle + ownership of pool/builder/queue | `snapshot::SnapshotManager` |
| Read-only observability | `snapshot::SnapshotDiagnostics` |

The subsystem owns only captured **values** and their buffers; it never owns entity simulation state, player lifecycle, session mechanics, or activation logic, and it never writes to any of them.

## 3. Components

- `snapshot::SnapshotTypes.h` — value types/enums: `SnapshotId` (`0 = none`), `SnapshotKind` / `SnapshotState` (with `Name()` helpers), and the POD captures `SnapshotMetadata`, `EntitySnapshot`, `PlayerSnapshot`, `EnvironmentSnapshot`, plus `SnapshotStatistics` / `SnapshotConsistency`. Deterministic layout; wall-clock excluded from replay identity.
- `snapshot::SnapshotConfiguration` (+ `FromConfig`) — pool capacity, max entities/players, version, queue depth, build budget (ticks). Cross-field invariants: `poolCapacity >= 2`, `queueDepth <= poolCapacity`, `version >= 1`, `maxEntities >= 1`.
- `snapshot::SimulationSnapshot` + `snapshot::ISnapshotView` — the immutable snapshot and its const-only consumer interface (`Metadata` / `Entities` ascending by `EntityId` / `Players` ascending by `PlayerId` / `Environment`). A `Building → Finalized` gate makes every mutator reject once finalized.
- `snapshot::SnapshotPool` — a fixed-capacity, pre-reserved pool of reusable buffers with an exception-free intrusive per-buffer ref-count; deterministic lowest-free-index `Acquire`; `PoolExhausted` value outcome; no steady-state heap allocation.
- `world::IEntitySnapshotSource` + `adapters::EngineEntitySnapshotSource` (`EngineAdapters.cpp`) + `NullEntitySnapshotSource` (test build) + `adapters::CreateEngineEntitySnapshotSource()` — the additive, snapshot-specific capture seam (Architecture §15: consumed **only** by `SnapshotBuilder`). The engine adapter reads id + transform on demand into values; the engine-free `adapters::detail::AppendAscendingUnique` helper enforces the ascending-unique, id≠0, append-only ordering contract and is unit-tested on both toolchains.
- `snapshot::SnapshotBuilder` — the per-tick pass: `BeginBuild(pool, tick)` → `Capture(entitySource, playerSurface, environmentSource)` → `Finalize()`. Values only; ascending entity/player order; `PoolExhausted` and `Overflow` are value outcomes; a failed `Capture` returns the pooled buffer (no orphan).
- `snapshot::SnapshotQueue` — the latest-wins, single-producer / multi-consumer handoff: `Publish` / `Acquire` / `Release` / `Depth` / `Published`. Lifetime is delegated to the pool's ref-count; a superseded snapshot is retired only after it is replaced **and** its ref-count reaches zero.
- `snapshot::SnapshotManager` — the `IService` + `ITickable` umbrella; owns the pool/builder/queue; `Initialize` reserves the pool; `Tick` runs `BeginBuild → Capture → Finalize → Publish` exactly once at `tick_order::kReplication = 400`; exposes the read-only surface (`LatestMetadata` / `Statistics` / `ValidateConsistency` / `Queue` / `LastTick`).
- `snapshot::SnapshotDiagnostics` — read-only `Statistics` / `DescribeState` / `DumpSnapshot` / `BuildHistory` / `QueueStatus` / `MemoryUsage` / `ValidateConsistency` (iostream-free via `common::Format`).

## 4. Per-tick pass (`tick_order::kReplication = 400`)

The `SnapshotManager` is the single `ITickable` at the reserved replication slot, subscribed by the composition root strictly after every simulation producer (World 100 → EntityRegistry 200 → PlayerLifecycle 250 → Bubble 300 → Transition 350 → **Snapshot 400** → … → Networking 900). Each frame it acquires a pooled buffer, captures the completed simulation state as values (entities via the capture seam, players via the Sprint-007 read-only surface, environment via the Sprint-002 source), finalizes the buffer, and publishes it — **exactly one publication per tick**. On any value outcome (`PoolExhausted` when no buffer is free, `Overflow` when a cap is exceeded, or a finalize failure) the tick is skipped and the previously published snapshot remains valid. No exception is thrown and no partial publish occurs.

## 5. Immutability & lifecycle

A snapshot is built in a `Building` state and sealed with `Finalize()` into a `Finalized` state; after that, every mutating operation is rejected as a value outcome (`E-G1-S`). Consumers see only `ISnapshotView` (const). A published snapshot is replaced latest-wins; a consumer that acquired an older snapshot keeps it alive until it releases, and the buffer is retired (returned to the pool) only when its ref-count reaches zero.

## 6. Memory model

All buffers are pre-reserved once at `Initialize` (`SnapshotPool::Reserve(poolCapacity)`); steady-state `Acquire` / `AddRef` / `ReturnToPool` never allocate — they flip flags and adjust counts. Capacity exhaustion is a `PoolExhausted` value outcome (bounded memory), never a throw or blocking wait. A capture over the configured entity/player cap is refused with `Overflow` rather than an unbounded allocation. Teardown drains the queue and returns all buffers to the pool (no leak); the manager owns pool/builder/queue, so reverse-order shutdown destroys them together.

## 7. Boundaries

- **Engine boundary (ADR-008/009).** The only engine-touching code is `adapters::EngineEntitySnapshotSource` in `EngineAdapters.cpp` (the sole engine TU). It performs read-only observation, marshals values, retains no `CObject*`, and mutates nothing. Every other snapshot TU is engine-free; the test build links `NullEntitySnapshotSource.cpp`.
- **Platform boundary (ADR-009).** Untouched — snapshots are not transported here; no OS/socket header appears in the subsystem.
- **Wire protocol (ADR-010).** Untouched — a snapshot is not a packet, has no `MessageId`, and no wire encoding is added.

## 8. Worker-consumer contract (ADR-011)

The subsystem is single-threaded in Sprint-008 — **no thread is created** and it is exercised single-threaded. The `SnapshotQueue` is concurrency-*ready* behind a documented **single-producer / multi-consumer** contract: exactly one producer calls `Publish` (the manager at tick 400); any number of consumers may `Acquire` / `Release`. Lifetime is delegated to the pool's intrusive ref-count, so multiple synchronous consumers safely read the same immutable published snapshot. A future threading sprint consumes only immutable snapshots through this seam — never live state.

## 9. Governing ADRs

All snapshot code conforms to ADR-007 (exception/RTTI/iostream-free; `core::Expected<T>` / value outcomes), ADR-008 (read-only observation only), ADR-009 (platform boundary untouched), ADR-010 (wire protocol untouched — snapshots are not packets), and ADR-011 (single-threaded / deterministic; no thread created). No ADR is superseded or modified.

## 10. Extension points (reserved, not implemented)

`ReplicationSnapshot` (Sprint-009 — a prioritized, delta-able projection over `ISnapshotView`); `PersistenceSnapshot` (Sprints 011–012 — a durable serialization projection); `ThreadSnapshot` / worker-thread consumers (a future threading sprint — the queue's single-producer / multi-consumer contract is the seam); a memory-pool growth policy; the `SnapshotDiagnostics` reserved bandwidth/latency/worker fields and a deeper build-history ring; and snapshot **versioning** (the `version` metadata field enables forward-compatible consumers). No future producer `tick_order` value is assigned or depended upon (Sprint-008 occupies only the frozen `kReplication = 400`).

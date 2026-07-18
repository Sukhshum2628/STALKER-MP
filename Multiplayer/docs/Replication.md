# Replication Pipeline (Sprint-009) — Subsystem Documentation

- **Status:** Implemented & Verified — Sprint-009 (closure is Step 16). Steps 1–15 delivered and independently verified.
- **Architecture / scope authority:** `Documentation/SPRINTS/Sprint-009/Sprint-009-Delta-Replication.md` (FROZEN) and the complete step plan `Documentation/SPRINTS/Sprint-009/Sprint-009-Implementation-Plan.md` (FROZEN, single implementation authority). This document records the delivered subsystem; it introduces no new decisions.
- **Governing ADRs:** ADR-007 (Engine ABI Contract — no exceptions/RTTI/iostream; `core::Expected<T>` / value outcomes), ADR-008 (Engine State Mutation Boundary — replication is read-only observation; nothing is mutated), ADR-009 (One Platform Boundary — untouched; replication is transport-agnostic behind the Sprint-006 seam), ADR-010 (Wire Protocol Contract — extended **only additively**: new DATA-range ids, never renumbered), ADR-011 (Single-Threaded / Deterministic — **no thread is created**; the worker runs synchronously at a deterministic tick).

---

## 1. Pipeline overview

The Replication Pipeline synchronizes the authoritative host simulation with connected clients. It **consumes** the immutable per-tick snapshots produced by the Sprint-008 Snapshot System and transforms them into per-client, delta-encoded, bandwidth-shaped network packets. Replication executes no gameplay, owns no entities, and never modifies simulation — its sole responsibility is transporting authoritative state to clients (Host Authority: clients receive; the host decides). It is engine-free, OS-free, and — in Sprint-009 — single-threaded (no worker thread is spawned; the pipeline is exercised synchronously at its reserved tick).

## 2. End-to-end execution flow

Once per frame, at `tick_order::kReplicationPipeline = 450` (after the Snapshot System has published at 400), the `ReplicationManager::Update()` runs:

1. **Acquire snapshot.** The manager acquires the latest immutable published `SimulationSnapshot` from the Sprint-008 `SnapshotQueue` (read-only) and releases it after the pass.
2. **Run the worker once.** `ReplicationWorker::Execute(snapshot)` runs the per-client pipeline for every active client, ascending by `ClientId`, one client at a time:
   1. **Interest selection** — `IInterestPolicy::SelectRelevant(client, snapshot, out)` appends the relevant `EntityId`s (ascending, unique).
   2. **Build current states** — the relevant snapshot entities are marshaled into `EntityReplicationState` values (ascending).
   3. **Delta generation** — `EncodeEntityDelta(baseline, current, maxEntitiesPerUpdate)` produces the minimal `ReplicationChangeSet` (added / changed / removed); unchanged entities are omitted.
   4. **Classify + queue** — `ReplicationQueues::EnqueueChangeSet` classifies each change via the frozen §7.A classifier and routes it to the reliable or unreliable FIFO queue (capacity-bounded; `Overflow` value outcome).
   5. **Packet assembly** — `ReplicationPacketBuilder::BuildPacket` drains each queue into one deterministic, little-endian `ReplicationPacket` under the additive id `kMsgReplicationUpdate = 0x0200`, respecting the per-tick byte budget.
   6. **Advance baseline** — the client's baseline is advanced via `NextEntityBaseline` for the next pass's delta.
3. **Consume packets.** The assembled packets are collected into `ReplicationExecuteResult` and exposed via `ReplicationManager::LastResult()`. **The manager does not send them** — transport is out of scope for Sprint-009; sending is a later concern.

Inbound **acknowledgements** (`kMsgReplicationAck = 0x0201`) are handled out of band by `ReplicationManager::HandleReplicationAck`, advancing per-client ack baselines (monotonic; duplicate/stale acks ignored).

## 3. Component responsibilities

| Component | Responsibility |
|---|---|
| `replication::ReplicationTypes` | Engine-free value vocabulary: `ClientId`, `ReplicationId`, the reliability/priority/channel/state/outcome enums, and the POD capture/metadata/statistics/consistency structs. |
| `replication::ReplicationConfiguration` (+ `FromConfig`) | Parsed `[replication]` config: client/entity/player caps, queue depths, retry limit, bandwidth budget, interest radius, version. Cross-field minimums enforced as value outcomes. |
| `replication::IReplicationView` / `ReplicationUpdate` | The immutable per-client payload projection (the reserved "ReplicationSnapshot" over the snapshot). Build → `Finalize` → immutable; ascending-unique; values only. |
| `replication::ReplicationClientRegistry` / `ClientRecord` | Per-client store: identity, connection handle, focus, and last-acknowledged baseline. Ascending non-reused `ClientId`; capacity-bounded; monotonic acks. |
| `replication::IInterestPolicy` / `IBubbleMembershipSource` / `BubbleInterestPolicy` / `BubbleMembershipAdapter` | The engine-free interest seam and its Bubble-based policy (Preserve Before Replace — reuses Sprint-004 activation read-only). |
| `replication::DeltaEncoder` (`ReplicationChangeSet`) | Deterministic, pure value-in/value-out delta: added/changed/removed + player changes; version bumps; `Overflow` value outcome; baseline advance. |
| `replication::ReplicationClassifier` | The pure, total §7.A mapping: every `ReplicationChangeKind` → exactly one reliability, priority, and channel. |
| `replication::ReplicationQueues` (`FixedRecordQueue`, `ReliableQueue`, `UnreliableQueue`) | Two independent, fixed-capacity, FIFO ring-buffer queues; no allocation during operations; `Overflow` value outcome. |
| `replication::ReplicationPacketBuilder` / `ReplicationPacket` / `net/ReplicationMessageIds` | Deterministic little-endian packet assembly under the additive ids; budget-bounded; no partial packet. |
| `replication::ReplicationWorker` (`ReplicationExecuteResult`) | The synchronous per-client pipeline; produces packets; no send, no thread, no mutation. |
| `replication::ReplicationManager` | `IService` + `ITickable` at 450: one worker pass per tick; consumes packets (no transport); processes inbound acks. |
| `replication::ReplicationDiagnostics` | A non-invasive collector of monotonic counters (`RecordTick`/`RecordPacket`/`RecordAck`/`RecordOverflow` → immutable `Snapshot`). |

## 4. Tick ordering

Replication occupies the newly reserved central slot `core::tick_order::kReplicationPipeline = 450`, allocated in `FrameDispatcher.h`. The per-frame order is:

`World (100) → EntityRegistry (200) → PlayerLifecycle (250) → Bubble (300) → ALifeTransition (350) → Snapshot (400) → Replication (450) → … → Networking (900)`

Replication ticks strictly **after** the Snapshot producer (so it consumes the just-published snapshot in the same frame) and **before** Persistence (500) and Networking (900). The placement is asserted at compile time (`kReplication < kReplicationPipeline < kPersistence < kNetworking`). Networking remains last and unaffected.

## 5. Snapshot lifecycle (consumption)

Replication is a **consumer** of the Sprint-008 snapshot. Each tick the manager acquires the current published `SimulationSnapshot` from the `SnapshotQueue` and releases it after the pass. The snapshot is read strictly through the const `ISnapshotView` — replication never mutates it and never retains a live object or raw pointer (E-G1-R / ADR-008). If no snapshot has been published, the tick is skipped as a value outcome and the previous state remains valid.

## 6. Interest filtering

For each client, `BubbleInterestPolicy` selects the relevant entities: an entity is relevant when it is in the active Bubble region (`Inside` or entering — `PendingActivation`) **or**, when a positive `interestRadiusMeters` is configured, within that radius of the client's focus. Selection reads the immutable snapshot and the engine-free `IBubbleMembershipSource` seam (bound to `world::BubbleManager::MembershipOf` in Bootstrap); it appends ascending, unique `EntityId`s and is deterministic. Only relevant entities are replicated (E-G3-R).

## 7. Delta generation

`EncodeEntityDelta(baseline, current, maxEntitiesPerUpdate)` compares the client's previous baseline with the current relevant states using a deterministic two-pointer merge over ascending-unique ids:

- **added** — present in current, absent in baseline (stamped `version = 1`);
- **changed** — present in both with differing tracked fields (position, velocity, stateFlags), stamped `version = baselineVersion + 1`;
- **removed** — present in baseline, absent in current;
- **unchanged** — omitted (bandwidth minimization).

Exceeding `maxEntitiesPerUpdate` returns an `Overflow` value outcome. `EncodePlayerDelta` handles player changes analogously; `NextEntityBaseline` computes the baseline to store after the pass. Identical inputs yield identical output (E-G2-R).

## 8. Queue processing

Change records are routed to two **independent**, fixed-capacity, FIFO queues (`ReliableQueue` / `UnreliableQueue`) whose depths come from `reliableQueueDepth` / `unreliableQueueDepth`. Storage is a pre-reserved ring buffer — `Enqueue` / `Dequeue` / `Clear` perform **no dynamic allocation**. A full queue returns an `Overflow` value outcome (the queue is unchanged); `EnqueueChangeSet` classifies each change via §7.A (removed → `EntityRemove`, added → `EntitySpawn`, changed → `Position`) and enqueues in deterministic order. Cross-queue (priority) ordering is the consumer's concern; within a queue the order is FIFO.

## 9. Packet construction

`ReplicationPacketBuilder::BuildPacket` drains a queue into one deterministic, fixed **little-endian** frame under `kMsgReplicationUpdate = 0x0200`: header (message id, client, source snapshot tick, reliability, record count) → per-record fields (kind, entity id, version, state flags, position xyz, priority) → FNV-1a checksum. Field widths are fixed (`kHeaderBytes = 21`, `kRecordBytes = 26`, `kChecksumBytes = 4`). The record count is computed up front from the byte budget (`bandwidthBudgetBytesPerTick`) and `maxEntitiesPerUpdate`; records that do not fit remain queued for the next packet. A budget below an empty frame returns an `Overflow` value outcome with **no partial drain** (no partial packet corruption). Identical input yields byte-for-byte identical output.

## 10. Acknowledgement processing

Clients acknowledge received updates with `kMsgReplicationAck = 0x0201` (client → host). `ReplicationManager::HandleReplicationAck` (typed and wire-`ByteReader` overloads) advances the client's ack baseline through the registry: an ack strictly newer than the current baseline is applied (`AcksApplied`); a duplicate or stale ack is ignored (`AcksIgnored`); an unknown client or malformed payload is a value outcome. Acks never mutate simulation — they only advance replication ack state (Host Authority).

## 11. Diagnostics

`ReplicationDiagnostics` is a pure, **non-invasive** collector: it holds no reference to the pipeline and accumulates monotonic counters from the Step-10 worker results and the Step-11 ack events (`RecordTick`, `RecordPacket`, `RecordAck`, `RecordOverflow`). `Snapshot()` returns an immutable `ReplicationStatistics` value (a copy; mutating it cannot affect the collector); `Reset()` restores the all-zero initial state. It performs no output, networking, scheduling, threading, simulation, or engine work.

## 12. Bootstrap integration

The composition root registers a single `ReplicationManager` with the `ServiceRegistry` **after** the SnapshotManager and every simulation producer (so registration-order initialization runs it last), and subscribes it to the one `FrameDispatcher` at `kReplicationPipeline = 450`. Its collaborators — the `ReplicationClientRegistry`, the `BubbleMembershipAdapter` (bound to the live `BubbleManager`), and the `BubbleInterestPolicy` — are Runtime-owned so they outlive the ServiceRegistry-owned manager; the manager consumes the SnapshotManager's `SnapshotQueue` read-only. Teardown is reverse-order: the manager is unsubscribed before the SnapshotManager and before `ShutdownAll`. Exactly one manager instance is wired, bringing the frame subscriber count to eight; the wiring includes only engine-free headers (no engine or OS header).

`Dependencies()` = {World, EntityRegistry, BubbleManager, TransitionManager, PlayerManager, Networking, SnapshotManager} (ordering-only).

## 13. Boundaries & governing ADRs

- **Engine boundary (ADR-008).** Replication introduces **no** new engine translation unit and touches no engine header; every input is already behind an engine-free seam (snapshots, Bubble membership, player surface). Read-only observation only.
- **Platform boundary (ADR-009).** Untouched — replication assembles in-memory byte frames and is transport-agnostic behind the existing Sprint-006 seam; no OS/socket code is added.
- **Wire protocol (ADR-010).** Extended only additively: `kMsgReplicationUpdate = 0x0200` and `kMsgReplicationAck = 0x0201` (block 0x0200–0x020F), appended and never renumbered; fixed little-endian framing; DATA-range ids.
- **Error model (ADR-007).** No exceptions, no RTTI, no iostream; every fallible operation is a `core::Expected<T>` or a `ReplicationOutcome` value; bounded memory (pre-reserved queues; `Overflow` outcomes).
- **Threading (ADR-011).** No thread is created; the worker runs synchronously at tick 450. It is *designed* for a future worker thread behind the snapshot seam, but Sprint-009 exercises it single-threaded.

## 14. Evidence gates

- **E-G1-R** — replication consumes immutable snapshots only; no live object or raw pointer is captured (value-only). ✅
- **E-G2-R** — deterministic build/delta: identical snapshot + baseline → identical replication content and byte-for-byte packets. ✅
- **E-G3-R** — interest correctness: only relevant entities are replicated per client. ✅
- **E-G4-R** — reliability/priority classification correct (§7.A); priority ordering total and deterministic. ✅
- **E-G5-R** — worker independence: replication never mutates simulation/engine; networking remains a consumer. ✅

## 15. Extension points (permitted by the architecture)

Reserved and consistent with the frozen design — not implemented here: an actual worker **thread** consuming the snapshot seam (the queue's single-producer/multi-consumer contract and the synchronous worker are the seam); **transport send** of the assembled `ReplicationPacket`s and inbound-ack message-handler registration; richer per-field change kinds for "changed" entities; player-state packet fields beyond the current entity frame; bandwidth/priority scheduling policies; and the `ReplicationDiagnostics` counters feeding a richer reporting surface. No future producer `tick_order` value beyond the frozen `kReplicationPipeline = 450` is assigned or depended upon; no non-goal (client prediction, interpolation, lag compensation, save system) is implemented.

# Sprint-009: Delta Replication
# Sprint-009: Replication Pipeline

---

# Sprint Information

| Field | Value |
|--------|-------|
| Sprint | 009 |
| Title | Replication Pipeline |
| Status | **Implemented / Verified / Closed / Frozen** (2026-07-16) |
| Priority | Critical |
| Estimated Duration | 3–4 Weeks |
| Prerequisites | Sprint-001 through Sprint-008 |
| Next Sprint | Sprint-010 – Client Prediction & Interpolation |

---

# 1. Sprint Overview

Sprint-009 implements the Replication Pipeline responsible for synchronizing the authoritative simulation with connected clients.

The Replication Worker consumes immutable snapshots produced by the Simulation Thread and transforms them into efficient network updates.

Replication does not execute gameplay.

Replication does not own entities.

Replication never modifies simulation.

Its sole responsibility is transporting authoritative state to connected clients.

---

# 2. Objectives

## Primary Objectives

- Create Replication Manager
- Create Replication Worker
- Implement Interest Management
- Implement Delta Replication
- Implement Packet Prioritization
- Implement Reliability Rules
- Integrate Snapshot System

## Secondary Objectives

- Replication diagnostics
- Network profiling
- Bandwidth statistics
- Debug visualization

---

# 3. Scope

Included

- Replication Manager
- Snapshot consumption
- Interest Management
- Delta generation
- Packet construction
- Reliability channels
- Replication queues

---

# 4. Out of Scope

Not Included

- Client prediction
- Interpolation
- Lag compensation
- Inventory UI
- Chat
- Voice
- Save system

---

# 5. Architecture References

- 06_Multiplayer.md
- 07_Replication.md

---

# 6. RFC References

- RFC-0003 — Immutable Snapshot Architecture
- RFC-0004 — Replication Pipeline
- RFC-0006 — Threading Execution Model

---

# 7. Technical Tasks

---

## 7.1 Replication Manager

Create

- ReplicationManager
- ReplicationConfiguration
- ReplicationContext

Responsibilities

- Worker management
- Client replication
- Queue scheduling
- Statistics

---

## 7.2 Snapshot Consumer

Consume

SimulationSnapshot

Responsibilities

- Read snapshot
- Validate snapshot
- Build replication state

Never modify snapshots.

---

## 7.3 Replication Worker

Implement

StartWorker()

StopWorker()

PublishPackets()

ProcessSnapshot()

FlushQueues()

Runs independently from the Simulation Thread.

---

## 7.4 Interest Management

Determine entities relevant to each client.

Consider

- Bubble membership
- Distance
- Visibility
- Gameplay relevance
- Ownership

Only replicate relevant entities.

---

## 7.5 Delta Replication

Implement

Previous Snapshot

↓

Current Snapshot

↓

Difference

↓

Replication Update

Transmit only meaningful changes.

---

## 7.6 Entity Replication

Replicate

Position

Rotation

Velocity

Animation

Health

State Flags

Ownership

Entity Version

---

## 7.7 Player Replication

Replicate

Movement

Pose

Equipment

Health

Stance

Interaction State

Connection State

Authority Flags

---

## 7.8 Packet Prioritization

Assign priority levels.

Highest

- Players
- Combat
- Damage
- Entity Spawn
- Entity Destruction

Medium

- Nearby NPCs
- Inventory
- Animations

Lowest

- Ambient objects
- Weather updates
- Distant entities

---

## 7.9 Reliability Rules

Reliable

- Entity Spawn
- Entity Removal
- Inventory
- Quest Updates
- Player Join
- Player Leave

Unreliable

- Position
- Rotation
- Velocity
- Animation
- Camera

---

## 7.10 Replication Queues

Support

Outgoing Queue

Retry Queue

Reliable Queue

Unreliable Queue

Priority Queue

---

## 7.11 Packet Assembly

Build

Packet Header

↓

Entity Updates

↓

Player Updates

↓

Metadata

↓

Checksum

Packet layout remains implementation-specific.

---

## 7.12 Diagnostics

Provide

Replication Inspector

Packet Viewer

Bandwidth Graph

Priority Statistics

Dropped Packet Counter

Client Statistics

---

## 7.13 Performance

Measure

Packet Build Time

Delta Generation

Queue Latency

Worker Latency

Bandwidth Usage

Packets Per Second

---

## 7.14 Unit Tests

Verify

Snapshot consumption

Interest filtering

Delta generation

Reliable updates

Unreliable updates

Queue ordering

Packet assembly

Stress tests

---

## 7.15 Integration Tests

Verify

Client synchronization

Multiple clients

Large entity counts

High update frequency

Packet prioritization

Interest management

---

## 7.16 Documentation

Document

Replication lifecycle

Delta generation

Interest management

Reliability model

Packet priorities

Worker interaction

---

# 8. Deliverables

✓ Replication Manager

✓ Replication Worker

✓ Interest Management

✓ Delta Replication

✓ Packet Prioritization

✓ Reliability Rules

✓ Diagnostics

✓ Performance Metrics

✓ Tests

✓ Documentation

---

# 9. Risks

Potential Risks

- Excessive bandwidth
- Large packet sizes
- Queue congestion
- Delta calculation overhead
- Client desynchronization

Mitigation

- Packet prioritization
- Interest management
- Delta compression
- Bandwidth budgeting
- Profiling

---

# 10. Validation Strategy

Replication

✓ Snapshots consumed correctly.

Interest

✓ Only relevant entities transmitted.

Delta

✓ Unchanged entities skipped.

Packets

✓ Priority ordering correct.

Performance

✓ Bandwidth within budget.

Stress Test

✓ Multiple clients synchronize correctly.

---

# 11. Acceptance Criteria

☑ Replication Manager operational.

☑ Snapshot consumption complete.

☑ Delta replication operational.

☑ Interest management verified.

☑ Reliability rules implemented.

☑ Packet prioritization operational.

☑ Tests passing (442 / 442).

☑ Documentation updated.

---

# 12. Definition of Done

Sprint-009 is complete when

- Replication consumes immutable snapshots exclusively.
- Clients receive only relevant updates.
- Delta replication minimizes bandwidth.
- Packet priorities function correctly.
- Worker execution remains independent of simulation.
- Networking never modifies gameplay state.

---

# 13. Next Sprint

Sprint-010 – Client Prediction & Interpolation

Replication provides authoritative updates from the host, but network latency can still affect responsiveness.

Sprint-010 introduces client-side prediction, interpolation, reconciliation, and smoothing techniques to provide responsive player movement while maintaining strict host authority.
---

# 14. Sprint Closure (2026-07-16)

**Sprint-009 (Replication Pipeline) is declared Implemented / Verified / Closed / Frozen.**

All 16 steps were implemented one at a time under the mandatory workflow (implement → Antigravity verification → git commit → GitHub push → next step) and each was independently verified by Antigravity, with the tree left buildable after every step.

## 14.1 Final verified baseline
- **442 / 442 build tests passing** — Release x64 on **MSVC** and the engine-free **GCC** test build; **0 errors, 0 warnings, no regressions.** (Sprint-008 baseline 377 + the Sprint-009 replication suite.)
- MSVC Release clean under `EngineAbi.props`. Game testing has not started yet.

## 14.2 Steps 01–15 (all implemented, verified, documented)
01 value types & vocabulary · 02 `ReplicationConfiguration` · 03 immutable `ReplicationUpdate` + `IReplicationView` · 04 `ReplicationClientRegistry` · 05 interest seam + `BubbleInterestPolicy` · 06 delta generation (`DeltaEncoder`) · 07 §7.A classifier · 08 replication queues · 09 packet assembly + additive wire ids (0x0200/0x0201) · 10 synchronous `ReplicationWorker` · 11 `ReplicationManager` at `kReplicationPipeline = 450` · 12 Bootstrap wiring · 13 `ReplicationDiagnostics` · 14 validation hardening · 15 integration documentation (`Replication.md`).

## 14.3 Definition of Done (§12) — satisfied
1. Replication consumes immutable snapshots exclusively (read-only `ISnapshotView`; no live object captured). ✅
2. Clients receive only relevant updates (Bubble membership + interest radius). ✅
3. Delta replication minimizes bandwidth (unchanged entities omitted; deterministic change set). ✅
4. Packet priorities/reliability function correctly (frozen §7.A classification; deterministic little-endian framing). ✅
5. Worker execution is independent of simulation (no thread created; no mutation; runs synchronously at 450). ✅
6. Networking never modifies gameplay state (replication owns no entities; acks advance only replication state). ✅

## 14.4 Completion criteria — satisfied
- All 16 steps implemented, each Antigravity-verified, tree buildable after each. ✅
- `ReplicationManager` runs as a single `ITickable` at the reserved `tick_order::kReplicationPipeline = 450`, after Snapshot (400), before Persistence (500)/Networking (900), Bootstrap-wired with reverse-order teardown. ✅
- One Engine Boundary **and** One Platform Boundary hold — **no new engine TU, no OS code added**; the wire protocol is extended only additively (`kMsgReplicationUpdate`/`kMsgReplicationAck`), never renumbered. ✅
- ADR-007…ADR-011 all conformed to; no thread created; one new sanctioned `tick_order` key. ✅
- Full suite green on GCC + MSVC, MSVC Release clean, zero new warnings, no regressions; the Sprint-008 377/377 baseline preserved and extended to 442/442. ✅
- No non-goal implemented (no client prediction/interpolation/lag-compensation/save-system); replication owns no entities and executes no gameplay. ✅
- Subsystem doc `Multiplayer/docs/Replication.md` written; status docs synchronized to Closed/Verified. ✅

## 14.5 Evidence gates — satisfied
- **E-G1-R** (immutable-snapshot consumption; no live object captured): **PASSED**.
- **E-G2-R** (deterministic build/delta; byte-for-byte packets): **PASSED**.
- **E-G3-R** (interest correctness — only relevant entities per client): **PASSED**.
- **E-G4-R** (reliability/priority classification correct; total priority ordering): **PASSED**.
- **E-G5-R** (worker independence — no simulation/engine mutation; networking a consumer): **PASSED**.

## 14.6 Sprint-010 readiness
Replication delivers authoritative, delta-encoded, per-client updates from the host. **Sprint-010 (Client Prediction & Interpolation)** builds on these updates to provide responsive client movement (prediction, interpolation, reconciliation, smoothing) while preserving strict Host Authority. No future producer `tick_order` value beyond the frozen `kReplicationPipeline = 450` is assigned or depended upon. **The project is ready for Sprint-010.**

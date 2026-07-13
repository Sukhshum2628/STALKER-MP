# RFC-0006: Threading Execution Model
# RFC-0006: Threading Execution Model

| Field | Value |
|--------|-------|
| RFC | 0006 |
| Title | Threading Execution Model |
| Status | Accepted |
| Author | STALKER-MP Project |
| Created | July 2026 |
| Depends On | RFC-0001, RFC-0002, RFC-0003, RFC-0004, RFC-0005, 09_Threading.md |
| Supersedes | None |
| Superseded By | None |

---

# 1. Abstract

This RFC defines the threading architecture used throughout STALKER-MP.

The project adopts a single authoritative Simulation Thread responsible for all gameplay execution. Worker threads operate exclusively on immutable snapshots and are prohibited from directly accessing or modifying live simulation state.

This model preserves deterministic execution, minimizes synchronization overhead, eliminates shared mutable state, and provides a scalable execution model suitable for future multiplayer features.

The threading architecture reinforces the ownership boundaries established by previous RFCs and serves as the execution foundation for the entire engine.

---

# 2. Motivation

The original X-Ray Engine primarily executes gameplay on a single thread.

Modern multiplayer introduces numerous asynchronous workloads, including:

- network replication;
- persistence;
- diagnostics;
- replay recording;
- telemetry;
- future plugin execution.

Executing these workloads directly on the Simulation Thread would reduce frame stability and increase simulation latency.

Conversely, allowing multiple threads to modify gameplay state would violate deterministic simulation and introduce race conditions.

A structured threading model is therefore required.

---

# 3. Problem Statement

Multiple engine subsystems require computational resources while preserving:

- deterministic gameplay;
- host authority;
- subsystem ownership;
- consistent world state.

Naïve multithreading introduces:

- race conditions;
- deadlocks;
- inconsistent reads;
- simulation stalls;
- difficult debugging.

The project therefore requires a strict execution model defining thread ownership and communication rules.

---

# 4. Goals

The threading model must:

- preserve deterministic simulation;
- eliminate shared mutable state;
- avoid gameplay locks;
- maximize parallelism where safe;
- simplify debugging;
- scale with additional worker systems;
- maintain clear ownership boundaries.

---

# 5. Non-Goals

This RFC does not define:

- operating system scheduling;
- thread pool implementation;
- CPU affinity;
- synchronization primitives;
- task scheduler implementation.

These remain implementation details.

---

# 6. Decision

The project adopts a single Simulation Thread.

Only this thread may:

- execute gameplay;
- update ALife;
- modify entities;
- advance world time;
- process player actions;
- execute physics;
- modify inventories;
- progress story state.

Every other thread consumes immutable snapshots.

Workers never modify authoritative state.

---

# 7. Thread Ownership

## Simulation Thread

Owns:

- World
- ALife
- Scheduler
- Physics
- Story
- Inventory
- Entity Registry
- Bubble Manager
- Smart Terrain
- Environment
- Simulation Time

No other thread may modify these systems.

---

## Worker Threads

Examples include:

- Replication Worker
- Persistence Worker
- Replay Worker
- Analytics Worker
- Diagnostic Worker
- Future Plugin Workers

Workers consume immutable snapshots only.

---

## Render Thread

Rendering remains separate from gameplay.

The Render Thread consumes presentation state.

Rendering never modifies simulation.

---

# 8. Communication Model

Thread communication occurs exclusively through immutable data.

The Simulation Thread produces snapshots.

Workers consume snapshots.

Workers may produce output artifacts such as:

- network packets;
- save files;
- replay data;
- telemetry reports.

Workers never produce gameplay state.

---

# 9. Design Rationale

## Deterministic Simulation

Only one thread executes gameplay.

Execution order is therefore deterministic.

---

## Lock-Free Simulation

The Simulation Thread never waits for worker completion during normal execution.

Frame time remains predictable.

---

## Ownership Preservation

Thread ownership mirrors subsystem ownership.

Each subsystem has exactly one authoritative execution context.

---

## Future Scalability

Additional workers may be introduced without modifying simulation logic.

This minimizes long-term architectural coupling.

---

# 10. Synchronization Strategy

The architecture minimizes synchronization.

Workers receive completed snapshots.

No worker requires:

- gameplay locks;
- entity locks;
- scheduler locks.

Synchronization occurs only when transferring snapshot ownership.

---

# 11. Failure Isolation

Worker failures must not terminate gameplay.

Examples include:

- replication failure;
- save failure;
- telemetry failure.

The Simulation Thread remains authoritative.

Worker recovery is subsystem-specific.

---

# 12. Alternatives Considered

## Fully Parallel Gameplay

Rejected.

Reason:

ALife was designed around sequential execution.

Parallel gameplay introduces nondeterministic behavior.

---

## Fine-Grained Entity Locks

Rejected.

Reason:

High complexity.

Difficult debugging.

Performance overhead.

---

## Global Engine Lock

Rejected.

Reason:

Eliminates benefits of multithreading.

Workers would block gameplay.

---

## Shared Mutable World

Rejected.

Reason:

Violates RFC-0003.

Creates race conditions.

---

# 13. Consequences

Positive:

- deterministic execution;
- simplified debugging;
- lock-free gameplay;
- scalable worker architecture;
- explicit ownership.

Negative:

- Simulation Thread remains CPU bottleneck.
- Snapshot creation introduces memory overhead.
- Some workloads cannot be parallelized.

These trade-offs align with the project's simulation-first philosophy.

---

# 14. Interaction with Other Systems

## World

Executed only on the Simulation Thread.

---

## ALife

Sequential execution.

Never parallelized.

---

## Replication

Runs asynchronously using snapshots.

---

## Persistence

Runs asynchronously using snapshots.

---

## Plugins

Future plugins must respect thread ownership.

Unsafe access to live simulation is prohibited.

---

# 15. Future Evolution

The architecture supports future worker systems including:

- pathfinding preprocessing;
- AI analytics;
- replay generation;
- world statistics;
- performance profiling;
- dedicated server monitoring.

All future workers must consume immutable snapshots.

Simulation ownership may never be transferred.

---

# 16. Risks

Potential risks include:

- Simulation Thread becoming CPU-bound.
- Snapshot generation overhead.
- Worker queue congestion.
- Excessive memory usage.

Mitigation strategies include:

- efficient snapshot generation;
- worker prioritization;
- memory pooling;
- workload batching;
- profiling-driven optimization.

---

# 17. Acceptance Criteria

The Threading Execution Model is considered correctly implemented when:

- only the Simulation Thread modifies gameplay state;
- workers consume immutable snapshots exclusively;
- no gameplay locks exist during normal execution;
- worker failures cannot corrupt simulation;
- simulation remains deterministic regardless of worker timing;
- subsystem ownership is never violated.

---

# 18. References

Architecture Documents

- 09_Threading.md

Related RFCs

- RFC-0001 — Host Authoritative Simulation
- RFC-0002 — MultiPlayer Bubble System
- RFC-0003 — Immutable Snapshot Architecture
- RFC-0004 — Replication Pipeline
- RFC-0005 — Persistence Architecture
- RFC-0007 — Plugin & Extension System
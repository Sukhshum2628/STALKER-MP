# STALKER-MP Threading Architecture

**Document Version:** 1.0

**Status:** Draft

**Audience:** Engine Developers, Systems Developers, Networking Developers

---

# 1. Purpose

The Threading subsystem defines how the various architectural components of STALKER-MP execute concurrently while preserving deterministic simulation.

Rather than maximizing CPU utilization at all costs, the objective is to separate independent workloads without compromising correctness, reproducibility, or authority.

Threading is therefore an execution concern rather than a gameplay concern.

---

# 2. Philosophy

Simulation correctness is more important than parallel execution.

The architecture favors:

- deterministic execution
- explicit ownership
- immutable handoff
- predictable synchronization

over:

- aggressive parallelism
- unrestricted shared memory
- implicit synchronization

Parallelism exists to improve scalability.

It must never change simulation results.

---

# 3. Design Goals

The Threading subsystem has several primary goals.

### Deterministic Simulation

The World and ALife must produce identical results regardless of execution timing.

---

### Safe Parallelism

Independent systems should execute concurrently only when doing so cannot alter authoritative state.

---

### Explicit Ownership

Every thread owns clearly defined data.

Cross-thread modification should be minimized.

---

### Future Scalability

The execution model should naturally extend to additional worker threads as the project evolves.

---

### Debuggability

The threading model should remain understandable and observable.

Complex synchronization should be avoided whenever practical.

---

# 4. Architectural Position

Within STALKER-MP, Threading coordinates execution across the major runtime systems.

```
                Threading

                      │

      ┌───────────────┼───────────────┐

      ▼               ▼               ▼

 Simulation      Replication     Persistence

      ▼               ▼               ▼

 Snapshot       Packet Build     Save Snapshot

      ▼

 Rendering
```

Threading coordinates *when* systems execute.

It never changes *what* they do.

---

# 5. Core Principles

The following principles govern every threaded subsystem.

---

## One Authoritative Simulation

Only one thread owns authoritative simulation.

---

## Immutable Handoff

Shared data is exchanged through immutable snapshots rather than mutable shared structures.

---

## Explicit Synchronization

Synchronization points are deliberate and well-defined.

Implicit synchronization is avoided.

---

## Read-Mostly Design

Worker threads primarily consume data rather than modify it.

---

## Isolation

Failures within one worker should not corrupt the authoritative simulation.

---

## Deterministic Ordering

Simulation ordering must remain independent of operating system scheduling.

---

# 6. Execution Model

The Version 1 architecture separates execution into logical domains.

```
Simulation Thread

↓

Snapshot

↓

Worker Threads

↓

Completed Results

↓

Next Simulation Tick
```

The Simulation Thread remains authoritative.

Workers perform auxiliary tasks.

---

# 7. Thread Roles

Version 1 defines several conceptual execution roles.

### Simulation Thread

Owns:

- World
- ALife
- Scheduler
- Gameplay
- Physics
- Entity Registry

---

### Render Thread

Owns:

- rendering
- presentation
- GPU submission
- frame generation

---

### Replication Worker

Owns:

- snapshot reading
- relevance evaluation
- serialization
- packet construction

---

### Persistence Worker

Owns:

- save serialization
- validation
- storage
- integrity verification

---

### Future Workers

Potential additions include:

- asset streaming
- replay recording
- diagnostics
- analytics

Each worker should remain independent whenever practical.

---

# 8. Simulation Thread

## 8.1 Purpose

The Simulation Thread is the heart of the engine.

Every authoritative gameplay decision originates here.

No other thread modifies authoritative world state.

---

## 8.2 Responsibilities

The Simulation Thread executes:

- World updates
- ALife Scheduler
- Smart Terrain
- Story
- Faction Planner
- Gameplay
- Physics
- Online/offline transitions

This thread owns the authoritative state of the Zone.

---

## 8.3 Update Pipeline

```
Input

↓

World

↓

Bubble Evaluation

↓

ALife

↓

Gameplay

↓

Physics

↓

Commit

↓

Snapshot Generation
```

The snapshot marks the synchronization boundary between simulation and worker threads.

---

# 9. Render Thread

## 9.1 Purpose

The Render Thread is responsible for visual presentation only.

It consumes replicated and predicted state but never modifies authoritative gameplay.

---

## 9.2 Responsibilities

The Render Thread performs:

- rendering
- animation playback
- particle systems
- UI
- audio submission
- GPU command generation

Simulation remains external.

---

## 9.3 Data Flow

```
Authoritative State

↓

Prediction

↓

Interpolation

↓

Rendering

↓

Display
```

Rendering never feeds information back into simulation.

---

# 10. Replication Worker

## 10.1 Purpose

Replication operates on immutable snapshots rather than live simulation data.

This separation allows packet generation to occur independently of gameplay execution.

---

## 10.2 Responsibilities

The Replication Worker performs:

- snapshot reading
- relevance evaluation
- delta generation
- serialization
- packet construction

The worker never modifies simulation state.

---

## 10.3 Execution Pipeline

```
Snapshot Received

↓

Relevance

↓

Delta Generation

↓

Serialization

↓

Packets Ready
```

The completed packets are then passed to the Multiplayer transport layer.

---

# 11. Persistence Worker

## 11.1 Purpose

Persistence executes independently of the simulation whenever practical.

Rather than blocking gameplay during serialization, the worker operates on immutable persistence snapshots.

---

## 11.2 Responsibilities

The Persistence Worker performs:

- save serialization
- validation
- integrity checking
- storage writes
- metadata generation

The worker never modifies live gameplay state.

---

## 11.3 Save Pipeline

```
Persistence Snapshot

↓

Serialization

↓

Validation

↓

Storage

↓

Save Complete
```

The authoritative simulation continues independently once the snapshot has been captured.

---

# 12. Synchronization Model

## 12.1 Purpose

Multiple threads operate concurrently throughout STALKER-MP.

Without a well-defined synchronization model, concurrent execution could introduce race conditions, inconsistent world state, and non-deterministic simulation.

The synchronization model defines exactly when data may cross thread boundaries and under what conditions it may be accessed.

---

## 12.2 Design Philosophy

Synchronization should be rare.

Whenever possible, threads operate independently using immutable data.

Rather than continuously sharing mutable objects, threads communicate through explicit synchronization points.

This approach reduces locking complexity while preserving deterministic simulation.

---

## 12.3 Synchronization Boundaries

Version 1 defines several synchronization boundaries.

### Simulation → Replication

Authoritative snapshot handoff.

---

### Simulation → Persistence

Persistence snapshot handoff.

---

### Replication → Multiplayer

Packet handoff.

---

### Multiplayer → Transport

Packet transmission.

---

### Client → Simulation

Validated player input.

Each boundary transfers ownership explicitly.

---

## 12.4 Synchronization Pipeline

```
Simulation Tick

↓

Commit World State

↓

Generate Snapshots

↓

Signal Workers

↓

Worker Processing

↓

Worker Completion

↓

Next Tick
```

Simulation never waits on worker execution unless explicitly required.

---

## 12.5 Synchronization Guarantees

The architecture guarantees:

- deterministic simulation order
- immutable cross-thread data
- explicit ownership transfer
- bounded synchronization points

---

# 13. Data Ownership

## 13.1 Philosophy

Correct multithreaded systems begin with ownership.

Every piece of mutable data has one authoritative owner.

Other threads may observe that data only through immutable representations.

---

## 13.2 Ownership Matrix

| Data | Owner | Readers |
|------|-------|---------|
| World | Simulation Thread | Snapshot Builders |
| ALife | Simulation Thread | Snapshot Builders |
| Physics | Simulation Thread | None |
| Replication Snapshot | Replication Worker | Multiplayer |
| Persistence Snapshot | Persistence Worker | Storage |
| Packet Queue | Multiplayer | Transport |
| Render State | Render Thread | GPU |

Ownership is never ambiguous.

---

## 13.3 Ownership Rules

The following rules apply throughout the engine.

- One mutable owner.
- Many immutable readers.
- Explicit transfer.
- No shared mutation.
- No hidden ownership.

---

## 13.4 Lifetime

Ownership remains stable throughout object lifetime.

Temporary processing ownership (for example, serialization) does not imply gameplay ownership.

---

# 14. Immutable Snapshot Handoff

## 14.1 Purpose

The primary synchronization mechanism within STALKER-MP is immutable snapshot handoff.

Rather than exposing live simulation data to worker threads, the Simulation Thread creates immutable representations that can be processed safely in parallel.

---

## 14.2 Handoff Pipeline

```
Simulation Tick Complete

↓

Freeze State

↓

Create Snapshot

↓

Transfer Snapshot

↓

Worker Consumes Snapshot

↓

Destroy Snapshot
```

The Simulation Thread resumes immediately after the snapshot has been created.

---

## 14.3 Snapshot Ownership

Ownership transfers occur as follows.

### Replication Snapshot

Simulation Thread

↓

Replication Worker

---

### Persistence Snapshot

Simulation Thread

↓

Persistence Worker

---

### Render State (Conceptual)

Simulation

↓

Render Thread

The original simulation data remains authoritative.

---

## 14.4 Benefits

Immutable handoff provides:

- no race conditions
- minimal locking
- deterministic processing
- easier debugging
- future parallelization
- safer memory management

---

# 15. Locking Strategy

## 15.1 Philosophy

Locks are a necessary tool but should remain exceptional.

The architecture minimizes lock usage through explicit ownership and immutable snapshots.

The preferred order of solutions is:

1. Eliminate shared mutable state.
2. Transfer ownership.
3. Use immutable data.
4. Introduce locks only when unavoidable.

---

## 15.2 Lock-Free Domains

The following systems should execute without runtime locks under normal operation.

- World simulation
- ALife
- Scheduler
- Smart Terrain
- Faction Planner

These systems are confined to the Simulation Thread.

---

## 15.3 Limited Lock Usage

Locks may be appropriate for:

- transport queues
- logging
- metrics
- diagnostics
- administrative interfaces

These systems do not influence authoritative simulation.

---

## 15.4 Locking Invariants

The architecture aims to avoid:

- nested locks
- cyclic dependencies
- long-lived locks
- blocking the Simulation Thread

Maintaining short lock durations reduces contention and simplifies debugging.

---

# 16. Thread Communication

## 16.1 Communication Principles

Threads communicate through well-defined channels.

Communication should be:

- explicit
- asynchronous where practical
- ownership-aware
- deterministic

Hidden shared state is prohibited.

---

## 16.2 Communication Channels

Examples include:

### Simulation → Replication

Immutable snapshot.

---

### Simulation → Persistence

Immutable snapshot.

---

### Replication → Multiplayer

Packet queue.

---

### Multiplayer → Transport

Outgoing packet buffer.

---

### Transport → Multiplayer

Incoming packet queue.

---

### Multiplayer → Simulation

Validated input events.

---

## 16.3 Event Flow

```
Producer

↓

Queue

↓

Consumer

↓

Process

↓

Complete
```

Queues represent synchronization boundaries.

They should remain bounded and observable.

---

## 16.4 Communication Guarantees

The architecture guarantees:

- ordered delivery within a channel where required
- explicit ownership transfer
- bounded queue growth
- deterministic processing order

---

# 17. Future Parallelization

## 17.1 Philosophy

Version 1 intentionally favors simplicity and determinism.

Future versions may increase parallelism without changing subsystem ownership.

---

## 17.2 Candidate Systems

Potential candidates include:

- Smart Terrain evaluation
- faction planning
- artifact simulation
- environment updates
- replay generation
- diagnostics
- analytics

Each candidate must preserve deterministic commit ordering.

---

## 17.3 Parallelization Strategy

Future expansion should follow a phased approach.

```
Simulation

↓

Independent Workers

↓

Barrier

↓

Commit

↓

Next Tick
```

Only completed results become visible to the authoritative simulation.

---

## 17.4 Architectural Stability

The ownership model established in Version 1 should remain unchanged.

New worker threads extend the architecture rather than replacing it.

---

# 18. Failure Recovery

## 18.1 Philosophy

Thread failures should remain isolated whenever possible.

A worker thread failure must not corrupt the authoritative simulation.

---

## 18.2 Replication Worker Failure

If replication processing fails:

```
Log Failure

↓

Discard Work

↓

Retry Next Snapshot
```

Simulation continues.

---

## 18.3 Persistence Worker Failure

If persistence fails:

```
Abort Save

↓

Retain Previous Save

↓

Continue Simulation
```

The World remains authoritative.

---

## 18.4 Worker Starvation

Long-running workers should never block the Simulation Thread.

Timeouts and diagnostics should identify abnormal execution.

---

## 18.5 Simulation Thread Failure

Because the Simulation Thread owns the authoritative world, failure is considered fatal in Version 1.

Recovery occurs through the Persistence subsystem during restart.

---

# 19. Design Rationale

The Threading architecture intentionally favors deterministic execution over maximum CPU utilization.

By concentrating authoritative gameplay within a single Simulation Thread and using immutable snapshot handoffs for concurrent work, the engine achieves several goals.

### Correctness

Simulation results remain deterministic.

---

### Simplicity

Subsystem ownership remains explicit.

---

### Scalability

Additional workers can be introduced incrementally.

---

### Maintainability

Race conditions are minimized through architectural boundaries rather than extensive locking.

---

### Future Growth

Dedicated servers and higher-core-count systems benefit from the same execution model without requiring major redesign.

---

# 20. Summary

The Threading subsystem defines the execution model of STALKER-MP.

It establishes a single authoritative Simulation Thread supported by specialized worker threads for replication, persistence, rendering, and future auxiliary tasks.

The architecture is built upon several key principles:

- One authoritative simulation thread.
- Explicit ownership of mutable state.
- Immutable snapshot handoffs.
- Minimal locking.
- Deterministic synchronization.
- Safe incremental parallelization.

By separating execution concerns from gameplay concerns, the Threading architecture enables STALKER-MP to scale across modern hardware while preserving the deterministic World, ALife, Multiplayer, Replication, and Persistence models defined throughout the rest of the architecture.

# STALKER-MP Replication Architecture

**Document Version:** 1.0

**Status:** Draft

**Audience:** Engine Developers, Networking Developers, Multiplayer Developers

---

# 1. Purpose

The Replication subsystem synchronizes the authoritative state of the Zone from the host to connected clients.

Unlike the Multiplayer subsystem, Replication is not responsible for:

- connections
- authentication
- sessions
- transport

Instead, Replication answers a different question:

> Which parts of the authoritative simulation must be synchronized to each client?

Its responsibilities include:

- snapshot generation
- state synchronization
- relevance evaluation
- prediction support
- interpolation support
- bandwidth management
- serialization coordination

The Replication subsystem exists between the authoritative simulation and the network transport.

---

# 2. Philosophy

Replication never creates gameplay.

Replication never executes gameplay.

Replication only communicates the results of authoritative simulation.

The simulation always remains the source of truth.

Conceptually:

```
World

↓

ALife

↓

Gameplay

↓

Replication

↓

Networking

↓

Clients
```

This ordering is invariant.

---

# 3. Design Goals

The Replication subsystem has several primary goals.

### Preserve Authoritative State

Clients must observe the host's simulation.

---

### Minimize Bandwidth

Transmit only information that clients require.

---

### Maintain Determinism

Replication must never alter simulation results.

---

### Scale Efficiently

Support increasing player counts through relevance evaluation and prioritization.

---

### Separate Concerns

Replication should remain independent from transport implementation.

---

### Support Future Features

The architecture should naturally accommodate:

- dedicated servers
- replay systems
- spectators
- diagnostics

---

# 4. Architectural Position

Within STALKER-MP, Replication occupies the following position.

```
                 World
                   │
                   ▼
                 ALife
                   │
                   ▼
               Gameplay
                   │
                   ▼
              Replication
        ┌──────────┼──────────┐
        ▼          ▼          ▼
 Snapshot     Relevance    Serialization
        │          │          │
        └──────────┼──────────┘
                   ▼
             Multiplayer
                   ▼
              Transport
                   ▼
                Clients
```

Replication consumes authoritative state and produces synchronized client views.

---

# 5. Responsibilities

Replication is responsible for:

- snapshot generation
- entity synchronization
- relevance evaluation
- serialization coordination
- prediction support
- interpolation support
- bandwidth allocation
- packet prioritization
- state reconciliation

Replication is **not** responsible for:

- gameplay logic
- AI
- inventory authority
- physics ownership
- player authentication
- connection management
- persistence

---

# 6. Internal Architecture

The subsystem consists of several major components.

```
                Replication

                       │

      ┌────────────────┼────────────────┐

      ▼                ▼                ▼

 Snapshot        Relevance        State Cache

      ▼                ▼                ▼

 Serialization   Priority Queue   Delta Builder

                       ▼

                 Packet Builder

                       ▼

                 Multiplayer
```

Each component performs one well-defined task.

---

# 7. Snapshot Manager

The Snapshot Manager captures a consistent view of the authoritative simulation.

Snapshots include:

- entity state
- player state
- environmental state
- gameplay state

Snapshots are read-only representations.

They never modify simulation.

---

# 8. Relevance Manager

Not every entity should be replicated to every client.

The Relevance Manager determines:

- visibility
- interaction potential
- gameplay importance
- synchronization priority

Relevance is evaluated independently for each client.

---

# 9. State Cache

The State Cache stores previously transmitted information.

Responsibilities include:

- change detection
- delta generation
- redundancy elimination
- version tracking

The cache exists solely for replication efficiency.

---

# 10. Serialization Layer

Serialization converts authoritative state into transportable data.

Responsibilities include:

- object encoding
- state compression
- compatibility
- version handling

Serialization remains independent from packet transport.

---

# 11. Core Principles

Several architectural principles govern the Replication subsystem.

---

## Replication Is Read-Only

Replication never modifies simulation.

---

## Host Authority

Only the host generates replicated state.

---

## Relevance First

Only relevant information should consume bandwidth.

---

## Prediction Is Temporary

Predicted state is never authoritative.

---

## Deterministic Snapshots

Snapshots always represent completed simulation ticks.

---

## Transport Independence

Replication should remain compatible with multiple transport implementations.

---

# 12. Snapshot Architecture

## 12.1 Purpose

The Replication subsystem never reads directly from the live simulation while constructing network updates.

Instead, it operates exclusively on immutable snapshots of the authoritative world.

A snapshot represents a complete, internally consistent view of the simulation at a single point in time.

Snapshots isolate networking from ongoing simulation and guarantee that every client observes a coherent world state.

---

## 12.2 Design Philosophy

The simulation continuously changes.

Networking requires stability.

Snapshots bridge this difference.

Rather than exposing mutable engine data to the networking layer, the host performs the following sequence:

```
Simulation Tick

↓

Simulation Complete

↓

Commit World State

↓

Generate Snapshot

↓

Replication

↓

Packet Generation
```

Once a snapshot has been created, it is never modified.

---

## 12.3 Snapshot Characteristics

Every snapshot is:

- immutable
- deterministic
- versioned
- self-consistent
- authoritative
- read-only

These properties greatly simplify replication logic.

---

## 12.4 Snapshot Contents

A snapshot represents every piece of information required to reconstruct the authoritative world.

Typical contents include:

### World State

- simulation tick
- world time
- weather
- emission state
- environmental variables

---

### Entity State

- identifiers
- transforms
- animation state
- health
- inventory references
- interaction state

---

### Player State

- actor status
- equipment
- stamina
- active effects
- current level

---

### Dynamic Systems

- anomalies
- artifacts
- Smart Terrain occupancy
- story state
- faction updates

Not every client receives every portion of the snapshot.

Visibility is determined later by the Relevance Manager.

---

## 12.5 Snapshot Lifecycle

Every snapshot follows the same lifecycle.

```
Simulation Complete

↓

Allocate Snapshot

↓

Capture World

↓

Capture Entities

↓

Freeze Snapshot

↓

Relevance Evaluation

↓

Packet Construction

↓

Transmission

↓

Release
```

The lifecycle is intentionally linear.

Snapshots are never reopened after freezing.

---

## 12.6 Snapshot Boundaries

A snapshot always represents exactly one completed simulation tick.

It never spans multiple ticks.

Likewise, a snapshot never contains partially updated entities.

This guarantees temporal consistency.

```
Tick 100

↓

Snapshot 100

↓

Tick 101

↓

Snapshot 101
```

Each snapshot becomes the authoritative representation of its corresponding simulation tick.

---

## 12.7 Immutable Design

The immutable nature of snapshots provides several advantages.

- thread-safe reading
- deterministic packet generation
- replay capability
- simplified debugging
- rollback support (future)
- easier parallelization

The simulation never waits for packet generation once the snapshot has been frozen.

---

## 12.8 Snapshot Ownership

Ownership remains explicit.

| Component | Owner |
|-----------|------|
| Live Simulation | World / ALife |
| Snapshot Creation | Replication |
| Snapshot Reading | Replication |
| Snapshot Lifetime | Replication |
| Snapshot Transmission | Multiplayer |

The World never modifies frozen snapshots.

Replication never modifies live simulation.

---

## 12.9 Snapshot Versioning

Each snapshot receives a monotonically increasing identifier.

Conceptually:

```
Tick 100 → Snapshot 100

Tick 101 → Snapshot 101

Tick 102 → Snapshot 102
```

Version identifiers support:

- delta generation
- reconciliation
- diagnostics
- replay systems
- future rollback mechanisms

---

## 12.10 Snapshot Consistency

Consistency guarantees that all objects within a snapshot represent the same moment in simulation time.

For example:

Incorrect

```
NPC Position

Tick 100

Inventory

Tick 101
```

Correct

```
NPC Position

Tick 101

Inventory

Tick 101
```

Mixed simulation states are prohibited.

---

## 12.11 Snapshot Granularity

Snapshots exist at the world level.

However, replication may operate on smaller logical units.

Examples include:

- entity records
- player records
- environmental records
- Smart Terrain records

This separation allows efficient relevance filtering without compromising snapshot consistency.

---

## 12.12 Snapshot Production Pipeline

The host generates snapshots after simulation completes.

```
Simulation

↓

Commit

↓

Freeze World

↓

Snapshot Builder

↓

Entity Extraction

↓

Environment Extraction

↓

Snapshot Complete

↓

Relevance Evaluation
```

No gameplay code executes during snapshot production.

---

## 12.13 Snapshot Consumption

Clients never receive complete snapshots.

Instead:

```
Snapshot

↓

Relevance Filter

↓

Delta Builder

↓

Packet Builder

↓

Transport

↓

Client
```

Snapshots remain entirely server-side constructs.

Clients reconstruct their local world from replicated data.

---

## 12.14 Snapshot Invariants

Every snapshot guarantees:

- one simulation tick
- immutable state
- deterministic ordering
- complete entity consistency
- explicit version identifier
- read-only access

Violating these guarantees would undermine the entire replication architecture.

---

# 13. Entity Replication

## 13.1 Purpose

The World may contain thousands of persistent entities.

Replicating every entity to every client every tick would be prohibitively expensive.

The Replication subsystem therefore synchronizes entities selectively.

Replication is driven by relevance rather than existence.

---

## 13.2 Entity Lifecycle

Every replicated entity follows the same conceptual lifecycle.

```
Created

↓

Registered

↓

Relevant

↓

Replicated

↓

Updated

↓

Dormant

↓

Relevant Again

↓

Destroyed
```

The entity continues to exist even while dormant.

Only replication activity changes.

---

## 13.3 Replication States

An entity may exist in one of several replication states.

### Unknown

Client has never observed the entity.

---

### Initial

Initial state must be transmitted.

---

### Active

Entity is actively replicated.

---

### Dormant

Entity exists but currently receives minimal updates.

---

### Removed

Entity no longer exists or is no longer visible to the client.

---

## 13.4 Replication Pipeline

```
Snapshot

↓

Entity Extracted

↓

Relevance Check

↓

Priority Assignment

↓

Delta Evaluation

↓

Serialization

↓

Packet Builder
```

Each stage performs one clearly defined task.

---

## 13.5 Entity Records

Every replicated entity is represented by a replication record.

Conceptually:

```
Entity ID

Simulation Version

Replication State

Priority

Ownership

Last Sent Version
```

These records remain independent from gameplay objects.

---

## 13.6 State Categories

Different categories of state change at different rates.

Examples:

High Frequency

- position
- rotation
- animation

Medium Frequency

- inventory
- health
- equipment

Low Frequency

- faction
- story
- identity
- reputation

Separating categories allows more efficient bandwidth allocation.

---

## 13.7 Spawn Replication

When a client first becomes aware of an entity:

```
Entity Relevant

↓

Initial Record

↓

Serialize Complete State

↓

Transmit

↓

Client Creates Representation
```

The client must possess enough information to reconstruct the entity before incremental updates begin.

---

## 13.8 Update Replication

After the initial transmission:

```
Snapshot

↓

Compare Previous Version

↓

Changed Fields

↓

Serialize Delta

↓

Transmit
```

Only changed information is transmitted whenever practical.

---

## 13.9 Destruction

Entity removal follows a controlled sequence.

```
Entity Destroyed

↓

Snapshot Updated

↓

Destroy Record

↓

Client Removes Entity
```

Removal always originates from the authoritative simulation.

---

## 13.10 Replication Guarantees

The Replication subsystem guarantees:

- one authoritative entity state
- one active replication record per client
- deterministic update ordering
- explicit creation
- explicit destruction
- versioned updates

These guarantees provide the foundation for delta replication, prediction, and reconciliation.

---

# 14. State Ownership

## 14.1 Purpose

Replication can only function correctly when ownership is unambiguous.

Every replicated value must have exactly one authoritative source.

Without explicit ownership, clients may produce conflicting state, leading to desynchronization, duplication, or non-deterministic gameplay.

State ownership therefore forms the foundation of the entire synchronization architecture.

---

## 14.2 Ownership Philosophy

Replication never determines ownership.

Ownership already exists within the simulation.

Replication merely reflects it.

Conceptually:

```
World

↓

Owns Entity

↓

Simulation Changes Entity

↓

Replication Observes

↓

Clients Synchronize
```

Ownership originates in simulation.

Never in networking.

---

## 14.3 Ownership Categories

Persistent state falls into several ownership categories.

### World-Owned

Examples:

- world time
- weather
- emissions
- entity registry
- level state

---

### ALife-Owned

Examples:

- AI state
- Smart Terrain assignments
- faction plans
- ecology
- artifact lifecycle

---

### Gameplay-Owned

Examples:

- health
- inventory
- weapon state
- quests
- dialogue

---

### Client-Owned

Examples:

- camera orientation
- UI state
- prediction buffers
- interpolation history
- graphics settings

Client-owned data is never replicated as authoritative gameplay state.

---

## 14.4 Ownership Rules

The following rules apply throughout the project.

- Every replicated value has one authoritative owner.
- Clients never overwrite host-owned state.
- Replication never invents state.
- Simulation always precedes synchronization.
- Authority cannot be inferred; it must be explicit.

---

## 14.5 Ownership Transfer

Version 1 minimizes ownership transfers.

Most gameplay objects remain permanently host-owned.

Future versions may introduce ownership transfer for specialized systems, but persistent world state should continue to originate from the authoritative simulation.

---

# 15. Delta Replication

## 15.1 Purpose

Sending complete entity state every simulation tick would waste significant bandwidth.

Delta replication transmits only the information that has changed since the last acknowledged state.

This reduces bandwidth while preserving correctness.

---

## 15.2 Design Philosophy

Delta replication compares two authoritative snapshots.

It never compares client state.

```
Snapshot N

↓

Snapshot N+1

↓

Difference

↓

Serialize Delta

↓

Transmit
```

The comparison always occurs on the host.

---

## 15.3 Delta Generation

For every replicated entity:

```
Current Snapshot

↓

Previous Snapshot

↓

Field Comparison

↓

Changed Fields

↓

Delta Record
```

Only changed fields become candidates for transmission.

---

## 15.4 Field Categories

Different fields change at different rates.

### Continuous

- position
- rotation
- velocity

---

### Event-Driven

- weapon fire
- reload
- interaction
- dialogue

---

### Rare

- faction
- identity
- story flags
- reputation

Each category may use a different update policy.

---

## 15.5 Delta Invariants

Delta generation guarantees:

- deterministic comparison
- version tracking
- explicit additions
- explicit removals
- no implicit state creation

---

## 15.6 Recovery

If a required baseline is unavailable:

```
Missing Baseline

↓

Discard Delta

↓

Generate Full State

↓

Resume Delta Updates
```

Correctness always takes precedence over bandwidth savings.

---

# 16. Client Prediction

## 16.1 Purpose

Round-trip latency can make direct host-controlled movement feel unresponsive.

Client prediction improves responsiveness by temporarily estimating the results of local input before authoritative confirmation arrives.

Prediction exists only to improve presentation.

It never changes authoritative gameplay state.

---

## 16.2 Prediction Pipeline

```
Player Input

↓

Apply Local Prediction

↓

Render Immediate Result

↓

Send Input

↓

Await Host Response
```

The authoritative simulation continues independently on the host.

---

## 16.3 Prediction Scope

Prediction should remain limited to systems where immediate feedback is essential.

Typical candidates include:

- player movement
- camera transitions
- local animations

Persistent gameplay systems such as AI, inventory, or quests are never predicted authoritatively.

---

## 16.4 Prediction State

Predicted state is temporary.

It may be:

- confirmed
- corrected
- discarded

Clients must always be prepared to replace predicted state with the host's authoritative result.

---

## 16.5 Prediction Invariants

Prediction guarantees:

- no modification of authoritative state
- deterministic replay of local input
- immediate responsiveness
- automatic correction

Prediction must never become visible to other players until confirmed by the host.

---

# 17. Reconciliation

## 17.1 Purpose

Prediction occasionally differs from the host's authoritative simulation.

Reconciliation restores consistency between the client's predicted state and the host's confirmed state.

---

## 17.2 Reconciliation Pipeline

```
Receive Snapshot

↓

Compare Authoritative State

↓

Prediction Matches?

      │
 ┌────┴────┐
 │         │
 ▼         ▼
Yes        No
 │         │
 ▼         ▼
Continue   Correct
```

Corrections occur entirely on the client.

---

## 17.3 Correction Strategy

When divergence occurs:

```
Authoritative State

↓

Replace Predicted State

↓

Replay Pending Inputs

↓

Continue Prediction
```

This minimizes visible correction while preserving correctness.

---

## 17.4 Design Goals

Reconciliation should:

- minimize visual artifacts
- preserve responsiveness
- maintain deterministic gameplay
- avoid accumulating error

---

## 17.5 Architectural Principle

Only clients reconcile.

The host never reconciles to clients.

Authority flows in one direction.

---

# 18. Interpolation

## 18.1 Purpose

Snapshots arrive at discrete intervals.

Rendering occurs continuously.

Interpolation smooths the presentation of replicated objects between authoritative updates.

---

## 18.2 Concept

```
Snapshot A

──────────────

Snapshot B

↓

Interpolate

↓

Smooth Motion
```

Interpolation affects presentation only.

Simulation remains unchanged.

---

## 18.3 Interpolated Systems

Typical candidates include:

- remote player movement
- NPC movement
- animations
- object transforms

Critical gameplay events should use authoritative timing rather than visual interpolation.

---

## 18.4 Interpolation Buffer

Clients maintain a short history of received snapshots.

Conceptually:

```
Snapshot 120

↓

Snapshot 121

↓

Snapshot 122

↓

Render Between
```

The buffer allows smooth rendering despite network jitter.

---

## 18.5 Invariants

Interpolation guarantees:

- presentation only
- no gameplay modification
- smooth transitions
- deterministic rendering order

---

# 19. Interest Management

## 19.1 Purpose

Not every client requires knowledge of every entity in the Zone.

Interest management determines which subset of the authoritative world is relevant to each client.

This is one of the primary mechanisms used to scale multiplayer.

---

## 19.2 Design Philosophy

Interest is determined by the authoritative World.

Replication consumes this information.

Networking never performs gameplay relevance decisions.

---

## 19.3 Interest Pipeline

```
Snapshot

↓

Relevance Manager

↓

Visible Entity Set

↓

Priority Assignment

↓

Replication Queue
```

Only relevant entities proceed to serialization.

---

## 19.4 Relevance Factors

Possible relevance factors include:

- distance
- current level
- interaction potential
- combat
- scripted importance
- ownership
- observation range

The precise evaluation policy is owned by the World and Replication subsystems together.

---

## 19.5 Dynamic Interest

Interest changes continuously.

Triggers include:

- player movement
- level transitions
- combat
- entity creation
- entity destruction

Interest evaluation therefore occurs every replication cycle.

---

## 19.6 Design Benefits

Interest management provides:

- reduced bandwidth
- lower serialization cost
- smaller packet sizes
- improved scalability

Most importantly, it allows the authoritative simulation to remain much larger than the data transmitted to any individual client.

---

# 20. Packet Prioritization

## 20.1 Purpose

Available bandwidth is finite.

When more information is available than can be transmitted immediately, Replication must determine transmission order.

Packet prioritization ensures that gameplay-critical information reaches clients first.

---

## 20.2 Priority Categories

### Critical

- player state
- combat
- damage
- interactions

---

### High

- nearby NPCs
- active anomalies
- nearby physics objects

---

### Medium

- distant NPCs
- environmental updates
- artifact movement

---

### Low

- cosmetic state
- ambient activity
- long-range simulation updates

---

## 20.3 Priority Pipeline

```
Replication Queue

↓

Priority Assignment

↓

Bandwidth Budget

↓

Packet Builder

↓

Transport
```

Lower-priority updates may be deferred when necessary.

---

## 20.4 Fairness

Prioritization should avoid starvation.

Long-running low-priority entities must eventually receive synchronization even under sustained load.

The scheduler should balance urgency with long-term consistency.

---

## 20.5 Architectural Principle

Prioritization affects only transmission order.

It must never change simulation results or authoritative state.

---

# 21. Reliability Model

## 21.1 Purpose

Not all replicated information has the same delivery requirements.

Some state must arrive reliably and in order.

Other state becomes obsolete almost immediately and can safely be replaced by newer updates.

The Replication subsystem therefore classifies synchronization data according to reliability requirements rather than treating every message identically.

---

## 21.2 Design Philosophy

Reliability is a property of replicated information.

It is **not** a property of the transport alone.

The Replication subsystem determines *what* guarantees are required.

The Multiplayer transport determines *how* those guarantees are achieved.

---

## 21.3 Reliability Classes

Replication data is divided into logical reliability classes.

### Critical State

Examples:

- entity creation
- entity destruction
- inventory changes
- quest progression
- equipment changes
- story state

Characteristics:

- reliable
- ordered
- exactly-once semantics whenever practical

---

### Persistent Gameplay

Examples:

- health
- active effects
- anomaly interaction
- Smart Terrain assignments

Characteristics:

- reliable
- ordered

---

### Continuous State

Examples:

- movement
- rotation
- velocity
- animation

Characteristics:

- newer updates supersede older ones
- timeliness is more important than perfect delivery

---

### Cosmetic State

Examples:

- ambient animations
- visual-only effects
- non-essential presentation

Characteristics:

- lowest reliability requirements
- may be skipped under bandwidth pressure

---

## 21.4 Ordering Guarantees

Certain operations require strict ordering.

Example:

```
Spawn

↓

Equip Weapon

↓

Fire Weapon

↓

Destroy
```

Receiving these events in a different order would produce an invalid simulation.

The Replication subsystem therefore preserves ordering where required.

---

## 21.5 Reliability Invariants

The architecture guarantees:

- authoritative state is never lost due to replication policy
- entity creation precedes updates
- destruction follows all required updates
- inventory consistency is preserved
- story progression remains ordered

---

# 22. Performance Strategy

## 22.1 Philosophy

Replication exists on the critical path of every simulation tick.

Its design must therefore prioritize predictable execution time over maximum throughput.

Stable performance is preferable to occasional bursts of high bandwidth.

---

## 22.2 Performance Goals

The subsystem aims to achieve:

- bounded memory usage
- deterministic execution time
- stable packet generation
- efficient bandwidth utilization
- minimal serialization overhead

---

## 22.3 Primary Cost Centers

Major computational costs include:

- snapshot construction
- relevance evaluation
- delta generation
- serialization
- packet assembly

These systems should be profiled continuously during implementation.

---

## 22.4 Optimization Principles

Preferred optimization techniques include:

- immutable snapshots
- incremental delta generation
- cached relevance results
- efficient entity lookup
- memory pooling
- bounded packet queues

Optimizations should never alter replication correctness.

---

## 22.5 Scalability

The architecture is designed to scale through:

- selective relevance
- delta synchronization
- prioritization
- immutable snapshots
- independent packet generation

Simulation complexity and replication complexity remain intentionally separated.

---

# 23. Failure Recovery

## 23.1 Philosophy

Replication failures should degrade synchronization quality rather than compromise the authoritative simulation.

The World and ALife continue operating even if replication experiences temporary disruption.

---

## 23.2 Snapshot Failure

If snapshot construction cannot complete:

```
Snapshot Failed

↓

Discard Snapshot

↓

Retain Previous State

↓

Log Failure

↓

Retry Next Tick
```

Partially constructed snapshots are never transmitted.

---

## 23.3 Serialization Failure

If an entity cannot be serialized:

```
Serialization Error

↓

Skip Entity

↓

Log Diagnostic

↓

Continue Packet Generation
```

A single malformed entity should not prevent synchronization of unrelated entities.

---

## 23.4 Packet Failure

If packet construction exceeds the available bandwidth budget:

```
Packet Queue

↓

Priority Evaluation

↓

Transmit Highest Priority

↓

Defer Remaining Data
```

Deferred updates are reconsidered during the next replication cycle.

---

## 23.5 Client Desynchronization

When significant divergence is detected:

```
Detect Divergence

↓

Invalidate Client Cache

↓

Generate Fresh Baseline

↓

Resume Delta Updates
```

Recovery prioritizes correctness over bandwidth efficiency.

---

## 23.6 Failure Isolation

Replication failures should remain isolated.

Failures must never:

- modify simulation
- corrupt persistence
- alter ALife
- change authoritative ownership

The simulation continues even when synchronization quality temporarily decreases.

---

# 24. Future Extensions

## 24.1 Design Philosophy

The Replication architecture has been designed to evolve without requiring fundamental changes to the simulation.

Future improvements should extend the subsystem rather than replacing it.

---

## 24.2 Potential Enhancements

Future capabilities may include:

- adaptive bandwidth allocation
- multi-threaded packet generation
- region-based replication workers
- replay recording
- spectator support
- rollback debugging
- replication diagnostics
- bandwidth analytics
- compression improvements
- cloud-hosted dedicated servers

Each enhancement should preserve the existing ownership and snapshot contracts.

---

## 24.3 Replay Systems

Because replication is built around immutable snapshots, future replay functionality can consume the same snapshot stream used by networking.

This avoids introducing a separate recording pipeline.

---

## 24.4 Spectators

Spectator clients become an alternative consumer of replicated snapshots.

The simulation remains unchanged.

Only relevance rules differ.

---

# 25. Design Rationale

The Replication subsystem deliberately separates synchronization from gameplay.

Rather than embedding networking concerns within ALife or gameplay systems, Replication observes completed simulation ticks and converts them into efficient, client-specific synchronization streams.

Several architectural decisions support this objective.

### Immutable Snapshots

Guarantee consistency and simplify parallelism.

---

### Explicit Ownership

Ensure that every replicated value has one authoritative source.

---

### Relevance Evaluation

Reduce bandwidth without affecting simulation.

---

### Delta Synchronization

Transmit only meaningful changes whenever practical.

---

### Prediction and Reconciliation

Improve responsiveness while preserving host authority.

---

### Layered Architecture

Simulation, Replication, Multiplayer, and Transport remain independent subsystems with clearly defined contracts.

---

# 26. Summary

The Replication subsystem synchronizes the authoritative state of the Zone from the host to every connected client while remaining entirely separate from gameplay simulation.

It achieves this through immutable snapshots, explicit ownership, relevance evaluation, delta synchronization, prediction support, reconciliation, interpolation, and prioritized packet generation.

Several principles define the subsystem:

- Simulation is authoritative.
- Replication is read-only.
- Snapshots are immutable.
- Ownership is explicit.
- Relevance precedes synchronization.
- Correctness precedes optimization.

By maintaining these architectural boundaries, STALKER-MP preserves the integrity of the original X-Ray simulation while enabling efficient, scalable multiplayer synchronization.

The Replication subsystem therefore forms the bridge between the authoritative simulation and the Multiplayer transport layer, completing the networking architecture of STALKER-MP.
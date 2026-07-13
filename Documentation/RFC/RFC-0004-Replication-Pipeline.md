# RFC-0004: Replication Pipeline
# RFC-0004: Replication Pipeline

| Field | Value |
|--------|-------|
| RFC | 0004 |
| Title | Replication Pipeline |
| Status | Accepted |
| Author | STALKER-MP Project |
| Created | July 2026 |
| Depends On | RFC-0001, RFC-0002, RFC-0003, 06_Multiplayer.md, 07_Replication.md |
| Supersedes | None |
| Superseded By | None |

---

# 1. Abstract

This RFC defines the Replication Pipeline used by STALKER-MP.

Replication is responsible for transporting authoritative simulation state from the host to connected clients. It does not execute gameplay logic, own simulation state, or determine entity behavior.

The replication pipeline consumes immutable snapshots produced by the Simulation Thread and transforms them into prioritized network updates suitable for client synchronization.

This architecture preserves the project's fundamental principle:

> Networking transports simulation.
> Simulation never depends on networking.

---

# 2. Motivation

The X-Ray Engine was originally designed around a single local simulation.

In multiplayer, clients require a synchronized view of the authoritative world.

However, exposing live simulation objects directly to the networking layer would:

- violate subsystem ownership,
- introduce race conditions,
- require locking,
- reduce determinism,
- tightly couple networking with gameplay.

Replication must therefore operate entirely on immutable snapshots.

---

# 3. Problem Statement

The multiplayer system must continuously synchronize:

- NPCs
- Players
- Physics
- Inventory
- Environment
- World events

while satisfying the following constraints:

- simulation cannot stall for networking;
- clients cannot modify authoritative state;
- bandwidth is limited;
- latency is unavoidable;
- packet loss must be tolerated.

A dedicated replication architecture is therefore required.

---

# 4. Goals

The replication pipeline must:

- preserve host authority;
- consume immutable snapshots only;
- avoid simulation locks;
- minimize bandwidth;
- prioritize important state;
- tolerate latency;
- tolerate packet loss;
- support Join-In-Progress;
- scale to future networking improvements.

---

# 5. Non-Goals

This RFC does not define:

- packet formats;
- socket implementation;
- serialization layout;
- compression algorithms;
- encryption;
- matchmaking;
- NAT traversal.

These belong to implementation.

---

# 6. Decision

Replication is implemented as an asynchronous pipeline.

The Simulation Thread produces immutable snapshots.

The Replication Worker transforms snapshots into network updates.

Clients receive updates.

Clients reconstruct presentation state.

No networking component modifies authoritative simulation.

---

# 7. Pipeline Responsibilities

The replication subsystem is responsible for:

- snapshot consumption;
- entity prioritization;
- delta generation;
- packet scheduling;
- reliability management;
- client synchronization.

The replication subsystem is **not** responsible for:

- gameplay;
- AI;
- physics;
- inventory;
- story progression;
- world ownership.

---

# 8. Pipeline Stages

The replication pipeline consists of several logical stages.

---

## Stage 1 — Snapshot Acquisition

The Replication Worker receives a completed immutable snapshot.

No live simulation data is accessed.

---

## Stage 2 — Interest Filtering

Only entities relevant to a client are considered.

Interest management determines visibility.

Bubble ownership remains defined by RFC-0002.

---

## Stage 3 — Change Detection

Replication compares the current snapshot against the previously acknowledged state.

Only meaningful changes are selected.

---

## Stage 4 — Prioritization

Updates are ordered according to gameplay importance.

Examples include:

- player state;
- nearby NPCs;
- combat events;
- physics interactions.

Lower-priority updates may be delayed.

---

## Stage 5 — Packet Assembly

Selected updates are organized into outbound packets.

Packet layout is an implementation detail.

---

## Stage 6 — Transmission

Packets are sent asynchronously.

The Simulation Thread continues immediately.

Networking never blocks simulation.

---

# 9. Design Rationale

## Separation of Concerns

Replication exists only to synchronize simulation.

Simulation should never contain networking logic.

---

## Snapshot Isolation

Snapshots prevent networking from reading partially updated entities.

Workers operate independently.

---

## Bandwidth Efficiency

Only relevant changes should be transmitted.

Sending complete world state every frame is unnecessary.

---

## Scalability

The pipeline naturally supports:

- additional players;
- higher update rates;
- future compression;
- future replication strategies.

---

## Determinism

Networking cannot influence simulation execution.

Packet delay affects presentation only.

Authoritative state remains unchanged.

---

# 10. Interest Management

Replication does not transmit every entity.

Each client receives only entities relevant to its current gameplay context.

Interest management is based on:

- Bubble Manager activation.
- Player visibility.
- Gameplay relevance.

The replication layer consumes these decisions.

It does not create them.

---

# 11. Reliability Model

Different gameplay events require different delivery guarantees.

Examples include:

Reliable:

- inventory changes;
- quest progression;
- entity creation;
- entity destruction.

Unreliable:

- movement;
- animation;
- rotation;
- velocity.

Reliability policy belongs to replication.

Simulation remains unaware.

---

# 12. Alternatives Considered

## Full World Replication

Rejected.

Bandwidth requirements become excessive.

---

## Simulation-Owned Networking

Rejected.

Violates subsystem ownership.

Creates unnecessary coupling.

---

## Client Pull Model

Rejected.

Clients should not request authoritative simulation.

The host determines synchronization.

---

## Shared Simulation Objects

Rejected.

Would require locking and violate RFC-0003.

---

# 13. Consequences

Positive:

- lock-free networking;
- deterministic simulation;
- modular architecture;
- scalable replication;
- efficient bandwidth usage.

Negative:

- additional snapshot processing;
- increased worker complexity;
- delta tracking overhead.

These costs are acceptable.

---

# 14. Interaction with Other Systems

## World

Produces simulation state.

Never performs networking.

---

## ALife

Unaware of replication.

---

## Bubble Manager

Determines active entities.

Replication consumes activation decisions.

---

## Persistence

Independent subsystem.

Consumes separate snapshots.

---

## Threading

Replication executes on worker threads.

Simulation never waits.

---

# 15. Future Evolution

The pipeline may later support:

- adaptive update rates;
- packet compression;
- client-side rewind;
- replay streaming;
- spectator mode;
- dedicated relay servers.

These improvements must preserve:

- host authority;
- immutable snapshots;
- subsystem ownership.

---

# 16. Risks

Potential risks include:

- bandwidth spikes;
- snapshot backlog;
- excessive delta computation;
- packet fragmentation;
- client desynchronization.

Mitigation strategies include:

- packet prioritization;
- bandwidth budgeting;
- snapshot throttling;
- interest management;
- acknowledgment tracking.

---

# 17. Acceptance Criteria

The Replication Pipeline is considered correctly implemented when:

- only snapshots are replicated;
- networking never accesses live simulation;
- clients never modify authoritative state;
- simulation continues regardless of network latency;
- only relevant entities are transmitted;
- packet loss cannot corrupt authoritative simulation.

---

# 18. References

Architecture Documents

- 06_Multiplayer.md
- 07_Replication.md

Related RFCs

- RFC-0001 — Host Authoritative Simulation
- RFC-0002 — MultiPlayer Bubble System
- RFC-0003 — Immutable Snapshot Architecture
- RFC-0005 — Persistence Architecture
- RFC-0006 — Threading Execution Model
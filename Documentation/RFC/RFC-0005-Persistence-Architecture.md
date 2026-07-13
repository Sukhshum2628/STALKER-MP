# RFC-0005: Persistence Architecture
# RFC-0005: Persistence Architecture

| Field | Value |
|--------|-------|
| RFC | 0005 |
| Title | Persistence Architecture |
| Status | Accepted |
| Author | STALKER-MP Project |
| Created | July 2026 |
| Depends On | RFC-0001, RFC-0002, RFC-0003, 04_World.md, 08_Persistence.md |
| Supersedes | None |
| Superseded By | None |

---

# 1. Abstract

This RFC defines the persistence architecture for STALKER-MP.

Persistence is responsible for preserving the authoritative state of the Zone across application restarts. It consumes immutable snapshots produced by the Simulation Thread and serializes them into durable storage.

Persistence never participates in gameplay execution, simulation scheduling, networking, or entity ownership.

The persistence subsystem exists solely to ensure that the authoritative simulation can be safely restored without compromising determinism or subsystem ownership.

---

# 2. Motivation

One of the primary goals of STALKER-MP is to provide a persistent multiplayer Zone.

The world must continue evolving across multiple play sessions while preserving:

- ALife progression
- NPC state
- Player progression
- Inventory
- Story progression
- Smart Terrain assignments
- World environment
- Faction simulation

Without persistence, every server restart would reset the entire simulation.

Persistence therefore becomes a core architectural subsystem.

---

# 3. Problem Statement

Saving the world introduces several challenges.

The save system must:

- avoid interrupting gameplay;
- never serialize partially updated state;
- preserve deterministic simulation;
- support future version upgrades;
- recover safely from failed saves;
- remain independent of networking.

The architecture therefore requires persistence to operate asynchronously using immutable snapshots.

---

# 4. Goals

The persistence architecture must:

- preserve one authoritative world;
- serialize consistent simulation state;
- avoid blocking the Simulation Thread;
- support automatic saves;
- support manual saves;
- support dedicated servers;
- support version migration;
- tolerate interrupted save operations.

---

# 5. Non-Goals

This RFC does not define:

- file formats;
- database schemas;
- compression methods;
- encryption;
- cloud synchronization;
- backup policies.

These belong to implementation.

---

# 6. Decision

Persistence is implemented as an asynchronous consumer of immutable snapshots.

The Simulation Thread periodically generates persistence snapshots.

The Persistence Worker serializes snapshots independently.

The Simulation Thread never performs file I/O.

The persistence subsystem never modifies live simulation.

---

# 7. Ownership

Persistence owns:

- serialization;
- save scheduling;
- version metadata;
- recovery validation;
- storage management.

Persistence does **not** own:

- gameplay;
- ALife;
- networking;
- world simulation;
- entity behavior.

Authoritative ownership remains with the Simulation Thread.

---

# 8. Save Pipeline

The persistence pipeline consists of several logical stages.

---

## Stage 1 — Snapshot Creation

The Simulation Thread produces a complete persistence snapshot.

The snapshot represents one finished simulation tick.

---

## Stage 2 — Queue Submission

The snapshot is transferred to the Persistence Worker.

Ownership of live simulation never changes.

---

## Stage 3 — Serialization

The Persistence Worker converts the snapshot into persistent storage.

Serialization occurs independently of gameplay.

---

## Stage 4 — Validation

The completed save is validated.

Metadata is verified before becoming the active save.

---

## Stage 5 — Commit

The save becomes the newest authoritative persistent state.

Older saves remain available according to implementation policy.

---

# 9. Design Rationale

## Preserve Determinism

Saving must never interrupt simulation.

Snapshots guarantee a consistent world state.

---

## Avoid Simulation Stalls

Disk I/O is unpredictable.

Executing serialization on the Simulation Thread would introduce frame spikes.

Asynchronous persistence eliminates this problem.

---

## Maintain Ownership

Persistence consumes simulation state.

It never produces gameplay state.

This preserves the ownership model established in RFC-0001.

---

## Simplify Recovery

Each save represents one complete simulation state.

Loading restores the world directly.

No reconstruction from incremental history is required.

---

# 10. Save Scope

Persistence includes:

- World state
- Entity registry
- NPC state
- Player state
- Inventory
- Smart Terrain
- Story progression
- ALife scheduler
- Environment
- Weather
- Simulation time

Transient runtime systems are excluded.

Examples include:

- active network connections;
- replication queues;
- worker thread state;
- temporary caches;
- diagnostic information.

---

# 11. Autosave Strategy

The architecture supports multiple save triggers.

Examples include:

- periodic autosave;
- manual save;
- administrative save;
- graceful server shutdown.

The save trigger mechanism remains independent of serialization.

---

# 12. Versioning

Every save must include version metadata.

Version information allows future engine revisions to:

- detect incompatible saves;
- perform migration;
- reject unsupported formats.

Version compatibility belongs to persistence rather than gameplay systems.

---

# 13. Recovery

Loading follows the reverse process.

1. Validate metadata.
2. Load persistent snapshot.
3. Reconstruct world state.
4. Restore entity registry.
5. Restore ALife.
6. Resume authoritative simulation.

Gameplay resumes only after recovery is complete.

---

# 14. Alternatives Considered

## Direct Serialization from Simulation

Rejected.

Reason:

Simulation would stall during disk I/O.

---

## Database-Backed World

Rejected.

Reason:

Adds unnecessary complexity.

The project requires portable save files rather than continuous database synchronization.

---

## Incremental Save Journaling

Rejected.

Reason:

Recovery complexity outweighs benefits for the current project scope.

---

## Live Object Serialization

Rejected.

Reason:

Violates RFC-0003 by exposing mutable simulation state.

---

# 15. Consequences

Positive:

- consistent saves;
- asynchronous serialization;
- deterministic recovery;
- clear subsystem ownership;
- portable world saves.

Negative:

- temporary memory overhead;
- snapshot creation cost;
- save queue management.

These costs are acceptable in exchange for deterministic persistence.

---

# 16. Interaction with Other Systems

## World

Produces authoritative state.

Never performs serialization.

---

## ALife

Restored from persistence.

Never interacts directly with storage.

---

## Replication

Independent subsystem.

Does not participate in save generation.

---

## Threading

Persistence executes entirely on worker threads.

Simulation never waits for disk operations.

---

## Plugins

Future plugins may contribute persistent data through extension APIs.

Plugin ownership remains isolated from core persistence.

---

# 17. Future Evolution

The persistence architecture may later support:

- incremental saves;
- snapshot compression;
- cloud synchronization;
- world backups;
- replay checkpoints;
- distributed storage.

Future enhancements must preserve:

- immutable snapshots;
- host authority;
- asynchronous execution;
- deterministic recovery.

---

# 18. Risks

Potential risks include:

- corrupted save files;
- interrupted save operations;
- storage exhaustion;
- version incompatibility;
- excessive save frequency.

Mitigation strategies include:

- temporary save files;
- atomic file replacement;
- save validation;
- rolling backups;
- version migration.

---

# 19. Acceptance Criteria

The Persistence Architecture is considered correctly implemented when:

- only immutable snapshots are serialized;
- simulation never performs disk I/O;
- persistence never modifies gameplay state;
- saves restore complete world state;
- interrupted saves cannot corrupt existing saves;
- loading always reconstructs one authoritative simulation.

---

# 20. References

Architecture Documents

- 04_World.md
- 08_Persistence.md

Related RFCs

- RFC-0001 — Host Authoritative Simulation
- RFC-0002 — MultiPlayer Bubble System
- RFC-0003 — Immutable Snapshot Architecture
- RFC-0006 — Threading Execution Model
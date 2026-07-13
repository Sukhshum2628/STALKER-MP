# RFC-0002: MultiPlayer Bubble System
# RFC-0002: MultiPlayer Bubble System

| Field | Value |
|--------|-------|
| RFC | 0002 |
| Title | MultiPlayer Bubble System |
| Status | Accepted |
| Author | STALKER-MP Project |
| Created | July 2026 |
| Depends On | RFC-0001, 04_World.md, 05_ALife.md, 06_Multiplayer.md |
| Supersedes | None |
| Superseded By | None |

---

# 1. Abstract

This RFC defines the Multiplayer Bubble System used by STALKER-MP.

The original X-Ray Engine activates simulation around a single actor using a distance-based online/offline transition. STALKER-MP extends this mechanism to support multiple connected players without modifying ALife itself.

Rather than introducing player-owned simulation zones or partitioning the world, the system computes a unified simulation bubble representing the combined influence of all connected players.

The resulting activation region remains owned by the World subsystem, preserving ALife's assumption that entity activation is determined solely by world state.

This RFC establishes the architectural reasoning behind multiplayer-aware activation and forms the basis for replication, scheduling, and persistence decisions described in later RFCs.

---

# 2. Motivation

The original engine assumes the existence of one player.

Every entity transitions between offline and online simulation based on its distance to that player.

Introducing multiple players invalidates this assumption.

Without a generalized activation model, several problems arise:

- Different clients would activate different NPCs.
- ALife execution could diverge.
- Physics ownership becomes ambiguous.
- Story events may execute inconsistently.
- Replication would require conflicting authority.

The multiplayer system therefore requires a generalized activation mechanism while preserving the existing ALife implementation.

---

# 3. Problem Statement

The X-Ray Engine was never designed to answer the question:

> "Which entities should be online when multiple human players exist?"

A naïve implementation would perform activation independently for every player.

This creates duplicate simulation.

Another approach would assign ownership of regions to individual players.

This fragments the persistent world and violates the architectural goal of one authoritative simulation.

A new activation model is therefore required.

---

# 4. Goals

The Multiplayer Bubble System must:

- Preserve one persistent world.
- Preserve one ALife simulation.
- Support any number of connected players.
- Maintain deterministic activation.
- Avoid duplicated AI execution.
- Minimize changes to existing engine systems.
- Scale naturally as player count increases.

---

# 5. Non-Goals

This RFC does not:

- Partition the world.
- Assign ownership to players.
- Create separate simulations.
- Introduce instancing.
- Modify ALife scheduling logic.
- Replace existing online/offline simulation.

---

# 6. Decision

The project adopts a unified multiplayer bubble.

Every connected player contributes an activation radius.

The World subsystem computes the union of all active regions.

Entities intersecting this combined region become online.

Entities outside the combined region remain offline.

ALife receives only the final activation result.

It is unaware that multiple players exist.

---

# 7. Bubble Ownership

The Bubble Manager belongs to the World subsystem.

It is intentionally not part of:

- Networking
- ALife
- Replication
- Physics
- Scheduler

This preserves subsystem ownership boundaries.

The World determines which entities exist in the active simulation.

ALife determines how active entities behave.

Networking merely synchronizes results.

---

# 8. Design Rationale

## Preserve ALife

The ALife planner should not understand networking.

It should continue operating exactly as if one simulation exists.

Changing ALife to become multiplayer-aware would significantly increase engine complexity and reduce compatibility with future engine updates.

---

## Preserve World Ownership

Activation is fundamentally a world-level concern.

Whether an entity is online depends on where players exist within the world.

Therefore activation belongs to the World subsystem rather than ALife or Networking.

---

## Deterministic Activation

The same player positions must always produce the same activation set.

Bubble generation therefore depends only on authoritative world state.

Clients never compute authoritative activation.

---

## Scalability

The activation algorithm naturally scales with additional players.

No architectural changes are required as player count increases.

Only the size and shape of the active region changes.

---

# 9. Alternatives Considered

## Independent Player Bubbles

Rejected.

Each player would activate different entities.

This creates conflicting simulation ownership.

---

## Player-Owned Regions

Rejected.

Would divide the persistent world into multiple authorities.

Violates RFC-0001.

---

## Networking-Owned Activation

Rejected.

Networking transports simulation state.

It should never decide which entities are simulated.

---

## ALife-Owned Bubble Logic

Rejected.

ALife should remain responsible only for behavior.

World activation should remain external.

---

# 10. Consequences

Positive:

- One simulation.
- Preserves original ALife.
- Deterministic activation.
- Clear ownership boundaries.
- Easy replication integration.
- Supports Join-In-Progress naturally.

Negative:

- Large player groups activate more entities.
- Increased CPU usage near populated areas.
- Requires efficient scheduler implementation.

These costs are acceptable given the architectural goals.

---

# 11. Interaction with Other Systems

## ALife

Receives only the final online/offline state.

Never computes multiplayer logic.

---

## Scheduler

Executes only entities activated by the Bubble Manager.

---

## Replication

Replicates only active entities within player interest.

Defined in RFC-0004.

---

## Persistence

Bubble state is transient.

It is not serialized.

Persistent state belongs to entities, not activation regions.

---

## Threading

Bubble computation occurs on the Simulation Thread.

Workers consume immutable activation snapshots.

Defined in RFC-0006.

---

# 12. Risks

Potential risks include:

- Excessive entity activation in dense multiplayer sessions.
- Bubble fragmentation.
- Frequent online/offline transitions.
- Increased scheduler workload.

Mitigation strategies include:

- Efficient spatial partitioning.
- Activation hysteresis.
- Batched transitions.
- Priority scheduling.

---

# 13. Acceptance Criteria

The Multiplayer Bubble System is considered correctly implemented when:

- Every entity has one authoritative activation state.
- ALife remains unaware of player count.
- Multiple players expand the active simulation naturally.
- Bubble ownership belongs exclusively to the World subsystem.
- No duplicate simulation occurs.
- Online/offline transitions remain deterministic.

---

# 14. References

Architecture Documents

- 04_World.md
- 05_ALife.md
- 06_Multiplayer.md

Related RFCs

- RFC-0001 — Host Authoritative Simulation
- RFC-0003 — Immutable Snapshot Architecture
- RFC-0004 — Replication Pipeline
- RFC-0006 — Threading Execution Model
# RFC-0001: Host Authoritative Simulation
# RFC-0001: Host Authoritative Simulation

| Field | Value |
|--------|-------|
| RFC | 0001 |
| Title | Host Authoritative Simulation |
| Status | Accepted |
| Author | STALKER-MP Project |
| Created | July 2026 |
| Depends On | Architecture 02_Engine, 03_Player, 04_World, 05_ALife, 06_Multiplayer |
| Supersedes | None |
| Superseded By | None |

---

# 1. Abstract

This RFC defines the authoritative simulation model for STALKER-MP.

The project adopts a fully host-authoritative architecture in which the host machine owns the complete persistent game simulation. Connected clients participate only by providing player input and rendering synchronized world state.

This decision ensures deterministic ALife execution, preserves the original X-Ray Engine architecture, prevents simulation divergence, simplifies persistence, and provides a single source of truth for the entire Zone.

This RFC serves as the foundational engineering decision upon which all subsequent networking, replication, persistence, and threading RFCs depend.

---

# 2. Motivation

The X-Ray Engine was designed around a single authoritative simulation.

Nearly every gameplay system assumes a single owner responsible for:

- ALife scheduling
- Physics
- Smart Terrain
- Story progression
- Inventory
- NPC AI
- Faction simulation
- Environmental systems

Attempting to distribute authority across multiple machines would require rewriting significant portions of the engine and would fundamentally alter the behavior of the Zone.

Instead, STALKER-MP extends the existing architecture by allowing multiple human players to participate inside one authoritative simulation rather than introducing multiple competing simulations.

---

# 3. Problem Statement

Multiplayer introduces multiple machines capable of executing simulation logic.

Without a clearly defined authority model, the following issues arise:

- Divergent ALife state
- Duplicate AI execution
- Conflicting inventory ownership
- Story inconsistencies
- Physics desynchronization
- Save corruption
- Network race conditions

The project requires a deterministic authority model that guarantees only one simulation exists at any point in time.

---

# 4. Goals

This RFC aims to:

- Preserve the original ALife architecture.
- Maintain one persistent world.
- Ensure deterministic simulation.
- Eliminate conflicting ownership.
- Minimize engine modifications.
- Simplify persistence.
- Support Join-In-Progress.
- Enable dedicated server deployment.

---

# 5. Non-Goals

This RFC does not attempt to:

- Introduce peer-to-peer networking.
- Synchronize multiple simulations.
- Distribute AI across clients.
- Share authority dynamically.
- Replace ALife.
- Replace X-Ray scheduling.

---

# 6. Decision

STALKER-MP adopts a Host Authoritative Simulation model.

Exactly one simulation instance exists.

Exactly one scheduler executes.

Exactly one ALife planner owns AI.

Exactly one persistent world exists.

Clients never execute authoritative gameplay logic.

---

# 7. Authority Ownership

## Host Owns

- World State
- ALife
- Scheduler
- Physics
- Story
- Inventory
- Smart Terrain
- Faction Simulation
- Save System
- Time
- Weather
- Spawn Logic
- Entity Registry

The host is the only machine capable of modifying persistent state.

---

## Clients Own

Clients own only local presentation.

Including:

- Input
- Camera
- Rendering
- Animation blending
- Audio
- UI
- Prediction
- Interpolation

Clients never create persistent gameplay state.

---

# 8. Design Rationale

Host authority was selected because it provides several architectural advantages.

## Determinism

Only one machine executes simulation.

Simulation order cannot diverge.

---

## Engine Compatibility

The existing X-Ray engine already assumes one simulation owner.

This minimizes engine modifications.

---

## Persistence

Saving becomes straightforward.

Only one machine serializes the world.

---

## Join-In-Progress

New players simply synchronize with the host.

No simulation merge is required.

---

## Security

Clients cannot modify authoritative game state.

Cheating surface is significantly reduced.

---

# 9. Alternatives Considered

## Peer-to-Peer Authority

Rejected.

Reason:

- Non-deterministic.
- Difficult synchronization.
- Poor scalability.
- Large engine rewrite.

---

## Distributed Simulation

Rejected.

Reason:

ALife contains numerous global dependencies.

Partitioning simulation ownership would require redesigning nearly every gameplay system.

---

## Client Authority

Rejected.

Reason:

Breaks persistence.

Breaks determinism.

Large cheating surface.

---

## Lockstep Networking

Rejected.

Reason:

High latency.

Poor scalability.

Unsuitable for open-world simulation.

---

# 10. Consequences

Positive:

- Single source of truth.
- Deterministic execution.
- Easier debugging.
- Easier saves.
- Easier replication.
- Easier threading.

Negative:

- Host performs most computation.
- Increased upstream bandwidth.
- Host failure terminates session.
- Requires efficient replication.

These trade-offs are considered acceptable given the project's goals.

---

# 11. Impact on Future RFCs

This RFC establishes assumptions for:

RFC-0002

- Bubble ownership

RFC-0003

- Snapshot ownership

RFC-0004

- Replication pipeline

RFC-0005

- Save authority

RFC-0006

- Simulation thread ownership

RFC-0007

- Extension API restrictions

Future RFCs must not violate this authority model.

---

# 12. Risks

Potential risks include:

- Host CPU bottlenecks
- Host network bandwidth limitations
- Large player counts
- Simulation stalls

Mitigation strategies are defined in later RFCs.

---

# 13. Acceptance Criteria

The authority model is considered correctly implemented when:

- Only the host executes ALife.
- Clients never modify persistent entities.
- Players can join an existing session.
- Save files originate only from the host.
- Simulation remains deterministic.
- Disconnecting a client does not alter world ownership.

---

# 14. References

Architecture Documents

- 02_Engine.md
- 03_Player.md
- 04_World.md
- 05_ALife.md
- 06_Multiplayer.md

Related RFCs

- RFC-0002
- RFC-0003
- RFC-0004
- RFC-0005
- RFC-0006
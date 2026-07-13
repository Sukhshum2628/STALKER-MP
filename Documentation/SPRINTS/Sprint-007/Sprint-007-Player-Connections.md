# Sprint-007: Player Connections
# Sprint-007: Player Lifecycle

---

# Sprint Information

| Field | Value |
|--------|-------|
| Sprint | 007 |
| Title | Player Lifecycle |
| Status | **Implemented & Verified (Closed)** — 2026-07-11 |
| Priority | Critical |
| Estimated Duration | 3 Weeks |
| Prerequisites | Sprint-001 through Sprint-006 |
| Next Sprint | Sprint-008 – Snapshot System |

---

# Closure Summary (2026-07-11)

Sprint-007 (Player Lifecycle) is **Implemented / Verified / Closed / Frozen**. All 15 steps were implemented and each independently verified by Antigravity, with the tree buildable after every step.

- **Tests:** 315 passing (Release x64, MSVC; GCC test build) — the Sprint-006 baseline of 238 plus the Sprint-007 player suite; 0 new warnings; no regressions.
- **Build:** MSVC Release x64 clean under `EngineAbi.props`; GCC test build clean. The engine player-spawn TU compiles under `EngineAbi.props`; the engine-free test build links `NullPlayerSpawnGateway`.
- **Boundaries:** One Engine Boundary intact — the sole new engine TU is `adapters::EnginePlayerSpawnGateway` in `EngineAdapters.cpp`; One Platform Boundary untouched (no OS header added). The `player::` subsystem is engine-free / OS-free.
- **Tick order:** `PlayerManagerService` ticks once at `tick_order::kPlayerLifecycle = 250`, strictly between EntityRegistry (200) and Bubble (300); six frame subscribers; reverse-order teardown.
- **Ownership:** the player is a persistent Entity-Registry-owned World entity; the subsystem owns only the mapping + lifecycle; online/offline is reused from Sprint-005 (ADR-008 unbroken); no Sprint-002–006 ownership changed.
- **Determinism:** single-threaded, tick-derived; enqueue-only session ingress applied at 250; replay-identical (verified by the churn and integration replay tests).
- **Messages:** additive DATA-range ids `kMsgPlayerJoinRequest = 0x0100` and `kMsgPlayerRespawnRequest = 0x0101` registered (duplicate → Initialize failure); ADR-010 unchanged.
- **ADRs:** ADR-007 / ADR-008 / ADR-009 / ADR-010 / ADR-011 preserved.
- **Subsystem doc:** `Multiplayer/docs/PlayerLifecycle.md`.
- **Deferred (future sprints only):** reconnect activates when Sprint-012 enables `net::Session::TryReclaim` (Sprint-006 stub today); the position/state feed is the Sprint-008 replication source; the reconstructable `PlayerRecord` awaits Sprint-011/012 durable persistence.
- **Next:** Sprint-008 (Snapshot System).

# 1. Sprint Overview

Sprint-007 introduces persistent player entities into the authoritative simulation.

Rather than treating connected users as temporary network objects, STALKER-MP represents every player as a persistent entity owned by the World subsystem and registered within the Entity Registry.

This sprint defines the complete player lifecycle including joining, spawning, synchronization, disconnecting, reconnecting, death, respawn, and removal.

The player lifecycle remains entirely host-authoritative.

Clients request actions.

The host owns every state transition.

---

# 2. Objectives

## Primary Objectives

- Create Player Manager
- Create Player Entity
- Implement Join pipeline
- Implement Leave pipeline
- Implement Spawn pipeline
- Implement Respawn pipeline
- Implement Reconnect pipeline
- Integrate World and Registry

## Secondary Objectives

- Player diagnostics
- Player statistics
- Debug visualization
- Validation framework

---

# 3. Scope

Included

- Player Manager
- Player Registry
- Player lifecycle
- Spawn management
- Disconnect handling
- Reconnect handling
- Player validation
- Registry integration

---

# 4. Out of Scope

Not Included

- Snapshot replication
- Inventory synchronization
- Prediction
- Interpolation
- Voice Chat
- Chat System
- Trading
- Co-op mechanics

---

# 5. Architecture References

- 03_Player.md
- 04_World.md
- 06_Multiplayer.md

---

# 6. RFC References

- RFC-0001 — Host Authoritative Simulation
- RFC-0002 — MultiPlayer Bubble System
- RFC-0004 — Replication Pipeline
- RFC-0005 — Persistence Architecture

---

# 7. Technical Tasks

---

## 7.1 Player Manager

Create

- PlayerManager
- PlayerRegistry
- PlayerSession
- PlayerConfiguration

Responsibilities

- Player creation
- Player removal
- Spawn management
- Session ownership
- State validation

---

## 7.2 Player Entity

Create

PlayerEntity

Maintain

- Player ID
- Entity ID
- Session ID
- Character Profile
- Spawn Position
- Current State
- Connection State

Player entities are owned by the World.

---

## 7.3 Join Pipeline

Implement

RequestJoin()

ValidateJoin()

AllocatePlayer()

CreateEntity()

RegisterEntity()

AssignSession()

NotifyWorld()

Join order must remain deterministic.

---

## 7.4 Spawn Pipeline

Implement

SelectSpawn()

ValidateSpawn()

CreateActor()

RegisterOnline()

NotifyBubbleManager()

NotifyScheduler()

---

## 7.5 Disconnect Pipeline

Implement

Disconnect()

SuspendPlayer()

PreserveCharacter()

MaintainPersistence()

ReleaseNetworkResources()

Player entity remains persistent.

---

## 7.6 Reconnect Pipeline

Implement

Reconnect()

Authenticate()

LocatePlayer()

RestoreSession()

RestoreControl()

Reconnect must never duplicate entities.

---

## 7.7 Death Pipeline

Implement

PlayerDeath()

DropControl()

NotifySystems()

ScheduleRespawn()

PersistDeathState()

No automatic destruction.

---

## 7.8 Respawn Pipeline

Implement

ValidateRespawn()

DetermineLocation()

RestoreCharacter()

RegisterSimulation()

RestoreControl()

---

## 7.9 Entity Registry Integration

Register every player.

Support

FindPlayer()

FindBySession()

FindByEntity()

FindByPlayerID()

---

## 7.10 Bubble Manager Integration

Player movement updates

Bubble calculation

Activation notifications

Bubble membership

---

## 7.11 World Integration

Player creation

Player destruction

World ownership

World queries

Environment access

---

## 7.12 Session Integration

Associate

Player

↓

Connection

↓

Session

↓

Entity

All ownership remains explicit.

---

## 7.13 Validation

Detect

Duplicate players

Duplicate sessions

Duplicate entities

Invalid reconnects

Invalid spawn requests

Ownership violations

---

## 7.14 Statistics

Track

Connected Players

Disconnected Players

Respawns

Deaths

Reconnects

Join Time

Average Session Duration

---

## 7.15 Diagnostics

Provide

Player Inspector

Player State Dump

Connection History

Spawn History

Entity Ownership

---

## 7.16 Performance

Measure

Join latency

Spawn latency

Reconnect latency

Disconnect cleanup

Player lookup time

---

## 7.17 Unit Tests

Verify

Join

Leave

Spawn

Death

Respawn

Reconnect

Duplicate Join

Duplicate Entity Prevention

Stress test with many players

---

## 7.18 Integration Tests

Verify

Player appears in World

Player enters Registry

Bubble updates correctly

Session mapping correct

Disconnect preserves persistence

Reconnect restores control

---

## 7.19 Documentation

Document

Lifecycle

Ownership

Registry interaction

Session mapping

Spawn rules

Reconnect behavior

---

# 8. Deliverables

✓ Player Manager

✓ Player Entity

✓ Join pipeline

✓ Spawn pipeline

✓ Disconnect pipeline

✓ Reconnect pipeline

✓ Respawn pipeline

✓ Registry integration

✓ Bubble integration

✓ Diagnostics

✓ Tests

✓ Documentation

---

# 9. Risks

Potential Risks

- Duplicate player entities
- Session desynchronization
- Invalid reconnect ownership
- Spawn conflicts
- Entity leaks

Mitigation

- Unique Player IDs
- Session validation
- Registry assertions
- Deterministic lifecycle
- Ownership enforcement

---

# 10. Validation Strategy

Joining

✓ Player joins successfully

Spawning

✓ Player enters World correctly

Disconnect

✓ Character persists

Reconnect

✓ Previous character restored

Respawn

✓ Player respawns correctly

Stress Test

✓ Hundreds of joins/leaves execute correctly

Performance

✓ Stable under repeated reconnects

---

# 11. Acceptance Criteria

□ Player Manager operational.

□ Join pipeline complete.

□ Spawn pipeline complete.

□ Disconnect pipeline complete.

□ Reconnect pipeline complete.

□ Bubble integration verified.

□ Registry integration verified.

□ Tests passing.

□ Documentation updated.

---

# 12. Definition of Done

Sprint-007 is complete when

- Players exist as persistent World entities.
- Join and leave operations are deterministic.
- Disconnect does not destroy player persistence.
- Reconnect restores previous ownership.
- Bubble Manager recognizes players.
- Entity Registry owns all player entities.
- Tests pass.

---

# 13. Next Sprint

Sprint-008 – Snapshot System

With persistent players integrated into the authoritative simulation, the project is now ready to expose simulation state to asynchronous systems.

Sprint-008 introduces immutable simulation snapshots, allowing Replication, Persistence, Replay, and future worker systems to consume world state without directly accessing live simulation objects.
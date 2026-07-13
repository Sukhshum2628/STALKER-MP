# Sprint-005: Online/Offline Transitions
# Sprint-005: Online/Offline Entity Transition

---

# Sprint Information

| Field | Value |
|--------|-------|
| Sprint | 005 |
| Title | Online/Offline Entity Transition |
| Status | **Implemented & Verified (Closed)** — closed 2026-07-10 |
| Priority | Critical |
| Estimated Duration | 3 Weeks |
| Prerequisites | Sprint-001 through Sprint-004 |
| Next Sprint | Sprint-006 – Multiplayer Core |
| Architecture | Frozen — see `Multiplayer/docs/Sprint-005-Architecture.md` |
| Subsystem doc | `Multiplayer/docs/TransitionLayer.md` |
| Evidence Gates | E-G1 PASSED · E-G2 RESOLVED · E-G3 non-blocking (single-tick reconciliation; no timeout needed) |
| ADR-008 | ACCEPTED & IMPLEMENTED — Engine State Mutation Boundary (Cooperative Flag Override) |
| Production switch | `CALifeUpdateManager::set_switch_online/offline(id, bool)` |
| Tests | 157 passing (Release x64); 0 new warnings; no regressions |
| Implementation | 11 steps, each independently verified by Antigravity |

---

# 1. Sprint Overview

Sprint-005 implements the transition pipeline between offline ALife simulation and online gameplay simulation.

The Bubble Manager determines *which* entities should become active. This sprint implements *how* those entities safely transition between simulation states.

The transition system preserves the original ALife architecture by ensuring entities are activated and deactivated without duplicating simulation or violating ownership boundaries.

Only one simulation of an entity may exist at any time.

---

# 2. Objectives

## Primary Objectives

- Implement transition manager
- Online entity activation
- Offline entity deactivation
- Preserve ALife ownership
- Integrate Bubble Manager
- Synchronize Entity Registry
- Prevent duplicate simulation

## Secondary Objectives

- Transition diagnostics
- Debug visualization
- Performance instrumentation
- State validation

---

# 3. Scope

Included

- Transition Manager
- Activation pipeline
- Deactivation pipeline
- Transition state machine
- ALife integration
- Registry synchronization
- Validation framework

---

# 4. Out of Scope

Not Included

- Networking
- Player replication
- Snapshot generation
- Save system
- Threading modifications
- Prediction
- Interpolation

---

# 5. Architecture References

- 04_World.md
- 05_ALife.md
- 06_Multiplayer.md

---

# 6. RFC References

- RFC-0001 — Host Authoritative Simulation
- RFC-0002 — MultiPlayer Bubble System
- RFC-0003 — Immutable Snapshot Architecture

---

# 7. Technical Tasks

---

## 7.1 Transition Manager

Create

- TransitionManager
- TransitionState
- TransitionQueue

Responsibilities

- Activation scheduling
- Deactivation scheduling
- State validation
- Transition ordering

---

## 7.2 Transition States

Implement lifecycle

Offline

↓

Pending Activation

↓

Activating

↓

Online

↓

Pending Deactivation

↓

Deactivating

↓

Offline

No invalid transitions allowed.

---

## 7.3 Activation Pipeline

Implement

QueueActivation()

ValidateActivation()

CreateOnlineObject()

RegisterSimulation()

NotifySubsystems()

Activation must occur in deterministic order.

---

## 7.4 Deactivation Pipeline

Implement

QueueDeactivation()

SerializeRuntimeState()

RemoveSimulationObject()

ReturnControlToALife()

UpdateRegistry()

---

## 7.5 Bubble Integration

Receive activation requests from

BubbleManager

The Transition Manager does not calculate bubbles.

It consumes Bubble decisions only.

---

## 7.6 ALife Integration

Integrate with

- ALife simulator
- Smart Terrain
- Story Registry
- Scheduler

ALife remains authoritative for behavior.

Transition Manager manages only simulation state.

---

## 7.7 Entity Registry Integration

Update registry states

Offline

Online

Transitioning

Destroyed

Registry remains authoritative.

---

## 7.8 Scheduler Integration

Ensure

Entities activate before scheduler execution.

Entities deactivate after scheduler completion.

Maintain deterministic update order.

---

## 7.9 Validation Framework

Detect

Duplicate online entities

Missing registry entries

Invalid transition order

Activation loops

Orphaned entities

---

## 7.10 Transition Events

Expose

OnActivationStarted

OnActivated

OnDeactivationStarted

OnDeactivated

OnTransitionFailed

---

## 7.11 Debug Visualization

Provide hooks for

Transition queue

Entity state

Activation history

Offline entities

Online entities

Pending transitions

---

## 7.12 Statistics

Track

Entities activated

Entities deactivated

Average transition time

Failed transitions

Queue size

Peak online entities

---

## 7.13 Performance

Measure

Transition latency

Activation cost

Deactivation cost

Queue processing time

Scheduler overhead

---

## 7.14 Unit Tests

Test

Single activation

Single deactivation

Mass activation

Mass deactivation

Repeated transitions

Rapid player movement

Boundary crossings

Stress testing

---

## 7.15 Documentation

Document

Transition lifecycle

ALife interaction

Bubble interaction

Registry synchronization

Validation rules

---

# 8. Deliverables

✓ Transition Manager

✓ Activation pipeline

✓ Deactivation pipeline

✓ State machine

✓ Bubble integration

✓ ALife integration

✓ Registry synchronization

✓ Diagnostics

✓ Tests

✓ Documentation

---

# 9. Risks

Potential Risks

- Duplicate simulation
- Activation oscillation
- Scheduler inconsistencies
- Transition race conditions
- Entity leaks

Mitigation

- Deterministic ordering
- Transition queues
- State validation
- Hysteresis
- Registry assertions

---

# 10. Validation Strategy

Activation

✓ Offline entities become online correctly.

Deactivation

✓ Online entities return to offline simulation correctly.

Registry

✓ Entity states remain synchronized.

ALife

✓ Behavior preserved after transition.

Stress Test

✓ Thousands of transitions execute correctly.

Performance

✓ No significant frame spikes.

---

# 11. Acceptance Criteria

□ Transition Manager operational.

□ Activation pipeline complete.

□ Deactivation pipeline complete.

□ Bubble integration complete.

□ ALife integration verified.

□ Registry synchronized.

□ Tests passing.

□ Documentation updated.

---

# 12. Definition of Done

Sprint-005 is complete when

- Entities transition safely between offline and online simulation.
- ALife ownership remains intact.
- Duplicate simulation is impossible.
- Scheduler ordering remains deterministic.
- Bubble Manager controls activation decisions.
- Registry accurately reflects simulation state.

---

# 13. Next Sprint

Sprint-006 – Multiplayer Core

With entity transitions complete, the project is ready to introduce networking.

Sprint-006 establishes the Multiplayer Core responsible for session management, client connections, server lifecycle, authority enforcement, and the foundation upon which replication and player synchronization will be built.

No gameplay synchronization is implemented during Sprint-006; it focuses exclusively on networking infrastructure and session management.
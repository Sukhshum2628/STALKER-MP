# Sprint-004: Bubble Manager

---

# Sprint Information

| Field | Value |
|--------|-------|
| Sprint | 004 |
| Title | Bubble Manager |
| Status | Implemented & Verified (Closed) |
| Priority | Critical |
| Estimated Duration | 2–3 Weeks |
| Prerequisites | Sprint-001, Sprint-002, Sprint-003 |
| Next Sprint | Sprint-005 – Online/Offline Entity Transition |

---

# 1. Sprint Overview

Sprint-004 introduces the Bubble Manager, responsible for determining which entities participate in active simulation.

The Bubble Manager extends the original X-Ray single-player activation model to support multiple connected players while preserving one authoritative ALife simulation.

Instead of maintaining independent simulation regions for each player, the Bubble Manager computes a unified activation region derived from every connected player's position.

The resulting activation state becomes the authoritative online/offline state consumed by ALife.

The Bubble Manager does not execute gameplay logic.

Its sole responsibility is determining simulation participation.

---

# 2. Objectives

## Primary Objectives

- Implement Bubble Manager
- Track connected player positions
- Calculate unified simulation bubble
- Determine entity activation
- Integrate with Entity Registry
- Integrate with World subsystem
- Preserve deterministic activation

## Secondary Objectives

- Bubble debugging
- Visualization hooks
- Activation statistics
- Performance metrics

---

# 3. Scope

Included

- Bubble Manager
- Bubble calculation
- Bubble merging
- Activation queries
- Bubble configuration
- Bubble diagnostics

---

# 4. Out of Scope

Not included

- ALife transition logic
- Networking
- Replication
- Player synchronization
- Snapshot generation
- Persistence
- Threading changes

---

# 5. Architecture References

- 04_World.md
- 05_ALife.md
- 06_Multiplayer.md

---

# 6. RFC References

- RFC-0001 — Host Authoritative Simulation
- RFC-0002 — MultiPlayer Bubble System

---

# 7. Technical Tasks

---

## 7.1 Bubble Manager

Create

- BubbleManager
- BubbleConfiguration
- BubbleState

Responsibilities

- Bubble computation
- Activation decisions
- Player tracking
- Bubble statistics

---

## 7.2 Player Tracking

Track

- Connected players
- Current position
- World sector
- Last update tick

The Bubble Manager consumes player positions only.

It never owns player objects.

---

## 7.3 Bubble Generation

Implement

GenerateBubble()

RecalculateBubble()

InvalidateBubble()

Bubble generation must be deterministic.

---

## 7.4 Bubble Merge

Implement merging logic.

Support

- Single player
- Two players
- Multiple players
- Overlapping bubbles
- Separated bubbles

Result

One unified simulation region.

---

## 7.5 Bubble Configuration

Support

Simulation Radius

Activation Margin

Deactivation Margin

Debug Flags

Maximum Radius

Future configuration options may be added.

---

## 7.6 Entity Evaluation

Determine

Inside Bubble

Outside Bubble

Pending Activation

Pending Deactivation

Evaluation must not modify entities.

---

## 7.7 Bubble Queries

Provide

IsEntityInside()

IsPositionInside()

NearestBubble()

ActiveBubbleRadius()

ActivePlayerCount()

---

## 7.8 Bubble Events

Expose

OnBubbleUpdated

OnEntityEnteredBubble

OnEntityExitedBubble

OnBubbleMerged

OnBubbleSplit

Events remain informational.

---

## 7.9 Bubble Statistics

Track

Bubble Count

Merged Bubble Count

Average Radius

Largest Radius

Activated Entities

Deactivated Entities

---

## 7.10 Debug Visualization

Provide hooks for

Bubble Radius

Player Radius

Merged Areas

Entity State

Activation Boundaries

Rendering deferred.

---

## 7.11 Performance

Measure

Bubble calculation time

Entity evaluation time

Merge cost

Entities processed

Update frequency

---

## 7.12 Validation Rules

Verify

No duplicate bubbles

No invalid regions

Deterministic evaluation

Correct merge behavior

Stable activation boundaries

---

## 7.13 Unit Tests

Test

Single player bubble

Dual player merge

Multiple players

Players disconnecting

Empty world

Dense entity populations

Stress tests

---

## 7.14 Documentation

Document

Bubble ownership

Merge algorithm

World interaction

Entity evaluation

Configuration

---

# 8. Deliverables

✓ Bubble Manager

✓ Bubble generation

✓ Bubble merge algorithm

✓ Player tracking

✓ Entity evaluation

✓ Configuration

✓ Diagnostics

✓ Unit tests

✓ Documentation

---

# 9. Risks

Potential Risks

- Expensive bubble calculations
- Large player counts
- Frequent updates
- Oscillating activation
- Merge instability

Mitigation

- Spatial acceleration
- Hysteresis
- Cached calculations
- Incremental updates
- Profiling

---

# 10. Validation Strategy

Bubble Generation

✓ Correct radius calculation

Merge

✓ Multiple bubbles merge correctly

Evaluation

✓ Entities classified correctly

Performance

✓ Stable update time

Diagnostics

✓ Visualization matches runtime

Stress Test

✓ Hundreds of entities evaluated correctly

---

# 11. Acceptance Criteria

□ Bubble Manager initializes successfully.

□ Bubble updates every simulation tick.

□ Multiple players merge correctly.

□ Entity evaluation deterministic.

□ Bubble statistics operational.

□ Debug tools functional.

□ Tests passing.

□ Documentation updated.

---

# 12. Definition of Done

Sprint-004 is complete when

- Bubble Manager computes deterministic activation regions.
- Player positions influence one unified simulation bubble.
- No gameplay logic exists inside Bubble Manager.
- All entity evaluation occurs through the Entity Registry.
- Tests pass.
- Ready for Online/Offline transitions.

---

# 13. Next Sprint

Sprint-005 – Online/Offline Entity Transition

The Bubble Manager introduced in this sprint determines which entities should participate in active simulation.

Sprint-005 will implement the transition pipeline that moves entities between offline ALife simulation and fully online gameplay while preserving deterministic execution and avoiding duplicate activation.

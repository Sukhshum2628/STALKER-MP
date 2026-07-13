# STALKER-MP ALife Architecture

**Document Version:** 1.0

**Status:** Draft

**Audience:** Engine Developers, AI Developers, Gameplay Developers

---

# 1. Purpose

This document defines the architecture of the ALife subsystem within STALKER-MP.

ALife is the autonomous simulation responsible for creating the illusion of a living Zone.

Unlike rendering, networking, or gameplay systems, ALife continues to operate independently of direct player interaction.

Its responsibility is not to react to players alone, but to continuously evolve the state of the world regardless of human presence.

The purpose of this document is to define:

- ALife responsibilities
- scheduler architecture
- simulation states
- interaction with the World subsystem
- Smart Terrain integration
- faction simulation
- story integration
- multiplayer integration
- architectural constraints

Implementation details are intentionally omitted.

---

# 2. Philosophy

The defining principle of STALKER-MP is:

> **Preserve ALife.**

Multiplayer exists to transport ALife.

It does not replace it.

The architecture therefore assumes that ALife remains the authoritative simulation engine.

Whenever multiplayer introduces new requirements, the preferred solution is:

```
Extend ALife

↓

Never Replace ALife
```

This philosophy guides every architectural decision throughout the project.

---

# 3. What Is ALife?

ALife is an autonomous simulation framework.

It continuously updates the persistent state of the Zone.

Unlike traditional AI systems that react only to nearby players, ALife simulates activity across the entire world.

Examples include:

- stalker movement
- mutant migration
- artifact generation
- Smart Terrain occupation
- faction influence
- offline combat
- economy changes
- story progression

Players merely become additional participants within this ongoing simulation.

---

# 4. Responsibilities

ALife is responsible for:

- AI scheduling
- online simulation
- offline simulation
- Smart Terrain management
- squad movement
- faction planning
- mutant ecology
- story object behavior
- artifact lifecycle
- simulation progression

ALife is **not** responsible for:

- networking
- rendering
- persistence
- player connections
- packet replication
- UI
- audio

Those responsibilities belong to other subsystems.

---

# 5. Architectural Position

Within STALKER-MP the ALife subsystem occupies the following position.

```
                    World

                      │

                      ▼

               Bubble Manager

                      │

                      ▼

                  ALife

        ┌─────────┼─────────┐

        ▼         ▼         ▼

 Smart Terrain  Scheduler  Registry

        ▼         ▼         ▼

    AI Objects   Squads   Story Objects

                      │

                      ▼

                 Replication
```

Notice that ALife is positioned **below** the World.

The World determines *what should be simulated.*

ALife determines *how it is simulated.*

---

# 6. Core Design Principles

The following principles govern every ALife subsystem.

---

## Autonomous

ALife must continue operating without players.

---

## Deterministic

Identical world state must always produce identical simulation results.

---

## Persistent

Simulation survives saves, reconnects, and server restarts.

---

## Scalable

Simulation must support large numbers of entities through online/offline transitions.

---

## Modular

Individual simulation systems should remain independent whenever practical.

---

## Multiplayer Transparent

ALife should require minimal awareness of multiplayer.

Whenever possible, multiplayer concerns remain outside the AI architecture.

---

# 7. Internal Architecture

The ALife subsystem is composed of several major components.

```
               ALife

                  │

    ┌─────────────┼─────────────┐

    ▼             ▼             ▼

Scheduler    Registry     Smart Terrain

    ▼             ▼             ▼

Offline AI   Online AI   Story Objects

    ▼

Faction Planner

    ▼

Artifact System
```

Each component owns a clearly defined responsibility.

---

# 8. Component Responsibilities

## Scheduler

Coordinates simulation execution.

Determines update order.

Maintains deterministic execution.

---

## Registry

Maintains simulation objects.

Provides lookup services.

Tracks simulation metadata.

---

## Smart Terrain

Assigns NPCs to locations.

Coordinates occupancy.

Controls local behavior.

---

## Offline Simulation

Represents entities beyond active simulation regions.

Maintains low-cost world progression.

---

## Online Simulation

Executes full AI.

Runs physics interaction.

Supports combat.

Supports player interaction.

---

## Story Manager

Coordinates story objects participating within ALife.

Maintains narrative consistency.

---

## Faction Planner

Coordinates high-level strategic behavior.

Controls:

- territory
- hostility
- migration
- reinforcement

---

# 9. ALife Data Flow

Every simulation update follows a predictable pipeline.

```
World

↓

Bubble Evaluation

↓

Entity Activation

↓

Scheduler

↓

Simulation

↓

Gameplay Updates

↓

Registry

↓

Replication

↓

Persistence
```

Notice that replication observes ALife.

ALife never observes replication.

---

# 10. ALife Ownership

Ownership remains explicit.

| Component | Owner |
|------------|------|
| Scheduler | ALife |
| AI | ALife |
| Registry | ALife |
| Smart Terrain | ALife |
| Story Behaviour | ALife |
| World State | World |
| Persistence | World |
| Networking | Multiplayer |
| Replication | Replication |
| Rendering | Client |

Maintaining explicit ownership minimizes architectural ambiguity.

---

# 11. Scheduler Architecture

## 11.1 Purpose

The Scheduler is the central execution system of ALife.

Every autonomous behavior within the Zone ultimately executes through the Scheduler.

Its purpose is to ensure that every simulation update occurs:

- in the correct order,
- at the correct time,
- with deterministic results,
- while remaining scalable as the world grows.

The Scheduler is not responsible for AI itself.

Instead, it determines **when AI is allowed to execute**.

This distinction is fundamental.

---

## 11.2 Design Goals

The Scheduler has several primary objectives.

### Deterministic Execution

Identical world state must always produce identical update order.

This guarantees stable multiplayer synchronization.

---

### Fair Scheduling

No entity should permanently starve because other entities consume excessive update time.

---

### Scalability

The Scheduler must efficiently support:

- hundreds of NPCs
- mutants
- artifacts
- story objects
- Smart Terrains
- future entity types

through selective activation rather than brute-force execution.

---

### Bubble Transparency

The Scheduler should not know *which player* activated an entity.

It should only know:

"This entity is currently online."

---

### Modular Execution

Individual simulation systems remain independent.

For example:

```
Scheduler

│

├── NPC AI

├── Smart Terrain

├── Story

├── Faction Planner

└── Artifact Logic
```

Each system executes independently under Scheduler coordination.

---

# 11.3 Scheduler Responsibilities

The Scheduler is responsible for:

- determining update order
- activating online entities
- executing offline simulation
- coordinating subsystem updates
- maintaining deterministic ordering
- enforcing execution priorities
- distributing simulation time
- notifying dependent systems

The Scheduler is **not** responsible for:

- rendering
- networking
- persistence
- replication
- player input
- physics ownership

---

# 11.4 High-Level Scheduler Pipeline

Each simulation tick follows the same conceptual pipeline.

```
Receive Active World Bubble

↓

Determine Online Entities

↓

Determine Offline Entities

↓

Update Online Simulation

↓

Update Smart Terrain

↓

Update Story

↓

Update Faction Planner

↓

Update Artifact System

↓

Commit Simulation Results

↓

Notify World
```

Every subsystem executes after entity activation has already been determined.

---

# 11.5 Scheduler Phases

Execution is divided into distinct phases.

## Phase 1 — Activation

Receive active entity set from the World.

No AI executes during this phase.

---

## Phase 2 — Online Scheduling

Construct execution queues for entities requiring full simulation.

---

## Phase 3 — Offline Scheduling

Prepare reduced-cost updates for distant entities.

---

## Phase 4 — Autonomous Systems

Execute:

- Smart Terrain
- Story
- Faction Planner
- Ecology
- Artifact Systems

---

## Phase 5 — State Commit

Apply all simulation changes.

No subsystem should directly modify replicated state during execution.

State becomes authoritative only after the commit phase.

---

## Phase 6 — Completion

Return updated simulation state to the World.

The World may then generate replication snapshots.

---

# 11.6 Tick Lifecycle

Every simulation tick follows the same lifecycle.

```
Tick Begins

↓

Receive Bubble

↓

Evaluate Active Set

↓

Schedule Jobs

↓

Execute Simulation

↓

Commit Results

↓

Tick Ends
```

The Scheduler itself remains stateless between ticks except for persistent simulation metadata.

---

# 11.7 Scheduler Ordering

Ordering must remain deterministic.

The following conceptual order is recommended.

```
Entity Activation

↓

NPC AI

↓

Mutants

↓

Smart Terrain

↓

Story

↓

Faction Planner

↓

Artifacts

↓

Cleanup
```

Changing execution order may alter gameplay.

Therefore scheduler ordering is considered an architectural invariant.

---

# 11.8 Scheduling Queues

Rather than updating every entity simultaneously, the Scheduler organizes work into queues.

Example:

```
Online Queue

↓

Offline Queue

↓

Story Queue

↓

Faction Queue

↓

Artifact Queue
```

Queues simplify prioritization and future parallelization.

---

# 11.9 Priority System

Entities may receive different execution priorities.

Examples include:

Highest

- combat
- player interaction
- scripted events

Medium

- nearby patrols
- mutant movement
- squad coordination

Lowest

- distant ecology
- ambient simulation
- idle NPC behavior

Priority influences scheduling frequency.

It never changes ownership.

---

# 11.10 Scheduler and World

The Scheduler never searches the world directly.

Instead:

```
World

↓

Bubble Manager

↓

Active Entity Set

↓

Scheduler
```

The Scheduler consumes prepared information.

This separation prevents duplicated activation logic.

---

# 11.11 Scheduler and Smart Terrain

Smart Terrain executes under Scheduler control.

```
Scheduler

↓

Smart Terrain

↓

NPC Assignment

↓

Updated AI Tasks
```

The Scheduler decides *when* Smart Terrain updates.

Smart Terrain decides *what* assignments change.

---

# 11.12 Scheduler and Story

Story objects participate as simulation jobs.

```
Scheduler

↓

Story Queue

↓

Script Evaluation

↓

Simulation Update
```

Story progression therefore remains synchronized with ALife.

---

# 11.13 Scheduler and Bubble Manager

The Bubble Manager never schedules AI.

Instead:

```
Player Movement

↓

Bubble Manager

↓

World

↓

Scheduler
```

The Scheduler remains completely unaware of player identities.

Only activation information is visible.

---

# 11.14 Scheduler Invariants

The Scheduler guarantees the following.

### Every online entity executes at most once per tick.

---

### Every offline entity executes through the offline pipeline only.

---

### Update ordering remains deterministic.

---

### Bubble evaluation always completes before scheduling begins.

---

### Replication always occurs after scheduling completes.

---

### AI never executes on clients.

---

# 11.15 Performance Considerations

The Scheduler is expected to become one of the most CPU-intensive components of STALKER-MP.

Performance should therefore prioritize:

- stable frame time
- deterministic ordering
- incremental work
- bounded queue sizes
- efficient entity lookup
- predictable memory access

Optimization should never compromise simulation correctness.

---

# 11.16 Future Parallelization

Version 1 assumes primarily single-threaded simulation execution to preserve determinism.

However, the Scheduler architecture intentionally separates execution into independent phases.

Future versions may parallelize:

- Smart Terrain updates
- faction planning
- artifact simulation
- ecology systems

provided deterministic commit ordering is preserved.

The architecture therefore favors **phase parallelism** over unrestricted task parallelism.

---

# 11.17 Design Rationale

The Scheduler is intentionally designed as a coordinator rather than an AI system.

This separation provides several advantages.

- AI modules remain independent.
- Scheduling policy evolves separately from behavior.
- Bubble logic remains outside ALife.
- Multiplayer integration remains transparent.
- Future scalability is simplified.

Most importantly, the Scheduler preserves the original X-Ray philosophy:

**The simulation remains autonomous, deterministic, and authoritative.**

---

# 12. Simulation States

## 12.1 Purpose

Not every entity within the Zone requires the same level of simulation.

Executing full AI, physics, pathfinding, perception, animation, and combat for every entity across every level would be computationally impractical.

The ALife subsystem therefore models every entity using one of several simulation states.

These states determine:

- simulation fidelity
- CPU allocation
- scheduling frequency
- interaction capability
- replication eligibility

The World determines **when** an entity changes state.

ALife determines **how** that state behaves.

---

## 12.2 Design Philosophy

Simulation state represents a **level of participation** within the authoritative world.

It does **not** represent whether an entity exists.

Every entity continues to exist regardless of simulation state.

Changing simulation state affects only how that entity is processed.

This distinction preserves world continuity while allowing the simulation to scale efficiently.

---

## 12.3 State Model

Every persistent entity progresses through a defined state machine.

```
                Created
                    │
                    ▼
              Registered
                    │
                    ▼
              Offline State
                    │
         ┌──────────┴──────────┐
         │                     │
         ▼                     ▼
Remain Offline         Transition Online
         │                     │
         │                     ▼
         │              Online State
         │                     │
         │                     ▼
         └──────────────► Transition Offline
                               │
                               ▼
                        Offline State
                               │
                               ▼
                           Destroyed
```

The state machine is authoritative and deterministic.

---

## 12.4 Simulation State Responsibilities

Each state has clearly defined responsibilities.

### Offline

- strategic simulation
- squad movement
- migration
- economy
- artifact progression
- Smart Terrain occupancy
- faction planning

---

### Online

- AI perception
- pathfinding
- combat
- animation
- physics
- inventory interaction
- player interaction
- anomaly interaction

---

### Transitional

Responsible for preserving continuity while moving between states.

Examples include:

- restoring runtime AI
- rebuilding perception
- synchronizing inventory
- transferring scheduler ownership

---

## 12.5 State Ownership

Simulation state ownership is divided as follows.

| Responsibility | Owner |
|---------------|------|
| State Evaluation | World |
| State Transition | World |
| Runtime Behaviour | ALife |
| Replication | Replication |
| Persistence | World |
| Rendering | Client |

This explicit separation avoids responsibility leakage.

---

## 12.6 State Invariants

Every entity must satisfy the following invariants.

- Exactly one simulation state at any moment.
- Identity remains constant.
- Inventory persists across transitions.
- Story participation is preserved.
- Smart Terrain relationships remain valid.
- Replication reflects the current simulation state.
- Transition completion occurs before the next scheduler tick.

---

## 12.7 State Transition Triggers

State transitions may occur because of:

- Bubble evaluation
- Level transitions
- Story activation
- Scripted events
- Administrative commands
- Future gameplay systems

All transitions are coordinated by the World before ALife executes.

---

# 13. Online Simulation

## 13.1 Purpose

Online simulation represents the highest-fidelity execution mode available within ALife.

Entities in this state participate fully in the living world.

They are capable of interacting with:

- players
- NPCs
- anomalies
- physics
- Smart Terrain
- story systems
- environmental hazards

Online entities consume the majority of simulation resources.

---

## 13.2 Design Goals

Online simulation aims to:

- provide believable AI
- support real-time interaction
- maintain deterministic behavior
- integrate seamlessly with multiplayer
- preserve original X-Ray gameplay

---

## 13.3 Online Simulation Pipeline

```
World

↓

Bubble Evaluation

↓

Entity Activated

↓

Scheduler

↓

AI Update

↓

Gameplay Systems

↓

Physics

↓

Inventory

↓

Commit

↓

Replication
```

Every stage executes on the authoritative host.

---

## 13.4 AI Behaviour

Online entities execute complete AI behaviour.

Examples include:

- perception
- target acquisition
- navigation
- combat
- communication
- squad coordination
- cover selection
- anomaly avoidance

Behaviour remains entirely under ALife control.

---

## 13.5 Interaction Model

Online entities may interact with:

### Players

combat

trading

dialogue

quests

---

### NPCs

communication

combat

squad behaviour

Smart Terrain

---

### Environment

radiation

weather

anomalies

emissions

---

### Story

mission triggers

scripted events

dynamic objectives

---

## 13.6 Scheduler Participation

Every online entity receives scheduler time.

The scheduler determines:

- update ordering
- execution frequency
- priority
- fairness

The entity itself determines only its internal behaviour.

---

## 13.7 Runtime Components

An online entity may own:

- AI controller
- navigation state
- combat memory
- animation controller
- inventory
- perception system
- physics representation

These components exist only while the entity remains online.

---

## 13.8 Online Simulation Guarantees

The architecture guarantees:

- one AI execution
- one physics simulation
- one inventory state
- one authoritative position
- deterministic updates
- immediate interaction capability

These guarantees are essential for multiplayer consistency.

---

# 14. Offline Simulation

## 14.1 Purpose

Offline simulation allows the Zone to remain alive beyond active player observation.

Rather than freezing distant entities, ALife continues to simulate them using reduced computational cost.

Offline simulation is one of the defining innovations of the X-Ray Engine and is preserved entirely within STALKER-MP.

---

## 14.2 Design Philosophy

Offline does **not** mean inactive.

Offline means:

"Simulated using a lower level of detail."

This allows the world to evolve continuously without executing unnecessary high-fidelity AI.

---

## 14.3 Offline Responsibilities

Offline simulation continues to process:

- travel
- Smart Terrain occupation
- squad movement
- faction expansion
- economy
- artifact generation
- mutant ecology
- story progression

Only real-time behaviour is omitted.

---

## 14.4 Offline Pipeline

```
Scheduler

↓

Offline Queue

↓

Strategic AI

↓

World Events

↓

Position Updates

↓

State Commit
```

Offline entities bypass systems such as:

- animation
- perception
- combat physics
- detailed navigation

---

## 14.5 Transition to Online

When the World activates an entity:

```
Offline State

↓

Restore Runtime Components

↓

Rebuild AI Context

↓

Reinitialize Navigation

↓

Scheduler Registration

↓

Online Simulation
```

The goal is to make the transition appear seamless.

---

## 14.6 Transition to Offline

Likewise:

```
Online

↓

Finish Current Update

↓

Store Runtime State

↓

Release Runtime Components

↓

Offline Representation

↓

Strategic Simulation
```

Only unnecessary runtime data is discarded.

Persistent information remains.

---

## 14.7 Offline Representation

Offline entities maintain:

- identity
- inventory
- position
- Smart Terrain
- faction
- reputation
- story participation
- scheduled objectives

They discard only data required for real-time execution.

---

## 14.8 Scheduler Participation

Offline entities remain under scheduler control.

However:

- update frequency decreases
- processing cost decreases
- AI complexity decreases

The scheduler continues to guarantee deterministic ordering.

---

## 14.9 Benefits

Offline simulation provides:

- scalable AI
- believable world evolution
- reduced CPU usage
- persistent world continuity
- compatibility with multiplayer

Most importantly:

The Zone continues to exist independently of player observation.

---

## 14.10 Design Rationale

Preserving offline simulation is one of the most important architectural decisions within STALKER-MP.

Many multiplayer games eliminate distant simulation entirely.

This project deliberately chooses the opposite approach.

Rather than simplifying the world to fit multiplayer, multiplayer is designed to fit the existing ALife architecture.

This decision preserves the defining characteristic of the S.T.A.L.K.E.R. experience:

**The Zone never stops living.**

---

# 15. Smart Terrain Integration

## 15.1 Purpose

Smart Terrain is one of the defining subsystems of the ALife architecture.

Rather than scripting every NPC individually, Smart Terrain provides a high-level organizational framework that assigns NPCs to meaningful locations, activities, and objectives throughout the Zone.

Within STALKER-MP, Smart Terrain remains completely authoritative.

Multiplayer does not replace Smart Terrain.

Instead, players become additional participants within the ecosystem that Smart Terrain manages.

---

## 15.2 Architectural Role

Smart Terrain sits between strategic planning and individual AI.

Conceptually:

```
                    ALife Scheduler
                           │
                           ▼
                    Smart Terrain
                           │
        ┌──────────────────┼──────────────────┐
        ▼                  ▼                  ▼
  NPC Assignment     Squad Formation      Job Selection
        │                  │                  │
        └──────────────────┼──────────────────┘
                           ▼
                    Individual AI
```

The subsystem coordinates *where* NPCs belong rather than *how* they behave moment-to-moment.

---

## 15.3 Responsibilities

Smart Terrain is responsible for:

- assigning NPCs to locations
- maintaining population targets
- selecting jobs
- organizing squads
- controlling occupation
- balancing ecosystem activity
- coordinating idle behavior
- managing territory occupancy

Smart Terrain is **not** responsible for:

- combat AI
- pathfinding
- networking
- replication
- persistence
- player authority

---

## 15.4 Smart Terrain Lifecycle

Every Smart Terrain follows a predictable lifecycle.

```
Created

↓

Registered

↓

Initialized

↓

Population Evaluation

↓

NPC Assignment

↓

Runtime Updates

↓

Population Changes

↓

Shutdown
```

The World owns the existence of the Smart Terrain.

ALife owns its behavior.

---

## 15.5 NPC Assignment

Assignment occurs through the Scheduler.

```
Scheduler

↓

Smart Terrain

↓

Evaluate Population

↓

Select Candidate

↓

Assign Job

↓

NPC Receives Objective
```

Assignments remain deterministic.

Two identical simulations should always produce identical assignments.

---

## 15.6 Occupancy Management

Each Smart Terrain maintains occupancy information.

Conceptually:

```
Smart Terrain

Capacity : 12

Current : 9

Available : 3
```

Occupancy influences:

- migration
- reinforcement
- spawning
- patrol selection
- faction presence

The World observes occupancy but does not modify it.

---

## 15.7 Interaction with Players

Human players do not bypass Smart Terrain.

Instead, they become environmental factors.

Examples include:

- increasing perceived danger
- reducing available jobs
- interrupting patrols
- forcing evacuations
- triggering reinforcement requests

Smart Terrain reacts to player actions in the same way it reacts to significant world events.

---

## 15.8 Design Principles

Smart Terrain must remain:

- deterministic
- autonomous
- multiplayer-transparent
- scheduler-driven
- independent from networking

The subsystem should never need to know whether an Actor is controlled by AI or by a human.

---

# 16. Faction Simulation

## 16.1 Purpose

Faction Simulation models the long-term strategic behavior of organizations operating within the Zone.

Unlike individual AI, faction simulation operates at a strategic level.

It determines:

- territorial control
- reinforcement
- hostility
- alliances
- migration
- expansion
- resource pressure

This simulation continues regardless of player presence.

---

## 16.2 Hierarchical Position

Faction planning exists above individual NPC behavior.

```
Faction Planner

        │

        ▼

Strategic Goals

        │

        ▼

Squads

        │

        ▼

Individual NPCs
```

This hierarchy allows the Zone to evolve organically.

---

## 16.3 Responsibilities

The Faction Planner coordinates:

- territory evaluation
- reinforcement priorities
- patrol generation
- strategic movement
- occupation goals
- conflict escalation
- retreat decisions

Individual combat remains outside its responsibility.

---

## 16.4 Player Participation

Players are members of factions.

Architecturally:

```
Faction

↓

Squad (optional)

↓

Player
```

Human players therefore contribute to:

- territorial influence
- reputation
- strategic pressure
- military strength

without becoming controllers of the faction.

---

## 16.5 Planner Pipeline

```
Evaluate Territory

↓

Evaluate Threats

↓

Evaluate Resources

↓

Generate Strategic Goals

↓

Assign Squads

↓

Notify Scheduler
```

The Scheduler executes the resulting objectives.

---

## 16.6 Strategic Decisions

Examples include:

- defend territory
- capture location
- reinforce outpost
- retreat
- investigate anomaly
- escort squad
- recover artifacts

These are long-term objectives rather than immediate actions.

---

## 16.7 Multiplayer Transparency

Faction planners never communicate with networking.

Instead:

```
Faction Planner

↓

Scheduler

↓

NPC Behavior

↓

World

↓

Replication
```

Multiplayer receives only simulation results.

---

# 17. Story Integration

## 17.1 Purpose

Story progression must coexist with autonomous simulation.

Rather than existing outside ALife, story objects become participants within the simulation.

This preserves immersion while maintaining architectural consistency.

---

## 17.2 Story Objects

Story objects include:

- important NPCs
- quest targets
- scripted events
- unique artifacts
- mission locations
- narrative triggers

They remain persistent World entities.

---

## 17.3 Story Pipeline

```
World Event

↓

Story Condition

↓

Scheduler

↓

Story Evaluation

↓

Simulation Update

↓

Gameplay Consequences
```

The Scheduler ensures story progression remains synchronized with ALife.

---

## 17.4 Story State

Story objects maintain:

- identity
- progression
- objectives
- relationships
- activation conditions

These remain persistent across saves and reconnects.

---

## 17.5 Story and Players

Players influence story through simulation.

Examples include:

- completing objectives
- killing important NPCs
- changing faction reputation
- discovering locations
- delivering artifacts

Story logic reacts to simulation.

Simulation does not react to scripting alone.

---

## 17.6 Story and Smart Terrain

Story NPCs may also belong to Smart Terrains.

Example:

```
Story NPC

↓

Smart Terrain

↓

Patrol

↓

Player Encounter

↓

Quest Begins
```

This allows story content to emerge naturally from world activity.

---

## 17.7 Architectural Principles

Story systems should:

- remain deterministic
- participate in ALife
- avoid bypassing scheduler execution
- preserve persistent state
- remain independent from networking

Networking should never trigger story progression directly.

Only authoritative simulation may do so.

---

# 18. ALife and World Interaction

## 18.1 Purpose

Although ALife and the World operate together throughout every simulation tick, they intentionally remain independent subsystems.

The World represents the persistent structure of the Zone.

ALife represents the autonomous intelligence operating within that structure.

Maintaining this separation is one of the most important architectural decisions in STALKER-MP.

The World determines **where simulation occurs**.

ALife determines **what happens within that simulation**.

---

## 18.2 Responsibility Separation

The following table defines ownership boundaries.

| Responsibility | World | ALife |
|---------------|:----:|:------:|
| Entity existence | ✓ | |
| Entity registration | ✓ | |
| Persistent identity | ✓ | |
| Level ownership | ✓ | |
| Bubble evaluation | ✓ | |
| Online/offline transition | ✓ | |
| AI behaviour | | ✓ |
| Scheduler | | ✓ |
| Smart Terrain | | ✓ |
| Faction planning | | ✓ |
| Story simulation | | ✓ |
| Ecology | | ✓ |
| Artifact lifecycle | | ✓ |

Ownership must remain explicit.

The World never performs AI.

ALife never owns persistence.

---

## 18.3 Simulation Contract

The interaction between the World and ALife follows a strict contract.

```
World

↓

Evaluate Simulation Regions

↓

Determine Active Entities

↓

Provide Active Entity Set

↓

ALife Scheduler

↓

Execute Simulation

↓

Return Updated State

↓

World Registry Updated
```

The contract is intentionally one-directional.

The World never enters ALife internals.

ALife never manipulates World topology.

---

## 18.4 Entity Activation

Entity activation is entirely owned by the World.

Example:

```
Bubble Manager

↓

Entity Enters Active Region

↓

World

↓

Entity Activated

↓

ALife Scheduler
```

ALife assumes that any entity presented by the World is eligible for full simulation.

---

## 18.5 Entity Deactivation

Likewise:

```
Entity Leaves Active Region

↓

World

↓

Deactivate Entity

↓

ALife Stores Runtime State

↓

Offline Representation
```

ALife never decides whether an entity should become offline.

It merely performs the transition.

---

## 18.6 Data Exchange

The World provides:

- active entity identifiers
- level associations
- environmental state
- global time
- transition notifications

ALife provides:

- updated AI state
- Smart Terrain assignments
- faction changes
- artifact updates
- story progression
- runtime entity changes

The exchange occurs once per simulation tick.

---

## 18.7 Design Benefits

Separating responsibilities provides several advantages.

- simpler debugging
- reusable AI
- easier multiplayer integration
- deterministic ownership
- reduced coupling
- future engine extensibility

---

# 19. ALife and Multiplayer Integration

## 19.1 Design Philosophy

ALife should remain largely unaware that multiplayer exists.

This is arguably the most important design principle of STALKER-MP.

Instead of embedding networking into ALife, multiplayer observes the results of simulation.

Conceptually:

```
World

↓

ALife

↓

Replication

↓

Networking

↓

Clients
```

Notice that networking exists entirely outside the simulation.

---

## 19.2 Why This Matters

If ALife begins making networking decisions, several problems emerge:

- AI depends on latency.
- Simulation becomes nondeterministic.
- Dedicated servers become more difficult.
- Testing complexity increases.
- Engine coupling increases.

By isolating networking, ALife continues to behave as though it were running in a single-player environment.

---

## 19.3 Player Participation

Human players participate in ALife as entities.

The Scheduler does not distinguish between:

```
NPC Decision

↓

Scheduler

↓

Simulation
```

and

```
Player Input

↓

World Validation

↓

Scheduler

↓

Simulation
```

The only difference is the source of the decision.

The simulation pipeline remains identical.

---

## 19.4 Replication Timing

Replication begins only after ALife completes its update.

```
Scheduler

↓

Simulation

↓

Commit

↓

World Updated

↓

Replication Snapshot

↓

Networking
```

This guarantees that clients always receive a consistent world state.

---

## 19.5 Multiplayer Invariants

ALife guarantees:

- one authoritative simulation
- deterministic updates
- no client AI
- one scheduler
- one entity state
- one inventory state

Multiplayer must preserve these guarantees.

---

# 20. Bubble Integration

## 20.1 Purpose

The Bubble Manager is the bridge between player activity and ALife.

Importantly, it is **not** part of ALife.

It belongs to the World subsystem.

---

## 20.2 Information Flow

```
Player Movement

↓

Bubble Manager

↓

World

↓

Active Entity Set

↓

ALife Scheduler
```

ALife receives only the final active entity list.

---

## 20.3 Bubble Transparency

The Scheduler should never know:

- which player activated an entity
- how many players are nearby
- why activation occurred

It should know only:

"This entity is active."

This abstraction dramatically simplifies AI.

---

## 20.4 Multiple Players

Suppose three players activate the same region.

```
Player A

Player B

Player C

↓

Bubble Merge

↓

World Bubble

↓

Entity Activated Once

↓

Scheduler
```

No duplicate AI executes.

No duplicate physics executes.

No duplicate inventory exists.

---

## 20.5 Dynamic Updates

The Bubble Manager may notify the World of:

- player movement
- disconnects
- reconnects
- level transitions

The World recalculates activation.

ALife remains unaware of the reason.

---

## 20.6 Architectural Advantages

Separating Bubble evaluation from ALife provides:

- reusable AI
- deterministic scheduling
- simpler testing
- reduced engine coupling
- easier future optimization

---

# 21. Performance Strategy

## 21.1 Philosophy

ALife is expected to consume the majority of server CPU time.

Performance optimizations must therefore focus on:

- preserving deterministic execution
- minimizing unnecessary updates
- reducing scheduler overhead
- maintaining stable frame times

Correctness always takes priority over raw throughput.

---

## 21.2 Expected Bottlenecks

Major computational costs include:

- scheduler execution
- Smart Terrain evaluation
- faction planning
- pathfinding
- online AI
- entity activation
- state transitions

These systems should be profiled continuously.

---

## 21.3 Optimization Strategy

Preferred optimization techniques include:

- incremental scheduling
- spatial partitioning
- cached Smart Terrain data
- reduced offline update frequency
- efficient registry lookup
- bounded queues

Optimizations must never alter simulation results.

---

# 22. Failure Recovery

## 22.1 Philosophy

Simulation integrity is more important than uninterrupted execution.

If a subsystem fails, the objective is to preserve the authoritative world.

---

## 22.2 Scheduler Failure

If scheduling cannot complete:

- current tick aborted
- previous valid state retained
- error logged
- simulation resumes next tick

Partial simulation commits are prohibited.

---

## 22.3 Smart Terrain Failure

If Smart Terrain encounters an internal failure:

- affected assignments suspended
- scheduler continues
- NPCs retain previous objectives
- diagnostic information recorded

---

## 22.4 Story Failure

Story evaluation failures should not halt ALife.

Narrative systems degrade independently from core simulation whenever possible.

---

## 22.5 Invalid AI State

If an entity enters an impossible state:

```
Suspend Entity

↓

Log Failure

↓

Preserve World

↓

Continue Scheduler
```

One defective entity should never compromise the entire Zone.

---

# 23. Future Extensions

The ALife architecture intentionally supports future enhancements.

Potential extensions include:

- multi-threaded scheduler phases
- dynamic ecology systems
- seasonal migration
- advanced economic simulation
- machine-learning-assisted planners
- dedicated server optimization
- plugin AI modules
- dynamic event generation
- persistent server ecosystems

All future additions should preserve the existing contracts between the World, Scheduler, and simulation systems.

---

# 24. Design Rationale

The central objective of STALKER-MP is not to build a multiplayer game.

It is to preserve one of the most sophisticated autonomous simulations ever implemented in a first-person game while allowing multiple human players to inhabit that simulation.

Several architectural decisions support this objective:

- The World owns existence.
- ALife owns behaviour.
- The Scheduler owns execution.
- The Bubble Manager owns activation.
- Multiplayer observes simulation.
- Replication transports authoritative state.
- Clients present the simulation.

By preserving these boundaries, STALKER-MP extends the X-Ray Engine without fundamentally altering its identity.

---

# 25. Summary

The ALife subsystem remains the authoritative simulation engine of the Zone.

It coordinates autonomous behaviour through a deterministic Scheduler while managing Smart Terrain, faction planning, story progression, ecology, and online/offline simulation.

Rather than becoming multiplayer-aware, ALife interacts with the World through clearly defined interfaces.

The World determines **which entities participate**.

ALife determines **how those entities behave**.

Networking and replication remain external observers that synchronize the results of simulation without influencing its execution.

This architecture preserves the defining characteristics of the original X-Ray Engine while enabling multiple human players to coexist within a single persistent, living Zone.

The ALife subsystem therefore forms the behavioural foundation of STALKER-MP and serves as the primary reference for implementing autonomous simulation throughout the project.
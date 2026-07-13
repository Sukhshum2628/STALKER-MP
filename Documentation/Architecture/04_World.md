# STALKER-MP World Architecture

**Document Version:** 1.0

**Status:** Draft

**Audience:** Engine Developers, Gameplay Developers, Networking Developers

---

# 1. Purpose

The World subsystem represents the persistent simulation of the Zone.

It is responsible for coordinating every persistent entity that exists inside the game world while maintaining clear ownership boundaries between players, ALife, networking, persistence, and engine systems.

Unlike rendering or gameplay systems, the World subsystem does not decide *how* entities behave.

Instead, it defines **where entities exist**, **who owns them**, **when they participate in simulation**, and **how they interact with other major subsystems**.

The World acts as the central coordination layer of STALKER-MP.

---

# 2. Scope

The World subsystem is responsible for:

- Persistent world state
- Entity registration
- World ownership
- Simulation regions
- Online/offline transitions
- Level topology
- Environmental state
- Player participation
- World lifecycle
- Entity lookup
- Cross-level coordination

The World subsystem is **not** responsible for:

- AI decision making
- Combat
- Replication
- Networking
- Rendering
- Physics
- Inventory

Those responsibilities remain within their respective subsystems.

---

# 3. Design Philosophy

The World exists independently of every player.

This principle is fundamental.

Traditional multiplayer games often create worlds **for players**.

STALKER-MP deliberately rejects this model.

Instead,

Players exist inside the World.

The World continues to evolve regardless of player activity.

The World owns persistence.

Players merely influence it.

---

# 4. Definition of the World

Architecturally, the World is defined as:

> The persistent authoritative simulation of every entity, level, environmental system, and interaction existing within the Zone.

This includes:

- NPCs
- Players
- Mutants
- Artifacts
- Smart Terrains
- Story Objects
- Dynamic Objects
- Vehicles (future)
- Environmental hazards
- Weather
- Time
- Anomalies

Every persistent object ultimately belongs to the World.

---

# 5. World Architecture

```
                     World

                        │

        ┌───────────────┼───────────────┐

        ▼               ▼               ▼

   Entity Registry   Environment     Levels

        │

        ▼

    ALife System

        │

        ▼

Persistent Objects

        │

        ▼

Replication
```

The World does not replace ALife.

Instead,

ALife becomes one of several systems operating inside the World.

---

# 6. Core Responsibilities

The World subsystem performs six primary responsibilities.

## Responsibility 1

Maintain persistent world state.

---

## Responsibility 2

Coordinate entity ownership.

---

## Responsibility 3

Manage simulation boundaries.

---

## Responsibility 4

Provide a unified view of the Zone.

---

## Responsibility 5

Coordinate players with ALife.

---

## Responsibility 6

Provide authoritative world information to other systems.

---

# 7. World Components

The World subsystem is composed of several major components.

## World Manager

Central coordinator.

Responsible for:

- initialization
- shutdown
- world update
- subsystem coordination

---

## Entity Registry

Stores every persistent object.

Provides:

- lookup
- ownership
- identifiers
- metadata

---

## Level Manager

Maintains every level.

Responsible for:

- streaming
- transitions
- activation

---

## Environment Manager

Maintains:

- weather
- anomalies
- emissions
- time
- ambient simulation

---

## Simulation Manager

Coordinates:

- online entities
- offline entities
- simulation regions

This manager cooperates directly with ALife.

---

# 8. World Ownership

The World owns:

- entity existence
- persistent identifiers
- level assignment
- registration
- environmental state

The World does NOT own:

- AI
- inventory
- combat
- networking
- rendering

Ownership remains intentionally narrow.

---

# 9. Persistent World

One of the defining characteristics of STALKER-MP is that the World persists independently of connected players.

```
Server Starts

↓

World Loads

↓

Simulation Begins

↓

Player Joins

↓

Player Leaves

↓

Simulation Continues

↓

World Saves

↓

Server Stops
```

Notice:

The player lifecycle is contained entirely inside the world lifecycle.

---

# 10. World Lifecycle

The World itself follows a predictable lifecycle.

```
Create

↓

Initialize

↓

Load Persistent State

↓

Initialize Levels

↓

Initialize ALife

↓

Begin Simulation

↓

Accept Players

↓

Runtime

↓

Save

↓

Shutdown
```

Importantly,

Players are not required for the World to exist.

Future dedicated servers naturally extend from this design.

---

# 11. Architectural Principles

Several principles guide every decision within the World subsystem.

---

## The World Owns Existence

Objects cannot exist outside the World.

---

## ALife Owns Behavior

The World determines **that** an entity exists.

ALife determines **how** it behaves.

---

## Players Are World Entities

Players are not special.

Architecturally they are persistent entities.

---

## Networking Observes

Networking never owns the World.

It merely transports authoritative state.

---

## Persistence Belongs to the World

Saving the world includes every persistent entity.

Players are merely one category of persistent entity.

---

## One World

There is exactly one authoritative Zone.

No duplicated simulations.

No per-player worlds.

No client-owned worlds.

---

# 12. Entity Model

## 12.1 Purpose

The World exists to manage entities.

Everything that persists inside the Zone is represented as an entity.

Rather than allowing individual systems to maintain independent object collections, the World provides a unified entity model that defines:

- existence
- ownership
- lifetime
- identity
- simulation participation

Behavior is delegated to other subsystems.

Existence belongs to the World.

---

## 12.2 Definition

An entity is defined as:

> Any persistent object capable of participating in the authoritative simulation.

This includes, but is not limited to:

- Human Players
- NPC Stalkers
- Mutants
- Smart Terrain Controllers
- Story Objects
- Inventory Containers
- Corpses
- Artifacts
- Dynamic Props
- Doors
- Vehicles (Future)
- Interactive World Objects

Temporary rendering effects, particles, decals, and audio emitters are not considered entities because they do not participate in persistent simulation.

---

## 12.3 Entity Composition

Architecturally, every entity is composed of multiple conceptual layers.

```
Persistent Identity
        │
        ▼
Entity Metadata
        │
        ▼
Simulation State
        │
        ▼
Gameplay Components
        │
        ▼
Presentation Components
```

Each layer has a different owner.

| Layer | Owner |
|---------|------|
| Identity | World |
| Metadata | World |
| Simulation | ALife |
| Gameplay | Gameplay Systems |
| Presentation | Client |

This separation minimizes coupling while allowing different systems to evolve independently.

---

## 12.4 Entity Invariants

Every entity must satisfy the following conditions.

### Unique Identity

Every persistent entity has one globally unique identifier.

Identifiers never change during the lifetime of an entity.

---

### Single Ownership

Only one subsystem owns an entity's lifetime.

Other systems may reference the entity but never own it.

---

### Persistent Registration

Every persistent entity must exist inside the World Registry.

Subsystems may not maintain hidden persistent entities.

---

### Deterministic State

The authoritative state of an entity always exists on the host.

Clients maintain only reconstructed copies.

---

## 12.5 Entity Categories

Entities can be categorized by their simulation role.

### Active Entities

Currently participating in online simulation.

Examples:

- nearby NPCs
- connected players
- active anomalies

---

### Passive Entities

Remain persistent but are not currently simulated.

Examples:

- distant stalkers
- offline mutants
- unloaded containers

---

### Static Entities

Rarely change after creation.

Examples:

- story markers
- static props
- map objects

---

### Dynamic Entities

Frequently change.

Examples:

- players
- NPCs
- artifacts
- inventory objects

---

## 12.6 Entity Relationships

Entities do not exist in isolation.

Example relationships include:

```
Player

↓

Inventory

↓

Weapon

↓

Attachment
```

or

```
Smart Terrain

↓

NPC Squad

↓

NPC

↓

Inventory
```

The World tracks these relationships but does not define their gameplay meaning.

---

# 13. Simulation Regions

## 13.1 Purpose

The original X-Ray Engine was designed around a single-player simulation bubble.

Only entities near the local actor were promoted into full online simulation.

Everything else remained in offline simulation.

This approach dramatically reduced CPU usage while preserving the illusion of a living world.

STALKER-MP preserves this philosophy.

The architecture changes **how regions are defined**, not **why they exist**.

---

## 13.2 Why Simulation Regions Exist

Running full AI for every object in the Zone simultaneously is computationally infeasible.

Simulation regions allow the engine to concentrate computational effort where interaction is currently possible.

Benefits include:

- reduced CPU load
- lower memory pressure
- scalable AI
- deterministic updates
- stable frame times

Simulation regions therefore remain a fundamental architectural concept.

---

## 13.3 Original Bubble Model

Original engine:

```
                Player

                  ●

          ┌──────────────┐

          │ Online World │

          └──────────────┘

 Outside Bubble

↓

Offline Simulation
```

One player.

One simulation bubble.

Everything outside becomes offline.

---

## 13.4 Multiplayer Problem

Introducing additional players creates a fundamental conflict.

Example:

```
Player A

●

                    Player B

                    ●
```

Which player defines the simulation bubble?

If Player A owns the bubble:

Player B experiences missing NPCs.

If Player B owns the bubble:

Player A experiences missing NPCs.

Creating separate ALife simulations would fragment the world.

This directly violates the "One World" philosophy.

---

## 13.5 Architectural Goals

The redesigned simulation region must satisfy the following goals.

- Preserve ALife.
- Preserve offline simulation.
- Maintain one authoritative world.
- Support arbitrary player count.
- Avoid duplicated AI.
- Avoid duplicated entities.
- Avoid fragmented simulations.
- Scale naturally.

---

## 13.6 Design Philosophy

The project does **not** replace the simulation bubble.

Instead,

It generalizes it.

Original architecture:

```
Bubble(Player)
```

New architecture:

```
World Bubble

=

Union

(

Player Bubble A

+

Player Bubble B

+

Player Bubble C

+

...
)
```

The important observation is that ALife still sees a single online region.

The method used to construct that region changes.

---

## 13.7 Region Ownership

Simulation regions belong to the World subsystem.

Players merely contribute influence.

ALife consumes the resulting region.

This separation prevents networking from becoming coupled with AI.

---

## 13.8 Region Lifecycle

Each simulation region follows a predictable lifecycle.

```
Player Connects

↓

Bubble Created

↓

Merged into World

↓

Simulation Active

↓

Player Disconnects

↓

Bubble Removed

↓

World Recalculated
```

Notice:

The World owns the merge.

Players never merge regions directly.

---

## 13.9 Region Invariants

Simulation regions must satisfy several invariants.

Every online entity belongs to exactly one authoritative world.

Regions may overlap.

Overlapping regions never duplicate entities.

An entity is simulated only once.

Offline entities remain outside every active region.

These invariants become critical for replication and persistence.

---

# 14. Multi-Player Bubble Architecture

## 14.1 Introduction

The single largest architectural modification introduced by STALKER-MP is the redesign of the simulation bubble.

This redesign enables multiple human players to coexist inside one persistent ALife simulation without fragmenting the world into independent instances.

Importantly, this architecture **does not replace ALife**.

Instead, it changes **how ALife determines which entities should participate in online simulation**.

The simulation bubble therefore becomes a World responsibility rather than a Player responsibility.

---

# 14.2 Historical Background

The original X-Ray Engine assumes that only one human actor exists.

Conceptually:

```
              Local Actor

                  ●

        +-------------------+
        |                   |
        |   Online Bubble   |
        |                   |
        +-------------------+

Outside Bubble

↓

Offline Simulation
```

Every activation decision ultimately depends upon one question:

> "How far is this entity from the player?"

For a single-player game, this model is extremely efficient.

---

# 14.3 Why the Original Model Cannot Be Reused

Adding a second player immediately introduces conflicting activation requirements.

Example:

```
Player A                         Player B

   ●                               ●
```

Suppose an NPC is standing beside Player B.

Player A is several kilometers away.

The original algorithm produces contradictory results.

According to Player A:

```
NPC

↓

Too Far

↓

Offline
```

According to Player B:

```
NPC

↓

Nearby

↓

Online
```

Both cannot be correct simultaneously.

Duplicating the NPC would create two independent simulations.

Rejecting either player would break gameplay.

The architecture therefore requires a fundamentally different activation model.

---

# 14.4 Design Objectives

The redesigned bubble architecture must satisfy the following requirements.

### Preserve ALife

ALife remains the only authoritative simulation.

---

### One World

There must never be multiple independent versions of the Zone.

---

### One Simulation

Every persistent entity is simulated exactly once.

---

### Multiple Observers

Any number of players may observe and interact with the same simulation.

---

### Dynamic Scaling

Simulation automatically adapts as players move through the Zone.

---

### Deterministic Behaviour

Bubble evaluation must always produce identical results for identical world state.

---

# 14.5 Conceptual Shift

The original engine answers:

> Which objects are near the player?

STALKER-MP instead answers:

> Which objects are near **any connected player**?

This is a subtle but profound architectural shift.

The player no longer owns the bubble.

The world owns the bubble.

Players merely contribute influence.

---

# 14.6 Bubble Contributions

Each connected player contributes an independent influence region.

Example:

```
Player A

      ◯

Player B

                     ◯

Player C

                                      ◯
```

These influence regions are **not** simulation bubbles.

They are only inputs into the World evaluation.

This distinction is extremely important.

---

# 14.7 Bubble Manager

The World introduces a dedicated Bubble Manager.

Responsibilities include:

- tracking connected players
- maintaining influence regions
- recalculating active simulation regions
- detecting overlaps
- detecting separations
- notifying ALife when boundaries change

The Bubble Manager never executes AI.

It only defines where AI should execute.

---

# 14.8 Bubble Generation

Every player generates an influence region.

Conceptually:

```
Player Position

↓

Activation Radius

↓

Influence Region
```

Generation depends only on player position.

No gameplay logic is executed during this stage.

---

# 14.9 World Bubble Construction

Once all influence regions have been generated, the World combines them.

```
Player Bubble A

+

Player Bubble B

+

Player Bubble C

↓

Merged Region

↓

World Bubble
```

The World Bubble represents every area requiring online simulation.

ALife consumes this merged result as though it were one continuous activation region.

---

# 14.10 Bubble Overlap

Overlap is expected.

Example:

```
Player A

    ◯◯◯◯◯

          ◯◯◯◯◯

               Player B
```

The overlapping area is evaluated only once.

No duplicate entities exist.

No duplicate AI executes.

No duplicate physics runs.

Overlap increases interaction opportunities without increasing simulation ownership.

---

# 14.11 Bubble Separation

Players frequently move apart.

Example:

```
◯ Player A







                       ◯ Player B
```

The World Bubble may temporarily consist of multiple disconnected regions.

Conceptually:

```
World Bubble

=

Region A

+

Region B
```

Although physically disconnected, both remain part of the same authoritative world.

---

# 14.12 Bubble Lifetime

Bubble lifetime is tied to player participation.

```
Player Connects

↓

Region Created

↓

Region Updated

↓

Merged

↓

Player Disconnects

↓

Region Removed

↓

World Rebuilt
```

Regions are transient.

The World Bubble is continuously recomputed as player positions change.

---

# 14.13 Bubble Updates

Bubble evaluation is not event-driven alone.

It is performed continuously during world updates.

Typical triggers include:

- player movement
- player connection
- player disconnection
- level transition
- teleportation
- administrative movement

Incremental recalculation should be preferred over full reconstruction whenever practical.

---

# 14.14 Bubble Priority

Not every influence region contributes equally.

Priority rules may be introduced for future optimization.

Possible considerations include:

- player activity
- movement velocity
- combat
- scripted events

Version 1 treats all players equally.

Priority affects optimization only.

Never correctness.

---

# 14.15 Bubble Invariants

The Bubble Manager guarantees several architectural invariants.

### One Authoritative Simulation

No entity may exist in multiple simulations.

---

### No Duplicate AI

Every NPC executes exactly one decision loop.

---

### No Duplicate Physics

Every physics object updates exactly once.

---

### No Duplicate Inventory

Persistent objects never fork into multiple copies.

---

### Deterministic Evaluation

Identical world state always produces identical active regions.

---

### Bubble Transparency

ALife should not need to know which player activated an entity.

It only knows that the entity is online.

---

# 14.16 Bubble Ownership

Ownership is intentionally divided.

| Responsibility | Owner |
|----------------|------|
| Player Position | Player |
| Influence Region | Bubble Manager |
| Bubble Merge | World |
| Entity Activation | World |
| Online Simulation | ALife |
| Replication | Multiplayer |
| Rendering | Client |

This separation prevents responsibility leakage between subsystems.

---

# 14.17 Interaction with ALife

The Bubble Manager never schedules AI directly.

Instead:

```
Player Movement

↓

Bubble Manager

↓

World Bubble

↓

ALife Scheduler

↓

Entity Activation

↓

Simulation
```

ALife remains entirely responsible for deciding how activated entities behave.

---

# 14.18 Performance Considerations

The Bubble architecture is designed to scale with player count.

Optimization strategies include:

- incremental region updates
- cached influence regions
- spatial partitioning
- hierarchical region evaluation
- dirty-region recalculation

The architecture intentionally separates correctness from optimization.

Correctness is mandatory.

Optimization is iterative.

---

# 14.19 Failure Scenarios

Potential failure cases include:

### Player Disconnect

Associated influence region is removed.

World Bubble is rebuilt.

Simulation continues.

---

### Teleportation

Influence region relocates.

Affected entities are reevaluated.

---

### Host Migration (Future)

Entire Bubble Manager transfers ownership.

Not supported in Version 1.

---

### Invalid Region

Region discarded.

Previous valid state retained until recomputation succeeds.

---

# 14.20 Design Rationale

The Multi-Player Bubble architecture achieves the project's primary objective:

Support multiple simultaneous human players without replacing ALife.

Rather than introducing multiple simulations, duplicated NPCs, or fragmented worlds, the architecture generalizes the existing X-Ray activation model into a unified, host-authoritative evaluation.

The result is a system in which:

- the Zone remains singular,
- ALife remains authoritative,
- entities remain unique,
- players coexist naturally,
- and the existing engine architecture is preserved wherever practical.

This architectural principle becomes the foundation upon which the ALife, Multiplayer, Replication, and Persistence subsystems are built.

---

# 15. Online and Offline Objects

## 15.1 Overview

The distinction between online and offline objects is one of the defining characteristics of the X-Ray Engine.

STALKER-MP preserves this model because it enables the simulation of a living world while maintaining acceptable computational cost.

The purpose of the World subsystem is **not** to eliminate offline simulation.

Instead, it determines **when entities transition between simulation states** and coordinates those transitions with ALife.

---

## 15.2 Simulation States

Every persistent entity exists in exactly one simulation state.

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
Transition Online      Remain Offline
         │
         ▼
      Online State
         │
         ▼
 Active Simulation
         │
         ▼
Transition Offline
         │
         ▼
Offline Simulation
         │
         ▼
Destroyed
```

The World owns the transitions.

ALife owns the behavior within each state.

---

## 15.3 Offline Objects

Offline entities remain part of the persistent world but are not fully simulated.

Characteristics include:

- Persistent identity
- Registered within the World
- Represented within ALife
- Minimal memory footprint
- Simplified simulation
- Not replicated to clients

Examples include:

- distant stalkers
- mutant packs
- artifact migration
- distant Smart Terrain occupants

Offline does **not** mean inactive.

The simulation continues at a lower level of detail.

---

## 15.4 Online Objects

Online entities participate in full simulation.

Characteristics include:

- Full AI execution
- Physics participation
- Combat
- Animation updates
- Inventory interaction
- Replication eligibility

Online simulation represents the highest-fidelity representation of an entity.

---

## 15.5 Transition Criteria

An entity may transition online when:

- it intersects an active World Bubble,
- a scripted event requires high-fidelity simulation,
- story progression explicitly activates it,
- another gameplay system requests activation.

Likewise, an entity may return offline when:

- it leaves every active simulation region,
- high-fidelity simulation is no longer required,
- the entity becomes dormant.

The World evaluates these conditions continuously.

---

## 15.6 Transition Guarantees

Transitions must satisfy the following invariants:

- Entity identity never changes.
- Persistent state is preserved.
- Inventory remains intact.
- AI state remains consistent.
- Story state is maintained.
- Replication ownership is updated before visibility changes.

Transitions must appear seamless to gameplay systems.

---

# 16. Cross-Level Simulation

## 16.1 Purpose

The Zone is composed of multiple interconnected levels rather than a single continuous map.

From the perspective of the World subsystem, these levels collectively form one persistent world.

The World is therefore responsible for coordinating simulation across level boundaries.

---

## 16.2 World Topology

Conceptually:

```
+-----------+      +-----------+      +-----------+
|  Cordon   | <--> | Garbage   | <--> | Agroprom  |
+-----------+      +-----------+      +-----------+
        │
        ▼
+-----------+
| Darkscape |
+-----------+
```

Each level is an independent spatial environment.

Together, they form a single logical world.

---

## 16.3 Level Ownership

Each level owns:

- static geometry
- navigation meshes
- local environment
- level-specific resources

The World owns:

- entity persistence
- inter-level relationships
- global time
- global weather
- world identity

This separation allows levels to load and unload without affecting persistent world state.

---

## 16.4 Player Distribution

Multiple players may occupy different levels simultaneously.

Example:

```
Player A → Cordon

Player B → Garbage

Player C → Red Forest

Player D → Jupiter
```

The World continues to coordinate one authoritative simulation despite this geographic separation.

---

## 16.5 Cross-Level Awareness

Although entities are associated with individual levels, the World maintains global awareness.

Examples include:

- faction relationships
- reputation
- emissions
- story progression
- world time
- economy

These systems are not reset when a player changes levels.

---

## 16.6 Inter-Level Movement

When an entity transitions between levels:

```
Current Level

↓

Serialize Runtime State

↓

Transfer Ownership

↓

Load Destination

↓

Register Entity

↓

Resume Simulation
```

The transfer is coordinated by the World to ensure continuity.

---

# 17. Environmental Systems

## 17.1 Overview

The Zone is more than a collection of entities.

It also contains persistent environmental systems that influence gameplay.

These systems are coordinated by the World.

Their internal behavior remains the responsibility of specialized subsystems.

---

## 17.2 Environmental Components

The World coordinates:

- global time
- weather
- emissions
- anomalies
- ambient hazards
- radiation zones
- blowouts
- seasonal systems (future)

Each system contributes to the overall state of the Zone.

---

## 17.3 Global Time

Time exists at the World level.

Every subsystem observes the same authoritative clock.

Examples of time-dependent systems include:

- AI schedules
- Smart Terrain assignments
- emissions
- artifact spawning
- trader inventories
- scripted events

Clients never advance world time.

---

## 17.4 Weather

Weather represents a shared environmental state.

Responsibilities include:

- current weather profile
- transitions
- regional synchronization
- atmospheric conditions

Weather is simulated once and replicated as necessary.

---

## 17.5 Emissions

Major environmental events affect the entire Zone.

Example:

```
Emission Begins

↓

World Notification

↓

ALife Response

↓

NPC Behaviour

↓

Player Effects

↓

Replication

↓

Visual Presentation
```

The World coordinates the event.

ALife determines behavioral responses.

Clients present the results.

---

## 17.6 Environmental Ownership

Ownership remains explicit.

| System | Owner |
|---------|------|
| Time | World |
| Weather | World |
| Emissions | World |
| AI Response | ALife |
| Rendering | Client |
| Audio | Client |
| Visual Effects | Client |

This separation ensures that environmental simulation remains deterministic.

---

# 18. World Update Pipeline

## 18.1 Purpose

The World subsystem acts as the coordinator of the persistent simulation.

It does not replace subsystem execution.

Instead, it defines the order in which major systems are allowed to update.

Maintaining a deterministic update pipeline is essential for multiplayer synchronization.

---

## 18.2 High-Level Update Order

Each simulation tick follows the same conceptual sequence.

```
Receive Player Input
        │
        ▼
Update Player Positions
        │
        ▼
Recalculate Influence Regions
        │
        ▼
Construct World Bubble
        │
        ▼
Evaluate Online / Offline Transitions
        │
        ▼
Update ALife
        │
        ▼
Update Environmental Systems
        │
        ▼
Resolve Gameplay
        │
        ▼
Physics
        │
        ▼
Generate Replication Snapshot
        │
        ▼
Send Network Updates
```

This ordering guarantees that replication always reflects a completed simulation state.

---

## 18.3 World Responsibilities During Update

The World performs the following tasks every tick:

- update active player regions
- evaluate simulation boundaries
- process entity transitions
- coordinate level state
- update environmental systems
- expose authoritative world state to ALife and Replication

The World deliberately avoids gameplay-specific processing.

---

## 18.4 Deterministic Ordering

Subsystem execution order must remain stable.

For example:

```
Bubble Evaluation

↓

Entity Activation

↓

ALife Scheduling

↓

Physics

↓

Replication
```

Changing this order could lead to:

- inconsistent AI behavior
- duplicated entities
- invalid replication
- divergent simulations

Deterministic ordering is therefore considered an architectural invariant.

---

## 18.5 World Events

The World also acts as an event source.

Typical events include:

- entity registered
- entity removed
- entity transitioned online
- entity transitioned offline
- level loaded
- level unloaded
- player entered level
- player left level
- environmental state changed

Other subsystems may observe these events but should not redefine their semantics.

---

# 19. World and ALife Interaction

## 19.1 Purpose

The World and ALife are closely related but intentionally separate subsystems.

A common misconception is that ALife *is* the world.

Architecturally, this is incorrect.

The World represents the persistent existence of the Zone.

ALife represents the autonomous simulation operating within that world.

The distinction is fundamental to STALKER-MP.

---

## 19.2 Responsibilities

The World answers:

- What exists?
- Where does it exist?
- Which entities are active?
- Which level owns the entity?
- Which simulation region contains the entity?

ALife answers:

- What should the entity do?
- Where should it move?
- Which Smart Terrain should it occupy?
- Which squad should it join?
- How should factions react?
- How should offline simulation progress?

---

## 19.3 Ownership Matrix

| Responsibility | World | ALife |
|----------------|:----:|:-----:|
| Entity existence | ✓ | |
| Entity registration | ✓ | |
| Persistent identity | ✓ | |
| Level ownership | ✓ | |
| Bubble evaluation | ✓ | |
| Online/offline transition | ✓ | |
| AI | | ✓ |
| Smart Terrain | | ✓ |
| Scheduler | | ✓ |
| Offline simulation | | ✓ |
| Faction logic | | ✓ |
| Story reactions | | ✓ |

This separation is one of the primary architectural principles of STALKER-MP.

---

## 19.4 Interaction Pipeline

Every simulation update follows the same conceptual relationship.

```
World

↓

Determine Active Regions

↓

Determine Active Entities

↓

Notify ALife

↓

ALife Schedules AI

↓

Simulation Executes

↓

Results Returned

↓

World State Updated

↓

Replication
```

The World decides *which* entities participate.

ALife decides *how* those entities behave.

---

## 19.5 Bubble Consumption

The Bubble Manager never executes AI.

Instead:

```
Player Positions

↓

Bubble Manager

↓

Merged World Bubble

↓

ALife Scheduler
```

ALife remains completely unaware of individual player bubbles.

It receives only the final activation region.

This abstraction prevents networking concepts from leaking into AI systems.

---

## 19.6 Entity Activation

Entity activation occurs in two stages.

Stage 1

The World determines whether an entity should become online.

Stage 2

ALife initializes active simulation for that entity.

The transition is cooperative.

Ownership remains distinct.

---

## 19.7 State Synchronization

When ALife modifies an entity, it does not own persistence.

Instead:

```
ALife

↓

Entity Updated

↓

World Registry

↓

Replication

↓

Persistence
```

The World becomes the authoritative source for the updated state.

---

## 19.8 Design Benefits

Separating World and ALife provides several advantages.

- AI remains reusable.
- Bubble logic remains isolated.
- Networking never enters ALife.
- Dedicated servers become easier.
- Future AI improvements remain independent of networking.

---

# 20. World and Player Interaction

## 20.1 Overview

Players do not communicate directly with ALife.

Instead, every player interacts with the World.

The World determines whether an interaction is valid and coordinates the appropriate subsystems.

---

## 20.2 Interaction Flow

```
Player Input

↓

Host Validation

↓

World

↓

Gameplay System

↓

ALife (if required)

↓

World Updated

↓

Replication
```

The World acts as the central coordinator.

---

## 20.3 Player Registration

When a player joins:

```
Connection

↓

Identity

↓

Actor Allocation

↓

World Registration

↓

Bubble Creation

↓

ALife Registration

↓

Gameplay Begins
```

The player does not become part of the simulation until World registration completes successfully.

---

## 20.4 Movement

Movement updates influence the World rather than directly modifying ALife.

```
Player Moves

↓

World Updates Position

↓

Bubble Updated

↓

Affected Entities Evaluated

↓

ALife Adjusted
```

Movement therefore changes the simulation indirectly.

---

## 20.5 World Queries

Players frequently request information.

Examples include:

- nearby entities
- interaction targets
- loot containers
- quest objects
- level transitions

The World validates these requests before forwarding them to the appropriate subsystem.

---

## 20.6 Design Principle

Players influence the World.

They never own it.

This distinction preserves host authority and deterministic simulation.

---

# 21. World Persistence

## 21.1 Purpose

The World exists independently of gameplay sessions.

Persistence ensures that the Zone continues across restarts, reconnects, and future dedicated server deployments.

---

## 21.2 Persistent Components

The World persists:

- entity registry
- player locations
- NPC locations
- inventory ownership
- anomalies
- artifacts
- emissions
- weather
- time
- story progression
- Smart Terrain assignments
- faction state

Collectively, these represent the authoritative state of the Zone.

---

## 21.3 Save Pipeline

```
Simulation

↓

World Registry

↓

Persistence Snapshot

↓

Serializer

↓

Storage
```

Only the World generates persistence snapshots.

Gameplay systems never write directly to storage.

---

## 21.4 Loading

Loading follows the reverse process.

```
Storage

↓

Deserializer

↓

World Registry

↓

Levels

↓

Entities

↓

ALife

↓

Simulation Begins
```

This ordering ensures that every subsystem observes a consistent world state during initialization.

---

## 21.5 Versioning

Future revisions may evolve persistence formats.

The World architecture therefore assumes versioned save data.

Migration should occur during loading rather than during runtime simulation.

---

# 22. Failure Scenarios

## 22.1 Philosophy

Failures should be isolated.

A failure affecting one entity should never invalidate the entire World.

---

## 22.2 Invalid Entity

If an entity becomes invalid:

```
Detect Failure

↓

Suspend Entity

↓

Log Error

↓

Continue Simulation
```

The World should remain operational whenever possible.

---

## 22.3 Bubble Evaluation Failure

If Bubble evaluation fails:

- previous valid bubble retained
- invalid evaluation discarded
- warning logged
- simulation continues

This prevents transient failures from destabilizing the simulation.

---

## 22.4 Level Failure

If a level cannot load:

- affected transition cancelled
- player remains in current level
- World integrity preserved

---

## 22.5 Persistence Failure

If persistence fails:

- current simulation continues
- save marked unsuccessful
- retry scheduled
- players notified (future)

Simulation correctness takes priority over persistence.

---

## 22.6 Host Failure

Because the host owns the World, host failure terminates the authoritative simulation in Version 1.

Future dedicated servers may introduce redundancy or recovery mechanisms.

---

# 23. Future Extensions

The World architecture has been designed with long-term extensibility in mind.

Potential future capabilities include:

- dedicated servers
- dynamic level streaming
- distributed simulation
- persistent online worlds
- server clustering
- plugin-based environmental systems
- AI-controlled disconnected players
- seasonal world events
- dynamic faction campaigns
- administrative world controls

These additions should extend the existing architecture without altering its fundamental principles.

---

# 24. Design Rationale

The World subsystem exists to separate *existence* from *behavior*.

Rather than allowing ALife, networking, persistence, and gameplay systems to independently manage world state, the World provides a single authoritative coordination layer.

This design offers several advantages.

### Clear Ownership

Every persistent entity has one authoritative owner.

---

### Reduced Coupling

ALife remains independent of networking.

Networking remains independent of AI.

Persistence remains independent of gameplay.

---

### Scalability

The Bubble Manager enables additional players without introducing multiple simulations.

---

### Maintainability

Subsystems evolve independently through well-defined interfaces.

---

### Future Growth

Dedicated servers, plugins, and large-scale simulation become architectural extensions rather than redesigns.

---

# 25. Summary

The World subsystem represents the persistent authoritative Zone within STALKER-MP.

It coordinates entities, simulation regions, environmental systems, and level topology while intentionally avoiding gameplay-specific responsibilities.

By separating **existence** from **behavior**, the World enables ALife to remain the autonomous simulation engine while providing a stable foundation for multiplayer participation.

Several architectural principles define the subsystem:

- The World owns persistent existence.
- ALife owns autonomous behavior.
- Players participate as entities within the World.
- Bubble evaluation belongs to the World.
- Networking observes rather than controls the simulation.
- Persistence operates on authoritative World state.

This architecture preserves the strengths of the original X-Ray Engine while enabling multiple human players to inhabit a single persistent Zone without fragmenting the simulation.

The World subsystem therefore serves as the architectural bridge between the Engine, Player, ALife, Multiplayer, Replication, and Persistence systems, and forms the foundation upon which the remainder of STALKER-MP is built.
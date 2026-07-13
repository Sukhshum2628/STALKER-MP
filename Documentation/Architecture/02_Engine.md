# STALKER-MP Engine Architecture

**Document Version:** 1.0

**Status:** Draft

**Audience:** Engine Developers, Networking Developers, Core Contributors

---

# 1. Purpose

This document defines the architectural relationship between STALKER-MP and the X-Ray Engine.

Unlike implementation documentation, this specification focuses on architectural boundaries, subsystem ownership, integration strategy, modification surfaces, and long-term maintainability.

Its purpose is to answer four fundamental questions:

1. What parts of the engine already solve our problem?
2. What parts of the engine require modification?
3. What parts of the engine must remain untouched?
4. How should future contributors integrate new functionality without violating architectural principles?

Every implementation decision should be evaluated against this document before code is written.

---

# 2. Scope

This document covers:

- Overall X-Ray Engine architecture
- Engine subsystem responsibilities
- Integration boundaries
- Reuse strategy
- Modification policy
- Engine ownership model
- Execution flow
- Simulation responsibilities
- Cross-subsystem dependencies
- Architectural constraints

This document does **not** describe multiplayer protocols, replication algorithms, serialization formats, or persistence mechanisms. Those topics are covered in their respective architecture documents.

---

# 3. Architectural Philosophy

The X-Ray Engine is not treated as a legacy codebase to be rewritten.

Instead, it is treated as a mature simulation engine whose existing systems provide significant value.

The primary engineering objective is therefore:

> Extend the engine while preserving its architectural identity.

This philosophy results in three guiding principles.

---

## 3.1 Reuse Before Replacement

Whenever an existing engine system already performs the required responsibility, that system should be reused.

Examples include:

- Object management
- Entity lifecycle
- Inventory
- Physics
- Animation
- AI scheduling
- Save serialization
- Packet serialization
- Event dispatch
- Script integration

Replacement is justified only when:

- The existing implementation cannot support multiplayer requirements.
- Extension would introduce unacceptable complexity.
- The subsystem fundamentally conflicts with host-authoritative simulation.

---

## 3.2 Preserve Existing Responsibilities

Subsystem responsibilities should remain recognizable.

For example:

Physics remains responsible for physics.

ALife remains responsible for simulation.

Renderer remains responsible for rendering.

Networking should never assume responsibilities that belong elsewhere.

---

## 3.3 Isolate Multiplayer

Multiplayer functionality should exist as an additional architectural layer rather than becoming intertwined with unrelated engine systems.

The preferred architecture is:

                Existing Engine
                       │
                       ▼
            Multiplayer Extension Layer
                       │
                       ▼
             Replication & Transport

rather than embedding networking logic throughout the engine.

This reduces coupling, simplifies maintenance, and preserves compatibility with future engine updates.

---

# 4. X-Ray Engine Overview

The X-Ray Engine is composed of multiple cooperating subsystems.

Each subsystem owns a clearly defined responsibility.

```
                 +--------------------+
                 |      Renderer      |
                 +---------+----------+
                           |
                           |
+-------------+    +--------+--------+    +-------------+
|    Audio    |    |   Game Objects  |    |    Input    |
+-------------+    +--------+--------+    +-------------+
                           |
                           |
                 +---------+----------+
                 |      ALife         |
                 +---------+----------+
                           |
                 +---------+----------+
                 |      Physics       |
                 +---------+----------+
                           |
                 +---------+----------+
                 |   Serialization    |
                 +---------+----------+
                           |
                 +---------+----------+
                 |      File I/O      |
                 +--------------------+
```

STALKER-MP does not replace this architecture.

Instead, it introduces additional systems that integrate with these existing responsibilities.

---

# 5. Engine Responsibility Map

The following table defines which systems remain engine-owned and which become multiplayer-aware.

| Engine System | Owner | Multiplayer Changes |
|--------------|-------|---------------------|
| Renderer | Engine | None |
| Audio | Engine | None |
| Physics | Engine | Minimal |
| Animation | Engine | Minimal |
| Inventory | Engine | Replication only |
| Weapon System | Engine | Replication only |
| ALife | Engine | Bubble redesign |
| Smart Terrain | Engine | Awareness of players |
| Story | Engine | Shared state |
| Save System | Engine | Extended |
| Object Registry | Engine | Extended |
| Networking | STALKER-MP | New |
| Replication | STALKER-MP | New |
| Session Manager | STALKER-MP | New |

The table illustrates an important architectural principle:

Existing gameplay systems remain responsible for gameplay.

STALKER-MP adds synchronization rather than replacing gameplay logic.

---

# 6. Engine Layering

The engine is conceptually divided into layers.

```
+------------------------------------------+
| Presentation Layer                        |
| Rendering, HUD, Audio                     |
+------------------------------------------+

+------------------------------------------+
| Gameplay Layer                            |
| Actor, Inventory, Weapons, Scripts        |
+------------------------------------------+

+------------------------------------------+
| Simulation Layer                          |
| ALife, AI, Smart Terrain, Story           |
+------------------------------------------+

+------------------------------------------+
| Core Engine Layer                         |
| Object Registry, Physics, Serialization   |
+------------------------------------------+

+------------------------------------------+
| Platform Layer                            |
| File System, Threads, Memory, OS          |
+------------------------------------------+
```

Multiplayer integrates primarily with the Gameplay and Simulation layers while relying on the Core Engine for serialization and object management.

---

# 7. Modification Boundaries

One of the most important architectural decisions is defining where modifications are permitted.

The engine is divided into three categories:

### Stable Systems

Should not be modified except for bug fixes.

Examples:

- Rendering pipeline
- Audio engine
- Particle systems
- Shader compilation
- UI framework

---

### Extendable Systems

Support extension through additional interfaces or wrapper layers.

Examples:

- Serialization
- Object registry
- Event system
- Inventory
- Save system
- Script interfaces

---

### Core Multiplayer Modification Surface

These systems are expected to receive significant architectural changes.

- ALife update logic
- Online/offline transition logic
- Simulation bubble
- Player ownership
- Network session management
- Replication layer

Restricting modifications to these areas minimizes regression risk and keeps the engine maintainable.

---

# 8. Engine Ownership

Every subsystem has exactly one owner.

Ownership determines which subsystem is allowed to modify authoritative state.

| Subsystem | Authoritative Owner |
|-----------|---------------------|
| Rendering | Client |
| Audio | Client |
| Input | Client |
| Physics | Host |
| ALife | Host |
| Story | Host |
| Inventory | Host |
| Save Games | Host |
| Replication | Host |
| Prediction | Client |
| Interpolation | Client |

Ownership is never shared.

A subsystem may expose data for observation, but only its owner may modify persistent state.

---

# 9. Integration Strategy

Rather than rewriting existing engine modules, STALKER-MP integrates through well-defined extension points.

Preferred integration techniques, in order of preference:

1. Existing engine interfaces
2. Wrapper classes
3. Observer/event hooks
4. Additional manager classes
5. Localized engine modifications

Direct modification of unrelated engine code should be considered a last resort.

This hierarchy minimizes maintenance cost and eases future upgrades.

---

# 10. Design Constraints

The engine architecture imposes several non-negotiable constraints on all future development.

- Preserve deterministic simulation on the host.
- Do not duplicate engine systems already present.
- Avoid introducing global ownership ambiguity.
- Keep multiplayer code isolated from rendering and presentation.
- Prefer composition over invasive modification.
- Ensure that all persistent state flows through the authoritative simulation.
- Maintain compatibility with existing save/load mechanisms whenever practical.

These constraints serve as architectural guardrails for every subsequent subsystem.

---

# 11. Engine Execution Model

## 11.1 Overview

The X-Ray Engine operates as a deterministic simulation loop that continuously updates the state of the game world. Every subsystem participates in this loop according to a well-defined execution order.

STALKER-MP does not introduce an alternative execution model. Instead, it extends the existing loop by inserting multiplayer-specific responsibilities at carefully controlled synchronization points.

The fundamental execution principle remains:

> **The simulation is always authoritative. Networking exists only to observe and distribute the results of simulation.**

This ordering is one of the most important architectural invariants of the project.

---

## 11.2 High-Level Frame Lifecycle

Every frame executed by the host follows the same conceptual lifecycle.

```
Input Collection
        │
        ▼
Player Command Processing
        │
        ▼
World Update
        │
        ▼
ALife Simulation
        │
        ▼
Gameplay Logic
        │
        ▼
Physics
        │
        ▼
Replication Snapshot
        │
        ▼
Packet Generation
        │
        ▼
Rendering
```

The ordering above is intentional.

Simulation must complete before replication begins.

Replication must complete before packets are transmitted.

Rendering must never modify simulation state.

---

## 11.3 Host Frame Responsibilities

The host executes every gameplay system.

The host is responsible for:

- processing player inputs
- validating requests
- executing AI
- updating ALife
- advancing Smart Terrain logic
- resolving combat
- processing inventory changes
- advancing quests
- executing story logic
- updating physics
- generating replication state
- saving persistent changes

The host is therefore the single source of truth for every persistent object.

---

## 11.4 Client Frame Responsibilities

Clients execute a significantly smaller subset of the frame.

```
Receive Packets
        │
        ▼
Apply Replicated State
        │
        ▼
Prediction
        │
        ▼
Interpolation
        │
        ▼
Animation
        │
        ▼
Rendering
```

Clients never execute authoritative ALife.

Clients never advance faction simulation.

Clients never resolve persistent gameplay decisions.

Instead, clients reconstruct the authoritative world for presentation.

---

# 12. Game Loop Integration

## 12.1 Existing Game Loop

The existing engine already contains a mature update pipeline.

Conceptually:

```
Frame

↓

Engine Update

↓

Game Update

↓

Object Updates

↓

Physics

↓

Renderer
```

Rather than replacing this pipeline, STALKER-MP inserts additional responsibilities into predefined integration points.

---

## 12.2 Multiplayer Integration Points

```
Frame

↓

Receive Client Commands

↓

Validate Commands

↓

Execute Existing Engine Update

↓

ALife

↓

Gameplay

↓

Physics

↓

Replication Snapshot

↓

Packet Broadcast

↓

Render
```

Notice that multiplayer processing surrounds the simulation rather than replacing it.

Incoming commands are processed before simulation.

Outgoing packets are produced after simulation.

---

## 12.3 Why This Ordering Exists

Changing execution order would introduce synchronization problems.

Examples include:

- physics running on outdated inputs
- AI reacting to stale player positions
- inventory duplication
- inconsistent combat results
- divergent simulations

Keeping simulation centralized guarantees deterministic behavior.

---

# 13. Object Lifecycle

## 13.1 Overview

Every object in the Zone follows a lifecycle.

STALKER-MP does not invent a new lifecycle.

Instead, multiplayer awareness is added to the existing one.

```
Created

↓

Registered

↓

Offline

↓

Online

↓

Replicated

↓

Dormant

↓

Destroyed
```

---

## 13.2 Creation

Objects originate from multiple systems.

Examples include:

- level loading
- ALife spawning
- Smart Terrain
- story scripts
- artifact generation
- mutant migration

Creation remains entirely host-controlled.

Clients never create persistent entities.

---

## 13.3 Registration

Every persistent object must enter the global object registry.

Registration assigns:

- unique identifier
- simulation ownership
- persistence metadata
- replication eligibility

The registry becomes the authoritative catalogue of world entities.

---

## 13.4 Online Transition

Objects become online when required by simulation.

Originally:

```
Distance to Player

↓

Online
```

STALKER-MP changes this to:

```
Distance to Any Connected Player

↓

Online
```

The transition remains controlled by ALife.

Only the activation criteria change.

---

## 13.5 Replication

Once online, objects may become replication candidates.

Not every online object must be replicated.

Replication policy depends on:

- visibility
- relevance
- ownership
- bandwidth budget
- simulation priority

Replication is therefore a secondary concern layered on top of simulation.

---

## 13.6 Destruction

Objects may leave the simulation through:

- death
- cleanup
- scripted removal
- anomaly destruction
- quest completion

Destruction always originates from the authoritative simulation.

Clients merely receive destruction notifications.

---

# 14. Entity Registration

## 14.1 Purpose

Entity registration provides a consistent mechanism for identifying every persistent object in the Zone.

Every major subsystem depends on this registry.

Including:

- ALife
- Story
- Replication
- Save system
- Inventory
- Smart Terrain

---

## 14.2 Registration Responsibilities

The registry stores metadata rather than gameplay logic.

Typical metadata includes:

- unique ID
- class type
- simulation status
- ownership
- persistence state
- network state
- level association

Gameplay systems remain responsible for behavior.

---

## 14.3 Registry Invariants

The following conditions must always remain true.

Every entity has exactly one identifier.

Identifiers are never reused during a session.

Every persistent entity is registered.

Destroyed entities are removed only after all dependent systems acknowledge the deletion.

Violating these invariants risks save corruption and replication errors.

---

# 15. Engine Event Flow

## 15.1 Event-Oriented Communication

Subsystems communicate through events rather than direct dependencies whenever practical.

Example:

```
Inventory

↓

Inventory Changed Event

↓

Replication

↓

Packet Builder

↓

Clients
```

The inventory system never calls networking directly.

Instead, networking observes state changes.

This minimizes coupling.

---

## 15.2 Advantages

Using event-driven communication:

- reduces dependencies
- improves modularity
- simplifies debugging
- enables future plugins
- supports replay systems
- supports dedicated servers

---

## 15.3 Event Ownership

Every event has:

- one producer
- zero or more consumers

Only the producing subsystem may define event semantics.

Consumers may observe but never reinterpret the event.

This prevents architectural ambiguity.


---

# 16. Module Dependency Graph

## 16.1 Purpose

The X-Ray Engine is composed of numerous interconnected systems. Without clear dependency rules, changes to one subsystem can unintentionally affect unrelated functionality, making maintenance difficult and increasing the risk of regression.

STALKER-MP adopts a layered dependency model that explicitly defines which modules may depend on others. Every subsystem must conform to this model to preserve architectural integrity.

Dependencies represent **knowledge**, not ownership.

A subsystem may depend on another subsystem's public interface without assuming responsibility for its internal state.

---

## 16.2 Architectural Dependency Layers

The engine is organized into conceptual layers.

```
+---------------------------------------------------+
|                Presentation Layer                 |
| Renderer | HUD | Audio | Camera | UI             |
+---------------------------------------------------+
                       │
                       ▼
+---------------------------------------------------+
|                 Gameplay Layer                    |
| Actor | Inventory | Weapons | Scripts | Quests   |
+---------------------------------------------------+
                       │
                       ▼
+---------------------------------------------------+
|                World Management                   |
| World Manager | Entity Registry | Level Manager  |
+---------------------------------------------------+
                       │
                       ▼
+---------------------------------------------------+
|               Simulation Layer                    |
| ALife | AI | Smart Terrain | Story | Scheduler   |
+---------------------------------------------------+
                       │
                       ▼
+---------------------------------------------------+
|                  Core Engine                      |
| Physics | Serialization | Memory | File System   |
+---------------------------------------------------+
                       │
                       ▼
+---------------------------------------------------+
|                  Platform Layer                   |
| OS | Networking API | Threading | Storage        |
+---------------------------------------------------+
```

Higher layers may depend on lower layers.

Lower layers must never depend on higher layers.

---

## 16.3 Multiplayer Placement

Multiplayer is not considered part of the simulation.

Instead, it forms a parallel service layer.

```
                    Multiplayer Services

+---------------------------------------------------+
| Session Manager                                   |
| Replication                                       |
| Packet Builder                                    |
| Connection Manager                                |
| Prediction                                        |
| Interpolation                                     |
+---------------------------------------------------+
                       │
                       ▼
                Simulation Interfaces
                       │
                       ▼
                   Existing Engine
```

This separation allows multiplayer to observe simulation without becoming responsible for simulation.

---

## 16.4 Dependency Rules

The following rules apply throughout the project.

### Rule 1

Rendering must never depend on networking.

---

### Rule 2

Networking must never modify simulation directly.

---

### Rule 3

ALife must not know whether an actor is human or AI unless explicitly required.

---

### Rule 4

Gameplay systems should communicate through public interfaces rather than internal data structures.

---

### Rule 5

Subsystems should depend upon abstractions whenever possible.

---

## 16.5 Dependency Direction

Correct dependency flow:

```
Player

↓

World

↓

ALife

↓

Physics

↓

Serialization
```

Incorrect dependency flow:

```
Physics

↓

Inventory

↓

Renderer

↓

Networking

↓

ALife
```

The incorrect example violates ownership boundaries and introduces circular dependencies.

---

# 17. Simulation Boundaries

## 17.1 Purpose

Simulation boundaries define where authoritative game logic begins and ends.

One of the largest architectural mistakes in multiplayer projects is allowing simulation logic to leak into presentation or networking systems.

STALKER-MP avoids this by enforcing strict simulation boundaries.

---

## 17.2 What Constitutes Simulation

Simulation includes every system responsible for changing the persistent state of the Zone.

Examples include:

- NPC decision making
- combat
- inventory changes
- anomaly behavior
- weather progression
- faction influence
- artifact generation
- Smart Terrain assignments
- offline simulation
- story progression
- player interactions

Only the host executes these systems.

---

## 17.3 Non-Simulation Systems

The following systems do not alter persistent world state.

- Rendering
- Audio
- HUD
- Particle effects
- Camera
- Animation blending
- Prediction
- Interpolation
- UI

These systems reconstruct or visualize the results of simulation.

---

## 17.4 Simulation Boundary Diagram

```
               Persistent World

          +----------------------+
          |      Simulation      |
          +----------------------+

                 ▲        ▲

                 │        │

          Gameplay     ALife

                 │

          Physics Resolution

                 │

--------------------------------------------
 Authoritative Boundary
--------------------------------------------

                 │

      Replication Snapshot

                 │

      Client Reconstruction

                 │

      Rendering & Audio
```

Everything above the boundary modifies the world.

Everything below the boundary represents the world.

---

## 17.5 Online and Offline Simulation

Objects may exist in one of several simulation states.

```
Created

↓

Offline Simulation

↓

Online Simulation

↓

Replication

↓

Dormant

↓

Destroyed
```

Simulation state is independent from replication state.

An object may be actively simulated without being replicated.

Likewise, an object may be replicated without being directly controlled by a player.

---

## 17.6 Human Players

Human players participate inside the simulation rather than outside it.

Architecturally:

```
NPC

↓

ALife Entity

↓

Simulation
```

Human Player:

```
Player Input

↓

Actor Entity

↓

Simulation
```

The distinction lies only in the origin of decision making.

NPC decisions originate from AI.

Player decisions originate from network input.

Both ultimately become simulation events processed by the host.

---

## 17.7 Why These Boundaries Matter

Maintaining strict simulation boundaries provides several benefits.

- deterministic execution
- simplified debugging
- replay capability
- authoritative saves
- consistent AI behavior
- future dedicated server support

Violating these boundaries risks simulation divergence and state corruption.

---

# 18. Memory Ownership

## 18.1 Purpose

Memory ownership defines which subsystem is responsible for allocating, maintaining, and destroying shared engine objects.

Without explicit ownership rules, multiplayer systems risk introducing dangling references, duplicated objects, and inconsistent world state.

---

## 18.2 Ownership Principles

Every persistent object has exactly one logical owner.

Ownership determines responsibility for:

- allocation
- initialization
- updates
- destruction
- persistence

Other systems may reference an object but do not own its lifetime.

---

## 18.3 Ownership Categories

### Engine-Owned

Examples:

- physics objects
- render resources
- animation controllers
- collision geometry

Managed entirely by the engine.

---

### World-Owned

Examples:

- entity registry
- persistent actors
- level objects
- simulation metadata

Managed by the World Manager.

---

### ALife-Owned

Examples:

- smart terrain assignments
- AI scheduling
- offline simulation records
- faction planners

Managed exclusively by ALife.

---

### Multiplayer-Owned

Examples:

- connection state
- network sessions
- packet buffers
- replication queues
- latency statistics

These objects never become part of the persistent world.

---

### Client-Owned

Examples:

- interpolation buffers
- prediction history
- HUD state
- local effects

Destroyed immediately when a client disconnects.

---

## 18.4 Object Lifetime

Persistent entities follow a predictable lifetime.

```
Allocate

↓

Initialize

↓

Register

↓

Activate

↓

Simulate

↓

Replicate

↓

Deactivate

↓

Unregister

↓

Destroy
```

Each transition is controlled by exactly one subsystem.

---

## 18.5 Shared References

Subsystems frequently reference the same entity.

Example:

```
Entity

│

├── ALife

├── Physics

├── Inventory

├── Renderer

├── Replication
```

Although multiple systems reference the entity, only one subsystem owns its lifetime.

This distinction is critical.

References do not imply ownership.

---

## 18.6 Lifetime Guarantees

The following guarantees apply.

An entity cannot be destroyed while another subsystem still holds an active reference.

Destruction occurs only after:

- replication completes
- save queues finish
- event dispatch concludes
- AI references are released
- physics callbacks complete

Deferred destruction prevents invalid memory access during frame execution.

---

## 18.7 Resource Management Strategy

The architecture favors deterministic resource management.

Guiding principles include:

- explicit ownership
- predictable lifetimes
- minimal shared mutable state
- deferred destruction where necessary
- clear allocation responsibility

These principles improve stability and simplify debugging in long-running multiplayer sessions.

---

# 19. Error Recovery

## 19.1 Purpose

A persistent multiplayer simulation is expected to operate continuously for extended periods of time. Unlike a traditional single-player session, temporary failures cannot always be resolved by restarting the game or reloading a save.

The engine architecture must therefore assume that failures will occur and define predictable recovery strategies that preserve simulation integrity whenever possible.

The primary objective is not simply to recover from errors, but to recover **without violating the authoritative state of the simulation**.

---

## 19.2 Architectural Philosophy

Error recovery follows four principles.

1. Detect failures as early as possible.

2. Isolate failures to the responsible subsystem.

3. Prevent invalid state from propagating.

4. Preserve simulation consistency above all else.

Simulation correctness always takes precedence over availability.

---

## 19.3 Failure Categories

Engine failures generally fall into one of several categories.

### Simulation Errors

Examples include:

- invalid entity state
- impossible AI transitions
- corrupted inventories
- invalid Smart Terrain assignments
- failed quest progression

These are considered critical because they affect persistent world state.

---

### Network Errors

Examples include:

- packet loss
- latency spikes
- client timeout
- disconnection
- malformed packets

Network failures should never directly corrupt simulation.

Clients may disconnect, but the Zone continues to exist.

---

### Resource Failures

Examples include:

- allocation failure
- file access errors
- save serialization failure
- missing assets

These failures should be isolated from gameplay whenever practical.

---

### Logic Failures

Examples include:

- assertion failures
- invalid state transitions
- scripting exceptions
- synchronization errors

These indicate defects in implementation and should be treated as development-time issues.

---

## 19.4 Failure Isolation

Subsystems should fail independently whenever possible.

Example:

```
Packet Parser Failure

↓

Connection Closed

↓

Player Disconnected

↓

Simulation Continues
```

The failure of a single client must never terminate the authoritative simulation.

---

## 19.5 Graceful Degradation

When recovery is possible, systems should degrade functionality rather than terminate execution.

Examples:

- temporarily suspend replication to a lagging client
- skip a malformed packet
- defer persistence until storage becomes available
- continue simulation despite client disconnects

Graceful degradation improves long-running server stability.

---

## 19.6 Assertions

Assertions exist to detect programming errors rather than recover from runtime failures.

Assertions should verify:

- ownership invariants
- state transitions
- identifier validity
- thread assumptions
- registry consistency

Assertions must never replace runtime validation of external input.

---

## 19.7 Logging Strategy

All critical failures should generate structured log entries.

Logs should include:

- subsystem
- timestamp
- entity identifier
- player identifier (if applicable)
- error category
- severity
- contextual information

Logs form the primary source of post-mortem debugging.

---

# 20. Performance Strategy

## 20.1 Design Objectives

Performance optimization must never compromise correctness.

The optimization priorities are:

1. Deterministic simulation
2. Stable frame time
3. Maintainability
4. Memory efficiency
5. CPU utilization
6. Bandwidth optimization

Correctness always precedes optimization.

---

## 20.2 Performance Philosophy

Optimization should be driven by measurement rather than assumption.

Premature optimization introduces unnecessary complexity and often targets the wrong bottlenecks.

Every optimization should answer three questions:

- What is slow?
- Why is it slow?
- How will this change improve it?

---

## 20.3 Expected Bottlenecks

Based on the architecture, the primary performance risks are expected to be:

- ALife scheduling
- online/offline transitions
- replication snapshot generation
- packet serialization
- physics synchronization
- entity relevance calculations
- persistence serialization

These systems should receive profiling attention before secondary optimizations.

---

## 20.4 CPU Strategy

The simulation thread remains the highest priority.

CPU time should be allocated approximately as follows.

```
Simulation
██████████████████████

Replication
██████

Networking
████

Persistence
██

Rendering
(Client only)
```

Simulation must never wait for rendering or network transmission.

---

## 20.5 Memory Strategy

Persistent simulations can remain active for many hours.

The engine should therefore favor:

- stable allocations
- predictable lifetimes
- object reuse where practical
- limited heap fragmentation
- bounded cache sizes

Long-term stability is more important than short-term allocation speed.

---

## 20.6 Bandwidth Strategy

Bandwidth is considered a limited resource.

Replication should prioritize:

1. Relevant entities
2. State changes
3. Nearby activity
4. High-priority gameplay events

Low-priority information may be delayed without affecting gameplay.

Bandwidth optimization belongs to the Replication subsystem and should not influence simulation logic.

---

## 20.7 Scalability

The architecture should scale by reducing unnecessary work rather than increasing complexity.

Examples include:

- relevance filtering
- interest management
- replication prioritization
- asynchronous persistence
- efficient registry lookups

Scalability should emerge from architecture rather than special-case optimizations.

---

# 21. Debugging and Instrumentation

## 21.1 Purpose

Large engine projects require extensive diagnostic capabilities.

Debugging tools should be designed as first-class architectural features rather than temporary development utilities.

---

## 21.2 Debug Categories

Instrumentation should exist for:

- ALife
- networking
- replication
- persistence
- player lifecycle
- Smart Terrain
- physics
- entity ownership
- threading

Each subsystem should expose meaningful diagnostic information.

---

## 21.3 Engine Metrics

Examples of useful runtime metrics include:

Simulation

- frame time
- active entities
- online entities
- offline entities
- scheduler duration

Networking

- packets sent
- packets received
- bandwidth
- packet loss
- latency

Replication

- replicated entities
- skipped entities
- snapshot size
- priority distribution

Persistence

- save duration
- pending writes
- serialization failures

These metrics enable continuous performance monitoring.

---

## 21.4 Debug Visualization

Whenever practical, internal engine state should be visualized.

Examples include:

- simulation bubbles
- replication regions
- Smart Terrain ownership
- AI paths
- entity states
- ownership transitions

Visualization greatly reduces debugging complexity.

---

## 21.5 Traceability

Major gameplay events should be traceable.

Example lifecycle:

```
Player Fires Weapon

↓

Weapon System

↓

Physics

↓

Damage

↓

Inventory

↓

ALife

↓

Replication

↓

Client
```

Being able to reconstruct event chains simplifies defect investigation.

---

## 21.6 Deterministic Reproduction

Whenever possible, defects should be reproducible using recorded inputs.

Future tooling may include:

- simulation replay
- packet replay
- deterministic execution
- event recording

These capabilities are particularly valuable for multiplayer debugging.

---

# 22. Future Engine Evolution

## 22.1 Long-Term Vision

The current architecture targets host-authoritative multiplayer.

However, it is intentionally designed to support future expansion without fundamental redesign.

---

## 22.2 Planned Evolution

Potential future developments include:

- dedicated servers
- headless simulation
- plugin framework
- Lua multiplayer API
- administrative tooling
- server-side scripting
- replay system
- telemetry services

These additions should extend the architecture rather than replace it.

---

## 22.3 Architectural Stability

Core architectural principles should remain stable across future versions.

These include:

- host authority
- ALife ownership
- deterministic simulation
- explicit ownership
- layered architecture
- subsystem isolation

Future features must preserve these principles.

---

## 22.4 Backward Compatibility

Where practical, architectural evolution should minimize disruption to:

- save formats
- scripting interfaces
- engine modules
- contributor workflows

Breaking changes should require formal RFC approval.

---

# 23. Design Rationale

Every major architectural decision within STALKER-MP is guided by a small number of consistent engineering principles.

Rather than introducing entirely new systems, the project builds upon the strengths of the X-Ray Engine.

This approach offers several advantages.

### Lower Maintenance Cost

Reusing mature engine systems reduces implementation effort and long-term maintenance.

---

### Greater Stability

Existing gameplay systems have already been tested through years of use.

Preserving them reduces regression risk.

---

### Clear Ownership

Explicit ownership simplifies debugging, persistence, networking, and future development.

---

### Deterministic Simulation

Keeping simulation centralized on the host prevents divergent world states.

---

### Modular Development

Subsystem isolation allows contributors to work independently without introducing unnecessary coupling.

---

### Future Expansion

A layered architecture enables future features such as dedicated servers and plugin APIs without requiring extensive redesign.

---

# 24. Summary

The Engine Architecture establishes the structural foundation upon which every other STALKER-MP subsystem is built.

Rather than replacing the X-Ray Engine, STALKER-MP extends it through carefully defined architectural boundaries.

The engine remains responsible for simulation, gameplay, rendering, and persistence, while multiplayer introduces complementary services that observe, synchronize, and transport authoritative state.

Several architectural principles guide all future development:

- Preserve before replace.
- Maintain deterministic host-authoritative simulation.
- Keep subsystem responsibilities clearly defined.
- Prefer extension over invasive modification.
- Separate simulation from networking.
- Enforce explicit ownership.
- Design for maintainability and future evolution.

Every subsequent architecture document assumes the constraints and responsibilities established here.

Changes that violate these principles should require formal architectural review and RFC approval before implementation.

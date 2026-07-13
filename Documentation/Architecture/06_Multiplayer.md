# STALKER-MP Multiplayer Architecture

**Document Version:** 1.0

**Status:** Draft

**Audience:** Engine Developers, Networking Developers, Infrastructure Developers

---

# 1. Purpose

This document defines the multiplayer architecture of STALKER-MP.

Unlike traditional multiplayer systems, the multiplayer subsystem is not responsible for gameplay simulation.

Its responsibility is to coordinate multiple participants within an already existing authoritative simulation.

The Multiplayer subsystem therefore manages:

- connections
- sessions
- authority
- ownership
- player participation
- synchronization coordination
- host/client relationships
- network topology

Simulation remains the responsibility of the World and ALife.

---

# 2. Philosophy

The Multiplayer subsystem exists to transport participation rather than gameplay.

Players do not own the world.

Players participate in the world.

The host owns:

- simulation
- AI
- physics
- persistence
- inventory
- quests
- environmental state

Clients own:

- input
- camera
- rendering
- prediction
- interpolation
- presentation

Networking communicates between these ownership boundaries.

---

# 3. Design Goals

The Multiplayer subsystem has several primary objectives.

### Preserve Host Authority

All persistent gameplay decisions originate from the host.

---

### Preserve ALife

Networking must never become responsible for simulation.

---

### Deterministic Participation

Every client should observe the same authoritative world.

---

### Session Persistence

Players may join and leave without disrupting simulation.

---

### Future Compatibility

The architecture should naturally extend to dedicated servers.

---

### Explicit Ownership

Every networked object has one authoritative owner.

---

# 4. Architectural Position

Within STALKER-MP, Multiplayer occupies the following position.

```
                World
                  │
                  ▼
                ALife
                  │
                  ▼
            Replication
                  │
                  ▼
            Multiplayer
        ┌─────────┼─────────┐
        ▼         ▼         ▼
 Connection  Session   Transport
        │         │         │
        └─────────┼─────────┘
                  ▼
               Clients
```

Notice that Multiplayer is positioned **below** the simulation.

This ordering is intentional.

---

# 5. Responsibilities

The Multiplayer subsystem is responsible for:

- accepting connections
- validating sessions
- managing player participation
- maintaining authority relationships
- routing packets
- coordinating synchronization
- monitoring network health
- managing disconnections

The subsystem is **not** responsible for:

- AI
- gameplay
- inventory logic
- combat
- persistence
- story
- Smart Terrain
- faction simulation

---

# 6. Internal Architecture

The subsystem consists of several cooperating components.

```
              Multiplayer

                     │

     ┌───────────────┼───────────────┐

     ▼               ▼               ▼

Connection      Session       Authority

     ▼               ▼               ▼

Transport      Player Map     Ownership

                     ▼

              Replication Bridge
```

Each component has one clearly defined responsibility.

---

# 7. Connection Manager

The Connection Manager owns all active network connections.

Responsibilities include:

- accepting clients
- disconnect detection
- connection cleanup
- timeout handling
- protocol negotiation
- transport initialization

Connections are temporary.

They contain no persistent gameplay state.

---

# 8. Session Manager

The Session Manager represents participation in the current multiplayer session.

A session is created after successful authentication and destroyed when participation ends.

Sessions contain:

- connection reference
- player identity reference
- synchronization progress
- latency metrics
- permission level
- current status

Sessions do not contain persistent character data.

---

# 9. Authority Manager

Authority determines which subsystem may modify a particular piece of state.

Within STALKER-MP, authority is explicit and never shared.

Examples:

| Component | Authority |
|-----------|-----------|
| AI | Host |
| Physics | Host |
| Inventory | Host |
| Story | Host |
| Quests | Host |
| Player Input | Client |
| Camera | Client |
| Rendering | Client |
| Prediction | Client |

The Authority Manager coordinates these ownership relationships.

---

# 10. Multiplayer Data Flow

Every interaction follows the same high-level pipeline.

```
Player Input

↓

Network Packet

↓

Host Validation

↓

World

↓

ALife

↓

Simulation

↓

Replication

↓

Multiplayer

↓

Clients
```

Multiplayer never bypasses the simulation.

It transports only authoritative results.

---

# 11. Core Principles

The following principles govern every Multiplayer component.

---

## Connections Are Temporary

Connections may disappear at any moment.

Gameplay must survive their loss.

---

## Sessions Are Replaceable

A session represents current participation.

It is not the character.

---

## Authority Is Explicit

Every networked object has one authoritative owner.

---

## Simulation Is External

The Multiplayer subsystem never performs gameplay simulation.

---

## Networking Observes

Simulation changes first.

Networking communicates those changes afterwards.

---

## One Authoritative World

Every connected player participates in the same persistent simulation.

No client maintains an independent world.

---

# 12. Connection Lifecycle

## 12.1 Purpose

A network connection represents a temporary communication channel between a client and the host.

It does **not** represent gameplay participation.

It does **not** represent a character.

It does **not** represent persistent identity.

Connections exist solely to transport information.

Separating connections from gameplay greatly simplifies recovery from latency, disconnects, and future transport changes.

---

## 12.2 Lifecycle

Every connection progresses through the same lifecycle.

```
Incoming Connection

↓

Transport Established

↓

Protocol Negotiation

↓

Version Validation

↓

Authentication

↓

Session Creation

↓

Synchronization

↓

Gameplay

↓

Disconnect

↓

Cleanup
```

Every stage must complete successfully before advancing.

---

## 12.3 Connection States

Connections exist in one of several states.

```
Pending

↓

Negotiating

↓

Authenticated

↓

Synchronizing

↓

Active

↓

Closing

↓

Closed
```

Only **Active** connections may participate in gameplay.

---

## 12.4 Validation

Before gameplay begins, the host validates:

- protocol version
- supported feature set
- character ownership
- session uniqueness
- connection limits
- save compatibility

Validation failures terminate the connection before any gameplay state is allocated.

---

## 12.5 Cleanup

Connection cleanup performs:

- transport shutdown
- packet queue disposal
- latency history removal
- timeout cancellation
- session notification

Cleanup never destroys persistent character data.

---

# 13. Session Lifecycle

## 13.1 Purpose

A session represents a player's active participation in the current multiplayer world.

Unlike a connection, a session is gameplay-aware.

Unlike an identity, it is temporary.

The separation between Connection, Session, and Identity is one of the key architectural principles of STALKER-MP.

---

## 13.2 Session Model

```
Human

↓

Connection

↓

Session

↓

Identity

↓

Actor

↓

World
```

Each layer has different responsibilities.

---

## 13.3 Session States

```
Created

↓

Synchronizing

↓

Participating

↓

Suspended

↓

Closed
```

Suspended sessions may occur during:

- temporary disconnects
- level transitions
- synchronization recovery

---

## 13.4 Session Responsibilities

The Session Manager maintains:

- player reference
- connection reference
- synchronization progress
- permissions
- latency statistics
- current level
- activity status

Gameplay state remains elsewhere.

---

## 13.5 Session Recovery

If the underlying transport is temporarily interrupted:

```
Connection Lost

↓

Session Suspended

↓

Reconnect

↓

Session Restored

↓

Gameplay Continues
```

The World remains unaffected.

---

# 14. Host Authority Model

## 14.1 Purpose

Host authority is the fundamental architectural principle of STALKER-MP.

Every persistent gameplay decision originates from the host.

Clients never modify authoritative world state directly.

---

## 14.2 Authority Hierarchy

```
Host

↓

World

↓

ALife

↓

Gameplay

↓

Replication

↓

Clients
```

Authority flows downward.

Requests flow upward.

---

## 14.3 Host Responsibilities

The host owns:

- World
- ALife
- Scheduler
- Physics
- Inventory
- Story
- Quests
- Smart Terrain
- Faction Planner
- Environmental systems
- Save games
- Entity registry
- Online/offline transitions

These responsibilities never migrate to clients.

---

## 14.4 Client Responsibilities

Clients own:

- local input
- camera
- rendering
- audio
- prediction
- interpolation
- user interface

These systems exist solely for presentation.

---

## 14.5 Request Flow

Clients request actions.

The host decides whether those actions become reality.

```
Input

↓

Request

↓

Host Validation

↓

Simulation

↓

Replication

↓

Client
```

The host may reject any request.

---

## 14.6 Why Host Authority

Host authority provides several advantages.

- deterministic simulation
- consistent persistence
- simplified AI
- easier debugging
- reduced cheating
- single authoritative save
- stable multiplayer behavior

The tradeoff is increased reliance on the host machine, which is acceptable for Version 1.

---

## 14.7 Authority Invariants

The following conditions must always hold.

- One authoritative World.
- One authoritative Scheduler.
- One authoritative Inventory.
- One authoritative Story.
- One authoritative Physics simulation.
- One authoritative Entity Registry.

These invariants simplify the entire architecture.

---

# 15. Ownership Model

## 15.1 Purpose

Authority determines *who makes decisions*.

Ownership determines *who manages the lifetime and state of an object*.

Although related, these concepts are distinct.

---

## 15.2 Ownership Categories

### World-Owned

Examples:

- entity registry
- world state
- levels
- environmental systems

---

### ALife-Owned

Examples:

- scheduler
- Smart Terrain
- faction planner
- ecology

---

### Multiplayer-Owned

Examples:

- sessions
- transport
- packet queues
- latency information

---

### Client-Owned

Examples:

- HUD
- camera
- prediction buffers
- interpolation history

---

## 15.3 Ownership Rules

Every persistent object has exactly one owner.

Subsystems may reference objects.

They may not assume ownership without explicit transfer.

---

## 15.4 Ownership Guarantees

Ownership changes must be:

- deterministic
- validated
- synchronized
- observable

Implicit ownership changes are prohibited.

---

# 16. Client Responsibilities

## 16.1 Philosophy

Clients are responsible for presenting the authoritative world to the player.

They should perform enough local processing to provide responsive gameplay while avoiding any modification of persistent state.

---

## 16.2 Core Responsibilities

Clients perform:

- input collection
- rendering
- audio playback
- prediction
- interpolation
- local animation
- UI updates
- visual effects

Clients do **not** perform:

- AI scheduling
- world persistence
- faction planning
- story progression
- inventory authority
- online/offline transitions

---

## 16.3 Client Pipeline

```
Receive Snapshot

↓

Apply State

↓

Prediction

↓

Interpolation

↓

Animation

↓

Rendering

↓

Player
```

This pipeline is entirely presentation-focused.

---

## 16.4 Local Prediction

Prediction exists solely to improve responsiveness.

Predicted state is always provisional.

The host remains authoritative.

If divergence occurs, the client reconciles to the host's state.

---

## 16.5 Rendering Independence

Rendering systems must never modify gameplay state.

Visual representation follows simulation.

Simulation never follows visuals.

---

# 17. Host Responsibilities

## 17.1 Overview

The host is the authoritative simulation node.

It owns the complete state of the Zone.

Every client observes the host's world.

---

## 17.2 Core Responsibilities

The host performs:

- world updates
- ALife scheduling
- gameplay logic
- inventory management
- quest progression
- environmental simulation
- replication snapshot generation
- persistence
- connection management

The host therefore becomes the single source of truth.

---

## 17.3 Host Update Pipeline

```
Receive Requests

↓

Validate

↓

World Update

↓

ALife

↓

Gameplay

↓

Physics

↓

Replication Snapshot

↓

Broadcast
```

This ordering guarantees that every client observes a completed simulation state.

---

## 17.4 Host Invariants

The host guarantees:

- one authoritative simulation
- deterministic updates
- explicit ownership
- consistent persistence
- stable scheduler ordering

These guarantees underpin every other multiplayer subsystem.

---

# 18. Join-in-Progress

## 18.1 Purpose

Unlike many multiplayer games, STALKER-MP assumes that the authoritative simulation is already running before a player connects.

The world is never restarted or recreated for a joining player.

Instead, the joining client synchronizes to the current state of the persistent Zone.

Join-in-progress is therefore a synchronization problem rather than a simulation problem.

---

## 18.2 Architectural Goals

Join-in-progress must satisfy the following goals.

- Preserve the running simulation.
- Avoid pausing the world.
- Minimize synchronization time.
- Maintain deterministic state.
- Prevent partially initialized gameplay.

The world continues to evolve throughout the synchronization process.

---

## 18.3 Join Pipeline

```
Connection Established

↓

Authentication

↓

Session Created

↓

Identity Loaded

↓

Actor Allocated

↓

World Snapshot Generated

↓

Snapshot Applied

↓

Replication Begins

↓

Gameplay Enabled
```

The player does not participate in gameplay until synchronization completes successfully.

---

## 18.4 Synchronization Phases

Synchronization occurs in ordered phases.

### Phase 1

World metadata

- world version
- global time
- weather
- current level

---

### Phase 2

Player state

- identity
- inventory
- health
- equipment
- quest state

---

### Phase 3

Relevant entities

- nearby NPCs
- nearby players
- dynamic objects
- anomalies

---

### Phase 4

Background state

- distant ALife
- environmental information
- lower-priority entities

This ordering allows the player to begin interacting with the world as early as possible while less critical information continues to synchronize.

---

## 18.5 Simulation Continuity

The host never pauses ALife during synchronization.

Other players continue to:

- move
- fight
- trade
- complete quests
- trigger emissions

The joining player enters an already living world.

---

# 19. Disconnect and Recovery

## 19.1 Philosophy

Connections are expected to fail.

The architecture therefore treats disconnects as routine operational events rather than exceptional failures.

The objective is to preserve the integrity of the authoritative world while allowing players to recover whenever possible.

---

## 19.2 Disconnect Pipeline

```
Connection Lost

↓

Session Suspended

↓

Input Disabled

↓

Actor Preserved

↓

Simulation Continues

↓

Reconnect

↓

Session Restored

↓

Gameplay Continues
```

The precise gameplay consequences of a disconnected actor (for example, remaining in the world, becoming AI-controlled, or despawning after a timeout) are gameplay policy decisions and are intentionally left outside the multiplayer architecture.

---

## 19.3 Temporary Recovery

If the player reconnects within the allowed recovery window:

- transport is re-established
- session is reattached
- authoritative state is resynchronized
- gameplay resumes

Character identity remains unchanged.

---

## 19.4 Permanent Disconnect

When recovery is no longer possible:

- session is destroyed
- transport resources are released
- multiplayer metadata is removed

Persistent character data remains under the control of the World and Persistence subsystems.

---

## 19.5 Recovery Principles

Recovery must always preserve:

- authoritative world state
- player identity
- deterministic simulation
- ownership consistency

Recovery must never introduce duplicate actors.

---

# 20. Network Topology

## 20.1 Version 1 Topology

Version 1 adopts a host-authoritative client/server architecture.

```
                  Host
                    │
      ┌─────────────┼─────────────┐
      ▼             ▼             ▼
   Client A      Client B      Client C
```

Although one participant also plays the game, architecturally the host functions as the server.

Clients communicate only with the host.

---

## 20.2 Communication Rules

The following communication paths are permitted.

```
Client

↓

Host

↓

Client
```

The following are prohibited.

```
Client

↓

Client
```

All gameplay communication passes through the authoritative host.

---

## 20.3 Benefits

This topology provides:

- one authoritative simulation
- simplified ownership
- deterministic updates
- straightforward persistence
- centralized validation

These benefits outweigh the additional network traffic for Version 1.

---

## 20.4 Scalability

The architecture intentionally separates multiplayer logic from transport implementation.

This allows future transports or deployment models to replace the underlying communication layer without affecting gameplay systems.

---

# 21. Failure Handling

## 21.1 Philosophy

Failures should be isolated whenever possible.

A networking failure should not become a simulation failure.

Similarly, a client failure should not compromise the World.

---

## 21.2 Connection Failure

If a client disconnects unexpectedly:

```
Connection Closed

↓

Session Suspended

↓

Simulation Continues

↓

Cleanup
```

Other players remain unaffected.

---

## 21.3 Packet Failure

Malformed or invalid packets should be rejected before they reach gameplay systems.

```
Receive Packet

↓

Validate

↓

Reject Invalid Data

↓

Continue Processing
```

Invalid network input must never reach ALife or the World.

---

## 21.4 Host Failure

Version 1 assumes the host is the authoritative simulation owner.

If the host terminates unexpectedly:

- simulation ends
- sessions terminate
- clients disconnect

Future dedicated servers remove this limitation.

---

## 21.5 Resource Exhaustion

If networking resources become constrained:

Priority should be given to:

1. gameplay-critical traffic
2. state synchronization
3. lower-priority updates

Graceful degradation is preferred over simulation instability.

---

# 22. Future Dedicated Server Architecture

## 22.1 Motivation

The Version 1 architecture intentionally mirrors a dedicated server design.

The primary difference is that the authoritative simulation currently shares resources with the host player's client.

By separating responsibilities cleanly today, migration to a dedicated server requires minimal architectural change.

---

## 22.2 Future Topology

```
             Dedicated Server
                    │
     ┌──────────────┼──────────────┐
     ▼              ▼              ▼
 Client A       Client B       Client C
```

The World, ALife, Persistence, and Replication remain on the server.

Clients remain presentation nodes.

---

## 22.3 Architectural Stability

The following components should remain unchanged:

- World
- ALife
- Scheduler
- Bubble Manager
- Entity Registry
- Replication interfaces

Only deployment changes.

Simulation architecture does not.

---

## 22.4 Long-Term Extensions

Potential future enhancements include:

- dedicated server binaries
- server administration
- remote console
- authentication services
- matchmaking
- persistent public worlds
- cluster management

These extensions should build upon the existing authority model rather than replacing it.

---

# 23. Design Rationale

The Multiplayer subsystem deliberately limits its responsibilities.

Instead of becoming responsible for gameplay, it focuses exclusively on enabling participation in an existing authoritative simulation.

Several architectural decisions support this objective.

### Separation of Concerns

Simulation, networking, replication, and persistence remain independent.

---

### Explicit Authority

Every persistent decision originates from one authoritative source.

---

### Temporary Participation

Connections and sessions are transient.

Characters and the World persist.

---

### Simulation First

The multiplayer architecture adapts to the simulation rather than forcing the simulation to adapt to networking constraints.

---

### Future Growth

The architecture naturally extends toward dedicated servers without requiring significant redesign.

---

# 24. Summary

The Multiplayer subsystem enables multiple human participants to inhabit a single persistent Zone while intentionally remaining separate from gameplay simulation.

It manages connections, sessions, authority, and participation but delegates all gameplay decisions to the World and ALife subsystems.

The architecture is built upon several core principles:

- Host-authoritative simulation.
- Explicit ownership.
- Temporary network participation.
- Persistent world continuity.
- Deterministic synchronization.
- Separation of simulation from networking.

By treating multiplayer as a distributed systems layer rather than a gameplay system, STALKER-MP preserves the original X-Ray Engine architecture while enabling cooperative participation within the same autonomous ALife simulation.

This subsystem establishes the contracts required by the Replication architecture, which is responsible for efficiently synchronizing the authoritative world state to every connected client.
# STALKER-MP Player Architecture

**Document Version:** 1.0

**Status:** Draft

**Audience:** Engine Developers, Gameplay Developers, Networking Developers

---

# 1. Purpose

This document defines the architectural design of human players within STALKER-MP.

Unlike traditional multiplayer games, players are not considered the center of the game world.

Instead, players are modeled as participants inside an already existing autonomous simulation.

This document defines:

- Player ownership
- Player lifecycle
- Player identity
- Player persistence
- Authority
- Simulation integration
- ALife interaction
- Network responsibilities
- World interaction

Implementation details are intentionally excluded.

---

# 2. Design Philosophy

The central design principle of STALKER-MP is:

> Players are stalkers living inside the Zone.

The world does not exist for the player.

The player exists inside the world.

This distinction affects every architectural decision throughout the project.

Traditional multiplayer architecture typically follows:

```
Players

↓

Game World
```

STALKER-MP deliberately reverses this relationship.

```
Zone

↓

ALife

↓

World

↓

Player
```

The simulation remains authoritative regardless of player activity.

---

# 3. Architectural Goals

The Player subsystem is designed to achieve the following goals.

## Goal 1

Represent human players as first-class entities within the ALife simulation.

---

## Goal 2

Maintain persistent player identity across sessions.

---

## Goal 3

Separate player ownership from player presentation.

---

## Goal 4

Allow players to seamlessly join and leave an active simulation.

---

## Goal 5

Preserve deterministic simulation.

---

## Goal 6

Integrate with existing X-Ray actor systems wherever practical.

---

## Goal 7

Remain compatible with future dedicated servers.

---

# 4. What Is a Player?

Architecturally, a player is composed of multiple conceptual layers.

```
Human

↓

Network Connection

↓

Player Session

↓

Player Identity

↓

Actor Entity

↓

ALife Object

↓

World Entity
```

Each layer has a different responsibility.

The Player subsystem coordinates these layers but does not own them all.

---

# 5. Player Components

A player consists of several cooperating components.

## Human

The physical user.

Outside the simulation.

---

## Connection

Represents an active network transport.

Temporary.

Destroyed on disconnect.

---

## Session

Tracks the player's participation in the current multiplayer session.

Contains:

- connection status
- latency
- permissions
- synchronization state

---

## Identity

Persistent.

Never changes.

Contains:

- unique player ID
- character profile
- faction
- progression
- statistics

Identity survives disconnects.

---

## Actor

The in-world representation.

Owns:

- inventory
- equipment
- health
- stamina
- position
- animation state

The Actor participates directly in simulation.

---

## ALife Object

The player is registered inside ALife exactly like other persistent entities.

ALife determines:

- online state
- simulation status
- persistence

---

# 6. Player Architecture

```
+--------------------------+
| Human                    |
+-------------+------------+
              │
              ▼
+--------------------------+
| Network Connection       |
+-------------+------------+
              │
              ▼
+--------------------------+
| Player Session           |
+-------------+------------+
              │
              ▼
+--------------------------+
| Player Identity          |
+-------------+------------+
              │
              ▼
+--------------------------+
| Actor Entity             |
+-------------+------------+
              │
              ▼
+--------------------------+
| ALife Object             |
+-------------+------------+
              │
              ▼
+--------------------------+
| World                    |
+--------------------------+
```

The important observation is that the player does not directly interact with the world.

The Actor does.

---

# 7. Ownership Model

Ownership determines who may modify each aspect of a player.

| Component | Owner |
|-----------|-------|
| Input | Client |
| Camera | Client |
| Audio | Client |
| HUD | Client |
| Prediction | Client |
| Connection | Multiplayer |
| Session | Multiplayer |
| Identity | Persistence |
| Actor State | Host |
| Inventory | Host |
| Health | Host |
| Position | Host |
| Quests | Host |
| Reputation | Host |
| ALife Registration | Host |

Ownership is explicit.

No component has multiple authoritative owners.

---

# 8. Player Identity

Identity is independent of any individual connection.

Identity represents the persistent character rather than the current network participant.

Identity includes:

- unique identifier
- character name
- faction
- rank
- reputation
- progression
- discovered locations
- completed quests
- statistics

Identity does not include transient state such as latency or camera position.

---

# 9. Player Registration

Players must be registered with the World Manager before they can participate in the simulation.

Registration performs the following tasks.

- validate identity
- load persistent data
- allocate Actor
- register ALife object
- assign ownership
- initialize inventory
- synchronize world state

Only after successful registration may the player enter the Zone.

---

# 10. Architectural Principles

Several principles guide all Player subsystem decisions.

### Human != Player

The human user exists outside the engine.

The Player subsystem only represents their participation.

---

### Connection != Identity

Connections are temporary.

Identity is persistent.

---

### Identity != Actor

Identity survives death.

The Actor may not.

---

### Actor != Camera

The camera merely observes the Actor.

Camera systems never own gameplay state.

---

### Actor != Authority

The host owns Actor state.

Clients merely request actions.

---

### Every Player Is an Entity

Architecturally, players are not special cases.

They are persistent entities that happen to receive decision input from humans instead of AI.

---

# 11. Player Lifecycle

## 11.1 Overview

A player does not simply "spawn" into the game world.

Instead, the player progresses through a well-defined lifecycle that separates networking, persistence, simulation, and gameplay responsibilities.

This separation prevents architectural coupling between unrelated systems and simplifies recovery from disconnects, deaths, and server restarts.

The complete lifecycle is shown below.

```
Character Created

        │

        ▼

Identity Loaded

        │

        ▼

Connection Established

        │

        ▼

Session Created

        │

        ▼

Actor Allocated

        │

        ▼

Registered with World

        │

        ▼

Registered with ALife

        │

        ▼

Simulation Begins

        │

        ▼

Gameplay

        │

 ┌──────┴───────────┐
 │                  │
 │                  │
 ▼                  ▼

Disconnect       Death

 │                  │

 ▼                  ▼

Reconnect     Respawn

 │                  │

 └──────┬───────────┘

        ▼

Continue Simulation
```

Each transition is owned by exactly one subsystem.

---

## 11.2 Lifecycle Invariants

The following rules must always hold.

A player cannot participate in simulation before registration.

An Actor cannot exist without an Identity.

A Session cannot exist without a Connection.

Persistent Identity survives all temporary failures.

Simulation ownership never changes during gameplay.

These invariants simplify debugging and ensure deterministic behavior.

---

# 12. Connection Flow

## 12.1 Purpose

Connection establishes communication between the client and the host.

It does not automatically place a player into the world.

Gameplay begins only after successful validation and synchronization.

---

## 12.2 Connection Sequence

```
Client

    │

    ▼

Network Handshake

    │

    ▼

Protocol Validation

    │

    ▼

Authentication

    │

    ▼

Identity Lookup

    │

    ▼

Session Creation

    │

    ▼

Load Character

    │

    ▼

Spawn Actor

    │

    ▼

World Synchronization

    │

    ▼

Gameplay
```

Each stage must complete successfully before advancing.

---

## 12.3 Validation

Before a player enters the simulation, the host validates:

- protocol version
- save compatibility
- character ownership
- session uniqueness
- available player slot

Only validated players may proceed.

---

## 12.4 Session Initialization

Once validated, the Multiplayer subsystem creates a Player Session.

The session records:

- connection identifier
- player identity
- latency metrics
- synchronization progress
- ownership information

The session itself contains no gameplay state.

---

# 13. Join-in-Progress

## 13.1 Overview

STALKER-MP supports joining an already active simulation.

The world is never paused while a player connects.

Instead, the joining player synchronizes to the current authoritative state.

---

## 13.2 Synchronization Process

```
Current World

↓

Snapshot Generation

↓

Player State

↓

Nearby Entities

↓

Relevant ALife State

↓

Inventory

↓

Quest State

↓

Gameplay Begins
```

The joining client reconstructs the existing simulation rather than creating a new one.

---

## 13.3 Synchronization Priorities

Synchronization occurs in stages.

Priority 1

Player Actor

Priority 2

Nearby entities

Priority 3

NPCs

Priority 4

Dynamic objects

Priority 5

Environmental effects

Priority 6

Background simulation

This ordering reduces perceived connection time.

---

## 13.4 World Continuity

Existing players continue gameplay during synchronization.

The simulation never waits for joining players.

---

# 14. Disconnect Handling

## 14.1 Philosophy

Disconnecting should not destroy the world.

Only the player's participation changes.

The Zone continues to evolve.

---

## 14.2 Disconnect Flow

```
Connection Lost

↓

Session Closed

↓

Input Removed

↓

Actor Preserved

↓

Persistence Updated

↓

Simulation Continues
```

The Actor does not disappear immediately.

The handling policy is defined by future gameplay decisions.

---

## 14.3 Temporary Disconnect

For short interruptions:

- identity remains loaded
- Actor remains in the world
- inventory remains unchanged
- ALife continues simulation

Reconnect simply reattaches the client.

---

## 14.4 Permanent Disconnect

Long-term disconnects may result in:

- Actor removal
- persistence save
- ALife deregistration
- resource cleanup

Policy decisions belong to gameplay rather than engine architecture.

---

# 15. Respawn Model

## 15.1 Purpose

Respawning creates a new Actor while preserving player identity.

Identity represents the character.

The Actor represents its current physical existence.

---

## 15.2 Separation

```
Identity

↓

Actor A

↓

Death

↓

Actor Destroyed

↓

Respawn

↓

Actor B
```

Identity remains continuous.

Actors are replaceable.

---

## 15.3 Responsibilities

Respawn performs:

- Actor allocation
- inventory initialization
- position selection
- world registration
- ALife registration
- replication initialization

Identity is never recreated.

---

# 16. Death

## 16.1 Architectural View

Death is treated as a simulation event rather than a networking event.

Simulation determines:

- cause
- timing
- consequences

Networking merely communicates the result.

---

## 16.2 Death Sequence

```
Combat

↓

Damage

↓

Health

↓

Death

↓

Simulation Update

↓

Persistence

↓

Replication
```

The host determines every stage.

---

## 16.3 Persistent Consequences

Gameplay systems may decide:

- equipment loss
- reputation changes
- quest failure
- faction reactions

The Player subsystem merely supports these outcomes.

---

# 17. Inventory Ownership

## 17.1 Ownership

Player inventory remains host-authoritative.

Clients never modify inventory directly.

Clients request actions.

The host validates those actions.

---

## 17.2 Inventory Flow

```
Client Input

↓

Interaction Request

↓

Host Validation

↓

Inventory Update

↓

Replication

↓

Client UI
```

Inventory state always originates from simulation.

---

## 17.3 Benefits

Host authority prevents:

- duplication
- desynchronization
- inconsistent trading
- invalid equipment states

Inventory becomes part of the persistent world.

---

# 18. Player and ALife Interaction

## 18.1 Philosophy

ALife should interact with players in the same way it interacts with NPC stalkers whenever possible.

Players are not external observers.

They are participants.

---

## 18.2 Architectural Relationship

```
Player

↓

Actor

↓

ALife Object

↓

Scheduler

↓

Simulation
```

Human decision making replaces AI decision making.

Everything else remains identical.

---

## 18.3 Smart Terrain

Smart Terrain should recognize players as occupants of the world.

Examples include:

- territorial influence
- combat participation
- threat evaluation
- squad interaction
- environmental reactions

The exact gameplay behavior is defined elsewhere.

---

## 18.4 Faction Simulation

Players participate in faction simulation.

Faction planners should consider:

- player affiliation
- reputation
- hostile actions
- completed objectives

Players influence the simulation without becoming its controller.

---

# 19. Online / Offline Transition

## 19.1 Overview

One of the defining characteristics of the X-Ray Engine is the ability to transition entities between online and offline simulation.

STALKER-MP preserves this mechanism.

Players do not replace the online/offline system—they become participants within it.

Unlike the original single-player implementation, online state is no longer determined solely by the local actor.

Instead, online state is evaluated against the combined presence of every connected player.

---

## 19.2 Architectural Goal

The objective is to preserve ALife scalability while supporting multiple human participants.

The Player subsystem therefore never decides whether entities become online or offline.

That responsibility remains entirely within the World and ALife subsystems.

Players merely contribute to the evaluation.

---

## 19.3 Transition Model

Original Engine

```
Entity

↓

Distance to Local Actor

↓

Online
```

STALKER-MP

```
Entity

↓

Distance to Nearest Connected Player

↓

World Evaluation

↓

Online
```

This seemingly small change fundamentally enables multiplayer while preserving ALife behavior.

---

## 19.4 Player Participation

Every connected player contributes an active simulation region.

Conceptually:

```
Player A

        ◯

Player B

                      ◯

Player C

                                 ◯
```

The World subsystem merges these regions into a unified online simulation.

Entities transition online if they intersect any active simulation region.

---

## 19.5 Benefits

Maintaining ALife ownership provides several advantages.

- Existing scheduling logic remains reusable.
- NPC behavior remains deterministic.
- Smart Terrain logic remains unchanged.
- Offline simulation continues to function.
- Simulation scales with player distribution.

---

# 20. Level Transitions

## 20.1 Purpose

The Zone consists of multiple interconnected levels.

Players may travel between them independently.

The Player subsystem does not own level streaming.

Instead, it coordinates player movement with the World subsystem.

---

## 20.2 Transition Flow

```
Player Reaches Transition

↓

Host Validation

↓

Save Temporary State

↓

World Transfer

↓

Load Destination Level

↓

Register Actor

↓

Resume Simulation
```

The transfer is authoritative.

Clients cannot initiate arbitrary level changes.

---

## 20.3 Multi-Level Simulation

Different players may occupy different levels simultaneously.

Example:

```
Player A

Cordon

Player B

Garbage

Player C

Army Warehouses
```

Despite geographic separation:

- All players remain within the same persistent world.
- ALife continues across all loaded and offline levels.
- Story progression remains globally consistent.

---

## 20.4 Responsibilities

The Player subsystem is responsible for:

- requesting the transition
- preserving player state
- restoring actor state after transfer

The World subsystem is responsible for:

- loading the destination
- entity activation
- streaming
- simulation ownership

---

# 21. Persistence

## 21.1 Overview

Persistence ensures that player progress survives beyond a single gameplay session.

Persistence belongs to the Persistence subsystem.

The Player subsystem defines **what** must be persisted, not **how** it is stored.

---

## 21.2 Persistent Data

Persistent player information includes:

Identity

- Player identifier
- Character name
- Faction
- Rank
- Reputation

Actor

- Position
- Health
- Stamina
- Equipment
- Inventory

Progression

- Quests
- Story progression
- Statistics
- Discovered locations

Simulation

- Current level
- Active Smart Terrain relationships
- Reputation changes
- Faction influence

---

## 21.3 Non-Persistent Data

The following data is transient and should never be stored as part of persistent character state.

- latency
- packet statistics
- prediction buffers
- interpolation history
- temporary animations
- camera position
- UI state

---

## 21.4 Save Triggers

Persistence may occur under several conditions.

Examples include:

- manual save
- scheduled save
- disconnect
- level transition
- server shutdown

The exact strategy is defined within Persistence.md.

---

## 21.5 Reconnection

Reconnect is conceptually different from spawning.

Reconnect restores an existing Actor whenever possible.

```
Disconnect

↓

Save

↓

Reconnect

↓

Load Existing Actor

↓

Continue Simulation
```

The player's history remains uninterrupted.

---

# 22. Failure Scenarios

## 22.1 Philosophy

Failures should affect as little of the simulation as possible.

A player failure should not become a world failure.

---

## 22.2 Lost Connection

```
Connection Lost

↓

Session Closed

↓

Input Removed

↓

Actor Remains

↓

Simulation Continues
```

The Zone does not pause.

---

## 22.3 Failed Synchronization

If synchronization cannot complete:

- Actor is not activated.
- Registration is rolled back.
- Session is terminated.
- World state remains unchanged.

Partial joins are never permitted.

---

## 22.4 Invalid State

Examples include:

- duplicated identity
- missing inventory
- invalid world position
- corrupted persistence

Recovery should prioritize preservation of authoritative simulation.

Invalid player state must never corrupt the world.

---

## 22.5 Host Failure

Host failure affects the entire simulation because the host owns the authoritative world.

Future dedicated server support may introduce more advanced recovery mechanisms.

Version 1 assumes host failure terminates the session.

---

# 23. Future Extensions

The architecture intentionally leaves room for future capabilities.

Potential additions include:

- dedicated servers
- character migration
- persistent online characters
- server transfers
- spectator mode
- AI takeover of disconnected players
- cooperative vehicles
- advanced squad systems
- administrative player controls

These additions should extend existing architecture rather than replace it.

---

# 24. Design Rationale

Several important architectural decisions define the Player subsystem.

---

## Players Are Entities

Treating players as ALife entities allows them to naturally participate in the simulation.

Special-case logic is minimized.

---

## Identity Is Independent

Separating identity from the physical Actor simplifies persistence, death, reconnects, and future character migration.

---

## Host Authority

Centralizing authority guarantees deterministic gameplay and prevents conflicting world state.

---

## Simulation Before Networking

Networking communicates simulation.

It never becomes responsible for gameplay.

---

## Explicit Ownership

Separating ownership between client, host, persistence, and multiplayer reduces ambiguity and simplifies debugging.

---

## Reuse Existing Systems

The Player subsystem intentionally builds upon existing X-Ray actor infrastructure rather than replacing it.

This minimizes implementation effort and preserves engine compatibility.

---

# 25. Summary

The Player subsystem defines how human participants exist within the persistent Zone.

Rather than acting as privileged controllers of the world, players are represented as persistent entities integrated directly into the existing ALife simulation.

The architecture separates identity, session, connection, and Actor representation into distinct layers, allowing each to evolve independently while maintaining clear ownership boundaries.

The Player subsystem follows several guiding principles:

- Players are participants, not the center of the simulation.
- Identity persists independently of physical Actors.
- The host remains authoritative over all gameplay state.
- Connections are temporary; characters are persistent.
- Players integrate with ALife instead of replacing it.
- Networking transports player intent rather than gameplay state.

These principles enable seamless integration with the World, ALife, Multiplayer, Replication, and Persistence subsystems while preserving the autonomous nature of the Zone.

The Player architecture established here serves as the foundation for all player-related RFCs and implementation work.
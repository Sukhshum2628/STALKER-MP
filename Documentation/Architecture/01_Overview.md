# STALKER-MP Architecture Overview

**Document Version:** 1.0

**Status:** Draft

**Audience:** Engine Developers, Contributors, Technical Designers

---

# 1. Purpose

This document defines the high-level architecture of STALKER-MP.

Unlike implementation documents, this specification focuses on system organization, ownership boundaries, design philosophy, and subsystem responsibilities rather than algorithms or source code.

It serves as the architectural foundation for every subsequent document in the STALKER-MP architecture.

No implementation should begin before the concepts described here are accepted.

---

# 2. Vision

The goal of STALKER-MP is to transform S.T.A.L.K.E.R. Anomaly into a persistent multiplayer experience while preserving the autonomous ALife simulation that defines the identity of the X-Ray Engine.

Traditional multiplayer games are designed around players being the primary actors.

STALKER-MP deliberately rejects this model.

Instead, the Zone itself remains the primary actor.

Human players become participants inside an already-living ecosystem.

The simulation continues regardless of player activity.

Players influence the simulation but do not define it.

---

# 3. Project Philosophy

Several principles guide every architectural decision.

## Preserve Before Replace

Existing X-Ray systems should be reused whenever practical.

Replacement is considered only when extension is technically impossible or introduces unacceptable complexity.

---

## Simulation First

Networking exists to transport the simulation.

Networking never replaces the simulation.

Simulation correctness always has higher priority than network convenience.

---

## Host Authority

The host maintains the authoritative state of the entire Zone.

Clients never become authoritative over persistent world data.

---

## One Persistent World

There is exactly one persistent simulation.

All connected players observe and interact with the same evolving world.

No player owns an independent instance of the Zone.

---

## Players Are Stalkers

Players are treated as entities inside ALife rather than external controllers.

Architecturally, players participate in the same ecosystem as NPC stalkers.

---

# 4. Architectural Goals

The architecture is designed to satisfy the following goals.

## Goal 1

Preserve existing ALife behavior.

## Goal 2

Support multiple simultaneous players.

## Goal 3

Maintain persistent world state.

## Goal 4

Reuse engine infrastructure whenever possible.

## Goal 5

Keep networking modular.

## Goal 6

Minimize modifications to unrelated engine systems.

## Goal 7

Support future dedicated server architecture.

## Goal 8

Provide clear subsystem ownership.

---

# 5. High-Level System Architecture

```
                +----------------------+
                |      Game Client     |
                +----------+-----------+
                           |
                           v
                +----------------------+
                |    Player System     |
                +----------+-----------+
                           |
                           v
                +----------------------+
                |    World Manager     |
                +----------+-----------+
                           |
           +---------------+---------------+
           |                               |
           v                               v
   +---------------+               +---------------+
   | ALife System  |               | Networking    |
   +---------------+               +---------------+
           |                               |
           +---------------+---------------+
                           |
                           v
                  Replication Layer
                           |
                           v
                     Connected Clients
```

The World Manager forms the architectural center of the project.

Every major subsystem communicates through clearly defined ownership boundaries rather than directly modifying unrelated systems.

---

# 6. Core Subsystems

The project is divided into several major architectural subsystems.

## Engine

Defines modification boundaries within the X-Ray Engine.

Responsible for integrating multiplayer support while preserving existing engine behavior.

---

## Player

Represents human-controlled stalkers.

Handles player lifecycle, ownership, persistence, and interaction with the world.

---

## World

Represents the persistent Zone.

Coordinates simulation ownership, online/offline transitions, and entity management.

---

## ALife

Maintains autonomous simulation.

Responsible for NPC behavior, smart terrains, faction simulation, offline processing, and story progression.

---

## Multiplayer

Handles connections, sessions, player registration, ownership negotiation, and network topology.

---

## Replication

Synchronizes authoritative world state from the host to connected clients.

Responsible for serialization, prediction, interpolation, and bandwidth management.

---

## Persistence

Maintains save games and long-term world continuity.

Responsible for reconnect support and player persistence.

---

## Threading

Defines execution ownership across engine threads.

Ensures deterministic simulation while allowing asynchronous networking and persistence.

---

## Extensibility

Provides interfaces for future plugins, scripting, and dedicated server support.

---

# 7. Ownership Model

Ownership is fundamental to the architecture.

The host owns:

- World state
- ALife
- AI
- Physics
- Inventory
- Story progression
- Save games
- Faction simulation
- Smart terrains

Clients own:

- Local input
- Camera
- Rendering
- Audio
- User interface
- Client-side prediction
- Visual interpolation

Ownership is never ambiguous.

Every subsystem has exactly one authoritative owner.

---

# 8. Data Flow

Information flows through the system in a controlled manner.

```
Player Input
      │
      ▼
Host Validation
      │
      ▼
Simulation
      │
      ▼
ALife Update
      │
      ▼
Replication
      │
      ▼
Client Presentation
```

The simulation always executes before replication.

Replication always reflects the authoritative simulation.

---

# 9. Layered Architecture

The project follows a layered architecture to reduce coupling.

```
Presentation Layer

↓

Player Layer

↓

World Layer

↓

Simulation Layer

↓

Engine Layer
```

Higher layers depend on lower layers.

Lower layers never depend on presentation systems.

---

# 10. Architectural Principles

The following principles apply to every subsystem.

- Single responsibility
- Explicit ownership
- Deterministic simulation
- Host authority
- Composition over replacement
- Modular networking
- Reuse existing engine systems
- Preserve ALife behavior
- Minimize coupling
- Maximize extensibility

---

# 11. Future Architecture

Version 1 targets host-authoritative multiplayer.

Future versions may introduce:

- Dedicated servers
- Plugin APIs
- Extended Lua interfaces
- Administrative tools
- Cross-session persistence improvements

These additions must preserve the architectural principles defined in this document.

---

# 12. Relationship to Other Documents

This document defines the overall architecture.

Detailed subsystem specifications are provided in the remaining architecture documents:

- 02_Engine.md
- 03_Player.md
- 04_World.md
- 05_ALife.md
- 06_Multiplayer.md
- 07_Replication.md
- 08_Persistence.md
- 09_Threading.md
- 10_Extensibility.md

Each document expands on the responsibilities introduced here without redefining the overall architecture.

---

# 13. Summary

STALKER-MP extends the X-Ray Engine by introducing multiplayer participation into an existing autonomous simulation.

The project preserves ALife as the authoritative simulation while adding networking, persistence, and player management as supporting systems.

Every architectural decision should reinforce the project's central philosophy:

- Preserve before replace.
- The Zone remains the primary actor.
- Networking transports simulation rather than replacing it.
- One persistent world is shared by all connected players.
- The host remains authoritative over the simulation.
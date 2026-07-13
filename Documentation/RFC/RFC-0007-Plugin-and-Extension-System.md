# RFC-0007: Plugin and Extension System
# RFC-0007: Plugin & Extension System

| Field | Value |
|--------|-------|
| RFC | 0007 |
| Title | Plugin & Extension System |
| Status | Accepted |
| Author | STALKER-MP Project |
| Created | July 2026 |
| Depends On | RFC-0001, RFC-0002, RFC-0003, RFC-0004, RFC-0005, RFC-0006, 10_Extensibility.md |
| Supersedes | None |
| Superseded By | None |

---

# 1. Abstract

This RFC defines the Plugin & Extension System for STALKER-MP.

The architecture allows future gameplay features, Lua scripts, developer tools, diagnostics, server plugins, and mod integrations to extend engine functionality without modifying the core multiplayer framework.

The extension system is intentionally constrained by the architectural principles established by previous RFCs. Plugins may observe, augment, and react to simulation events, but they may never violate host authority, deterministic simulation, immutable snapshots, or subsystem ownership.

The objective is to provide long-term extensibility while preserving the integrity of the authoritative simulation.

---

# 2. Motivation

A multiplayer framework intended for long-term development must support extension.

Without a defined extension model:

- developers modify engine source directly;
- engine updates become difficult;
- integrations become tightly coupled;
- compatibility decreases over time;
- maintenance cost increases.

A stable extension architecture allows new functionality to be developed independently while preserving core engine behavior.

---

# 3. Problem Statement

External systems require access to engine functionality.

Examples include:

- Lua gameplay scripts;
- server administration tools;
- analytics;
- anti-cheat systems;
- Discord integrations;
- web dashboards;
- replay systems;
- custom game modes.

Allowing unrestricted access to engine internals would violate the ownership model established throughout the architecture.

A controlled extension interface is therefore required.

---

# 4. Goals

The extension system must:

- preserve host authority;
- preserve deterministic simulation;
- isolate third-party code;
- support Lua integration;
- support dedicated server plugins;
- expose stable APIs;
- minimize engine coupling;
- maintain backward compatibility where practical.

---

# 5. Non-Goals

This RFC does not define:

- scripting language syntax;
- plugin packaging;
- dependency management;
- distribution mechanisms;
- online plugin repositories;
- marketplace functionality.

These remain implementation concerns.

---

# 6. Decision

STALKER-MP adopts a layered extension architecture.

Extensions interact with the engine exclusively through public APIs and event interfaces.

Plugins never directly manipulate engine internals.

Core simulation remains authoritative.

The extension layer exists outside the simulation core.

---

# 7. Extension Principles

Every extension must follow these principles.

---

## Respect Ownership

Plugins must not bypass subsystem ownership.

Examples:

- World owns entities.
- ALife owns behavior.
- Scheduler owns execution.
- Replication owns synchronization.
- Persistence owns serialization.

Plugins request services through public APIs.

---

## Preserve Determinism

Plugins must never introduce nondeterministic gameplay behavior.

Simulation execution order must remain predictable.

---

## No Direct Memory Access

Plugins interact through stable interfaces.

Direct manipulation of engine objects is prohibited.

---

## API First

Every supported extension point must exist as an explicit API.

Undefined behavior is not considered part of the public interface.

---

# 8. Extension Types

The architecture supports several categories of extensions.

---

## Gameplay Plugins

Provide new gameplay mechanics.

Examples:

- quests;
- factions;
- economy systems;
- dynamic events.

Gameplay plugins execute within the Simulation Thread.

---

## Lua Scripts

Lua remains the primary scripting language.

Lua interacts only through exposed APIs.

Lua cannot modify internal engine ownership.

---

## Server Plugins

Dedicated servers may load server-side extensions.

Examples:

- administration;
- moderation;
- statistics;
- automated events;
- external services.

---

## Tooling Plugins

Developer tools may consume snapshots.

Examples:

- debuggers;
- profilers;
- replay viewers;
- visualization tools.

---

# 9. Event System

The engine exposes well-defined lifecycle events.

Examples include:

- world initialization;
- player connected;
- player disconnected;
- entity spawned;
- entity destroyed;
- simulation tick;
- save started;
- save completed;
- server shutdown.

Events represent observation points.

They do not transfer ownership.

---

# 10. Public API

The public API serves as the contract between the engine and extensions.

The API should expose:

- player information;
- entity queries;
- inventory interfaces;
- world queries;
- weather state;
- server information;
- logging;
- command registration.

Internal implementation details remain hidden.

---

# 11. Thread Safety

Extensions must respect the threading architecture defined in RFC-0006.

Simulation APIs execute only on the Simulation Thread.

Worker threads may consume immutable snapshots.

Extensions must never access live simulation from worker threads.

---

# 12. Design Rationale

## Preserve Core Stability

The multiplayer framework should remain stable while gameplay evolves independently.

---

## Minimize Engine Forks

Stable extension points reduce the need for direct engine modification.

Future updates become easier.

---

## Improve Maintainability

Independent modules are easier to develop, test, and replace.

---

## Encourage Community Development

A documented extension system enables external contributors to build new functionality without understanding every internal subsystem.

---

# 13. Alternatives Considered

## Direct Engine Modification

Rejected.

Reason:

Creates maintenance burden.

Breaks upgrade compatibility.

---

## Unlimited Engine Access

Rejected.

Reason:

Violates subsystem ownership.

Increases crash risk.

---

## Reflection-Based Access

Rejected.

Reason:

Poor performance.

Weak compile-time guarantees.

---

## Scripting Without APIs

Rejected.

Reason:

Leads to undocumented behavior.

Creates fragile integrations.

---

# 14. Failure Isolation

Plugins should fail independently of the engine.

A plugin failure should not corrupt:

- world state;
- ALife;
- persistence;
- networking;
- scheduler.

Where possible, failures should be logged and isolated.

---

# 15. Versioning

The extension API should evolve independently of internal engine implementation.

Breaking API changes require:

- version increment;
- migration documentation;
- deprecation period where practical.

Internal refactoring should not invalidate existing plugins.

---

# 16. Future Evolution

Future capabilities may include:

- hot-reloadable plugins;
- remote administration APIs;
- REST interfaces;
- WebSocket integrations;
- dedicated server SDK;
- scripting sandboxing;
- plugin dependency management.

All future extensions must preserve:

- host authority;
- immutable snapshots;
- deterministic simulation;
- subsystem ownership.

---

# 17. Risks

Potential risks include:

- unstable third-party plugins;
- API misuse;
- excessive plugin overhead;
- compatibility fragmentation.

Mitigation strategies include:

- stable public APIs;
- documentation;
- versioning;
- validation;
- plugin isolation.

---

# 18. Acceptance Criteria

The Plugin & Extension System is considered correctly implemented when:

- plugins use only documented APIs;
- subsystem ownership is never bypassed;
- plugins cannot modify authoritative state directly;
- Lua scripts execute through supported interfaces;
- worker-thread plugins consume immutable snapshots only;
- plugin failures do not compromise simulation integrity.

---

# 19. References

Architecture Documents

- 10_Extensibility.md

Related RFCs

- RFC-0001 — Host Authoritative Simulation
- RFC-0002 — MultiPlayer Bubble System
- RFC-0003 — Immutable Snapshot Architecture
- RFC-0004 — Replication Pipeline
- RFC-0005 — Persistence Architecture
- RFC-0006 — Threading Execution Model
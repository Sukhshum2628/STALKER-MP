# Sprint-003: Entity Registry Refactor

---

# Sprint Information

| Field | Value |
|--------|-------|
| Sprint | 003 |
| Title | Entity Registry Refactor |
| Status | Implemented & Verified (Closed) |
| Priority | Critical |
| Estimated Duration | 2–3 Weeks |
| Prerequisites | Sprint-001, Sprint-002 |
| Next Sprint | Sprint-004 – Bubble Manager |
| Governing ADR | ADR-007 — Engine ABI Contract |
| Design Review | `Multiplayer/docs/Sprint-003-Design-Review.md` (Frozen) |

> **Freeze note.** This specification incorporates every accepted decision from the
> Final Architectural Gate. The architecture is frozen. Two items remain open as
> **pre-implementation evidence gates** (§14) — they select internal mechanisms
> (a lookup container and a feed mechanism) that are encapsulated behind the
> frozen contract and cannot reopen it.

---

# 1. Sprint Overview

Sprint-003 introduces the authoritative Entity Registry for STALKER-MP.

The Entity Registry is the canonical **record of identity and registration** for every
persistent entity in the Zone. It does **not** own engine object lifetime: the X-Ray
engine retains mechanical creation and destruction of `CObject`/`CSE_Abstract`; the
registry records identity and metadata and mirrors engine spawn/destroy through the
single engine observation layer (`EngineAdapters.cpp`).

Rather than allowing various engine subsystems to maintain independent collections of
objects, the World subsystem becomes the sole owner of entity **registration records**.
The registry is the canonical lookup mechanism for all future multiplayer systems while
preserving compatibility with the existing engine.

This sprint does not modify ALife behavior, online/offline simulation, or networking.

---

# 2. Objectives

## Primary Objectives

- Create Entity Registry (identity + metadata records)
- Define entity registration ownership (records, not engine lifetime)
- Create registration pipeline fed by `EngineAdapters.cpp`
- Create lookup interfaces
- Define entity registration lifecycle
- Create persistent `EntityId` and runtime `EntityHandle`
- Create registration lifecycle events
- Integrate with World subsystem behind the existing `IEntitySource` seam

## Secondary Objectives

- Debug visualization
- Registry statistics (metadata-only)
- Performance profiling
- Future snapshot support (identity only)

---

# 3. Scope

Included

- Entity Registry (record store)
- Persistent `EntityId` + runtime `EntityHandle`
- Registration / Deregistration API
- Lookup API
- Registration lifecycle events
- Deterministic entity iteration (ascending `EntityId`)
- Registry statistics (metadata-only)

---

# 4. Out of Scope

Not included

- Bubble Manager
- Replication
- Persistence and serialization format
- Snapshot generation
- Online/Offline simulation and **activation/deactivation events** (Sprint-004/005)
- Engine object spawn/destroy control (Sprint-005)
- Spatial iteration (remains in `ISpatialQueries`, Sprint-002)
- Player synchronization / session state
- AI modifications
- Smart Terrain modifications

---

# 5. Architecture References

- 04_World.md
- 05_ALife.md
- ADR-007 — Engine ABI Contract
- `Multiplayer/docs/Sprint-003-Design-Review.md` (Frozen)

---

# 6. RFC References

- RFC-0001 — Host Authoritative Simulation
- RFC-0002 — MultiPlayer Bubble System
- RFC-0003 — Immutable Snapshot Architecture

---

# 7. Technical Tasks

## 7.0 Governing Architectural Invariants (Frozen)

These invariants are binding for all of Sprint-003 and derive from the Final
Architectural Gate. Implementation must not violate them.

- **I1 — Record ownership, not lifetime.** The registry owns entity **identity and
  registration records** only. The engine retains mechanical object lifetime.
  Deregistration mirrors engine destroy; the registry never executes engine teardown.
- **I2 — One engine feed.** Engine spawn/destroy is observed **only** in
  `EngineAdapters.cpp`, which feeds register/unregister into the registry. The registry
  itself includes no engine headers and is engine-agnostic (preserves One Engine
  Boundary; keeps `EngineAdapters.cpp` the sole engine-header TU).
- **I3 — Canonical order.** The canonical iteration order is **ascending `EntityId`**.
  Any hash/associative structure is a secondary lookup index only and never defines
  iteration order.
- **I4 — Identity vs handle.** Persistent `EntityId` (stable, serialized identity) is
  distinct from runtime `EntityHandle{EntityId, generation}` (session-local). Generation
  is never persisted as identity.
- **I5 — Registration events only.** Events cover the registration lifecycle only.
  Activation/deactivation (online/offline) belong to Sprint-004/005.
- **I6 — Spatial stays out.** The registry exposes no spatial query; `ISpatialQueries`
  owns spatial iteration over the registry as backing store.
- **I7 — Metadata-only statistics.** Statistics count by entity-category metadata only.
  No session, connection, or gameplay state may enter the registry (WorldContext-leak
  lesson, AI_CONTEXT.md).
- **I8 — Single-writer threading.** The registry is accessed only on the host simulation
  thread. Cross-thread consumers use RFC-0003 snapshots, never direct registry access.
- **I9 — Deterministic, non-reentrant events.** Event dispatch is non-fallible (void),
  in deterministic subscriber-registration order; handlers must not mutate the registry
  during dispatch (defer if needed).
- **I10 — Engine ABI Contract conformance.** All registry translation units are
  engine-linked and must satisfy ADR-007. All internal data structures must be
  C4530-clean under `EngineAbi.props`; engine containers (`xr_hash_map`, `robin_hood`)
  are prohibited outside `EngineAdapters.cpp`.

---

## 7.1 Entity Registry

Create

- EntityRegistry (record store; engine-agnostic; implements `world::IEntitySource`)
- EntityRegistryService
- EntityRegistryConfig

Responsibilities

- Identity and registration record ownership (I1)
- Registration / Deregistration
- Lookup
- Deterministic enumeration (I3)

Not responsibilities

- Engine object lifetime, behavior, spatial indexing, session state.

---

## 7.2 Identity and Handles

Create two distinct concepts (I4):

- **`EntityId`** — persistent, globally unique, immutable identity. Assigned at
  registration; serialized as identity (format deferred to Sprint-011).
- **`EntityHandle` = { EntityId, generation }** — runtime reference. Generation is
  session-local, incremented on `EntityId` reuse, never persisted as identity.

Support

- Validation, Null Handle, Equality, Hashing (of `EntityHandle`)
- Id-space policy: relationship to engine `u16` object ids, reuse disambiguation, and a
  bidirectional engine-id ↔ `EntityId` index maintained by the feed (I2).

Avoid exposing raw engine pointers.

---

## 7.3 Entity Registration

Implement

- RegisterEntity()
- UnregisterEntity()
- ReserveEntity()  — pre-allocate a deterministic `EntityId` (host-authoritative)
- RestoreEntity()  — register a pre-assigned identity + metadata (format-agnostic;
  not coupled to the provisional `ISerializable`, D-006)

Validate uniqueness. All fallible operations return `core::Expected<T>` (ADR-007).

Registration is driven by the engine feed in `EngineAdapters.cpp` (I2).

---

## 7.4 Entity Lookup

Support

- FindByID()
- FindByName()
- FindByType()
- FindBySection()
- Exists()
- GetAll()  — returns entities in canonical ascending-`EntityId` order (I3)

Lookup operations are deterministic.

---

## 7.5 Registry Ownership

World owns:

- Entity **identity** and **registration records**
- Registration / Deregistration

The engine retains mechanical object lifetime; deregistration mirrors engine destroy (I1).

Other systems reference entities via handles. They never own them.

---

## 7.6 Entity Registration Lifecycle

Define the **registration** lifecycle (distinct from ALife online/offline, Sprint-004/005):

```
Construct → Register → Initialize → Active(registered-live) → PendingRemoval → Unregister
```

"Active" here means "registered and live in the record store" — it is **not** the
ALife online/offline state (`05_ALife.md §12.3`). Transitions are deterministic.

---

## 7.7 Registration Events

Expose registration-lifecycle events only (I5, I9):

- OnEntityRegistered
- OnEntityUnregistered
- OnEntityRestored

Dispatch is non-fallible (void), in deterministic subscriber-registration order, and
non-reentrant (handlers must not mutate the registry during dispatch). Subscription
lifetime is owned by the subscriber; the registry holds non-owning callbacks cleared at
shutdown.

Activation/deactivation events are **out of scope** (Sprint-004/005).

---

## 7.8 Iteration Interfaces

Support

- ForEach()          — canonical ascending-`EntityId` order (I3)
- ForEachOfType()
- ForEachActive()    — registered-live records

`ForEachNearby` is **not** part of the registry (I6); spatial iteration remains in
`ISpatialQueries`, which iterates the registry as its backing store. Future parallel
iteration deferred.

---

## 7.9 Query Optimization

Prepare

- ID lookup (primary, canonical order)
- Hash lookup (secondary index only — subject to the ABI probe, §14 Gate 1)
- Type lookup
- Section lookup

Future spatial indexing lives in `ISpatialQueries`, not here.

All lookup structures must be C4530-clean under `EngineAbi.props` (I10).

---

## 7.10 Entity Metadata

Store (metadata only; behavior belongs elsewhere — `04_World.md §12`):

- Persistent `EntityId`
- Entity Type / Category
- Section
- Flags
- Creation Tick
- Spawn Source
- Runtime registration state

---

## 7.11 Registry Statistics

Track by **entity-category metadata only** (I7; no session/connection state):

- Entity Count
- NPC Count
- Monster Count
- Item Count
- Vehicle Count
- Player Count (category attribute only — never session/connection state)
- Dynamic Spawn Count
- Destroyed (Unregistered) Count

---

## 7.12 Debug Utilities

Support

- Entity Inspector
- Registry Dump (canonical order)
- Duplicate Detection
- Invalid Handle Detection (generation mismatch)
- Missing Entity Validation

---

## 7.13 Performance Metrics

Measure

- Lookup time, Registration time, Removal time, Iteration time, Memory usage

---

## 7.14 Testing

Verify

- Registration, Removal, Duplicate IDs
- Handle validity (generation), Null handle
- Deterministic iteration (ascending `EntityId`)
- Lookup correctness
- Registration lifecycle transitions
- Event ordering and non-reentrancy (I9)
- Metadata-only statistics (I7)
- Stress test with thousands of entities

The registry is unit-tested engine-free using the `NullEngineAdapters` feed pattern.

---

## 7.15 Documentation

Document

- Registry record ownership (I1), engine feed (I2)
- Identity vs handle (I4), id-space policy
- Registration lifecycle and events
- Public interfaces, extension guidelines
- Threading invariant (I8)

---

# 8. Deliverables

✓ Entity Registry (record store)
✓ Persistent `EntityId` + runtime `EntityHandle`
✓ Registration system (fed by `EngineAdapters.cpp`)
✓ Lookup API (deterministic)
✓ Registration lifecycle events
✓ Statistics (metadata-only)
✓ Diagnostics
✓ Tests
✓ Documentation

---

# 9. Risks

Potential Risks

- Duplicate ownership (mitigated by I1 record-only ownership)
- Dangling references (mitigated by `EntityHandle` generation)
- Invalid handles (mitigated by generation validation)
- Non-deterministic iteration (mitigated by I3)
- ABI non-conformance of lookup containers (Gate 1, §14)
- Feed mechanism / engine observation (Gate 2, §14)

Mitigation

- Record-only central ownership, handle validation, canonical order, registry
  assertions, profiling, strict lifecycle rules, ADR-007 conformance.

---

# 10. Validation Strategy

- Registration / Removal correctness
- Deterministic iteration (ascending `EntityId`)
- All lookup methods verified
- Invalid-handle detection via generation
- Large-scale registry benchmark
- Registry stable with large entity counts

---

# 11. Acceptance Criteria

□ Entity Registry operational (records only).
□ Registration pipeline complete, fed exclusively by `EngineAdapters.cpp`.
□ Persistent `EntityId` and runtime `EntityHandle` functional and distinct.
□ Lookup interfaces operational and deterministic (ascending `EntityId`).
□ Registration lifecycle events implemented (non-fallible, deterministic, non-reentrant).
□ Statistics metadata-only.
□ Registry engine-agnostic; `EngineAdapters.cpp` remains the only engine-header TU.
□ All registry TUs conform to ADR-007 (C4530-clean).
□ Tests passing.
□ Documentation updated.
□ No engine object-lifetime ownership violations.

---

# 12. Definition of Done

Sprint-003 is complete when

- World owns the **identity and registration record** of every persistent entity.
- The registry is the sole source of entity lookup, in canonical ascending-`EntityId` order.
- All entity access occurs through the registry via handles.
- Engine object lifetime remains with the engine; the registry only mirrors it via the
  `EngineAdapters.cpp` feed.
- Persistent `EntityId` and runtime `EntityHandle` are distinct; generation is never
  persisted as identity.
- Registration-lifecycle events only; no activation/deactivation.
- Statistics are metadata-only.
- The registry is accessed only on the host simulation thread (I8).
- Every registry translation unit conforms to ADR-007.
- Both Pre-Implementation Evidence Gates (§14) are resolved and recorded.
- Tests pass; performance targets met.
- Ready for Bubble Manager integration.

---

# 13. Next Sprint

Sprint-004 – Bubble Manager

The Bubble Manager builds directly on the Entity Registry, consuming registered
entities (by `EntityHandle`) to determine online/offline transitions based on player
proximity. Activation/deactivation events and semantics are introduced there.

---

# 14. Pre-Implementation Evidence Gates

These are **implementation gates**, not architectural uncertainties. Each selects an
internal mechanism encapsulated behind the frozen contract, and each has a proven
fallback that lies entirely within this architecture. Neither can reopen the
specification. Both must be resolved and recorded before registry code is written.

## Gate 1 — `std::unordered_map` ABI probe
- **Question:** Do the intended associative lookup structures compile C4530-clean under
  `EngineAbi.props` (ADR-007)?
- **Procedure:** Antigravity compiles one throwaway TU under `EngineAbi.props` (identical
  flags to `xrMP.lib`) instantiating `std::unordered_map<EntityId, Record>` and
  `std::unordered_set` with insert / erase / rehash / iterate.
- **Outcomes:** Clean → use `std::`. C4530 → conforming fallback (sorted `std::vector` +
  binary search, or an in-module open-addressing map built from conforming primitives).
- **Invariant preserved:** I3 (canonical order is ascending `EntityId` regardless of the
  lookup structure) and I10.
- **Record:** result logged as a BC-note in `Build_Compatibility.md`.

## Gate 2 — Engine feed mechanism verification
- **Question:** Does the engine expose a spawn/destroy observation hook, or must the feed
  reconcile the engine object list each tick?
- **Constraint:** Zero engine source edits; observation confined to `EngineAdapters.cpp` (I2).
- **Proven fallback:** Sprint-002's `EngineEntitySource` already enumerates the live
  engine object list read-only via `FrameDispatcher` with zero engine edits; tick
  reconciliation (diff → deterministic register/unregister in ascending `EntityId`
  order) is therefore already feasible within the boundary.
- **Outcome:** Adopt an event hook if one exists and is cleaner; otherwise tick
  reconciliation. Either way, zero engine edits.
- **Record:** decision logged as an implementation note in the design review.

If either gate were ever to reveal that no solution exists within the frozen constraints
(e.g. observation impossible without an engine edit), that would escalate to a new ADR —
but the proven fallbacks above foreclose that outcome.

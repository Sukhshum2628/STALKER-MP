# Entity Registry — Usage & Design (Sprint-003, §7.15)

The Entity Registry is the authoritative record of **identity and metadata** for every
persistent entity in the Zone. It is the canonical lookup mechanism that all future
multiplayer systems (Bubble Manager, replication, persistence) build upon.

This document covers registry ownership, the entity lifecycle, handle usage, the public
interfaces, the engine feed, threading, and extension guidelines. It is governed by
ADR-007 (Engine ABI Contract) and the frozen Sprint-003 specification.

---

## 1. Ownership

The registry owns entity **identity and registration records only** (Sprint-003 I1;
`04_World.md §8`). It does **not** own engine object lifetime: the X-Ray engine remains
the mechanical owner of `CObject`/`CSE_Abstract`. The registry records identity and
metadata and mirrors engine spawn/destroy; it never creates or destroys engine objects.

Ownership at runtime: `EntityRegistry` is held by `EntityRegistryService` (a
`core::IService`), which is owned by the `ServiceRegistry` and created at the Bootstrap
composition root before `WorldQueryService` (Design Review §5). No other subsystem may
own an entity's identity, and no service-locator lookups occur outside Bootstrap.

The registry is **engine-agnostic** — it includes no engine headers and stores no engine
pointers (I2, C2). All engine observation lives in the single engine-facing translation
unit `EngineAdapters.cpp` (One Engine Boundary).

---

## 2. Identity vs Handle

Two distinct concepts (Sprint-003 I4):

- **`EntityId`** — the persistent, globally unique, immutable identity. Stable across a
  session and (once persistence lands) across save/load. Value `0` is reserved as "none".
  Engine-native entities use their engine object id; registry-reserved entities use ids
  from a high range (`>= 0x10000`) disjoint from the engine id space.
- **`EntityHandle` = { EntityId, generation }** — a runtime reference. `generation` is
  **session-local** and increments each time an `EntityId` is reused, so a handle to a
  prior occupant is detectably stale. **Generation is never persisted as identity.**

Use `EntityHandle` to refer to entities across frames; validate with `IsValid()` /
`ClassifyHandle()`. A handle never owns the entity.

---

## 3. Entity registration lifecycle (§7.6)

Registration state is a deterministic machine, distinct from the ALife online/offline
state (which is owned by Sprint-004/005, I5):

```
Construct → Register → Initialize → Active → PendingRemoval → Unregister
```

Legal transitions (`SetRegistrationState`):

- `Registered → Initialized`
- `Initialized → Active`
- `{Registered, Initialized, Active} → PendingRemoval`

`RegisterEntity` enters `Registered`; `UnregisterEntity` removes the record outright;
`PendingRemoval` is the marked pre-removal state. Illegal transitions and stale/unknown
handles are rejected via `core::Expected<T>`.

---

## 4. Public interface (summary)

All fallible operations return `core::Expected<T>` (no exceptions, ADR-007).

Registration:
- `RegisterEntity(id, name, metadata) -> Expected<EntityHandle>` — rejects null id and
  live duplicates; assigns the session-local generation.
- `UnregisterEntity(handle) -> Expected<void>` — validates generation.
- `ReserveEntity() -> Expected<EntityId>` — pre-allocates a deterministic id (`>= 0x10000`).
- `RestoreEntity(id, generation, name, metadata) -> Expected<EntityHandle>` — registers a
  pre-assigned identity (format-agnostic; not coupled to the provisional `ISerializable`).

Lookup (deterministic; results in ascending-`EntityId` order):
- `FindByEntityId(id)`, `FindByHandle(handle)` -> `const EntityRecord*` (valid until the
  next mutation).
- `GetRecord(handle) -> Expected<EntityRecord>` (a copy).
- `FindByName(name) -> const EntityRecord*`; `FindByType(category)` /
  `FindBySection(section) -> std::vector<EntityHandle>`.
- `Contains(id)`, `Size()`.

Iteration (canonical ascending-`EntityId` order, I3):
- `ForEach(visitor)`, `ForEachOfType(category, visitor)`, `ForEachActive(visitor)`.

Lifecycle:
- `SetRegistrationState(handle, target)`, `GetRegistrationState(handle)`.

Events (§7.7 — non-fallible, deterministic subscriber-registration order, non-reentrant):
- `SubscribeOnRegistered/Unregistered/Restored(callback) -> Expected<SubscriptionId>`;
  `UnsubscribeOn…(id)`. Handlers **must not** mutate the registry during dispatch.

Statistics (§7.11 — metadata-only, I7):
- `ComputeStatistics() -> RegistryStatistics` (total, per-category counts, dynamic-spawn,
  destroyed). `playerCount` is the count of records whose category is `Player`, **not**
  a count of connections/sessions.

Diagnostics (§7.12 — read-only):
- `InspectEntity(handle)`, `DumpRegistry()`, `ClassifyHandle(handle)`,
  `ValidateIntegrity() -> RegistryIntegrityReport`.

---

## 5. The engine feed

Population from the engine is performed by `EngineEntityFeed` (in `EngineAdapters.cpp`),
ticked once per frame at `tick_order::kEntityRegistry` (after `World`). It observes the
engine but never mutates it, honoring the approved constraints:

- **C1 — single deterministic phase:** all registry mutations happen in `Tick`, in
  ascending-`EntityId` order. The `relcase` destroy callback only queues ids.
- **C2 — no engine pointers:** the feed captures ids (and names) by value; it never
  stores a `CObject*`.
- **C3 — delta-only:** creation reconciliation registers only engine objects not already
  in the registry; unknown-id unregistrations are benign no-ops.
- **C4 — level-bound relcase:** the destroy callback is registered per level, re-registered
  on level reload, unregistered on shutdown while a level exists, and reconciliation
  no-ops while no level is loaded.

Creation is by per-frame reconciliation of `g_pGameLevel->Objects`; destruction is by the
engine's `relcase` notification, queued and applied in the same deterministic phase.

---

## 6. Threading

The registry is **single-writer**: it is accessed only on the host simulation thread
(I8). Cross-thread consumers must use the snapshot architecture (RFC-0003), never direct
registry access. The `relcase` callback runs on the same engine main thread as the feed
`Tick`, so the destroy queue needs no synchronization.

---

## 7. Extension guidelines

- **Refer to entities by `EntityHandle`**, not raw pointers; re-validate long-lived
  handles with `ClassifyHandle`. Pointers from `FindBy…` are valid only until the next
  mutating call.
- **Never mutate the registry from an event handler** (non-reentrancy, I9); defer work to
  the next tick if needed.
- **Keep new registry code engine-free.** Any engine observation belongs in
  `EngineAdapters.cpp` (One Engine Boundary). Engine containers (`xr_hash_map`,
  `robin_hood`) are prohibited outside that TU.
- **Statistics and metadata are identity/category only** — never introduce session,
  connection, or gameplay state into the registry (I7; the WorldContext Player Count Leak
  lesson).
- **All new fallible registry operations return `core::Expected<T>`** and must be
  C4530-clean under `EngineAbi.props` (ADR-007). Secondary hash indices, when added for
  performance (§7.9/§7.13), must not define iteration order (I3).

# Sprint-004 — Bubble Manager — Design Review (FROZEN)

- **Status:** Architecturally Frozen. Governing architectural document for Sprint-004.
- **Date:** 2026-07-06
- **Subordinate only to:** ADR-007 (Engine ABI Contract) and the architecture documents
  (`04_World.md`, `05_ALife.md`, `06_Multiplayer.md`, RFC-0001, RFC-0002).
- **Specification:** `Documentation/SPRINTS/Sprint-004-Bubble-Manager.md` (Status: Planned → to be closed on completion).
- **Author role:** Principal Architect / Architecture Review Board.

This document resolves the three outstanding architectural questions, freezes the
invariants and interface seams, and defines the composition root, evidence gates,
testing strategy, and Definition of Done. No implementation code is written here.

---

## 1. Executive summary

**Objective.** The Bubble Manager computes the **authoritative online/offline activation
set** for the Zone: the union of every connected player's activation region. Entities
inside the union are online; entities outside are offline. ALife receives only the final
activation result and remains unaware that multiple players exist (RFC-0002 §6).

**Responsibilities.** Track player positions (positions only), compute one unified
simulation region (set-union of per-player spheres), classify entities as inside/outside
with hysteresis, expose activation queries and informational events, and produce the
activation set consumed by Sprint-005. Nothing else.

**Non-goals (explicit).** No ALife transition execution (Sprint-005), no networking
(Sprint-006), no player connection/session ownership (Sprint-007), no replication,
persistence, snapshotting, threading changes, world partitioning, per-player instancing,
or any gameplay logic. The Bubble Manager **classifies**; it does not **act**.

**Relationships.**
- **World:** the Bubble Manager belongs to the World subsystem (RFC-0002 §7); World owns
  which entities are active.
- **Entity Registry (Sprint-003):** consumed **read-only** for the entity identity set.
  Never mutated.
- **Spatial queries (Sprint-002 `ISpatialQueries`):** the read-only source of entity
  **positions** and radius membership.
- **ALife:** the Bubble Manager **produces** the activation result ALife will consume in
  Sprint-005; it never calls into ALife behavior.
- **Networking:** none. Player positions arrive through an abstraction, never a network
  or session dependency.

---

## 2. Frozen architectural invariants

Binding for all of Sprint-004.

- **B1 — Deterministic activation.** Given the same player positions, entity positions,
  configuration, and prior activation state, the computed activation set is identical
  (RFC-0002 §8; RFC-0001).
- **B2 — Activation only.** The Bubble Manager decides simulation participation and
  nothing else. No gameplay, AI, physics, or scheduling logic.
- **B3 — No entity ownership.** It references entities by handle/id; it owns no entity and
  no entity lifetime.
- **B4 — Read-only Entity Registry.** It reads the registry's identity set (and handles);
  it never registers, unregisters, or mutates records.
- **B5 — Read-only ALife interaction.** It produces an activation result as an output; it
  never invokes ALife behavior and never executes transitions (Sprint-005 does).
- **B6 — One Engine Boundary.** The Bubble Manager is engine-agnostic — no engine headers,
  no `CObject`/engine pointers. All position/engine access is via `ISpatialQueries`
  (Sprint-002 adapter). `EngineAdapters.cpp` remains the only engine-header TU.
- **B7 — ADR-007 compliance.** Engine-linked; `core::Expected<T>` error model, no
  exceptions, no throwing STL, C4530-clean under `EngineAbi.props`.
- **B8 — Single-writer, host thread.** The Bubble Manager is accessed only on the host
  simulation thread; cross-thread consumers use snapshots (RFC-0003), never direct access.
- **B9 — Deterministic evaluation order.** Players are evaluated in ascending `PlayerId`;
  entities and emitted events in ascending `EntityId` (consistent with Sprint-003 I3).
- **B10 — Hysteresis.** Activation radius < deactivation radius; classification depends on
  the prior activation state so boundary entities do not oscillate.
- **B11 — No player/session state in World.** Player input is **positions only**, via the
  `IPlayerPositionSource` seam. No connection, session, identity-beyond-position, or
  network state ever enters the World subsystem (the WorldContext Player Count Leak lesson,
  `AI_CONTEXT.md`). `ActivePlayerCount()` is a derived count of the injected positions.
- **B12 — Composition-root ownership.** `BubbleManagerService` is owned by the
  `ServiceRegistry`, created at the Bootstrap composition root, and ticked at
  `tick_order::kBubble` (300). No service-locator lookups outside Bootstrap.

---

## 3. Resolution of the three outstanding questions

### A. Player position source — RESOLVED (`IPlayerPositionSource`)

A **positions-only seam** injected into the Bubble Manager, mirroring how `IEntitySource`
decoupled the registry from the engine.

- **Interface (shape):**
  - `struct PlayerPosition { PlayerId id; world::Vec3 position; std::uint64_t lastUpdateTick; };`
  - `class IPlayerPositionSource { virtual std::vector<PlayerPosition> ActivePlayers() const = 0; };`
  - `PlayerId` is a lightweight opaque value (a `std::uint32_t` newtype), **not** a
    connection/session handle. `ActivePlayers()` returns a by-value snapshot in ascending
    `PlayerId` order (B9). It exposes **position only** — no name, no connection, no
    network/session fields (B11).
- **Ownership / lifetime.** Owned by the Bootstrap `Runtime` (like `entitySource`),
  injected by reference; must outlive the Bubble Manager.
- **Sprint-004 implementation.** A local stub — `LocalPlayerPositionSource` — that returns
  the host/local player's position (or empty when none). It carries no networking. This
  lets the Bubble Manager be fully implemented and tested now.
- **Future integration (Sprint-006/007).** The networked player source implements the same
  `IPlayerPositionSource` behind the seam; the Bubble Manager does not change. Player
  connection lifecycle and session state live entirely in the Player subsystem and are
  projected down to *positions* at this seam.
- **Why this shape.** It keeps the World subsystem free of session/network state (B11),
  it is deterministic (ordered snapshot), and it is the minimal surface the bubble math
  needs.

### B. Entity position source — RESOLVED (`ISpatialQueries`, Sprint-002)

The Bubble Manager obtains entity positions through the existing Sprint-002
`ISpatialQueries` seam, **not** from the registry.

- **Seam:** `ISpatialQueries::QueryRadius(center, radius) -> std::vector<EntityView>`
  (EntityView carries `id` and `position`), with `PositionOf(id)` for point checks.
- **Why this is authoritative:**
  - Sprint-003 deliberately stores **identity/metadata only** in the registry — position is
    transient simulation state the registry does not hold. Reading position from the
    registry is therefore impossible by design.
  - Sprint-002 already assigned **position and radius membership** to `ISpatialQueries`
    (currently a linear scan over `IEntitySource`, with spatial partitioning deferred and
    measured-first). This is exactly the bubble-membership primitive.
  - It preserves One Engine Boundary (B6): live position comes from the engine through the
    Sprint-002 adapter behind `ISpatialQueries`; the Bubble Manager stays engine-free.
- **Division of labor:** the **registry** supplies the identity/handle for an entity id;
  **`ISpatialQueries`** supplies its position and per-player radius membership. The Bubble
  Manager composes the two: `QueryRadius` per player yields candidate `EntityView`s (id +
  position), which map to registry handles for the activation result.

### C. Deterministic activation algorithm — FROZEN

**Model: union of per-player spheres with state-dependent hysteresis.** There is no
explicit geometric "merge"; the unified region is the set-union of per-player membership,
which handles overlapping and separated bubbles uniformly.

- **Configuration (frozen fields, §7.5):** `simulationRadius`, `activationMargin`,
  `deactivationMargin` (with `deactivationMargin >= activationMargin`, enforced at config
  parse), `maximumRadius` (clamp), `debugFlags`.
  - `R_on  = min(simulationRadius + activationMargin,   maximumRadius)`
  - `R_off = min(simulationRadius + deactivationMargin, maximumRadius)`, and `R_off >= R_on`.
- **Per-tick computation (deterministic):**
  1. Snapshot players via `IPlayerPositionSource::ActivePlayers()` (ascending `PlayerId`).
  2. Gather candidate entities: for each player, `QueryRadius(player.position, R_off)`;
     union the results into a set keyed by `EntityId` (dedup across overlapping spheres).
  3. For each candidate (ascending `EntityId`), compute `d = min over players of
     distance(entity.position, player.position)` using squared distance (no `sqrt`;
     compare against `R^2`) for determinism and cost.
  4. **Classify with hysteresis against the prior activation set:**
     - previously **offline** and `d <= R_on`  → **online** (entering).
     - previously **online**  and `d >  R_off` → **offline** (exiting).
     - otherwise → unchanged.
  5. An entity previously online that is **absent** from all `R_off` candidate sets is
     necessarily beyond `R_off` → **offline** (exiting).
- **Evaluation order (B9):** players ascending `PlayerId`; entity classification and event
  emission ascending `EntityId`. Distance uses squared magnitude (`Vec3::DistanceSquared`
  already exists) so results are exact and order-independent.
- **Merge / overlap / separation:** union semantics — an entity online iff inside any
  player's `R_on` (entering) or still within some player's `R_off` (staying). Overlap is
  deduped by `EntityId`; separated player groups simply contribute disjoint members to the
  one activation set. No bubble geometry is materialized.
- **Oscillation prevention:** the hysteresis band (`R_off > R_on`) plus classification
  against the retained prior activation set. An entity must cross fully inside `R_on` to
  turn on and fully outside `R_off` to turn off, so a stationary entity near the boundary
  never flips.
- **Output:** the Bubble Manager holds the current activation set and exposes it via
  queries and informational events (`OnEntityEnteredBubble`/`ExitedBubble`,
  `OnBubbleUpdated`). It does **not** transition entities (B5) — Sprint-005 consumes this
  set.

---

## 4. Architecture decisions

**Accepted**
- Positions-only `IPlayerPositionSource` seam; local stub for Sprint-004 (A).
- `ISpatialQueries` as the entity-position/membership source; registry stays identity-only (B).
- Union-of-spheres membership with squared-distance math and prior-state hysteresis (C).
- Bubble Manager is a World-subsystem service, engine-agnostic, ticked at `kBubble` (300).
- Activation result is an **output**; ALife transition execution is Sprint-005.

**Rejected**
- **Independent per-player bubbles / player-owned regions** (RFC-0002 §9): would split the
  ALife timeline and leak ownership to players. Rejected — unified region only.
- **Networking-owned or ALife-owned activation** (RFC-0002 §9): violates subsystem
  ownership; the World owns activation.
- **Storing entity position in the registry to serve the bubble:** violates Sprint-003's
  identity-only decision and duplicates Sprint-002's spatial responsibility.
- **Geometric bubble merging (materialized merged shapes):** unnecessary; set-union is
  simpler, deterministic, and equivalent for membership.
- **Real player/session objects in Sprint-004:** out of scope (Sprint-006/007); the seam
  defers them without blocking.

**Deferred**
- Spatial acceleration for `ISpatialQueries` (still linear scan from Sprint-002) — measured
  first (Evidence Gate B-G1). The seam does not change if an index is later added.
- Debug rendering (`§7.10`) — hooks only; rendering deferred.
- Incremental / cached bubble computation — considered only if profiling requires it.

---

## 5. Composition root

- **`BubbleManagerService`** — `core::IService` + `core::ITickable`, owns `BubbleManager`.
  - `Name()` = "BubbleManager"; `Dependencies()` = `{ "World", "EntityRegistry" }`.
  - `Tick(dt)` runs the per-tick computation (§3C).
- **`BubbleConfiguration`** — value type parsed from the `[bubble]` section of `server.cfg`
  (`FromConfig`), following the `WorldConfiguration` convention; validates
  `deactivationMargin >= activationMargin` and positive radii (else `InvalidArgument`).
- **Injected interfaces (constructor injection):** `world::IWorldQueries`/`ISpatialQueries`
  (entity positions), `world::EntityRegistry&` (identity/handles, read-only),
  `adapters::IPlayerPositionSource&` (player positions).
- **Bootstrap wiring:** in `RegisterWorldServices`, after the Entity Registry: create the
  `LocalPlayerPositionSource` (owned by `Runtime`), construct `BubbleManager` with the
  registry + spatial queries + player source, register `BubbleManagerService`, and
  `frameDispatcher.Subscribe(service, tick_order::kBubble, "BubbleManager")`.
- **Update order:** `kWorld` (100) → `kEntityRegistry` (200) → `kBubble` (300). The bubble
  ticks after the registry is reconciled and the world context is current for the frame.
- **Teardown:** unsubscribe + destroy the service before the registry/spatial services it
  references (reverse order), mirroring the Sprint-003 feed teardown rule.

---

## 6. Evidence gates

**Gate B-G1 — spatial query cost at target scale (performance; non-blocking for correctness).**
- **Question:** Is the Sprint-002 linear-scan `ISpatialQueries` acceptable for
  `players × entities` membership each tick at the target Zone population, within the frame
  budget?
- **Procedure:** benchmark `QueryRadius` union cost at representative counts (e.g. up to a
  few thousand entities × several players) on the host build.
- **Outcomes:** within budget → proceed unchanged. Over budget → introduce the spatial
  index **behind the existing `ISpatialQueries` seam** (already deferred since Sprint-002);
  the Bubble Manager is unaffected.
- **Why it cannot invalidate the architecture:** correctness does not depend on it, and the
  fallback lives behind an existing seam. Recorded as a BC/implementation note when run.

No other architectural uncertainty exists: the player seam, the position seam, and the
hysteresis math are fully determined above.

---

## 7. Testing strategy

Engine-free unit tests in `xrMP_Tests` using a fake `IPlayerPositionSource` and a fake
`ISpatialQueries` (the `NullEngineAdapters` pattern), so the Bubble Manager is tested
without the engine.

- **Unit / correctness:** single-player membership; two-player overlap (dedup); multiple
  players; separated player groups (disjoint union); entity exactly on `R_on`/`R_off`
  boundary; zero players → empty activation set; player "disconnect" (removed from source).
- **Determinism:** identical inputs + prior state → identical activation set and event
  order; player/entity evaluation order is ascending id.
- **Hysteresis / no-oscillation:** an entity parked in the `[R_on, R_off]` band across many
  ticks never flips state; crossing fully inside/outside flips exactly once.
- **Stress:** hundreds–thousands of entities × several players; stable per-tick behavior;
  activation-set churn bounded.
- **Edge cases:** empty world; all entities inside; all outside; `maximumRadius` clamp;
  config with `deactivationMargin < activationMargin` rejected at parse.
- **Runtime verification (Antigravity):** clean MSVC Release under `EngineAbi.props`
  (no C4530/C2220, exceptions off, ADR-007), composition-root wiring at `kBubble`, and a
  live check that activation tracks the local player.

---

## 8. Definition of Done

Sprint-004 is complete when:

- All invariants **B1–B12** hold.
- The Bubble Manager computes a **deterministic** activation set as the union of per-player
  spheres with hysteresis; no oscillation at boundaries.
- Player positions enter only through `IPlayerPositionSource` (positions-only, B11);
  entity positions come only through `ISpatialQueries` (B6); the Entity Registry is
  read-only (B4); no ALife transition execution (B5).
- No gameplay logic exists in the Bubble Manager (B2); it owns no entities (B3).
- `BubbleManagerService` + `BubbleConfiguration` are wired at the composition root and
  ticked at `tick_order::kBubble`, with correct teardown ordering (B12).
- Every Bubble Manager translation unit conforms to ADR-007 (B7).
- Unit, determinism, hysteresis, stress, and edge-case tests pass; Evidence Gate **B-G1**
  is resolved and recorded.
- §7.14 documentation is written (bubble ownership, merge/union algorithm, world
  interaction, entity evaluation, configuration).
- Ready for Sprint-005 (Online/Offline Entity Transition), which consumes this activation
  set.

---

## 9. Gate outcome

**Architecturally Frozen.** The three outstanding questions are resolved (A: positions-only
`IPlayerPositionSource`; B: `ISpatialQueries` for positions; C: union-of-spheres with
hysteresis), the invariants B1–B12 are frozen, and the single evidence gate (B-G1) is a
non-blocking performance measurement behind an existing seam. Implementation may begin once
this review is accepted; no further design iteration is required.

---

## 10. Evidence-gate resolution (implementation note)

### Gate B-G1 — spatial query cost at target scale — DEFERRED (non-blocking)
Status at Sprint-004 close: **not benchmarked; deferred.** The implementation uses the
Sprint-002 linear-scan `ISpatialQueries` unchanged. No performance benchmark at target Zone
population was run, so no measured need for spatial acceleration exists yet. This is
consistent with the gate's design: it is a **performance** measurement, non-blocking for
correctness, and its fallback (a spatial index) lives entirely **behind the existing
`ISpatialQueries` seam** and would not change the Bubble Manager. It is recorded here as an
honest open item rather than claimed as resolved with data that does not exist; it should be
measured when a representative Zone workload is available (or alongside a later
performance-focused sprint). Correctness, determinism, and hysteresis are fully verified by
the Sprint-004 unit/stress tests independent of this gate.

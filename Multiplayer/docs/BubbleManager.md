# Bubble Manager — Usage & Design (Sprint-004, §7.14)

The Bubble Manager computes the **authoritative online/offline activation set** for the
Zone: the union of every connected player's activation region. Entities inside the union
are online; entities outside are offline. It determines *simulation participation only* —
no gameplay, no ALife, no networking. It is governed by ADR-007 (Engine ABI Contract), the
frozen Sprint-004 Design Review, and RFC-0002.

This document covers ownership, the two read-only seams, the activation algorithm and
hysteresis, the public interface, the runtime update flow, transition generation,
diagnostics, threading, and extension guidelines.

---

## 1. Ownership and boundaries

The Bubble Manager belongs to the **World subsystem** (RFC-0002 §7). World decides which
entities are active; ALife decides how active entities behave; networking only
synchronizes. The manager:

- **owns** its configuration, the current/previous online sets, the per-tick `BubbleState`,
  and the activation/deactivation transition lists;
- **consumes read-only** the entity positions (`ISpatialQueries`, Sprint-002) and the
  player positions (`IPlayerPositionSource`, Sprint-004);
- **owns no entities** and never mutates the Entity Registry (identity/handles are keyed by
  `EntityId` value; invariant B3/B4);
- is **engine-agnostic** — no engine headers, no engine pointers (B6). `EngineAdapters.cpp`
  remains the only engine-header translation unit.

At runtime the manager is owned by `BubbleManagerService` (a `core::IService` +
`core::ITickable`), created at the Bootstrap composition root, owned by the
`ServiceRegistry`, and ticked at `tick_order::kBubble` (300), after World (100) and the
Entity Registry feed (200).

---

## 2. The two read-only seams

- **Player positions — `IPlayerPositionSource`.** A positions-only seam
  (`ActivePlayers() -> std::vector<PlayerPosition>` in ascending `PlayerId` order). It
  carries **no** connection, session, or network state (invariant B11 — the WorldContext
  Player Count Leak lesson). Sprint-004 ships `LocalPlayerPositionSource`, an engine-free
  in-memory stub with a host-side feed (`SetPlayer`/`RemovePlayer`/`Clear`). The real
  networked source (Sprint-006/007) implements the same interface; the manager is unchanged.
- **Entity positions — `ISpatialQueries` (Sprint-002, reused unchanged).** Supplies entity
  positions and radius membership (`QueryRadius(center, radius)`, `PositionOf(id)`). The
  Entity Registry deliberately stores identity/metadata only (no position), so position is
  sourced here — which also keeps live-position access behind One Engine Boundary. At
  runtime this is `WorldQueryService`, which implements `ISpatialQueries`.

---

## 3. Activation algorithm and hysteresis

**Model: union of per-player spheres, with state-dependent hysteresis. No merged geometry
is materialized.** Each `Update(currentTick)`:

1. Snapshot players via `IPlayerPositionSource::ActivePlayers()` (ascending `PlayerId`).
2. Candidate entities = the union of `QueryRadius(player.position, R_off)` over all players,
   deduped by `EntityId`. Empty when there is no player/spatial source.
3. Evaluate candidates in **ascending `EntityId`** order (B9). For each, compute the
   minimum **squared** distance to any player (no `sqrt`; comparisons against `R²`).
4. Apply hysteresis against the prior online set (B10):
   - previously **offline** and `d² ≤ R_on²` → **online** (entering);
   - previously **online** and within `R_off` (guaranteed for a candidate) → **stays online**;
   - previously **online** and absent from all `R_off` candidate sets → **offline** (exiting);
   - offline within the `[R_on, R_off]` band → **stays offline**.

Radii (from `BubbleConfiguration`, frozen §3C): `R_on = min(simulationRadius +
activationMargin, maximumRadius)`, `R_off = min(simulationRadius + deactivationMargin,
maximumRadius)`, with `deactivationMargin ≥ activationMargin` enforced at config parse so
`R_on ≤ R_off`. The hysteresis band (`R_off > R_on`) plus classification against the prior
set prevents boundary entities from oscillating.

Overlapping player spheres dedup by `EntityId` (an entity in several spheres is counted
once); separated player groups contribute disjoint members to one online set.

Given identical player positions, entity positions, configuration, and prior state, the
computed set is identical (B1).

---

## 4. Public interface (summary)

Construction: `BubbleManager(BubbleConfiguration, const ISpatialQueries*, const IPlayerPositionSource*)`
(both seams non-owning, must outlive the manager).

Computation (host thread only, B8): `void Update(std::uint64_t currentTick)` — sole writer
of the activation state (B10).

Queries (read-only): `IsEntityInside(EntityId)`, `IsPositionInside(Vec3)`,
`NearestBubble(Vec3)`, `ActiveBubbleRadius()`, `ActivePlayerCount()`, `OnlineEntityCount()`,
`Configuration()`, `State()`, and `MembershipOf(EntityId)` →
`{ Outside, Inside, PendingActivation, PendingDeactivation }`.

Transitions (from the last `Update`, ascending `EntityId`): `Activations()`,
`Deactivations()` → `const std::vector<EntityId>&`.

Configuration: `BubbleConfiguration::FromConfig(ConfigStore)` parses the `[bubble]` section
(`simulation_radius`, `activation_margin`, `deactivation_margin`, `maximum_radius`,
`debug_flags`), keeps defaults for missing keys, and rejects non-positive radii, negative
margins, or `deactivation_margin < activation_margin`.

All fallible operations return `core::Expected<T>`; no exceptions (ADR-007).

---

## 5. Runtime update flow

```
FrameDispatcher (single Device.seqFrame bridge)
        │  tick_order::kWorld (100)      -> WorldManager
        │  tick_order::kEntityRegistry (200) -> EngineEntityFeed
        ▼  tick_order::kBubble (300)
BubbleManagerService::Tick(dt)
        └─> BubbleManager::Update(++tick)   // monotonic tick counter
              ├─ snapshot players (IPlayerPositionSource)
              ├─ union of QueryRadius(R_off) (ISpatialQueries)
              ├─ classify with hysteresis (prior online set)
              ├─ maintain current/previous online sets + BubbleState
              └─ generate ascending activation/deactivation transitions
```

`BubbleManagerService` subscribes to the one existing `FrameDispatcher` — no new
`seqFrame` object is created (Design Review §5). In `DestroyRuntime` it is unsubscribed
after the frame bridge is reset and before `ServiceRegistry` shutdown (no dangling
dispatcher reference).

---

## 6. Transition generation

After each `Update`, transitions are derived strictly from the two sorted sets:
`Activations() = m_online − m_previousOnline`, `Deactivations() = m_previousOnline −
m_online` (via `std::set_difference`), in ascending `EntityId` order (B9). They are
computed and stored only — the Bubble Manager never notifies, dispatches, or invokes any
subsystem. Sprint-005 consumes these lists to execute online/offline transitions.
`MembershipOf` is the per-entity view of the same information.

---

## 7. Diagnostics (read-only)

All const, deterministic, engine-free, and safe to call at runtime; human-readable output
uses `common::Format` (no iostream):

- `ActivePlayers()` — snapshot of contributing players (ascending `PlayerId`).
- `OnlineEntities()` — current online set as `EntityId`s (ascending).
- `DescribeState()` — one-line summary (tick, player/online counts, transition counts, radii).
- `DumpOnline()` / `DumpTransitions()` — human-readable dumps.
- `ValidateConsistency()` → `BubbleConsistencyReport` — verifies the online/previous sets are
  strictly ascending and unique, `R_on ≤ R_off`, and that the stored transition lists equal
  the set differences of the two sets.

---

## 8. Threading

The Bubble Manager is **single-writer**: `Update` runs only on the host simulation thread
(B8). Cross-thread consumers must use the snapshot architecture (RFC-0003), never direct
access.

---

## 9. Extension guidelines

- **Keep new bubble code engine-free** (B6); any live-position access goes through
  `ISpatialQueries`, and player input through `IPlayerPositionSource`.
- **Player input is positions only** — never introduce session, connection, or network
  state into the World subsystem (B11).
- **The manager classifies; it does not act** — no ALife transition execution, no gameplay,
  no events dispatch (B2/B5). Sprint-005 consumes the activation set and transition lists.
- **Preserve determinism** — evaluate players ascending `PlayerId`, entities ascending
  `EntityId`; use squared distances; keep online sets sorted.
- **All fallible operations return `core::Expected<T>`** and must be C4530-clean under
  `EngineAbi.props` (ADR-007). Spatial acceleration, if added, lives behind the existing
  `ISpatialQueries` seam and must not change the Bubble Manager (Evidence Gate B-G1).

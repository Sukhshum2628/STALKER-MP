# Player Lifecycle (Sprint-007) — Subsystem Documentation

- **Status:** Implemented & Verified — 2026-07-11 (Sprint-007 closure is Step 15).
- **Architecture:** `Multiplayer/docs/Sprint-007-Architecture.md` (FROZEN 2026-07-11). This document records the delivered subsystem; it introduces no new decisions.
- **Governing ADRs:** ADR-007 (Engine ABI Contract), ADR-008 (Engine State Mutation Boundary — preserved; player online/offline is reused, not re-implemented), ADR-009 (One Platform Boundary — untouched), ADR-010 (Wire Protocol Contract — additive ids only), ADR-011 (Single-Threaded / Deterministic).

---

## 1. Purpose

The Player Lifecycle subsystem turns a Sprint-006 network connection into a **persistent, host-authoritative player** in the one authoritative simulation. It owns the deterministic state machine and record store that drives a player through join → spawn → death → respawn → disconnect (suspend, not destroy) → reconnect (reclaim, never duplicate). It reuses — and does not reimplement — connection/session (Sprint-006), entity materialization (via the engine seam), activation (Sprint-004 Bubble), and ALife online/offline switching (Sprint-005). Clients issue *requests*; the host owns every state transition (Host Authority).

## 2. Ownership boundaries

| Data | Owner |
|---|---|
| Transport / socket / endpoint | Sprint-006 `PlatformSockets.cpp` / `TransportEndpoint` (unchanged) |
| Connection record + connection lifecycle | Sprint-006 `net::ConnectionTable` (unchanged) |
| Session membership + reconnect token | Sprint-006 `net::Session` (unchanged) |
| **Player identity, lifecycle state, and the id↔session↔entity mapping** | **`player::PlayerRegistry` (Sprint-007)** |
| Persistent entity identity / registration | Engine ALife via `IPlayerSpawnGateway` (materialized behind the seam) |
| Entity online/offline (ALife) status | Engine ALife via the Sprint-005 Bubble→Transition pipeline (unchanged) |
| Activation decision (who is online) | Sprint-004 `world::BubbleManager` (unchanged) |
| Player position feed the Bubble reads | `player::NetworkedPlayerPositionSource` (Sprint-007), positions only |

The subsystem owns only the mapping + lifecycle state; it never owns the entity's simulation state, session mechanics, or activation logic. Clients own **no** persistent world state.

## 3. Components

- `player::PlayerTypes.h` — value types/enums: `PlayerId` (reuses `world::PlayerId`), `PlayerLifecycleState` (Joining / Active / Dead / AwaitingRespawn / Suspended / Removed), `PlayerConnectionState`, `DisconnectDisposition`, `JoinOutcome` / `SpawnOutcome` / `ReconnectOutcome`, `PlayerRecord`, `PlayerMappingView`, `PlayerStatistics`, with `Name()` helpers. Engine-free.
- `player::PlayerConfiguration` (+ `FromConfig`) — capacity, respawn delay (ticks), reconnect-retention window (ticks), spawn-policy id. Durations are tick counts.
- `player::PlayerRegistry` — canonical record store: sorted `std::vector` keyed ascending by `PlayerId` + hash accelerators (BC-005) for the secondary keys; ascending non-reused `PlayerId`; `ValidateConsistency`.
- `player::PlayerLifecycle` — pure, stateless transition rules (`ApplyJoin`/`ApplyDeath`/`ApplyRespawn`/`ApplySuspend`/`ApplyReclaim`/`ApplyRemove`, `ComputeRespawnEligibleTick`); illegal transitions are value outcomes.
- `player::PlayerDeltaQueue` + `player::PlayerSessionObserver` — the enqueue-only `net::ISessionObserver` bridge; session join/leave events are recorded into an ordered queue, mutating nothing during the networking tick.
- `player::PlayerManager` — the engine-free core: drains the queue and applies join/suspend/reclaim/death/respawn transactionally against the registry, requesting entity materialization/removal through the abstract `IPlayerSpawnGateway`; exposes the read-only surface (`Find*`, `Statistics`, `ValidateConsistency`, `ActivePlayerPositions`, `Registry`).
- `player::NetworkedPlayerPositionSource` — implements the frozen Sprint-004 `world::IPlayerPositionSource` (positions only, ascending `PlayerId`); the Bubble binds this in place of the local stub.
- `player::IPlayerSpawnGateway` + `adapters::EnginePlayerSpawnGateway` (`EngineAdapters.cpp`) + `NullPlayerSpawnGateway` (test build) + `adapters::CreatePlayerSpawnGateway()` — the single engine seam that materializes/removes the vanilla ALife player object.
- `player::PlayerManagerService` — the `IService` + `ITickable` umbrella; ticked at `tick_order::kPlayerLifecycle = 250`; owns the observer + the two request handlers; on `Initialize` subscribes the observer to the Session and registers the additive player-request message ids.
- `player::PlayerDiagnostics` — read-only `Statistics` / `DescribeState` / `DescribePlayer` / `DumpPlayers` / `ValidateConsistency` (+ position-feed agreement).
- `player::PlayerMessageIds.h` — `kMsgPlayerJoinRequest = 0x0100`, `kMsgPlayerRespawnRequest = 0x0101` (additive DATA-range).

## 4. Per-tick pass and tick placement

`PlayerManagerService` ticks once per frame at `tick_order::kPlayerLifecycle = 250`, **strictly between EntityRegistry (200) and Bubble (300)**: EntityRegistry (200) → **Player (250)** → Bubble (300) → ALifeTransition (350) → … → Networking (900).

Session join/leave events fire during the networking tick (900); the enqueue-only observer records them into `PlayerDeltaQueue` and mutates nothing. On the following frame at 250, `PlayerManagerService::Tick` drains the queue and applies the deltas: a joined connection becomes a player (materialized via the gateway, registered, positioned); a left connection is suspended (retained). Because the player is applied at 250, it is visible to Bubble activation the same frame; the 900→250 ingress latency is one deterministic frame. All mutation happens at 250 — never in the observer.

## 5. Lifecycle and reconnect

Join is transactional: allocate `PlayerId` → materialize the entity via `IPlayerSpawnGateway::Spawn` → on success insert the record (Active); on spawn failure nothing is inserted (no orphan). Death transitions Active→Dead and schedules a tick-derived respawn; it never destroys the entity. Respawn re-activates an eligible dead player (the entity persists); a too-early respawn is rejected and leaves state unchanged. Disconnect **suspends** (retains) the record and clears the connection binding — the entity is not destroyed. Reconnect is routed through the Sprint-006 token seam (`net::Session::TryReclaim`); on success it rebinds a new connection to the **same** `PlayerId`/`EntityId` (never a duplicate). *Sprint-006's `TryReclaim` is a reserved stub returning `NotFound` until Sprint-012, so a reconnect attempt currently yields `TokenUnknown` while the suspended record — and its entity — is preserved; the rebind path is wired and activates once the seam is enabled.*

## 6. Engine boundary (One Engine Boundary, ADR-008)

The **only** engine-touching code is `adapters::EnginePlayerSpawnGateway` in `EngineAdapters.cpp`, reached through the engine-free `IPlayerSpawnGateway`. It materializes a vanilla ALife player object (`alife().spawn_item`) and removes it (`release`), resolving `EntityId ↔ object` on demand with no retained pointer/cache — mirroring the Sprint-005 discipline. It performs **no** online/offline switching: that remains the Sprint-005 Bubble→Transition pipeline (ADR-008 unbroken). The test build links `NullPlayerSpawnGateway` and never compiles the engine TU.

## 7. Platform & networking boundaries

No OS/socket header enters the subsystem — One Platform Boundary (ADR-009) is untouched. Networking is consumed via the Sprint-006 control-plane seams only (`net::ISessionObserver`, `net::Session::TryReclaim`, and the `net::MessageRegistry` additive-id extension point for `0x0100`/`0x0101`). No transport, wire-header, or endianness code is added (ADR-010: additive ids only). Networking remains last in the frame (900) and owns no simulation.

## 8. Composition-root integration

The player spawn gateway, `PlayerManager`, its `PlayerDeltaQueue`, and the `NetworkedPlayerPositionSource` are Runtime-owned (constructed before the Bubble, which binds the networked source at construction). `PlayerManagerService` is registered after the Sprint-006 services — so registration-order `InitializeAll` initializes it after World, EntityRegistry, BubbleManager, TransitionManager, and Networking — and subscribed to the single `FrameDispatcher` at 250. `Initialize` subscribes the observer to the Session and registers `0x0100`/`0x0101` (a duplicate registration fails `Initialize`). Teardown is reverse-order: the service is frame-unsubscribed after the Bubble and before `ShutdownAll`; networking is unsubscribed first so no session callback reaches the observer during teardown. The frame subscriber set is `World (100) → EntityRegistry (200) → PlayerLifecycle (250) → Bubble (300) → TransitionManager (350) → Networking (900)` — six subscribers.

## 9. Concurrency & determinism (ADR-011)

Single-threaded: the subsystem runs on the one `FrameDispatcher` tick at 250 and on synchronous enqueue-only observer callbacks. All control flow is tick-derived (join tick, respawn eligibility, session-duration in ticks) with no wall-clock; `PlayerId` allocation is ascending and non-reused; queue drain is fixed-order (ascending `ConnectionId`). Consequently an identical (session-delta + tick) sequence replays to identical registry state, entity registrations, and position feed — verified by the integration and churn replay tests.

## 10. Diagnostics

`PlayerDiagnostics` is read-only and deterministic: `Statistics()` (connected/suspended/dead + cumulative respawns/deaths/reconnects + last join tick; `averageSessionDurationTicks` reserved), `DescribeState()`, `DescribePlayer(PlayerId)`, `DumpPlayers()` (ascending), and `ValidateConsistency()` (delegates the registry checks — ordering, accelerators, bijection — and extends with position-feed agreement). All facts are tick-derived, so diagnostics are replay-stable.

## 11. Extension points (Sprints 008–012)

Reserved, not implemented: the `MessageRegistry` additive-id space above `0x0101` for future player requests; the deliberately reconstructable `PlayerRecord` for a future durable-persistence serialization (Sprints 011–012); the positions/state feed as the Sprint-008 replication source; `IPlayerSpawnGateway` as the single seam for any future engine-side player representation; the reconnect path that activates once Sprint-012 enables `net::Session::TryReclaim`; `PlayerDiagnostics` reserved fields. No future producer `tick_order` value is assigned (Sprint-007 added only `250`), and no non-goal (replication, snapshot, prediction, persistence, chat/voice/trade) is implemented.

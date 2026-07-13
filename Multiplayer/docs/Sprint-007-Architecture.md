# Sprint-007 — Player Lifecycle — Architecture

- **Status:** Architecture (frozen — see §29). Architecture phase only; no implementation, no step specifications, no code, no verification content.
- **Governance:** Conforms to ADR-007, ADR-008, ADR-009, ADR-010, ADR-011 (none superseded; none modified here). Governed by the project hierarchy ADR → Architecture → Sprint Architecture → Implementation Plan → Step Specs → Implementation → Independent Verification.
- **Sprint definition:** `Documentation/SPRINTS/Sprint-007-Player-Connections.md` (scope authority). This document is the frozen *architecture* for that sprint; it introduces no decision that contradicts an Accepted ADR or a previously frozen sprint architecture.
- **Consumes (frozen, unchanged):** Sprint-002 World / `FrameDispatcher` / `IPlayerPositionSource` seam, Sprint-003 Entity Registry, Sprint-004 Bubble Manager, Sprint-005 ALife Transition Layer (`IAlifeSwitchGateway`), Sprint-006 Host Networking (`Session`, `ISessionObserver`, `ConnectionTable`, `MessageRegistry`).

---

## 1. Sprint objective

Introduce the **persistent player** as a first-class, host-authoritative participant in the one authoritative simulation. Sprint-007 delivers the player-lifecycle subsystem: a deterministic state machine and record store that turns a Sprint-006 network connection into a persistent World entity and drives it through join → spawn → (online via the existing Bubble→Transition pipeline) → death → respawn → disconnect (suspend, not destroy) → reconnect (reclaim, never duplicate). The subsystem owns player *identity and lifecycle state*; it reuses — and does not reimplement — connection/session (Sprint-006), entity registration (Sprint-003), activation (Sprint-004), and online/offline switching (Sprint-005).

## 2. Problem statement

After Sprint-006 the host can accept connections, run a handshake, maintain session membership, and frame packets — but a connection is an ephemeral network object with no representation in the simulation. There is no notion of a player that (a) exists as a World entity the Entity Registry owns, (b) survives a disconnect so a returning user resumes the same character, (c) has a deterministic, replay-safe lifecycle, and (d) never spawns a duplicate entity across reconnects or concurrent joins. The prior sprints deliberately reserved the seams for this (`ISessionObserver` join/leave, `Session::TryReclaim` reconnect tokens, the positions-only `IPlayerPositionSource` the Bubble reads) but left the lifecycle itself unbuilt. Sprint-007 fills exactly that gap without reopening any frozen boundary.

## 3. Scope

In scope: the `player::` subsystem — `PlayerManager`, `PlayerRegistry`, `PlayerRecord` (identity + lifecycle + session/entity mapping), `PlayerConfiguration`; the join, spawn, disconnect (suspend), reconnect (reclaim), death, and respawn pipelines as a deterministic host-authoritative state machine; integration with the Session/observer seam (Sprint-006), the Entity Registry (Sprint-003), the Bubble position feed (`IPlayerPositionSource`, Sprint-004) via a networked implementation the subsystem feeds, and the ALife online/offline pipeline (Sprint-005) *by reuse only*; a single new engine-free seam `IPlayerSpawnGateway` for materializing/removing the persistent ALife player object (concrete impl only in `EngineAdapters.cpp`, null in tests); read-only player diagnostics/statistics; additive player control-message ids registered through the existing `MessageRegistry`; unit-testable engine-free core.

## 4. Explicit out-of-scope items

Not in this sprint (and not depended upon): snapshot replication, delta compression, client-side prediction/interpolation (Sprints 008–010); **durable/disk persistence and any save-game format** (Sprints 011–012) — Sprint-007 persistence is in-memory retention only; inventory synchronization, trading, co-op mechanics, chat, and voice; any new transport, wire-header, or endianness work (ADR-010 is closed — Sprint-007 only *registers additive message ids*, it does not touch framing); any new engine-state *mutation* path for online/offline (owned by Sprint-005/ADR-008 — reused, never duplicated); any IO thread or multi-threading (ADR-011); assignment of, or dependency on, any future producer `tick_order` value; movement physics/authority arbitration beyond recording host-owned player positions.

## 5. Functional requirements

FR-1 A connection admitted to the `Session` (Sprint-006) results in exactly one `PlayerRecord` with a unique, ascending, non-reused `PlayerId`. FR-2 Join is a deterministic ordered pipeline: validate → allocate player → materialize/register the persistent entity → assign session mapping → publish position → notify. FR-3 The player entity is registered in the Entity Registry and owned by the World; the player subsystem holds only the *mapping and lifecycle state*, never the entity's simulation state. FR-4 Bringing a player **online/offline is performed solely by the existing Bubble→Transition pipeline**; the player subsystem makes the player eligible (registered + positioned), it does not switch ALife. FR-5 Disconnect **suspends** the player (retains the record and the entity); it never destroys player persistence. FR-6 Reconnect reclaims the retained player via the Sprint-006 reconnect token (`Session::TryReclaim`) and rebinds a new `ConnectionId` to the same `PlayerId`/`EntityId` — **never** creating a second entity. FR-7 Death transitions lifecycle state and schedules respawn without automatic entity destruction. FR-8 Respawn selects/validates a location and republishes the player so the existing activation pipeline reactivates it. FR-9 The registry answers `FindByPlayerId`, `FindBySession`, `FindByEntity`, `FindByConnection`. FR-10 The subsystem feeds an `IPlayerPositionSource` implementation with positions only (ascending `PlayerId`), leaving the Bubble Manager unchanged. FR-11 Validation detects and rejects duplicate players/sessions/entities, invalid reconnects, invalid spawn requests, and ownership violations, as value outcomes (never exceptions). FR-12 Client messages are *requests*; every state transition is decided by the host (Host Authority).

## 6. Non-functional requirements

NFR-1 Deterministic and replay-safe: identical (session-delta + tick) input sequences produce identical registry state, entity registrations, and position feed. NFR-2 Engine-free core: everything except the single `IPlayerSpawnGateway` concrete adapter compiles and is tested without engine headers (One Engine Boundary). NFR-3 Exception-free, RTTI-free, `/W4 /WX`, C4530-clean (ADR-007); all fallible operations return `core::Expected<T>` / value outcomes. NFR-4 Single-threaded; ticked once on the existing `FrameDispatcher` (ADR-011). NFR-5 No wall-clock in control flow — all timing is tick-derived (join tick, session-duration in ticks). NFR-6 Bounded and predictable: player capacity is configured; lookups are sorted-vector + hash-accelerator (BC-005), ascending by id. NFR-7 No platform/OS header enters the subsystem (One Platform Boundary is Sprint-006's and remains untouched). NFR-8 Additive-only integration: no frozen interface of Sprints 002–006 is modified.

## 7. System responsibilities

The subsystem is responsible for: allocating and retiring `PlayerId`s; holding the authoritative `PlayerRecord` (identity, character profile handle, spawn position, lifecycle state, connection state, and the `PlayerId↔ConnectionId↔SessionMember↔EntityId` mapping); draining queued session join/leave deltas and applying the lifecycle state machine deterministically at its tick slot; requesting entity materialization/removal through `IPlayerSpawnGateway`; registering/unregistering the entity with the Entity Registry; publishing player positions to the Bubble position feed; validating every transition and enforcing ownership invariants; exposing read-only diagnostics/statistics. It is **not** responsible for: transport, framing, or session admission mechanics (Sprint-006); entity identity storage internals (Sprint-003); activation decisions (Sprint-004); ALife online/offline switching (Sprint-005); any persistence to disk (Sprints 011–012).

## 8. High-level architecture

The subsystem is a new engine-free module `player::` fronted by a single `PlayerManagerService` (`IService` + `ITickable`) that the composition root owns and subscribes to the one `FrameDispatcher` at a new additive key **`tick_order::kPlayerLifecycle = 250`**, between `kEntityRegistry (200)` and `kBubble (300)`. This placement yields a clean single-frame pipeline: EntityRegistry (200) → **PlayerLifecycle (250)** → Bubble (300) → ALifeTransition (350). Session join/leave events (fired during the networking tick at 900) are captured by a lightweight observer that only **enqueues** deltas; the manager **drains and applies** them at 250 on the following frame — a deterministic one-frame ingress latency that keeps networking free of simulation mutation. The manager updates the `PlayerRegistry`, drives entity materialization through `IPlayerSpawnGateway`, and republishes the `IPlayerPositionSource` feed the Bubble reads later the same frame. Online/offline then flows through the untouched Bubble→Transition path.

Data-flow (one frame):
`Session (900, prev frame) → OnMemberJoined/Left → PlayerDeltaQueue` → **[frame N, 250]** `PlayerManager.drain+apply → PlayerRegistry + IPlayerSpawnGateway + EntityRegistry + PositionFeed` → `Bubble (300) reads positions → Transition (350) switches ALife`.

## 9. Component decomposition

- `player::PlayerTypes` — value types/enums: `PlayerId` (reuses `world::PlayerId`), `PlayerLifecycleState` (Joining / Active / Dead / AwaitingRespawn / Suspended / Removed), `PlayerConnectionState` (Connected / Suspended / Reclaimed), `DisconnectDisposition`, `JoinOutcome`, `SpawnOutcome`, `ReconnectOutcome`, `PlayerRecord`, `PlayerMappingView`, `PlayerStatistics`. Engine-free, OS-free.
- `player::PlayerConfiguration` (+ `FromConfig`) — capacity, respawn delay (ticks), reconnect-retention window (ticks), spawn-selection policy id. Mirrors existing `*Config` FromConfig parsing.
- `player::PlayerRegistry` — canonical record store: sorted `std::vector` keyed ascending by `PlayerId` + hash accelerators for the secondary keys (`ConnectionId`, session member, `EntityId`), with `ValidateConsistency`.
- `player::PlayerManager` — engine-free core: owns the registry, runs the per-tick drain+apply of the lifecycle state machine, requests spawn/despawn via the gateway, publishes the position feed, exposes read-only surface + diagnostics.
- `player::PlayerManagerService` — `IService` + `ITickable`; owns the manager; ticked at `kPlayerLifecycle = 250`; `Dependencies() = {World, EntityRegistry, BubbleManager, AlifeTransition, Networking}` (ordering-only const-reference edges).
- `player::ISessionMembershipObserver` impl (internal) — subscribes to Sprint-006 `net::ISessionObserver`; records join/leave deltas into `PlayerDeltaQueue` only (no mutation in the networking tick).
- `player::NetworkedPlayerPositionSource` — implements `world::IPlayerPositionSource` (positions only, ascending `PlayerId`), fed by the manager; the Bubble Manager binds to this in place of the Sprint-004 local stub.
- `player::IPlayerSpawnGateway` — the **single new engine-free seam**: `Spawn(profile, position) -> Expected<EntityId>`, `Despawn(EntityId) -> outcome`. Concrete `adapters::EnginePlayerSpawnGateway` lives **only** in `EngineAdapters.cpp`; `adapters::NullPlayerSpawnGateway` is the inert test-build counterpart; `adapters::CreatePlayerSpawnGateway()` is the factory. Directly mirrors Sprint-005's `IAlifeSwitchGateway` precedent.
- `player::PlayerDiagnostics` — read-only `Statistics` / `DescribeState` / `DescribePlayer` / `DumpPlayers` / `ValidateConsistency` (iostream-free, `common::Format`).

## 10. Public interfaces (conceptual only)

Conceptual contracts (no signatures frozen here; the Implementation Plan/Step Specs will fix them):

- `PlayerManager`: `RequestJoin(connection, profile) -> JoinOutcome`; `ApplyPendingDeltas(tick)` (drain queued session join/leave); `RequestRespawn(PlayerId) -> ReconnectOutcome`/`SpawnOutcome`; `NotifyDeath(PlayerId) -> outcome`; read-only `Find*`, `Statistics`, `ValidateConsistency`. All host-decided; all value outcomes.
- `PlayerRegistry`: allocate/retire `PlayerId`; insert/suspend/reclaim/remove records; the four `Find*` lookups; `ValidateConsistency`. Ascending, non-reused ids.
- `IPlayerSpawnGateway` (engine-free seam): `Spawn(profile, position) -> Expected<EntityId>`; `Despawn(EntityId) -> outcome`. Engine types confined behind it.
- `NetworkedPlayerPositionSource` (implements existing `world::IPlayerPositionSource`): `ActivePlayers() -> vector<PlayerPosition>` ascending, positions only.
- Observer bridge (implements existing `net::ISessionObserver`): `OnMemberJoined(id, joinTick)` / `OnMemberLeft(id, reason, reconnectToken)` → enqueue delta only.

## 11. Data ownership

| Data | Owner |
|---|---|
| Transport / socket / endpoint | Sprint-006 `PlatformSockets.cpp` / `TransportEndpoint` (unchanged) |
| Connection record + connection lifecycle | Sprint-006 `net::ConnectionTable` (unchanged) |
| Session membership + reconnect token | Sprint-006 `net::Session` (unchanged) |
| **Player identity, lifecycle state, and the id↔session↔entity mapping** | **`player::PlayerRegistry` (Sprint-007)** |
| Persistent entity identity / registration | Sprint-003 `world::EntityRegistry` (unchanged) |
| Entity online/offline (ALife) status | Engine ALife via Sprint-005 pipeline (unchanged) |
| Activation decision (who is online) | Sprint-004 `world::BubbleManager` (unchanged) |
| Player position feed the Bubble reads | `player::NetworkedPlayerPositionSource` (Sprint-007), positions only |
| Engine player-object creation/removal | Engine, behind `IPlayerSpawnGateway` (concrete in `EngineAdapters.cpp`) |

The player subsystem owns only mapping + lifecycle state; it never owns the entity's simulation state, session mechanics, or activation logic. Clients own **no** persistent world state (they issue requests only).

## 12. Lifetime and ownership model

`PlayerManagerService` is created in `Bootstrap.cpp` after the Sprint-006 networking services, owned by the `ServiceRegistry`. It is constructed with `PlayerConfiguration`, const references to the Entity Registry, Bubble Manager, and Session, and the gateway from `CreatePlayerSpawnGateway()` (Runtime-owned; null in tests). It subscribes to `net::Session` as an `ISessionObserver` during `Initialize` and to the `FrameDispatcher` at `kPlayerLifecycle = 250`. Teardown is strict reverse order: unsubscribe from the `FrameDispatcher` and from the `Session` **before** the Sprint-006 services shut down and before `ShutdownAll`, so no observer callback or tick fires against a torn-down subsystem. Player records are retained in-memory across disconnect (suspended) and released only on explicit removal or session-window expiry; the persistent entity's lifetime remains the engine/Entity-Registry's, released only via `IPlayerSpawnGateway::Despawn` on explicit removal — never on mere disconnect.

## 13. Determinism considerations

All ingress is applied from a queue drained in a fixed order (ascending `ConnectionId`, then ascending `PlayerId`) at a fixed tick slot (250); `PlayerId` is allocated strictly ascending and never reused; reconnect tokens are the deterministic Sprint-006 `MintToken(id, joinTick)`; respawn delay and retention windows are counted in ticks, never wall-clock; spawn-location selection is a deterministic policy over ordered inputs. The one-frame ingress latency (session event at 900 → applied at 250 next frame) is itself deterministic. Consequently an identical (session-delta + tick) sequence replays to identical registry state, identical entity registrations, and an identical position feed — the Sprint-007 analogue of the E-G3-N replay property.

## 14. Threading model

Single-threaded. The subsystem runs entirely on the one `FrameDispatcher` tick at 250 and on synchronous `ISessionObserver` callbacks (which only enqueue). It introduces no thread, no lock, no async work, and no IO. This conforms to ADR-011 and inherits its race/lifetime-hazard-free guarantee; the future thread-handoff boundary, if ever needed, remains behind the Sprint-006 transport seam, never in this subsystem.

## 15. Engine interaction boundaries

One Engine Boundary is preserved: the **only** engine-touching code added by Sprint-007 is `adapters::EnginePlayerSpawnGateway` inside `EngineAdapters.cpp`, reached exclusively through the engine-free `IPlayerSpawnGateway`. No engine header, type, or pointer appears in `player::*` or crosses the seam; the seam exposes only `PlayerProfile`, `PlayerPosition`, `EntityId`, and value outcomes. Online/offline switching is **not** re-implemented — it remains ADR-008's Cooperative Flag Override behind Sprint-005's `IAlifeSwitchGateway`. The engine player object is materialized via the engine's vanilla ALife creature/actor creation (Preserve Before Replace, Reuse Engine Systems); the gateway performs on-demand resolution with no retained engine pointer or cache, exactly as Sprint-005 established.

## 16. Networking boundaries

Sprint-007 does **not** touch transport, framing, endianness, or the wire header (ADR-009/ADR-010 closed). It consumes the Sprint-006 control-plane seams only: `net::ISessionObserver` for join/leave, `net::Session::TryReclaim` for reconnect, and the `net::MessageRegistry` **additive-id** extension point to register player request messages (join/respawn *requests*) in the reserved data-id range. Client messages are requests; the host owns every transition (Host Authority). Networking remains last in the frame (900) and owns no simulation; the player subsystem never calls into transport and never sends from within the networking tick — outbound player-state notifications, when later required by replication, are Sprint-008's concern, not this sprint's.

## 17. Persistence boundaries

Persistence in Sprint-007 is **in-memory retention only**. On disconnect the `PlayerRecord` is retained in a `Suspended` state and the persistent entity is left to fall offline through the normal Bubble path (no position published → not activated); the engine's own vanilla ALife already persists offline objects, so no new persistence format is introduced. Reconnect reclaims the retained record via the Sprint-006 token and republishes the position, letting the existing pipeline reactivate the same entity. **No disk save-game format, no database, no serialization schema** is defined here; those are Sprints 011–012, and the retained `PlayerRecord` is deliberately reconstructable so a future persistence sprint can serialize it without redesign. The provisional `ISerializable` interface (noted as a known risk in `CURRENT_STATUS.md`) is **not** finalized or depended upon here.

## 18. Failure handling

Every fallible operation degrades benignly as a value outcome, never an exception or a partial mutation. Duplicate join, capacity-exceeded, unknown session, invalid reconnect token, spawn-gateway failure (`EntityMissing`/engine-unavailable), invalid respawn, and ownership violation each produce a specific outcome enum and leave the registry unchanged (transactional per delta). A failed `Spawn` rolls back the in-flight join (no orphan `PlayerId`, no dangling mapping). A reconnect whose token does not resolve is treated as a fresh join candidate (subject to validation), never as a silent duplicate. Teardown with active/suspended players releases mappings and despawns owned entities in reverse order, leaving no orphan record, mapping, or entity.

## 19. Error model

Conforms to ADR-007: no exceptions, no RTTI. Fallible calls return `core::Expected<T>`; multi-step pipelines return per-stage value outcomes (`JoinOutcome`/`SpawnOutcome`/`ReconnectOutcome`) parallel to their inputs, mirroring Sprint-005's `TransitionOutcome` discipline. Diagnostics use `common::Format` (iostream-free). Where no dedicated `ErrorCode` exists, `InvalidArgument` carries a precise `common::Format` message and the call site maps it to the domain outcome (the established project convention). Assertions (`SMP_ASSERT`/`SMP_VERIFY`) guard internal invariants only, never validate untrusted client input (which flows through outcomes).

## 20. Diagnostics strategy

`PlayerDiagnostics` is read-only and deterministic: `Statistics()` (connected/suspended/dead counts, respawns, deaths, reconnects, join tick, average session duration in ticks), `DescribeState()`, `DescribePlayer(PlayerId)`, `DumpPlayers()` (ascending), and an extended `ValidateConsistency()` covering registry ordering, mapping bijection (`PlayerId↔ConnectionId↔SessionMember↔EntityId` one-to-one where required), lifecycle-state legality, position-feed↔registry agreement, and no-orphan/no-duplicate guarantees. Bandwidth/latency fields are reserved (not populated) for a later sprint. All facts are tick-derived so diagnostics are themselves replay-stable.

## 21. Testing strategy

(Strategy only; the test suite is authored in the implementation phase.) The engine-free core is fully unit-testable against `NullPlayerSpawnGateway`, `MockTransport`/`LoopbackTransport`, and the existing null adapters — no engine, no OS. Coverage targets: join, leave/suspend, spawn, death, respawn, reconnect (reclaim), duplicate-join rejection, duplicate-entity prevention, ownership-violation rejection, capacity limits, and a many-player churn stress test; a determinism/replay test (identical session-delta+tick sequence twice → identical registry + registrations + position feed); a teardown/leak test (reverse-order shutdown with active + suspended players leaves no orphan). Integration-level checks (behind seams): player appears in the Entity Registry, Bubble includes the player's position, session mapping is correct, disconnect preserves the record, reconnect restores the same entity. Independent verification remains Antigravity's, per governance.

## 22. Extension points

Reserved, not implemented: the `MessageRegistry` additive-id space for future player messages; `PlayerRecord` fields reserved for a future durable-persistence serialization (Sprints 011–012); the position feed as the natural producer for Sprint-008 replication's player-state source; `IPlayerSpawnGateway` as the single seam for any future engine-side player representation change; `PlayerDiagnostics` reserved bandwidth/latency fields. No future producer `tick_order` value is assigned or depended upon (the 400/500/600/700 keys remain owned by their sprints; Sprint-007 adds only 250).

## 23. Risks

R-1 Duplicate entity on reconnect or concurrent join. R-2 Session↔player mapping desynchronization (a leftover mapping after disconnect). R-3 Invalid-reconnect ownership takeover (a client reclaiming another's player). R-4 Spawn conflict / spawn-gateway failure leaving an orphan `PlayerId` or dangling mapping. R-5 Entity leak on teardown. R-6 Hidden coupling: the player subsystem reaching into engine or simulation state directly, eroding One Engine Boundary or Sprint-005 ownership. R-7 Ordering hazard: mutating simulation from within the networking tick (900), breaking "networking owns no simulation" and determinism. R-8 Scope creep into replication/persistence.

## 24. Mitigations

R-1 → strictly ascending, non-reused `PlayerId`; reconnect is *only* via `Session::TryReclaim` rebinding to the existing `PlayerId`/`EntityId`; `ValidateConsistency` enforces one-entity-per-player; duplicate-entity-prevention is a required test. R-2 → the mapping is a bijection checked every `ValidateConsistency`; suspend/remove are transactional. R-3 → reconnect authority is the host-minted deterministic token; a non-matching token can never reclaim. R-4 → join is transactional: a failed `Spawn` rolls back the `PlayerId` and mapping (no orphan). R-5 → reverse-order teardown despawns owned entities and clears mappings; leak test required. R-6 → the only engine code is `EnginePlayerSpawnGateway` in `EngineAdapters.cpp`; online/offline stays behind Sprint-005's gateway; enforced by the engine-free test build. R-7 → observer callbacks only *enqueue*; all mutation happens at tick 250; determinism/replay test guards it. R-8 → out-of-scope items (§4) are explicit; no serialization/replication interface is defined or depended upon.

## 25. Acceptance criteria

(Architecture-level; delivery criteria are set by the Implementation Plan.) The architecture is acceptable when it: establishes the `player::` subsystem and its single `ITickable` at the additive `kPlayerLifecycle = 250` between EntityRegistry and Bubble; defines join/spawn/disconnect/reconnect/death/respawn as a deterministic, replay-safe, host-authoritative state machine; makes every player a persistent World entity owned by the Entity Registry while the subsystem owns only mapping+lifecycle; preserves reconnect-without-duplication via the Sprint-006 token; confines all engine contact to one gateway in `EngineAdapters.cpp` (One Engine Boundary) and reuses Sprint-005 for online/offline (ADR-008 unbroken); touches no transport/framing (One Platform Boundary and ADR-009/010 intact) and only registers additive message ids; keeps persistence in-memory (no save format); and conforms to ADR-007/008/009/010/011 with no modification to any frozen sprint architecture. §28 records the review as passed.

## 26. Dependencies on previous sprints

Sprint-002 (World / `FrameDispatcher` / `IPlayerPositionSource` seam), Sprint-003 (Entity Registry — registration and `Find*`), Sprint-004 (Bubble Manager — consumes the position feed; the networked source implements the same interface so the Bubble is unchanged), Sprint-005 (ALife Transition — the sole online/offline mechanism, reused via `IAlifeSwitchGateway`; the `EnginePlayerSpawnGateway` follows its engine-boundary precedent), Sprint-006 (Host Networking — `Session`, `ISessionObserver`, `Session::TryReclaim`/`MintToken`, `ConnectionTable`, `MessageRegistry` additive ids). All are consumed as frozen; none is modified.

## 27. Future sprint dependencies

Sprint-008 (Snapshot System) consumes the player position/state feed as a replication source and will read player records via the read-only surface. Sprints 009–010 (delta replication, prediction/interpolation) build on that snapshot, not on this subsystem directly. Sprints 011–012 (Persistence) serialize the deliberately-reconstructable `PlayerRecord` to a durable format — the field reservations here exist for that. No future sprint is pre-committed to a signature or a `tick_order` value by this document.

## 28. Architecture decision summary

AD-1 New engine-free `player::` subsystem; a single `ITickable` `PlayerManagerService`. AD-2 Additive `tick_order::kPlayerLifecycle = 250` (EntityRegistry → **Player** → Bubble → Transition) — a clean single-frame pipeline. AD-3 Session join/leave captured by an observer that only *enqueues*; all mutation applied deterministically at 250 (preserves "networking owns no simulation"). AD-4 The player is a persistent Entity-Registry-owned World entity; the subsystem owns only mapping + lifecycle. AD-5 Online/offline is **reused** from Sprint-005 (not duplicated); ADR-008 untouched. AD-6 Exactly one new engine seam, `IPlayerSpawnGateway`, concrete only in `EngineAdapters.cpp` (One Engine Boundary), mirroring `IAlifeSwitchGateway`. AD-7 Reconnect without duplication via the Sprint-006 deterministic token (`TryReclaim`/`MintToken`). AD-8 Persistence is in-memory retention only; no save format (deferred to 011–012). AD-9 Networking untouched except additive `MessageRegistry` ids; ADR-009/010 closed. AD-10 Single-threaded, tick-derived, replay-safe (ADR-011). No new ADR is required; no existing ADR is superseded.

---

## Architecture Review (self-review)

- **Architectural consistency.** The subsystem mirrors the established shape (value types → engine-free core + store → `IService`/`ITickable` service → single engine seam with null test counterpart → read-only diagnostics), consistent with Sprints 003–006. Consistent. No inconsistency found.
- **ADR compliance.** ADR-007: engine-free core, no exceptions/RTTI, value outcomes, `common::Format` — conformant. ADR-008: no new engine-mutation path; online/offline reused via Sprint-005 — conformant. ADR-009: no OS/platform header enters the subsystem — conformant. ADR-010: no wire/header change; additive ids only — conformant. ADR-011: single-threaded, tick-derived — conformant. No ADR modified or superseded.
- **Scope isolation.** Every out-of-scope area (replication, prediction, durable persistence, chat/voice/trade, transport/framing) is explicitly excluded (§4) and none is depended upon. Isolated.
- **Determinism & replay safety.** Fixed-order queue drain at a fixed tick slot, ascending non-reused ids, tick-derived timing, deterministic tokens, deterministic spawn policy → replayable (§13). The one-frame ingress latency is deterministic. Safe.
- **Ownership boundaries.** Mapping+lifecycle in `PlayerRegistry`; entity identity in Entity Registry; ALife status in the engine via Sprint-005; session in Sprint-006; positions-only feed to the Bubble. Bijection enforced by `ValidateConsistency`. Clean; no ownership overlap.
- **Engine boundary preservation.** One new engine TU path (`EnginePlayerSpawnGateway` in `EngineAdapters.cpp`) behind an engine-free seam; test build uses the null gateway. Preserved.
- **Platform boundary preservation.** No transport/socket contact; Sprint-006's One Platform Boundary is untouched. Preserved.
- **Hidden coupling.** Two candidate couplings were identified and resolved in-architecture: (a) **networking-tick mutation** — resolved by the enqueue-only observer + apply-at-250 rule (AD-3); (b) **duplicate online/offline logic** — resolved by reusing Sprint-005 rather than switching ALife here (AD-5). A third candidate, the Bubble depending on player/session state, is avoided because the feed is positions-only through the *existing* `IPlayerPositionSource` (invariant B11). No residual hidden coupling.
- **Future extensibility.** Additive message ids, reconstructable `PlayerRecord`, positions/state feed as a replication source, and the single spawn seam provide clean extension points for Sprints 008–012 without reopening this architecture.

**Risks discovered during review and their resolution:** RR-1 *Tick-ordering ambiguity* (should player run before or after Bubble?) — **resolved** by fixing `kPlayerLifecycle = 250` (before Bubble, after EntityRegistry) so a joined player is visible to activation the same frame (AD-2). RR-2 *Re-entrant mutation from the 900 networking tick* — **resolved** by the enqueue-only observer and apply-at-250 rule (AD-3, R-7). RR-3 *Temptation to switch ALife directly for "spawn online"* — **resolved** by delegating online/offline entirely to the Sprint-005 pipeline; Sprint-007 only makes the player eligible (AD-5, FR-4). RR-4 *Persistence scope bleed* — **resolved** by constraining persistence to in-memory retention with a reconstructable record and no save format (AD-8, §17). All discovered risks are resolved within this architecture; none requires a new ADR.

---

## 29. Architecture Freeze

Sprint-007 (Player Lifecycle) Architecture is **FROZEN** as of 2026-07-11. The self-review passed on every dimension; all risks discovered during review (RR-1…RR-4) are resolved within this document. It conforms to ADR-007, ADR-008, ADR-009, ADR-010, and ADR-011, modifies no previously frozen sprint architecture, requires no new or superseding ADR, and preserves every project principle (Preserve Before Replace, Host Authority, One Engine Boundary, One Platform Boundary, Replay Determinism, Deterministic Simulation, One World, One ALife Simulation, clients own no persistent world state, Reuse Engine Systems, Incremental/Frozen architecture, Independent verification).

This architecture is ready for the next phase: **Implementation Plan**. No implementation plan, step specifications, code, or verification content is produced here — the architecture phase ends at this freeze.

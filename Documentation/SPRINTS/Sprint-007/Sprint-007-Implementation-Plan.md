# Sprint-007 — Player Lifecycle — Implementation Plan (AUTHORITATIVE)

- **Status:** Active implementation roadmap. Architecture **FROZEN** (`Multiplayer/docs/Sprint-007-Architecture.md`, frozen 2026-07-11). This plan implements that architecture verbatim and introduces no new architectural decision, ADR, scope change, or ownership change.
- **Roles:** Principal Architect authored this plan; the Implementation Engineer converts each step into one implementation prompt; Antigravity (Verification Engineer) independently verifies each step before the next begins.
- **Frozen invariants (immutable here):** Preserve Before Replace, Host Authority, One World, One ALife Simulation, One Engine Boundary, One Platform Boundary (ADR-009), Wire Protocol Contract (ADR-010), Single-Threaded/Deterministic boundary (ADR-011), Deterministic Simulation, Replay Determinism, ADR-007, ADR-008, and the existing ownership of Entity Registry (Sprint-003), Bubble Manager (Sprint-004), ALife Transition (Sprint-005), and Session/Connection (Sprint-006). Clients own no persistent world state. The existing `tick_order` table is preserved; Sprint-007 adds exactly one additive key `kPlayerLifecycle = 250` (frozen architecture AD-2).

---

## 1. Sprint Overview

**Objective.** Build the engine-free `player::` subsystem that turns a Sprint-006 connection into a persistent, host-authoritative World entity and drives it through join → spawn → death → respawn → disconnect (suspend) → reconnect (reclaim), as a deterministic, replay-safe, single-threaded `ITickable` at `tick_order::kPlayerLifecycle = 250`. Online/offline switching is **reused** from Sprint-005 (not reimplemented); all engine contact is confined to a single new gateway in `EngineAdapters.cpp`; networking, framing, and persistence-to-disk are untouched.

**Implementation boundaries (in scope).** `player::PlayerTypes`; `player::PlayerConfiguration`; `player::PlayerRegistry` + the four `Find*` lookups; the lifecycle state machine; the session observer bridge + `PlayerDeltaQueue`; `player::PlayerManager`; `player::NetworkedPlayerPositionSource` (implements the existing `world::IPlayerPositionSource`); `player::IPlayerSpawnGateway` (engine-free seam) + `adapters::NullPlayerSpawnGateway` + `adapters::EnginePlayerSpawnGateway` (the one engine TU) + factory; `player::PlayerManagerService` + `tick_order::kPlayerLifecycle = 250` + Bootstrap wiring; additive `MessageRegistry` player-request ids; `player::PlayerDiagnostics`; documentation + closure.

**Explicit non-goals (do NOT implement).** Snapshot replication, delta compression, prediction/interpolation (Sprints 008–010); durable/disk persistence, save-game format, serialization schema, or finalizing `ISerializable` (Sprints 011–012); inventory/trading/co-op/chat/voice; any transport, wire-header, or endianness change (ADR-009/010 closed — only additive `MessageRegistry` ids); any new engine-state online/offline mutation path (owned by Sprint-005/ADR-008 — reused only); any IO thread or multithreading (ADR-011); assignment of, or dependency on, any future producer `tick_order` value (the 400/500/600/700/900 keys remain owned by their sprints).

---

## 2. Implementation Strategy

**Philosophy — engine-free core first, single engine TU late, wiring last (the Sprints 005/006 shape).** Steps 1–8 and 12–13 are entirely engine-free, verified on GCC + MSVC against `NullPlayerSpawnGateway`, `MockTransport`/`LoopbackTransport`, and the existing null adapters. Only Step 10 introduces engine headers, confined to `adapters::EnginePlayerSpawnGateway` inside `EngineAdapters.cpp` behind the engine-free `IPlayerSpawnGateway` (One Engine Boundary). Composition-root wiring (Step 11) touches the frozen `Bootstrap.cpp` exactly once, after the subsystem is proven in isolation.

**Why this order minimizes integration risk.** The highest-risk surfaces (engine object creation, composition-root lifetime, the `Bubble` position-feed swap, and the networking-tick ingress) are isolated and deferred; the bulk of the logic (records, lookups, the lifecycle state machine, transactional join/reclaim) is proven with in-memory fakes first. Every step is purely additive and leaves the tree buildable; no step depends on a later step to compile. The queue + observer substrate (Step 6) exists before the manager (Step 7), so the deterministic drain-and-apply is testable the moment it lands. The `IPlayerPositionSource` swap (Step 8) is a drop-in behind an existing frozen interface, so the Bubble Manager is never modified.

**Subsystem dependency order (strict).** value types → config → registry storage → lookup APIs → lifecycle state machine → session integration (observer + queue) → player manager (drain/apply, transactional join/reclaim) → position source → spawn-gateway interface + null → engine adapter → service wiring (`kPlayerLifecycle` + Bootstrap) → diagnostics → validation hardening → integration cleanup → readiness review. Each capability is introduced once, in dependency order, so no partial-ownership state ever exists.

**Namespace / layout conventions (mapping only, not new decisions).** New subsystem namespace `stalkermp::player`; headers under `include/stalkermp/player/`; sources under `src/player/`; the engine gateway impl in the existing `src/adapters/EngineAdapters.cpp` with its null counterpart under `tests/support/NullPlayerSpawnGateway.cpp`; tests in a new `tests/PlayerTests.cpp` (+ `tests/BootstrapTests.cpp` for the wiring assertion). This mirrors the `world`/`net`/`adapters` split already in the tree. The `NetworkedPlayerPositionSource` binds into the Bubble in place of the Sprint-004 `LocalPlayerPositionSource` **only** at the wiring step.

---

## 3. Implementation Steps

> Every step: engine-free unless explicitly noted (only Step 10 touches engine headers); single-threaded; ADR-007 (no exceptions, no RTTI, `Expected<T>`/value outcomes, C4530-clean, no iostream — `common::Format`); deterministic and replay-safe; additive; leaves the project buildable on GCC + MSVC. Each step is independently verified by Antigravity before the next begins.

### Step 1 — Player value types
- **Step number.** 1.
- **Objective.** Establish the engine-free vocabulary for the subsystem.
- **Scope.** Value types and enums only; no logic, no storage.
- **Files expected to change.** Create `include/stalkermp/player/PlayerTypes.h`; modify `xrMP.vcxproj` / `tests/xrMP_Tests.vcxproj` (add header); create `tests/PlayerTests.cpp` wired into the test project.
- **Dependencies.** Sprint-001 core (`Expected`); Sprint-004 `world::PlayerId`/`PlayerPosition` (reused); Sprint-003 `world::EntityId`; Sprint-006 `net::ConnectionId`/`DisconnectReason` (referenced by value only).
- **Interfaces involved.** None (types only).
- **Inputs.** Frozen architecture §9 (component value shapes), §11 (ownership vocabulary).
- **Outputs.** `PlayerId` (reuses `world::PlayerId`), `PlayerLifecycleState` (Joining/Active/Dead/AwaitingRespawn/Suspended/Removed), `PlayerConnectionState` (Connected/Suspended/Reclaimed), `DisconnectDisposition`, `JoinOutcome`/`SpawnOutcome`/`ReconnectOutcome`, `PlayerRecord`, `PlayerMappingView`, `PlayerStatistics`, with `Name()` helpers.
- **Invariants.** Engine-free, OS-free; value types carry no behavior; no engine/platform header.
- **Determinism considerations.** Enumerations are fixed-order; ids are plain values (no hashing here).
- **Engine-boundary considerations.** No engine header; only value references to existing engine-free ids.
- **Acceptance criteria.** Types match architecture §9; compile engine/OS-free; ADR-007-clean.
- **Testing expectations.** Trivial construction/`Name()` round-trip tests in `PlayerTests.cpp`; green on GCC + MSVC.
- **Stop condition.** Types exist and compile; no registry, no logic. Stop.

### Step 2 — Configuration
- **Step number.** 2.
- **Objective.** Parse and validate player-subsystem configuration.
- **Scope.** `PlayerConfiguration` + `FromConfig` only.
- **Files expected to change.** Create `include/stalkermp/player/PlayerConfiguration.h` (+ `src/player/PlayerConfiguration.cpp`); modify the two vcxprojs.
- **Dependencies.** Step 1; Sprint-001 `ConfigStore`, `common::Format`.
- **Interfaces involved.** `FromConfig(const ConfigStore&) -> Expected<PlayerConfiguration>` (project convention).
- **Inputs.** `[player]` config section: capacity, respawn delay (ticks), reconnect-retention window (ticks), spawn-selection policy id.
- **Outputs.** A validated `PlayerConfiguration` value; all durations stored as tick counts (never ms/wall-clock in control flow).
- **Invariants.** Missing → documented default; invalid → `InvalidArgument` with a precise message; no side effects.
- **Determinism considerations.** All timing is tick-derived at parse time; no wall-clock.
- **Engine-boundary considerations.** Engine-free.
- **Acceptance criteria.** Values match architecture §9; defaults/validation behave per project convention.
- **Testing expectations.** Default, override, and invalid-value cases; green on both toolchains.
- **Stop condition.** Config parses and validates; nothing consumes it yet. Stop.

### Step 3 — Registry storage
- **Step number.** 3.
- **Objective.** The canonical `PlayerRecord` store with id allocation.
- **Scope.** `PlayerRegistry` storage + `PlayerId` allocation + insert/retire only (no lookups, no lifecycle yet).
- **Files expected to change.** Create `include/stalkermp/player/PlayerRegistry.h` (+ `src/player/PlayerRegistry.cpp`); modify the two vcxprojs.
- **Dependencies.** Steps 1–2.
- **Interfaces involved.** Internal storage API: `Allocate() -> PlayerId`, `Insert(PlayerRecord) -> Expected<void>`, `Retire(PlayerId)`, `size()/capacity()`.
- **Inputs.** `PlayerConfiguration.capacity`.
- **Outputs.** A sorted `std::vector<PlayerRecord>` keyed ascending by `PlayerId`; monotonic non-reused id counter.
- **Invariants.** Ids strictly ascending, never reused; storage sorted-unique; capacity-bounded (over-capacity → `InvalidArgument`, call site maps to a capacity outcome — the established Sprint-006 convention).
- **Determinism considerations.** Ascending id allocation; deterministic insertion order.
- **Engine-boundary considerations.** Engine-free.
- **Acceptance criteria.** Allocation ascending/non-reused; capacity enforced; ordering holds.
- **Testing expectations.** Allocation monotonicity, capacity rejection, ordering invariants.
- **Stop condition.** Records can be stored/retired by `PlayerId`; no secondary lookups. Stop.

### Step 4 — Lookup APIs
- **Step number.** 4.
- **Objective.** The four secondary lookups over the registry.
- **Scope.** `FindByPlayerId`, `FindBySession`, `FindByEntity`, `FindByConnection` + their hash accelerators; `ValidateConsistency` (storage-level).
- **Files expected to change.** Modify `PlayerRegistry.h`/`.cpp`; extend `PlayerTests.cpp`.
- **Dependencies.** Step 3.
- **Interfaces involved.** Read-only `Find*` returning `optional<PlayerMappingView>` / `optional<const PlayerRecord*>` (const-correct).
- **Inputs.** `PlayerId`, `net::ConnectionId`, session member key, `world::EntityId`.
- **Outputs.** Const views resolved via hash accelerators keyed to the ascending vector (BC-005).
- **Invariants.** Accelerators consistent with the sorted vector; the `PlayerId↔ConnectionId↔SessionMember↔EntityId` mapping is a bijection where required; lookups never mutate.
- **Determinism considerations.** Lookups are order-independent and side-effect-free.
- **Engine-boundary considerations.** Engine-free.
- **Acceptance criteria.** All four lookups resolve; `ValidateConsistency` detects a deliberately corrupted accelerator.
- **Testing expectations.** Hit/miss for each key; accelerator-consistency negative test.
- **Stop condition.** Lookups resolve and validate; no lifecycle transitions yet. Stop.

### Step 5 — Lifecycle state machine
- **Step number.** 5.
- **Objective.** The pure, host-authoritative transition rules over a `PlayerRecord`.
- **Scope.** State-machine transitions only (join→active, active→dead, dead→awaiting-respawn, awaiting-respawn→active, active/dead→suspended, suspended→reclaimed→active, any→removed); legality checks; **no** session, gateway, registry-wide, or tick wiring.
- **Files expected to change.** Modify `PlayerRegistry.h`/`.cpp` or add `include/stalkermp/player/PlayerLifecycle.h` (+ `.cpp`) per the frozen component split; extend `PlayerTests.cpp`.
- **Dependencies.** Steps 1–4.
- **Interfaces involved.** Pure functions/methods returning value outcomes; no I/O.
- **Inputs.** Current `PlayerLifecycleState`/`PlayerConnectionState` + a requested transition + current tick.
- **Outputs.** Next state or a specific rejection outcome; respawn scheduling recorded as a target tick.
- **Invariants.** Every transition is legal-checked; illegal transitions are rejected as outcomes (never asserted on external input); death never auto-destroys; disconnect maps to Suspended (retain), not Removed; timing is tick-derived.
- **Determinism considerations.** Transition legality and respawn timing depend only on state + tick; no wall-clock, no ordering ambiguity.
- **Engine-boundary considerations.** Engine-free.
- **Acceptance criteria.** The full legal transition table passes; each illegal transition returns its outcome; respawn delay counts in ticks.
- **Testing expectations.** Exhaustive legal/illegal transition matrix; respawn-delay tick math.
- **Stop condition.** State machine is correct in isolation; nothing drives it yet. Stop.

### Step 6 — Session integration (observer + delta queue)
- **Step number.** 6.
- **Objective.** Capture Sprint-006 join/leave events without mutating simulation in the networking tick.
- **Scope.** The `net::ISessionObserver` bridge + `PlayerDeltaQueue` (enqueue only); no application of deltas.
- **Files expected to change.** Create `include/stalkermp/player/PlayerDeltaQueue.h` and the observer bridge (header + `src/player/*.cpp`); extend `PlayerTests.cpp`; modify the two vcxprojs.
- **Dependencies.** Steps 1–5; Sprint-006 `net::ISessionObserver`, `net::Session` (consumed, unchanged).
- **Interfaces involved.** Implements `net::ISessionObserver::OnMemberJoined(id, joinTick)` / `OnMemberLeft(id, reason, reconnectToken)`; `PlayerDeltaQueue::Enqueue/Drain`.
- **Inputs.** Session join/leave callbacks (fired during the networking tick at 900).
- **Outputs.** Ordered queued deltas (join/leave with `ConnectionId`, tick, reason, reconnect token) — nothing else.
- **Invariants.** Observer callbacks perform **no** registry/gateway/entity mutation — they only enqueue (frozen architecture AD-3, R-7); queue preserves deterministic order (ascending `ConnectionId`, then arrival); Session ownership is unchanged.
- **Determinism considerations.** Enqueue-only guarantees the one-frame ingress latency is deterministic; drain order is fixed.
- **Engine-boundary considerations.** Engine-free; no networking redesign (consumes the existing seam).
- **Acceptance criteria.** Join/leave callbacks enqueue correctly and mutate nothing else; drain returns deltas in the fixed order.
- **Testing expectations.** Drive the bridge via `MockTransport`/`LoopbackTransport`-backed `Session`; assert enqueue-only and order.
- **Stop condition.** Deltas are captured and drainable; no manager applies them yet. Stop.

### Step 7 — Player manager (drain + apply; transactional join/reclaim)
- **Step number.** 7.
- **Objective.** The engine-free core that applies queued deltas through the state machine and registry deterministically.
- **Scope.** `PlayerManager`: drain the queue at a tick, apply join/suspend/reclaim/death/respawn transactionally against the registry; request spawn/despawn through an injected `IPlayerSpawnGateway&` (Step 9's interface — here consumed via the null in tests); publish nothing to the engine directly.
- **Files expected to change.** Create `include/stalkermp/player/PlayerManager.h` (+ `src/player/PlayerManager.cpp`); extend `PlayerTests.cpp`; modify the two vcxprojs.
- **Dependencies.** Steps 1–6; and the `IPlayerSpawnGateway` **interface** (Step 9) — sequencing note below.
- **Interfaces involved.** `RequestJoin`, `ApplyPendingDeltas(tick)`, `RequestRespawn`, `NotifyDeath`, read-only `Find*`/`Statistics`/`ValidateConsistency` (architecture §10); depends on `IPlayerSpawnGateway&`, `net::Session&`, `world::EntityRegistry&` (const where possible).
- **Inputs.** Drained deltas + current tick + host requests.
- **Outputs.** Updated registry state, entity registration requests via the gateway, and the data backing the position feed.
- **Invariants.** All mutation happens here at the manager's tick (never in the observer); join is transactional — a failed `Spawn` rolls back the `PlayerId` and mapping (no orphan); reconnect rebinds to the **existing** `PlayerId`/`EntityId` via `Session::TryReclaim` (never a duplicate entity); Host Authority (client input is request-only); Entity Registry / Session ownership unchanged.
- **Determinism considerations.** Deltas applied in fixed order at the fixed tick slot; ascending `PlayerId`; deterministic reconnect token; identical (delta+tick) sequence → identical state (replay-safe).
- **Engine-boundary considerations.** Engine-free; the gateway is an abstract reference (null in tests).
- **Acceptance criteria.** Join/suspend/reclaim/death/respawn apply correctly; transactional rollback on gateway failure; no duplicate entity on reclaim or concurrent join.
- **Testing expectations.** Full lifecycle flows against `NullPlayerSpawnGateway`; rollback test; duplicate-prevention test; a determinism/replay test (same sequence twice → identical registry + registrations + feed data).
- **Stop condition.** The core applies all lifecycle flows deterministically against the null gateway; no service, no engine, no position-source binding. Stop.

> **Sequencing note (dependency ordering vs the suggested progression).** The frozen architecture defines `PlayerManager` as depending on the `IPlayerSpawnGateway` **interface**. To keep every step compilable and testable, the interface header + null (progression items 9) is introduced **before** the manager consumes it. This plan therefore introduces the `IPlayerSpawnGateway` header and `NullPlayerSpawnGateway` as **Step 9 but delivered no later than Step 7's need** — concretely, the interface header lands at the start of Step 7 and the standalone null/adapters work remains Steps 9–10. This is the only deviation from the numeric progression and is required by the frozen architecture's dependency edges (§9/§10); it changes no decision.

### Step 8 — Position source
- **Step number.** 8.
- **Objective.** Feed player positions to the Bubble via the existing frozen interface, positions only.
- **Scope.** `NetworkedPlayerPositionSource` implementing `world::IPlayerPositionSource`; fed by `PlayerManager`; **no** Bubble modification.
- **Files expected to change.** Create `include/stalkermp/player/NetworkedPlayerPositionSource.h` (+ `.cpp`); extend `PlayerTests.cpp`; modify the two vcxprojs.
- **Dependencies.** Steps 1–7; Sprint-004 `world::IPlayerPositionSource` (implemented, unchanged).
- **Interfaces involved.** `world::IPlayerPositionSource::ActivePlayers() -> vector<PlayerPosition>` (ascending `PlayerId`, positions only).
- **Inputs.** The manager's per-tick active-player positions.
- **Outputs.** An immutable ascending snapshot the Bubble Manager reads unchanged.
- **Invariants.** Positions only — no connection/session/lifecycle state leaks into World (invariant B11); ascending `PlayerId` (B9); the Bubble Manager and its interface are untouched.
- **Determinism considerations.** Snapshot is a pure function of manager state; ascending order.
- **Engine-boundary considerations.** Engine-free.
- **Acceptance criteria.** Implements the frozen interface exactly; only positions cross; ordering holds.
- **Testing expectations.** Feed manager state, assert the snapshot content/order; assert no non-position field is exposed.
- **Stop condition.** The source produces correct snapshots; not yet bound into the Bubble (wiring is Step 11). Stop.

### Step 9 — Spawn gateway interface + null
- **Step number.** 9.
- **Objective.** The single engine-free seam for materializing/removing the persistent player entity, plus its inert test counterpart.
- **Scope.** `IPlayerSpawnGateway` interface + `NullPlayerSpawnGateway` + `CreatePlayerSpawnGateway()` factory declaration; **no** engine code.
- **Files expected to change.** Create `include/stalkermp/player/IPlayerSpawnGateway.h` and `include/stalkermp/adapters/PlayerSpawnGateway.h` (factory decl); create `tests/support/NullPlayerSpawnGateway.cpp`; modify `tests/xrMP_Tests.vcxproj`.
- **Dependencies.** Steps 1, 7 (interface consumed by the manager — see the Step 7 sequencing note).
- **Interfaces involved.** `Spawn(PlayerProfile, PlayerPosition) -> Expected<EntityId>`; `Despawn(EntityId) -> outcome`.
- **Inputs.** Character profile handle + spawn position (engine-free values).
- **Outputs.** An `EntityId` on success, value outcomes on failure; the null returns deterministic synthetic ids.
- **Invariants.** The seam names no engine call and exposes no engine type (mirrors Sprint-005 `IAlifeSwitchGateway`); the null is fully deterministic and OS/engine-free; the test build links only the null.
- **Determinism considerations.** Null-gateway ids are deterministic and ascending, preserving replay in the test build.
- **Engine-boundary considerations.** Interface is engine-free; **no** engine header here.
- **Acceptance criteria.** Manager (Step 7) compiles/tests against the null; factory declared but engine impl deferred.
- **Testing expectations.** Manager lifecycle tests run against `NullPlayerSpawnGateway`; spawn/despawn outcomes exercised.
- **Stop condition.** Interface + null exist and back the test build; no engine adapter yet. Stop.

### Step 10 — Engine adapter implementation
- **Step number.** 10.
- **Objective.** The one engine-touching TU that materializes/removes the vanilla ALife player object.
- **Scope.** `adapters::EnginePlayerSpawnGateway` + `CreatePlayerSpawnGateway()` definition inside `src/adapters/EngineAdapters.cpp`; **reuses** vanilla ALife creation; **does not** switch online/offline (that stays Sprint-005's pipeline).
- **Files expected to change.** Modify `src/adapters/EngineAdapters.cpp` and `include/stalkermp/adapters/PlayerSpawnGateway.h`; modify `xrMP.vcxproj` only (engine build). The test build continues to link `tests/support/NullPlayerSpawnGateway.cpp` (unchanged).
- **Dependencies.** Step 9.
- **Interfaces involved.** Implements `IPlayerSpawnGateway`; internally uses the engine's vanilla ALife object creation/removal.
- **Inputs.** `PlayerProfile` + `PlayerPosition`.
- **Outputs.** A real `EntityId` bound to a vanilla ALife object; value outcomes on failure (`EntityMissing`/engine-unavailable).
- **Invariants.** One Engine Boundary — engine headers appear **only** here; no engine pointer/id cache is retained (on-demand resolution, Sprint-005 precedent); Preserve Before Replace / Reuse Engine Systems (vanilla creation, no ALife edit); ADR-008 untouched (no online/offline mutation added).
- **Determinism considerations.** The engine call is behind the seam; determinism of the core is unaffected (verified via the null in the test build). Engine-side smoke is Antigravity's on Windows.
- **Engine-boundary considerations.** This is the sole engine TU added by Sprint-007; the test build never compiles it.
- **Acceptance criteria.** Engine build compiles under `EngineAbi.props`; test build unaffected; no engine header leaks outside `EngineAdapters.cpp`.
- **Testing expectations.** Engine-free test build stays green (null path); Antigravity performs the Windows engine compile + spawn/despawn smoke.
- **Stop condition.** The engine adapter compiles behind the seam; not yet wired into Bootstrap. Stop.

### Step 11 — Service wiring (`kPlayerLifecycle` + Bootstrap)
- **Step number.** 11.
- **Objective.** Integrate the subsystem as one `ITickable` at the additive tick key, with correct init/teardown and the Bubble feed swap.
- **Scope.** `PlayerManagerService` (`IService` + `ITickable`); add `tick_order::kPlayerLifecycle = 250`; register in `Bootstrap.cpp` after the Sprint-006 services; subscribe to the `FrameDispatcher` at 250 and to `net::Session` as an `ISessionObserver`; bind `NetworkedPlayerPositionSource` into the Bubble Manager in place of the Sprint-004 `LocalPlayerPositionSource`; register additive player-request `MessageRegistry` ids.
- **Files expected to change.** Create `include/stalkermp/player/PlayerManagerService.h` (+ `src/player/PlayerManagerService.cpp`); modify `include/stalkermp/core/FrameDispatcher.h` (add `kPlayerLifecycle = 250`), `src/core/Bootstrap.cpp`, `tests/BootstrapTests.cpp`; modify the two vcxprojs.
- **Dependencies.** Steps 1–10.
- **Interfaces involved.** `IService` (Name/Dependencies/Initialize/Shutdown), `ITickable` (Tick); `Dependencies() = {World, EntityRegistry, BubbleManager, AlifeTransition, Networking}` (ordering-only const edges); `net::ISessionObserver`; `net::MessageRegistry` additive ids.
- **Inputs.** The composed `PlayerConfiguration`, const references to Entity Registry / Bubble / Session, and the gateway from `CreatePlayerSpawnGateway()` (null in tests).
- **Outputs.** A running subsystem ticked once per frame at 250; the Bubble reading the networked position feed.
- **Invariants.** Existing `tick_order` table preserved (only `kPlayerLifecycle = 250` added, between 200 and 300); frame order EntityRegistry(200) → **Player(250)** → Bubble(300) → Transition(350); strict reverse-order teardown (unsubscribe from `FrameDispatcher` and `Session` before Sprint-006 shutdown and `ShutdownAll`); Bubble Manager code unchanged (only the injected source instance differs); networking untouched except additive ids; subscriber count assertion updated in `BootstrapTests`.
- **Determinism considerations.** The 250 slot makes a joined player visible to activation the same frame; the 900→250 ingress latency is deterministic (AD-2/AD-3).
- **Engine-boundary considerations.** Bootstrap composes the engine gateway in the engine build and the null in tests; no engine header added to Bootstrap.
- **Acceptance criteria.** Service ticks at 250; teardown leaves no orphan; `BootstrapTests` shows the new subscriber count with Player between EntityRegistry and Bubble; Bubble consumes the networked feed.
- **Testing expectations.** Bootstrap wiring/teardown tests; `static_assert(kEntityRegistry < kPlayerLifecycle && kPlayerLifecycle < kBubble)`; end-to-end lifecycle through the composed (null-gateway) stack.
- **Stop condition.** The subsystem is wired, ticks deterministically, and tears down cleanly. Stop.

### Step 12 — Diagnostics
- **Step number.** 12.
- **Objective.** Read-only, replay-stable observability.
- **Scope.** `PlayerDiagnostics`: `Statistics`, `DescribeState`, `DescribePlayer`, `DumpPlayers`, extended `ValidateConsistency`.
- **Files expected to change.** Create `include/stalkermp/player/PlayerDiagnostics.h` (+ `.cpp`); extend `PlayerTests.cpp`; modify the two vcxprojs.
- **Dependencies.** Steps 1–11.
- **Interfaces involved.** Read-only accessors over `PlayerManager`/`PlayerRegistry`.
- **Inputs.** Manager/registry state + current tick.
- **Outputs.** Tick-derived statistics (connected/suspended/dead, respawns, deaths, reconnects, join tick, average session duration in ticks); state dumps (ascending); a consistency report.
- **Invariants.** Read-only, deterministic, iostream-free (`common::Format`); bandwidth/latency fields reserved (not populated); mapping-bijection + ordering + lifecycle-legality + feed↔registry agreement checked.
- **Determinism considerations.** All facts tick-derived → diagnostics are replay-stable.
- **Engine-boundary considerations.** Engine-free.
- **Acceptance criteria.** All accessors read-only; `ValidateConsistency` catches injected corruptions (duplicate mapping, orphan, ordering break).
- **Testing expectations.** Statistics math; dump ordering; each consistency check has a negative test.
- **Stop condition.** Diagnostics report correctly and mutate nothing. Stop.

### Step 13 — Validation hardening
- **Step number.** 13.
- **Objective.** Prove the full negative surface and ownership enforcement.
- **Scope.** Test-and-harden pass (no new subsystem behavior): duplicate join, duplicate session, duplicate entity, invalid reconnect (bad/absent token), invalid spawn/respawn request, ownership violation, capacity limits, and a many-player churn stress test; tighten outcomes/messages as gaps surface.
- **Files expected to change.** Extend `tests/PlayerTests.cpp`; minor hardening in `player::*` `.cpp` only if a gap is found (no interface change).
- **Dependencies.** Steps 1–12.
- **Interfaces involved.** Existing `PlayerManager`/`PlayerRegistry` outcomes; no new interface.
- **Inputs.** Adversarial/edge request sequences.
- **Outputs.** Specific rejection outcomes; unchanged registry on every rejection.
- **Invariants.** Every rejection is a value outcome (never an exception or partial mutation); no duplicate entity ever; ownership (token-gated reconnect) enforced; determinism preserved under churn.
- **Determinism considerations.** Stress/churn replays identically; ascending ids under load.
- **Engine-boundary considerations.** Engine-free (null gateway).
- **Acceptance criteria.** All negative cases rejected with the correct outcome and no state change; stress test stable and replay-identical.
- **Testing expectations.** The full negative matrix + duplicate-entity-prevention + reconnect-authority + capacity + churn stress.
- **Stop condition.** The negative surface is fully covered and green. Stop.

### Step 14 — Integration cleanup
- **Step number.** 14.
- **Objective.** End-to-end integration behind seams + documentation of the delivered subsystem.
- **Scope.** Integration tests (player appears in Entity Registry; Bubble includes the player position; session mapping correct; disconnect preserves the record; reconnect restores the same entity) via the composed null-gateway/loopback stack; author `Multiplayer/docs/PlayerLifecycle.md`; final consistency sweep. No behavior change.
- **Files expected to change.** Extend `tests/PlayerTests.cpp`/`tests/BootstrapTests.cpp`; create `Multiplayer/docs/PlayerLifecycle.md`; no `player::*` interface change.
- **Dependencies.** Steps 1–13.
- **Interfaces involved.** The composed subsystem via existing seams (`ISessionObserver`, `IPlayerPositionSource`, `IPlayerSpawnGateway` null, Entity Registry).
- **Inputs.** Full join→spawn→death→respawn→disconnect→reconnect flows through the composed stack.
- **Outputs.** Passing integration tests; the subsystem doc; a clean tree.
- **Invariants.** All frozen boundaries intact; no engine/OS header outside the established TUs; ownership unchanged.
- **Determinism considerations.** Integration flows replay identically.
- **Engine-boundary considerations.** Engine-free test build; engine path exercised by Antigravity per Step 10.
- **Acceptance criteria.** Integration checks pass; doc matches delivered code; consistency sweep clean.
- **Testing expectations.** The six integration checks (architecture §21) green on both toolchains.
- **Stop condition.** Integration green and documented; ready for the readiness review. Stop.

### Step 15 — Final implementation-readiness review (no code)
- **Step number.** 15.
- **Objective.** Confirm Sprint-007 is implemented, verified, and closeable — not a code step.
- **Scope.** Cross-check every Definition-of-Done item (architecture §25, sprint doc §12) against delivered artifacts; record final test counts, build status, and boundary/ownership confirmations; update status docs to Closed/Verified; declare readiness for Sprint-008.
- **Files expected to change.** `Documentation/SPRINTS/Sprint-007-Player-Connections.md`, `Documentation/AI/CURRENT_STATUS.md`, `Documentation/AI/SESSION_LOG.md` (status/closure only) — mirroring the Sprint-006 closure precedent.
- **Dependencies.** Steps 1–14 verified.
- **Interfaces involved.** None (review/documentation).
- **Inputs.** Antigravity-verified facts (test counts, build status, gate/ownership confirmations).
- **Outputs.** Synchronized Closed/Verified status; recorded readiness for Sprint-008.
- **Invariants.** No code/architecture/ADR change; record only verified facts.
- **Determinism considerations.** N/A (documentation).
- **Engine-boundary considerations.** N/A.
- **Acceptance criteria.** All DoD items satisfied and recorded; status consistent across the three docs; no stale "Planned/in progress" for Sprint-007.
- **Testing expectations.** A consistency scan (no stale status; facts present) — the negative check.
- **Stop condition.** Sprint-007 declared Implemented / Verified / Closed / Frozen. Stop.

---

## 4. Step-to-architecture / suggested-progression mapping

| Plan step | Suggested progression item | Frozen architecture reference |
|---|---|---|
| 1 Player value types | 1 | §9 (PlayerTypes), §5 FRs |
| 2 Configuration | 2 | §9 (PlayerConfiguration) |
| 3 Registry storage | 3 | §9 (PlayerRegistry), §11 ownership |
| 4 Lookup APIs | 4 | §9/§10 (`Find*`), §20 consistency |
| 5 Lifecycle state machine | 5 | §7, §13, FR-2/5/7/8 |
| 6 Session integration | 6 | §8, AD-3, R-7 |
| 7 Player manager | 7 | §8/§10/§18, FR-1/4/6/12 |
| 8 Position source | 8 | §9, §15 (B11), Sprint-004 seam |
| 9 Spawn gateway interface + null | 9 | §9/§10/§15, Sprint-005 precedent |
| 10 Engine adapter | 10 | §15, AD-6, ADR-008 reuse |
| 11 Service wiring | 11 | §8/§12, AD-2/AD-3 |
| 12 Diagnostics | 12 | §20 |
| 13 Validation hardening | 13 | §18/§24, R-1…R-5 |
| 14 Integration cleanup | 14 | §21, §22 |
| 15 Readiness review | 15 | §25, DoD |

The only ordering deviation from the numeric progression is the Step 7↔9 interface dependency, handled by landing the `IPlayerSpawnGateway` header + null at the start of Step 7 (see the Step 7 sequencing note); the standalone gateway/adapter work remains Steps 9–10. This is required by the frozen architecture's dependency edges and changes no decision.

---

## 5. Implementation-Risk Review

- **Dependency ordering.** The strict order (types → config → registry → lookups → state machine → session queue → manager → position source → gateway/null → engine adapter → wiring → diagnostics → validation → integration → review) means every step compiles and tests against already-delivered pieces. *Risk:* the `PlayerManager`↔`IPlayerSpawnGateway` edge inverts the numeric 7↔9 order. *Resolution (in-plan):* the interface header + null land at the start of Step 7; standalone gateway work stays Steps 9–10 (Step 7 sequencing note). No later step is needed to compile an earlier one.
- **Hidden coupling.** Two architecture-identified couplings are enforced by step boundaries: (a) networking-tick mutation — Step 6 delivers an **enqueue-only** observer; all mutation is Step 7 at tick 250; (b) duplicate online/offline logic — no step switches ALife; Step 10 only materializes the entity and the existing Sprint-005 pipeline brings it online. A third (Bubble depending on player/session state) is prevented by Step 8 feeding positions-only through the existing frozen interface. No residual coupling.
- **Rollback safety.** Join is transactional (Step 7): a failed `Spawn` rolls back the `PlayerId` and mapping, leaving no orphan; every rejection (Step 13) leaves the registry unchanged. Each *plan* step is additive and leaves the tree buildable, so a step can be reverted without breaking earlier steps.
- **Transactional behavior.** Per-delta application is all-or-nothing; the mapping bijection is restored or untouched on failure; `ValidateConsistency` (Steps 4/12) is the invariant guard after every mutation path.
- **Initialization order.** `PlayerManagerService` is registered after the Sprint-006 services (Step 11) with `Dependencies()` naming World/EntityRegistry/Bubble/Transition/Networking (ordering-only), so `InitializeAll` constructs it after its inputs exist; the `Session` subscription and Bubble feed swap happen in `Initialize`, after those services are up.
- **Shutdown order.** Strict reverse order (Step 11): unsubscribe from the `FrameDispatcher` and `Session` before Sprint-006 shutdown and before `ShutdownAll`, so no tick or observer callback fires against a torn-down subsystem; owned entities are despawned via the gateway on removal.
- **Ownership correctness.** Step boundaries keep ownership exactly per architecture §11: mapping+lifecycle in `PlayerRegistry`; entity identity in the Entity Registry; ALife status in the engine via Sprint-005; session in Sprint-006; positions-only feed to the Bubble. No step writes another subsystem's owned state.
- **Determinism.** Fixed-order queue drain at the fixed 250 slot, ascending non-reused ids, tick-derived timing, deterministic reconnect token, deterministic null-gateway ids — introduced at the steps that need them (6, 7, 3, 9) and guarded by the Step 7/13 replay tests.
- **Replay safety.** The Step 7 determinism test (identical delta+tick sequence twice → identical registry + registrations + feed) and the Step 13 churn replay are the explicit guards; the engine adapter (Step 10) is behind the seam and does not affect test-build replay.
- **Future-sprint compatibility.** Additive `MessageRegistry` ids (Step 11), the reconstructable `PlayerRecord` (Steps 1/3), the positions/state feed as a Sprint-008 replication source (Step 8), and the single spawn seam (Steps 9–10) are delivered without reopening the architecture; no future producer `tick_order` value is assigned (only 250 added).

All implementation-planning risks above are resolved within this plan; none requires an architecture or ADR change.

---

## 6. Implementation Plan Freeze

The Sprint-007 Implementation Plan is **FROZEN** as of 2026-07-11.

- **Internally consistent.** The 15 steps form a strict, additive dependency chain; each is independently verifiable, leaves the tree buildable, and maps to the frozen architecture (§4 mapping). The single ordering deviation (7↔9 interface edge) is explicitly resolved in-plan.
- **Conforms to the frozen Sprint-007 Architecture.** Every step derives from `Multiplayer/docs/Sprint-007-Architecture.md`; the engine-free core, single-threaded execution, queue-based deterministic processing, Host Authority, and all existing ownership boundaries (Bubble, ALife Transition, Session, Entity Registry) and tick ordering are preserved. No step introduces networking/persistence redesign, replication, snapshot, prediction, platform code outside `EngineAdapters`, or engine headers outside the established boundary.
- **No architecture modified.** This plan changes no architectural decision, no ADR (ADR-007/008/009/010/011 all preserved), and no previously frozen sprint architecture.
- **Ready for the next phase.** Sprint-007 is ready to begin **Step Specifications**, one step at a time, each frozen before implementation and independently verified by Antigravity before the next begins.

No step specifications, implementation, or verification are produced here — the implementation-planning phase ends at this freeze.

---

## 7. Implementation Plan Clarifications

> Planning-clarification pass (post-freeze addendum). This section clarifies existing plan content for zero-ambiguity Step Specifications. It changes no architectural decision, no ADR, no sprint scope, and no implementation order; it only makes explicit what the frozen plan already implies and adds verification requirements within the existing Step 11 scope.

### 7.1 Gateway interface sequencing (clarifies Step 7 / Step 9)

Already partially covered by the Step 7 sequencing note; restated here as four unambiguous, binding rules so the Step Specifications inherit no ambiguity. No order change results — the plan already lands the interface before the manager consumes it.

1. **Interface-before-implementation.** The engine-free `include/stalkermp/player/IPlayerSpawnGateway.h` header (declaration only) MUST exist and compile before any `PlayerManager` implementation begins. Concretely, this header is authored at the **start of Step 7** (it is otherwise part of the Step 9 deliverable). The `PlayerManager` code is written only after the interface header is in the tree.
2. **Abstract-only dependency.** `PlayerManager` depends **solely** on the abstract `IPlayerSpawnGateway&` (constructor-injected). It names no concrete gateway type, includes no adapter header, and holds no engine type. Swapping the concrete implementation for the null (or vice versa) requires no change to `PlayerManager`.
3. **Engine adapter deferred.** `adapters::EnginePlayerSpawnGateway` (and the engine-touching definition of `CreatePlatform…`/`CreatePlayerSpawnGateway`) remains deferred to **Step 10** and is added only to `src/adapters/EngineAdapters.cpp` in the engine build. No engine gateway code exists in the tree before Step 10.
4. **Null throughout engine-free testing.** Every engine-free test — Steps 7, 8, 11 (composed wiring), 12, 13, 14 — links and injects `adapters::NullPlayerSpawnGateway` (delivered in Step 9). The test build never compiles or links `EnginePlayerSpawnGateway`. The null's deterministic synthetic `EntityId`s preserve replay in the test build.

*Net effect:* the numeric progression is unchanged; the only clarification is that the `IPlayerSpawnGateway` **header** is a Step-7 prerequisite (not a Step-9 discovery). This matches the frozen architecture's §9/§10 dependency edges exactly.

### 7.2 MessageRegistry allocation governance (clarifies Step 11)

The frozen plan states Step 11 registers "additive player-request `MessageRegistry` ids" in the Sprint-006 reserved data-id range, but does not pin the numeric sub-range or its governance. Clarification is warranted (it prevents id collisions during Step Specifications) and is added here **without** touching ADR-010, the wire format, or any packet layout — it governs *allocation only*, reusing the ranges Sprint-006 already froze (`kControlIdMin=0x0000`, `kControlIdMax=0x00FF`, `kDataIdMin=0x0100`).

- **Reserved numeric range.** Player-lifecycle **request** messages occupy the data-id sub-range **`[0x0100 .. 0x010F]`** (16 ids), the first block of the Sprint-006 data range (`>= 0x0100`). These are client→host *requests* only (e.g. a join/respawn request); the host owns every resulting transition (Host Authority). No player ids are placed in the control range `[0x0000..0x00FF]` (that range is Sprint-006's handshake/keepalive/session-control space and is not reused).
- **Ownership of the range.** The `[0x0100..0x010F]` block is **owned by Sprint-007** and its constants are declared in **one** location — a new `include/stalkermp/player/PlayerMessageIds.h` (e.g. `kMsgJoinRequest{0x0100}`, `kMsgRespawnRequest{0x0101}`, …), never inline at call sites. Sprint-006's `ProtocolConstants.h` is **not** modified. Later sprints that need player messages extend this Sprint-007 header additively within the block, or claim the next data sub-range if the block is exhausted.
- **Collision-prevention rules.** (a) All player ids are `IsDataId`-valid and disjoint from the control range and from any other subsystem's data block. (b) Ids are **additive only** — never renumbered or reused once assigned (mirrors Sprint-006's additive-id rule). (c) Registration goes through the existing `net::MessageRegistry`, which already rejects duplicate-id registration; the Step 11 wiring MUST treat a duplicate-registration outcome as a hard failure (surfaced, not ignored). (d) A single-source-of-truth header makes overlap detectable at compile time.
- **Future extensibility.** The data range above `0x010F` remains unallocated and available to Sprints 008+ (e.g. replication uses its own data sub-range). Reserving a small, named block now — rather than a scattered set — keeps future allocation collision-free and self-documenting. This introduces no new packet format and no protocol change; it only names constants within the frozen id space.

*Scope note:* this reserves ids and governance only. Defining message **payloads/handlers** remains Step 11's implementation detail (and is authored in the implementation phase, not here); no new wire structure is introduced.

### 7.3 Bootstrap dependency & ordering verification (adds Step 11 verification requirements)

The frozen plan's Step 11 already requires reverse-order teardown, an updated subscriber count, and the `kEntityRegistry < kPlayerLifecycle < kBubble` static assertion, but it does not enumerate the five ordering verifications as explicit acceptance items. These are added here as **verification requirements within the existing Step 11 scope** — they change no service ownership, no dependency edge, and no Bootstrap design; they only make the required checks explicit for the Step-11 specification.

Step 11 verification MUST explicitly require:

1. **Dependency-graph validation.** `PlayerManagerService.Dependencies()` resolves to already-registered services `{World, EntityRegistry, BubbleManager, AlifeTransition, Networking}` with **no cycle** and no missing edge; the `ServiceRegistry` topological order places `PlayerManagerService` after all five (edges are ordering-only, const-reference, per architecture §9/§12).
2. **Initialization-order verification.** `InitializeAll()` constructs/initializes `PlayerManagerService` **after** its five dependencies; the `net::Session` subscription and the `IPlayerPositionSource` feed swap occur inside `PlayerManagerService::Initialize` (i.e., after those services are up), never earlier.
3. **Shutdown-order verification.** `ShutdownAll()` tears down in **strict reverse** order: `PlayerManagerService` unsubscribes from the `FrameDispatcher` and from `net::Session`, and despawns owned entities via the gateway, **before** the Sprint-006 services shut down — leaving no orphan record, mapping, subscription, or entity.
4. **FrameDispatcher subscription-order verification.** After wiring, the frame subscriber set is exactly `World(100) → EntityRegistry(200) → PlayerLifecycle(250) → Bubble(300) → AlifeTransition(350) → Networking(900)`; `BootstrapTests` asserts the updated subscriber **count** and that Player ticks strictly between EntityRegistry and Bubble (`static_assert(kEntityRegistry < kPlayerLifecycle && kPlayerLifecycle < kBubble)`).
5. **Session-observer registration ordering.** `PlayerManagerService` registers as a `net::ISessionObserver` during its `Initialize` (after `net::Session` exists) and **deregisters before** `net::Session` is torn down; no `OnMemberJoined/Left` callback can fire before registration or after deregistration. The observer remains **enqueue-only** (per §7.1 / architecture AD-3), so this ordering never mutates simulation from the networking tick.

*Net effect:* these are additive acceptance/verification items for the already-planned Step 11; they introduce no new step, no ownership change, and no dependency change.

### 7.4 Summary of disposition

| Observation | Disposition |
|---|---|
| 1 — Gateway interface sequencing | **Clarified** (§7.1): four binding rules; interface header is a Step-7 prerequisite; order unchanged. |
| 2 — MessageRegistry allocation | **Clarified** (§7.2): reserve `[0x0100..0x010F]` in a single Sprint-007 header; additive, collision-checked; no protocol change. |
| 3 — Bootstrap dependency verification | **Clarified** (§7.3): five explicit Step-11 verification requirements added within existing scope. |

No architecture, ADR, scope, or implementation order was altered by this clarification pass. The plan remains frozen; Step Specifications may begin.

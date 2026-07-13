# Sprint-007 · Step 07 — Player Manager (Drain + Apply; Transactional Join/Reclaim) — Implementation Specification

- **Status:** Step Specification (pre-implementation). Derives exclusively from the frozen Architecture (§8/§10/§18, FR-1/4/6/12, §13), the Implementation Plan (Step 7 + its sequencing note), and the frozen Implementation Plan Clarifications §7.1. No architecture/ADR/scope/order change.
- **Governance:** ADR-007; ADR-008 (online/offline reused, not re-implemented); ADR-011. Host Authority. One Engine Boundary / One Platform Boundary preserved.
- **Nature:** The engine-free core applying queued deltas through the state machine and registry deterministically, requesting spawn/despawn via the **abstract** `IPlayerSpawnGateway&` (null in tests).

---

## 1. Step objective

Deliver `player::PlayerManager`: drain the `PlayerDeltaQueue` at a tick and apply join/suspend/reclaim/death/respawn transactionally against `PlayerRegistry` (via the Step-5 state machine), request entity materialization/removal through an injected abstract `IPlayerSpawnGateway&`, and expose the read-only surface (`Find*`, `Statistics`, `ValidateConsistency`) plus the data backing the position feed. All mutation happens here at the manager's tick — never in the observer.

## 2. Scope

In scope: (a) per the Clarifications §7.1 rule 1, land `include/stalkermp/player/IPlayerSpawnGateway.h` (interface **declaration only**) at the start of this step so the manager can depend on it; (b) create `include/stalkermp/player/PlayerManager.h` + `src/player/PlayerManager.cpp`: `RequestJoin`, `ApplyPendingDeltas(tick)`, `RequestRespawn`, `NotifyDeath`, read-only accessors; transactional join (rollback on spawn failure); reconnect via `net::Session::TryReclaim`. Extend `PlayerTests.cpp` (against `NullPlayerSpawnGateway`, delivered standalone in Step 9).

## 3. Explicit out-of-scope items

The standalone `NullPlayerSpawnGateway`/factory/adapters header packaging (Step 9) and `EnginePlayerSpawnGateway` (Step 10) — only the **interface header** is introduced here; position source (Step 8); service subscription / `tick_order` / Bootstrap / message registration (Step 11); diagnostics population (Step 12); validation hardening (Step 13); integration (Step 14). No online/offline switching (reused from Sprint-005 via the existing pipeline — the manager only makes the player eligible, FR-4). No engine/platform contact (the gateway is abstract here). No `MessageId` registration.

## 4. Dependencies

Steps 01–06; the `IPlayerSpawnGateway` interface (introduced at the start of this step per §7.1); Sprint-006 `net::Session` (`TryReclaim`, `Contains`, `Members`), `net::ConnectionId`; Sprint-003 `world::EntityRegistry` (registration, const-ref, consumed — the actual registration call is behind the gateway/adapter; the manager records the returned `EntityId`); Sprint-001 core.

## 5. Files to create

- `include/stalkermp/player/IPlayerSpawnGateway.h` — abstract seam: `Spawn(profile, position) -> Expected<EntityId>`, `Despawn(EntityId) -> outcome` (declaration only; no impl).
- `include/stalkermp/player/PlayerManager.h` + `src/player/PlayerManager.cpp`.

## 6. Files to modify

- Both vcxprojs (add `PlayerManager.cpp` + headers); `tests/PlayerTests.cpp`. (The null gateway TU is added to the test project in Step 9; if the manager tests need it earlier, the null is delivered at Step 9 and this step's tests link it — see the Plan Step-7 sequencing note.)

## 7. Public interfaces / types

Class `PlayerManager` (ctor injects `PlayerConfiguration`, `IPlayerSpawnGateway&`, `net::Session&`, and a const `world::EntityRegistry&` where needed):

- `[[nodiscard]] JoinOutcome RequestJoin(net::ConnectionId, const PlayerProfile&, std::uint64_t tick);`
- `void ApplyPendingDeltas(PlayerDeltaQueue&, std::uint64_t tick);`
- `[[nodiscard]] ReconnectOutcome ApplyReclaim(net::ConnectionId, std::uint64_t reconnectToken, std::uint64_t tick);`
- `[[nodiscard]] SpawnOutcome RequestRespawn(PlayerId, std::uint64_t tick);`
- `[[nodiscard]] JoinOutcome NotifyDeath(PlayerId, std::uint64_t tick);`
- Read-only: `Find*` (delegated to the registry), `Statistics()`, `ValidateConsistency()`, and `ActivePlayerPositions()` (data backing Step-8's feed, ascending, positions only).
- `PlayerProfile` — an engine-free character-profile handle value (opaque id/token; **not** an engine type). Defined minimally here (a value struct) since the gateway seam consumes it; no engine profile is referenced.

## 8. Internal components

Owns a `PlayerRegistry` (Steps 3–4). Holds non-owning `IPlayerSpawnGateway&`, `net::Session&`, const `world::EntityRegistry&`. Uses `PlayerLifecycle` (Step 5) for transitions. A file-local transactional-join helper (allocate → spawn → on failure roll back).

## 9. Responsibilities

Drain the queue in fixed order and apply each delta: a **Joined** delta → transactional join (validate capacity/duplicate → `Allocate` → `Spawn` via gateway → on success `Insert` record with mapping + published position; on `Spawn` failure roll back the `PlayerId`, insert nothing); a **Left** delta → suspend (retain) via the state machine, clear the connection binding, keep the record. `RequestRespawn`/`NotifyDeath` drive the corresponding transitions. `ApplyReclaim` calls `net::Session::TryReclaim(token)`; on success rebind the **existing** `PlayerId`/`EntityId` to the new `ConnectionId` (never a new entity), transition Suspended→Active. Maintain `Statistics` counters. Publish `ActivePlayerPositions()`. The manager decides every transition (Host Authority); client input is request-only.

## 10. Data structures

Reuses `PlayerRecord`/`PlayerMappingView`/outcomes (Step 1), `PlayerSessionDelta` (Step 6). Introduces `PlayerProfile` (engine-free value). No engine/OS type.

## 11. Invariants

All mutation occurs here at the manager tick, never in the observer (AD-3); join is transactional — failed `Spawn` leaves no orphan `PlayerId`/mapping/record; reconnect rebinds to the existing `PlayerId`/`EntityId` via `TryReclaim` (never a duplicate entity — FR-6); Host Authority (requests only); the manager depends only on the abstract `IPlayerSpawnGateway&` (Clarifications §7.1 rule 2), names no concrete gateway, holds no engine type; Entity Registry / Session ownership unchanged; single-threaded.

## 12. Ownership rules

`PlayerManager` **owns** the `PlayerRegistry`. It does not own the gateway (injected `&`), the `Session` (Sprint-006), or the entity (Entity Registry / engine). The `PlayerDeltaQueue` is owned by the manager (passed to `ApplyPendingDeltas`, or held as a member fed by the Step-6 observer — the observer remains non-owning). Positions are produced, not owned by the Bubble.

## 13. Naming conventions

Namespace `stalkermp::player`; `PlayerManager`; PascalCase methods; `m_` members; `#pragma once`. Outcomes via the Step-01 enums; messages via `common::Format`.

## 14. Step-specific constraints

Engine-free (abstract gateway only); no online/offline switching; no position-source binding (Step 8 defines the source; Step 11 binds it); no `Session`/`FrameDispatcher` subscription (Step 11); no `MessageId`. Reuses Sprint-005 for online/offline strictly by *not* switching here.

## 15. Determinism constraints

Deltas applied in fixed order at the fixed tick; ascending `PlayerId`; deterministic reconnect token (Sprint-006 `MintToken`); null-gateway ids deterministic (Step 9). Identical (delta+tick) sequence → identical registry state, entity registrations, and feed data — the Step-7 replay property (verified here).

## 16. Engine-boundary constraints

Engine-free: the gateway is abstract; no engine header/type/pointer in `PlayerManager`. One Engine Boundary preserved.

## 17. Platform-boundary constraints

OS-free.

## 18. Acceptance criteria

Join/suspend/reclaim/death/respawn apply correctly against the null gateway; transactional rollback on `Spawn` failure (no orphan); no duplicate entity on reclaim or concurrent join; determinism/replay test passes; manager depends only on the abstract gateway; engine/OS-free; ADR-007-clean; 238/238 green; zero new warnings.

## 19. Test requirements

Full lifecycle flows against `NullPlayerSpawnGateway`; transactional-rollback test (gateway returns failure → no orphan); duplicate-join and reclaim-no-duplicate tests; `ApplyReclaim` token success/failure via `Session::TryReclaim`; determinism/replay test (same sequence twice → identical registry + registrations + `ActivePlayerPositions()`); Host-Authority (client request rejected when invalid).

## 20. Completion checklist

- [ ] `IPlayerSpawnGateway.h` interface (declaration only) landed at step start.
- [ ] `PlayerManager.h`/`.cpp` created (drain/apply, transactional join, `TryReclaim` reconnect, requests, read-only surface, `ActivePlayerPositions`).
- [ ] Depends only on the abstract gateway; no concrete/engine type.
- [ ] Transactional rollback; no duplicate entity; Host Authority.
- [ ] Determinism/replay test green.
- [ ] vcxprojs updated; manager tests against the null gateway.
- [ ] Engine/OS-free; ADR-007 clean; single-threaded.
- [ ] 238/238 green on GCC + MSVC; zero new warnings.

## 21. Stop condition

Stop when all lifecycle flows apply deterministically against the null gateway and the replay test passes. No service, engine adapter, position-source binding, or wiring. Do not begin Step 8.

## 22. Self-review

- **Architecture compliance.** Implements §8/§10/§18 and FR-1/4/6/12; online/offline reused (FR-4, AD-5); reconnect via the Sprint-006 token (AD-7). Compliant.
- **ADR compliance.** ADR-007 clean; ADR-008 untouched (no switching added); ADR-011 single-threaded. Others untouched.
- **Dependency correctness.** Steps 01–06 + the interface header (per §7.1) + Sprint-003/006 seams. The 7↔9 edge is handled by landing the interface here (Clarifications §7.1). Correct.
- **Scope isolation.** Abstract gateway only; no switching/binding/subscription. Isolated.
- **Determinism.** Fixed-order drain, ascending ids, deterministic token/null ids. Sound.
- **Engine/Platform boundary.** Abstract gateway; no engine/OS contact. Preserved.
- **Ownership.** Owns registry; gateway/session/entity not owned. Correct.
- *Ambiguity resolved:* `PlayerProfile` is defined as an engine-free opaque value (not an engine profile type), so the manager and seam stay engine-free; the engine adapter (Step 10) maps it to a real profile behind the boundary. Consistent with §15/AD-6. No other issue.

## 23. Step-07 Specification Freeze

Sprint-007 Step-07 (Player Manager) Specification is **FROZEN** (2026-07-11). Derived exclusively from the frozen architecture (§8/§10/§18, FR-1/4/6/12, §13), plan (Step 7 + sequencing note), and Clarifications §7.1; no architecture/ADR/order/scope change. Self-review passed; ambiguity resolved within the step. Boundaries, ADR-008 reuse, Host Authority, determinism, ADR-007, ownership preserved. Ready for implementation.

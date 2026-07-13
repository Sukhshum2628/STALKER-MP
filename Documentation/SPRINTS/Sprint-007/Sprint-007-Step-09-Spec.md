# Sprint-007 · Step 09 — Spawn Gateway Interface + Null — Implementation Specification

- **Status:** Step Specification (pre-implementation). Derives exclusively from the frozen Architecture (§9/§10/§15, AD-6, Sprint-005 `IAlifeSwitchGateway` precedent) and Implementation Plan (Step 9 + Clarifications §7.1). No architecture/ADR/scope/order change.
- **Governance:** ADR-007. One Engine Boundary preserved (no engine code here). One Platform Boundary preserved.
- **Nature:** The single engine-free spawn seam + its inert test counterpart + factory declaration. **No engine code.**

---

## 1. Step objective

Package the `IPlayerSpawnGateway` seam (its declaration lands at the start of Step 7 per §7.1; this step finalizes its home), deliver `adapters::NullPlayerSpawnGateway` (inert, deterministic) and the `adapters::CreatePlayerSpawnGateway()` factory **declaration**, so the engine-free test build links only the null. The concrete engine adapter is Step 10.

## 2. Scope

In scope: finalize `include/stalkermp/player/IPlayerSpawnGateway.h` (interface); create `include/stalkermp/adapters/PlayerSpawnGateway.h` (factory declaration); create `tests/support/NullPlayerSpawnGateway.cpp` (the inert impl + test-build factory definition). Wire the null TU into the test project. Extend/confirm the Step-7 manager tests run against the null.

## 3. Explicit out-of-scope items

`EnginePlayerSpawnGateway` + the engine-build factory definition (Step 10); service wiring / Bootstrap / factory selection (Step 11); diagnostics (Step 12); validation (Step 13); integration (Step 14). **No engine header, no engine type** here; no online/offline switching (Sprint-005 owns it); no `MessageId`.

## 4. Dependencies

Step 01 (`PlayerProfile`, `PlayerId`/`EntityId` values via Step-7 `PlayerManager.h` / `PlayerTypes.h`), Step 07 (interface consumer). Sprint-003 `world::EntityId`; Sprint-004 `world::PlayerPosition`. Sprint-006 `net::MockTransport` not required here.

## 5. Files to create

- `include/stalkermp/player/IPlayerSpawnGateway.h` — if not already present from Step 7, this is its canonical home (interface only).
- `include/stalkermp/adapters/PlayerSpawnGateway.h` — `[[nodiscard]] std::unique_ptr<player::IPlayerSpawnGateway> CreatePlayerSpawnGateway();` (declaration).
- `tests/support/NullPlayerSpawnGateway.cpp` — `adapters::NullPlayerSpawnGateway` + the test-build `CreatePlayerSpawnGateway` definition returning it.

## 6. Files to modify

- `Multiplayer/tests/xrMP_Tests.vcxproj` — add `support\NullPlayerSpawnGateway.cpp` (ClCompile). (`xrMP.vcxproj` gains the engine adapter only in Step 10.)
- `tests/PlayerTests.cpp` — confirm manager lifecycle tests inject the null.

## 7. Public interfaces / types

- `class IPlayerSpawnGateway { virtual ~IPlayerSpawnGateway() = default; [[nodiscard]] virtual core::Expected<world::EntityId> Spawn(const PlayerProfile&, const world::PlayerPosition&) = 0; [[nodiscard]] virtual SpawnOutcome Despawn(world::EntityId) = 0; };` — engine-free; names no engine call/type (mirrors `IAlifeSwitchGateway`).
- `class NullPlayerSpawnGateway final : public IPlayerSpawnGateway` — returns deterministic synthetic ascending `EntityId`s on `Spawn`; `Despawn` → `Spawned`/benign outcome; no engine, no OS.
- `std::unique_ptr<IPlayerSpawnGateway> adapters::CreatePlayerSpawnGateway();` — declared here; defined per build (null in test, engine in Step 10).

## 8. Internal components

The null holds a monotonic synthetic-id counter (deterministic). No other state. The factory is a free function in `namespace stalkermp::adapters`.

## 9. Responsibilities

Provide the abstract materialize/remove seam and a fully deterministic, engine/OS-free implementation for the test build so every engine-free step (7, 8, 11, 12, 13, 14) links the null and never the engine adapter (Clarifications §7.1 rule 4). Preserve One Engine Boundary by containing no engine contact.

## 10. Data structures

Reuses `PlayerProfile` (Step 7, engine-free value), `world::EntityId`, `world::PlayerPosition`, `SpawnOutcome` (Step 1). Null: a `std::uint32_t` synthetic-id counter.

## 11. Invariants

The seam exposes no engine type and names no engine call (AD-6); the null is deterministic (ascending synthetic ids, replay-stable) and engine/OS-free; the test build links only the null; `Spawn`/`Despawn` return value outcomes (ADR-007), never throw.

## 12. Ownership rules

The gateway instance is owned by the Runtime/composition root (from Step 11) via `unique_ptr`; here the null is constructed by the factory. `PlayerManager` holds a non-owning `IPlayerSpawnGateway&`. No engine ownership.

## 13. Naming conventions

`IPlayerSpawnGateway` (in `player`); `NullPlayerSpawnGateway` + `CreatePlayerSpawnGateway` (in `adapters`); `PlayerSpawnGateway.h` factory header mirrors Sprint-006 `PlatformTransport.h`. `#pragma once`.

## 14. Step-specific constraints

Interface + null + factory declaration only; **no engine code**; the test build gains the null TU; `xrMP.vcxproj` is not given any engine gateway TU yet (Step 10). Engine/OS-free; no `MessageId`; no switching.

## 15. Determinism constraints

Null synthetic ids ascending and deterministic → the engine-free test build stays fully replay-stable (matching the Step-7 replay property). No wall-clock, no address-based ids.

## 16. Engine-boundary constraints

Engine-free: **no** engine header/type anywhere in this step. One Engine Boundary intact; the sole engine TU is deferred to Step 10.

## 17. Platform-boundary constraints

OS-free.

## 18. Acceptance criteria

Manager (Step 7) compiles/tests against `NullPlayerSpawnGateway`; the interface names no engine type; the factory is declared (engine impl deferred); the test build links only the null; engine/OS-free; ADR-007-clean; 238/238 green; zero new warnings.

## 19. Test requirements

Manager lifecycle tests run against the null; `Spawn` returns ascending deterministic ids; `Despawn` benign; null is engine/OS-free (include-tree check: no engine header reachable from the test build for this TU).

## 20. Completion checklist

- [ ] `IPlayerSpawnGateway.h` finalized (engine-free, no engine type).
- [ ] `adapters/PlayerSpawnGateway.h` factory declaration.
- [ ] `tests/support/NullPlayerSpawnGateway.cpp` (inert, deterministic) + test-build factory definition.
- [ ] Test vcxproj links the null; `xrMP.vcxproj` unchanged (no engine TU yet).
- [ ] Engine/OS-free; ADR-007 clean; deterministic null ids.
- [ ] 238/238 green on GCC + MSVC; zero new warnings.

## 21. Stop condition

Stop when the interface + null + factory declaration exist and back the test build. No engine adapter. Do not begin Step 10.

## 22. Self-review

- **Architecture compliance.** Matches §9/§10/§15 and AD-6; mirrors the Sprint-005 `IAlifeSwitchGateway`/null precedent. Compliant.
- **ADR compliance.** ADR-007 clean; ADR-008 untouched (no switching); One Engine Boundary preserved (no engine code).
- **Dependency correctness.** Steps 01/07 + reused id types. The §7.1 sequencing (interface at Step 7, null/factory here) is honored. Correct.
- **Scope isolation.** Interface + null + factory decl only; no engine code. Isolated.
- **Determinism.** Deterministic null ids. Sound.
- **Engine/Platform boundary.** No engine/OS contact. Preserved.
- **Ownership.** Gateway owned by composition root later; manager non-owning. Correct.
- *Ambiguity resolved:* the interface's canonical home is `include/stalkermp/player/IPlayerSpawnGateway.h` (declared at Step 7, finalized here) so there is exactly one declaration; the factory lives in `adapters` mirroring `PlatformTransport.h`. Consistent with §7.1 and the Sprint-006 factory precedent. No other issue.

## 23. Step-09 Specification Freeze

Sprint-007 Step-09 (Spawn Gateway Interface + Null) Specification is **FROZEN** (2026-07-11). Derived exclusively from the frozen architecture (§9/§10/§15, AD-6) and plan (Step 9 + Clarifications §7.1); no architecture/ADR/order/scope change. Self-review passed; ambiguity resolved within the step. One Engine/Platform Boundary, determinism, ADR-007, ownership preserved. Ready for implementation.

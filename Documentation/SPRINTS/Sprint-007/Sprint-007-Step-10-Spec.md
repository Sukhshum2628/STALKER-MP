# Sprint-007 · Step 10 — Engine Adapter Implementation — Implementation Specification

- **Status:** Step Specification (pre-implementation). Derives exclusively from the frozen Architecture (§15, AD-6, ADR-008 reuse, Sprint-005 `EngineAlifeSwitchGateway` precedent) and Implementation Plan (Step 10). No architecture/ADR/scope/order change.
- **Governance:** ADR-007 (`EngineAbi.props`), ADR-008 (no new online/offline mutation). **One Engine Boundary — the single engine-touching TU of Sprint-007.** One Platform Boundary preserved.
- **Nature:** The one engine TU materializing/removing the vanilla ALife player object behind `IPlayerSpawnGateway`.

---

## 1. Step objective

Deliver `adapters::EnginePlayerSpawnGateway` and the engine-build `CreatePlayerSpawnGateway()` definition inside `src/adapters/EngineAdapters.cpp`, materializing/removing a **vanilla** ALife player object via the engine's own creation path, on-demand id resolution, no retained engine pointer/cache — and **without** switching online/offline (that remains Sprint-005's pipeline).

## 2. Scope

In scope: implement `EnginePlayerSpawnGateway : player::IPlayerSpawnGateway` in `EngineAdapters.cpp` (`Spawn` → create vanilla ALife player object at position from profile → return its `EntityId`; `Despawn` → remove it), define the engine-build factory, add the engine TU to `xrMP.vcxproj` only. The test build continues linking `tests/support/NullPlayerSpawnGateway.cpp` unchanged.

## 3. Explicit out-of-scope items

Any online/offline switching (ADR-008 / Sprint-005 owns it — reused, not duplicated); service wiring / factory selection in Bootstrap (Step 11); diagnostics (Step 12); validation (Step 13); integration (Step 14). No change to `IPlayerSpawnGateway` or the null; no engine header outside `EngineAdapters.cpp`; no `MessageId`; no platform/socket code.

## 4. Dependencies

Step 09 (interface + factory declaration + null). Engine: the vanilla ALife object creation/removal path (behind the existing engine include roots scoped to `EngineAdapters.cpp`). Sprint-005 precedent (`EngineAlifeSwitchGateway`) for on-demand resolution discipline.

## 5. Files to create

None (extends existing `EngineAdapters.cpp`).

## 6. Files to modify

- `src/adapters/EngineAdapters.cpp` — add `EnginePlayerSpawnGateway` + engine-build `CreatePlayerSpawnGateway` definition.
- `include/stalkermp/adapters/PlayerSpawnGateway.h` — (already declares the factory; unchanged unless a doc note is added).
- `Multiplayer/xrMP.vcxproj` — ensure `EngineAdapters.cpp` remains the engine TU (already present); no new TU needed if the gateway lives in `EngineAdapters.cpp`. The **test** vcxproj is unchanged (still links the null).

## 7. Public interfaces / types

Implements `player::IPlayerSpawnGateway` (`Spawn`/`Despawn`) — no new public type. The factory `adapters::CreatePlayerSpawnGateway()` is defined here for the engine build (returns `std::make_unique<EnginePlayerSpawnGateway>()`).

## 8. Internal components

`EnginePlayerSpawnGateway` (anonymous namespace in `EngineAdapters.cpp`): maps `PlayerProfile` + `PlayerPosition` to a vanilla ALife creation call, resolves/creates the object on demand, returns its `EntityId`; `Despawn` removes by `EntityId` via the engine path. No retained engine pointer or id→object cache (Sprint-005 discipline).

## 9. Responsibilities

Materialize a persistent vanilla ALife player object (Preserve Before Replace / Reuse Engine Systems) and return its `EntityId`; remove it on `Despawn`. Report engine-unavailable / missing-object as value outcomes (`EngineUnavailable`/`EntityMissing`). Do **not** switch the object online/offline — the existing Bubble→Transition (Sprint-005) pipeline performs that when the player becomes eligible.

## 10. Data structures

Reuses `PlayerProfile`, `world::EntityId`, `world::PlayerPosition`, `SpawnOutcome`, `core::Expected` — no new value type. Engine types are confined to the `.cpp` body.

## 11. Invariants

Engine headers appear **only** in `EngineAdapters.cpp` (One Engine Boundary); no engine pointer/id cache retained (on-demand resolution); no online/offline mutation added (ADR-008 intact); vanilla creation only (no ALife edit); every fallible op is a value outcome (ADR-007, `EngineAbi.props`, C4530-clean); the test build never compiles this TU.

## 12. Ownership rules

The engine owns the real ALife object; the gateway owns no engine handle beyond the call. The gateway instance is owned by the composition root (`unique_ptr`, Step 11). `PlayerManager` remains non-owning.

## 13. Naming conventions

`EnginePlayerSpawnGateway` (anonymous namespace, `adapters`); factory `CreatePlayerSpawnGateway`. Matches the Sprint-005 `EngineAlifeSwitchGateway` naming.

## 14. Step-specific constraints

The only engine-touching step of Sprint-007; confined to `EngineAdapters.cpp`; no switching; no platform code; no change to the seam/null. `EngineAbi.props` applies (no exceptions/RTTI/iostream; C4530-clean).

## 15. Determinism constraints

The engine call is behind the seam; the engine-free core's determinism is unaffected and is verified via the null in the test build. Engine-side behavior (real object creation) is validated by Antigravity on Windows; the id returned is stored deterministically by the core.

## 16. Engine-boundary constraints

This step **is** the One Engine Boundary crossing for Sprint-007: engine headers only here; nothing above the seam sees an engine type. Verified by an include-tree check (no engine header reachable from `player::*` or the test build).

## 17. Platform-boundary constraints

OS-free: no socket/platform header. One Platform Boundary untouched.

## 18. Acceptance criteria

Engine build compiles under `EngineAbi.props` with the gateway; test build unaffected (null path, 238/238 green); no engine header leaks outside `EngineAdapters.cpp`; no online/offline switching added; ADR-008 intact.

## 19. Test requirements

Engine-free test build stays green via the null (no new engine dependency in the test build). Antigravity (Windows/MSVC) performs the engine compile under `EngineAbi.props` and the spawn/despawn smoke — this in-sandbox spec requires only that the null path and boundary checks hold; the engine smoke is Antigravity's, per the plan.

## 20. Completion checklist

- [ ] `EnginePlayerSpawnGateway` + engine-build `CreatePlayerSpawnGateway` in `EngineAdapters.cpp`.
- [ ] Vanilla creation/removal; on-demand resolution; no retained pointer/cache.
- [ ] No online/offline switching added (ADR-008 intact).
- [ ] No engine header outside `EngineAdapters.cpp`; test build unchanged (null).
- [ ] `EngineAbi.props` clean (no exceptions/RTTI/iostream; C4530-clean).
- [ ] Engine build compiles (Antigravity/MSVC); 238/238 test-build green.

## 21. Stop condition

Stop when the engine adapter compiles behind the seam and the test build (null) stays green. Not yet wired into Bootstrap. Do not begin Step 11.

## 22. Self-review

- **Architecture compliance.** Matches §15/AD-6; reuses vanilla ALife + Sprint-005 for switching (ADR-008 intact). Compliant.
- **ADR compliance.** ADR-007/`EngineAbi.props`; ADR-008 unbroken (no switching); One Engine Boundary preserved.
- **Dependency correctness.** Step 09 seam + engine path. Correct.
- **Scope isolation.** One engine TU; no switching/wiring/platform. Isolated.
- **Determinism.** Core determinism verified via null; engine behind seam. Sound.
- **Engine boundary.** Sole crossing, confined to `EngineAdapters.cpp`. Preserved.
- **Platform boundary.** No OS code. Preserved.
- **Ownership.** Engine owns object; gateway owns no handle; composition root owns instance. Correct.
- *Ambiguity resolved:* whether "spawn" switches the object online — resolved firmly to **no**: Step 10 only materializes/registers; the existing Bubble→Transition pipeline brings it online (FR-4/AD-5), so ADR-008 gains no new mutation path. Consistent with the frozen architecture. No other issue.

## 23. Step-10 Specification Freeze

Sprint-007 Step-10 (Engine Adapter Implementation) Specification is **FROZEN** (2026-07-11). Derived exclusively from the frozen architecture (§15, AD-6, ADR-008 reuse) and plan (Step 10); no architecture/ADR/order/scope change. Self-review passed; ambiguity resolved within the step. One Engine Boundary (sole crossing), One Platform Boundary, ADR-007/008, determinism, ownership preserved. Ready for implementation.

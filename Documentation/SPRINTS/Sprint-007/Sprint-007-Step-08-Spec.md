# Sprint-007 · Step 08 — Position Source — Implementation Specification

- **Status:** Step Specification (pre-implementation). Derives exclusively from the frozen Architecture (§9, §15 invariant B11, §26 Sprint-004 seam) and Implementation Plan (Step 8). No architecture/ADR/scope/order change.
- **Governance:** ADR-007. One Engine Boundary / One Platform Boundary preserved. Bubble Manager unchanged.
- **Nature:** Feed player positions to the Bubble via the **existing frozen** `world::IPlayerPositionSource`, positions only.

---

## 1. Step objective

Deliver `player::NetworkedPlayerPositionSource` implementing the frozen `world::IPlayerPositionSource`, fed by `PlayerManager`, returning an ascending, positions-only snapshot the Bubble Manager consumes without modification.

## 2. Scope

In scope: create `include/stalkermp/player/NetworkedPlayerPositionSource.h` (+ `.cpp`) implementing `world::IPlayerPositionSource::ActivePlayers()` over the manager's active-player positions. Extend `PlayerTests.cpp`. No Bubble modification; binding into the Bubble is Step 11.

## 3. Explicit out-of-scope items

Binding the source into the Bubble Manager (Step 11); service/tick/Bootstrap (Step 11); gateway (Steps 9–10); diagnostics (Step 12); validation hardening (Step 13); integration (Step 14). No modification to `IPlayerPositionSource` or the Bubble Manager; no connection/session/lifecycle leak into World; no engine/platform/networking contact; no `MessageId`.

## 4. Dependencies

Steps 01–07 (the manager's `ActivePlayerPositions()`); Sprint-004 `world::IPlayerPositionSource` + `world::PlayerPosition`/`PlayerId`. Sprint-001 core.

## 5. Files to create

- `include/stalkermp/player/NetworkedPlayerPositionSource.h` + `src/player/NetworkedPlayerPositionSource.cpp`.

## 6. Files to modify

- Both vcxprojs (add the `.cpp`/header); `tests/PlayerTests.cpp`.

## 7. Public interfaces / types

- `class NetworkedPlayerPositionSource final : public world::IPlayerPositionSource` — ctor takes a const `PlayerManager&` (non-owning); `[[nodiscard]] std::vector<world::PlayerPosition> ActivePlayers() const override;` (ascending `PlayerId`, positions only).

## 8. Internal components

A non-owning `const PlayerManager&`; the override delegates to the manager's positions-only snapshot. No stored state.

## 9. Responsibilities

Return an immutable ascending snapshot of active players' positions (from the manager), exposing **only** `world::PlayerPosition` (id + position + lastUpdateTick). Never expose connection/session/lifecycle state. The Bubble Manager and its interface remain untouched.

## 10. Data structures

Reuses `world::PlayerPosition` (Sprint-004). No new type.

## 11. Invariants

Positions only — no connection/session/lifecycle field crosses into World (invariant B11); ascending `PlayerId` (B9); the snapshot is a pure function of manager state; the source is non-owning; the frozen `IPlayerPositionSource` signature is implemented exactly.

## 12. Ownership rules

Non-owning `const PlayerManager&`; owns nothing. The manager owns the underlying data; the Bubble owns its consumption. No ownership change to World/Bubble.

## 13. Naming conventions

Namespace `stalkermp::player`; `NetworkedPlayerPositionSource`; `#pragma once`; `override` on `ActivePlayers`.

## 14. Step-specific constraints

Implements the existing interface only; no Bubble change; no binding; positions-only. Engine/OS-free; no `MessageId`.

## 15. Determinism constraints

Snapshot is a deterministic, ascending, side-effect-free function of manager state; identical manager state → identical snapshot. No wall-clock introduced (uses `PlayerPosition.lastUpdateTick`).

## 16. Engine-boundary constraints

Engine-free (World seam is engine-free).

## 17. Platform-boundary constraints

OS-free.

## 18. Acceptance criteria

Implements `world::IPlayerPositionSource` exactly; only positions cross; ascending order; no non-position field exposed; Bubble Manager unchanged; engine/OS-free; ADR-007-clean; 238/238 green; zero new warnings.

## 19. Test requirements

Feed known manager state → assert snapshot content + ascending order; assert only `PlayerPosition` fields are present (no leaked state); empty-state → empty snapshot. Engine/OS-free.

## 20. Completion checklist

- [ ] `NetworkedPlayerPositionSource.h`/`.cpp` implementing the frozen interface.
- [ ] Positions-only, ascending; no leaked connection/session state.
- [ ] Bubble Manager / `IPlayerPositionSource` untouched.
- [ ] vcxprojs updated; tests added.
- [ ] Engine/OS-free; ADR-007 clean; deterministic.
- [ ] 238/238 green on GCC + MSVC; zero new warnings.

## 21. Stop condition

Stop when the source produces correct snapshots; not yet bound into the Bubble (Step 11). Do not begin Step 9.

## 22. Self-review

- **Architecture compliance.** Implements the §9 `NetworkedPlayerPositionSource` against the Sprint-004 seam (§26); positions-only per §15/B11. Compliant.
- **ADR compliance.** ADR-007 clean; others untouched.
- **Dependency correctness.** Steps 01–07 + Sprint-004 seam. Correct.
- **Scope isolation.** Implements interface only; no binding/Bubble change. Isolated.
- **Determinism.** Pure ascending snapshot. Sound.
- **Engine/Platform boundary.** No contact. Preserved.
- **Ownership.** Non-owning; owns nothing. Correct.
- *Ambiguity resolved:* the source reads the manager's already-published `ActivePlayerPositions()` (Step 7) rather than recomputing activation — keeping positions-only and avoiding any Bubble-logic duplication. Consistent with §8/§15. No other issue.

## 23. Step-08 Specification Freeze

Sprint-007 Step-08 (Position Source) Specification is **FROZEN** (2026-07-11). Derived exclusively from the frozen architecture (§9/§15/§26) and plan (Step 8); no architecture/ADR/order/scope change. Self-review passed; ambiguity resolved within the step. Boundaries, invariant B11, determinism, ADR-007, ownership preserved. Ready for implementation.

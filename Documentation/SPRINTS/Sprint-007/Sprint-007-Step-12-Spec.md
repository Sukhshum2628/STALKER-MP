# Sprint-007 · Step 12 — Diagnostics — Implementation Specification

- **Status:** Step Specification (pre-implementation). Derives exclusively from the frozen Architecture (§20) and Implementation Plan (Step 12). No architecture/ADR/scope/order change.
- **Governance:** ADR-007 (iostream-free, `common::Format`). One Engine Boundary / One Platform Boundary preserved.
- **Nature:** Read-only, replay-stable observability over the manager/registry.

---

## 1. Step objective

Deliver `player::PlayerDiagnostics`: read-only `Statistics`, `DescribeState`, `DescribePlayer`, `DumpPlayers`, and an extended `ValidateConsistency`, all tick-derived and deterministic, mirroring the Sprint-005/006 diagnostics precedent.

## 2. Scope

In scope: create `include/stalkermp/player/PlayerDiagnostics.h` (+ `.cpp`) with read-only accessors over `PlayerManager`/`PlayerRegistry`. Extend `PlayerTests.cpp` with statistics/dump/consistency tests (incl. negatives).

## 3. Explicit out-of-scope items

Validation hardening of the subsystem's negative surface (Step 13); integration/docs (Step 14). No mutation; no engine/platform/networking contact; no `MessageId`; bandwidth/latency fields reserved (not populated).

## 4. Dependencies

Steps 01–11. Sprint-001 `common::Format`. Read-only access to `PlayerManager`/`PlayerRegistry`.

## 5. Files to create

- `include/stalkermp/player/PlayerDiagnostics.h` + `src/player/PlayerDiagnostics.cpp`.

## 6. Files to modify

- Both vcxprojs; `tests/PlayerTests.cpp`.

## 7. Public interfaces / types

- `class PlayerDiagnostics` (ctor: const `PlayerManager&`): `Statistics() -> PlayerStatistics`; `DescribeState() -> std::string`; `DescribePlayer(PlayerId) -> std::string`; `DumpPlayers() -> std::string` (ascending); `ValidateConsistency() -> PlayerRegistryConsistency` (delegates + extends Step-4 checks).

## 8. Internal components

Non-owning `const PlayerManager&`; formatting via `common::Format`. No stored state.

## 9. Responsibilities

Compute tick-derived tallies (connected/suspended/dead, respawns, deaths, reconnects, join tick, average session duration in ticks); render deterministic state/player dumps (ascending); run the extended consistency report (registry ordering, mapping bijection, lifecycle-state legality, position-feed↔registry agreement, no orphan/duplicate). All read-only.

## 10. Data structures

Reuses `PlayerStatistics` (Step 1) and `PlayerRegistryConsistency` (Step 4). No new value type; bandwidth/latency remain reserved fields (unpopulated).

## 11. Invariants

Read-only, deterministic, iostream-free; all facts tick-derived (replay-stable); `ValidateConsistency` catches injected corruption (duplicate mapping, orphan, ordering break, illegal state, feed mismatch).

## 12. Ownership rules

Non-owning; owns nothing. Reads manager/registry state; produces value/string reports.

## 13. Naming conventions

`PlayerDiagnostics`; `Statistics`/`DescribeState`/`DescribePlayer`/`DumpPlayers`/`ValidateConsistency` (Sprint-005/006 precedent); `#pragma once`; `common::Format` only.

## 14. Step-specific constraints

Read-only; no mutation, no engine/platform/networking, no `MessageId`; reserved fields stay unpopulated.

## 15. Determinism constraints

Tick-derived facts + ascending dumps → deterministic, replay-stable, toolchain-independent. No wall-clock, no float, no hash-order.

## 16. Engine-boundary constraints

Engine-free.

## 17. Platform-boundary constraints

OS-free.

## 18. Acceptance criteria

All accessors read-only; statistics math correct; dumps ascending/deterministic; `ValidateConsistency` catches each injected corruption; engine/OS-free; ADR-007-clean; 238/238 green; zero new warnings.

## 19. Test requirements

Statistics tallies (incl. average session duration in ticks); dump ordering; a negative test per consistency check (duplicate mapping, orphan, ordering break, illegal state, feed mismatch) → `IsHealthy()` false.

## 20. Completion checklist

- [ ] `PlayerDiagnostics.h`/`.cpp` (read-only accessors + extended `ValidateConsistency`).
- [ ] Tick-derived statistics; ascending dumps; reserved fields unpopulated.
- [ ] Negative consistency tests included.
- [ ] Engine/OS-free; ADR-007 clean; deterministic.
- [ ] 238/238 green on GCC + MSVC; zero new warnings.

## 21. Stop condition

Stop when diagnostics report correctly and mutate nothing. Do not begin Step 13.

## 22. Self-review

- **Architecture compliance.** Matches §20 exactly (accessors, reserved fields, consistency scope). Compliant.
- **ADR compliance.** ADR-007 clean (iostream-free); others untouched.
- **Dependency correctness.** Steps 01–11 + `common::Format`. Correct.
- **Scope isolation.** Read-only; no mutation/validation-hardening. Isolated.
- **Determinism.** Tick-derived, ascending. Sound.
- **Engine/Platform boundary.** No contact. Preserved.
- **Ownership.** Non-owning. Correct.
- *Ambiguity resolved:* "average session duration" is defined in ticks (join→leave tick delta averaged), consistent with the tick-derived rule; no wall-clock. No other issue.

## 23. Step-12 Specification Freeze

Sprint-007 Step-12 (Diagnostics) Specification is **FROZEN** (2026-07-11). Derived exclusively from the frozen architecture (§20) and plan (Step 12); no architecture/ADR/order/scope change. Self-review passed; ambiguity resolved within the step. Boundaries, determinism, ADR-007, ownership preserved. Ready for implementation.

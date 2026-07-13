# Sprint-007 · Step 14 — Integration Cleanup — Implementation Specification

- **Status:** Step Specification (pre-implementation). Derives exclusively from the frozen Architecture (§21 testing strategy, §22 extension points) and Implementation Plan (Step 14). No architecture/ADR/scope/order change.
- **Governance:** ADR-007/008/009/010/011 preserved. One Engine Boundary / One Platform Boundary preserved.
- **Nature:** End-to-end integration behind seams + subsystem documentation. **No behavior change.**

---

## 1. Step objective

Prove the composed subsystem end-to-end behind seams (player appears in the Entity Registry; Bubble includes the player position; session mapping correct; disconnect preserves the record; reconnect restores the same entity) and author `Multiplayer/docs/PlayerLifecycle.md`, with a final consistency sweep.

## 2. Scope

In scope: extend `tests/PlayerTests.cpp`/`tests/BootstrapTests.cpp` with the six integration checks (via the composed null-gateway/loopback stack); create `Multiplayer/docs/PlayerLifecycle.md`; run a consistency sweep. No `player::*` interface or behavior change.

## 3. Explicit out-of-scope items

Sprint closure/status-doc updates (Step 15); any new behavior/interface/type; any engine/platform/networking change; any `MessageId`; any replication/persistence/snapshot content.

## 4. Dependencies

Steps 01–13 (verified). The composed stack: `PlayerManagerService` + null gateway + loopback/mock `Session` + Entity Registry + Bubble.

## 5. Files to create

- `Multiplayer/docs/PlayerLifecycle.md` — the subsystem doc (mirrors `TransitionLayer.md`/`Networking.md` structure: purpose, ownership, components, per-tick pass at 250, boundaries, ADRs, extension points).

## 6. Files to modify

- `tests/PlayerTests.cpp`, `tests/BootstrapTests.cpp` — integration checks. No `player::*` interface change.

## 7. Public interfaces / types

None added or changed. Integration exercises existing seams (`ISessionObserver`, `IPlayerPositionSource`, `IPlayerSpawnGateway` null, Entity Registry).

## 8. Internal components

None added. Test scaffolding only (composed stack fixtures).

## 9. Responsibilities

Demonstrate the full join→spawn→death→respawn→disconnect→reconnect flow through the composed stack; assert the six integration properties; document the delivered subsystem accurately (components, tick order, boundaries, ADR-007/008/009/010/011, extension points for Sprints 008–012); confirm the consistency sweep is clean.

## 10. Data structures

None new. Reuses the subsystem's types in tests/doc.

## 11. Invariants

All frozen boundaries intact (no engine/OS header outside the established TUs); ownership unchanged; the doc matches the delivered code exactly (no aspirational claim); integration flows replay identically.

## 12. Ownership rules

Unchanged. Documentation + tests only.

## 13. Naming conventions

Doc `Multiplayer/docs/PlayerLifecycle.md`; test names follow existing conventions.

## 14. Step-specific constraints

Integration + docs only; no behavior/interface change; engine/OS-free test build (engine path is Antigravity's per Step 10); no `MessageId`.

## 15. Determinism constraints

Integration flows replay identically (same composed input+tick sequence → same observable state).

## 16. Engine-boundary constraints

Engine-free test build; the doc records that the sole engine TU is `EnginePlayerSpawnGateway` (Step 10).

## 17. Platform-boundary constraints

OS-free.

## 18. Acceptance criteria

The six integration checks pass on GCC + MSVC; the doc matches delivered code; consistency sweep clean; no behavior/interface change; 238/238 (+ integration tests) green; zero new warnings; all boundaries intact.

## 19. Test requirements

Integration (architecture §21): (1) player appears in the Entity Registry; (2) Bubble includes the player position; (3) session mapping correct; (4) disconnect preserves the record; (5) reconnect restores the same entity; (6) full-flow replay identity. Via the composed null-gateway/loopback stack.

## 20. Completion checklist

- [ ] Six integration checks added and green.
- [ ] `Multiplayer/docs/PlayerLifecycle.md` written; matches delivered subsystem.
- [ ] Consistency sweep clean; boundaries intact; no behavior/interface change.
- [ ] Engine/OS-free test build; determinism preserved.
- [ ] 238/238 (+ integration) green on GCC + MSVC; zero new warnings.

## 21. Stop condition

Stop when integration is green and documented; ready for the readiness review. Do not begin Step 15.

## 22. Self-review

- **Architecture compliance.** Matches §21/§22; documents the delivered subsystem faithfully. Compliant.
- **ADR compliance.** ADR-007/008/009/010/011 preserved; no change.
- **Dependency correctness.** Steps 01–13 composed. Correct.
- **Scope isolation.** Integration + docs only; no behavior change. Isolated.
- **Determinism.** Replay-identical flows. Sound.
- **Engine/Platform boundary.** Engine-free test build; boundaries recorded. Preserved.
- **Ownership.** Unchanged. Correct.
- *Ambiguity resolved:* the reconnect-integration check asserts the **same** `EntityId` before/after (no new entity), operationalizing FR-6 in the integration test. Consistent with the architecture. No other issue.

## 23. Step-14 Specification Freeze

Sprint-007 Step-14 (Integration Cleanup) Specification is **FROZEN** (2026-07-11). Derived exclusively from the frozen architecture (§21/§22) and plan (Step 14); no architecture/ADR/order/scope change. Self-review passed; ambiguity resolved within the step. Boundaries, determinism, ADR-007/008/009/010/011, ownership preserved. Ready for implementation.

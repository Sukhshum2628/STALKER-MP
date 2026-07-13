# Sprint-007 · Step 13 — Validation Hardening — Implementation Specification

- **Status:** Step Specification (pre-implementation). Derives exclusively from the frozen Architecture (§18/§24, R-1…R-5) and Implementation Plan (Step 13). No architecture/ADR/scope/order change.
- **Governance:** ADR-007. One Engine Boundary / One Platform Boundary preserved. Host Authority.
- **Nature:** A test-and-harden pass proving the full negative surface and ownership enforcement — **no new subsystem behavior**.

---

## 1. Step objective

Prove and, where a gap surfaces, tighten the subsystem's negative surface: duplicate join, duplicate session, duplicate entity, invalid reconnect (bad/absent token), invalid spawn/respawn, ownership violation, capacity limits, and a many-player churn stress — all as value outcomes leaving state unchanged, with determinism preserved under load.

## 2. Scope

In scope: extend `tests/PlayerTests.cpp` with the full negative matrix + churn stress; make **minimal** hardening edits in existing `player::*` `.cpp` only if a gap is found (tightening an outcome/message), with **no** interface change and **no** new behavior. No new files.

## 3. Explicit out-of-scope items

Integration/docs (Step 14); any new feature, interface, type, or behavior; any engine/platform/networking change; any `MessageId`; any architecture-level change. Hardening is limited to closing a proven gap within existing outcomes.

## 4. Dependencies

Steps 01–12 (full engine-free subsystem against the null gateway). Sprint-006 `Session::TryReclaim` for reconnect-authority tests.

## 5. Files to create

None.

## 6. Files to modify

- `tests/PlayerTests.cpp` — the negative matrix + stress.
- Existing `player::*` `.cpp` — only if a gap requires an outcome/message tightening (no interface/behavior change). vcxprojs unchanged.

## 7. Public interfaces / types

None added or changed. Uses existing `PlayerManager`/`PlayerRegistry` outcomes.

## 8. Internal components

None added. Possibly a private outcome/message tightening if a gap is proven.

## 9. Responsibilities

Exercise adversarial/edge request sequences and confirm each is rejected with the correct outcome and no state change; confirm no duplicate entity is ever created; confirm reconnect authority (token-gated) cannot take over another player; confirm capacity limits; confirm determinism/replay under churn.

## 10. Data structures

None new. Reuses existing outcomes/consistency reports.

## 11. Invariants

Every rejection is a value outcome (never exception/partial mutation); no duplicate entity ever; ownership (token-gated reconnect) enforced; registry unchanged on every rejection; determinism preserved under churn (ascending ids, fixed-order drain); no interface/behavior added.

## 12. Ownership rules

Unchanged from prior steps — this step adds no owner and alters no ownership. Confirms ownership correctness under stress.

## 13. Naming conventions

Test names follow the existing `PlayerTests.cpp` convention; any hardening edit keeps existing names/signatures.

## 14. Step-specific constraints

Test-and-harden only; no new subsystem behavior, interface, type, file, or `MessageId`; engine/OS-free (null gateway).

## 15. Determinism constraints

Stress/churn replays identically; ascending ids under load; fixed-order drain; identical adversarial sequences → identical outcomes and state.

## 16. Engine-boundary constraints

Engine-free (null gateway).

## 17. Platform-boundary constraints

OS-free.

## 18. Acceptance criteria

Every negative case rejected with the correct outcome and no state change; duplicate-entity-prevention holds; reconnect-authority enforced; capacity enforced; churn stress stable and replay-identical; engine/OS-free; ADR-007-clean; 238/238 (plus new tests) green; zero new warnings; no interface/behavior change.

## 19. Test requirements

The full negative matrix (duplicate join/session/entity, invalid reconnect bad/absent token, invalid spawn/respawn, ownership violation, capacity) + duplicate-entity-prevention + reconnect-authority + a many-player join/leave/reconnect churn stress with a replay-identity assertion.

## 20. Completion checklist

- [ ] Full negative matrix covered; each rejection leaves state unchanged.
- [ ] Duplicate-entity-prevention + reconnect-authority + capacity tests pass.
- [ ] Churn stress stable and replay-identical.
- [ ] Any hardening limited to outcome/message tightening (no interface/behavior change).
- [ ] Engine/OS-free; ADR-007 clean; deterministic.
- [ ] 238/238 (+ new) green on GCC + MSVC; zero new warnings.

## 21. Stop condition

Stop when the negative surface is fully covered and green. Do not begin Step 14.

## 22. Self-review

- **Architecture compliance.** Matches §18/§24 and R-1…R-5; proves the failure surface without adding behavior. Compliant.
- **ADR compliance.** ADR-007 (value outcomes, no exceptions); others untouched.
- **Dependency correctness.** Steps 01–12 + Sprint-006 token seam. Correct.
- **Scope isolation.** Test-and-harden only; no new behavior/interface/file. Isolated.
- **Determinism.** Replay-identical churn. Sound.
- **Engine/Platform boundary.** No contact (null). Preserved.
- **Ownership.** Unchanged; confirmed under stress. Correct.
- *Ambiguity resolved:* "hardening" is bounded to tightening an existing outcome/message when a test proves a gap — explicitly not a behavior/interface change — so the step cannot drift into redesign. Consistent with the plan's "no new subsystem behavior". No other issue.

## 23. Step-13 Specification Freeze

Sprint-007 Step-13 (Validation Hardening) Specification is **FROZEN** (2026-07-11). Derived exclusively from the frozen architecture (§18/§24, R-1…R-5) and plan (Step 13); no architecture/ADR/order/scope change. Self-review passed; ambiguity resolved within the step. Boundaries, Host Authority, determinism, ADR-007, ownership preserved. Ready for implementation.

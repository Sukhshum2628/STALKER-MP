# Sprint-007 · Step 05 — Lifecycle State Machine — Implementation Specification

- **Status:** Step Specification (pre-implementation). Derives exclusively from the frozen Architecture (§7 responsibilities, §13 determinism, FR-2/5/7/8, §18/§19) and Implementation Plan (Step 5). No architecture/ADR/scope/order change.
- **Governance:** ADR-007. One Engine Boundary / One Platform Boundary preserved. Host Authority.
- **Nature:** The pure, host-authoritative transition rules over a `PlayerRecord`. **State-machine legality only** — no session, gateway, registry-wide, or tick wiring.

---

## 1. Step objective

Deliver the pure lifecycle state machine: legal transitions over `PlayerLifecycleState`/`PlayerConnectionState`, tick-derived respawn scheduling, and per-transition value outcomes — with no I/O, no session, no gateway, no registry-wide iteration, and no tick subscription.

## 2. Scope

In scope: create `include/stalkermp/player/PlayerLifecycle.h` (+ `src/player/PlayerLifecycle.cpp`) providing pure transition functions/methods that, given a record's current state + a requested transition + current tick, return the next state or a specific rejection outcome, and compute `respawnEligibleTick`. Extend `PlayerTests.cpp` with the full legal/illegal matrix.

## 3. Explicit out-of-scope items

Session observer/queue (Step 6); manager drain/apply, transactional join/reclaim, gateway calls (Step 7); position source (Step 8); gateway (Steps 9–10); wiring (Step 11); diagnostics (Step 12); validation hardening (Step 13); integration (Step 14). No engine/platform/networking; no `MessageId`; no registry-wide operation (single-record rules only); the state machine performs no spawn/despawn and touches no `EntityId` beyond recording it.

## 4. Dependencies

Steps 01 (`PlayerTypes` enums/record), 02 (`PlayerConfiguration.respawnDelayTicks`), 03–04 (record shape/lookups available, though transitions operate on a single record). Sprint-001 core (`Expected`/value outcomes, `common::Format`).

## 5. Files to create

- `include/stalkermp/player/PlayerLifecycle.h` — pure transition API (namespace `stalkermp::player`).
- `src/player/PlayerLifecycle.cpp` — transition legality + respawn tick math.

## 6. Files to modify

- Both vcxprojs (add the new `.cpp`/header); `tests/PlayerTests.cpp` (transition matrix).

## 7. Public interfaces / types

Free functions or a stateless `PlayerLifecycle` facade (no member state), e.g.:

- `[[nodiscard]] core::Expected<PlayerLifecycleState> ApplyJoin(const PlayerRecord&, std::uint64_t tick);`
- `[[nodiscard]] core::Expected<PlayerLifecycleState> ApplyDeath(const PlayerRecord&, std::uint64_t tick);`
- `[[nodiscard]] core::Expected<PlayerLifecycleState> ApplyRespawn(const PlayerRecord&, std::uint64_t tick, const PlayerConfiguration&);`
- `[[nodiscard]] core::Expected<PlayerLifecycleState> ApplySuspend(const PlayerRecord&, DisconnectDisposition);`
- `[[nodiscard]] core::Expected<PlayerLifecycleState> ApplyReclaim(const PlayerRecord&, std::uint64_t tick);`
- `[[nodiscard]] core::Expected<PlayerLifecycleState> ApplyRemove(const PlayerRecord&);`
- `[[nodiscard]] std::uint64_t ComputeRespawnEligibleTick(std::uint64_t deathTick, const PlayerConfiguration&) noexcept;`

Each returns the next state on legality or a rejection outcome (`InvalidArgument` + `common::Format`, call site maps to `JoinOutcome`/`ReconnectOutcome` etc.). Functions are pure (no mutation of the passed record; the caller — Step 7 — applies the returned state).

## 8. Internal components

File-local legality table (current state → permitted transitions). No stored state, no owner.

## 9. Responsibilities

Encode the legal transition graph: `Joining→Active`; `Active→Dead`; `Dead→AwaitingRespawn`; `AwaitingRespawn→Active`; `Active|Dead→Suspended` (on disconnect with `Retain`); `Suspended→Active` (reclaim); any→`Removed` (explicit removal / `Remove` disposition). Reject every illegal transition as an outcome (never assert on external input). Death never auto-destroys; disconnect maps to Suspended (retain), not Removed. Respawn timing is tick-derived (`deathTick + respawnDelayTicks`).

## 10. Data structures

No new struct. Consumes `PlayerRecord`, `PlayerLifecycleState`, `PlayerConnectionState`, `DisconnectDisposition`, `PlayerConfiguration`.

## 11. Invariants

Every transition legality-checked; illegal → value outcome, record unchanged; respawn eligible tick = `deathTick + respawnDelayTicks` (no wall-clock); Suspended retains (never Removed on ordinary disconnect); pure functions (no side effects, deterministic given inputs).

## 12. Ownership rules

Stateless; owns nothing. Records are owned by `PlayerRegistry`; the state machine only computes next-states for the caller (Step 7) to apply. No engine/session ownership.

## 13. Naming conventions

`Apply*` PascalCase; `ComputeRespawnEligibleTick`; namespace `stalkermp::player`; `#pragma once`. Value outcomes via `Expected`; messages via `common::Format`.

## 14. Step-specific constraints

Single-record pure rules only. No session, gateway, registry iteration, tick subscription, spawn/despawn, or I/O. Engine/OS-free; no `MessageId`.

## 15. Determinism constraints

Legality and respawn timing depend only on state + tick + config → fully deterministic and replay-stable across toolchains. No wall-clock, no ordering ambiguity, no float.

## 16. Engine-boundary constraints

Engine-free.

## 17. Platform-boundary constraints

OS-free.

## 18. Acceptance criteria

The full legal transition table passes; every illegal transition returns its outcome with the record unchanged; respawn delay counts in ticks; pure/deterministic; engine/OS-free; ADR-007-clean; 238/238 green; zero new warnings.

## 19. Test requirements

Exhaustive legal/illegal transition matrix (every state × every requested transition); respawn-tick math (including `respawnDelayTicks = 0` → immediate); Suspended-retain vs Remove disposition; determinism (same inputs → same output).

## 20. Completion checklist

- [ ] `PlayerLifecycle.h`/`.cpp` created (pure `Apply*` + `ComputeRespawnEligibleTick`).
- [ ] Full legality table encoded; illegal transitions rejected as outcomes.
- [ ] Death does not auto-destroy; disconnect → Suspended (retain).
- [ ] Tick-derived respawn timing; no wall-clock.
- [ ] vcxprojs updated; matrix tests added.
- [ ] Engine/OS-free; ADR-007 clean; deterministic.
- [ ] 238/238 green on GCC + MSVC; zero new warnings.

## 21. Stop condition

Stop when the state machine is correct in isolation and green. Nothing drives it yet. Do not begin Step 6.

## 22. Self-review

- **Architecture compliance.** Encodes the §7/FR-2/5/7/8 lifecycle; tick-derived timing per §13; outcome vocabulary per §18/§19 (Step-01 enums). Compliant.
- **ADR compliance.** ADR-007 clean (outcomes, no assert on external input); others untouched.
- **Dependency correctness.** Steps 01–04 + core. Correct.
- **Scope isolation.** Pure single-record rules; no session/gateway/registry/tick. Isolated.
- **Determinism.** State+tick+config only. Sound.
- **Engine/Platform boundary.** No contact. Preserved.
- **Ownership.** Stateless; owns nothing. Correct.
- *Ambiguity resolved:* the exact legal graph (which transitions are permitted) is fixed here from the architecture's lifecycle states (§9) and pipelines (§7) — additive and consistent with the frozen states; no new state introduced. No other issue.

## 23. Step-05 Specification Freeze

Sprint-007 Step-05 (Lifecycle State Machine) Specification is **FROZEN** (2026-07-11). Derived exclusively from the frozen architecture (§7/§13/§18/§19, FR-2/5/7/8) and plan (Step 5); no architecture/ADR/order/scope change. Self-review passed; ambiguity resolved within the step. Boundaries, Host Authority, determinism, ADR-007, ownership preserved. Ready for implementation.

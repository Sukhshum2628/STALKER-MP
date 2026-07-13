# Sprint-007 · Step 06 — Session Integration (Observer + Delta Queue) — Implementation Specification

- **Status:** Step Specification (pre-implementation). Derives exclusively from the frozen Architecture (§8 data-flow, AD-3, R-7, §16) and Implementation Plan (Step 6). No architecture/ADR/scope/order change.
- **Governance:** ADR-007; ADR-011 (single-threaded); Host Authority. One Engine Boundary / One Platform Boundary preserved.
- **Nature:** Capture Sprint-006 join/leave events **enqueue-only** (no simulation mutation in the networking tick).

---

## 1. Step objective

Deliver the `net::ISessionObserver` bridge and `PlayerDeltaQueue`: the observer records join/leave events into an ordered queue and mutates nothing else; a separate drain (consumed by Step 7) returns the deltas in deterministic order. This is the mechanism that keeps the networking tick (900) free of simulation mutation while feeding the manager's tick (250).

## 2. Scope

In scope: create `include/stalkermp/player/PlayerDeltaQueue.h` (the delta value + queue) and the observer bridge (`include/stalkermp/player/PlayerSessionObserver.h` + `src/player/PlayerSessionObserver.cpp`) implementing `net::ISessionObserver` with enqueue-only callbacks. Extend `PlayerTests.cpp` (driven via a `MockTransport`/`LoopbackTransport`-backed `Session`).

## 3. Explicit out-of-scope items

Applying deltas / registry mutation / lifecycle transitions (Step 7); manager (Step 7); position source (Step 8); gateway (Steps 9–10); service subscription to `Session`/`FrameDispatcher` and message registration (Step 11); diagnostics (Step 12); validation hardening (Step 13); integration (Step 14). No transport/framing change (consumes the Sprint-006 seam unchanged); no engine/platform contact; no `MessageId` registration.

## 4. Dependencies

Steps 01–05. Sprint-006 `net::ISessionObserver` (`OnMemberJoined(id, joinTick)` / `OnMemberLeft(id, reason, reconnectToken)`), `net::Session`, `net::ConnectionId`/`DisconnectReason`. Sprint-001 core.

## 5. Files to create

- `include/stalkermp/player/PlayerDeltaQueue.h` — the `PlayerSessionDelta` value + `PlayerDeltaQueue`.
- `include/stalkermp/player/PlayerSessionObserver.h` + `src/player/PlayerSessionObserver.cpp` — the enqueue-only observer bridge.

## 6. Files to modify

- Both vcxprojs (add the `.cpp` + headers); `tests/PlayerTests.cpp`.

## 7. Public interfaces / types

- `enum class PlayerSessionEventKind : std::uint8_t { Joined, Left };`
- `struct PlayerSessionDelta { PlayerSessionEventKind kind{}; net::ConnectionId connection{}; std::uint64_t tick = 0; net::DisconnectReason reason{}; std::uint64_t reconnectToken = 0; };`
- `class PlayerDeltaQueue { void Enqueue(const PlayerSessionDelta&); [[nodiscard]] std::vector<PlayerSessionDelta> Drain(); [[nodiscard]] std::size_t size() const noexcept; };`
- `class PlayerSessionObserver final : public net::ISessionObserver` — ctor takes `PlayerDeltaQueue&`; `OnMemberJoined`/`OnMemberLeft` **only enqueue**.

## 8. Internal components

`PlayerDeltaQueue` holds an ordered `std::vector<PlayerSessionDelta>` (drain empties it, `std::exchange` precedent). The observer holds a non-owning `PlayerDeltaQueue&`. No other state.

## 9. Responsibilities

Translate each `ISessionObserver` callback into one queued delta (kind, connection, tick, reason, token) and nothing more; preserve deterministic order (ascending `ConnectionId`, then arrival) on drain; leave the registry, gateway, entity, and session untouched during the callback.

## 10. Data structures

`PlayerSessionDelta` (above); `PlayerDeltaQueue`'s internal ordered vector. No engine/OS type.

## 11. Invariants

Observer callbacks perform **no** registry/gateway/entity/session mutation — enqueue only (AD-3, R-7); `Drain()` returns deltas in the fixed order and empties the queue; the observer is non-owning; Session ownership unchanged; single-threaded (callbacks are synchronous, ADR-011).

## 12. Ownership rules

`PlayerDeltaQueue` is owned by the manager (from Step 7); here it is a standalone value the observer references. The observer owns nothing (non-owning `PlayerDeltaQueue&`). `net::Session` remains Sprint-006-owned.

## 13. Naming conventions

Namespace `stalkermp::player`; `PlayerSessionDelta`/`PlayerDeltaQueue`/`PlayerSessionObserver`; PascalCase methods; `#pragma once`; enum `enum class : std::uint8_t` with a `Name()` helper (Step-01 style).

## 14. Step-specific constraints

Enqueue-only; no application, no mutation of any owned state, no `Session`/`FrameDispatcher` subscription (that is Step 11 wiring). Engine/OS-free; no transport/framing change; no `MessageId`.

## 15. Determinism constraints

Enqueue-only + fixed drain order guarantees the one-frame ingress latency (900→250) is deterministic; identical callback sequences → identical drained deltas. No wall-clock (uses the join/leave tick supplied by Sprint-006). Order is by `ConnectionId` then arrival, never by hash iteration.

## 16. Engine-boundary constraints

Engine-free.

## 17. Platform-boundary constraints

OS-free (consumes the engine-free `ISessionObserver`/`Session` seam; no socket contact).

## 18. Acceptance criteria

Join/leave callbacks enqueue exactly one correct delta each and mutate nothing else; `Drain()` returns the fixed order and empties; observer is non-owning; engine/OS-free; ADR-007-clean; 238/238 green; zero new warnings.

## 19. Test requirements

Drive the bridge via a `Session` backed by `MockTransport`/`LoopbackTransport`: assert each callback enqueues the right delta; assert **no** external state changes during the callback (enqueue-only); assert drain order + emptying; multi-event ordering.

## 20. Completion checklist

- [ ] `PlayerDeltaQueue.h` (`PlayerSessionDelta` + queue) created.
- [ ] `PlayerSessionObserver` implementing `net::ISessionObserver`, enqueue-only.
- [ ] Deterministic drain order; queue empties on drain.
- [ ] vcxprojs updated; observer/queue tests added (enqueue-only asserted).
- [ ] Engine/OS-free; ADR-007 clean; single-threaded.
- [ ] 238/238 green on GCC + MSVC; zero new warnings.

## 21. Stop condition

Stop when deltas are captured and drainable, enqueue-only proven, and green. No manager applies them yet. Do not begin Step 7.

## 22. Self-review

- **Architecture compliance.** Implements the §8 ingress (observer→queue) and AD-3/R-7 enqueue-only rule; consumes the Sprint-006 `ISessionObserver` seam unchanged (§16). Compliant.
- **ADR compliance.** ADR-007 clean; ADR-011 single-threaded synchronous callbacks; ADR-009/010 untouched.
- **Dependency correctness.** Steps 01–05 + Sprint-006 seam. Correct.
- **Scope isolation.** Enqueue-only; no application/subscription. Isolated.
- **Determinism.** Fixed drain order; tick-supplied timing. Sound.
- **Engine/Platform boundary.** No engine/OS contact. Preserved.
- **Ownership.** Observer non-owning; queue owned by manager later. Correct.
- *Ambiguity resolved:* the `PlayerSessionDelta` fields are fixed to exactly the data the Sprint-006 callbacks supply (id, tick, reason, token) plus kind — no inferred/derived data, preserving enqueue-only. No other issue.

## 23. Step-06 Specification Freeze

Sprint-007 Step-06 (Session Integration) Specification is **FROZEN** (2026-07-11). Derived exclusively from the frozen architecture (§8, AD-3, R-7, §16) and plan (Step 6); no architecture/ADR/order/scope change. Self-review passed; ambiguity resolved within the step. Boundaries, ADR-011 single-threaded, Host Authority, determinism, ADR-007, ownership preserved. Ready for implementation.

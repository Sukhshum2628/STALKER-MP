# Sprint-006 · Step 7 — Timeout + Unified Disconnect Flow — Implementation Specification

- **Status:** Implementation specification (no code). Maps to frozen `Sprint-006-Architecture.md` (§3) and Implementation Plan Step 7. Consumes Steps 1–6. Engine-free / OS-free.
- **Nature:** Tick-derived timeouts and one canonical disconnect path. **No** session (Step 8), no queues (Step 9), no wall-clock control flow.
- **Constraints:** ADR-007/011 (timeouts tick-derived, no wall-clock in control flow); deterministic ascending-`ConnectionId` processing; unified disconnect; One Engine/Platform Boundary.

## 1. Files to create
None. (Extends `Connection`, `ConnectionTable`, `Handshake`.)

## 2. Files to modify
`src/net/ConnectionTable.cpp` (+ `ConnectionTable.h`) — timeout scan + `BeginDisconnect`; `src/net/Handshake.cpp`/`.h` — surface the handshake-timeout budget. `tests/NetworkTests.cpp` (Step-7 tests). vcxprojs unchanged (no new files).

## 3. Responsibilities
- **Tick conversion.** `connectionTimeoutMs` / `handshakeTimeoutMs` are converted to tick counts once, using the frame clock (a fixed ms-per-tick derived at module Initialize — Step 11; in Step 7 the conversion is a pure function `TicksFromMs(ms, msPerTick)` exercised with a test constant). Timeouts are compared against the connection's `lastRecvTick` / `establishedTick` vs. the current tick.
- **Timeout scan.** `ScanTimeouts(currentTick) -> std::vector<ConnectionId>` iterates connections in **ascending `ConnectionId`** and flags: (a) any connection whose `handshake != Established` and `(currentTick - createdTick) > handshakeTickBudget` → handshake timeout; (b) any `Connected` connection whose `(currentTick - lastRecvTick) > connectionTickBudget` → idle timeout. Each flagged id is disconnected with `DisconnectReason::Timeout`.
- **Unified disconnect.** `BeginDisconnect(ConnectionId, DisconnectReason) -> void`: sets `state = Disconnecting`, records `lastReason`, signals a best-effort `Bye` control message (returned as intent / enqueued via the transport seam by the host — Step 10; in Step 7 tests the intent is asserted), releases any per-connection resources owned so far (none beyond the record in Step 7; queue release is Step 9's addition to this same path), and finally `Remove`s the connection from the table (→ `Disconnected`). Always reaches removal exactly once; idempotent if called twice for the same id (second call is a no-op).

## 4. Ownership & lifetime
Disconnect operates on table-owned connections resolved by id. No resource is leaked: `BeginDisconnect` is the single exit, guaranteeing every connection that enters the table eventually leaves through it (or through table teardown, which invokes the same release). `DisconnectReason` is stored on the connection for diagnostics until removal.

## 5. Dependencies
Steps 1–6. Not Step 8/9.

## 6. Constraints
Timeouts tick-derived only (no `std::chrono` in control flow); ascending-id processing; single disconnect path; idempotent; ADR-007/011; engine-free.

## 7. Validation rules
- `TicksFromMs` is total and monotonic (larger ms → ≥ ticks); ms below one tick → at least 1 tick (never 0, so a timeout can always fire).
- A connection is timed out only by the applicable rule (handshake budget while not Established; idle budget while Connected).
- `BeginDisconnect` on an already-`Disconnecting`/absent id is a no-op.

## 8. Failure behavior
No exceptions. A `Bye` that cannot be sent (transport down) does not block disconnect — release/removal proceed regardless (best-effort). Timeout of an absent id is impossible (scan reads live table).

## 9. Unit testing
- `TicksFromMs`: known ms/msPerTick → expected ticks; sub-tick ms → 1; monotonicity.
- Idle timeout: a `Connected` connection with stale `lastRecvTick` is flagged after the budget, disconnected with `Timeout`; a fresh one is not.
- Handshake timeout: a non-Established connection past the handshake budget is flagged; one that reached `Established` in time is not.
- Unified disconnect: `BeginDisconnect` sets `Disconnecting`, emits `Bye` intent, releases, removes; connection gone from table; reason recorded pre-removal.
- Scan order is ascending `ConnectionId`.

## 10. Negative testing
`BeginDisconnect` twice for one id → single removal, second is no-op; `BeginDisconnect(absent)` → no-op; timeout scan on empty table → empty result.

## 11. Boundary testing
Exactly-at-budget vs. one tick past: at budget not yet timed out; budget+1 timed out (define the comparison as strictly `>`). Multiple simultaneous timeouts processed in ascending id order, all removed.

## 12. Definition of Done
- [ ] Tick-derived handshake + idle timeouts; ascending-id scan; `DisconnectReason::Timeout`.
- [ ] Unified `BeginDisconnect` (Disconnecting → Bye intent → release → remove), idempotent, no leak, every reason routed through it.
- [ ] No wall-clock in control flow; deterministic; ADR-007/011; engine-free.
- [ ] §9–§11 tests green (GCC + MSVC); full suite green; no regressions.
- [ ] Nothing from Steps 8–14.

## 13. Handoff notes (ChatGPT)
Pre-decided: comparison is strict `>` budget; `TicksFromMs(ms, msPerTick) = max(1, (ms + msPerTick - 1) / msPerTick)` (ceil, min 1); `msPerTick` is supplied by the caller (module Initialize computes it in Step 11) — Step 7 tests pass a fixed value (e.g. 16). `BeginDisconnect` returns the `Bye` intent; the host performs the actual `Send` in Step 10. Queue release joins this path in Step 9. Do not implement session removal (Step 8) — though note Step 8 will subscribe to disconnect to fire `OnMemberLeft`.

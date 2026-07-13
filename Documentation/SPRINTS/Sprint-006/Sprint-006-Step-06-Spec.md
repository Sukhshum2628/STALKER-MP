# Sprint-006 · Step 6 — Handshake State Machine (+ IConnectionAuthenticator) — Implementation Specification

- **Status:** Implementation specification (no code). Maps to frozen `Sprint-006-Architecture.md` (§3) and Implementation Plan Step 6. Consumes Steps 1–5. Engine-free / OS-free.
- **Nature:** Deterministic, single-step-per-tick handshake over control messages, plus the authentication seam (allow-all default). **No** transport internals beyond the mock, no timeout logic (Step 7), no session (Step 8).
- **Constraints:** ADR-007; single-step-per-tick determinism; ADR-010 (control ids, version exact-match); One Engine/Platform Boundary; no auth *logic* beyond allow-all; no encryption.

## 1. Files to create
| File | Contents |
|---|---|
| `include/stalkermp/net/Handshake.h` | Handshake driver (per-connection state advance) + control-message shapes. |
| `src/net/Handshake.cpp` | Transition logic. |
| `include/stalkermp/net/IConnectionAuthenticator.h` | Auth seam + `AllowAllAuthenticator`. |

## 2. Files to modify
`xrMP.vcxproj` / `tests/xrMP_Tests.vcxproj` (add `Handshake.cpp`, two headers); `tests/NetworkTests.cpp` (Step-6 tests, driven via `MockTransport`).

## 3. Responsibilities
- **Handshake sequence.** `Hello → Challenge → Response → Established` (host role: receives Hello, sends Challenge, receives Response, emits Established). Mapped onto `HandshakeState` (Step 1: `None → HelloSent`/received → `ChallengeSent` → `ResponseSent`/received → `Established`) and control message ids `kMsgHello/kMsgChallenge/kMsgResponse/kMsgEstablished`.
- **Content (no crypto).** Hello carries `protocolVersion` + a client nonce; Challenge echoes a server nonce; Response echoes the server nonce. Version is checked at Hello (exact-match, ADR-010) — mismatch rejects **before any allocation**. Nonces are plain `u32` (reserve the exact frames a future auth/encryption handshake occupies).
- **Single-step-per-tick.** `Advance(Connection&, incoming control message opt, tick, DispatchContext&) -> HandshakeStep` advances **at most one** state per connection per tick; it consumes at most one handshake control message and, if appropriate, enqueues exactly one outbound control message (through the transport seam, via the host later — in Step 6 the driver returns the outbound intent; wiring to `Send` is exercised through the mock).
- **Authenticator.** `IConnectionAuthenticator::Authorize(const Connection&, const AuthPayload&) -> AuthDecision {Allow, Deny}` called at the `Response → Established` edge. `AllowAllAuthenticator` returns `Allow`. Auth *logic* is out of scope.
- **Illegal transitions.** Any out-of-order/duplicate handshake message, or an unexpected id for the current state, yields a drop signal with `DisconnectReason::ProtocolError` (the actual disconnect is Step 7's unified path; Step 6 returns the reason).

## 4. Ownership & lifetime
Handshake state lives **on** the `Connection` (Step 5), owned by the `ConnectionTable`; the handshake driver is stateless (operates on the connection by reference, resolved by id). The authenticator is a non-owning seam pointer (allow-all singleton in Step 6). Nonces are transient per-connection fields (may be reserved on `Connection` or held in a small handshake-scratch owned by the table entry).

## 5. Dependencies
Steps 1–5 (types, codec, transport mock, connection table). Not Step 7/8.

## 6. Constraints
One advance per tick per connection; version exact-match; reject-before-allocate on version mismatch; no crypto/auth logic; deterministic; ADR-007/010; engine-free.

## 7. Validation rules
- Message id must match the expected id for the current `HandshakeState`; else `ProtocolError`.
- Hello `protocolVersion != kProtocolVersion` → `ProtocolError` (pre-allocation).
- Response nonce must equal the server nonce sent in Challenge; mismatch → `ProtocolError`.
- `Authorize` == `Deny` at the Response edge → drop with `DisconnectReason::Rejected`.

## 8. Failure behavior
All failures return a `DisconnectReason` (Step 7 performs the drop); nothing throws; a rejected handshake never reaches `Established`.

## 9. Unit testing (via MockTransport)
- Happy path: scripted Hello→(Challenge out)→Response→Established over successive ticks; state advances exactly one step per tick; ends `Established`.
- Version mismatch at Hello → `ProtocolError`, no `Established`, no allocation beyond the connection.
- Nonce mismatch at Response → `ProtocolError`.
- `AllowAllAuthenticator` reaches `Established`; a test `DenyAuthenticator` yields `Rejected` at the Response edge.

## 10. Negative testing (every illegal transition)
- Response before Challenge; Challenge received in `None`; duplicate Hello; Established-id received early — each → `ProtocolError`, connection not established.
- Unknown/non-handshake control id during handshake → `ProtocolError` (or ignored per the routing rule; specify: ignored if it is a valid *other* control id like Ping, `ProtocolError` if it is a handshake id out of order).

## 11. Boundary testing
Exactly-one-step-per-tick: feeding all messages in one tick still advances only one state; the rest are consumed on subsequent ticks. A connection with no incoming message in a tick does not change state (except via Step 7 timeout, not here).

## 12. Definition of Done
- [ ] Handshake `Hello→Challenge→Response→Established` implemented, single-step-per-tick, over control messages.
- [ ] `IConnectionAuthenticator` + `AllowAllAuthenticator`; authorize called at Response→Established.
- [ ] Version exact-match reject-before-allocate; nonce check; every illegal transition → `ProtocolError`.
- [ ] No crypto/auth logic; no timeout (Step 7); no session (Step 8); deterministic; engine-free; ADR-007/010.
- [ ] §9–§11 tests green (GCC + MSVC); full suite green; no regressions.
- [ ] Nothing from Steps 7–14.

## 13. Handoff notes (ChatGPT)
Pre-decided: host-side handshake only (client is the fake driver in tests); nonces are `u32`; `AuthPayload` carries the echoed nonce + reserved fields; the driver returns `{ nextState, outboundMessageOpt, dropReasonOpt }` and the host (Step 10) performs the actual `Send`/disconnect — in Step 6 tests, drive it directly and assert the returned intents. Do not implement the disconnect resource-release (Step 7) or session admission (Step 8).

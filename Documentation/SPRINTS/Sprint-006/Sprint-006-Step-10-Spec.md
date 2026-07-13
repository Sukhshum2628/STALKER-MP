# Sprint-006 · Step 10 — HostServer (+ fake-client integration) — Implementation Specification

- **Status:** Implementation specification (no code). Maps to frozen `Sprint-006-Architecture.md` (§2, §7) and Implementation Plan Step 10. Consumes Steps 1–9. Engine-free / OS-free (mock/loopback transport). **Evidence gate E-G3-N** (deterministic replay).
- **Nature:** The authoritative endpoint driving accept/admission/heartbeat/disconnect and the per-tick pump — over mock/loopback. **No** module services (Step 11), no Bootstrap wiring (Step 12), no real sockets (Step 13).
- **Constraints:** ADR-007/010/011; Host Authority; **networking reads no simulation state**; deterministic single-threaded per-tick pass; One Engine/Platform Boundary.

## 1. Files to create
| File | Contents |
|---|---|
| `include/stalkermp/net/HostServer.h` + `src/net/HostServer.cpp` | The host endpoint + per-tick pump. |
| `tests/` fake-client driver (in `NetworkTests.cpp` or a test support header) | Speaks the wire protocol against the host over loopback. |

## 2. Files to modify
`xrMP.vcxproj` / `tests/xrMP_Tests.vcxproj` (add `HostServer.cpp`, header); `tests/NetworkTests.cpp` (Step-10 integration + replay tests).

## 3. Responsibilities
- **Startup/shutdown.** `Start(NetworkingConfig, TransportConfig, ITransport&) -> core::Expected<void>` → transport `Bind`+`Listen` (no-op success if `!enabled`); `Stop() noexcept` → `CloseListen` + disconnect all. Holds the listen state, running flag, and the frame tick it stamps on control messages.
- **Per-tick pass (the deterministic pump, §7).** `Tick(currentTick, ITransport&, ConnectionTable&, Session&, MessageRegistry&) -> void`:
  1. `transport.Poll(maxBytesBudget)`.
  2. Drain `transport.Accepted()` (count-bounded) → `ConnectionTable.Add` (capacity admission; reject with `CapacityFull` → immediate disconnect) → new connections enter handshake.
  3. Drain `transport.Received()` → per datagram: `DeserializeHeader` (drop on error, count) → route: handshake control ids to the handshake driver (advance one step), `Ping`→`Pong`, `Bye`→disconnect, other control ids handled, data ids → `MessageRegistry.Dispatch` (unknown → dropped/counted).
  4. Advance handshakes/timeouts in **ascending `ConnectionId`**: `ConnectionTable.ScanTimeouts` → `BeginDisconnect(Timeout)`; on handshake `Established` → `Session.Admit` + emit `Established`.
  5. Heartbeat: for `Connected` connections past `heartbeatInterval` since last send, enqueue `Ping`.
  6. Drain outgoing queues → `transport.Send` (bounded by `maxBytesPerTick`).
- **Admission policy.** Capacity (`maxConnections`), then handshake; `IConnectionAuthenticator` at Established. Host assigns `ConnectionId` (never wire-trusted).
- **Disconnect orchestration.** All disconnects go through Step 7's `BeginDisconnect`; the host performs the actual `Bye` `Send` and `Session.Remove`.
- **No simulation access.** The host holds no simulation reference and calls no engine/ALife code.

## 4. Ownership & lifetime
The host is owned by the Module (Step 11); it holds **non-owning** references to the transport, table, session, and registry (all Module-owned) and drives them. It owns only its own scalar state (listen handle, running flag, tick, counters). Connections/members are resolved by id each tick (no retained pointers).

## 5. Dependencies
Steps 1–9. Not Step 11/12/13.

## 6. Constraints
Single-threaded per-tick; reads no simulation; deterministic ordering (id-ordered processing; bounded drains); `maxBytesPerTick` respected; ADR-007/010/011; engine-free/OS-free (mock/loopback).

## 7. Validation rules
- Accept drain bounded per tick; over-capacity accept → immediate `CapacityFull` disconnect (not admitted).
- Malformed header → datagram dropped + counted, connection unaffected unless it is a handshake `ProtocolError` (then disconnect).
- Handshake advances exactly one step per connection per tick.
- Outgoing drain never exceeds `maxBytesPerTick`.
- `ValidateConsistency` (host + table + session) healthy after each tick.

## 8. Failure behavior
Transport errors surface as `TransportOutcome`/`Expected`, never throw; a failed `Send` leaves the packet queued (retry next tick, reliable) or dropped (unreliable); `Start` failure returns `Expected` (bind failure). Disconnects are the unified path.

## 9. Unit / integration testing (loopback + fake client)
- Full flow: fake client Hello → Challenge → Response → Established → `Session` admits; Ping/Pong exchanged; graceful `Bye` → removed. Over loopback across ticks.
- Timeout disconnect: fake client stops responding → idle/handshake timeout → removed with `Timeout`.
- Capacity rejection: admit `maxConnections`, next client rejected with `CapacityFull`.
- Version-mismatch rejection: fake client sends wrong `protocolVersion` → `ProtocolError`, never admitted.
- **E-G3-N replay:** feed an identical sequence of received datagrams + ticks twice → identical connection/session state and identical outgoing queue contents.

## 10. Negative testing
Over-capacity accept rejected; malformed datagram dropped (host survives); duplicate handshake message → `ProtocolError`; `Send` when transport down → packet handled per channel policy, no crash.

## 11. Boundary testing
Exactly `maxConnections` admitted; heartbeat fires exactly at the interval; `maxBytesPerTick` boundary (one over → remainder deferred to next tick); handshake completes in the minimum tick count (one step/tick).

## 12. Definition of Done
- [ ] `HostServer` Start/Stop + the deterministic per-tick pass (§3) over mock/loopback.
- [ ] Accept/admission/heartbeat/disconnect via the unified paths; reads no simulation; host-authoritative ids.
- [ ] Full handshake→connected→ping→disconnect and timeout integration pass; capacity + version rejection.
- [ ] **E-G3-N replay** passes (identical state + outgoing queues).
- [ ] Deterministic, single-threaded; ADR-007/010/011; engine-free/OS-free.
- [ ] §9–§11 tests green (GCC + MSVC); full suite green; no regressions.
- [ ] Nothing from Steps 11–14.

## 13. Handoff notes (ChatGPT)
Pre-decided: the host takes non-owning refs to transport/table/session/registry (Module wires them in Step 11); the fake client is a test-only driver using the same codec + a `MockTransport`/`LoopbackTransport`; `Ping/Pong` carry a tick stamp for RTT (diagnostic, Step 11). Do not construct the module services or touch Bootstrap. The replay harness records `(tick, received datagrams)` and re-runs the pump on a fresh host.

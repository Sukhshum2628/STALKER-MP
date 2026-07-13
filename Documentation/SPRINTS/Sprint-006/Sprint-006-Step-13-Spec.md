# Sprint-006 · Step 13 — Real UDP Transport (PlatformSockets.cpp) — Implementation Specification

- **Status:** Implementation specification (no code). Maps to frozen `Sprint-006-Architecture.md` (§4, ADR-009) and Implementation Plan Step 13. Consumes Steps 3, 11, 12. **Evidence gate E-G1-N.** Mirrors the Sprint-005 `EngineAdapters.cpp` One-Boundary precedent.
- **Nature:** The concrete `ITransport` over OS sockets — **the one OS-touching translation unit of Sprint-006**. **No** module logic change; the whole module above `ITransport` is unchanged.
- **Constraints:** ADR-007 (no exceptions/RTTI/iostream; `Expected`/`TransportOutcome`; compiles under `EngineAbi.props`); **ADR-009 (One Platform Boundary — OS socket headers confined to this TU; no socket/address type crosses `ITransport`)**; ADR-011 (non-blocking, single-threaded); One Engine Boundary (this TU includes no engine header).

## 1. Files to create
| File | Contents |
|---|---|
| `include/stalkermp/adapters/PlatformTransport.h` | `adapters::CreatePlatformTransport(const net::TransportConfig&) -> std::unique_ptr<net::ITransport>` factory declaration (engine/OS-free header). |
| `src/adapters/PlatformSockets.cpp` | The ONLY OS-header TU: UDP `ITransport` impl + factory definition. |
| `tests/support/NullPlatformTransport.cpp` | Test-build factory definition returning a loopback/mock transport (no OS). |

## 2. Files to modify
| File | Why |
|---|---|
| `xrMP.vcxproj` | Add `src\adapters\PlatformSockets.cpp` (`ClCompile`) with **per-file** settings scoped to this TU: Winsock/socket library link (`ws2_32.lib`) and any OS include roots — mirrors the `EngineAdapters.cpp` per-file pattern (One Platform Boundary). Add the factory header to `ClInclude`. |
| `tests/xrMP_Tests.vcxproj` | Link `tests\support\NullPlatformTransport.cpp` (the loopback/null factory) instead of `PlatformSockets.cpp` — the test build has no OS transport, exactly as it links `NullEngineAdapters.cpp` instead of `EngineAdapters.cpp`. |
| `tests/NetworkTests.cpp` | A build-configuration assertion that the test build uses the null/loopback factory (One Platform Boundary). |

## 3. Responsibilities
- **`EngineAbi`-clean UDP transport** implementing `ITransport`: `Bind(port, address)` (create non-blocking UDP socket, `bind`), `Listen(backlog)` (for UDP, a no-op/accept-model shim — connection acceptance is modeled from first datagram per remote address, producing a `TransportEndpoint`), `Poll(maxBytesBudget)` (non-blocking `recvfrom` loop bounded by budget → internal `Received()` store; flush queued `Send`s via `sendto`), `Send(endpoint, channel, span)` (enqueue or `sendto`; `WouldBlock` on EWOULDBLOCK), `CloseListen`/`CloseEndpoint`. `IPacketCodec` (identity) applied at the outermost boundary (encode on send, decode on receive) — reserved seam, identity in Sprint-006.
- **Endpoint mapping.** Remote `(address, port)` ↔ opaque `TransportEndpoint` handle maintained **inside** this TU (a private map); the handle is what crosses `ITransport`. No `sockaddr` ever leaves the TU (ADR-009).
- **Address validation.** Syntactic/semantic `bindAddress` validation (`inet_pton` or equivalent) happens **here**, returning `Expected` on failure — the deferred check promised by Step-1's `TransportConfig` spec.
- **Factory.** `CreatePlatformTransport` returns the UDP transport (engine build) / loopback (test build).
- **Non-blocking + budget.** Sockets set non-blocking; `Poll` never blocks and never exceeds `maxBytesPerTick`.

## 4. Ownership & lifetime
The transport owns its OS socket handles and the endpoint map; it is owned by the Runtime (Step 12) and outlives the host that uses it. Sockets are closed in the destructor / `Stop`; no handle leaks. No pointer to OS structures crosses `ITransport`.

## 5. Dependencies
Steps 3 (`ITransport`), 11 (module consumes it), 12 (factory wired). OS sockets (Winsock on Windows).

## 6. Constraints
OS headers confined to `PlatformSockets.cpp`; compiles under `EngineAbi.props` (exceptions off, `/W4 /WX`, C4530-clean); non-blocking; no blocking calls; `Expected`/`TransportOutcome` (never throw); no socket type across the seam; test build links the null/loopback factory only.

## 7. Validation rules
- `Bind` fails (`Expected`) on invalid address, port in use, or socket creation failure — never throws.
- `Poll` respects `maxBytesPerTick`; leftover datagrams remain for next tick.
- `Send` to an unknown endpoint → `EndpointMissing`; EWOULDBLOCK → `WouldBlock`.
- No `sockaddr`/`SOCKET`/OS type appears in any header or crosses `ITransport` (One Platform Boundary — grep-verifiable).

## 8. Failure behavior
All socket errors mapped to `Expected`/`TransportOutcome` values; partial sends handled per channel policy (Step 9); a closed socket surfaces as `Error`, not a crash. Destruction closes all sockets.

## 9. Unit / verification testing
- **Test build (engine/OS-free):** `CreatePlatformTransport` (the null/loopback definition) yields a working `ITransport`; the whole Step 1–11 suite runs unchanged over it; assertion that no OS/engine TU is linked into tests.
- **Antigravity (engine build, real sockets):** `PlatformSockets.cpp` compiles under `EngineAbi.props`; a real-socket loopback smoke test — bind two UDP transports on localhost, handshake a fake client end-to-end, exchange Ping/Pong, disconnect — proving non-blocking `Poll` within budget and exception-free operation. **This is E-G1-N.**

## 10. Negative testing
Bind to an invalid address → `Expected` error; bind to an in-use port → error; `Send` to unknown endpoint → `EndpointMissing`; oversized `Poll` budget still bounded by available data; socket closed mid-run → `Error`, no crash.

## 11. Boundary testing
`Poll(maxBytesPerTick)` at exactly the budget; a datagram exactly at `mtuPayloadBytes`; `enabled = false` → transport constructed but never bound (no socket opened).

## 12. Definition of Done
- [ ] `PlatformSockets.cpp` implements a non-blocking UDP `ITransport`; `CreatePlatformTransport` factory; identity `IPacketCodec` seam.
- [ ] One Platform Boundary intact: OS headers only in this TU (per-file vcxproj scope); no socket/address type crosses `ITransport`; test build links the null/loopback factory.
- [ ] Address validation here (not in config); `Expected`/`TransportOutcome` only; compiles under `EngineAbi.props`.
- [ ] **E-G1-N** passes: engine build compiles + real-socket loopback smoke (non-blocking, within budget, exception-free).
- [ ] Test build + full suite green (GCC + MSVC); MSVC Release clean; no regressions.
- [ ] No module logic change; nothing from Step 14.

## 13. Handoff notes (ChatGPT)
Pre-decided: UDP single socket; UDP "accept" is modeled from first datagram per remote address; endpoint↔address map is private to the TU; `IPacketCodec` is identity in Sprint-006; per-file vcxproj settings add `ws2_32.lib` + OS include roots scoped to `PlatformSockets.cpp` only (mirror `EngineAdapters.cpp`); the test build links `NullPlatformTransport.cpp` returning a loopback transport. Expect Antigravity to iterate the exact Winsock include/link set during the engine build — those are TU-local build-config adjustments, not architecture changes. Do not modify any module logic above `ITransport`.

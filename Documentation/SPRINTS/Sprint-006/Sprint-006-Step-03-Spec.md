# Sprint-006 · Step 3 — ITransport seam + MockTransport + LoopbackTransport — Implementation Specification

- **Status:** Implementation specification (no code). Maps to frozen `Sprint-006-Architecture.md` (§4, ADR-009) and Implementation Plan Step 3. Consumes frozen Step-1 types (`TransportEndpoint`, `Channel`, `TransportOutcome`) and Step-2 codec (for tests only).
- **Nature:** Engine-free **and** OS-free. The transport abstraction and the entire in-memory test substrate. **No** real sockets, message registry, connection/table, queues, reliability, handshake, services, or wiring.
- **Constraints (binding):** ADR-007 (no exceptions/RTTI/iostream; `core::Expected<T>` / `TransportOutcome`; trivially-safe), ADR-009 (**no OS types cross `ITransport`**; only owned byte buffers), ADR-011 (no threads), One Engine/Platform Boundary, Deterministic Simulation.

## 1. Files to create
| File | Contents |
|---|---|
| `include/stalkermp/net/ITransport.h` | Abstract transport seam (§4 shapes). Engine/OS-free. |
| `include/stalkermp/net/MockTransport.h` (+ `src/net/MockTransport.cpp` if non-trivial) | In-memory scriptable transport with loss/reorder/duplicate/corrupt knobs. |
| `include/stalkermp/net/LoopbackTransport.h` (+ `src/net/LoopbackTransport.cpp` if non-trivial) | Two in-process endpoints wired to each other. |

## 2. Files to modify
| File | Why |
|---|---|
| `xrMP.vcxproj` / `tests/xrMP_Tests.vcxproj` | Add any new `.cpp` (`ClCompile`) and the three headers (`ClInclude`); test project links the mock/loopback for tests. |
| `tests/NetworkTests.cpp` | Add Step-3 transport tests. |

## 3. Responsibilities
- **`ITransport`** — the ONLY seam over the OS. Methods (shapes, not code): `Bind(port, addressView) -> core::Expected<void>`; `Listen(backlog) -> core::Expected<void>`; `Poll(std::size_t maxBytesBudget) -> void` (non-blocking; moves bytes between the OS/backing store and internal queues); `Send(TransportEndpoint, Channel, ConstByteSpan) -> TransportOutcome`; `CloseListen() -> void`; `CloseEndpoint(TransportEndpoint) -> void`; `Accepted() -> std::vector<TransportEndpoint>` (drain, by value); `Received() -> std::vector<ReceivedDatagram>` (drain; `ReceivedDatagram { TransportEndpoint from; Channel channel; std::vector<std::uint8_t> bytes; }`, owned by value). Virtual destructor. No socket/address/OS type anywhere in the signature or the `ReceivedDatagram`.
- **`MockTransport`** — deterministic in-memory transport for unit tests. Scriptable: inject inbound datagrams/accepts; capture outbound `Send` calls; knobs `dropNextN`, `duplicateNext`, `reorderWindow`, `corruptNext` (flip a byte) applied deterministically; a per-endpoint online flag. `Poll` moves scripted inbound into the `Received()` drain and applies the failure knobs to outbound capture.
- **`LoopbackTransport`** — pairs two in-process module instances: `Send` on endpoint A appears in the peer's `Received()` on the next `Poll`. No knobs (clean channel) — used for host↔fake-client integration (Step 10). May be built as two cooperating `MockTransport`-like halves sharing an in-memory mailbox.

## 4. Ownership & lifetime
- **Ownership.** The (future) Networking Module owns exactly one `ITransport` (Step 11). In Step 3, tests own the mock/loopback directly. Byte buffers crossing the seam are **owned by value** (moved), never aliased into transport-internal memory.
- **Lifetime.** `ITransport` outlives every `TransportEndpoint` handle referring to it. Endpoints are opaque `u32` handles (Step-1 `TransportEndpoint`), resolved internally by the transport; callers never hold pointers to transport internals.

## 5. Dependencies
Steps 1 (`TransportEndpoint`, `Channel`, `TransportOutcome`) and 2 (codec — tests only). No later step.

## 6. Constraints
No OS headers, no socket/address types across the seam; non-blocking `Poll` (no blocking calls); owned-buffer value transfer; deterministic knob application; ADR-007 posture; engine-free.

## 7. Validation rules
- `Send` to an unknown/closed endpoint → `TransportOutcome::EndpointMissing` (benign). `Send` when internal outgoing capacity is exceeded (mock config) → `TransportOutcome::WouldBlock`. Successful enqueue → `TransportOutcome::Queued`; immediate loopback delivery may report `Ok`.
- `Bind`/`Listen` before use; calling `Send`/`Poll` before `Bind` on the mock is a defined no-op returning `Error` (documented), not a crash.
- `Poll(budget)` never processes more than `budget` bytes; remaining bytes stay queued for the next tick.

## 8. Failure behavior
Every fallible op returns `Expected`/`TransportOutcome`; nothing throws. Malformed or corrupted inbound bytes are surfaced *as bytes* to `Received()` (the codec/registry rejects them later) — the transport does not interpret payloads.

## 9. Unit testing (in `tests/NetworkTests.cpp`)
- Mock: scripted inbound datagram appears in `Received()` after `Poll`; `Send` captured with correct endpoint/channel/bytes; `Accepted()` drains injected accepts once.
- Knobs: `dropNextN` drops exactly N; `duplicateNext` yields two copies; `reorderWindow` reorders deterministically; `corruptNext` flips one byte (payload still delivered, codec later rejects).
- Loopback: `Send` A → `Received()` on B after one `Poll`; bidirectional.
- Buffer ownership: mutating the caller's source buffer after `Send` does not change delivered bytes (value copy).
- `Poll(budget)` respects the byte budget (partial drain, remainder persists).

## 10. Negative testing
`Send` to unknown endpoint → `EndpointMissing`; over-capacity → `WouldBlock`; `Send`/`Poll` before `Bind` → defined `Error`, no crash.

## 11. Boundary testing
`Poll(0)` moves nothing; `Poll(exact-datagram-size)` moves exactly one; empty `Received()`/`Accepted()` return empty vectors.

## 12. Definition of Done
- [ ] `ITransport` defined with the §3 shapes; no OS type in any signature or `ReceivedDatagram`.
- [ ] `MockTransport` + `LoopbackTransport` implemented; deterministic; knobs work.
- [ ] Engine-free/OS-free (include-tree verified); ADR-007/009/011 preserved.
- [ ] §9–§11 tests green on GCC + MSVC; full suite green; zero new warnings; no regressions.
- [ ] Nothing from Steps 4–14.

## 13. Handoff notes (ChatGPT)
Pre-decided: byte element `std::uint8_t`; `ConstByteSpan` = `(const std::uint8_t*, std::size_t)` or `gsl`-free equivalent already used elsewhere; `ReceivedDatagram`/`TransportEndpoint` are the only cross-seam types; endpoints are opaque `u32`. Do not add reliability/queues (Step 9) or real sockets (Step 13). The mock is the primary vehicle for all later steps' tests.

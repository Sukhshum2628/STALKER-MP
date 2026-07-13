# Host Networking Infrastructure (Sprint-006) — Subsystem Documentation

- **Status:** Implemented & Verified (Closed) — 2026-07-11.
- **Architecture:** `Multiplayer/docs/Sprint-006-Architecture.md` (FROZEN 2026-07-10). This document records the delivered subsystem; it introduces no new decisions.
- **Governing ADRs:** ADR-007 (Engine ABI Contract), ADR-009 (One Platform Boundary), ADR-010 (Wire Protocol Contract), ADR-011 (Single-Threaded Networking / Deterministic Control Flow) — all implemented. ADR-008 (Engine State Mutation Boundary) preserved (untouched — networking mutates no engine state).

---

## 1. Purpose

The Host Networking Infrastructure is the multiplayer framework that future systems (Player Lifecycle, Snapshot Replication, Persistence) rely upon. It provides the host-authoritative transport, connection lifecycle, handshake, session membership, wire framing, and per-tick pump — **without** gameplay replication.

It embodies the project invariant that **networking transports simulation rather than owning it**: the subsystem reads no simulation state, owns no world data, and produces no gameplay effect. It is deliberately the *last* subsystem in the deterministic frame, downstream of every producer.

## 2. Ownership boundaries

| Data | Owner |
|---|---|
| OS socket / address / platform handle | **`PlatformSockets.cpp` only** (the one OS-touching TU) |
| Opaque transport endpoint identity | `net::TransportEndpoint` (value handle crossing the seam) |
| Connection records + lifecycle state | **`net::ConnectionTable` (Sprint-006)** |
| Session membership + observers | **`net::Session` / `net::SessionService` (Sprint-006)** |
| Message id → handler routing | **`net::MessageRegistry` (Sprint-006)** |
| Per-tick receive/admit/send pump | **`net::HostServer` (Sprint-006)** |
| Simulation / entity / world state | **Not owned** — networking reads none of it |

Networking owns only its own transient connection/session/queue bookkeeping. It never owns, writes, or reads simulation; its dependency edges on World / EntityRegistry / BubbleManager / TransitionManager are **ordering-only** (to guarantee networking-last), not data reads.

## 3. Components

- `net::NetTypes.h` — value types (`ConnectionId`, `MessageId`, `TransportEndpoint`) and enums (`Channel`, `HandshakeState`, `ConnectionState`, `DisconnectReason`, `TransportOutcome`) with `Name()` helpers. Engine-free, OS-free.
- `net::ProtocolConstants.h` — `kProtocolMagic`, `kProtocolVersion`, control-id range `[0x0000–0x00FF]` vs data-id range `>= 0x0100`, control message ids, `IsControlId`/`IsDataId` (ADR-010).
- `net::PacketHeader.h` / `.cpp` — the fixed **18-byte** wire header; field-by-field little-endian `SerializeHeader`/`DeserializeHeader`/`ValidateHeader`; flags (`kFlagFragment`/`kFlagAck`/`kFlagControl`); no packing/`memcpy` assumptions (ADR-010).
- `net::ByteCursor.h` — `ByteWriter`/`ByteReader`, little-endian by shift/mask, `Expected` on over/underflow.
- `net::NetworkingConfig` / `net::TransportConfig` (+ `.cpp`) — `FromConfig` parsing; engine-free.
- `net::ITransport` — the engine-free / OS-free transport seam (`Bind`/`Listen`/`Poll`/`Send`/`Close*`/`Accepted`/`Received`). `net::MockTransport` and `net::LoopbackTransport` — header-only test substrate.
- `net::MessageRegistry` / `net::MessageRegistryService` — id→handler routing over the control-id range; on-demand resolution, no retained pointers.
- `net::Connection` + `net::ConnectionTable` (+ `.cpp`) — capacity-bounded record store; ascending, non-reused `ConnectionId`; `ScanTimeouts`, `BeginDisconnect`/`DisconnectIntent`, tick-derived `TicksFromMs`.
- `net::Handshake` (+ `.cpp`) / `net::IConnectionAuthenticator` — stateless host-side handshake with deterministic `ServerNonce(id)`; `AllowAllAuthenticator`.
- `net::Session` / `net::SessionService` / `net::ISessionObserver` — membership + observer notifications.
- `net::PacketQueues.h`, `net::Reliability` (+ `.cpp`), `net::Fragmentation` (+ `.cpp`) — outgoing/incoming queue store, reliability bookkeeping, fragmentation/reassembly.
- `net::HostServer` (+ `.cpp`) — the per-tick pump: non-blocking receive → admit/handshake → route → send, byte-budgeted, deterministic; `DroppedDatagrams` / `RejectedCapacity` counters.
- `net::NetworkingService` (+ `.cpp`) — the umbrella `IService` + `ITickable`; owns the host pump; `Dependencies() = {World, EntityRegistry, BubbleManager, TransitionManager}` (ordering-only).
- `net::TransportService` (+ `.cpp`) — owns the `std::unique_ptr<ITransport>` (real in the engine build, null/mock in tests).
- `net::NetworkDiagnostics` (+ `.cpp`) — read-only `Statistics` / `DescribeState` / `DescribeConnection` / `DumpConnections` / `ValidateConsistency`.
- `adapters::PlatformTransport.h` + `adapters::PlatformSockets.cpp` — the single OS-touching TU: a non-blocking Winsock UDP `ITransport` behind `CreatePlatformTransport`. `tests/support/NullPlatformTransport.cpp` — the test-build factory returning an OS-free transport.

## 4. Per-tick pass (`HostServer::Tick`) and networking-last placement

`NetworkingService` ticks once per frame at `tick_order::kNetworking` — the **highest** key, strictly after every producer. `kNetworking` is defined relationally (`kNetworking > kPlugins`); no future producer key is assigned or depended upon. The deterministic frame order is:

World (100) → EntityRegistry (200) → Bubble (300) → ALifeTransition (350) → … → **Networking (900, last)**.

The per-tick pass, driven from a tick-derived control flow (no wall-clock in control decisions):

1. **Poll** the transport non-blockingly, bounded by the per-tick byte budget (`maxBytesPerTick`) — never blocks the frame.
2. **Admit / handshake** new endpoints through the `ConnectionTable` and stateless `Handshake` (capacity- and version-checked; rejects counted, not thrown).
3. **Route** validated datagrams by header id through the `MessageRegistry`.
4. **Scan timeouts** (tick-derived) and run the unified disconnect flow; notify `ISessionObserver`s.
5. **Send** queued outgoing datagrams (reliability/fragmentation bookkeeping applied), byte-budgeted.

The pass is deterministic and replay-safe: an identical received-buffer + tick sequence twice yields identical connection/session state and identical outgoing queues (E-G3-N).

## 5. One Platform Boundary (ADR-009)

All OS socket calls and types — `winsock2.h`/`ws2tcpip.h`, `SOCKET`, `sockaddr_in`, `WSAStartup`, `recvfrom`, `sendto` — are confined to `src/adapters/PlatformSockets.cpp`, the one OS-touching TU. Only an opaque `net::TransportEndpoint` value handle crosses the engine-free `net::ITransport` seam; no `sockaddr` or platform type is ever exposed above it. `ws2_32.lib` is linked via a TU-scoped `#pragma comment(lib)` so the dependency never reaches the project link line. The test build links `tests/support/NullPlatformTransport.cpp` (and the mock/loopback transports) instead, so **no OS socket header enters the test build**. Every fallible OS call returns `core::Expected` / `net::TransportOutcome` — exception-free (ADR-007).

## 6. Wire protocol contract (ADR-010)

The packet header is a fixed **18-byte** layout serialized field-by-field in little-endian order, with no struct packing or `memcpy` assumptions, so it is byte- and value-identical across MSVC (engine build) and GCC (tests) — verified by the header round-trip + endianness gate (E-G2-N). Message ids are partitioned into a control range `[0x0000–0x00FF]` and a data range `>= 0x0100`; ids are additive (never renumbered) and the layer order is fixed. `ByteWriter`/`ByteReader` enforce bounds with `Expected` on over/underflow.

## 7. Concurrency & determinism boundary (ADR-011)

Networking is **single-threaded** for Sprint-006. All control flow is tick-derived (no wall-clock leaks into decisions), `Poll` is non-blocking, and the subsystem introduces no IO thread. This removes race and lifetime hazards by construction and makes the network tick deterministically replayable. ADR-011 also fixes the future thread-handoff boundary should an IO thread ever be introduced — behind the transport seam, never above it.

## 8. Composition-root integration

In `Bootstrap.cpp` the services are registered after the Transition service in the order `TransportService → MessageRegistryService → SessionService → NetworkingService`, owned by the `ServiceRegistry`. `NetworkingService` is injected with the `NetworkingConfig`, `TransportConfig`, the `ITransport&` (from `TransportService`), the `Session&`, the `MessageRegistry&`, and an `IConnectionAuthenticator&` (`AllowAllAuthenticator`), and subscribed to the single existing `FrameDispatcher` at `tick_order::kNetworking`. `Initialize` computes `msPerTick` (default 16) and starts the host pump. Teardown unsubscribes networking **first** (reverse order), before `ShutdownAll`. `BootstrapTests` shows **5** frame subscribers with networking last (`static_assert(kNetworking > kPlugins)`).

## 9. Boundary & invariants

Only `EngineAdapters.cpp` includes engine headers (One Engine Boundary); only `PlatformSockets.cpp` includes OS socket headers (One Platform Boundary). Every other net TU — types, codec, transport seam, registry, table, handshake, session, queues/reliability/fragmentation, host server, services, diagnostics — is engine-free and OS-free, covered by the null/loopback + mock test build. ADR-007 (no exceptions, no RTTI, `Expected<T>`/value outcomes, `/W4 /WX`, C4530-clean), ADR-009, ADR-010, and ADR-011 hold throughout. Host Authority and Deterministic Simulation are preserved; networking owns no simulation.

## 10. Future-sprint consumption & extension points

The subsystem reserves — but does not implement — the seams that Sprints 007–016 will consume:

- **Player Lifecycle (Sprint-007):** connection/session membership + `ISessionObserver` are the join/leave surface; the networked player-position source arrives behind the existing `world::IPlayerPositionSource` (Sprint-004).
- **Replication (Sprints 008–010):** `world::TransitionResult` (the per-tick online/offline delta) feeds networking as a **sink** — networking is downstream (last in the frame) and will serialize deltas onto the wire; the data-id range (`>= 0x0100`), reliability, and fragmentation bookkeeping are the reserved carriers.
- **Persistence (Sprints 011–012):** connection/session state is transient and reconstructable; no new persistence format is introduced here.
- **Extensibility / Diagnostics (Sprints 013–016):** `MessageRegistry` additive ids and `NetworkDiagnostics` (bandwidth/RTT/trace fields reserved) are the extension points.

No future producer `tick_order` value is assigned or depended upon, and no non-goal (replication, snapshot, delta, prediction, persistence, player-sync, Lua, auth-logic, encryption-logic, IO-thread) is implemented.

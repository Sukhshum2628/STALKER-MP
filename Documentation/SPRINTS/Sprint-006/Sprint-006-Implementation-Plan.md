# Sprint-006 — Host Networking Infrastructure — Implementation Plan (AUTHORITATIVE)

- **Status:** Active implementation roadmap. Architecture **FROZEN** (`Multiplayer/docs/Sprint-006-Architecture.md`, refined 2026-07-10). This plan implements it verbatim and introduces no new architectural decisions, ADRs, or ownership changes.
- **Roles:** Principal Architect authored this plan; ChatGPT (Implementation Engineer) converts each step into one implementation prompt; Antigravity (Verification Engineer) verifies each step before the next begins.
- **Frozen invariants (immutable here):** Preserve Before Replace, Host Authority, One World, One ALife Simulation, One Engine Boundary, **One Platform Boundary (ADR-009)**, **Wire Protocol & Compatibility Contract (ADR-010)**, **Networking Concurrency & Determinism Boundary (ADR-011, single-threaded)**, Deterministic Simulation, ADR-007, ADR-008, Entity Registry / Bubble / Transition ownership, and the **networking-last invariant** (`kNetworking` is the highest `tick_order` key; no future producer values assigned or depended upon).

---

## 1. Sprint Overview

**Objective.** Build the complete host networking **infrastructure** — the transport/connection/session/message backbone every later multiplayer sprint depends on — without any gameplay networking. Deliver a host-authoritative, deterministic, single-threaded networking subsystem that integrates as one `ITickable` at `tick_order::kNetworking`, confines all OS socket access to a single platform translation unit behind the engine-free `ITransport` seam, and is fully unit-testable with mock/loopback transports (no OS in the test build).

**Implementation boundaries (in scope).** `NetworkingConfig`/`TransportConfig`; wire header + `ByteWriter`/`ByteReader`; `ITransport` seam + `MockTransport` + `LoopbackTransport`; `MessageRegistry` (+ service, control-range ids only); `ConnectionTable`/`Connection` + lifecycle; handshake state machine; timeout + unified disconnect; `Session`/`SessionService`/`ISessionObserver`; packet queues + reliability/fragmentation bookkeeping; `HostServer`; `NetworkingService`/`TransportService`; `tick_order::kNetworking` + Bootstrap wiring; the real UDP `PlatformSockets.cpp` transport; documentation + closure.

**Explicit non-goals (do NOT implement).** Replication, snapshots, delta compression, prediction, persistence, player/inventory synchronization, Lua networking, authentication *logic* (only the `AllowAllAuthenticator` seam), encryption *logic* (only the identity `IPacketCodec` seam), compression *logic* (only the identity `IPayloadCompressor` seam), reconnect *logic* (only the reserved token + `TryReclaim` seam), any IO worker thread (ADR-011 keeps Sprint-006 single-threaded), and any assignment of/dependency on future producer `tick_order` values.

---

## 2. Implementation Strategy

**Philosophy — OS-free first, platform TU last-but-one, wiring last (the Sprint-005 shape).** Steps 1–11 are entirely engine-free **and OS-free**, verified on GCC + MSVC using `MockTransport`/`LoopbackTransport`. Only Step 13 introduces OS socket headers, confined to `src/adapters/PlatformSockets.cpp` behind `ITransport` (One Platform Boundary, ADR-009). Composition-root wiring (Step 12) is deferred until the module is proven in isolation. This mirrors how Sprint-005 kept engine headers out of Steps 1–8 and confined them to Step 9.

**Why this order minimizes risk.**
- The highest-risk surfaces (OS sockets, ABI, composition-root lifetime) are isolated and deferred, so the bulk of the logic (protocol, state machines, ownership) is proven with deterministic in-memory transports first.
- Every step is purely additive and leaves the tree buildable; no step depends on a later step to compile.
- The mock/loopback substrate (Step 3) exists before any consumer, so every subsequent step is testable the moment it lands.
- Composition-root changes touch a frozen file (`Bootstrap.cpp`) exactly once (Step 12), after the module is self-contained.

**Subsystem dependency order (strict).** value types/config → wire codec → transport seam + fakes → message registry → connection table/lifecycle → handshake → timeout/disconnect → session → queues/reliability/fragmentation → host server → module services → tick-order + Bootstrap wiring → real platform transport → docs/closure. Each capability is introduced once, in dependency order, so no partial-ownership state ever exists.

**How implementation remains incremental.** One logical capability per step; each compiles and tests independently; the module is driven by `MockTransport` until the very end, so host↔client behavior is fully exercised before a real socket is ever opened. The single `kNetworking` subscription and the whole Bootstrap wiring arrive as one reviewable step after everything it wires already exists and passes tests.

**Namespace / layout conventions (mapping only, not new decisions).** New subsystem namespace `stalkermp::net`, headers under `include/stalkermp/net/`, sources under `src/net/`, the platform TU at `src/adapters/PlatformSockets.cpp` with its null/loopback counterpart under `tests/support/`, tests in `tests/NetworkTests.cpp` (+ `tests/BootstrapTests.cpp` for the wiring assertion). This mirrors the `world`/`adapters` split already in the tree.

---

## 3. Implementation Steps

> Every step: engine-free unless explicitly noted; OS-free until Step 13; ADR-007 (no exceptions, `Expected<T>`/value outcomes, C4530-clean, no iostream — `common::Format`); deterministic; additive; leaves the project buildable. Each is verified by Antigravity before the next begins.

### Step 1 — Config + value types
- **Objective.** Establish the engine/OS-free foundation types and configuration.
- **Scope.** Value types and config only; no logic.
- **Files to create.** `include/stalkermp/net/NetTypes.h` (`ConnectionId`, `MessageId`, `Channel` enum, `HandshakeState`, `ConnectionState`, `DisconnectReason`, `TransportOutcome`), `include/stalkermp/net/PacketHeader.h` (header struct, field widths, `kProtocolMagic`, `kProtocolVersion`, control-id range constants), `include/stalkermp/net/NetworkingConfig.h`, `include/stalkermp/net/TransportConfig.h` (+ `.cpp` for the two `FromConfig`).
- **Files to modify.** `xrMP.vcxproj` / `tests/xrMP_Tests.vcxproj` (add the config `.cpp`s and headers); new `tests/NetworkTests.cpp` wired into the test project.
- **Responsibilities.** Frozen value shapes (§5, §11 of the architecture); `FromConfig` parses `[networking]`/`[transport]` (missing→default, invalid→`InvalidArgument`), converts timeouts to a single stored ms value (tick conversion happens at Initialize, later step).
- **Dependencies.** Sprint-001 core (`Expected`, `ConfigStore`, `common::Format`).
- **Constraints.** No transport, no sockets, no logic beyond parsing/validation.
- **Expected outcome.** The vocabulary + config every later step consumes.
- **Definition of Done.** Types match architecture §5/§11; `FromConfig` validated; engine/OS-free; ADR-007-clean; tests green on GCC+MSVC.

### Step 2 — Wire codec (header serialize/deserialize)
- **Objective.** Deterministic, ABI-safe wire framing. *(Evidence Gate E-G2-N.)*
- **Scope.** `ByteWriter`/`ByteReader` + `PacketHeader` encode/decode only.
- **Files to create.** `include/stalkermp/net/ByteCursor.h` (writer/reader over a byte span, `Expected` on under/overflow), `src/net/PacketHeader.cpp` (field-by-field little-endian serialize/deserialize).
- **Files to modify.** vcxprojs; `tests/NetworkTests.cpp`.
- **Responsibilities.** ADR-010 layout exactly: fixed field order/width, little-endian, **no raw struct copy**; round-trip identity; boundary-length safety.
- **Dependencies.** Step 1.
- **Constraints.** No packing assumptions; no `memcpy` of structs; no exceptions.
- **Expected outcome.** A portable header codec identical on MSVC and GCC.
- **Definition of Done.** Round-trip + endianness + boundary tests pass on both toolchains; E-G2-N satisfied.

### Step 3 — `ITransport` seam + `MockTransport` + `LoopbackTransport`
- **Objective.** The engine/OS-free transport abstraction and the entire test substrate.
- **Scope.** Interface + two in-memory implementations; no real sockets.
- **Files to create.** `include/stalkermp/net/ITransport.h` (§4 shapes: `Bind/Listen/Poll/Send/CloseListen/CloseEndpoint/Accepted/Received`, `TransportEndpoint` opaque handle, `Channel`), `include/stalkermp/net/MockTransport.h`, `include/stalkermp/net/LoopbackTransport.h` (header-only or with `src/net/*.cpp`).
- **Files to modify.** vcxprojs; `tests/NetworkTests.cpp`.
- **Responsibilities.** Non-blocking `Poll` semantics; owned-byte-buffer value transfer across the seam; mock knobs for drop/duplicate/reorder/corrupt; loopback pairs two in-process endpoints.
- **Dependencies.** Steps 1–2.
- **Constraints.** No OS headers; no socket types cross the seam (ADR-009).
- **Expected outcome.** Every later step is testable without a network stack.
- **Definition of Done.** Mock/loopback move bytes deterministically; failure knobs work; engine/OS-free; tests green.

### Step 4 — `MessageRegistry` (+ `MessageRegistryService`)
- **Objective.** Id→handler routing for the control range only.
- **Scope.** Registry + its service; control-range ids registered; no gameplay ids.
- **Files to create.** `include/stalkermp/net/MessageRegistry.h`, `src/net/MessageRegistry.cpp`, `include/stalkermp/net/MessageRegistryService.h`, `src/net/MessageRegistryService.cpp`.
- **Files to modify.** vcxprojs; `tests/NetworkTests.cpp`.
- **Responsibilities.** Register control ids (`0x0000–0x00FF`); duplicate id = init failure; unknown id = drop + counter; id-ordered dispatch (deterministic); registration API reserved for later sprints (data range `>= 0x0100`).
- **Dependencies.** Steps 1–2.
- **Constraints.** No gameplay message types; `IService` only (not `ITickable` — the registry does not tick).
- **Expected outcome.** Deterministic routing table.
- **Definition of Done.** Duplicate/unknown handling + id-ordered dispatch tested; ADR-010 id ranges honored.

### Step 5 — `ConnectionTable` + `Connection` (storage + lifecycle enum)
- **Objective.** The connection store and its state storage — no transport interaction yet.
- **Scope.** `Connection` record + capacity-bounded `ConnectionTable` (sorted-vector + hash accelerator, ascending `ConnectionId`, BC-005).
- **Files to create.** `include/stalkermp/net/Connection.h`, `include/stalkermp/net/ConnectionTable.h`, `src/net/ConnectionTable.cpp`.
- **Files to modify.** vcxprojs; `tests/NetworkTests.cpp`.
- **Responsibilities.** Deterministic ascending `ConnectionId` assignment (host-owned, non-reused-in-session); capacity = `maxConnections`; on-demand resolution by id (no `Connection*` retained across ticks); reserved null `PlayerSlot`/`ReconnectToken` fields; `ValidateConsistency` for ordering/accelerator.
- **Dependencies.** Steps 1, 3 (for `TransportEndpoint` handle type).
- **Constraints.** No handshake/timeout/queue logic yet; no transport calls.
- **Expected outcome.** Deterministic connection storage.
- **Definition of Done.** Add/resolve/remove keep ascending order + accelerator consistent; capacity enforced; tests green.

### Step 6 — Handshake state machine
- **Objective.** Deterministic, single-step-per-tick handshake over control messages.
- **Scope.** `Hello → Challenge → Response → Established`; illegal-transition drops; `IConnectionAuthenticator` seam with `AllowAllAuthenticator`.
- **Files to create.** `include/stalkermp/net/Handshake.h`, `src/net/Handshake.cpp`, `include/stalkermp/net/IConnectionAuthenticator.h` (+ `AllowAllAuthenticator`).
- **Files to modify.** vcxprojs; `tests/NetworkTests.cpp` (driven via `MockTransport`).
- **Responsibilities.** One state advance per tick per connection; protocol-version + nonce echo (no crypto); authenticator called at `Response→Established`; illegal transition → drop with reason.
- **Dependencies.** Steps 2, 4, 5.
- **Constraints.** No blocking; no auth *logic* beyond allow-all; no encryption.
- **Expected outcome.** A connection can reach `Established` deterministically.
- **Definition of Done.** Happy path + every illegal transition tested; single-step-per-tick proven; version-mismatch rejected pre-allocation.

### Step 7 — Timeout + unified disconnect flow
- **Objective.** Tick-derived timeouts and one disconnect path.
- **Scope.** `connectionTimeoutMs`/`handshakeTimeoutMs` → ticks; `BeginDisconnect(id, reason)`; `DisconnectReason` enum end-to-end.
- **Files to create.** none (extend `Connection`/`ConnectionTable`/`Handshake`).
- **Files to modify.** `src/net/ConnectionTable.cpp`, `Handshake.cpp` (+ headers); `tests/NetworkTests.cpp`.
- **Responsibilities.** Detect timeouts in ascending `ConnectionId` order against the frame tick (no wall-clock control flow); unified `BeginDisconnect` → best-effort `Bye` → `Disconnecting` → release → table removal; always reaches `Disconnected`.
- **Dependencies.** Step 6.
- **Constraints.** No wall-clock in control flow; no reconnect logic.
- **Expected outcome.** Deterministic, leak-free disconnect for all reasons.
- **Definition of Done.** Timeout math + every reason path tested; no orphan endpoint/queue; deterministic order.

### Step 8 — `Session` + `SessionService` + `ISessionObserver`
- **Objective.** Authoritative membership + join/leave observer seam.
- **Scope.** `Session` (members keyed by `ConnectionId`, reserved `PlayerSlot`/`ReconnectToken`), `SessionService`, `ISessionObserver`, reserved `TryReclaim`.
- **Files to create.** `include/stalkermp/net/Session.h`, `src/net/Session.cpp`, `include/stalkermp/net/SessionService.h`, `src/net/SessionService.cpp`, `include/stalkermp/net/ISessionObserver.h`.
- **Files to modify.** vcxprojs; `tests/NetworkTests.cpp`.
- **Responsibilities.** `Admit` on handshake `Established`; `Remove` on any disconnect; observers fire exactly once; capacity-checked; ascending iteration; `TryReclaim` returns "not supported"; session state transient/reconstructable.
- **Dependencies.** Steps 5–7.
- **Constraints.** No persistence; no reconnect logic; `IService` (not `ITickable`).
- **Expected outcome.** Host-authoritative membership with future-proof hooks.
- **Definition of Done.** Join/leave fire once; capacity rejection; observer subscription tested.

### Step 9 — Packet queues + reliability/fragmentation bookkeeping
- **Objective.** Per-connection/per-channel queues and the reliable/fragment structures. *(Gate: failure injection.)*
- **Scope.** Bounded outgoing/incoming queues; reliable ack/retransmit bookkeeping (sliding window, ack bitfield); fragmentation/reassembly (bounded, time-boxed).
- **Files to create.** `include/stalkermp/net/PacketQueues.h`, `src/net/PacketQueues.cpp`, `include/stalkermp/net/Reliability.h`, `src/net/Reliability.cpp`, `include/stalkermp/net/Fragmentation.h`, `src/net/Fragmentation.cpp`.
- **Files to modify.** `Connection.h` (queue handles as indices, not pointers); vcxprojs; `tests/NetworkTests.cpp`.
- **Responsibilities.** Bounds per config; deterministic overflow (reliable→disconnect `TransportError`, unreliable→drop-oldest); fragment sub-header `(messageId, index, count)`; reassembly budget `reassemblyBudgetTicks`; newest-wins on unreliable.
- **Dependencies.** Steps 2, 5.
- **Constraints.** No aliasing into transport memory (owned buffers); bounded reassembly.
- **Expected outcome.** Reliable-channel recovery and graceful unreliable degradation under loss/reorder.
- **Definition of Done.** Overflow policies, ack/retransmit, and fragment reassembly tested under mock drop/reorder/duplicate; no leak.

### Step 10 — `HostServer`
- **Objective.** The authoritative endpoint driving accept/admission/heartbeat. *(Evidence Gate E-G3-N replay.)*
- **Scope.** Accept loop (bounded per tick), admission policy, heartbeat (Ping/Pong), disconnect orchestration; host↔fake-client integration over loopback.
- **Files to create.** `include/stalkermp/net/HostServer.h`, `src/net/HostServer.cpp`, test-only fake client driver in `tests/`.
- **Files to modify.** vcxprojs; `tests/NetworkTests.cpp`.
- **Responsibilities.** Drain `transport.Accepted()` count-bounded; capacity admission; issue/track keepalives; stamp frame tick on control messages; drive handshake/session/queues; reads no simulation state.
- **Dependencies.** Steps 3–9.
- **Constraints.** No simulation access; single-threaded; deterministic.
- **Expected outcome.** Full handshake→connected→ping→disconnect over loopback.
- **Definition of Done.** Integration + capacity/version rejection tested; **E-G3-N replay** (identical received-buffer+tick sequence twice → identical state + outgoing queues) passes.

### Step 11 — `NetworkingService` umbrella + `TransportService`
- **Objective.** The module aggregation and the single tick — still mock/loopback.
- **Scope.** `NetworkingService` (`IService` + `ITickable`, the one `kNetworking` subscriber) owning `HostServer`/`ConnectionTable`; `TransportService` owning the injected `ITransport`.
- **Files to create.** `include/stalkermp/net/NetworkingService.h`, `src/net/NetworkingService.cpp`, `include/stalkermp/net/TransportService.h`, `src/net/TransportService.cpp`, `include/stalkermp/net/NetworkDiagnostics.h` (+ `.cpp`).
- **Files to modify.** vcxprojs; `tests/NetworkTests.cpp`.
- **Responsibilities.** `Dependencies() = {World, EntityRegistry, BubbleManager, TransitionManager}` (ordering only); `Initialize` validates config, converts timeouts to ticks, binds via host **only if enabled**; `Tick` runs the deterministic per-tick pass (§7); `Shutdown` drains/closes idempotently; diagnostics (`Statistics/DescribeState/DumpConnections/ValidateConsistency`).
- **Dependencies.** Steps 3–10.
- **Constraints.** No dispatcher subscription and no Bootstrap wiring yet (Step 12); no simulation ownership.
- **Expected outcome.** A self-contained, tickable module driven by a test transport.
- **Definition of Done.** Lifecycle + tick + diagnostics tested with mock/loopback; dependency contract correct; engine/OS-free.

### Step 12 — `tick_order::kNetworking` + Bootstrap composition-root wiring
- **Objective.** Integrate the proven module into the runtime.
- **Scope.** Add the single additive `tick_order::kNetworking` (highest key) to `FrameDispatcher.h`; wire the services in `Bootstrap.cpp`.
- **Files to modify.** `include/stalkermp/core/FrameDispatcher.h` (one additive constant), `src/core/Bootstrap.cpp` (register TransportService → MessageRegistryService → SessionService → NetworkingService after the Transition service; inject transport via `adapters::CreatePlatformTransport(config)` [null/loopback in tests]; subscribe `NetworkingService` at `kNetworking`; reverse-order teardown), `tests/BootstrapTests.cpp` (subscriber count 4 → 5; wiring/teardown assertions).
- **Responsibilities.** Networking-last placement (highest key); Runtime owns the transport (destroyed with Runtime, not dereferenced at destruction — the Sprint-005 gateway precedent); unsubscribe networking before `ShutdownAll`.
- **Dependencies.** Step 11 (+ Step 13's factory symbol; in the test build the null/loopback factory is linked, so wiring compiles and runs without the real socket TU).
- **Constraints.** One additive constant only; the only frozen-file (`FrameDispatcher.h`, `Bootstrap.cpp`) touch; no future producer values.
- **Expected outcome.** Networking runs each frame at the highest key in both builds.
- **Definition of Done.** Bootstrap wires 5 subscribers, networking last; reverse-order teardown; `BootstrapTests` updated and green; MSVC Release clean.

### Step 13 — Real UDP transport (`PlatformSockets.cpp`) — the one OS-touching TU
- **Objective.** The concrete `ITransport` over OS sockets. *(Evidence Gate E-G1-N.)*
- **Scope.** Non-blocking UDP transport behind `ITransport`; `adapters::CreatePlatformTransport` factory; null/loopback factory for tests.
- **Files to create.** `src/adapters/PlatformSockets.cpp` (the ONLY OS-header TU), factory decl in `include/stalkermp/adapters/PlatformTransport.h`, null/loopback factory in `tests/support/NullPlatformTransport.cpp`.
- **Files to modify.** `xrMP.vcxproj` (per-file OS/socket lib settings scoped to this TU — Winsock link; mirrors the `EngineAdapters.cpp` per-file pattern); `tests/xrMP_Tests.vcxproj` (link the null/loopback counterpart, not the real TU).
- **Responsibilities.** `Bind/Listen/non-blocking Poll/Send/Close` via OS sockets; `IPacketCodec` (identity) at the outermost boundary; stay within `maxBytesPerTick`; return `Expected`/value outcomes, never throw; no socket type crosses `ITransport`.
- **Dependencies.** Steps 3, 11, 12.
- **Constraints.** OS headers confined here (ADR-009); compiles under `EngineAbi.props`; no blocking.
- **Expected outcome.** A real host binds/listens/pumps without blocking the tick.
- **Definition of Done.** Test build links only the null/loopback transport (One Platform Boundary intact); **E-G1-N** passes — real UDP TU compiles under `EngineAbi.props`, exception-free, non-blocking, within budget; Antigravity real-socket loopback smoke passes.

### Step 14 — Documentation + sprint closure
- **Objective.** Subsystem doc + closure.
- **Scope.** `Multiplayer/docs/Networking.md`; status docs.
- **Files to create.** `Multiplayer/docs/Networking.md`.
- **Files to modify.** `Documentation/SPRINTS/Sprint-006-Host-Networking.md` (Status → Closed), `Documentation/AI/CURRENT_STATUS.md`, `Documentation/AI/SESSION_LOG.md`.
- **Responsibilities.** Record delivered subsystem, evidence-gate outcomes (E-G1-N/E-G2-N/E-G3-N/E-G4-N), ADR-009/010/011 as implemented, final test counts, build status.
- **Dependencies.** Steps 1–13.
- **Constraints.** Documentation only; no code change.
- **Expected outcome.** Sprint-006 closed and consistent across all status docs.
- **Definition of Done.** Docs written; status synchronized to Closed/Verified; Sprint-006 Definition of Done (§6) satisfied.

---

## 4. Verification Expectations (per step)

| Step | Antigravity verifies | Build validation | Runtime validation | Unit testing | Regression |
|---|---|---|---|---|---|
| 1 | Config parse + type shapes | GCC+MSVC test build; ADR-007 | — | `FromConfig` valid/invalid; type defaults | full suite green |
| 2 | Header round-trip identity, endianness | both toolchains | — | round-trip, boundary lengths (E-G2-N) | no regressions |
| 3 | Seam engine/OS-free; mock/loopback determinism | test build; no OS headers | — | byte transfer, drop/reorder knobs | — |
| 4 | Id-ordered dispatch; duplicate/unknown | test build | — | registry behavior | — |
| 5 | Ascending `ConnectionId`, accelerator, capacity | test build | — | add/resolve/remove, ValidateConsistency | — |
| 6 | Single-step handshake; illegal drops; version reject | test build | — | happy + all illegal paths | — |
| 7 | Tick-derived timeout; unified disconnect; no leak | test build | — | timeout math, all reasons | — |
| 8 | Join/leave fire once; capacity; observers | test build | — | admit/remove/observer | — |
| 9 | Overflow policy, ack/retransmit, reassembly | test build | — | failure-injection (drop/reorder/dup) | — |
| 10 | Host integration; **replay determinism** | test build | loopback host↔fake-client | full flow, capacity/version reject (E-G3-N) | — |
| 11 | Lifecycle + single tick + diagnostics | test build | mock/loopback tick | Initialize/Tick/Shutdown, diagnostics | — |
| 12 | 5 subscribers, networking last; reverse teardown | MSVC Release clean | frame dispatch smoke | Bootstrap wiring/teardown | full suite green, no regressions |
| 13 | One Platform Boundary; **real UDP** compile+smoke | **both builds**; `EngineAbi.props`; test build links null/loopback only | real-socket bind/listen/poll smoke (E-G1-N) | transport contract via null | no regressions |
| 14 | Docs match delivery; status consistent | — | — | — | final counts recorded |

**Global regression expectation.** After every step, the entire existing suite (Sprints 001–005 + accumulated Sprint-006 tests) remains green on GCC + MSVC with zero new warnings; MSVC Release stays clean under `EngineAbi.props`; One Engine Boundary and (from Step 13) One Platform Boundary remain intact (test build links no engine TU and no real-socket TU).

---

## 5. Risk Analysis (per step, with order-based mitigation)

- **Step 1–2 (types/codec).** *ABI risk:* wire layout differing across toolchains → mitigated by field-by-field serialization (no raw copy) and the E-G2-N cross-toolchain gate. *Determinism:* fixed field order.
- **Step 3 (transport seam).** *ABI/ownership risk:* OS types leaking → mitigated by the seam admitting only owned byte buffers; mock/loopback keep it OS-free (ADR-009). *Lifetime:* buffers by value, no aliasing.
- **Step 4 (registry).** *Determinism:* id-ordered dispatch; duplicate=init failure prevents ambiguous routing.
- **Step 5 (table).** *Ownership/lifetime:* connections owned by the table; resolve-by-id (no retained `Connection*`) generalizes the E-G2 discipline → no dangling. *Determinism:* ascending id + accelerator invariant.
- **Step 6–7 (handshake/timeout).** *Determinism:* single-step-per-tick, tick-derived timeouts (no wall-clock control flow). *Lifetime:* unified disconnect guarantees no orphan queue/endpoint.
- **Step 8 (session).** *Ownership:* session holds ids, not `Connection*`; observers non-owning. *Integration:* fire-once join/leave prevents double-accounting downstream.
- **Step 9 (queues/reliability).** *Determinism:* bounded queues + explicit overflow policy; reassembly time-boxed. *Lifetime:* owned buffers; queue handles are indices.
- **Step 10 (host).** *Determinism:* the E-G3-N replay gate is the strongest guard against hidden nondeterminism. *Ownership:* host reads no simulation.
- **Step 11 (services).** *Ownership risk (highest project-level):* networking must not own simulation → mitigated by const-only dependency edges (ordering) and no simulation handles; single ticking service.
- **Step 12 (wiring).** *Integration/lifetime risk:* composition-root/frame lifetime → mitigated by touching frozen files exactly once, Runtime-owned transport with the Sprint-005 no-deref-at-destruction precedent, reverse-order teardown, and the `BootstrapTests` subscriber-count assertion catching miswiring. *Determinism:* networking-last (highest key).
- **Step 13 (platform TU).** *ABI/platform risk (isolated, deferred):* OS/Winsock headers → confined to one TU under `EngineAbi.props` (ADR-009), test build links the null/loopback counterpart; E-G1-N validates exception-free non-blocking compile+smoke. *Threading:* single-threaded (ADR-011) removes race/lifetime hazards.
- **Step 14 (docs).** No code risk.

**How order mitigates overall.** All ABI/OS/lifetime hot-spots (2, 3, 12, 13) are either isolated behind seams or deferred until the logic they support is already proven with deterministic in-memory transports; the one composition-root change lands after the module passes its own tests.

---

## 6. Completion Criteria

Sprint-006 implementation is **complete** when:
1. All 14 steps are implemented, each independently verified by Antigravity, tree buildable after each.
2. The networking module runs as a single `ITickable` at `tick_order::kNetworking` (highest key), integrated via Bootstrap with reverse-order teardown; `BootstrapTests` shows 5 subscribers, networking last.
3. One Engine Boundary **and** One Platform Boundary hold: only `EngineAdapters.cpp` includes engine headers; only `PlatformSockets.cpp` includes OS socket headers; the test build links neither (null/loopback + mock).
4. ADR-007, ADR-008, ADR-009, ADR-010, ADR-011 all conformed to and (009/010/011) implemented.
5. Evidence gates: **E-G1-N PASSED, E-G2-N PASSED, E-G3-N PASSED**; E-G4-N confirmed (non-blocking).
6. Full suite green on GCC + MSVC, MSVC Release clean under `EngineAbi.props`, zero new warnings, no regressions; real-socket loopback smoke passes.
7. No non-goal implemented; no future producer `tick_order` value assigned or depended upon; no simulation owned by networking.
8. Subsystem doc written; all status docs synchronized to Closed/Verified.

---

## 7. Sprint Closure Checklist

Before Sprint-006 is declared **Implemented / Verified / Closed / Frozen**:

- [ ] Steps 1–14 implemented and each independently Antigravity-verified.
- [ ] Project buildable after every step (GCC + MSVC), zero new warnings.
- [ ] `NetworkingService` ticks once at `tick_order::kNetworking` (highest key); Bootstrap wiring + reverse-order teardown verified; 5 frame subscribers.
- [ ] One Engine Boundary intact (no engine header outside `EngineAdapters.cpp`).
- [ ] One Platform Boundary intact (no OS socket header outside `PlatformSockets.cpp`; test build links null/loopback only).
- [ ] Networking owns **no** simulation state; dependency edges are ordering-only, const-reference discipline held.
- [ ] ADR-009 / ADR-010 / ADR-011 implemented; ADR-007 / ADR-008 preserved.
- [ ] Evidence gates E-G1-N / E-G2-N / E-G3-N PASSED; E-G4-N confirmed.
- [ ] Deterministic replay of the network tick verified (identical state + outgoing queues).
- [ ] Wire protocol contract honored (header layout, little-endian, control/data id ranges, additive ids, fixed layer order).
- [ ] No non-goal implemented (no replication/snapshot/delta/prediction/persistence/player-sync/Lua/auth-logic/encryption-logic/IO-thread).
- [ ] Final test counts + build status recorded; `Networking.md` written; `Sprint-006-Host-Networking.md`, `CURRENT_STATUS.md`, `SESSION_LOG.md` updated to Closed/Verified.
- [ ] Sprint-006 Definition of Done (§6) fully satisfied; architecture unchanged (frozen).

---

## 8. Recommended implementation sequence (authoritative execution order)

1. Config + value types.
2. Wire codec (header serialize/deserialize) — **E-G2-N**.
3. `ITransport` seam + `MockTransport` + `LoopbackTransport`.
4. `MessageRegistry` (+ service), control-range ids.
5. `ConnectionTable` + `Connection` (storage + lifecycle enum).
6. Handshake state machine (+ `IConnectionAuthenticator` / allow-all).
7. Timeout + unified disconnect flow.
8. `Session` + `SessionService` + `ISessionObserver`.
9. Packet queues + reliability/fragmentation bookkeeping.
10. `HostServer` (+ fake-client integration) — **E-G3-N replay**.
11. `NetworkingService` umbrella + `TransportService` + diagnostics.
12. `tick_order::kNetworking` + Bootstrap composition-root wiring.
13. Real UDP `PlatformSockets.cpp` transport (One Platform Boundary) — **E-G1-N**.
14. Documentation + sprint closure.

Steps 1–11 (and the test work in 12) are engine-free and OS-free, verified with mock/loopback transports; Step 13 is the single OS-touching translation unit; Steps 12–14 add wiring, the real transport, and closure. Execute strictly in this order, verifying each step before beginning the next.

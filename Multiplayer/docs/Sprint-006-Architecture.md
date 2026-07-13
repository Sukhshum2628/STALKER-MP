# Sprint-006 — Host Networking Infrastructure — Architecture (PROPOSED, pre-freeze)

- **Status:** Proposed architecture package for review and freeze. No implementation.
- **Date:** 2026-07-10.
- **Author role:** Principal Architect.
- **Subordinate to:** ADR-007 (Engine ABI Contract), ADR-008 (Engine State Mutation Boundary), the architecture documents (`04_World.md`, `05_ALife.md`, `06_Multiplayer.md`), RFC-0001/0002/0003, and — if adopted — the new **ADR-009 / ADR-010 / ADR-011** proposed in §15.
- **Scope:** Networking **infrastructure only**. No replication, snapshots, delta, prediction, persistence, player/inventory/Lua sync, or authentication implementation. This sprint builds the transport/connection/session/message backbone every later multiplayer sprint depends on.

---

## 0. Positioning within the frozen pipeline

The simulation pipeline is complete and authoritative:

```
World (100) → EntityRegistry (200) → Bubble (300) → Transition (350)
```

Networking is layered **beneath and after** simulation. It **consumes** simulation state and **transports** information; it **never owns** simulation state and never mutates engine or ALife state. This is the load-bearing invariant of the whole sprint: *simulation is authoritative; networking is a courier.* Concretely, no networking type may hold a non-const reference to `WorldManager`, `EntityRegistry`, `BubbleManager`, or `TransitionManager`, and no networking code path may call the ALife switch gateway or any engine mutator.

A **new boundary** appears here, symmetric to One Engine Boundary: the **One Platform Boundary** — all OS socket / Winsock calls are confined to a single adapter translation unit (`PlatformSockets.cpp`), with an engine-free `ITransport` seam above it and a null/loopback counterpart for the test build. This is the spine of the sprint and the subject of ADR-009.

---

## 1. Networking Module

**Responsibility.** Own the lifecycle and composition of the networking subsystem: the host server, transport, connections, sessions, message routing, and diagnostics. It is the single aggregation point the composition root registers, mirroring how the World/Bubble/Transition subsystems are aggregated.

**Ownership.** The Networking Module owns (via `unique_ptr`) the `HostServer`, the `ITransport` instance (created through a factory, real in the engine build, loopback/null in tests), the `ConnectionTable`, the `Session`, and the `MessageRegistry`. It is itself owned by the `ServiceRegistry` as `NetworkingService` (§10). It owns no simulation object; it holds **read-only const references** to the simulation subsystems it must observe (initially none are required for pure infrastructure — see §7).

**Lifecycle.**
- *Construction:* value-owning; receives a validated `NetworkingConfig` and the transport factory result. No sockets opened in the constructor (Preserve Before Replace: nothing observable happens until Initialize).
- *Initialize (`core::Expected<void>`):* validates config, constructs the message registry (registers core control message IDs only), constructs the connection table sized to `maxConnections`, and — **only if `NetworkingConfig.enabled`** — asks the `HostServer` to bind/listen through the transport. Binding failure returns `Expected` failure; it never throws (ADR-007).
- *Shutdown (`noexcept`):* drains and closes all connections (graceful disconnect control message best-effort), stops the host server, closes the listen socket, and releases transport resources. Idempotent. Must complete even on partial init (mirrors the frame-bridge remove-first discipline).

**Dependency registration.** `NetworkingService::Dependencies()` = `{ "World", "EntityRegistry", "BubbleManager", "TransitionManager" }`. Rationale: networking initializes **after** the full simulation pipeline exists and shuts down **before** it, guaranteeing that no packet is ever dispatched into a half-constructed or already-torn-down simulation. (It does not *use* these as owners; the dependency edge encodes ordering, exactly as Sprint-005 declared `BubbleManager` without owning it.)

**Configuration.** Injected `NetworkingConfig` parsed from `server.cfg` via `NetworkingConfig::FromConfig` (§11), following the `EntityRegistryConfig` / `BubbleConfiguration` precedent (missing keys keep defaults; invalid values fail with `InvalidArgument`).

**Diagnostics.** Exposes read-only `NetworkDiagnostics` (§9): aggregate counters, per-connection stats, and validation. All read-only, deterministic, iostream-free (`common::Format`).

**Bootstrap integration.** Registered in `Bootstrap.cpp` after the Transition service; owned by `ServiceRegistry`; the transport is created via `adapters::CreatePlatformTransport(config)` (Runtime-owned, loopback in tests) and injected. Teardown unsubscribes the networking tick before `ShutdownAll`, reverse-order (transport released with the Runtime, never dereferenced at destruction — the Sprint-005 gateway precedent).

**FrameDispatcher integration.** Subscribed at a single new additive `tick_order::kNetworking` slot (§7). One subscription, one deterministic phase per frame. No new engine `seqFrame`.

---

## 2. Host Server

**Responsibility.** The authoritative endpoint. Owns the listen socket lifecycle (via transport), accepts inbound connections, and is the single arbiter of connection admission and session membership. It is *host-authoritative by construction*: there is exactly one `HostServer`; clients are never authorities.

**Ownership.** Owned by the Networking Module. Owns the listen endpoint handle (an opaque `TransportEndpoint`, not a raw socket — the socket lives behind the transport, One Platform Boundary) and a non-owning pointer to the `ConnectionTable` and `Session` (both Module-owned) that it drives.

**Lifecycle / startup.** `Start(NetworkingConfig)` → transport `Bind(port, bindAddress)` then `Listen(backlog)`; returns `Expected`. `Stop()` (noexcept) → transport `CloseListen()`; graceful. Start is a no-op returning success when `enabled == false` (dedicated-listen-server vs. embedded-host is a config choice, not an architectural fork).

**Authority model.** The host owns admission (accept/reject), the authoritative connection→session mapping, and the authoritative clock reference for future timing sprints. It **reads** simulation state through the same read-only query seams simulation already exposes (`IWorldQueries`, registry lookups) when later sprints need it; in Sprint-006 it needs none. It never writes simulation.

**Responsibilities.** Accept loop (drained once per tick, deterministic count-bounded), connection admission policy (capacity, `NetworkingConfig.maxConnections`, future auth seam), heartbeat scheduling (issue/track keepalive control messages), and disconnect orchestration.

**Internal state.** Bind address/port, listen endpoint handle, running flag, admission counters, and the frame tick counter it stamps on outbound control messages. All plain values; no simulation state.

**Future scalability.** The single-host model is fixed (One World). Scalability dimensions left open behind seams: connection count (table is capacity-bounded and O(1) indexed), transport replacement (ADR-009), and an optional IO thread (ADR-011). The host never assumes a specific transport or thread model.

**Interaction with existing simulation.** Read-only and *pull-based*: the host/session never push into simulation. Later sprints (007+) add explicit, one-directional adapters that *read* `TransitionResult` and registry state to build outbound payloads — those adapters are extension points (§13), not part of this sprint.

---

## 3. Connection System

**Connection abstraction.** `Connection` is a value-owning record representing one remote peer's link state. It holds: `ConnectionId`, remote address, `TransportEndpoint` handle, `HandshakeState`, timing (last-recv tick, last-send tick, RTT estimate slot — populated later), reliable/unreliable queue handles (§4), and sequence-number state. It owns **no** simulation data and no player identity (player identity is Sprint-007; the connection carries only an opaque, later-assigned `PlayerSlot` reference field left null here).

**Connection IDs.** `ConnectionId { std::uint32_t value; }` — a monotonically assigned, session-unique, non-reused-within-session handle (value 0 = none), mirroring `EntityId`/`PlayerId` conventions. Assignment is deterministic (ascending) and host-owned. IDs are never wire-trusted: the host assigns them; peers cannot claim one.

**Connection lifecycle.** `Disconnected → Connecting → Handshaking → Connected → Disconnecting → Disconnected`. Stored in the `ConnectionTable` (capacity = `maxConnections`, canonical ascending-`ConnectionId` iteration for determinism, sorted-vector + hash accelerator per BC-005, exactly like the Entity Registry and Transition intent store).

**Handshake state machine.** Deterministic, message-driven, bounded: `Hello → Challenge → Response → Established` (control messages only; no cryptographic content in Sprint-006 — the challenge is a protocol-version + nonce echo that reserves the exact frames a future auth/encryption handshake will occupy). Each state has an explicit timeout budget (config). Illegal transitions drop the connection with a logged reason. The handshake never blocks the tick — it advances one step per tick per connection (the single-step-per-tick determinism discipline proven in Sprint-005).

**Timeout handling.** Per-connection last-recv tick vs. `connectionTimeoutMs` (converted to ticks against the frame clock). Timeouts are detected in the deterministic tick pass, in ascending `ConnectionId` order, and produce a disconnect with reason `Timeout`. No wall-clock threads.

**Disconnect flow.** Unified path `BeginDisconnect(id, reason) → (best-effort Bye control message) → Disconnecting → resource release → table removal`. Always reaches `Disconnected`; never leaks a queue or endpoint. Reasons are an enum (`Timeout`, `Graceful`, `Rejected`, `ProtocolError`, `CapacityFull`, `TransportError`).

**Reconnect hooks.** The `Session` retains a `ReconnectToken` slot per departed member (opaque, populated later; here just reserved storage + the seam `ISessionObserver::OnMemberLeft`) so Sprint-012 can rebind a returning peer without protocol change. No reconnect logic is implemented.

**Future authentication seam.** `IConnectionAuthenticator` (engine-free interface) with a default `AllowAllAuthenticator` bound in Sprint-006. The handshake calls it at the `Response → Established` edge. Sprint-013/auth replaces the binding, not the handshake.

**Future encryption seam.** `IPacketCodec` (encode/decode over a byte span, identity codec in Sprint-006) applied at the transport boundary immediately inside/outside the platform TU. Reserves the exact place symmetric encryption slots in without touching message code.

**Ownership.** All connections owned by the `ConnectionTable` (Module-owned). The host and session hold non-owning `ConnectionId` handles, never `Connection*` across ticks (no dangling; resolve on demand by id — the E-G2 discipline generalized).

---

## 4. Transport Layer

**Abstract transport interface (`ITransport`, engine-free, ADR-009).** The single seam over the OS. Shapes (not code): `Bind(port, addr) -> Expected<void>`; `Listen(backlog) -> Expected<void>`; `Poll(deadlineBudget) -> void` (non-blocking; moves bytes between OS and the module's queues); `Send(endpoint, channel, span) -> Expected<void>` (enqueues; actual OS send happens in `Poll`); `CloseListen()`, `CloseEndpoint(endpoint)`; `Accepted()`/`Received()` drains (return owned byte buffers by value). No socket type ever crosses this interface.

**Reliable vs. unreliable transport.** Two logical **channels** over one transport, not two transports: `Channel::Reliable` (ordered, ack'd, retransmitted) and `Channel::Unreliable` (fire-and-forget, sequence-tagged for newest-wins). The infrastructure defines the channel abstraction and the reliable ack/retransmit **bookkeeping structures**; the concrete reliability algorithm (sliding window, ack bitfields) is specified here and implemented incrementally, but no gameplay uses it yet. Underlying datagram transport is UDP (single socket); reliability is layered in the module, keeping the OS surface minimal and replaceable.

**Packet queues.** Per connection, per channel: an **outgoing** queue and an **incoming** queue, each bounded by config (`maxOutgoingQueue`, `maxIncomingQueue`). Overflow policy is explicit and deterministic: reliable overflow → disconnect (`TransportError`); unreliable overflow → drop-oldest. Queues hold owned byte buffers (value semantics; no aliasing into transport memory).

**Transport ownership.** The Networking Module owns the single `ITransport`. `Connection` objects own their per-channel queue handles (indices into Module-owned queue storage, not raw pointers). The platform TU owns the actual sockets and nothing above the seam.

**Future transport replacement.** Any `ITransport` implementation (UDP, mock, loopback, a future Steam/relay transport) plugs in via `adapters::CreatePlatformTransport(config)`. The module is transport-agnostic (ADR-009).

**Compression seam.** `IPayloadCompressor` (identity in Sprint-006) applied to message payloads before framing. Reserved, not implemented.

**Encryption seam.** `IPacketCodec` (§3) at the platform boundary, outermost. Order is fixed and permanent (ADR-010): `serialize → compress → frame(header) → encode(codec) → transport`. Reversed on receive. Fixing this order now prevents every future protocol-layer reshuffle.

**Fragmentation strategy.** Messages larger than the effective MTU payload are fragmented into indexed fragments carried on the reliable channel with a `(messageId, fragmentIndex, fragmentCount)` sub-header; reassembly buffer is bounded and time-boxed (drop on incomplete within budget). Datagrams are sized to stay at or below the configured `mtuPayloadBytes`.

**MTU considerations.** `mtuPayloadBytes` is a config value (default conservative, e.g. 1200 to survive common path MTU without IP fragmentation). The header + codec overhead is reserved from this budget. No path-MTU discovery in Sprint-006; the conservative constant is the frozen decision, with the value overridable via config for future tuning.

---

## 5. Message Architecture

**Packet format (ADR-010, permanent).** A packet = fixed header + payload. Header fields (fixed order, fixed width, explicit endianness = little-endian on the wire): `magic (u16)`, `protocolVersion (u16)`, `channel (u8)`, `flags (u8)` (fragment/ack/control bits), `sequence (u16)`, `ack (u16)`, `ackBits (u32)`, `messageId (u16)`, `payloadLength (u16)`. All multi-byte fields serialized/deserialized through explicit byte-order helpers (never `memcpy` of structs — ABI-safe, ADR-007-aligned).

**Versioning.** `protocolVersion` is checked at the first handshake message. Mismatch → reject with `ProtocolError` before any allocation. The version is a single monotonically increasing integer owned by ADR-010; the compatibility rule is *exact-match in Sprint-006* (no negotiation yet), with the negotiation seam (`OnVersionMismatch`) reserved for future range-based compatibility.

**Message IDs.** `MessageId { std::uint16_t value; }`. A reserved low range (`0x0000–0x00FF`) is **control/infrastructure** (Hello, Challenge, Response, Established, Ping, Pong, Bye), owned by Sprint-006. Everything `>= 0x0100` is reserved for future gameplay sprints and registered by them — Sprint-006 registers only the control range, guaranteeing it never collides with later sprints.

**Serialization ownership.** Each message type owns its own serialize/deserialize into/from a `ByteWriter`/`ByteReader` (value-based cursor over a byte span, `Expected` on under/overflow, no exceptions). There is **no** central switch that knows message internals — the `MessageRegistry` maps `MessageId → handler` and message types are self-describing. This mirrors "each type owns its own bytes," the pattern that kept the Transition layer decoupled.

**Message registry.** `MessageRegistry` maps `MessageId → { deserializer, handler seam }`. Registration is explicit and validated (duplicate id = init failure). Deterministic: iteration and dispatch are id-ordered. The registry is a service (§10) so later sprints register their message ids at composition time without touching Sprint-006 code.

**Dispatch pipeline (per tick).** `transport.Poll → drain incoming → decode(codec) → deframe(validate header, version, length) → route(messageId → handler) → handler consumes (read-only w.r.t. simulation)`. Handlers for gameplay ids don't exist yet; unknown/unregistered ids are dropped with a counter increment (forward-compatible: an old host tolerates a newer client's unknown messages without crashing).

**Packet routing.** By `(channel, messageId)`; control messages route to the connection/handshake state machine, everything else to the registry. Routing is a pure function of header + registry, fully deterministic.

**Compatibility strategy / protocol evolution.** Additive message ids (never renumber), append-only header evolution guarded by `protocolVersion`, unknown-id tolerance, and the reserved control range. These four rules are the permanent compatibility contract (ADR-010) and are what let Sprints 007–016 extend the wire without breaking Sprint-006 peers under the same major version.

---

## 6. Session Management

**Server session.** `Session` is the authoritative membership registry: the set of `Connected` connections that have completed handshake, keyed by `ConnectionId`, with a reserved (null in Sprint-006) `PlayerSlot` per member. One `Session`, host-owned via the Module. Deterministic ascending iteration.

**Connected clients.** A member = `{ ConnectionId, joinTick, PlayerSlot (reserved), ReconnectToken (reserved) }`. Capacity-bounded by `maxConnections`.

**Join lifecycle.** `Handshake Established → Session.Admit(connectionId) → OnMemberJoined` observer fires. Admission is host-authoritative and capacity-checked.

**Leave lifecycle.** `Disconnect(any reason) → Session.Remove(connectionId) → OnMemberLeft` observer fires, stashing a `ReconnectToken` for the slot. Always fires exactly once per departed member.

**Reconnect hooks.** `ISessionObserver::OnMemberLeft` provides the token; `Session.TryReclaim(token)` seam is defined and returns "not supported" in Sprint-006. Sprint-012 implements it without protocol change.

**Future persistence hooks.** `ISessionObserver` (join/leave) is exactly the seam Sprint-011/012 subscribe to for persistence and reconnect. Session state itself is **transient and reconstructable** (never persisted by Sprint-006), mirroring the Transition intent-state decision.

**Session ownership.** Module owns the `Session`; the host drives it; observers are non-owning subscribers registered at composition time. No simulation ownership.

---

## 7. Tick Integration

**Placement — the only scheduler decision Sprint-006 freezes.** Sprint-006 introduces exactly **one** new `tick_order` constant: **`tick_order::kNetworking`**, and it is the **highest key** in the namespace so that networking runs **last** in the deterministic frame, after every producer stage has finished producing this tick's state.

Sprint-006 does **not** assign, introduce, or depend on the *values* of any future producer stage. The ordering requirement is expressed **relationally**, as a frozen contract:

> **Networking-last invariant.** `tick_order::kNetworking` must be **strictly greater than the tick_order key of every producer stage** whose output networking may transmit. Each producer stage owns its own `tick_order` constant, chosen (with value `< kNetworking`) in that stage's own architecture sprint. Sprint-006 depends only on the invariant "networking has the highest key," never on any specific predecessor value.

```
[ all producer stages, each owning its own tick_order key < kNetworking ]
        └────────────────────────────────►  kNetworking   [THIS SPRINT — highest key]
```

*Context, not a Sprint-006 decision:* the core `FrameDispatcher.h` already carries slots reserved by earlier sprints (`kWorld=100 … kAlifeTransition=350`) plus **pre-existing reservations owned by Sprint-002's core** (`kReplication`, `kPersistence`, `kDiagnostics`, `kPlugins`). Those constants are not introduced, re-frozen, or relied upon by value here; whether they remain reserved or are deferred to their owning sprints is a Sprint-002-core governance question outside this sprint's scope (see the refinement note in §15). Sprint-006 only requires that `kNetworking` sits above whatever the last producer's key turns out to be. Choosing a value in a high band (e.g. above the current highest reserved slot) satisfies this while leaving room for future producers to insert their own keys below it.

**Why networking-last is correct.** Networking consumes simulation/replication output; it must observe a fully-settled tick. Being the highest key guarantees that (a) any state a future producer stage hands to networking is already computed before networking runs, and (b) networking never races an in-progress phase. The single-phase, single-subscription discipline (proven in Sprints 002–005) is preserved: one `ITickable` at `kNetworking`.

**Interaction per subsystem.** World/EntityRegistry/Bubble/Transition: **read-only, and in Sprint-006, not read at all** — networking only pumps transport and advances connection/handshake/session state machines. Future replication: networking is the *sink* for the replication stage's output (extension point §13). The eventual data flow is strictly `simulation → replication(build) → networking(send)` within one frame, never the reverse — enforced solely by the networking-last invariant, not by any hard-coded predecessor value.

**Determinism guarantees.** The per-tick network pass is: `Poll transport → process incoming (id-ordered) → advance handshakes/timeouts (ascending ConnectionId) → drain outgoing`. Given identical inputs (received byte buffers) and identical tick, the resulting state transitions and outgoing queue contents are identical. Wall-clock is used only for RTT *measurement* (a diagnostic value), never for control flow — all timeouts are tick-derived. This keeps the module deterministic under replay (E-G3).

---

## 8. Threading Model

**Decision: single-threaded on the FrameDispatcher for Sprint-006** (subject of ADR-011). The networking tick runs inline at `kNetworking` on the engine main thread, exactly like every other subsystem. `ITransport::Poll` is **non-blocking** (sockets in non-blocking mode; bounded per-tick byte/packet budget).

**Justification.**
- **Determinism.** A single-threaded, tick-driven pump gives reproducible ordering and makes replay/failure-injection tests deterministic — the property every prior sprint relied on and the thing a background IO thread would immediately jeopardize.
- **Simplicity & safety.** No locks, no shared-state races, no cross-thread lifetime hazards during the highest-risk (first) networking sprint. The frame-bridge/relcase lifetime discipline already assumes main-thread execution (I8).
- **One Engine Boundary friendliness.** The engine is not thread-safe for the reads later sprints will need; keeping networking on the main thread avoids inventing a synchronization contract with the engine now.
- **Sufficiency.** Infrastructure (handshakes, keepalives, bounded pumping) is not CPU-bound; non-blocking polling within the frame budget is adequate for the connection counts this project targets.

**Reserved future IO thread (seam, not implemented).** ADR-011 fixes the boundary that *would* separate an IO thread from the tick: a **double-buffered, single-producer/single-consumer handoff** of owned byte buffers between (future) `NetworkIoThread` and the main-thread tick, swapped once per frame under one mutex, with **zero shared mutable state** beyond the two buffers. Memory ownership transfers by move; no object is touched by both threads simultaneously. If Sprint-016 introduces it, it changes only the transport pump's internals, never the message/connection/session logic. Determinism is preserved because the tick still consumes a *frozen* per-frame buffer snapshot.

---

## 9. Diagnostics

All read-only, deterministic, allocation-light, iostream-free (`common::Format` + C stdio), mirroring the Bubble/Transition diagnostics.

- **Connection statistics.** Per `ConnectionId`: state, RTT estimate, last-recv/last-send tick, reliable in-flight count, retransmits, drops.
- **Traffic counters.** Per connection and aggregate: packets/bytes sent & received, by channel; control vs. data.
- **Bandwidth statistics.** Rolling per-tick and per-second send/recv byte rates (tick-windowed; wall-clock only for the human-readable /sec view).
- **Latency tracking.** RTT via Ping/Pong control messages (diagnostic only; not used for control flow).
- **Packet tracing.** Opt-in (debug flag) bounded ring buffer of `(tick, connectionId, channel, messageId, length, direction)` — headers only, never payload bytes (privacy + size).
- **Connection inspector.** `DescribeConnection(id) -> string`; `DumpConnections() -> string` (ascending).
- **Debug dump.** `DescribeState()` aggregate one-liner (listen state, connection count, session size, totals).
- **Validation utilities.** `ValidateConsistency() -> NetworkConsistencyReport`: connection-table ordering/accelerator health, state legality, session⊆connected invariant, queue bounds respected, no orphan endpoints.
- **Health monitoring.** `NetworkHealth` summary (listening?, capacity headroom, error-rate thresholds) for future ops tooling; pure derivation, no side effects.

---

## 10. Services

All are `core::IService` (and where ticked, `core::ITickable`), owned by the `ServiceRegistry`, registered in `Bootstrap.cpp`. Ownership is strictly hierarchical to avoid cross-service lifetime coupling.

- **`NetworkingService`** — the umbrella (§1). `IService` + `ITickable` (the single `kNetworking` subscriber). Owns the Module which owns everything else. `Dependencies() = {World, EntityRegistry, BubbleManager, TransitionManager}`. **This is the only networking service that ticks.**
- **`TransportService`** — thin `IService` owning nothing beyond exposing the `ITransport` handle created via `adapters::CreatePlatformTransport(config)`; kept separate so transport can be swapped/injected in tests independently. `Dependencies() = {}` (pure infrastructure). *Rationale for a distinct service:* isolates the One Platform Boundary object so tests inject a loopback transport without constructing the whole module.
- **`SessionService`** — `IService` owning the `Session` and the observer registry. `Dependencies() = {}` beyond the module. Exposes `ISessionObserver` subscription for future sprints.
- **`MessageRegistryService`** — `IService` owning the `MessageRegistry`; registers the control-range message ids at Initialize; exposes registration API for later sprints. `Dependencies() = {}`.
- **Configuration** — no separate service; `NetworkingConfig` is parsed at Bootstrap from `server.cfg` and injected (the established config pattern; a service would be over-engineering).

**Dependency injection & Bootstrap registration.** Registration order: TransportService → MessageRegistryService → SessionService → NetworkingService (umbrella last, depending on the simulation quartet for ordering). The umbrella receives non-owning handles to the transport/session/registry the sub-services own, plus the injected config. Only `NetworkingService` subscribes to the dispatcher.

**Ownership relationships (single tree).**
```
ServiceRegistry
 ├─ TransportService ─ owns ITransport (behind One Platform Boundary)
 ├─ MessageRegistryService ─ owns MessageRegistry
 ├─ SessionService ─ owns Session + observer registry
 └─ NetworkingService ─ owns HostServer, ConnectionTable; drives the above via non-owning handles; ticks at kNetworking
```

---

## 11. Configuration

Parsed from `server.cfg` `[networking]` / `[transport]` sections via `FromConfig` (missing → default; invalid → `InvalidArgument`), matching `BubbleConfiguration`.

**`NetworkingConfig`:** `enabled (bool, default false)`, `maxConnections (u32, default 16)`, `connectionTimeoutMs (u32, default 15000)`, `handshakeTimeoutMs (u32, default 5000)`, `heartbeatIntervalMs (u32, default 2000)`, `debugFlags (u32, default 0)`.

**`TransportConfig`:** `listenPort (u16, default 27015)`, `bindAddress (string, default "0.0.0.0")`, `backlog (u32, default 32)`, `mtuPayloadBytes (u32, default 1200)`, `maxOutgoingQueue (u32, default 256)`, `maxIncomingQueue (u32, default 256)`, `maxBytesPerTick (u32, default 262144)`, `reliableWindow (u16, default 256)`, `reassemblyBudgetTicks (u32, default 120)`.

**Ports / timeouts / limits.** All above; every timeout is milliseconds-in-config converted once to ticks against the frame clock at Initialize (single conversion point). Bandwidth/packet/queue limits are hard bounds with the deterministic overflow policies of §4.

**Diagnostics options / debug flags.** `debugFlags` bits (assigned in the doc): packet-trace enable, verbose-connection-log, health-log. Opaque bit constants, no behavioral coupling beyond diagnostics.

**Future scalability.** All numeric limits are config-driven, so scaling connection count / bandwidth / MTU is a config change, not an architecture change. New config keys are additive (unknown keys ignored with a warning, per the config precedent).

---

## 12. Testing Strategy

- **Mock transport.** `MockTransport` implementing `ITransport` with in-memory endpoints and scriptable receive queues — the primary test vehicle (no OS sockets in unit tests; One Platform Boundary keeps the whole module testable engine-free/OS-free).
- **Loopback transport.** `LoopbackTransport` connecting two in-process modules for host↔client integration tests without the network stack.
- **Fake clients.** Test-only client driver that speaks the wire protocol (handshake, ping) against the host through the mock/loopback transport.
- **Unit testing.** Header serialize/deserialize round-trips (incl. endianness and boundary lengths), connection state machine transitions, handshake happy-path + every illegal transition, timeout tick math, queue overflow policies, message registry duplicate/unknown-id handling, fragmentation/reassembly, `ValidateConsistency`.
- **Integration testing.** Full handshake→connected→ping→graceful-disconnect and →timeout-disconnect over loopback; capacity rejection; version-mismatch rejection.
- **Deterministic replay.** Feed a recorded sequence of received buffers + ticks twice; assert identical connection/session state and identical outgoing queues (the E-G3 gate harness).
- **Failure injection.** Dropped/duplicated/reordered/corrupted datagrams (mock transport knobs); assert reliable channel recovers and unreliable degrades gracefully with no crash and no leak.
- **Regression testing.** The full suite runs in the existing test project (GCC + MSVC), engine-free (mock/loopback transport; no `PlatformSockets.cpp` in the test build — the null/loopback counterpart is linked, exactly like `NullEngineAdapters.cpp`).
- **ABI validation strategy.** `PlatformSockets.cpp` compiles under `EngineAbi.props` in the engine build (exceptions off, `/W4 /WX`, C4530-clean); wire structs are serialized field-by-field (never raw-copied), so no packing/ABI assumptions cross the wire. Antigravity validates the platform TU compile + a real loopback socket smoke test.

---

## 13. Extension Points (no future sprint is blocked)

- **Sprint-007 Player Lifecycle:** `Connection.PlayerSlot` (reserved), `ISessionObserver::OnMemberJoined/Left`, `IConnectionAuthenticator` edge, and the control-range/data-range message split (players register data-range ids).
- **Sprint-008 Snapshot Replication:** networking is the *sink* for the replication stage's output. Replication owns its own `tick_order` constant (assigned in Sprint-008, with value `< kNetworking` per the networking-last invariant, §7); networking never hard-codes that value. `MessageRegistry` data-range ids; reliable channel + fragmentation carry baseline snapshots. `TransitionResult` (Sprint-005) is the activation delta the replication builder consumes upstream.
- **Sprint-009 Delta Replication:** unreliable channel + sequence/ack fields already in the header; newest-wins bookkeeping present.
- **Sprint-010 Prediction:** RTT/latency diagnostics + per-connection tick stamps provide the timing substrate; no protocol change needed.
- **Sprint-011 Persistence:** `ISessionObserver` join/leave is the persistence hook; session is reconstructable.
- **Sprint-012 Reconnect:** `ReconnectToken` + `Session.TryReclaim` seam.
- **Sprint-013 Lua API:** `MessageRegistryService` registration API + a future scripted-message id sub-range; handlers behind the same seam.
- **Sprint-014 Plugin System:** plugins register message ids/observers at composition time via the same service APIs; the plugin stage owns its own `tick_order` key (`< kNetworking`) and therefore runs before networking, per the networking-last invariant (§7).
- **Sprint-015 Optimization:** `IPayloadCompressor`, bandwidth counters, and queue tuning knobs are all present as config/seams.
- **Sprint-016 Networking improvements:** `ITransport` replacement (ADR-009), the `IPacketCodec` encryption seam (ADR-010 order fixed), and the ADR-011 IO-thread boundary.

Every one of these is a *seam or reserved field defined in this sprint*, so later sprints extend without reopening Sprint-006.

---

## 14. Risks

1. **Ownership drift — networking creeping into simulation authority (highest).** *Mitigation:* the const-reference-only rule (§0), no networking access to the ALife gateway, and `ValidateConsistency`/review gates. Encoded structurally: networking services depend on simulation for *ordering* only and hold no non-const simulation handles.
2. **Lifetime / dangling endpoints across ticks.** *Mitigation:* connections resolved by `ConnectionId` on demand (no `Connection*`/socket retained above the seam); capacity-bounded table; unified disconnect path; reverse-order teardown. Generalizes the E-G2 discipline.
3. **Threading/determinism risk if an IO thread is added prematurely.** *Mitigation:* ADR-011 mandates single-threaded for Sprint-006 and fixes the future handoff boundary; determinism replay gate (E-G3-N) enforces it.
4. **Determinism risk from wall-clock leakage into control flow.** *Mitigation:* all timeouts tick-derived; wall-clock confined to diagnostic RTT/bandwidth display; replay gate asserts identical outgoing queues.
5. **Platform/ABI risk from OS socket headers polluting the module.** *Mitigation:* One Platform Boundary (ADR-009) — Winsock/BSD headers only in `PlatformSockets.cpp`, compiled under `EngineAbi.props`; the rest of the module is OS-free and tested with mock/loopback transports.
6. **Protocol-evolution risk (future sprints breaking the wire).** *Mitigation:* ADR-010 fixes header layout, endianness, the reserved control range, additive-id rule, and the fixed serialize→compress→frame→encode order.
7. **Future scalability risk (connection count / bandwidth).** *Mitigation:* all limits config-driven; O(1) id-indexed table; transport replaceable. No architectural change needed to scale the fixed dimensions.
8. **Blocking-socket stall risk.** *Mitigation:* non-blocking sockets + bounded per-tick byte/packet budget (`maxBytesPerTick`); `Poll` never blocks the frame.

---

## 15. ADRs introduced

**ADR-009 — One Platform Boundary (Network Transport Boundary).** All OS socket / Winsock / BSD-sockets calls and headers are confined to a single translation unit (`PlatformSockets.cpp`, real build) behind the engine-free `ITransport` seam, with a null/loopback counterpart (`tests/support` / mock) for the test build. No socket type crosses the seam; the module above is OS-agnostic and unit-testable without a network stack. Symmetric to ADR-007's One Engine Boundary. **Permanent** — it governs every future transport and the whole test strategy.

**ADR-010 — Wire Protocol & Compatibility Contract.** Fixes, permanently: the packet header layout and field widths, little-endian on-the-wire byte order, field-by-field serialization (never raw struct copy — ABI-safe), the reserved control message-id range (`0x0000–0x00FF`) vs. gameplay range (`>= 0x0100`), the additive-only id rule, `protocolVersion` semantics (exact-match now, negotiation seam reserved), and the fixed layer order `serialize → compress → frame → encode(codec) → transport`. This is the contract that lets Sprints 007–016 evolve the protocol without breaking peers.

**ADR-011 — Networking Concurrency & Determinism Boundary.** Sprint-006 networking executes single-threaded, inline at `tick_order::kNetworking`, with non-blocking transport polling; all control flow is tick-derived (wall-clock is diagnostic-only). Defines the *only* sanctioned future concurrency extension: a double-buffered SPSC byte-buffer handoff between a future IO thread and the main-thread tick, swapped once per frame, ownership transferred by move, zero shared mutable state otherwise. Guarantees replay determinism now and bounds any future threading to a single, reviewable boundary.

No other ADRs are warranted; connection/session/message specifics are design, not permanent cross-cutting decisions.

**Refinement note (scheduler-constant ownership, 2026-07-10).** A review raised that Sprint-006 must not freeze scheduler positions belonging to future sprints. Accepted. Sprint-006 introduces **only** `tick_order::kNetworking` and expresses its placement through the relational **networking-last invariant** (§7) — it assigns no value to, and depends on no value of, any future producer stage. Each future producer stage owns its own `tick_order` constant (value `< kNetworking`), assigned in that stage's architecture sprint. Separately, the core `FrameDispatcher.h` (owned by Sprint-002) still carries pre-existing forward reservations (`kReplication`, `kPersistence`, `kDiagnostics`, `kPlugins`) added when the dispatcher was designed. Those predate Sprint-006 and are **not** re-frozen or depended upon here. Whether to keep them as convenience reservations or defer each to its owning sprint is a **Sprint-002-core governance decision, explicitly out of Sprint-006's scope** — this document neither requires nor prevents that cleanup, and Sprint-006 remains correct under either outcome because it relies solely on `kNetworking` being the highest key.

---

## 16. Evidence Gates

Objective checks that must have a defined spike/outcome before implementation of the affected component proceeds (the Sprint-005 gate discipline; each has a fallback that cannot reopen the architecture).

- **E-G1-N — Non-blocking transport pump within the frame budget (blocking).** Spike: a real UDP `ITransport` behind `PlatformSockets.cpp` performing bind/listen/non-blocking `Poll`/send under `EngineAbi.props`, proving the OS surface is reachable, exception-free, and stays within `maxBytesPerTick` without blocking the tick. *Fallback:* if a specific OS call is unavailable exception-free, wrap it in the platform TU returning `Expected` — never above the seam. Validates ADR-009 + §4/§8. **Highest priority.**
- **E-G2-N — Header round-trip & endianness ABI (blocking).** Spike: serialize→deserialize every header field across a byte boundary on both MSVC (engine build) and GCC (tests), asserting identical bytes and values, with no packing assumptions. Validates ADR-010. *Fallback:* none needed beyond field-by-field helpers (already mandated).
- **E-G3-N — Deterministic replay of the network tick (blocking).** Spike: the replay harness (§12) — identical received-buffer+tick sequence twice yields identical connection/session state and identical outgoing queues. Validates §7/§8 determinism and ADR-011. *Fallback:* if any nondeterminism is found, it must be a wall-clock leak into control flow — remove it (tick-derive), not rearchitect.
- **E-G4-N — Lifetime & teardown safety (non-blocking).** Spike: connect/disconnect/timeout churn under `ValidateConsistency` + a leak check; reverse-order Bootstrap teardown with active connections leaves no orphan endpoint/queue. Sizes nothing architectural; confirms the ownership model holds under churn.

**Freeze rule:** E-G1-N, E-G2-N, E-G3-N are blocking; E-G4-N is non-blocking (confirmatory). Each resolves entirely behind the transport seam / module internals and cannot reopen the architecture.

---

## 17. Acceptance Criteria (freeze conditions)

Sprint-006 architecture is **ready to freeze** when all hold:

- [ ] Every subsystem (§1–§11) has explicit responsibilities, ownership, and lifecycle — no placeholders (met by this document).
- [ ] One Platform Boundary defined and the whole module is testable via mock/loopback with no OS in the test build (ADR-009).
- [ ] Wire protocol header, endianness, versioning, id ranges, and layer order fixed (ADR-010).
- [ ] Threading decision made and the future-thread boundary bounded (ADR-011).
- [ ] Networking placed at a **single** additive `tick_order::kNetworking` (the only scheduler constant Sprint-006 introduces), defined as the highest key and justified by the relational networking-last invariant — no future producer values assigned or depended upon; determinism guarantees stated (§7).
- [ ] Ownership is a single tree with no networking ownership of simulation; const-reference-only rule stated (§0/§10/§14).
- [ ] Every Sprint-007…016 extension point is a named seam or reserved field (§13).
- [ ] ADR-009/010/011 authored (this document, §15).
- [ ] Evidence gates E-G1-N/E-G2-N/E-G3-N have defined spikes with non-reopening fallbacks (§16).
- [ ] Invariants preserved: Preserve Before Replace, Host Authority, One World, One ALife Simulation, One Engine Boundary, Deterministic Simulation, ADR-007, ADR-008, Entity Registry / Bubble / Transition ownership.

Sprint-006 is **Frozen** once the three blocking evidence gates have defined spikes and ADR-009/010/011 are accepted. It is **Ready for Implementation** immediately thereafter, following the sequence below.

---

## 18. Recommended implementation sequence (risk-minimizing, incremental)

Engine-free / OS-free first (mock transport), the One-Platform-Boundary TU last-but-one, wiring and closure last — exactly the Sprint-005 shape. Each step is independently buildable, independently verifiable, purely additive, and verified before the next begins.

1. **Config + value types.** `NetworkingConfig`/`TransportConfig` (`FromConfig`), `ConnectionId`, `MessageId`, `Channel`, header struct, enums (states, reasons, outcomes). Engine/OS-free.
2. **Wire codec.** `ByteWriter`/`ByteReader` + header serialize/deserialize (little-endian, field-by-field). *(Gate E-G2-N.)*
3. **`ITransport` seam + `MockTransport` + `LoopbackTransport`.** Engine/OS-free; the whole test substrate.
4. **`MessageRegistry` (+ service).** Control-range id registration, duplicate/unknown handling, id-ordered dispatch.
5. **`ConnectionTable` + `Connection`** (sorted-vector + hash, ascending `ConnectionId`) and the connection lifecycle enum/state storage — no transport yet.
6. **Handshake state machine** over control messages (single-step-per-tick), incl. illegal-transition drops. Driven by mock transport.
7. **Timeout + disconnect flow** (tick-derived), unified `BeginDisconnect`, reason enum.
8. **`Session` + `SessionService` + `ISessionObserver`** (admit/remove/observer fire); reserved reconnect token.
9. **Packet queues + reliability/fragmentation bookkeeping** on the channels (mock transport with drop/reorder knobs). *(Gate: failure injection.)*
10. **`HostServer`** (accept loop, admission, heartbeat) over mock/loopback; full host↔fake-client integration. *(Gate E-G3-N replay.)*
11. **`NetworkingService` umbrella + `TransportService`** wiring of the module; single `kNetworking` tick; still mock/loopback.
12. **`tick_order::kNetworking = 800`** (additive) + **Bootstrap composition-root wiring** (register after Transition; subscribe at 800; reverse-order teardown; transport via factory).
13. **`PlatformSockets.cpp` real UDP `ITransport`** behind ADR-009 (the one OS-touching TU) + `adapters::CreatePlatformTransport` + null/loopback factory for tests. *(Gate E-G1-N; Antigravity real-socket smoke + ABI.)*
14. **Documentation + sprint closure** (`Networking.md`, status docs), Definition of Done verification.

This ordering keeps every OS/engine concern out of the first eleven steps (all verifiable on GCC + MSVC with no network stack), confines the platform surface to a single late TU, and defers composition-root changes until the module is proven — minimizing integration risk while preserving every project invariant.

# Sprint-006 · Step 1 — Config + Value Types — Implementation Specification

- **Status:** Implementation specification (no code). Maps to the frozen `Sprint-006-Architecture.md` (§5, §11) and `Sprint-006-Implementation-Plan.md` Step 1.
- **Nature:** Engine-free **and** OS-free foundation. Strongly-typed data + configuration parsing only. **No** transport, sockets, services, message processing, connection logic, or runtime behavior.
- **Constraints (binding):** ADR-007 (no exceptions, no RTTI, no iostream, `core::Expected<T>` for fallible ops, C4530-clean, trivially-copyable value types), ADR-009/010/011 respected by *omission* (no OS types, no wire behavior, no threads), One Engine Boundary, One Platform Boundary, Host Authority, Deterministic Simulation. Nothing here includes an engine or OS header.

---

## 1. Directory & namespace conventions

- Namespace: `stalkermp::net` (new subsystem namespace).
- Public headers: `include/stalkermp/net/`.
- Sources: `src/net/`.
- Tests: `tests/NetworkTests.cpp` (new TU, added to the test project).
- Reuses existing core: `stalkermp::core::Expected`, `core::MakeError`, `core::ErrorCode::InvalidArgument`, `core::ConfigStore` (from `stalkermp/core/Config.h`). Mirrors the `world::BubbleConfiguration` / `EntityRegistryConfig` precedent exactly.

---

## 2. Files to create

| File | Contents |
|---|---|
| `include/stalkermp/net/NetTypes.h` | Identifier value types (`ConnectionId`, `MessageId`), `TransportEndpoint` placeholder, and all Step-1 enums (`Channel`, `HandshakeState`, `ConnectionState`, `DisconnectReason`, `TransportOutcome`) with `*Name()` helpers. Header-only. |
| `include/stalkermp/net/ProtocolConstants.h` | Wire/protocol constants: `kProtocolMagic`, `kProtocolVersion`, control message-id range bounds, and the named control message ids. Header-only, `constexpr`. |
| `include/stalkermp/net/PacketHeader.h` | The `PacketHeader` value struct (field layout + widths per ADR-010) and `kPacketHeaderWireSize`. **Declaration/shape only** — serialize/deserialize logic is Step 2, not here. Header-only. |
| `include/stalkermp/net/NetworkingConfig.h` | `NetworkingConfig` struct (fields + defaults) + `FromConfig` declaration. |
| `include/stalkermp/net/TransportConfig.h` | `TransportConfig` struct (fields + defaults) + `FromConfig` declaration. |
| `src/net/NetworkingConfig.cpp` | `NetworkingConfig::FromConfig` implementation (parse `[networking]`). |
| `src/net/TransportConfig.cpp` | `TransportConfig::FromConfig` implementation (parse `[transport]`). |
| `tests/NetworkTests.cpp` | Step-1 unit tests (value types, enums, both configs). |

**No new `.cfg` files are required.** Defaults are the frozen values (§5 below); the generated `server.cfg` may later document a `[networking]`/`[transport]` section, but Step 1 must work with those sections **absent** (all-defaults path). *Optional, non-blocking:* the config-template generator may append commented example `[networking]`/`[transport]` keys — this is documentation only and must not change parsing behavior.

## 3. Files to modify

| File | Why |
|---|---|
| `xrMP.vcxproj` | Add the two config `.cpp`s to `ClCompile`; add the five new headers to `ClInclude`. Required for the library build to compile the new TUs. |
| `tests/xrMP_Tests.vcxproj` | Add `..\src\net\NetworkingConfig.cpp`, `..\src\net\TransportConfig.cpp`, and `NetworkTests.cpp` to `ClCompile`. Required so the test build links the config parsers and runs the new tests. |

No other file is modified. (`Bootstrap.cpp`, `FrameDispatcher.h`, `EngineAdapters.*`, and all Sprint-001…005 files are untouched — Step 1 introduces no wiring and no tick.)

---

## 4. Value types & enums — responsibilities

For each: **Purpose · Ownership · Lifetime · Relationships · Usage constraints · Future consumers · Validation · Construction · Invariants.**

### 4.1 `ConnectionId`
- **Purpose.** Session-unique, host-assigned handle for one peer link. Mirrors `EntityId`/`PlayerId` (`struct { std::uint32_t value = 0; }` with `==`/`!=`; `value == 0` reserved = "none").
- **Ownership.** Pure value; owned by whoever holds it. No resource.
- **Lifetime.** Copyable/trivially-copyable; outlives nothing.
- **Relationships.** Keys the (future) `ConnectionTable`, `Session` members, per-connection queues.
- **Usage constraints.** Never wire-trusted (host assigns; peers cannot claim). Never reused within a session.
- **Future consumers.** Steps 5–11 (table, handshake, session, host, diagnostics).
- **Validation.** `IsNone()` ⇔ `value == 0`. No other constraint at type level.
- **Construction.** Aggregate `{value}`; default = none.
- **Invariants.** Comparable, orderable by `value` (ascending determinism); provide an ordering helper or rely on `.value` comparison exactly as `EntityId`/`TransitionCommand` do.

### 4.2 `MessageId`
- **Purpose.** Wire message discriminator (`struct { std::uint16_t value = 0; }`, `==`/`!=`).
- **Ownership/Lifetime.** Pure value.
- **Relationships.** Keys the (future) `MessageRegistry`; carried in `PacketHeader.messageId`.
- **Usage constraints.** Control range `[kControlIdMin, kControlIdMax]` reserved to Sprint-006; data range `>= kDataIdMin` reserved for later sprints (ADR-010). Step 1 defines the ranges but registers nothing.
- **Future consumers.** Step 4 (registry) and every later sprint registering data ids.
- **Validation.** Helpers `IsControlId(MessageId)`, `IsDataId(MessageId)` (pure predicates).
- **Construction.** Aggregate; default 0.
- **Invariants.** Ordering by `value` (deterministic dispatch later).

### 4.3 `TransportEndpoint` (placeholder)
- **Purpose.** Opaque handle standing in for a transport-level peer address/socket slot **without exposing any OS type** (ADR-009). In Step 1 it is a value struct `{ std::uint32_t value = 0; }` (0 = invalid), identical in spirit to `ConnectionId` but owned by the transport layer.
- **Ownership.** Conceptually owned by the (future) `ITransport`; Step 1 only defines the value type.
- **Lifetime/Relationships.** Stored (later) on `Connection`; produced/consumed only across the `ITransport` seam (Step 3+). No socket, no address family, no OS struct.
- **Usage constraints.** Never dereferenced as a pointer; it is an index/handle. No OS header may appear to define it.
- **Future consumers.** Step 3 (transport seam), Step 5 (connection).
- **Validation.** `IsValid()` ⇔ `value != 0`.
- **Construction/Invariants.** Aggregate; default invalid.

### 4.4 `Channel` (enum class `std::uint8_t`)
- **Purpose.** Logical delivery channel over the single transport.
- **Values.** `Reliable = 0`, `Unreliable = 1`. (Fixed; `Reliable` first so 0 = the safe default.)
- **Relationships.** Carried in `PacketHeader.channel`; keys per-channel queues (Step 9).
- **Usage constraints.** Exactly these two in Sprint-006. `ChannelName(Channel)` helper.
- **Invariants.** Stable numeric values (wire-visible via the header — ADR-010 stability).

### 4.5 `HandshakeState` (enum class `std::uint8_t`)
- **Purpose.** Per-connection handshake progress (state machine is Step 6; Step 1 defines the states only).
- **Values.** `None = 0`, `HelloSent`, `ChallengeSent`, `ResponseSent`, `Established`. (Names reflect the `Hello → Challenge → Response → Established` sequence; `None` is the pre-handshake default.)
- **Relationships.** Stored on `Connection`; drives Step 6.
- **Usage constraints.** No transition logic here. `HandshakeStateName` helper.
- **Invariants.** `Established` is terminal-success; `None` is default.

### 4.6 `ConnectionState` (enum class `std::uint8_t`)
- **Purpose.** Connection lifecycle phase (§3 of architecture).
- **Values.** `Disconnected = 0`, `Connecting`, `Handshaking`, `Connected`, `Disconnecting`. (`Disconnected` default = 0.)
- **Relationships.** Stored on `Connection`; the table iterates these deterministically.
- **Usage constraints.** No transition logic here. `ConnectionStateName` helper.
- **Invariants.** Default `Disconnected`; `Connected` requires `HandshakeState::Established` (enforced later, documented here).

### 4.7 `DisconnectReason` (enum class `std::uint8_t`)
- **Purpose.** Cause classification for the unified disconnect path (Step 7).
- **Values.** `None = 0`, `Timeout`, `Graceful`, `Rejected`, `ProtocolError`, `CapacityFull`, `TransportError`. (Exactly the frozen §3 set; `None` default.)
- **Relationships.** Carried on disconnect; surfaced in diagnostics/logs.
- **Usage constraints.** `DisconnectReasonName` helper. No logic.
- **Invariants.** Total; every future disconnect maps to exactly one.

### 4.8 `TransportOutcome` (enum class `std::uint8_t`)
- **Purpose.** Value result of a (future) transport operation, parallel in spirit to `world::TransitionOutcome` — keeps the transport seam exception-free (ADR-007/009).
- **Values.** `Ok = 0`, `WouldBlock`, `Queued`, `Rejected`, `EndpointMissing`, `Error`. (`Ok` default; `WouldBlock` for non-blocking sockets; `Queued` for enqueued sends; `EndpointMissing` the benign "unknown endpoint" skip.)
- **Relationships.** Returned by `ITransport` methods (Step 3+); Step 1 defines the enum only.
- **Usage constraints.** `TransportOutcomeName` helper. No I/O.
- **Invariants.** Total; a well-behaved transport never throws — it returns one of these.

> **Enum discipline (all of the above).** `enum class` with explicit `std::uint8_t` underlying type; default value = 0 with the "neutral/none" meaning; a `constexpr const char*` `…Name()` free function covering every enumerator (mirrors `TransitionStateName` / `BubbleMembershipName`), returning `"Unknown"` on out-of-range for safety. `Channel`'s numeric values are wire-stable (ADR-010); the others are internal and may be reordered only within this sprint.

### 4.9 `PacketHeader` (value struct — shape only)
- **Purpose.** The fixed wire header (ADR-010). Step 1 defines the **struct shape, field order, and widths**; serialization is Step 2.
- **Fields (fixed order, fixed width, little-endian on the wire — declared here, encoded in Step 2):** `magic (u16)`, `protocolVersion (u16)`, `channel (u8)`, `flags (u8)`, `sequence (u16)`, `ack (u16)`, `ackBits (u32)`, `messageId (u16)`, `payloadLength (u16)`.
- **`kPacketHeaderWireSize`** — a `constexpr std::size_t` = the exact on-wire byte count (sum of field widths = 18 bytes), defined independently of `sizeof(PacketHeader)` (no packing assumption; ADR-010 forbids raw struct copy).
- **`flags` bit meanings** — reserved `constexpr` bit constants: `kFlagFragment`, `kFlagAck`, `kFlagControl` (others reserved 0). Defined here for later use; no behavior.
- **Ownership/Lifetime.** Pure value; trivially copyable.
- **Usage constraints.** Never `memcpy`'d to/from the wire (Step 2 serializes field-by-field). Multi-byte fields are logically little-endian on the wire.
- **Future consumers.** Step 2 (codec), Step 4 (routing), Step 9 (reliability/fragmentation).
- **Invariants.** `kPacketHeaderWireSize` is authoritative for framing; struct is for in-memory convenience only.

### 4.10 Protocol & control-id constants (`ProtocolConstants.h`)
- `constexpr std::uint16_t kProtocolMagic` — fixed sentinel (e.g. a distinctive non-zero value; exact literal chosen at implementation, documented in the header).
- `constexpr std::uint16_t kProtocolVersion` — starts at `1`. Owned by ADR-010; monotonically increasing thereafter. Compatibility rule in Sprint-006 = **exact match**.
- `constexpr MessageId kControlIdMin = {0x0000}`, `kControlIdMax = {0x00FF}`, `kDataIdMin = {0x0100}` — the ADR-010 id partitioning.
- Named control ids (within the control range, stable values): `kMsgHello`, `kMsgChallenge`, `kMsgResponse`, `kMsgEstablished`, `kMsgPing`, `kMsgPong`, `kMsgBye`. Step 1 **defines** these constants; **registration** (Step 4) and **handling** (Steps 6–10) are later.
- **Invariant.** All named control ids satisfy `IsControlId`; none overlaps the data range.

---

## 5. Configuration — full definition

Both configs mirror `world::BubbleConfiguration`: a plain struct of fields with in-class defaults, plus a static `FromConfig(const core::ConfigStore&) -> core::Expected<T>`. Parsing rules: **missing section ⇒ all defaults** (returns success); **present-but-invalid value ⇒ `core::MakeError(core::ErrorCode::InvalidArgument, "[section] key <reason>")`**; unknown keys ignored (config precedent). Reads use `ConfigStore::GetInt/GetDouble/GetBool/GetString` (each returns `std::optional`). Integer fields validate range then narrow-cast from `std::int64_t`. Provide small internal `ReadU16`/`ReadU32`/`ReadBool`/`ReadString` helpers analogous to Sprint-004's `ReadDouble` (bounds + message), kept in the `.cpp` (anonymous namespace).

### 5.1 `NetworkingConfig` — section `[networking]`
| Field | Type | Default | Validation | Failure |
|---|---|---|---|---|
| `enabled` | `bool` | `false` | none (any bool) | — |
| `maxConnections` | `std::uint32_t` | `16` | `1 … 4096` inclusive | `InvalidArgument` |
| `connectionTimeoutMs` | `std::uint32_t` | `15000` | `>= 1000` | `InvalidArgument` |
| `handshakeTimeoutMs` | `std::uint32_t` | `5000` | `>= 500` and `<= connectionTimeoutMs` | `InvalidArgument` |
| `heartbeatIntervalMs` | `std::uint32_t` | `2000` | `>= 100` and `< connectionTimeoutMs` | `InvalidArgument` |
| `debugFlags` | `std::uint32_t` | `0` | `>= 0` (reject negative source int) | `InvalidArgument` |

- **Cross-field rules:** `handshakeTimeoutMs <= connectionTimeoutMs`; `heartbeatIntervalMs < connectionTimeoutMs`. Both checked after individual parsing (mirrors the Bubble hysteresis cross-check).
- **`debugFlags` bits (documented, opaque):** `kNetDebugPacketTrace`, `kNetDebugVerboseConn`, `kNetDebugHealthLog` — `constexpr` bit constants co-located in `NetworkingConfig.h`; no behavioral coupling in Step 1 (diagnostics consume them later).
- **Ownership.** A value, parsed at Bootstrap (Step 12) from `server.cfg` and injected into the module; Step 1 only defines the struct + parser. No global state.
- **Future extensibility.** New keys are additive; unknown keys ignored with the established warning behavior. Timeouts are stored as **milliseconds**; conversion to ticks happens once at module Initialize (Step 11) — Step 1 does **not** convert.

### 5.2 `TransportConfig` — section `[transport]`
| Field | Type | Default | Validation | Failure |
|---|---|---|---|---|
| `listenPort` | `std::uint16_t` | `27015` | `1 … 65535` | `InvalidArgument` |
| `bindAddress` | `std::string` | `"0.0.0.0"` | non-empty; length `<= 45` (IPv4/IPv6 textual max) | `InvalidArgument` |
| `backlog` | `std::uint32_t` | `32` | `1 … 1024` | `InvalidArgument` |
| `mtuPayloadBytes` | `std::uint32_t` | `1200` | `256 … 9000` and `> kPacketHeaderWireSize` | `InvalidArgument` |
| `maxOutgoingQueue` | `std::uint32_t` | `256` | `1 … 65536` | `InvalidArgument` |
| `maxIncomingQueue` | `std::uint32_t` | `256` | `1 … 65536` | `InvalidArgument` |
| `maxBytesPerTick` | `std::uint32_t` | `262144` | `>= mtuPayloadBytes` | `InvalidArgument` |
| `reliableWindow` | `std::uint16_t` | `256` | `1 … 65535` | `InvalidArgument` |
| `reassemblyBudgetTicks` | `std::uint32_t` | `120` | `1 … 100000` | `InvalidArgument` |

- **Cross-field rules:** `mtuPayloadBytes > kPacketHeaderWireSize` (a packet must carry at least a header + 1 byte); `maxBytesPerTick >= mtuPayloadBytes` (a tick must be able to move at least one datagram).
- **Ownership.** Same as `NetworkingConfig` — value parsed at Bootstrap, injected into the transport factory (Step 13). Step 1 defines struct + parser only.
- **Bind-address validation.** Step 1 validates **only** non-emptiness and length (no OS `inet_pton` — that would breach One Platform Boundary). Syntactic/semantic address validation happens inside `PlatformSockets.cpp` at bind (Step 13), returning `Expected`. Document this explicitly so ChatGPT does not reach for a socket API here.
- **Future extensibility.** All limits config-driven; additive keys; unknown keys ignored.

> **Parsing responsibility & determinism.** Parsing is pure (no I/O beyond the already-loaded `ConfigStore`), deterministic, and total (every input maps to either a valid config or a described `InvalidArgument`). No exceptions; `Expected<T>` throughout. Timeouts remain in ms (no tick math). Neither `FromConfig` reads any section other than its own.

---

## 6. Constraints checklist (must hold for the step)

- Engine-free and OS-free: no engine header, no `<winsock2.h>`/`<sys/socket.h>`/OS header, no socket/address types anywhere in Step 1.
- ADR-007: no exceptions, no RTTI, no iostream (`common::Format`/C stdio only if any string is built — none required here); `core::Expected<T>` for `FromConfig`; value types trivially copyable and C4530-clean.
- ADR-009 respected by omission: `TransportEndpoint` is an opaque integer handle, not an OS type; no transport behavior.
- ADR-010 respected: header field order/widths and id ranges/constants are fixed here; **no serialization logic** (that is Step 2) and **no raw struct copy** assumption is baked in (`kPacketHeaderWireSize` is independent of `sizeof`).
- ADR-011 respected by omission: no threads, no synchronization primitives.
- Host Authority / Deterministic Simulation: `ConnectionId` host-assigned and ascending; all types deterministic; no wall-clock.
- No wiring: `Bootstrap.cpp`, `FrameDispatcher.h` untouched (no `kNetworking` yet — that is Step 12).

---

## 7. Unit testing — expectations (in `tests/NetworkTests.cpp`)

**Value types & enums.**
- Identifier defaults: `ConnectionId{}.value == 0` and `IsNone()`; `MessageId{}.value == 0`; `TransportEndpoint{}` invalid.
- Equality/inequality for `ConnectionId`, `MessageId`, `TransportEndpoint`.
- Ascending ordering by `value` (sort a shuffled set; assert ascending) — the determinism substrate.
- `IsControlId`/`IsDataId`: boundary tests at `0x00FF` (control) and `0x0100` (data); every named control id is a control id and never a data id.
- Enum default values are 0 with neutral meaning (`Channel::Reliable`, `HandshakeState::None`, `ConnectionState::Disconnected`, `DisconnectReason::None`, `TransportOutcome::Ok`).
- `…Name()` helpers: exhaustive — every enumerator returns its expected string; out-of-range cast returns `"Unknown"`.
- ADR-007 posture: `static_assert` trivial-copyability of `ConnectionId`, `MessageId`, `TransportEndpoint`, `PacketHeader`; `static_assert(kPacketHeaderWireSize == 18)`.
- `PacketHeader` shape: default-constructs zeroed; `flags` bit constants are distinct single bits; control/fragment/ack flags don't overlap.

**Protocol constants.**
- `kProtocolVersion == 1`; `kProtocolMagic != 0`; `kControlIdMin.value == 0x0000`, `kControlIdMax.value == 0x00FF`, `kDataIdMin.value == 0x0100`; named control ids are distinct and within range.

**`NetworkingConfig::FromConfig`.**
- Missing `[networking]` ⇒ all defaults, success.
- Valid overrides parsed correctly (each field).
- Negative tests (each ⇒ `InvalidArgument` with a message naming the key): `maxConnections = 0`; `maxConnections = 5000`; `connectionTimeoutMs = 500`; `handshakeTimeoutMs = 0`; `handshakeTimeoutMs > connectionTimeoutMs`; `heartbeatIntervalMs >= connectionTimeoutMs`; `debugFlags` negative source int.
- Boundary tests: `maxConnections = 1` and `= 4096` accepted; `connectionTimeoutMs = 1000` accepted.

**`TransportConfig::FromConfig`.**
- Missing `[transport]` ⇒ all defaults, success.
- Valid overrides parsed correctly.
- Negative tests: `listenPort = 0`; `bindAddress = ""`; over-length `bindAddress`; `mtuPayloadBytes = 100` (< header+1); `mtuPayloadBytes <= kPacketHeaderWireSize`; `maxBytesPerTick < mtuPayloadBytes`; `maxOutgoingQueue = 0`; `reliableWindow = 0`; `reassemblyBudgetTicks = 0`.
- Boundary tests: `listenPort = 65535`; `mtuPayloadBytes = 256` and `= 9000`; `maxBytesPerTick == mtuPayloadBytes` accepted.
- Independence: a `[transport]` error does not affect `[networking]` parsing and vice-versa (each `FromConfig` reads only its own section).

**Determinism/failure discipline.** Parsing the same `ConfigStore` twice yields identical results; no test triggers an exception (build has exceptions disabled); every failure path returns `Expected` error, never asserts/throws.

---

## 8. Definition of Done (Step 1)

- [ ] All eight files created under the specified paths; two vcxprojs updated (`ClCompile` + `ClInclude`); no other file modified.
- [ ] `ConnectionId`, `MessageId`, `TransportEndpoint`, `Channel`, `HandshakeState`, `ConnectionState`, `DisconnectReason`, `TransportOutcome` defined exactly as §4, each with an exhaustive `…Name()` helper; enums are `enum class : std::uint8_t` with 0 = neutral default.
- [ ] `PacketHeader` shape + `kPacketHeaderWireSize == 18` + `flags` bit constants defined; **no serialization logic** present.
- [ ] `ProtocolConstants.h` defines magic, version (=1), id ranges, and named control ids, all consistent with ADR-010 partitioning.
- [ ] `NetworkingConfig` / `TransportConfig` structs + `FromConfig` implemented per §5 (defaults, per-field + cross-field validation, `InvalidArgument` on bad input, missing-section ⇒ defaults); timeouts kept in ms (no tick conversion); no OS address validation.
- [ ] Engine-free and OS-free: no engine/OS header anywhere in Step 1 (verifiable by include-tree inspection).
- [ ] ADR-007 clean: no exceptions/RTTI/iostream; `Expected<T>` used; trivially-copyable value types; compiles under `EngineAbi.props` (`/W4 /WX`, C4530-clean) and on GCC.
- [ ] No wiring, no tick, no transport, no services, no message processing, no runtime behavior.
- [ ] All §7 unit tests present and green on GCC + MSVC; full existing suite still green; zero new warnings; no regressions.
- [ ] Nothing from any later step introduced; no architecture or ADR modified.

---

## 9. Handoff note to the Implementation Engineer (ChatGPT)

This specification is sufficient to produce **one** implementation prompt for Step 1 with no further architectural decisions. Open items are intentionally *implementation-local* and pre-decided here: (a) the literal value of `kProtocolMagic` (choose any fixed non-zero `u16`, document it in the header); (b) the exact numeric values of internal enums (any stable assignment with 0 = neutral); (c) the internal `ReadU16/U32/Bool/String` helper signatures (follow the Sprint-004 `ReadDouble` pattern). Everything else — field sets, defaults, validation ranges, id partitioning, header layout, file list, test list — is fixed. Do not add transport, sockets, services, message logic, or wiring; those are Steps 2–14.

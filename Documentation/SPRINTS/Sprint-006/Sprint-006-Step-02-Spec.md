# Sprint-006 · Step 2 — Wire Codec (Header Serialize / Deserialize) — Implementation Specification

- **Status:** Implementation specification (no code). Maps to frozen `Sprint-006-Architecture.md` (§5, ADR-010), `Sprint-006-Implementation-Plan.md` Step 2, and the frozen **Step-1 Specification** (which defined `PacketHeader`, `kPacketHeaderWireSize == 18`, the `flags` bit constants, `kProtocolMagic`, `kProtocolVersion`, the id ranges, and `TransportOutcome`). Step 1 types are consumed unchanged.
- **Nature:** Engine-free **and** OS-free. A deterministic, ABI-safe, field-by-field little-endian codec for the fixed wire header — plus the generic byte cursors it is built on. **No** transport, sockets, message registry, connection/table, queues, reliability, fragmentation, handshake, services, or wiring.
- **Evidence gate:** This step satisfies **E-G2-N** (header round-trip + endianness identity across MSVC and GCC).
- **Constraints (binding):** ADR-007 (no exceptions/RTTI/iostream; `core::Expected<T>`; C4530-clean; trivially-copyable value types), ADR-009 (no OS types), ADR-010 (fixed layout, little-endian, **field-by-field only — no `memcpy` of `PacketHeader`, no struct-packing assumption**), ADR-011 (no threads), One Engine/Platform Boundary, Deterministic Simulation.

---

## 1. Files to create

| File | Contents |
|---|---|
| `include/stalkermp/net/ByteCursor.h` | `ByteWriter` and `ByteReader` — generic bounds-checked little-endian byte cursors over a caller-owned span. Header-only (small, `inline`), engine/OS-free. |
| `src/net/PacketHeader.cpp` | Definitions of `SerializeHeader` / `DeserializeHeader` (declared in `PacketHeader.h`), plus the header-validation predicate(s). Field-by-field encode/decode using the cursors. |

No other new files. (`PacketHeader.h` already exists from Step 1; Step 2 only *adds function declarations* to it — see §2. Tests go into the existing `tests/NetworkTests.cpp`.)

## 2. Files to modify

| File | Why |
|---|---|
| `include/stalkermp/net/PacketHeader.h` | Add the free-function **declarations** `SerializeHeader(...)`, `DeserializeHeader(...)`, and `ValidateHeader(...)` (see §5–§6), and the `HeaderError`/result vocabulary (§7). The `PacketHeader` struct, `kPacketHeaderWireSize`, and flag constants are **unchanged** from Step 1. Rationale: keep the header's serialization API co-located with the header type, exactly as the codec belongs with the shape it serializes. |
| `xrMP.vcxproj` | Add `src\net\PacketHeader.cpp` to `ClCompile`; add `include\stalkermp\net\ByteCursor.h` to `ClInclude`. Required to compile the codec TU. |
| `tests/xrMP_Tests.vcxproj` | Add `..\src\net\PacketHeader.cpp` to `ClCompile`. Required so the test build links the codec. (`NetworkTests.cpp` already listed from Step 1.) |

No other file is modified. (Transport, services, `Bootstrap.cpp`, `FrameDispatcher.h`, `EngineAdapters.*` untouched.)

---

## 3. `ByteWriter` / `ByteReader` — responsibilities

Both are thin, non-owning cursors over a contiguous byte span (`std::uint8_t*` + size for the writer; `const std::uint8_t*` + size for the reader). They are the reusable primitive on which the header codec — and every later Sprint-006 serializer — is built.

### 3.1 Buffer & cursor ownership
- **Non-owning.** Neither cursor allocates or frees. The caller owns the underlying buffer and guarantees it outlives the cursor. (Value-semantics discipline consistent with the rest of the module; no aliasing surprises.)
- **Cursor state.** Each holds a mutable offset (`std::size_t position`) into the fixed-size span. Construction sets `position = 0`.
- **Trivially copyable.** Copying a cursor copies the pointer/size/position (a bookmark); it does not copy the buffer. Document that two cursors over the same buffer share bytes but not position.

### 3.2 Write semantics (`ByteWriter`)
- Typed appends for the exact wire field widths used by the header: `WriteU8`, `WriteU16`, `WriteU32` (and, reserved for later steps, `WriteBytes(span)` for payloads — *may* be included since it is the same primitive, but it is not required by Step 2 and must remain unused here).
- Each write: (1) bounds-check `position + width <= size`; on failure return `TransportOutcome::WouldBlock`-equivalent codec error (see §7) and **do not advance**; (2) emit the value **little-endian** byte-by-byte (`byte[i] = (value >> (8*i)) & 0xFF`); (3) advance `position` by `width`.
- **Return type.** `core::Expected<void>` (or a codec-specific `Expected`) — never a bare bool that can be ignored; ADR-007 value-error discipline.
- Query helpers: `BytesWritten()` (== `position`), `Remaining()` (`size - position`), `Capacity()` (`size`).

### 3.3 Read semantics (`ByteReader`)
- Typed extracts mirroring the writer: `ReadU8`, `ReadU16`, `ReadU32` (+ reserved `ReadBytes(count)` returning a `const`-span view/`Expected`, unused in Step 2).
- Each read: (1) bounds-check `position + width <= size`; on underflow return the codec error and **do not advance**; (2) reassemble the value from little-endian bytes (`value |= byte[i] << (8*i)`); (3) advance `position`.
- **Return type.** `core::Expected<std::uintN_t>` for the typed reads.
- Query helpers: `BytesRead()`, `Remaining()`, `Size()`.

### 3.4 Overflow / underflow behavior
- **Deterministic and total.** Writing past capacity → error, no partial write past the boundary, position unchanged for the failing call. Reading past the end → error, position unchanged. No exception, no UB, no truncation silently.
- The cursor never resizes; overflow is a caller/contract error surfaced as a value.

### 3.5 Bounds checking
- Every typed read/write is individually bounds-checked before touching memory. There is no "unchecked fast path." (Determinism + safety > micro-perf for infrastructure.)

### 3.6 Alignment guarantees
- **None required and none assumed.** Access is byte-granular (`std::uint8_t` reads/writes + shifts); the codec never reinterpret-casts the span to a wider type and never dereferences a misaligned pointer. This is precisely what makes it ABI-safe and portable (ADR-010) — no alignment or padding of `PacketHeader` ever reaches the wire.

### 3.7 Endian-conversion responsibility
- **The cursors own endianness.** All multi-byte values are encoded/decoded little-endian **by explicit shifts**, independent of host endianness. No `htons`/`ntohs` (OS/platform), no `std::byteswap` dependency, no compiler intrinsics required. Host byte order is irrelevant by construction — the same bytes result on MSVC/x64 and GCC. This is the crux of E-G2-N.

### 3.8 Error handling & validation responsibility
- Cursors validate **space only** (bounds). Semantic validation (magic/version/flags/length) belongs to the header codec (§6), not the cursor. This separation keeps the cursor a pure, reusable primitive.

---

## 4. Serialization contract (ADR-010) — precise rules

### 4.1 Field order (fixed, exactly as Step 1's `PacketHeader`)
`magic (u16) → protocolVersion (u16) → channel (u8) → flags (u8) → sequence (u16) → ack (u16) → ackBits (u32) → messageId (u16) → payloadLength (u16)`.

### 4.2 Field sizes & total
`2+2+1+1+2+2+4+2+2 = 18 bytes == kPacketHeaderWireSize`. `SerializeHeader` must write exactly 18 bytes; `DeserializeHeader` must consume exactly 18 bytes.

### 4.3 Little-endian conversion
Every `u16`/`u32` field is emitted least-significant-byte-first via the cursor (§3.7). `u8` fields (`channel`, `flags`) are single bytes (endianness-neutral). `channel` is written from the `Channel` enum's underlying `std::uint8_t`; `messageId` from `MessageId.value`.

### 4.4 Padding & alignment rules
- **Zero padding on the wire.** The 18 bytes are contiguous field bytes; no inter-field padding, no trailing pad. `sizeof(PacketHeader)` (which may be 20 due to struct alignment) is **irrelevant** and must never be used for framing — `kPacketHeaderWireSize` is authoritative.

### 4.5 Forbidden operations (must not appear)
- No `memcpy`/`std::memcpy` of `PacketHeader` (or any reinterpret of the struct as bytes).
- No `reinterpret_cast<PacketHeader*>(buffer)`.
- No `#pragma pack` / packed-struct assumption.
- No `union` type-punning.
- No host-endian-dependent code path.
- No OS byte-order function, no engine header, no OS header.

### 4.6 Serialize responsibility (`SerializeHeader`)
- Signature (shape): `SerializeHeader(const PacketHeader&, ByteWriter&) -> core::Expected<void>`.
- Writes the nine fields in order via the writer; if any write fails (insufficient space), returns that error immediately (no partial-header left as "valid"). On success the writer has advanced exactly 18 bytes.
- Pure function of its inputs (deterministic).

### 4.7 Deserialize responsibility (`DeserializeHeader`)
- Signature (shape): `DeserializeHeader(ByteReader&) -> core::Expected<PacketHeader>`.
- Reads the nine fields in order; on any underflow returns the codec error. On success returns the populated header **and** the reader has advanced exactly 18 bytes.
- **Validation is applied after field extraction** (§6) — a header that reads structurally but fails semantic checks returns a *validation* error, distinct from an underflow error.

---

## 5. Validation order & failure behavior

`DeserializeHeader` (and the standalone `ValidateHeader(const PacketHeader&, std::size_t availablePayloadBytes)` used by callers who already have a header) applies checks in this **fixed order**, returning the first failure:

1. **Wire-size / underflow** (during extraction): fewer than 18 bytes available → `HeaderError::Truncated`. (Checked implicitly by the reader per field; the net effect is "not enough bytes for a full header.")
2. **Magic**: `magic != kProtocolMagic` → `HeaderError::BadMagic`. (Checked first among semantic checks — cheapest rejection of non-protocol/garbage datagrams.)
3. **Protocol version**: `protocolVersion != kProtocolVersion` → `HeaderError::VersionMismatch`. (Exact-match rule, ADR-010; no negotiation in Sprint-006.)
4. **Channel**: `channel` not in `{Reliable, Unreliable}` → `HeaderError::BadChannel`.
5. **Reserved flags**: any bit set outside the defined set `{kFlagFragment, kFlagAck, kFlagControl}` → `HeaderError::ReservedFlag`.
6. **Payload length**: `payloadLength > availablePayloadBytes` (when a payload context is supplied) → `HeaderError::BadLength`; and `payloadLength` must not exceed the configured `mtuPayloadBytes - kPacketHeaderWireSize` upper bound when that bound is provided by the caller. Within pure `DeserializeHeader` (no payload context) only the field itself is read; the length-vs-buffer check is performed by `ValidateHeader` at the call site that owns the datagram size.

**Failure behavior.** Every failure is a returned `Expected` error carrying a `HeaderError` code and a short `common::Format` message (no iostream). Nothing throws; nothing asserts on attacker-controlled input. On failure the caller drops the datagram and (later steps) increments a counter — Step 2 only returns the error.

**Corrupted-header handling.** A corrupted datagram manifests as one of: `Truncated` (short), `BadMagic` (garbage/other protocol), `VersionMismatch`, `BadChannel`, `ReservedFlag`, or `BadLength`. All are benign, non-crashing rejections. No corrupted input can advance past validation.

**Invalid magic / invalid version.** Explicit, ordered as above (magic before version). These are the two earliest, cheapest rejections so a flood of non-protocol traffic is discarded with minimal work.

**Unknown flag behavior.** Bits **outside** the defined set are a hard `ReservedFlag` rejection in Sprint-006 (strict, since no forward-compat flag semantics are defined yet). *Note the deliberate asymmetry with unknown message ids:* unknown reserved header **flag bits** are rejected, whereas unknown **message ids** are tolerated/dropped by the registry (Step 4) — flags gate framing integrity, ids gate routing. This is intentional and must be preserved.

**Payload-length validation.** `payloadLength` is a `u16` (max 65535); the semantic bound is `<= mtuPayloadBytes - kPacketHeaderWireSize` and `<= actual bytes remaining in the datagram`. The field is always read; the bound checks happen wherever the datagram size / config is known (`ValidateHeader`), keeping `DeserializeHeader` a pure header extractor.

---

## 6. Result / error vocabulary (added to `PacketHeader.h`)

- `enum class HeaderError : std::uint8_t { None = 0, Truncated, BadMagic, VersionMismatch, BadChannel, ReservedFlag, BadLength }` with an exhaustive `HeaderErrorName(...)` helper (mirrors Step-1 enum discipline).
- Codec functions return `core::Expected<T>`; the `Expected` error is constructed via `core::MakeError(core::ErrorCode::InvalidArgument, <message>)` with the message naming the `HeaderError` (so the existing error channel is reused — no new error framework). Optionally expose a thin `ToHeaderError(const core::Error&)` if a test wants the discriminant; not required.
- The cursor over/underflow error uses the same `Expected`/`InvalidArgument` channel with a distinct message (`"byte cursor overflow"` / `"byte cursor underflow"`).

---

## 7. Constraints checklist (must hold)

- Engine-free and OS-free; include-tree contains only `stalkermp/*` + standard headers.
- ADR-007: no exceptions/RTTI/iostream; `Expected<T>` everywhere fallible; `ByteWriter`/`ByteReader`/`PacketHeader` trivially copyable; compiles `/W4 /WX`, C4530-clean, and on GCC `-Wall -Wextra -Werror`.
- ADR-010: field-by-field little-endian; **no `memcpy`/reinterpret of `PacketHeader`**; `kPacketHeaderWireSize` (not `sizeof`) drives framing; field order/widths exactly §4.1–§4.2.
- ADR-009/011: no OS types, no threads, no synchronization.
- Deterministic: same input bytes → same header (and vice-versa) on any platform/toolchain; no wall-clock, no global state.
- Scope: nothing from Steps 3–14; no transport, no registry, no wiring; `WriteBytes`/`ReadBytes` may exist as the same primitive but must be unexercised by Step 2 logic and tests beyond, at most, trivial presence.

---

## 8. Unit testing — expectations (in `tests/NetworkTests.cpp`)

**Cursor primitives.**
- `ByteWriter` writes `u8/u16/u32` little-endian: assert exact bytes (e.g. `WriteU16(0x0102)` → bytes `{0x02, 0x01}`; `WriteU32(0x01020304)` → `{0x04,0x03,0x02,0x01}`).
- `ByteReader` reads back the same values; `BytesWritten()`/`BytesRead()`/`Remaining()` correct after each op.
- Overflow: writing into a too-small buffer returns error, `position` unchanged, no bytes past bound touched.
- Underflow: reading past end returns error, `position` unchanged.
- Cursor is trivially copyable (`static_assert`); a copied cursor shares buffer, independent position.

**Header round-trip (E-G2-N core).**
- Serialize a fully-populated `PacketHeader` (distinct, non-zero, multi-byte values in every field, at least one flag set) into an 18-byte buffer; assert exactly 18 bytes written; deserialize; assert field-by-field equality.
- Assert the on-wire byte layout explicitly for a known header (hard-coded expected 18-byte array) — this is the cross-toolchain determinism anchor: the same expected bytes must hold on MSVC and GCC.
- `static_assert(kPacketHeaderWireSize == 18)` and assert `SerializeHeader` advances the writer by exactly 18.

**Endianness.**
- Multi-byte fields (`magic`, `sequence`, `ackBits`, `messageId`, `payloadLength`) verified LSB-first in the buffer regardless of host — asserted via explicit byte positions.

**Boundary / wire-size.**
- Serialize into an exactly-18-byte buffer succeeds; into a 17-byte buffer fails at the field that overflows, with no out-of-bounds write.
- Deserialize from exactly 18 bytes succeeds; from 17 bytes → `Truncated`.

**Validation / corrupted-header failure paths (each returns the specified error, no throw):**
- `BadMagic`: flip `magic`.
- `VersionMismatch`: set `protocolVersion = kProtocolVersion + 1`.
- `BadChannel`: set `channel = 2`.
- `ReservedFlag`: set a flag bit outside the defined set.
- `BadLength` (via `ValidateHeader` with a payload context): `payloadLength` greater than available bytes / greater than `mtuPayloadBytes - kPacketHeaderWireSize`.
- Validation **order**: a header that is simultaneously bad-magic and bad-version returns `BadMagic` (magic checked first); bad-version + bad-channel returns `VersionMismatch`.

**Determinism.**
- Serializing the same header twice yields identical bytes; deserializing the same bytes twice yields identical headers; no test triggers an exception (exceptions disabled).

**Cross-toolchain expectation (documented for Antigravity).** The explicit expected-byte-array tests must pass **identically** on MSVC Release (x64, `EngineAbi.props`) and GCC — that equality *is* evidence gate E-G2-N. No test may depend on host endianness, struct size, or padding.

---

## 9. Definition of Done (Step 2)

- [ ] `ByteCursor.h` created with `ByteWriter`/`ByteReader` per §3 (non-owning, bounds-checked, little-endian by explicit shifts, `Expected` on over/underflow, byte-granular/no alignment assumption, trivially copyable).
- [ ] `SerializeHeader` / `DeserializeHeader` / `ValidateHeader` declared in `PacketHeader.h`, defined in `PacketHeader.cpp`, field-by-field per §4 — **no `memcpy`/reinterpret of `PacketHeader`, no packing assumption**; framing driven by `kPacketHeaderWireSize`.
- [ ] `HeaderError` enum + `HeaderErrorName` added; codec errors flow through `core::Expected` / `InvalidArgument`.
- [ ] Validation implemented in the fixed order (§5): truncation → magic → version → channel → reserved flags → length; each returns the specified error; unknown reserved flag bits rejected; unknown-message-id handling explicitly **not** done here (Step 4).
- [ ] Two vcxprojs updated (`PacketHeader.cpp` compiled in both; `ByteCursor.h` in `ClInclude`); no other file modified beyond `PacketHeader.h`'s added declarations.
- [ ] Engine-free and OS-free (include-tree verified); ADR-007/009/010/011 preserved.
- [ ] All §8 tests present and green on GCC + MSVC; explicit expected-byte arrays identical across toolchains (**E-G2-N**); full existing suite green; zero new warnings; no regressions.
- [ ] Nothing from Steps 3–14 introduced; no transport/registry/wiring; no architecture or ADR modified.

---

## 10. Handoff note to the Implementation Engineer (ChatGPT)

This specification is sufficient to produce **one** Step-2 implementation prompt with no further architectural or implementation decisions. Pre-decided implementation-local choices: (a) byte element type is `std::uint8_t`; (b) little-endian is realized by explicit shift/mask (no intrinsics, no OS byte-order calls); (c) cursors are non-owning over a caller span with a `std::size_t position`; (d) codec errors reuse `core::Expected` + `ErrorCode::InvalidArgument` with a `HeaderError`-named message (no new error type); (e) `kPacketHeaderWireSize` (18) — never `sizeof(PacketHeader)` — drives framing. The reserved `WriteBytes`/`ReadBytes` may be declared for later steps but must carry no Step-2 behavior. Do not implement transport, sockets, message registry, connection/table, queues, reliability, fragmentation, handshake, services, or wiring — those are Steps 3–14.

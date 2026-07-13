# Sprint-006 · Step 9 — Packet Queues + Reliability/Fragmentation Bookkeeping — Implementation Specification

- **Status:** Implementation specification (no code). Maps to frozen `Sprint-006-Architecture.md` (§4) and Implementation Plan Step 9. Consumes Steps 1–2, 5. Engine-free / OS-free.
- **Nature:** Per-connection/per-channel bounded queues + reliable ack/retransmit bookkeeping + fragmentation/reassembly. **No** host loop (Step 10), no services (Step 11), no real transport.
- **Constraints:** ADR-007; ADR-010 (header sequence/ack/ackBits/flags fields drive reliability; fragment sub-header); deterministic overflow policy; bounded reassembly; owned buffers (no aliasing into transport memory); One Engine/Platform Boundary.

## 1. Files to create
| File | Contents |
|---|---|
| `include/stalkermp/net/PacketQueues.h` + `src/net/PacketQueues.cpp` | Bounded outgoing/incoming queues per connection/channel. |
| `include/stalkermp/net/Reliability.h` + `src/net/Reliability.cpp` | Sliding-window ack/retransmit bookkeeping. |
| `include/stalkermp/net/Fragmentation.h` + `src/net/Fragmentation.cpp` | Fragment/reassemble large messages. |

## 2. Files to modify
`include/stalkermp/net/Connection.h` — add per-channel queue handles (indices into Module-owned queue storage, **not** pointers) + reliability/reassembly state handles. `xrMP.vcxproj` / `tests/xrMP_Tests.vcxproj` (add the three `.cpp`, headers); `tests/NetworkTests.cpp` (Step-9 tests with mock drop/reorder/duplicate).

## 3. Responsibilities
- **`PacketQueues`.** Per connection, per `Channel`: bounded `outgoing` and `incoming` queues of owned byte buffers (`std::vector<std::uint8_t>` by value). Bounds `maxOutgoingQueue` / `maxIncomingQueue` (config). Overflow policy (deterministic): **reliable outgoing overflow → disconnect** (`DisconnectReason::TransportError` signaled to Step 7's path); **unreliable overflow → drop-oldest**. `Enqueue`/`Dequeue`/`Count`/`Clear`.
- **`Reliability`** (reliable channel only). Sliding window over `sequence`; per-packet in-flight record with send tick; ack processing from inbound `ack` + `ackBits` (32-packet ack history, ADR-010 header); retransmit selection for un-acked packets past a retransmit interval (tick-derived); `reliableWindow` bound (config) — exceeding it blocks new reliable sends (backpressure) rather than growing unbounded. Newest-wins is **not** used on reliable (ordered); it is used on unreliable via `sequence` comparison.
- **`Fragmentation`.** Messages larger than `mtuPayloadBytes - kPacketHeaderWireSize` are split into indexed fragments carried on the **reliable** channel with a `(messageId, fragmentIndex, fragmentCount)` sub-header (the `kFlagFragment` header bit set). Reassembly buffer per (connection, messageId) is bounded and time-boxed (`reassemblyBudgetTicks`): incomplete reassembly past the budget is dropped. `Fragment(payload) -> fragments`; `Accept(fragment) -> optional<completedMessage>`.

## 4. Ownership & lifetime
Queue storage is owned by the Module (Step 11) as a flat store; `Connection` holds **indices/handles** into it (no pointers), so table growth/removal never dangles. Owned byte buffers are moved in/out — never aliased into transport-internal memory. Reassembly buffers are owned by the fragmentation component, keyed by (connection, messageId), and reclaimed on completion, timeout, or disconnect (via Step 7's release path, which Step 9 extends).

## 5. Dependencies
Steps 1–2, 5 (Connection to carry handles; codec header fields for sequence/ack/flags). Not Step 10/11 (but designed to slot into them).

## 6. Constraints
Bounded queues; deterministic overflow (reliable→disconnect, unreliable→drop-oldest); tick-derived retransmit; bounded/time-boxed reassembly; owned buffers; ADR-007/010; engine-free.

## 7. Validation rules
- Queue never exceeds its bound; overflow triggers the specified policy exactly.
- Reliable window never exceeds `reliableWindow`; acked packets removed from in-flight; retransmit only past interval.
- Fragment indices `< fragmentCount`; reassembly completes only when all fragments present within budget; duplicate fragments idempotent.
- `ValidateConsistency` (queues within bounds; no orphan reassembly buffers).

## 8. Failure behavior
Reliable overflow → `TransportError` disconnect signal (no throw); unreliable overflow → silent drop-oldest (counted); incomplete reassembly past budget → drop (counted); duplicate/late fragment → ignored. No exceptions.

## 9. Unit testing (mock drop/reorder/duplicate)
- Enqueue/dequeue FIFO within bound; unreliable overflow drops oldest; reliable overflow raises the disconnect signal.
- Reliability: send N reliable, ack a subset via `ack`/`ackBits`, assert un-acked retransmit past interval and acked do not; window backpressure at `reliableWindow`.
- Fragmentation: a message > MTU splits into ceil(size/frag) fragments; reassembly under reorder/duplicate completes correctly; incomplete past `reassemblyBudgetTicks` drops.
- Newest-wins on unreliable: older `sequence` after newer is discarded.

## 10. Negative testing
Reliable overflow → disconnect signal (not silent); fragment index ≥ count → rejected; reassembly missing a fragment past budget → dropped, buffer reclaimed; ack for unknown sequence → ignored.

## 11. Boundary testing
Queue at exactly bound accepts, bound+1 triggers policy; message of exactly MTU payload = 1 fragment; message of MTU+1 = 2 fragments; window exactly full blocks the next reliable send.

## 12. Definition of Done
- [ ] Bounded per-connection/per-channel queues with the exact overflow policies; owned buffers; handles-not-pointers on `Connection`.
- [ ] Reliable ack/retransmit within `reliableWindow`; unreliable newest-wins; tick-derived retransmit.
- [ ] Fragmentation/reassembly bounded + time-boxed; `kFlagFragment` + fragment sub-header (ADR-010).
- [ ] `ValidateConsistency`; deterministic under mock loss/reorder/duplicate; engine-free; ADR-007/010.
- [ ] §9–§11 tests green (GCC + MSVC); full suite green; no regressions.
- [ ] Nothing from Steps 10–14.

## 13. Handoff notes (ChatGPT)
Pre-decided: queue element is `std::vector<std::uint8_t>` (owned); `Connection` stores integer handles into a Module-owned queue store (added to `Connection` here, populated by Step 11 wiring); retransmit interval is tick-derived from a config/const; fragment sub-header is `(u16 messageId, u16 fragmentIndex, u16 fragmentCount)` inside the payload, guarded by `kFlagFragment`. Do not implement the host pump (Step 10) or the module store allocation (Step 11) — only the data structures + algorithms, tested via direct calls + mock transport.

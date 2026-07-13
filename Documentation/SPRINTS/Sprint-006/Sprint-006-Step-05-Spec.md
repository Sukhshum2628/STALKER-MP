# Sprint-006 · Step 5 — ConnectionTable + Connection — Implementation Specification

- **Status:** Implementation specification (no code). Maps to frozen `Sprint-006-Architecture.md` (§3) and Implementation Plan Step 5. Consumes Step-1 (`ConnectionId`, `ConnectionState`, `HandshakeState`, `DisconnectReason`, `TransportEndpoint`) and Step-3 (`TransportEndpoint` type). Engine-free / OS-free.
- **Nature:** Connection storage + lifecycle **state storage only** — no transport interaction, no handshake logic, no timeouts, no queues.
- **Constraints:** ADR-007; deterministic ascending-`ConnectionId` ordering; on-demand resolution (no retained `Connection*` across ticks); One Engine/Platform Boundary.

## 1. Files to create
| File | Contents |
|---|---|
| `include/stalkermp/net/Connection.h` | `Connection` record (value struct). |
| `include/stalkermp/net/ConnectionTable.h` | Capacity-bounded table (sorted-vector + hash accelerator). |
| `src/net/ConnectionTable.cpp` | Add/resolve/remove/assign-id/validate logic. |

## 2. Files to modify
`xrMP.vcxproj` / `tests/xrMP_Tests.vcxproj` (add `ConnectionTable.cpp`, two headers); `tests/NetworkTests.cpp` (Step-5 tests).

## 3. Responsibilities
- **`Connection`.** Value record holding: `ConnectionId id`; `TransportEndpoint endpoint`; `ConnectionState state = Disconnected`; `HandshakeState handshake = None`; `DisconnectReason lastReason = None`; timing slots `std::uint64_t lastRecvTick = 0`, `lastSendTick = 0`, `establishedTick = 0`; sequence-number state slots `std::uint16_t nextSequence = 0`, `remoteSequence = 0`; reserved `std::uint32_t playerSlot = 0` (null; Sprint-007) and `std::uint64_t reconnectToken = 0` (null; Sprint-012). **No** queue objects here (queues are Step 9 — this step reserves only integer handle slots if needed, or leaves queue wiring to Step 9). Owns no simulation data, no player identity.
- **`ConnectionTable`.** Capacity = `maxConnections` (from config, injected). Canonical sorted `std::vector<Connection>` ascending by `ConnectionId.value` + `std::unordered_map<u32, index>` accelerator (BC-005). API: `Add(TransportEndpoint) -> core::Expected<ConnectionId>` (assigns the next ascending, non-reused id; `CapacityFull`-style failure when full); `Find(ConnectionId) const -> const Connection*` / `FindMutable(ConnectionId) -> Connection*` (on-demand resolution; nullptr if absent); `Remove(ConnectionId) -> void`; `Count() const`; `Ids() const -> std::vector<ConnectionId>` (ascending); `ValidateConsistency() const -> report`.

## 4. Ownership & lifetime
- **Ownership.** The table owns all `Connection` records by value. The host/session (later steps) hold `ConnectionId` handles only — **never** a `Connection*` retained across ticks; they re-resolve by id each use (generalizes the E-G2 discipline). `TransportEndpoint` is owned by the transport; the connection stores the handle only.
- **Lifetime.** Ids are session-unique and **never reused within a session** (a monotonically increasing `nextId` counter, skipping 0). Removing a connection frees its slot but not its id for reuse.

## 5. Dependencies
Steps 1, 3. (Config `maxConnections` is injected as a value; the table does not parse config.)

## 6. Constraints
Ascending id assignment; capacity-bounded; on-demand resolution; deterministic iteration; no transport calls, no handshake/timeout/queue logic; ADR-007; engine-free.

## 7. Validation rules
`ValidateConsistency`: canonical vector strictly ascending & unique by id; accelerator size == vector size and every id maps to its index; `Count() <= maxConnections`; no id == 0.

## 8. Failure behavior
`Add` at capacity → `Expected` failure (reason maps to `DisconnectReason::CapacityFull` at the call site). `Find` of an absent id → nullptr (benign). No exceptions.

## 9. Unit testing
- `Add` assigns ascending ids (1,2,3…), never 0, never reused after `Remove`.
- `Find`/`FindMutable` resolve present ids; nullptr for absent.
- `Remove` keeps ascending order + accelerator consistent; `Ids()` ascending.
- `ValidateConsistency` healthy across add/remove churn.
- Default `Connection` fields are the neutral defaults (Disconnected/None/0; reserved slots 0).

## 10. Negative testing
`Add` beyond `maxConnections` → failure; `Find(absent)`/`Remove(absent)` → benign no-op/nullptr; id 0 never assigned.

## 11. Boundary testing
Fill to exactly `maxConnections` (all succeed), next `Add` fails; remove one, `Add` succeeds with a **new** id (not the removed one).

## 12. Definition of Done
- [ ] `Connection` value record per §3 (reserved player/reconnect slots null); `ConnectionTable` sorted-vector + hash, ascending ids, capacity-bounded, on-demand resolution.
- [ ] No transport/handshake/timeout/queue logic; no retained `Connection*` API across ticks.
- [ ] `ValidateConsistency` implemented; engine-free/OS-free; ADR-007.
- [ ] §9–§11 tests green (GCC + MSVC); full suite green; no regressions.
- [ ] Nothing from Steps 6–14.

## 13. Handoff notes (ChatGPT)
Pre-decided: id assignment via a monotonic `u32 nextId` starting at 1; `Add` takes the endpoint and returns the id; `FindMutable` is the only mutation entry (state transitions are driven by later steps through it); queue handles are added in Step 9, not here. Do not implement handshake, timeout, or transport interaction.

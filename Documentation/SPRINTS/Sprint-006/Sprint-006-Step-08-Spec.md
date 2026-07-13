# Sprint-006 · Step 8 — Session + SessionService + ISessionObserver — Implementation Specification

- **Status:** Implementation specification (no code). Maps to frozen `Sprint-006-Architecture.md` (§6) and Implementation Plan Step 8. Consumes Steps 1, 5–7. Engine-free / OS-free.
- **Nature:** Authoritative membership + join/leave observer seam + reserved reconnect hook. **No** persistence, no reconnect *logic*, no transport internals.
- **Constraints:** ADR-007; Host Authority (host-authoritative admission); deterministic ascending iteration; transient/reconstructable session state; One Engine/Platform Boundary.

## 1. Files to create
| File | Contents |
|---|---|
| `include/stalkermp/net/Session.h` | `Session` (members keyed by `ConnectionId`). |
| `src/net/Session.cpp` | Admit/remove/observer-fire/reclaim logic. |
| `include/stalkermp/net/SessionService.h` | `core::IService` owning `Session` + observer registry. |
| `src/net/SessionService.cpp` | Service lifecycle. |
| `include/stalkermp/net/ISessionObserver.h` | Join/leave observer seam. |

## 2. Files to modify
`xrMP.vcxproj` / `tests/xrMP_Tests.vcxproj` (add `Session.cpp`, `SessionService.cpp`, three headers); `tests/NetworkTests.cpp` (Step-8 tests).

## 3. Responsibilities
- **`Session`.** Authoritative set of admitted members. `Member { ConnectionId id; std::uint64_t joinTick; std::uint32_t playerSlot = 0 /*reserved*/; std::uint64_t reconnectToken = 0 /*reserved*/; }`. Sorted-vector + hash accelerator (BC-005), ascending `ConnectionId`. API: `Admit(ConnectionId, tick) -> core::Expected<void>` (capacity-checked against `maxConnections`; duplicate admit → `AlreadyExists`); `Remove(ConnectionId) -> void` (fires `OnMemberLeft` exactly once, stashes a `ReconnectToken`); `Contains`, `Count`, `Members() const` (ascending); `TryReclaim(ReconnectToken) -> core::Expected<ConnectionId>` returning `NotFound`/"not supported" in Sprint-006; `ValidateConsistency`.
- **`ISessionObserver`.** `OnMemberJoined(ConnectionId, tick)` / `OnMemberLeft(ConnectionId, DisconnectReason, ReconnectToken)`. Non-owning subscribers; observers fire **exactly once** per event, in registration order (deterministic).
- **`SessionService`.** `core::IService` (not `ITickable`). `Name() = "Session"`; `Dependencies() = {}` beyond the module; owns the `Session` + the observer registry; exposes `Session()` accessor and `Subscribe(ISessionObserver*)` for later sprints.
- **Admission trigger.** Admission is invoked (by the host, Step 10) at handshake `Established`; removal is invoked on any disconnect (Step 7's path calls `Session::Remove`). Step 8 delivers the mechanism; the host wires the calls in Step 10.

## 4. Ownership & lifetime
`SessionService` owns the `Session` and observer registry. The session holds `ConnectionId`s (and reserved slots), **not** `Connection*` — membership is by id, resolved against the table when needed. `ReconnectToken` is an opaque `u64` minted on removal (deterministic; e.g. derived from id + joinTick — documented as opaque). Session state is transient and reconstructable (never persisted).

## 5. Dependencies
Steps 1, 5–7. Not Step 9.

## 6. Constraints
Host-authoritative admission; capacity-checked; observers fire once; ascending iteration; no persistence/reconnect logic; `IService` (no tick); ADR-007; engine-free.

## 7. Validation rules
`ValidateConsistency`: members ascending & unique by id; accelerator consistent; `Count() <= maxConnections`; every member id is non-zero. `TryReclaim` always returns not-supported in Sprint-006 (documented).

## 8. Failure behavior
`Admit` at capacity → failure (maps to `CapacityFull` rejection at the host); duplicate admit → `AlreadyExists`; `Remove(absent)` → no-op (no spurious `OnMemberLeft`); `TryReclaim` → `NotFound`. No exceptions.

## 9. Unit testing
- Admit → `Count` increments, `OnMemberJoined` fires once, ascending order preserved.
- Remove → `Count` decrements, `OnMemberLeft` fires once with reason + token; second remove is no-op (no second fire).
- Multiple observers all fire in registration order.
- Capacity: admit up to `maxConnections`; next admit fails.
- `ValidateConsistency` healthy across churn; `TryReclaim` returns not-supported.

## 10. Negative testing
Duplicate admit → `AlreadyExists`, single membership; `Remove(absent)` → no fire; admit beyond capacity → failure; `TryReclaim(anything)` → `NotFound`.

## 11. Boundary testing
Fill session to exactly `maxConnections`; remove one then admit a new id (new membership, `OnMemberLeft`+`OnMemberJoined` fire once each); empty-session `Members()` empty.

## 12. Definition of Done
- [ ] `Session` (ascending, capacity-bounded, reserved player/reconnect slots) + `SessionService` + `ISessionObserver`.
- [ ] Admit/remove fire observers exactly once; `TryReclaim` reserved (not-supported); transient/reconstructable.
- [ ] `ValidateConsistency`; `IService` only; engine-free; ADR-007; Host Authority preserved.
- [ ] §9–§11 tests green (GCC + MSVC); full suite green; no regressions.
- [ ] No persistence/reconnect logic; nothing from Steps 9–14.

## 13. Handoff notes (ChatGPT)
Pre-decided: `ReconnectToken` is an opaque `u64` (deterministic derivation, documented as opaque); observers are non-owning raw pointers stored in registration order; the host (Step 10) calls `Admit` at Established and the Step-7 disconnect path calls `Remove`. Do not implement reconnect reclaim or persistence — only the reserved seam.

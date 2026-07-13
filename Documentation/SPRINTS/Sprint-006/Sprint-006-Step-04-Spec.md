# Sprint-006 · Step 4 — MessageRegistry (+ MessageRegistryService) — Implementation Specification

- **Status:** Implementation specification (no code). Maps to frozen `Sprint-006-Architecture.md` (§5, §10, ADR-010) and Implementation Plan Step 4. Consumes Step-1 (`MessageId`, control-id constants), Step-2 (`ByteReader` for handler signature). Engine-free / OS-free.
- **Nature:** Deterministic id→handler routing for the **control range only**. **No** gameplay ids, no transport, no connection/session, no wiring.
- **Constraints:** ADR-007 (no exceptions/RTTI/iostream; `core::Expected`), ADR-010 (id ranges/partitioning; additive ids), One Engine/Platform Boundary, Deterministic Simulation.

## 1. Files to create
| File | Contents |
|---|---|
| `include/stalkermp/net/MessageRegistry.h` | The registry (id → handler entry) + `IMessageHandler` seam. |
| `src/net/MessageRegistry.cpp` | Registration/lookup/dispatch logic. |
| `include/stalkermp/net/MessageRegistryService.h` | `core::IService` wrapper owning a `MessageRegistry`, registering the control ids. |
| `src/net/MessageRegistryService.cpp` | Service lifecycle + control-id registration. |

## 2. Files to modify
`xrMP.vcxproj` / `tests/xrMP_Tests.vcxproj` (add the two `.cpp`, two headers); `tests/NetworkTests.cpp` (Step-4 tests).

## 3. Responsibilities
- **`MessageRegistry`.** Maps `MessageId → RegistryEntry`. `Register(MessageId, IMessageHandler*) -> core::Expected<void>` (duplicate id → `AlreadyExists` failure; id outside a legal range → `InvalidArgument`). `Dispatch(MessageId, ByteReader& payload, DispatchContext&) -> DispatchResult` where unknown id → `DispatchResult::Unknown` + counter increment (no error, forward-compatible). `Contains(MessageId) const`, `Count() const`, id-ordered iteration for determinism. Sorted-vector + hash accelerator (BC-005) keyed by `MessageId.value` ascending.
- **`IMessageHandler`.** Engine-free seam: `Handle(ByteReader& payload, DispatchContext&) -> core::Expected<void>`. `DispatchContext` is an opaque forward-declared struct (defined minimally here: carries `ConnectionId` source + tick; **no simulation handle**). Step 4 registers control-message handlers as inert stubs (they parse nothing yet — behavior arrives in Steps 6–10); the registry mechanism is what's delivered.
- **`MessageRegistryService`.** `core::IService` (not `ITickable`). `Name() = "MessageRegistry"`; `Dependencies() = {}`; `Initialize` registers the seven control ids (`kMsgHello…kMsgBye`) with their (stub) handlers, returns `Expected`; `Shutdown` clears. Exposes `Registry()` accessor and a `Register(MessageId, IMessageHandler*)` passthrough for later sprints (data range).

## 4. Ownership & lifetime
`MessageRegistryService` owns the `MessageRegistry`; handlers are **non-owning** pointers (owned by whoever creates them — in Step 4, static/stub handlers). Registry outlives dispatch. No simulation ownership.

## 5. Dependencies
Steps 1–2. No transport/connection.

## 6. Constraints
Control range `[0x0000, 0x00FF]` only; data range `>= 0x0100` rejected in Sprint-006 registration (reserved for later sprints via the passthrough, which validates `IsDataId` when later used); deterministic id-ordered dispatch; ADR-007; engine-free.

## 7. Validation rules
- Duplicate `Register` of the same id → `AlreadyExists`.
- `Register` of an id outside `[kControlIdMin, kControlIdMax]` in Sprint-006's own control registration → `InvalidArgument` (later data-range registration uses `IsDataId`).
- `Dispatch` of an unregistered id → `Unknown` (benign, counted), never an error/throw.
- Accelerator consistent with the canonical vector (`ValidateConsistency`).

## 8. Failure behavior
`Expected`/`DispatchResult` values only; unknown ids tolerated; no exceptions.

## 9. Unit testing
- Register the seven control ids; `Count() == 7`; `Contains` each; iteration ascending by id.
- Dispatch to a registered id calls its handler exactly once with the payload reader.
- Dispatch to an unknown id → `Unknown`, counter increments, no crash.
- `ValidateConsistency` healthy; accelerator matches after registrations.

## 10. Negative testing
Duplicate registration → `AlreadyExists`; data-range id via control registration → `InvalidArgument`; dispatch unknown id → `Unknown`.

## 11. Boundary testing
Register id `0x0000` and `0x00FF` (range ends) succeed; `0x0100` rejected by control registration; empty registry dispatch → `Unknown`.

## 12. Definition of Done
- [ ] `MessageRegistry` + `IMessageHandler` + `MessageRegistryService` per §3; control ids registered; unknown-id tolerated; duplicate rejected.
- [ ] Deterministic id-ordered dispatch; sorted-vector + hash (BC-005); `ValidateConsistency`.
- [ ] Engine-free/OS-free; ADR-007/010 preserved; `IService` only (no tick).
- [ ] §9–§11 tests green (GCC + MSVC); full suite green; no regressions.
- [ ] No gameplay ids, no transport, no wiring; nothing from Steps 5–14.

## 13. Handoff notes (ChatGPT)
Pre-decided: `DispatchResult { Handled, Unknown, Failed }`; `DispatchContext` carries `ConnectionId` + `std::uint64_t tick` only (no simulation); control handlers are stubs returning `Success()` (real parsing is Steps 6–10). Do not add the connection table or transport. The `Register` passthrough is the seam later sprints use for data-range ids — implement it but exercise only control ids in Step 4.

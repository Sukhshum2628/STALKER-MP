# Sprint-006 · Step 11 — NetworkingService umbrella + TransportService + Diagnostics — Implementation Specification

- **Status:** Implementation specification (no code). Maps to frozen `Sprint-006-Architecture.md` (§1, §9, §10) and Implementation Plan Step 11. Consumes Steps 1–10. Engine-free / OS-free (mock/loopback transport).
- **Nature:** The module aggregation, the single tick, and diagnostics — still driven by a test transport. **No** Bootstrap wiring (Step 12), no real sockets (Step 13).
- **Constraints:** ADR-007/010/011; networking owns **no** simulation (const-only dependency edges = ordering); single `kNetworking` subscriber; One Engine/Platform Boundary.

## 1. Files to create
| File | Contents |
|---|---|
| `include/stalkermp/net/NetworkingService.h` + `src/net/NetworkingService.cpp` | Umbrella `core::IService` + `core::ITickable`, owns HostServer/ConnectionTable/queue store. |
| `include/stalkermp/net/TransportService.h` + `src/net/TransportService.cpp` | Thin `core::IService` owning the injected `ITransport`. |
| `include/stalkermp/net/NetworkDiagnostics.h` + `src/net/NetworkDiagnostics.cpp` | Read-only diagnostics + `NetworkConsistencyReport`. |

## 2. Files to modify
`xrMP.vcxproj` / `tests/xrMP_Tests.vcxproj` (add the `.cpp`s + headers); `tests/NetworkTests.cpp` (Step-11 lifecycle/tick/diagnostics tests).

## 3. Responsibilities
- **`NetworkingService`** (`IService` + `ITickable`). `Name() = "Networking"`; `Dependencies() = {"World", "EntityRegistry", "BubbleManager", "TransitionManager"}` (ordering only — **no** owned/const simulation handle stored). Owns the `HostServer`, `ConnectionTable`, `Session` (or references the `SessionService`'s), the Module-owned per-connection **queue store** (from Step 9), and holds a non-owning `ITransport&` (from `TransportService`) + non-owning `MessageRegistry&`. `Initialize()`: validate config, compute `msPerTick` (single conversion of ms timeouts → ticks), size the connection table + queue store to `maxConnections`, and **if `enabled`** call `HostServer::Start`; returns `Expected`. `Tick(deltaSeconds)`: advance a monotonic tick and run the host per-tick pass (Step 10) — the single deterministic network phase. `Shutdown() noexcept`: drain/close all connections, `HostServer::Stop`, release; idempotent.
- **`TransportService`** (`IService`, no tick). `Name() = "Transport"`; `Dependencies() = {}`. Owns the `ITransport` created via `adapters::CreatePlatformTransport(config)` (Step 13; loopback/mock in tests) and exposes it. Kept separate so tests inject a transport without building the whole module.
- **`NetworkDiagnostics`** (read-only, deterministic, iostream-free via `common::Format`). `Statistics()` (aggregate + per-connection traffic/state counters), `DescribeState()` (one-liner: listen state, connection count, session size, totals), `DescribeConnection(ConnectionId)`, `DumpConnections()` (ascending), rolling tick-windowed bandwidth, RTT (from Ping/Pong; diagnostic only), opt-in bounded packet trace (headers only, `kNetDebugPacketTrace`), and `ValidateConsistency() -> NetworkConsistencyReport` (table/session/queue-bound/no-orphan-endpoint invariants).

## 4. Ownership & lifetime
Single ownership tree (architecture §10): `TransportService` owns `ITransport`; `SessionService` owns `Session`; `MessageRegistryService` owns `MessageRegistry`; `NetworkingService` owns `HostServer` + `ConnectionTable` + queue store and drives the others via non-owning handles. The transport outlives the host that uses it (destroy order: host before transport). **No** networking object stores a simulation pointer/reference; the dependency list encodes ordering only.

## 5. Dependencies
Steps 1–10. In tests, inject `MockTransport`/`LoopbackTransport` directly (no `TransportService` platform factory needed until Step 12/13).

## 6. Constraints
One ticking service (`kNetworking`, subscribed in Step 12 — **not** here); no dispatcher subscription and no Bootstrap wiring in Step 11; no simulation ownership; single `msPerTick` conversion point; ADR-007/010/011; engine-free/OS-free.

## 7. Validation rules
- `Initialize` fails (`Expected`) on invalid config or (when enabled) bind failure; succeeds as a no-op tick when `!enabled`.
- `Dependencies()` returns exactly the four ordering names; no simulation handle is retained (structural review + no such member).
- `ValidateConsistency` healthy after any tick sequence.
- Diagnostics never mutate state (read-only).

## 8. Failure behavior
`Initialize` returns `Expected` error; `Shutdown` is noexcept and idempotent; a tick with no traffic is a deterministic no-op; diagnostics never throw.

## 9. Unit testing (mock/loopback)
- Construct with injected mock transport; `Initialize` (enabled + disabled paths); `Tick` runs the host pass; `Shutdown` idempotent (call twice).
- End-to-end via loopback: two module instances handshake, admit, ping, disconnect — through `NetworkingService::Tick` only.
- Diagnostics: `Statistics`/`DescribeState`/`DumpConnections` reflect a scripted session; `ValidateConsistency` healthy; packet trace bounded and headers-only.
- Monotonic tick advance; no simulation handle present (compile-time/structural check).

## 10. Negative testing
`Initialize` with invalid config → error; enabled + bind failure (mock returns error) → `Initialize` error; over-capacity handled by the host; diagnostics on empty module → zeroed, no crash.

## 11. Boundary testing
`enabled = false` → `Initialize` success, ticks are no-ops; exactly `maxConnections` sessions; bandwidth window rolls exactly per tick; trace ring buffer at capacity drops oldest.

## 12. Definition of Done
- [ ] `NetworkingService` (IService+ITickable, deps = four ordering names, no simulation handle) + `TransportService` + `NetworkDiagnostics`.
- [ ] Lifecycle (Initialize/Tick/Shutdown) + single-phase per-tick pass over mock/loopback; single `msPerTick` conversion.
- [ ] Diagnostics read-only + `ValidateConsistency`; **no** dispatcher subscription / Bootstrap wiring yet.
- [ ] Engine-free/OS-free; ADR-007/010/011; networking owns no simulation.
- [ ] §9–§11 tests green (GCC + MSVC); full suite green; no regressions.
- [ ] Nothing from Steps 12–14.

## 13. Handoff notes (ChatGPT)
Pre-decided: `NetworkingService::Tick` advances a `u64` tick and calls the Step-10 host pass; `msPerTick` is computed once in `Initialize` (e.g. from a fixed frame rate constant or `deltaSeconds` guard); the queue store is a Module-owned flat vector indexed by the `Connection` handles from Step 9; `TransportService` gets its transport from `adapters::CreatePlatformTransport(config)` in the real build (Step 13) and a test transport in tests. Do not subscribe to `FrameDispatcher` or touch `Bootstrap.cpp` — that is Step 12.

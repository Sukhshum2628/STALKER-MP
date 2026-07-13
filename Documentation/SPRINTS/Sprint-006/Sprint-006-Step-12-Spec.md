# Sprint-006 · Step 12 — tick_order::kNetworking + Bootstrap Composition-Root Wiring — Implementation Specification

- **Status:** Implementation specification (no code). Maps to frozen `Sprint-006-Architecture.md` (§7, §10) and Implementation Plan Step 12. Consumes Steps 1–11 (and Step 13's factory symbol, resolved via the null/loopback counterpart in the test build). Mirrors the Sprint-005 Bootstrap-wiring precedent.
- **Nature:** Integrate the proven module into the runtime: one additive `tick_order` constant + composition-root registration + reverse-order teardown. **No** module logic change, no real sockets (Step 13).
- **Constraints:** ADR-007/010/011; **networking-last invariant** (`kNetworking` is the highest key); One Engine/Platform Boundary; deterministic; reverse-order teardown; Runtime-owned transport (no deref at destruction — Sprint-005 gateway precedent).

## 1. Files to create
None.

## 2. Files to modify
| File | Why |
|---|---|
| `include/stalkermp/core/FrameDispatcher.h` | Add the **single** additive constant `tick_order::kNetworking` as the **highest** key (relational networking-last invariant; §7 of the architecture — no future producer values assigned). |
| `src/core/Bootstrap.cpp` | Register the four networking services after the Transition service; inject the transport via `adapters::CreatePlatformTransport(config)` (Runtime-owned; loopback/null in tests); subscribe `NetworkingService` at `kNetworking`; add reverse-order teardown. |
| `tests/BootstrapTests.cpp` | Subscriber count 4 → **5**; assert networking subscribed last (highest key); teardown assertions. |

## 3. Responsibilities
- **`FrameDispatcher.h`.** Add `inline constexpr std::uint32_t kNetworking = <highest>;` (a value strictly greater than every existing key; e.g. `900` to sit clearly above the current highest reserved slot, satisfying the invariant with headroom). One additive line; non-behavioral; the only touch to this frozen file.
- **`Bootstrap.cpp` — Runtime struct.** Add: `std::unique_ptr<net::ITransport> transport;` (Runtime-owned, engine/OS-free interface — real impl in engine build, loopback/null in tests); non-owning cache pointer `net::NetworkingService* networkingService = nullptr`. (The four services are owned by `ServiceRegistry`.)
- **`Bootstrap.cpp` — registration (after the Transition service).** Parse `NetworkingConfig`/`TransportConfig` from `serverConfig`; `runtime.transport = adapters::CreatePlatformTransport(transportConfig)`; register `TransportService` → `MessageRegistryService` → `SessionService` → `NetworkingService` (umbrella last), injecting non-owning handles + config + `runtime.transport.get()`; subscribe **only** `NetworkingService` to the dispatcher at `tick_order::kNetworking`; cache the pointer.
- **`Bootstrap.cpp` — teardown (`DestroyRuntime`).** Unsubscribe `networkingService` **before** `ShutdownAll` (and before the Transition unsubscribe — reverse of subscription order, since networking subscribed last); then `ShutdownAll`; the Runtime-owned `transport` is destroyed with the Runtime and never dereferenced at destruction (the Sprint-005 gateway precedent).

## 4. Ownership & lifetime
`ServiceRegistry` owns the four services; `Runtime` owns the `ITransport` (declared so it outlives the services that reference it, and released with the Runtime). `NetworkingService` holds non-owning handles to transport/session/registry. Reverse-order teardown: frame bridge → networking unsubscribe → (existing) transition/bubble/... unsubscribes → `ShutdownAll` → member destruction (transport destroyed without being dereferenced by any service destructor).

## 5. Dependencies
Steps 1–11 (module) + Step 13's `adapters::CreatePlatformTransport` (test build links the null/loopback counterpart, so Step 12 compiles and runs without the real socket TU).

## 6. Constraints
One additive constant (highest key, no future producer values); the only frozen-file touches are `FrameDispatcher.h` (constant) and `Bootstrap.cpp` (wiring) + the `BootstrapTests.cpp` assertion; no module logic change; deterministic; ADR-007/010/011.

## 7. Validation rules
- `kNetworking` is strictly greater than every other `tick_order` key (compile-time assertable).
- Exactly one networking dispatcher subscription (the umbrella); the other three services do not tick.
- Teardown unsubscribes networking before `ShutdownAll`; no dangling subscription; transport not dereferenced at destruction.
- Bootstrap Initialize succeeds with `enabled = false` (default) — networking wired but idle.

## 8. Failure behavior
Config parse failure or (when enabled) bind failure propagates as `Bootstrap` Initialize `Expected` failure (existing pattern); partial-init teardown still completes; `Shutdown` idempotent.

## 9. Unit / integration testing (`BootstrapTests.cpp`)
- After `Initialize`, `SubscriberCount() == 5`; networking is the highest-order subscriber; frames dispatch safely (with the null/loopback transport, networking tick is a deterministic no-op when disabled).
- `Shutdown` reverses cleanly; `GetModuleFrameDispatcher() == nullptr` after.
- Static/compile assertion: `kNetworking > kPlugins` (and > all others).

## 10. Negative testing
Invalid `[networking]`/`[transport]` config → `Initialize` fails; enabled + bind failure (test transport error) → `Initialize` fails; double `Shutdown` → no crash.

## 11. Boundary testing
`enabled = false` default → 5 subscribers, networking ticks no-op; `enabled = true` with loopback → handshake path reachable through real frame dispatch.

## 12. Definition of Done
- [ ] `tick_order::kNetworking` added as the single additive highest key (no future producer values).
- [ ] Four services registered after Transition; transport Runtime-owned via factory; only `NetworkingService` subscribed at `kNetworking`.
- [ ] Reverse-order teardown; transport not dereferenced at destruction; `BootstrapTests` shows 5 subscribers, networking last.
- [ ] MSVC Release clean; engine boundary intact; test build links null/loopback transport (no real socket TU).
- [ ] `BootstrapTests` + full suite green (GCC + MSVC); no regressions.
- [ ] No module logic change; nothing from Steps 13–14 beyond the factory symbol.

## 13. Handoff notes (ChatGPT)
Pre-decided: `kNetworking` value is the highest key (recommend `900`, leaving `800` band free — the exact literal is implementation-local as long as it is strictly greatest); registration order Transport → MessageRegistry → Session → Networking; only the umbrella subscribes; the `CreatePlatformTransport` factory is declared in `include/stalkermp/adapters/PlatformTransport.h` (Step 13) with the null/loopback definition linked in the test build. Do not implement the real UDP transport here (Step 13). Mirror the Sprint-005 gateway teardown discipline exactly.

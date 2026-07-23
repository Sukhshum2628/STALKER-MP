# Lua Integration (Sprint-013)

A controlled, host-side scripting layer through which Lua scripts observe the
authoritative world and effect change only via sanctioned subsystem seams. The core is
engine-free, VM-free, and OS-free: the concrete Lua runtime lives only in
`EngineAdapters.cpp`, script-file I/O only in `PlatformScriptStore.cpp`, and every
fallible operation is a value outcome (`ScriptOutcome`). The engine remains
authoritative; scripts execute through documented APIs only, on the Simulation Thread
only.

## Boundary decisions

- **D-LUA1 — the Lua VM lives behind an engine-free runtime seam (ADR-009).** The core
  talks only to `lua::ILuaRuntime` with value-only arguments; the concrete binding
  reuses the X-Ray engine's existing Lua runtime and is confined to `EngineAdapters.cpp`.
  Tests use a fake/null runtime, so the whole core is unit-testable without a VM.
- **D-LUA2 — the Public API is an engine-free facade; no internal engine object is
  exposed (scope §7.4 + ADR-008).** The seven facades (`IWorldApi`, `IEnvironmentApi`,
  `IEntityApi`, `IPlayerApi`, `IInventoryApi`, `ILoggingApi`, `IConfigApi`) exchange
  value handles/ids/structs only; the real implementations in `EngineAdapters.cpp` route
  reads through Sprint-002/Sprint-008 seams and authoritative effects through Sprint-003
  (Entity Registry), Sprint-005 (`IAlifeSwitchGateway`), and Sprint-007 (player/spawn)
  seams.
- **D-LUA3 — single-thread, no-thread execution (ADR-011).** Scripts run on the
  Simulation Thread only; worker threads never invoke scripts; snapshots remain the only
  worker channel; the module creates no thread. The `ScriptManager` ticks at the reserved
  `tick_order::kScripting = 375` (Gameplay phase: after `kAlifeTransition = 350`, before
  the Snapshot producer `kReplication = 400`), so authoritative script effects are
  captured in the same-frame snapshot.
- **D-LUA4 — script-file I/O behind the platform boundary (ADR-009).** Discovery/reading
  go through the engine-free `IScriptSource`; the real filesystem source
  (`PlatformScriptStore`) is the only script-side OS/file TU. Tests use the in-memory
  source.
- **D-LUA5 — scripts never own state or events; faults are isolated (scope §7.5/§7.9 +
  ADR-007).** Scripts register callbacks and react to host-dispatched events; every fault
  (runtime error, invalid API use, missing script, recursion limit, execution timeout,
  crash) disables the offending script while the engine continues; no C++ exception
  escapes the runtime seam.

## Components

- `LuaScriptTypes` — value vocabulary (`ScriptOutcome`, `ScriptState`, `ScriptEvent`,
  ids, `ScriptDescriptor`, statistics).
- `LuaConfiguration` — the `[lua]` config (opaque script directory token, max scripts,
  execution budget, recursion limit, enable/validation flags, version).
- `ILuaRuntime` (+ fake/null) — the engine-free VM seam.
- `ScriptRegistry` / `ScriptContext` — deterministic registry of loaded scripts.
- `EventBinding` — engine-free event→callback table (deterministic dispatch order).
- `ScriptValidator` — syntax (via the runtime seam), API compatibility, version,
  duplicate, callbacks, dependencies.
- `IScriptSource` (+ in-memory/null) / `PlatformScriptStore` — the read seam and its
  filesystem backend.
- Public API facades (`ScriptApi.h`) — the seven engine-free API seams.
- `ScriptLifecycle` — Load → Initialize → Execute → Suspend → Resume → Unload → Destroy.
- `ScriptLoader` — discover → read → validate → runtime-load → register → lifecycle.
- `LuaManager` — runtime init/shutdown + Public API registration.
- `ScriptManager` — orchestration + event/OnTick dispatch + fault isolation (`IService` +
  `ITickable` at `kScripting`).
- `LuaDiagnostics` — non-invasive collector (counters, diagnostic timing/memory, bounded
  timeline/console).
- `ScriptThreadGuard` — deterministic Simulation-Thread-only enforcement.

## Lifecycle

Each script advances deterministically: `Load → Initialize` at load time; `Execute`,
`Suspend`/`Resume` during operation; `Unload`/`Destroy` at teardown. Illegal transitions
are rejected as value outcomes and leave the state unchanged.

## Events (supported callbacks)

`OnServerStart`, `OnServerStop`, `OnPlayerJoin`, `OnPlayerLeave`, `OnPlayerDeath`,
`OnEntitySpawn`, `OnEntityDestroy`, `OnWorldLoaded`, `OnWorldSaved`, `OnTick`. The engine
publishes events; scripts subscribe. Scripts never own, emit, or reorder events. Dispatch
order is deterministic (ascending by script id, then callback id). `OnTick` is dispatched
once per frame from the `kScripting` slot with the deterministic tick counter (not
wall-clock).

## Thread rules

Script entry points run on the Simulation Thread only. Worker threads (replication,
persistence, future workers) never invoke gameplay scripts and communicate only through
immutable snapshots (`09_Threading.md`, `RFC-0006`). The `ScriptThreadGuard` verifies
every entry against the bound simulation-thread token; a non-owner entry is a counted
value outcome. No thread is created by the scripting layer.

## Composition & startup

`Bootstrap` binds the build-selected runtime (`CreateEngineLuaRuntime`), Public API
bundle (`CreateEngineScriptApi`), and script source (`CreateEngineScriptSource`) behind
engine-free factories, constructs `LuaManager`/`ScriptManager`, registers `ScriptManager`
as an `IService`, and subscribes it at `kScripting = 375`. During `Initialize` — before
networking accepts connections — it binds the thread guard, calls `LuaManager.Init`
(create state, load libraries, register the API facades), then `ScriptManager.LoadAll`. A
runtime-init or load failure is non-fatal: scripting is left disabled and the engine
continues. Teardown is reverse-order: the `ScriptManager` is unsubscribed and the runtime
state destroyed before `ShutdownAll`; the Runtime-owned seams outlive it.

## Best practices

Keep scripts to gameplay logic (quests, dialogue, missions, scripted events). Reach
subsystems only through the documented Public API — never assume direct engine access.
Do not rely on execution timing or table iteration order for authoritative results
(determinism is a host guarantee). Expect faults to disable only the offending script;
design callbacks to be independently recoverable.

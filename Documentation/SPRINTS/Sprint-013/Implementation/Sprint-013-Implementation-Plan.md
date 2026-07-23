# Sprint-013 — Lua Integration · Architecture & Implementation Plan

| Field | Value |
|---|---|
| Sprint | 013 — Lua Integration |
| Status | **EXECUTED & CLOSED** (2026-07-20) — all 18 steps implemented across 11 batches, Antigravity-verified; 736/736 tests passing |
| Scope authority | `Documentation/SPRINTS/Sprint-013/Sprint-013-Lua-Integration.md` (approved) |
| Baseline | Sprint-012 CLOSED & FROZEN — **675 / 675** tests passing (GCC + MSVC) |
| Governing ADRs | ADR-007, ADR-008, ADR-009, ADR-010, ADR-011 (all preserved; none modified) |
| Architecture refs | `10_Extensibility.md`, `03_Player.md`, `04_World.md`; RFC-0007, RFC-0006, RFC-0001 |
| Reuses (unchanged) | Sprint-002 `IWorldQueries`/`ISpatialQueries`/`EnvironmentService`; Sprint-003 Entity Registry; Sprint-005 `IAlifeSwitchGateway`; Sprint-007 player/spawn seams; Sprint-008 `ISnapshotView`; Sprint-012 `ISaveSource` read-seam pattern (mirrored, not modified); core `FrameDispatcher`, `ServiceRegistry`, `Expected`, `ConfigStore`, `Log` |
| Steps | 18 (Step-01 … Step-18), strict additive dependency chain |

---

## Status lifecycle

This plan moves through three distinct states. It is currently at **③ Frozen** (2026-07-20).

- **① DRAFT** — content under construction; may contain open `[AR-n]` items, TODOs, or unresolved architecture. *(Passed: all §0.B items were resolved and the final documentation audit passed.)*
- **② FREEZE REVIEW** — content complete, internally consistent, fully traceable, with no open architectural items, TODOs, or Approval-Required markers. *(Passed: the plan cleared freeze review.)*
- **③ FROZEN → EXECUTED & CLOSED (2026-07-20).** The plan was frozen, then all 11 batches (Steps 01–18) were implemented and independently Antigravity-verified; the step scope, dependency chain, and batching are immutable. Sprint-013 is now **COMPLETED** — see the completion summary below.

## Completion summary (2026-07-20)

**Sprint-013 (Lua Integration) is COMPLETED — Implemented / Verified / Closed / Frozen.**

- **Batches:** B1 (01–02) · B2 (03–04) · B3 (05–06) · B4 (07) · B5 (08) · B6 (09) · B7 (10–11) · B8 (12–14) · B9 (15–16) · B10 (17) · B11 (18). Every batch independently verified; the load-bearing gates (Step-08 platform boundary / ADR-009, Step-09 engine API boundary / ADR-008, Step-17 integration) verified in isolation.
- **Final passing test count:** **736 / 736** (Release x64, GCC + MSVC) — Sprint-012 baseline 675 + 61 Sprint-013 tests; 0 warnings, no regressions.
- **Boundaries:** engine code confined to `EngineAdapters.cpp`; script-file I/O confined to `PlatformScriptStore.cpp`; exactly one new `tick_order` key (`kScripting = 375`, Gameplay phase); no thread created; wire protocol untouched; ADR-007…ADR-011 preserved; E-G1-LU…E-G5-LU PASSED.
- **Deliverable doc:** `Multiplayer/docs/LuaScripting.md`. Engine/VM/filesystem build + smoke Antigravity-verified on Windows.
- **Readiness:** the project is ready for **Sprint-014 (Plugin Framework)**.

---

## 0. Governance & preserved invariants (apply to every step)

This plan is **purely additive** and modifies no prior sprint's frozen documents, code, or ADRs. Every step preserves, without exception:

- **Preserve Before Replace** — Sprint-013 REUSES the existing subsystem seams unchanged (Sprint-002 world/environment read seams, Sprint-003 Entity Registry, Sprint-005 `IAlifeSwitchGateway`, Sprint-007 player/spawn seams, Sprint-008 `ISnapshotView`) and mirrors the Sprint-012 `ISaveSource` read-seam pattern for script sources. It adds a new engine-free `lua::` scripting layer **additively**; it reimplements none of the world, ALife, snapshot, replication, persistence, or save/load logic.
- **Host Authority** — scripting is host-side. Scripts react to host-dispatched events and may effect authoritative change ONLY through sanctioned subsystem seams; they never own authoritative state, never create a second world/simulation, and are never authoritative themselves. Clients do not run gameplay scripts (multiplayer scripting is out of scope, §3).
- **One World / One ALife Simulation** — the scripting layer holds no world/simulation state; it observes the single authoritative world through existing read seams and effects change through existing write seams.
- **Deterministic Simulation & Replay Determinism** — per `[AR-3]` (resolved, §0.B), authoritative script execution is gameplay on the single Simulation Thread and is bound by the project determinism mandate: identical authoritative results regardless of execution timing, and identical authoritative ordering independent of OS scheduling and worker timing (`09_Threading.md` §3; `RFC-0006` §9/§17). VM-internal timing (GC, memory) is timing and is outside authoritative results (diagnostic-only). The runtime mechanism that satisfies this is implementation (Step-17), not decided here.
- **Clients Never Own Persistent/Authoritative World State** — only the host executes gameplay scripts and only through sanctioned seams; no authoritative state originates on clients.
- **Existing Engine Ownership / Existing ALife Ownership** — the scripting Public API applies authoritative effects ONLY through the sanctioned engine seams (see D-LUA2); the engine-free `lua::` core owns no engine state and includes no engine headers. No internal engine object is ever exposed to Lua.
- **Singular Engine Boundary** — engine headers and the concrete engine-facade implementations remain confined to `EngineAdapters.cpp`; the API facades are engine-free interfaces in the core.
- **Singular Platform Boundary** — the concrete Lua VM binding **and** all script-file/OS access are each confined to ONE boundary translation unit behind engine-free seams (see D-LUA1 / D-LUA4); the core is VM-free and OS/file-free.
- **ADR-007** — no exceptions, no RTTI, no iostream in the engine-free core; all fallible operations return `core::Expected<T>` / `ScriptOutcome` value outcomes; a script fault never propagates as a C++ exception into the engine; bounded memory.
- **ADR-008** — authoritative effects from scripts are applied only through the sanctioned subsystem seams (D-LUA2); the engine-free core never mutates authoritative simulation directly.
- **ADR-009** — the platform boundary is preserved: VM and filesystem code live only in their boundary TUs behind engine-free seams (D-LUA1 / D-LUA4).
- **ADR-010** — the wire protocol is untouched; scripting is host-local and sends nothing.
- **ADR-011** — the multiplayer module creates **no thread**; the VM runs synchronously on the Simulation Thread only; worker threads never invoke scripts; snapshots remain the only worker communication channel (scope §7.8). Per `[AR-1]` (resolved, §0.B), the per-frame `OnTick` runs in the authoritative Gameplay phase — after ALife, before Snapshot Generation — via one new `tick_order` key allocated between `kAlifeTransition` (350) and `kReplication` (400) (`09_Threading.md` §8.2/§8.3; `FrameDispatcher.h` registry convention).

**Compatibility.** Every step is additive and leaves the Sprint-012 **675/675** baseline intact; new tests only add to the count. No prior public API changes.

---

## 0.A ADR-derived boundary decisions (supported by the frozen ADRs)

These decisions follow directly from the frozen ADRs applied to the scope's components; they invent no architecture beyond ADR-007…ADR-011 and the scope. Points the scope/ADRs do **not** settle are deferred to §0.B (`[AR-n]`), not decided here.

- **D-LUA1 — The Lua VM lives behind an engine-free runtime seam (ADR-009, Singular Platform Boundary).** ADR-009 requires third-party/OS/engine-linked code to sit behind an engine-free seam confined to one boundary TU. Accordingly the `lua::` core defines an engine-free `ILuaRuntime` seam (create/destroy state, load standard libraries, load+execute a named chunk, register a host function, invoke a registered callback) returning value outcomes only (ADR-007); the concrete VM binding is confined to ONE boundary TU behind an engine-free factory (mirroring `PlatformSockets.cpp` / `PlatformSaveStore.cpp`); test/default builds use a fake/in-memory runtime. Per `[AR-2]` (resolved, §0.B), the concrete binding **reuses the X-Ray engine's existing Lua runtime** (not a separately-embedded VM), with version validation against the engine's Lua/API version — authority `10_Extensibility.md` §14.1 ("preserves this capability") and §16 (API versioning).
- **D-LUA2 — The Public API is an engine-free facade; no internal engine object is exposed (scope §7.4 + ADR-008).** The scope mandates "No internal engine objects exposed"; ADR-008 requires authoritative mutation to cross only sanctioned seams. Accordingly the script-callable API surface (World, Player, Entity, Environment, Inventory, Logging, Configuration) is a set of engine-free facade interfaces in the core exchanging value handles/ids and value structs only, with the real engine-touching implementations confined to `EngineAdapters.cpp` and recording/fake facades in tests. Per `[AR-4]` (resolved, §0.B), each category is implemented over its corresponding existing sanctioned seam — World/Environment via Sprint-002 `IWorldQueries`/`ISpatialQueries`/`EnvironmentService` + Sprint-008 `ISnapshotView`; Entity via Sprint-003 Entity Registry; ALife via Sprint-005 `IAlifeSwitchGateway`; Player/spawn via Sprint-007 seams — the forced consequence of ADR-008 (no other sanctioned path) and `10_Extensibility.md` §12.2/§12.4/§14.4; no new seam.
- **D-LUA3 — Single-thread, no-thread execution + Gameplay-phase placement (ADR-011 + scope §7.8; `09_Threading.md` §8; `10_Extensibility.md` §13.5).** Scripts execute on the **Simulation Thread only**; worker threads never invoke scripts; snapshots remain the only worker channel; the multiplayer module creates no thread. Per `[AR-1]` (resolved, §0.B) the `ScriptManager` runs its `OnTick` in the authoritative **Gameplay phase** — after ALife, before Snapshot Generation — via one new `tick_order` key allocated between `kAlifeTransition` (350) and `kReplication` (400), so script effects are captured in the same-frame snapshot. Per `[AR-3]` (resolved, §0.B), authoritative script results/ordering must be identical regardless of execution timing and OS scheduling (`09_Threading.md` §3; `RFC-0006` §17); the runtime mechanism is implementation, not decided here.
- **D-LUA4 — Script source I/O is confined behind the platform boundary (ADR-009).** ADR-009 requires OS/file access behind an engine-free seam in one TU. Accordingly script discovery and reading go through an additive engine-free `IScriptSource` read seam (enumerate / read-bytes / exists), mirroring the Sprint-012 `ISaveSource`; the real filesystem backend is confined to ONE platform TU (`PlatformScriptStore.cpp`) behind engine-free factories; test/default builds use in-memory/null sources; the core performs no path resolution (the scope's "script search paths", §7.3, are resolved only in that TU).
- **D-LUA5 — Scripts never own authoritative state or events; faults are isolated (scope §7.5/§7.9 + ADR-007).** The scope states scripts "react to events / do not own events" and that on error "the engine continues running". Accordingly scripts register callbacks and react to host-dispatched events (they do not own, emit, or reorder events), and every script fault named in §7.9 (runtime error, invalid API use, missing script, infinite recursion, execution-time overrun, crash) is caught at the runtime boundary and surfaced as a `ScriptOutcome` value outcome that isolates the offending script while the engine continues; no script fault becomes a C++ exception in the engine (ADR-007).

---

## 0.B Approval register — resolved against frozen documentation

The five items raised in review were checked against the frozen **architecture documents** (`09_Threading.md`, `10_Extensibility.md`), the RFCs (`RFC-0006`), the ADRs, and the closed Sprint-001…Sprint-012 record. **All five are resolved by existing frozen documentation** and now carry their documented decision (with authority cited); each was a deduction forced by the cited documents rather than a new architectural choice. Inline `[AR-n]` tags elsewhere in this plan carry the decision recorded here. **No architectural item remains open;** this plan is **FROZEN** (2026-07-20).

| Ref | Status | Decision (if resolved) | Authority |
|---|---|---|---|
| **[AR-1]** — per-frame execution model & tick placement | **Resolved from documentation** | Script `OnTick` runs in the authoritative **Gameplay phase** on the **Simulation Thread**, **after ALife and before Snapshot Generation**, so effects are captured in the **same-frame** snapshot. Realized as one new `tick_order` key allocated in the open interval between `kAlifeTransition` (350) and `kReplication` (400) per the registry convention; the exact integer is a mechanical allocation at the wiring step, not an architectural choice. | `09_Threading.md` §8.2 (Simulation Thread executes "Gameplay"), §8.3 (Update Pipeline: World → Bubble → ALife → **Gameplay** → Physics → Commit → **Snapshot Generation**; "the snapshot marks the synchronization boundary"); `10_Extensibility.md` §14.3 (Lua = gameplay logic), §13.5 ("Events should reflect the order of authoritative simulation… never receive events describing gameplay that has not yet occurred"), §13.6 (deterministic ordering); `FrameDispatcher.h` `tick_order` comment ("New slots are allocated only here… with the sprint introducing them"). |
| **[AR-2]** — VM implementation & version-validation target | **Resolved from documentation** | **Reuse the X-Ray engine's existing Lua runtime** (do not embed a separate or alternative VM); "version validation" targets the engine's Lua/API version under the API-versioning model. | `10_Extensibility.md` §14.1 ("The X-Ray Engine already makes extensive use of Lua. STALKER-MP **preserves this capability**…"); §16 (API Versioning: version metadata, compatibility levels). |
| **[AR-3]** — determinism requirement for scripting | **Resolved from documentation** | Script execution that produces authoritative effects is gameplay on the single Simulation Thread ([AR-1]) and is therefore bound by the project's determinism mandate: it **must produce identical authoritative results regardless of execution timing and identical authoritative ordering independent of operating-system scheduling and worker timing**; authoritative state crosses thread boundaries only through the immutable snapshot. VM-internal timing (GC, memory) is timing and is therefore, like all timing, outside authoritative results (diagnostic-only). *The mechanism by which the runtime binding satisfies this requirement is implementation (Step-17), not an architectural decision, and is not specified here.* | `09_Threading.md` §3 ("Deterministic Simulation": "identical results regardless of execution timing"; "Deterministic Ordering": "Simulation ordering must remain independent of operating system scheduling"), §12.2/§12.5 (deterministic simulation order; immutable cross-thread data; snapshot as the sole authoritative handoff); `RFC-0006` §6/§9 (only the Simulation Thread executes gameplay; "Execution order is therefore deterministic"), §17 ("simulation remains deterministic regardless of worker timing"); `10_Extensibility.md` §13.5/§13.6 (deterministic event ordering), §14.3 (Lua = gameplay); ADR-011 (single-thread deterministic model). |
| **[AR-4]** — Public API → sanctioned-seam mapping | **Resolved from documentation** | Each Public API category is implemented over the corresponding **existing sanctioned seam** (the forced consequence of ADR-008 + the seam inventory): World/Environment reads via Sprint-002 `IWorldQueries`/`ISpatialQueries`/`EnvironmentService` and Sprint-008 `ISnapshotView`; Entity via Sprint-003 Entity Registry; ALife via Sprint-005 `IAlifeSwitchGateway`; Player/spawn via Sprint-007 seams. Read vs controlled-write per the interface contract; no internal engine structure exposed; no new seam. | `10_Extensibility.md` §12.2 (interface categories), §12.4 (read vs controlled write), §9 (Script Bridge "prevents scripts from bypassing engine ownership rules"), §14.4 (Lua → Script Bridge → Public API → Engine); **ADR-008** (authoritative effects only through sanctioned seams — no alternative path exists); the owning sprints (S-002/003/005/007/008, all closed). |
| **[AR-5]** — startup timing of runtime init / `LoadAll` | **Resolved from documentation** | Runtime init + script `LoadAll` run during `Bootstrap Initialize`, **before networking accepts connections** (networking-last invariant); intra-Initialize position follows dependency order (after the subsystems the Public API binds to are constructed) — standard composition-root sequencing. | `FrameDispatcher.h` `tick_order` (`kNetworking = 900`, "networking-last invariant"); Sprint-012 Implementation Plan (recovery/init "before networking"; "simulation always starts before networking") as applied precedent; `10_Extensibility.md` §4 (extension layer sits above core runtime, consumes engine interfaces). |

**All five items are resolved from frozen documentation; none remains outstanding.** No new architecture is introduced above: `[AR-1]`, `[AR-4]`, and `[AR-5]` are deductions forced by the cited frozen documents and ADR-008; `[AR-2]` is the explicit "preserve existing Lua" decision in `10_Extensibility.md` §14.1; `[AR-3]` restates the existing universal determinism mandate as it applies to scripting (the runtime mechanism is left to implementation, not decided here).

---

## 1. Sprint objective

Implement the **Lua Integration Layer**: embed a controlled Lua runtime, expose a documented, engine-free Public API through which host-side scripts observe the authoritative world and effect change ONLY via sanctioned subsystem seams, dispatch a fixed set of gameplay events to registered script callbacks, and manage the full deterministic script lifecycle — with validation, diagnostics, and fault isolation such that a misbehaving script never destabilizes the engine. The engine remains authoritative; scripts execute through documented APIs only, on the Simulation Thread only.

## 2. Scope

**In scope.** A new engine-free `lua::` namespace: value types & vocabulary; `LuaConfiguration`; the engine-free `ILuaRuntime` VM seam (+ fake/null runtime); `ScriptContext` + `ScriptRegistry`; the event-binding registry; script validation; the additive `IScriptSource` read seam; the platform script-source backend confined to one platform TU; the engine-free Public API facade seams (World/Player/Entity/Environment/Inventory/Logging/Config); the script lifecycle state machine; `ScriptLoader`; `LuaManager`; `ScriptManager`; error handling / fault isolation; diagnostics + statistics + performance instrumentation; thread-safety enforcement; composition-root + engine-adapter wiring (real runtime (`[AR-2]`) + engine facades, `ScriptManager` per the `[AR-1]` execution model); integration tests; and the subsystem doc `Multiplayer/docs/LuaScripting.md`.

## 3. Out-of-scope items

Plugin system (Sprint-014), hot reload, sandbox security hardening, multiplayer/client-side scripting, mod packaging, marketplace (§4 of the scope authority); any modification of prior frozen seams, the vanilla X-Ray Lua/gameplay scripts, or the wire protocol; any `tick_order` change beyond the single Gameplay-phase key established by `[AR-1]` (resolved, §0.B); any thread creation.

## 4. Design goals

Controlled, documented API access with no internal engine object exposed (scope §7.4); deterministic, single-thread script execution (scope §7.6/§7.8; the frame-capture model is `[AR-1]`); complete fault isolation (a script error/recursion/timeout/crash never destabilizes the engine, scope §7.9); reuse of existing subsystem seams without redesign; an engine-free, VM-free, platform-free, unit-testable core with the concrete VM confined to one boundary TU, engine facades confined to `EngineAdapters.cpp`, and script-file I/O confined to one platform TU; zero impact on the Sprint-012 baseline.

## 5. Architectural overview

Scripting is a **host-side** subsystem with three flows:

```
STARTUP (Bootstrap Initialize, before networking — [AR-5] resolved):
  LuaManager.Init → ILuaRuntime.CreateState + LoadLibraries + register Public API facades
  ScriptManager.LoadAll ← IScriptSource.enumerate/read → validate → lifecycle: Load → Initialize

PER FRAME (Gameplay phase: after ALife, before Snapshot Generation — [AR-1] resolved):
  ScriptManager dispatch OnTick to registered callbacks (Simulation Thread only)
     → script API calls read/effect change ONLY through sanctioned subsystem seams ([AR-4] resolved)
     → authoritative effects captured in the same-frame snapshot ([AR-1] resolved)

EVENTS (host-dispatched, synchronous):
  subsystem raises OnPlayerJoin / OnEntitySpawn / OnWorldSaved / … → EventBinding → registered callbacks
     → faults isolated as value outcomes (offending script disabled; engine continues)
```

Scripts never own events or authoritative state; they react through documented callbacks and the Public API (scope §7.4/§7.5). Loading/registration happen once at startup (before networking, `[AR-5]` resolved); the per-frame `OnTick` runs in the Gameplay phase — after ALife, before Snapshot Generation (`[AR-1]` resolved, §0.B).

## 6. Component responsibilities

| Component | Responsibility |
| --- | --- |
| `lua::LuaScriptTypes` | Value vocabulary: `ScriptOutcome`, `ScriptId`/`CallbackId`, lifecycle-state & event-type enums, `ScriptDescriptor`, value API-argument/result structs, `ScriptStatistics`, `ScriptConsistency`. |
| `lua::LuaConfiguration` (+ `FromConfig`) | `[lua]` config: script directory token (opaque), max scripts, execution budget, recursion limit, enable/validation/API-version flags, version. |
| `lua::ILuaRuntime` (+ fake/null) | Engine-free VM seam: create/destroy state, load libraries, load+execute chunk, register host function, invoke callback — value outcomes. Concrete VM binding (implementation `[AR-2]`) confined to a boundary TU (D-LUA1). |
| `lua::ScriptContext` + `ScriptRegistry` | Per-script value context; deterministic registry (ascending ids, duplicate detection, enumerate). |
| `lua::EventBinding` | Engine-free event→callback registry: register/unregister; deterministic dispatch order; scripts react, never own (D-LUA5). |
| `lua::ScriptValidator` | Syntax (via runtime seam), API compatibility, version, duplicate registration, invalid callbacks, missing dependencies → value outcomes. |
| `lua::IScriptSource` (+ null/in-memory) | Additive engine-free read seam (enumerate / read / exists), mirroring Sprint-012 `ISaveSource` (D-LUA4). |
| `adapters::PlatformScriptStore` (platform TU) | Real filesystem script source over the configured directory token. The only script-side OS/file code (ADR-009). Null in tests. |
| `lua::` Public API facades | `IWorldApi`/`IPlayerApi`/`IEntityApi`/`IEnvironmentApi`/`IInventoryApi`/`ILoggingApi`/`IConfigApi` — engine-free facades over the sanctioned seams; engine impls in `EngineAdapters.cpp` (D-LUA2). |
| `lua::ScriptLifecycle` | Deterministic state machine: Load → Initialize → Execute → Suspend → Resume → Unload → Destroy. |
| `lua::ScriptLoader` | Compose `IScriptSource` + runtime load + validation in deterministic order. |
| `lua::LuaManager` | Runtime init/shutdown, library loading, Public API registration; owns the `ILuaRuntime`. |
| `lua::ScriptManager` | Load/unload/reload, execution tracking, failure isolation; `IService` + `ITickable` whose `OnTick` runs in the Gameplay phase (new `tick_order` key between 350 and 400) per `[AR-1]` (resolved, §0.B). |
| `lua::LuaDiagnostics` | Non-invasive collector + inspectors (script inspector, execution timeline, loaded scripts, API browser, callback viewer, console; statistics; timing — diagnostic-only). |
| `adapters` wiring | Composition-root wiring in `Bootstrap.cpp` (real runtime + engine facades, `ScriptManager` subscription per `[AR-1]`, reverse-order teardown); default `[lua]` config block. |

## 7. Frame/startup placement (resolved from frozen documentation — §0.B)

- **Thread & no-thread (ADR-011 + scope §7.8):** scripts run on the Simulation Thread only; worker threads never invoke scripts; the multiplayer module creates no thread; networking-last (`kNetworking = 900`) is preserved.
- **Per-frame `OnTick` — `[AR-1]` (resolved):** runs in the authoritative **Gameplay phase** — after ALife, before Snapshot Generation — via one new `tick_order` key allocated between `kAlifeTransition` (350) and `kReplication` (400), so script effects are captured in the same-frame snapshot. Authority: `09_Threading.md` §8.2/§8.3 (Update Pipeline places Gameplay after ALife, before Snapshot Generation); `10_Extensibility.md` §14.3/§13.5; `FrameDispatcher.h` `tick_order` allocation convention. The exact integer is a mechanical allocation at the wiring step (Step-17).
- **Startup — `[AR-5]` (resolved):** `LuaManager.Init` + `ScriptManager.LoadAll` run during `Bootstrap Initialize`, **before networking accepts connections** (networking-last invariant; Sprint-012 "simulation always starts before networking" precedent), after the subsystems the Public API binds to are constructed.

## 8. Boundaries

- **Singular Engine Boundary** — the concrete Public-API facade implementations (and any engine reads/writes) are added only in `EngineAdapters.cpp`, behind the engine-free facade seams (D-LUA2). No engine header enters the `lua::` core; no engine object is exposed to Lua.
- **Singular Platform Boundary** — the concrete Lua VM binding is confined to one boundary TU behind `ILuaRuntime` (D-LUA1); all script-file I/O is confined to `PlatformScriptStore.cpp` behind `IScriptSource` factories (D-LUA4). No VM or OS/file header enters the core.

## 9. Testing strategy

`Multiplayer/tests/LuaScriptingTests.cpp` accumulates per-step unit tests (config, types, registry, event binding, validation, lifecycle, loader, manager orchestration through the **fake runtime** + null/in-memory sources and recording API facades) plus a `LuaScriptingIntegrationStep17` composed suite (server-lifecycle events, player/world/entity events, save/load callbacks, large script collections, fault isolation) and Bootstrap wiring/ordering assertions (`ScriptManager` subscribed once in the Gameplay-phase slot — after `kAlifeTransition` 350, before `kReplication` 400 — per `[AR-1]`; networking-last preserved). GCC engine-free build + MSVC. Every fault is a value outcome that isolates the script and leaves the engine running; determinism verified by twin-instance callback-order identity. The real Lua runtime (`[AR-2]`), engine facades, and filesystem script source are Antigravity-smoke-verified on Windows.

## 10. Evidence gates (must all pass by close)

- **E-G1-LU** — controlled API: no internal engine object exposed to Lua (scope §7.4); every script effect crosses into subsystems only through sanctioned seams (ADR-008). *The specific seam mapping verified is `[AR-4]`.*
- **E-G2-LU** — determinism & single-thread: identical authoritative results/ordering regardless of execution timing and OS scheduling (`[AR-3]` resolved; `09_Threading.md` §3, `RFC-0006` §17); deterministic script/callback ordering; Simulation-Thread-only execution; no thread created (ADR-011 / scope §7.8). *Frame slot / same-frame capture is `[AR-1]`.*
- **E-G3-LU** — fault isolation: runtime error / invalid API / missing script / infinite recursion / execution timeout / crash are value outcomes that isolate the script; the engine continues; no C++ exception escapes the runtime boundary (ADR-007 / scope §7.9 / D-LUA5).
- **E-G4-LU** — Singular Platform Boundary: the concrete VM binding and all script-file I/O are confined to their boundary TUs behind engine-free seams (ADR-009); core VM-free and OS/file-free.
- **E-G5-LU** — Preserve Before Replace: existing subsystem seams reused unchanged; scripts own no authoritative state/events; networking-last preserved; baseline 675/675 intact. *Any new `tick_order` reservation is `[AR-1]`.*

## 11. Definition of Done (from scope authority §12)

Lua executes through documented APIs only; scripts cannot bypass subsystem ownership; event callbacks function correctly; the runtime remains stable under load; worker threads never execute gameplay scripts; ready for the Plugin Framework. Recorded at closure (Step-18).

## 12. Step ordering (dependency chain)

`01 types → 02 config → 03 ILuaRuntime seam (+fake/null) → 04 ScriptContext + ScriptRegistry → 05 event-binding registry → 06 script validation → 07 IScriptSource read seam (+null/in-memory) → 08 platform script-source backend (PlatformScriptStore) → 09 Public API facade seams → 10 script lifecycle state machine → 11 ScriptLoader → 12 LuaManager → 13 ScriptManager → 14 error handling + fault isolation → 15 diagnostics + statistics + performance → 16 thread-safety enforcement → 17 composition-root + engine-adapter wiring + integration + LuaScripting.md → 18 sprint closure.`

Each step is independently buildable (clean compile, tree green after each) and independently verifiable (its own tests), strictly additive.

### Dependency graph (edges = "depends on")

```
01 ← 02,03,04,05,06,09,10
03 ← 06(syntax),11,12
04 ← 05,06,10,11,13
05 ← 13(dispatch),17(engine events)
06 ← 11,13
07 ← 08,11        08 ← 17(wiring)
09 ← 12(register),17(engine facades)
10 ← 11,13        11,12 ← 13
13 ← 14,15,16,17  14 ← 17
16 ← 17           17 ← 18
```
No cycles. Reused Sprint-002/003/005/007/008/012 seams are pre-existing inputs (not steps).

### 12.A Implementation batching strategy (proposed — freeze with the plan)

The 18 steps above are frozen exactly as written; this subsection defines only the **execution batching** — which consecutive steps are implemented and Antigravity-verified together. Batching combines steps only where it violates no dependency, crosses no architectural/ADR/verification boundary, keeps diagnostics non-invasive and hardening test-only, and preserves independent testability and failure isolation. The load-bearing architectural gates (Step-08 platform boundary / ADR-009, Step-09 engine API boundary / ADR-008, Step-17 integration) remain isolated; Step-07 (read seam) and Step-18 (closure) are also their own batches. Step-13 (`ScriptManager`) is verified within the cohesive B8 Scripting Runtime Subsystem — its per-frame tick placement is resolved (`[AR-1]`) and is wired only at Step-17, so it is not a separate gate.

| Batch | Steps | Isolation | Nature of the batch |
|---|---|---|---|
| **B1** | 01 + 02 | — | Value types + `LuaConfiguration` (pure, independent, engine-free/VM-free foundation). |
| **B2** | 03 + 04 | — | `ILuaRuntime` seam (+ fake/null) + `ScriptContext`/`ScriptRegistry` (engine-free). |
| **B3** | 05 + 06 | — | Event-binding registry + script validation (mutually-independent pure components). |
| **B4** | 07 | **Isolated** | `IScriptSource` read seam (+ null/in-memory) — additive load boundary. |
| **B5** | 08 | **Isolated** | Platform script-source backend — **Singular Platform Boundary / ADR-009** (only script-side OS/file TU; Windows smoke). |
| **B6** | 09 | **Isolated** | Public API facade seams — **Singular Engine Boundary / ADR-008** decision point (no engine object exposed). |
| **B7** | 10 + 11 | — | Script lifecycle + `ScriptLoader` (engine-free consumers of the Step-07/09 seams). |
| **B8** | 12 + 13 + 14 | — | **Scripting Runtime Subsystem** (cohesive unit): `LuaManager` + `ScriptManager` + fault isolation, over the fake runtime + null sources + recording facades. |
| **B9** | 15 + 16 | — | Non-invasive diagnostics/statistics/performance + thread-safety enforcement (guard/assertions). |
| **B10** | 17 | **Isolated** | Composition-root + engine adapters + integration + `LuaScripting.md` — **Integration gate** (ADR-008 engine facades + ADR-009 VM/source backends + the `[AR-1]` execution-model/ordering decision; Windows smoke). |
| **B11** | 18 | **Isolated** | Sprint closure (documentation only; no code). |

**Batching does not alter the dependency graph.** Cross-batch dependencies are the edges listed above; batching only groups the verification cadence. Within any multi-step batch, steps are implemented in their documented order.

---

## Step specifications

Each step: **Objective · Scope In/Out · FR · Evidence · Tests · Commit.** All engine-free/VM-free/OS-free unless the step names an existing boundary; ADR-007 value outcomes; additive.

### Step-01 — Lua value types & vocabulary
- **Objective.** The engine-free vocabulary for the subsystem.
- **In.** `LuaScriptTypes.h` (header-only): `enum class ScriptOutcome : std::uint8_t { Ok, LoadFailed, SyntaxError, ApiIncompatible, VersionMismatch, DuplicateScript, InvalidCallback, MissingDependency, RuntimeError, RecursionLimit, ExecutionTimeout, ScriptDisabled, NotFound }` (+ name); `enum class ScriptState` (Unloaded, Loaded, Initialized, Executing, Suspended, Failed, Destroyed); `enum class ScriptEvent` (OnServerStart, OnServerStop, OnPlayerJoin, OnPlayerLeave, OnPlayerDeath, OnEntitySpawn, OnEntityDestroy, OnWorldLoaded, OnWorldSaved, OnTick); `struct ScriptId`/`CallbackId`; `struct ScriptDescriptor { ScriptId id; … version, generation }`; value API argument/result structs; `ScriptStatistics`, `ScriptConsistency`. Types only. **Out.** Logic.
- **FR.** POD/trivially-copyable value captures; deterministic layout; VM/timing figures isolated to diagnostic fields. **Evidence.** E-G2-LU groundwork. **Tests.** `LuaTypesStep1`. **Commit.** `docs(lua): freeze Sprint-013 Step-01 spec (types)`.

### Step-02 — `LuaConfiguration`
- **Objective.** Parsed `[lua]` config.
- **In.** `LuaConfiguration.h`/`.cpp`: opaque `scriptDirectoryToken` (resolved only by the platform backend — no path logic in the core), `maxScripts` (≥1), `executionBudgetMicros` (≥1), `recursionLimit` (≥1), `enabled`/`validateOnLoad`/`strictApiVersion` (bools), `version` (≥1); `FromConfig` (missing → defaults; invalid → InvalidArgument). **Out.** VM/filesystem access.
- **FR.** Cross-field validation; value outcomes; no OS/path resolution or VM access in the core. **Evidence.** E-G4-LU groundwork. **Tests.** `LuaConfigStep2`. **Commit.** `docs(lua): freeze Sprint-013 Step-02 spec (config)`.

### Step-03 — `ILuaRuntime` seam (+ fake/null)
- **Objective.** The engine-free VM boundary (the concrete VM binding is Step-17; its implementation is `[AR-2]`).
- **In.** `ILuaRuntime.h` (engine-free): create/destroy state, `LoadStandardLibraries`, `LoadChunk(name, bytes) -> Expected<ScriptId>`, `Execute(ScriptId) -> ScriptOutcome`, `RegisterFunction(name, handle)`, `InvokeCallback(CallbackId, args) -> ScriptOutcome`; a deterministic `FakeLuaRuntime` (records loads/executions/calls; scriptable outcomes) and `NullLuaRuntime` for tests. No VM/engine headers. **Out.** Concrete VM binding (Step-17, implementation `[AR-2]`).
- **FR.** Value-only surface; no VM type exposed; deterministic fake. **Evidence.** E-G4-LU. **Tests.** `LuaRuntimeSeamStep3`: fake load/execute/register/invoke; failure outcomes. **Commit.** `docs(lua): freeze Sprint-013 Step-03 spec (runtime seam)`.

### Step-04 — `ScriptContext` + `ScriptRegistry`
- **Objective.** The engine-free per-script context and deterministic registry.
- **In.** `ScriptContext.h` (value context: id, descriptor, state, callback ids) + `ScriptRegistry.h`/`.cpp`: `Register`/`Unregister`/`Find`/`Enumerate` (ascending id), duplicate-id detection → value outcome. Pure. **Out.** Loading/execution (Steps 11-13).
- **FR.** Deterministic enumeration; duplicate rejected as a value outcome; no VM/engine. **Evidence.** E-G2-LU. **Tests.** `ScriptRegistryStep4`. **Commit.** `docs(lua): freeze Sprint-013 Step-04 spec (registry)`.

### Step-05 — Event-binding registry
- **Objective.** The engine-free event→callback table (scripts react, never own).
- **In.** `EventBinding.h`/`.cpp`: `Bind(ScriptEvent, ScriptId, CallbackId)`/`Unbind`; deterministic ascending dispatch order; `CallbacksFor(ScriptEvent)`; no event emission from scripts. Pure. **Out.** Actual dispatch through the runtime (Step-13); engine-raised events (Step-17).
- **FR.** Deterministic dispatch order; scripts cannot emit/reorder events; value outcomes. **Evidence.** E-G5-LU. **Tests.** `EventBindingStep5`: bind/unbind, ordering, duplicate binding. **Commit.** `docs(lua): freeze Sprint-013 Step-05 spec (events)`.

### Step-06 — Script validation
- **Objective.** Pure validators over a candidate script.
- **In.** `ScriptValidator.h`/`.cpp`: syntax (via `ILuaRuntime` load), API compatibility, version, duplicate registration, invalid callbacks, missing dependencies → `ScriptOutcome`. Pure (uses the runtime seam only for a syntax load). **Out.** Loading orchestration (Step-11).
- **FR.** Deterministic; each rejection a value outcome; no mutation. **Evidence.** E-G3-LU. **Tests.** `ScriptValidatorStep6`: pass + each negative. **Commit.** `docs(lua): freeze Sprint-013 Step-06 spec (validation)`.

### Step-07 — `IScriptSource` read seam (+ null/in-memory)
- **Objective.** The additive engine-free read boundary for script sources (mirrors Sprint-012 `ISaveSource`).
- **In.** `IScriptSource.h` (engine-free): `Enumerate() -> std::vector<ScriptDescriptor>`, `Read(ScriptId) -> Expected<std::vector<std::byte>>`, `Exists(ScriptId) -> bool`; `InMemoryScriptSource` + `NullScriptSource`. No OS access. **Out.** Real filesystem backend (Step-08).
- **FR.** Value-outcome surface; deterministic enumeration (ascending). **Evidence.** E-G4-LU. **Tests.** `ScriptSourceStep7`. **Commit.** `docs(lua): freeze Sprint-013 Step-07 spec (source seam)`.

### Step-08 — Platform script-source backend (`PlatformScriptStore`)
- **Objective.** The real script-file backend, confined to ONE platform TU (ADR-009).
- **In.** `adapters/PlatformScriptStore.cpp` (the sole script-side OS/file TU) implementing `IScriptSource` (enumerate/read/exists) over the configured `scriptDirectoryToken`; engine-free factory declarations in a header; null/in-memory counterpart for tests. OS/file headers appear ONLY here. **Out.** Any VM/engine access; any validation logic.
- **FR.** Deterministic enumeration; value outcomes; OS confinement (One Platform Boundary). **Evidence.** E-G4-LU (platform build smoke is Antigravity's on Windows). **Tests.** in-memory/null tests in the engine-free build; the real TU is Antigravity-verified. **Commit.** `docs(lua): freeze Sprint-013 Step-08 spec (platform source)`.

### Step-09 — Public API facade seams
- **Objective.** The engine-free API surface through which scripts observe/effect the world (D-LUA2 / ADR-008).
- **In.** `IWorldApi`/`IPlayerApi`/`IEntityApi`/`IEnvironmentApi`/`IInventoryApi`/`ILoggingApi`/`IConfigApi` (each value-only: ids/handles/value structs, no engine object — scope §7.4); recording/fake facades for tests. The engine implementations are added at Step-17 in `EngineAdapters.cpp`, over the existing sanctioned seams per `[AR-4]` (resolved, §0.B): World/Environment → Sprint-002 + Sprint-008; Entity → Sprint-003; ALife → Sprint-005; Player/spawn → Sprint-007. **Out.** Engine implementations (Step-17); binding into the VM (Step-12).
- **FR.** No engine type exposed; every authoritative effect maps to a sanctioned seam; value outcomes. **Evidence.** E-G1-LU. **Tests.** `PublicApiStep9`: fake facades record calls; no engine coupling. **Commit.** `docs(lua): freeze Sprint-013 Step-09 spec (public api)`.

### Step-10 — Script lifecycle state machine
- **Objective.** The deterministic per-script lifecycle.
- **In.** `ScriptLifecycle.h`/`.cpp`: transitions Load → Initialize → Execute → Suspend → Resume → Unload → Destroy with legal-transition enforcement → value outcomes. Pure. **Out.** Orchestration (Step-13).
- **FR.** Deterministic; illegal transitions rejected as value outcomes; no VM/engine. **Evidence.** E-G2-LU. **Tests.** `ScriptLifecycleStep10`. **Commit.** `docs(lua): freeze Sprint-013 Step-10 spec (lifecycle)`.

### Step-11 — `ScriptLoader`
- **Objective.** Deterministic load composition.
- **In.** `ScriptLoader.h`/`.cpp`: enumerate via `IScriptSource`, read bytes, validate (Step-06), load into the runtime (Step-03), register (Step-04) in deterministic order → value outcomes. Engine-free. **Out.** Manager/tick (Step-13).
- **FR.** Deterministic order; failure isolates the one script; value outcomes. **Evidence.** E-G3-LU. **Tests.** `ScriptLoaderStep11`: load set, one bad script isolated. **Commit.** `docs(lua): freeze Sprint-013 Step-11 spec (loader)`.

### Step-12 — `LuaManager`
- **Objective.** Runtime lifecycle + Public API registration.
- **In.** `LuaManager.h`/`.cpp`: owns an `ILuaRuntime`; `Init` (create state, load libraries, register the Public API facades as host functions), `Shutdown`; exposes the runtime to the loader/manager. Engine-free (holds seams). **Out.** Per-frame tick / event dispatch (Step-13).
- **FR.** Deterministic init/shutdown; value outcomes; no engine/VM header. **Evidence.** E-G1-LU/E-G4-LU. **Tests.** `LuaManagerStep12`: init registers APIs; shutdown tears down. **Commit.** `docs(lua): freeze Sprint-013 Step-12 spec (lua manager)`.

### Step-13 — `ScriptManager`
- **Objective.** Script orchestration + the per-frame `OnTick` phase.
- **In.** `ScriptManager.h`/`.cpp`: `LoadAll`/`Unload`/`Reload`/execution tracking/failure isolation; dispatches events via `EventBinding` through the runtime; `IService` + `ITickable` whose `OnTick` runs in the Gameplay phase (new `tick_order` key between 350 and 400) per `[AR-1]` (resolved, §0.B), subscribed only at Step-17. Engine-free orchestration. **Out.** Bootstrap wiring/engine facades (Step-17).
- **FR.** Deterministic; single-thread; a per-callback fault isolates the script (engine continues); value outcomes. **Evidence.** E-G2-LU/E-G3-LU. **Tests.** `ScriptManagerStep13`: load/tick/reload; faulting callback isolated; deterministic dispatch order. **Commit.** `docs(lua): freeze Sprint-013 Step-13 spec (script manager)`.

### Step-14 — Error handling + fault isolation
- **Objective.** Harden the boundary against misbehaving scripts (D-LUA5).
- **In.** Boundary handling for runtime error, invalid API usage, missing script, infinite recursion (recursion-limit guard), execution timeout (budget guard), and crash — each mapped to a `ScriptOutcome` that disables the offending script while the engine continues; no C++ exception escapes the runtime seam. Additive/test-driven over the fake runtime. **Out.** New behavior beyond isolation.
- **FR.** Every fault is a value outcome; engine continues; deterministic. **Evidence.** E-G3-LU. **Tests.** `ScriptHardeningStep14`: each fault isolates the script; siblings keep running. **Commit.** `docs(lua): freeze Sprint-013 Step-14 spec (fault isolation)`.

### Step-15 — Diagnostics + statistics + performance
- **Objective.** A pure, non-invasive collector + inspectors.
- **In.** `LuaDiagnostics.h`: loaded-scripts / execution-timeline / API-browser / callback-viewer / console snapshots; statistics (loaded scripts, execution time, memory, API calls, callback count, errors); performance timing — all diagnostic-only, `Reset`/`Snapshot`. **Out.** Any control-flow dependence on timing.
- **FR.** Non-invasive; monotonic counters + deterministic aggregates; timing/memory never gate control flow. **Evidence.** E-G2-LU. **Tests.** `LuaDiagnosticsStep15`. **Commit.** `docs(lua): freeze Sprint-013 Step-15 spec (diagnostics)`.

### Step-16 — Thread-safety enforcement
- **Objective.** Enforce Simulation-Thread-only execution (scope §7.8 / ADR-011).
- **In.** A deterministic guard/assertion surface asserting script entry points run on the Simulation Thread and are never entered from a worker; documentation that snapshots remain the only worker channel. No thread created. **Out.** Any threading model change.
- **FR.** Deterministic guard; no thread; value outcomes. **Evidence.** E-G2-LU/E-G5-LU. **Tests.** `ThreadSafetyStep16`: guard admits sim-thread entry, rejects/asserts otherwise (test-simulated). **Commit.** `docs(lua): freeze Sprint-013 Step-16 spec (thread safety)`.

### Step-17 — Composition-root + engine-adapter wiring + integration + `LuaScripting.md`
- **Objective.** Wire the engine-free scripting layer to the real runtime, engine facades, and frame at the composition root.
- **In.** `Bootstrap.cpp`: construct `LuaConfiguration`; bind the real `ILuaRuntime` (VM implementation `[AR-2]`) + real `IScriptSource` (filesystem) behind factories (fake/in-memory in tests); construct `LuaManager`/`ScriptManager`; `LoadAll` at Initialize before networking (`[AR-5]` resolved); subscribe `ScriptManager` at the new Gameplay-phase `tick_order` key between `kAlifeTransition` (350) and `kReplication` (400) per `[AR-1]` (resolved); reverse-order teardown; default `[lua]` config block. `EngineAdapters.cpp`: the real Public API facade implementations (World/Player/Entity/Environment/Inventory/Logging/Config) over the sanctioned seams per the `[AR-4]` mapping, and the concrete `ILuaRuntime` binding (One Engine/Platform Boundary); null/fake counterparts in tests. Integration tests (server lifecycle, player/world/entity events, save/load callbacks, large collections, fault isolation). Author `Multiplayer/docs/LuaScripting.md`. **Out.** Behavior change; new features.
- **FR.** The per-frame execution model and any `tick_order` change follow `[AR-1]`; networking-last preserved; existing ordering/services unchanged; engine code confined to `EngineAdapters.cpp`; VM/filesystem code confined to their boundary TUs; no thread; reverse-order teardown. **Evidence.** E-G1-LU…E-G5-LU composed; engine/VM/platform build smoke is Antigravity's on Windows. **Tests.** `LuaScriptingIntegrationStep17` + Bootstrap wiring/ordering assertions (concrete ordering assertions per `[AR-1]`). **Commit.** `docs(lua): freeze Sprint-013 Step-17 spec (wiring + integration + LuaScripting.md)`.

### Step-18 — Sprint closure (no code)
- **Objective.** Confirm DoD; freeze status; declare Sprint-014 readiness.
- **In.** Cross-check §11 acceptance criteria and §12 DoD against delivered, verified artifacts; record the final baseline (`675 + Sprint-013 additions`); set `Sprint-013-Lua-Integration.md` to Implemented / Verified / Closed / Frozen (status + closure section); set this plan to EXECUTED & CLOSED; update `CURRENT_STATUS.md`, `SESSION_LOG.md`, README textual progress; consistency scan. **Out.** Any code/test change (except a genuine closure defect); any Sprint-014 work.
- **FR.** DoD satisfied & recorded; status consistent; Sprint-013 declared Closed/Frozen; Sprint-014 readiness stated. **Evidence.** All E-G*-LU recorded. **Commit.** `docs(lua): close Sprint-013`.

---

## Verification checkpoints (one per batch)

Each **batch** (§12.A) is a verification boundary: implement the batch's step(s) in their documented order → clean Release|x64 build (GCC engine-free + MSVC) with zero warnings, all step tests green, baseline preserved → Antigravity verification → git commit → git push → next batch. Every underlying step remains independently buildable and independently testable; batching only groups the verification cadence and never relaxes a step's own requirements.

Load-bearing gates are verified **in isolation** (never batched across): **Step-08 / Batch B5** (platform script-source boundary / ADR-009), **Step-09 / Batch B6** (Public API engine boundary / ADR-008), **Step-13** (script orchestration core, in the cohesive B8 Scripting Runtime Subsystem; its per-frame execution model / tick placement is `[AR-1]`, wired only at Step-17), **Step-17 / Batch B10** (integration — real runtime/facades; the `[AR-1]` execution-model/ordering decision applied here). **Step-18 / Batch B11** is the documentation-only closure boundary.

## Testing strategy (summary)

Per-step unit tests in `LuaScriptingTests.cpp`; deterministic registry/event/dispatch ordering; validation/lifecycle/fault negatives as value outcomes; orchestration through the fake runtime + null/in-memory sources + recording API facades (no engine, no VM); integration (`LuaScriptingIntegrationStep17`) over the composed manager; Bootstrap wiring/ordering assertions (concrete ordering per `[AR-1]`). The real runtime (`[AR-2]`), engine facades, and filesystem script source are Antigravity-smoke-verified on Windows.

## Documentation update sequence

- Per step: README progress marker + running test count (Steps 01-17).
- Step-17: author `Multiplayer/docs/LuaScripting.md` (Lua API, lifecycle, thread rules, supported callbacks, example scripts, best practices).
- Step-18: `Sprint-013-Lua-Integration.md` status + closure section; this plan → EXECUTED & CLOSED; `CURRENT_STATUS.md`; `SESSION_LOG.md`; README textual status.

## Acceptance criteria (from scope authority §11)

Lua runtime operational · Script Manager complete · Public API exposed · Event callbacks operational · Thread safety verified · Diagnostics operational · Tests passing · Documentation updated — each confirmed at Step-18.

## Definition of Done (from scope authority §12)

Lua executes through documented APIs only; scripts cannot bypass subsystem ownership; event callbacks function correctly; runtime remains stable under load; worker threads never execute gameplay scripts; ready for the Plugin Framework — all recorded at closure.

## Sprint closure checklist (Step-18)

1. All 18 steps implemented, each Antigravity-verified, tree buildable after each.
2. Final baseline recorded (`675 + Sprint-013 additions`), GCC + MSVC, zero warnings, no regressions.
3. Evidence gates E-G1-LU…E-G5-LU recorded PASSED.
4. Boundaries confirmed: engine code only in `EngineAdapters.cpp`; VM binding and script-file I/O only in their boundary TUs; any `tick_order` change matches the approved `[AR-1]` decision; networking-last preserved; no thread; wire protocol untouched.
5. Preserve Before Replace confirmed: existing subsystem seams reused unchanged per the approved `[AR-4]` mapping; no internal engine object exposed to Lua.
6. ADR-007…ADR-011 preserved; no ADR reinterpreted.
7. `Sprint-013-Lua-Integration.md` → Implemented / Verified / Closed / Frozen; this plan → EXECUTED & CLOSED; status docs + README synchronized.
8. Sprint-014 (Plugin Framework) readiness stated.

## Compatibility & non-regression checklist (every step)
- Sprint-012 **675/675** baseline preserved; new tests only add.
- Engine headers only in `EngineAdapters.cpp`; VM headers only in the runtime boundary TU; OS/file headers only in `PlatformScriptStore.cpp`; no wire change; no thread; any `tick_order` change per the approved `[AR-1]`.
- Existing subsystem seams (world/environment/entity/player/ALife/snapshot) reused unchanged per the approved `[AR-4]` mapping; scripts own no authoritative state or events.
- Reads via sanctioned read seams; authoritative effects only through sanctioned write seams; Simulation-Thread-only execution; ADR-007…ADR-011 preserved.

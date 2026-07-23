# Sprint-013: Lua Integration
# Sprint-013: Lua Integration

---

# Sprint Information

| Field | Value |
|--------|-------|
| Sprint | 013 |
| Title | Lua Integration |
| Status | **Implemented / Verified / Closed / Frozen** (2026-07-20) |
| Priority | High |
| Estimated Duration | 2–3 Weeks |
| Prerequisites | Sprint-001 through Sprint-012 |
| Next Sprint | Sprint-014 – Plugin Framework |

---

# 1. Sprint Overview

Sprint-013 introduces the Lua Integration Layer.

Rather than exposing internal engine implementation directly to scripts, this sprint creates a controlled scripting API that allows gameplay logic, events, quests, debugging tools, and server customization while preserving subsystem ownership and deterministic simulation.

Lua becomes the primary scripting interface for gameplay extensions.

The engine remains authoritative.

Scripts execute through documented APIs only.

---

# 2. Objectives

## Primary Objectives

- Embed Lua runtime
- Create Lua Manager
- Create Script Manager
- Expose Engine API
- Implement Event Binding
- Implement Script Loading
- Implement Script Lifecycle

## Secondary Objectives

- Lua diagnostics
- Script profiling
- Debug console
- API documentation

---

# 3. Scope

Included

- Lua runtime
- Script Manager
- Public API
- Event binding
- Script lifecycle
- Lua diagnostics
- Script validation

---

# 4. Out of Scope

Not Included

- Plugin system
- Hot reload
- Sandbox security
- Multiplayer scripting
- Mod packaging
- Marketplace

---

# 5. Architecture References

10_Extensibility.md

03_Player.md

04_World.md

---

# 6. RFC References

RFC-0007 – Plugin & Extension System

RFC-0006 – Threading Execution Model

RFC-0001 – Host Authoritative Simulation

---

# 7. Technical Tasks

---

## 7.1 Lua Manager

Create

LuaManager

LuaConfiguration

LuaContext

Responsibilities

Initialize runtime

Shutdown runtime

Load libraries

Manage script state

Register APIs

---

## 7.2 Script Manager

Create

ScriptManager

ScriptRegistry

ScriptLoader

ScriptContext

Responsibilities

Load scripts

Unload scripts

Reload scripts

Track execution

Handle failures

---

## 7.3 Lua Runtime

Initialize

Lua VM

Memory allocator

Script search paths

Runtime configuration

Version validation

---

## 7.4 Public Engine API

Expose

World API

Player API

Entity API

Environment API

Inventory API

Logging API

Configuration API

No internal engine objects exposed.

---

## 7.5 Event Binding

Register

OnServerStart

OnServerStop

OnPlayerJoin

OnPlayerLeave

OnPlayerDeath

OnEntitySpawn

OnEntityDestroy

OnWorldLoaded

OnWorldSaved

OnTick

Scripts react to events.

Scripts do not own events.

---

## 7.6 Script Lifecycle

Support

Load

Initialize

Execute

Suspend

Resume

Unload

Destroy

Maintain deterministic execution.

---

## 7.7 Script Validation

Verify

Syntax

API compatibility

Version

Duplicate registration

Invalid callbacks

Missing dependencies

---

## 7.8 Thread Safety

Ensure

Lua executes on Simulation Thread only.

Worker threads never invoke gameplay scripts.

Snapshots remain the only worker communication mechanism.

---

## 7.9 Error Handling

Handle

Runtime exceptions

Invalid API usage

Missing scripts

Infinite recursion detection

Execution timeout

Script crashes

Engine continues running.

---

## 7.10 Diagnostics

Provide

Script Inspector

Execution Timeline

Loaded Scripts

API Browser

Callback Viewer

Lua Console

---

## 7.11 Statistics

Track

Loaded Scripts

Execution Time

Memory Usage

API Calls

Callback Count

Script Errors

---

## 7.12 Performance

Measure

Script execution

API overhead

Memory allocation

VM startup

Callback latency

---

## 7.13 Unit Tests

Verify

Runtime initialization

API registration

Script loading

Event callbacks

Error handling

Script unloading

Stress testing

---

## 7.14 Integration Tests

Verify

Player events

World events

Entity events

Server lifecycle

Save/Load callbacks

Large script collections

---

## 7.15 Documentation

Document

Lua API

Lifecycle

Thread rules

Supported callbacks

Example scripts

Best practices

---

# 8. Deliverables

✓ Lua Manager

✓ Script Manager

✓ Public API

✓ Event System

✓ Lua Runtime

✓ Diagnostics

✓ Tests

✓ Documentation

---

# 9. Risks

Potential Risks

Script abuse

Performance degradation

API misuse

Infinite loops

Memory leaks

Version mismatch

---

Mitigation

Controlled APIs

Execution limits

Profiling

Validation

Documentation

Testing

---

# 10. Validation Strategy

Runtime

✓ Lua initializes.

Loading

✓ Scripts load correctly.

Execution

✓ Callbacks execute.

Events

✓ Events delivered correctly.

Performance

✓ Execution remains within budget.

Stress Test

✓ Hundreds of scripts execute reliably.

---

# 11. Acceptance Criteria

✅ Lua runtime operational.

✅ Script Manager complete.

✅ Public API exposed.

✅ Event callbacks operational.

✅ Thread safety verified.

✅ Diagnostics operational.

✅ Tests passing.

✅ Documentation updated.

---

# 12. Definition of Done

Sprint-013 is complete when

- Lua executes through documented APIs only.
- Scripts cannot bypass subsystem ownership.
- Event callbacks function correctly.
- Runtime remains stable under load.
- Worker threads never execute gameplay scripts.
- Ready for Plugin Framework.

---

# 13. Next Sprint

Sprint-014 – Plugin Framework

Sprint-014 expands the extensibility layer by introducing a native plugin framework for server modules, tooling, diagnostics, and future engine extensions. Unlike Lua scripts, plugins provide compiled extensions through stable interfaces while continuing to respect host authority, immutable snapshots, and subsystem ownership.

---

# 14. Sprint Closure (2026-07-20)

**Sprint-013 (Lua Integration) is declared Implemented / Verified / Closed / Frozen.**

All 18 steps were implemented under the mandatory workflow (implement → Antigravity verification → git commit → GitHub push → next batch) — grouped into eleven verification batches (B1–B11) with the load-bearing gates (Step-08 platform boundary, Step-09 engine API boundary, Step-17 integration) verified in isolation — and each step was independently verified by Antigravity, with the tree left buildable after every step.

## 14.1 Final verified baseline
- **736 / 736 build tests passing** — Release x64 on **MSVC** and the engine-free **GCC** test build; **0 errors, 0 warnings, no regressions.** (Sprint-012 baseline 675 + the 61-test Sprint-013 scripting suite.)
- MSVC Release clean under `EngineAbi.props`. The concrete Lua runtime binding, the seven Public API facades, and the real filesystem script source are Antigravity-smoke-verified on Windows. Game testing has not started yet.

## 14.2 Steps 01–18 (all implemented, verified, documented)
01 value types + vocabulary (`LuaScriptTypes.h`) · 02 `LuaConfiguration` · 03 `ILuaRuntime` seam (+ fake/null) · 04 `ScriptContext` + `ScriptRegistry` · 05 event-binding registry (`EventBinding`) · 06 `ScriptValidator` · 07 `IScriptSource` read seam (+ null/in-memory) · 08 platform script-source backend (`PlatformScriptStore`) · 09 Public API facade seams (`ScriptApi`) · 10 `ScriptLifecycle` state machine · 11 `ScriptLoader` · 12 `LuaManager` · 13 `ScriptManager` · 14 fault isolation · 15 `LuaDiagnostics` · 16 `ScriptThreadGuard` · 17 composition-root + engine adapters + integration + `Multiplayer/docs/LuaScripting.md` · 18 sprint closure.

## 14.3 Acceptance criteria (§11) — satisfied
1. Lua runtime operational (`ILuaRuntime` seam + the real binding reusing the engine's existing Lua runtime, confined to `EngineAdapters.cpp`). ✅
2. Script Manager complete (`ScriptManager`: load/unload/reload + deterministic event/OnTick dispatch + fault isolation). ✅
3. Public API exposed (the seven engine-free facades; no internal engine object exposed, ADR-008 / §7.4). ✅
4. Event callbacks operational (`EventBinding` deterministic dispatch; the ten host-dispatched events). ✅
5. Thread safety verified (`ScriptThreadGuard` Simulation-Thread-only enforcement; no thread created). ✅
6. Diagnostics operational (`LuaDiagnostics` non-invasive counters/timeline/console). ✅
7. Tests passing (736/736, GCC + MSVC). ✅
8. Documentation updated (`Multiplayer/docs/LuaScripting.md`; status docs synchronized). ✅

## 14.4 Definition of Done (§12) — satisfied
- Lua executes through documented APIs only (the seven facades are the sole surface; no engine object exposed). ✅
- Scripts cannot bypass subsystem ownership (authoritative effects only through the sanctioned Sprint-002/003/005/007/008 seams). ✅
- Event callbacks function correctly (deterministic, order-stable dispatch). ✅
- Runtime remains stable under load (large-collection + fault-isolation integration tests; a faulting script disables only itself). ✅
- Worker threads never execute gameplay scripts (Simulation-Thread-only; snapshots remain the only worker channel; no thread created). ✅
- Ready for the Plugin Framework (Sprint-014). ✅

## 14.5 Evidence gates — satisfied
- **E-G1-LU** (controlled API; no internal engine object exposed; effects only via sanctioned seams): **PASSED**.
- **E-G2-LU** (determinism & single-thread; `OnTick` at the fixed `kScripting` slot; no thread; VM timing/GC diagnostic-only): **PASSED**.
- **E-G3-LU** (fault isolation; every fault a value outcome; engine continues; no exception escapes the seam): **PASSED**.
- **E-G4-LU** (Singular Platform Boundary; VM binding + script-file I/O confined to their boundary TUs): **PASSED**.
- **E-G5-LU** (Preserve Before Replace; existing seams reused unchanged; one additive `tick_order` key; networking-last preserved; baseline intact): **PASSED**.

## 14.6 Boundaries & invariants (verified)
- Engine code confined to `EngineAdapters.cpp`; the concrete VM binding reuses the engine's existing Lua runtime ([AR-2]); script-file I/O confined to `PlatformScriptStore.cpp`; authoritative effects only through the Step-09 Public API facades over the sanctioned seams (ADR-008 / ADR-009). ✅
- **Exactly one new `tick_order` key** — `kScripting = 375` (Gameplay phase: after ALifeTransition 350, before Snapshot 400); networking-last (`kNetworking = 900`) preserved; the `ScriptManager` ticks there and nowhere else; no thread created (ADR-011); wire protocol untouched (ADR-010). ✅
- Preserve Before Replace: Sprint-002/003/005/007/008 seams reused unchanged; no internal engine object exposed to Lua; scripts own no authoritative state or events. ✅
- ADR-007…ADR-011 preserved; no ADR reinterpreted. ✅

## 14.7 Sprint-014 readiness
The scripting layer embeds a controlled Lua runtime, exposes a documented engine-free Public API through which host-side scripts observe the world and effect change only via sanctioned seams, dispatches gameplay events deterministically on the Simulation Thread, and isolates script faults so the engine continues. No authoritative `tick_order` value beyond the reserved `kScripting = 375` is assigned or depended upon. **The project is ready for Sprint-014 (Plugin Framework).**
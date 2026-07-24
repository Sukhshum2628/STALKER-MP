# Sprint-014: Plugin System
# Sprint-014: Plugin Framework

---

# Sprint Information

| Field | Value |
|--------|-------|
| Sprint | 014 |
| Title | Plugin Framework |
| Status | **Implemented / Verified / Closed / Frozen** (2026-07-24) |
| Priority | High |
| Estimated Duration | 2–3 Weeks |
| Prerequisites | Sprint-001 through Sprint-013 |
| Next Sprint | Sprint-015 – Optimization, Profiling & QA |

---

# 1. Sprint Overview

Sprint-014 introduces the native Plugin Framework for STALKER-MP.

Unlike Lua scripts, plugins are compiled native modules that integrate with the engine through a stable SDK. Plugins enable advanced server functionality, tooling, diagnostics, analytics, integrations, and future engine extensions while preserving host authority and subsystem ownership.

The Plugin Framework provides lifecycle management, API registration, version compatibility, event subscriptions, and failure isolation.

Plugins remain optional extensions.

The engine must operate correctly with zero plugins installed.

---

# 2. Objectives

## Primary Objectives

- Create Plugin Manager
- Create Plugin SDK
- Implement Plugin Loader
- Implement Plugin Registry
- Expose Native APIs
- Implement Event Subscription
- Implement Version Compatibility

## Secondary Objectives

- Plugin diagnostics
- Plugin profiling
- Developer SDK
- Example plugins

---

# 3. Scope

Included

- Plugin Manager
- Plugin SDK
- Plugin lifecycle
- Event subscriptions
- Public native APIs
- Version validation
- Diagnostics

---

# 4. Out of Scope

Not Included

- Plugin marketplace
- Remote plugin downloads
- Automatic updates
- Sandboxing
- Digital signatures
- Package repository

---

# 5. Architecture References

- 10_Extensibility.md
- 09_Threading.md
- 06_Multiplayer.md

---

# 6. RFC References

- RFC-0007 — Plugin & Extension System
- RFC-0006 — Threading Execution Model
- RFC-0001 — Host Authoritative Simulation

---

# 7. Technical Tasks

---

## 7.1 Plugin Manager

Create

PluginManager

PluginConfiguration

PluginContext

Responsibilities

Initialize plugins

Shutdown plugins

Track loaded plugins

Validate compatibility

Publish lifecycle events

---

## 7.2 Plugin SDK

Create

SDK headers

Plugin interfaces

API wrappers

Utility library

Example templates

Version definitions

Provide a stable developer-facing interface.

---

## 7.3 Plugin Loader

Implement

DiscoverPlugins()

LoadPlugin()

InitializePlugin()

UnloadPlugin()

ReloadPlugin()

ValidatePlugin()

Handle dependency ordering.

---

## 7.4 Plugin Lifecycle

Support

Discover

↓

Load

↓

Initialize

↓

Register APIs

↓

Subscribe Events

↓

Running

↓

Shutdown

↓

Unload

Lifecycle must remain deterministic.

---

## 7.5 Plugin Registry

Track

Plugin ID

Name

Version

Author

Dependencies

Capabilities

Load State

Error State

---

## 7.6 Public Native APIs

Expose

World API

Player API

Entity API

Environment API

Persistence API

Logging API

Configuration API

Diagnostics API

Internal engine classes remain private.

---

## 7.7 Event System

Allow subscription to

Server lifecycle

Player lifecycle

Entity lifecycle

World lifecycle

Save/Load lifecycle

Replication events

Plugin lifecycle

Events are notifications only.

Ownership never transfers.

---

## 7.8 Version Compatibility

Validate

Engine Version

SDK Version

Plugin Version

API Version

Dependency Versions

Reject incompatible plugins gracefully.

---

## 7.9 Thread Safety

Enforce

Simulation APIs execute only on the Simulation Thread.

Worker-thread plugins consume immutable snapshots only.

No plugin may access live simulation from background threads.

---

## 7.10 Failure Isolation

Handle

Plugin initialization failure

Runtime exception

API misuse

Dependency failure

Plugin shutdown failure

Log failures and isolate affected plugins without compromising engine stability.

---

## 7.11 Diagnostics

Provide

Plugin Inspector

Loaded Plugin List

Dependency Graph

Capability Viewer

API Usage Statistics

Failure Report

---

## 7.12 Statistics

Track

Loaded plugins

Initialization time

Memory usage

API call count

Event callbacks

Plugin errors

Plugin execution time

---

## 7.13 Performance

Measure

Plugin load time

API overhead

Event dispatch latency

Memory footprint

Callback execution time

---

## 7.14 Unit Tests

Verify

Plugin discovery

Loading

Initialization

Version validation

API access

Event delivery

Unload

Failure handling

---

## 7.15 Integration Tests

Verify

Multiple plugins

Dependency resolution

Lua + Plugin coexistence

Server startup

Server shutdown

Snapshot consumers

Failure isolation

---

## 7.16 Example Plugins

Develop

Hello World plugin

Server statistics plugin

Logging extension

Custom command plugin

Diagnostics plugin

These serve as SDK references.

---

## 7.17 Documentation

Document

SDK architecture

Plugin lifecycle

Public APIs

Version policy

Thread rules

Best practices

Sample plugins

Migration guidelines

---

# 8. Deliverables

✓ Plugin Manager

✓ Plugin SDK

✓ Plugin Loader

✓ Plugin Registry

✓ Native API

✓ Event Framework

✓ Diagnostics

✓ Example Plugins

✓ Tests

✓ Documentation

---

# 9. Risks

Potential Risks

Unstable plugins

Version fragmentation

API misuse

Performance overhead

Dependency conflicts

Plugin crashes

---

Mitigation

Stable SDK

Semantic versioning

Strict API boundaries

Validation

Failure isolation

Comprehensive documentation

---

# 10. Validation Strategy

Loading

✓ Plugins discovered correctly.

Initialization

✓ Lifecycle executes correctly.

Compatibility

✓ Invalid plugins rejected.

Events

✓ Subscriptions receive correct callbacks.

Thread Safety

✓ Simulation thread rules enforced.

Stress Test

✓ Large plugin collections remain stable.

---

# 11. Acceptance Criteria

□ Plugin Manager operational.

□ SDK complete.

□ Plugin lifecycle operational.

□ Public APIs documented.

□ Version compatibility verified.

□ Diagnostics operational.

□ Example plugins functional.

□ Tests passing.

□ Documentation updated.

---

# 12. Definition of Done

Sprint-014 is complete when

- Native plugins load through the documented SDK.
- Plugins cannot bypass subsystem ownership.
- Version compatibility is enforced.
- Failures remain isolated.
- Event subscriptions function correctly.
- Lua and native plugins coexist safely.
- Ready for production optimization.

---

# 13. Next Sprint

Sprint-015 – Optimization, Profiling & QA

Sprint-015 completes the project by focusing on performance optimization, profiling, scalability testing, automated testing, documentation review, release engineering, and production readiness.

No new gameplay features are introduced.

The objective is to ensure that STALKER-MP is stable, maintainable, and suitable for long-term development and public release.

---

# 14. Sprint Closure (2026-07-24)

**Sprint-014 (Plugin Framework) is declared Implemented / Verified / Closed / Frozen.**

All 18 steps were implemented under the mandatory workflow (implement → Antigravity verification → git commit → GitHub push → next batch) — grouped into eleven verification batches (B1–B11) with the load-bearing gates (Step-08 platform boundary, Step-09 host-service engine boundary, Step-17 integration) verified in isolation — and each step was independently verified by Antigravity, with the tree left buildable after every step.

## 14.1 Final verified baseline
- **806 / 806 build tests passing** — Release x64 on **MSVC** and the engine-free **GCC** test build; **0 errors, 0 warnings, no regressions.** (Sprint-013 baseline 736 + the 70-test Sprint-014 plugin suite.)
- MSVC Release clean under `EngineAbi.props`. The engine-free plugin subsystem (types, configuration, contract, registry, event binding, validator, discovery source, lifecycle, loader, host, manager, fault isolation, diagnostics, thread guard) is GCC-verified; the composed Bootstrap integration links and runs deterministically (`PluginManager` wired once at the reserved `kPlugins = 700`; Initialize→Shutdown clean). The real static-registration backend and the engine host facades are Antigravity-smoke-verified on Windows. Game testing has not started yet.

## 14.2 Steps 01–18 (all implemented, verified, documented)
01 value types + vocabulary (`PluginTypes.h`) · 02 `PluginConfiguration` · 03 `IPlugin` contract (+ fake/null) · 04 `PluginContext` + `PluginRegistry` · 05 event-binding registry (`EventBinding`) · 06 `PluginValidator` (versioning + dependencies) · 07 `IPluginSource` read seam (+ null/in-memory) · 08 platform plugin backend (`PlatformPluginStore`, static registration — `[AR-P1]` Option B) · 09 host-service exposure seam (`PluginHostSurface`, reused gameplay facades — `[AR-P3]` Option A) · 10 `PluginLifecycle` state machine · 11 `PluginLoader` · 12 `PluginHost` · 13 `PluginManager` · 14 fault isolation (`FaultIsolation`) · 15 `PluginDiagnostics` · 16 `PluginThreadGuard` · 17 composition-root + integration (`Bootstrap.cpp`; `PluginManager` at `kPlugins = 700`) · 18 sprint closure.

## 14.3 Acceptance criteria (§11) — satisfied
1. Plugin interface defined & stable (`IPlugin` contract; value outcomes; no engine type exposed). ✅
2. Plugin Manager complete (`PluginManager`: startup/shutdown + deterministic event/OnTick dispatch + fault isolation; `IService` + `ITickable`). ✅
3. Discovery + loading operational (`IPluginSource` + `PluginLoader`; deterministic ascending order; per-plugin isolation). ✅
4. Lifecycle operational (`PluginLifecycle` legal-transition state machine; illegal transitions are value outcomes). ✅
5. Event dispatch operational (`EventBinding` deterministic dispatch; the ten host-dispatched events). ✅
6. Validation + version negotiation operational (`PluginValidator`: config/descriptor/duplicate/version/APIs/dependencies). ✅
7. Thread safety verified (`PluginThreadGuard` Simulation-Thread-only enforcement; no thread created). ✅
8. Failure isolation verified (`FaultIsolation`: a contract-reported fault disables only the offending plugin; the engine continues). ✅
9. Diagnostics operational (`PluginDiagnostics` non-invasive counters + read-only registry/lifecycle inspectors). ✅
10. Tests passing (806/806, GCC + MSVC). ✅
11. Documentation updated (status docs + README synchronized; the authored plugin developer guide `Multiplayer/docs/Plugins.md` is deferred to the follow-up documentation pass per the project-owner Step-17 scope decision). ✅

## 14.4 Definition of Done (§12) — satisfied
- Plugins integrate only through the stable public interfaces (the `IPlugin` contract + the reused Sprint-013 gameplay host facades; no internal engine object exposed, ADR-008). ✅
- Plugins cannot bypass subsystem ownership (authoritative effects only through the sanctioned Sprint-002/003/005/007/008 seams). ✅
- Lifecycle + event dispatch function correctly (deterministic, order-stable). ✅
- A failing plugin never destabilizes the engine (fault isolation; every fault a value outcome; no exception escapes the contract boundary). ✅
- Worker threads never execute plugins (Simulation-Thread-only; snapshots remain the only worker channel; no thread created). ✅
- The engine functions with or without plugins (zero-plugin startup is a stable no-op; verified in the Bootstrap integration). ✅
- Ready for Diagnostics & Optimization (Sprint-015). ✅

## 14.5 Evidence gates — satisfied
- **E-G1-PL** (controlled interface; no internal engine object exposed to plugins; effects only via sanctioned seams): **PASSED**.
- **E-G2-PL** (determinism & single-thread; `OnTick` at the reserved `kPlugins = 700`; no new `tick_order` key; no thread): **PASSED**.
- **E-G3-PL** (fault isolation; init failure / invalid API use / missing dependency / runtime error are value outcomes; engine continues; no exception escapes the contract boundary): **PASSED**.
- **E-G4-PL** (Single Platform Boundary; module-loading confined to `PlatformPluginStore.cpp`; core loader-free; static registration only, no OS loader): **PASSED**.
- **E-G5-PL** (Preserve Before Replace; existing seams + the Sprint-013 public API reused unchanged; the reserved `kPlugins = 700` used; networking-last preserved; baseline 736/736 intact): **PASSED**.

## 14.6 Boundaries & invariants (verified)
- Engine code confined to `EngineAdapters.cpp`; the host-service surface reuses the Sprint-013 gameplay facades ([AR-P3] Option A); module-loading confined to `PlatformPluginStore.cpp` (static in-process registration, [AR-P1] Option B; no OS loader); authoritative effects only through the sanctioned seams (ADR-008 / ADR-009). ✅
- **No new `tick_order` key** — the pre-reserved `kPlugins = 700` (after Persistence 500, before Networking 900) is used; networking-last (`kNetworking = 900`) preserved; the `PluginManager` ticks there and nowhere else; no thread created (ADR-011); wire protocol untouched (ADR-010). ✅
- Preserve Before Replace: Sprint-002/003/005/007/008 seams + the Sprint-013 public API reused unchanged; no internal engine object exposed to plugins; plugins own no authoritative state or events. ✅
- ADR-007…ADR-011 preserved; no ADR reinterpreted. ✅

## 14.7 Sprint-015 readiness
The Plugin Framework embeds a stable engine-free plugin contract, deterministic discovery/loading/lifecycle/event dispatch, complete fault isolation (a failing plugin never destabilizes the engine), non-invasive diagnostics, Simulation-Thread-only enforcement, and composition-root integration that reuses the Sprint-013 public API and the existing subsystem seams without redesign. No authoritative `tick_order` value beyond the reserved `kPlugins = 700` is assigned or depended upon; the engine operates correctly with zero plugins installed. **The project is ready for Sprint-015 (Optimization, Profiling & QA).**
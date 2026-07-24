# Sprint-014 — Plugin Framework · Architecture & Implementation Plan

| Field | Value |
|---|---|
| Sprint | 014 — Plugin Framework |
| Status | **COMPLETED** — EXECUTED & CLOSED (2026-07-24). All 18 steps implemented across batches B1–B11, each independently Antigravity-verified; final baseline **806 / 806** tests passing (GCC + MSVC), zero warnings, no regressions; evidence gates E-G1-PL…E-G5-PL PASSED. Previously **FROZEN** (2026-07-20) — All Approval-Required items resolved (`[AR-P2]`/`[AR-P4]` from frozen docs; `[AR-P1]` Option B / `[AR-P3]` Option A by project-owner decision); step scope, dependency chain, and batching immutable. |
| Scope authority | `Documentation/SPRINTS/Sprint-014/Sprint-014-Plugin-System.md` (approved) |
| Baseline | Sprint-013 CLOSED & FROZEN — **736 / 736** tests passing (GCC + MSVC) |
| Governing ADRs | ADR-007, ADR-008, ADR-009, ADR-010, ADR-011 (all preserved; none modified) |
| Architecture refs | `10_Extensibility.md` (§7/§11/§12/§13/§16/§20), `02_Engine.md`, `09_Threading.md`; RFC-0007, RFC-0006, RFC-0001 |
| Reuses (unchanged) | Sprint-013 public API surface (`lua::` Public API facades / host interfaces); Sprint-002 `IWorldQueries`/`ISpatialQueries`/`EnvironmentService`; Sprint-003 Entity Registry; Sprint-005 `IAlifeSwitchGateway`; Sprint-007 player/spawn seams; Sprint-008 `ISnapshotView`; core `FrameDispatcher` (reserved `tick_order::kPlugins = 700`), `ServiceRegistry`, `Expected`, `ConfigStore`, `Log` |
| Steps | 18 (Step-01 … Step-18), strict additive dependency chain |

---

## Status lifecycle

This plan moved through three states and is now **④ EXECUTED & CLOSED (2026-07-24)** — implementation complete, verification successful.

- **① DRAFT (PLANNED)** — content authored; Approval-Required register open. *(Passed.)*
- **② FREEZE REVIEW** — Antigravity verification PASSED; `[AR-P2]`/`[AR-P4]` resolved from frozen documentation; `[AR-P1]`/`[AR-P3]` referred to the project owner. *(Passed.)*
- **③ FROZEN (2026-07-20).** The project owner decided `[AR-P1]` (Option B — static registration; dynamic loading deferred) and `[AR-P3]` (Option A — gameplay facades only; administration/tooling deferred). All Approval-Required items resolved; step scope, dependency chain, and batching immutable. *(Passed.)*
- **④ EXECUTED & CLOSED — current state (2026-07-24).** All 18 steps implemented across batches B1–B11 under the standard workflow (implement → Antigravity verification → commit → push → next batch), the load-bearing gates (B5/B6/B10) verified in isolation. Final baseline **806 / 806** tests passing (GCC + MSVC), zero warnings, no regressions; evidence gates E-G1-PL…E-G5-PL PASSED. Sprint-014 is Closed & Frozen.

---

## 0. Governance & preserved invariants (apply to every step)

This plan is **purely additive** and modifies no prior sprint's frozen documents, code, or ADRs. Every step preserves, without exception:

- **Preserve Before Replace** — Sprint-014 REUSES the existing subsystem seams unchanged (Sprint-002 world/environment reads, Sprint-003 Entity Registry, Sprint-005 `IAlifeSwitchGateway`, Sprint-007 player/spawn seams, Sprint-008 `ISnapshotView`) and the Sprint-013 public API surface as the plugin host-service surface. It adds a new engine-free `plugin::` framework **additively**; it reimplements none of the world, ALife, snapshot, replication, persistence, scripting, or save/load logic.
- **Host Authority / One Persistent World / One ALife Simulation** — plugins are host-side consumers. They observe the single authoritative world through the existing read seams and effect change only through the sanctioned write seams; they own no world/ALife/simulation state and create no second world/simulation.
- **Explicit Ownership** — the engine retains authority over World, ALife, Replication, Persistence, Threading, and Multiplayer (`10_Extensibility.md` §11.5); plugins consume services and never replace subsystem ownership.
- **Deterministic Simulation** — plugin discovery, loading, lifecycle transitions, and event/tick dispatch are deterministic and order-stable; timing/memory figures are diagnostic-only and never gate control flow.
- **Immutable Snapshots** — plugins never mutate snapshots; snapshots remain the only worker-thread channel.
- **Single Engine Boundary (ADR-008)** — plugin authoritative effects cross into subsystems only through the sanctioned seams; engine-touching implementations are confined to `EngineAdapters.cpp`; no internal engine object is exposed to a plugin.
- **Single Platform Boundary (ADR-009)** — the concrete plugin module-loading / OS code is confined to ONE platform translation unit behind an engine-free seam (mirroring `PlatformScriptStore.cpp` / `PlatformSockets.cpp`); the core is OS/loader-free.
- **ADR-007** — no exceptions, no RTTI, no iostream in the engine-free core; all fallible operations return `core::Expected<T>` / `PluginOutcome` value outcomes; a plugin fault reported through the contract never propagates as a C++ exception into the engine; bounded memory.
- **ADR-010** — the wire protocol is untouched; plugins are host-local and send nothing.
- **ADR-011 / Single-threaded Deterministic Networking & Gameplay** — the module creates **no thread**; plugins execute synchronously on the Simulation Thread only; worker threads never invoke plugins. **No new `tick_order` key is introduced** — the reserved `tick_order::kPlugins = 700` is used (see §7).

**Compatibility.** Every step is additive and leaves the Sprint-013 **736/736** baseline intact; new tests only add to the count. No prior public API changes.

---

## 0.A ADR-derived boundary decisions (supported by the frozen architecture/ADRs)

These follow directly from the frozen ADRs/architecture applied to the scope's components; they invent no architecture. Points the scope/ADRs do **not** settle are deferred to §0.B (`[AR-Pn]`), not decided here.

- **D-PL1 — Plugins integrate through a stable engine-free contract; registration is confined to a platform boundary (ADR-009; `10_Extensibility.md` §7/§11).** The `plugin::` core defines an engine-free `IPlugin` contract and an engine-free `IPluginSource` discovery/read seam; the concrete registration/loading path is confined to ONE platform TU behind engine-free factories. Per the `[AR-P1]` decision (Option B, §0.B.2), Sprint-014 uses **static in-process registration** behind that seam; native shared-library loading is deferred to a future sprint. Tests use a fake/null plugin and an in-memory source, so the core is loader-free and unit-testable.
- **D-PL2 — Plugins reach subsystems ONLY through the existing public interfaces; no internal engine object is exposed (scope §7.5 + ADR-008 + `10_Extensibility.md` §11.7).** The plugin host-service surface reuses the Sprint-013 engine-free Public API facades (World/Player/Entity/Environment/Inventory/Logging/Config); the real implementations remain confined to `EngineAdapters.cpp` and route through the sanctioned Sprint-002/003/005/007/008 seams. Plugins exchange value handles/ids/structs only.
- **D-PL3 — Single-thread, no-thread execution at the reserved `kPlugins = 700` (ADR-011 + `09_Threading.md` + the frozen `tick_order` registry).** Plugins execute on the Simulation Thread only; the `PluginManager` ticks as one `IService` + `ITickable` at the pre-reserved `tick_order::kPlugins = 700` (after Persistence 500, before Networking 900) — plugins are reactors over the completed frame; any authoritative effect they apply through the sanctioned seams is captured in the subsequent frame's snapshot. **No new `tick_order` key is introduced**; networking-last preserved; no thread created.
- **D-PL4 — Plugins never own state or events; faults are isolated (`10_Extensibility.md` §13/§20.2 + ADR-007).** Plugins register hooks and react to host-dispatched events (they do not own, emit, or reorder events); a plugin fault reported through the contract disables that plugin (Log → Disable → Continue) while the engine continues; no contract-reported fault becomes a C++ exception in the engine.
- **D-PL5 — API versioning follows the frozen model (`10_Extensibility.md` §16).** Plugins declare version + supported engine versions + required APIs + optional dependencies; compatibility is classified Fully Compatible / Compatible-with-Warnings / Migration Required / Unsupported before activation.

---

## 0.B Approval-Required register

The freeze-review pass (2026-07-20) checked each item against the frozen architecture (`10_Extensibility.md`), RFC-0007, the ADRs, and the scope authority. **Two items are resolved by existing frozen documentation and are removed from the open register (§0.B.1); two remain genuinely undecided and require an explicit project-owner decision (§0.B.2).** The plan cannot be frozen while any item in §0.B.2 is open.

### 0.B.1 Resolved from existing frozen documentation

- **[AR-P2] — Version-negotiation target → RESOLVED.** Plugins negotiate against the host **public extension-API version** and its declared **supported engine versions**, classified by the four-level compatibility model (Fully Compatible / Compatible-with-Warnings / Migration Required / Unsupported), evaluated **before initialization**; breaking API changes require a version increment + migration documentation + a deprecation period where practical. *Authority:* `10_Extensibility.md` §16 (esp. §16.3 version metadata — "API version, supported engine versions, compatibility level, deprecation status; extensions evaluate compatibility before initialization" — and §16.4 compatibility levels); `RFC-0007` §15 (Versioning). This settles D-PL5.
- **[AR-P4] — Native-fault-isolation policy → RESOLVED.** A plugin fault reported through the contract is a value outcome that is logged, disables the offending plugin, and lets the engine continue (Log → Disable → Continue), never corrupting World/ALife/Persistence/Networking/Scheduler; **hard native-crash (segfault) containment is explicitly out of scope** (best-effort only, and OS-level/process isolation is a scope-authority Out-of-Scope item). *Authority:* `10_Extensibility.md` §20.1/§20.2; `RFC-0007` §14 ("plugins should fail independently… where possible, failures should be logged and isolated"); ADR-007 (value outcomes in the engine-free core); Scope Authority §4 (Sandbox / OS-level security isolation — Out of Scope). This settles D-PL4.

### 0.B.2 Project-owner decisions (recorded 2026-07-20)

Both items below were decided by the project owner. Each keeps Sprint-014 strictly additive, introduces no new sanctioned seam and no new architecture, and matches the RFC's own deferral of the advanced capabilities to future evolution. **The open register is now empty; no Approval-Required item remains.**

- **[AR-P1] — Module-loading mechanism → RESOLVED (Owner decision: Option B).** Sprint-014 implements **static in-process plugin registration** behind the platform seam (`IPluginSource` / `PlatformPluginStore`); **native shared-library (DLL/module) loading is explicitly deferred to a future sprint.** ADR-009 confinement is preserved (the registration path is the only plugin-side platform TU). *Basis:* project-owner decision within `RFC-0007` §5 (loading/packaging is an implementation concern) and §16 (advanced loading deferred to future); ADR-009 (confinement).
- **[AR-P3] — Host-API surface → RESOLVED (Owner decision: Option A).** Plugins receive **only the existing sanctioned gameplay host-service facades** (World/Player/Entity/Environment/Inventory/Logging/Config, reused from Sprint-013 per D-PL2). **Administration, server information, command registration, diagnostics, and remote-management APIs are explicitly deferred** (no new seam is added this sprint). *Basis:* project-owner decision within `10_Extensibility.md` §17.3 and `RFC-0007` §16 (administration/remote APIs deferred to future); D-PL2 / ADR-008 (existing facades only).

**All Approval-Required items are resolved.** The plan is eligible for freeze.

---

## 1. Sprint objective

Implement the **Plugin Framework**: a controlled, host-side loader + lifecycle + management layer for compiled server-side plugins that integrate through a stable, engine-free contract, consume host services only through the existing public interfaces, react to host-dispatched events deterministically on the Simulation Thread, and isolate their faults so the authoritative simulation always continues. The engine remains authoritative and functions correctly with or without plugins.

## 2. Scope

**In scope.** A new engine-free `plugin::` namespace: value types & vocabulary; `PluginConfiguration`; the engine-free `IPlugin` contract (+ fake/null); `PluginContext` + `PluginRegistry`; the event/hook-binding registry; `PluginValidator` (version negotiation, dependencies, duplicate); the additive `IPluginSource` discovery read seam; the platform plugin backend (the single module-loading TU); the plugin host-service exposure seam (reusing the Sprint-013 public API); the plugin lifecycle state machine; `PluginLoader`; `PluginHost`; `PluginManager` (`IService` + `ITickable` at `kPlugins = 700`); fault isolation; diagnostics + statistics; composition-root + engine-adapter wiring; integration tests; and the subsystem doc `Multiplayer/docs/Plugins.md`.

## 3. Out-of-scope items

Hot reload, sandbox/OS-level security, marketplace/distribution, remote-administration transport, multiplayer plugin synchronization (§4 of the scope authority); any modification of prior frozen seams or the wire protocol; any new `tick_order` key; any thread creation; any networking/replication/persistence/prediction/ALife behavior change.

## 4. Design goals

Stable, documented, engine-free plugin contract; deterministic discovery/loading/lifecycle/dispatch; complete fault isolation (a failing plugin never destabilizes the engine); reuse of the existing subsystem seams and the Sprint-013 public API without redesign; an engine-free, loader-free, unit-testable core with module loading confined to one platform TU and engine facades confined to `EngineAdapters.cpp`; zero impact on the Sprint-013 baseline.

## 5. Architectural overview

The Plugin Framework is a **host-side** subsystem:

```
STARTUP (Bootstrap Initialize, before networking):
  PluginManager.DiscoverAndLoad ← IPluginSource.enumerate → validate (version/deps) →
     lifecycle: Discovered → Validated → Loaded → Initialized → Active
  PluginHost registers the public host-service surface for plugins

PER FRAME (reserved tick_order::kPlugins = 700; after Persistence 500, before Networking 900):
  PluginManager.Tick → dispatch OnTick to active plugins (Simulation Thread only)
     → plugin API calls read/effect change ONLY through sanctioned subsystem seams
     → authoritative effects captured in the SUBSEQUENT frame's snapshot (reactor placement)

EVENTS (host-dispatched, synchronous):
  subsystem raises OnPlayerJoin / OnEntitySpawn / OnWorldSaved / … → hook binding → active plugins
     → a contract-reported fault disables the plugin (Log → Disable → Continue)
```

Plugins never own events or authoritative state; they react through the contract and the public host services. Loading/registration happen once at startup; `OnTick` occupies the pre-reserved `kPlugins = 700` slot (no new key).

## 6. Component responsibilities

| Component | Responsibility |
| --- | --- |
| `plugin::PluginTypes` | Value vocabulary: `PluginOutcome`, `PluginId`, lifecycle-state & event-type enums, `PluginDescriptor` (manifest: version, supported engine versions, required APIs, optional deps), statistics/consistency. |
| `plugin::PluginConfiguration` (+ `FromConfig`) | `[plugins]` config: plugin directory token (opaque), max plugins, enable/validation flags, version. |
| `plugin::IPlugin` (+ fake/null) | The stable engine-free plugin contract: identity/version metadata, Initialize/Shutdown, event hooks — value outcomes only. |
| `plugin::PluginContext` + `PluginRegistry` | Per-plugin value context; deterministic registry (ascending ids, duplicate detection, enumerate). |
| `plugin::EventBinding` | Engine-free event→hook registry: deterministic dispatch order; plugins react, never own. |
| `plugin::PluginValidator` | Version negotiation, supported-engine-version check, required-API check, dependency resolution, duplicate detection → value outcomes. |
| `plugin::IPluginSource` (+ null/in-memory) | Additive engine-free discovery/read seam (enumerate / read manifest / exists). |
| `adapters::PlatformPluginStore` (platform TU) | The plugin discovery/registration backend over the configured directory token (static in-process registration per `[AR-P1]` Option B; dynamic library loading deferred). The only plugin-side platform TU (ADR-009). Null/in-memory in tests. |
| `plugin::` host-service surface | The public interfaces plugins consume — reusing the Sprint-013 Public API facades (D-PL2); engine impls in `EngineAdapters.cpp`. |
| `plugin::PluginLifecycle` | Deterministic state machine: Discovered → Validated → Loaded → Initialized → Active → Suspended → Shutdown → Unloaded. |
| `plugin::PluginLoader` | Compose `IPluginSource` + validation + contract load + registration in deterministic order. |
| `plugin::PluginHost` | Expose the host-service surface to plugins; hold the public API bundle. |
| `plugin::PluginManager` | Discover/load/unload, event/OnTick dispatch, failure isolation; `IService` + `ITickable` at `kPlugins = 700`. |
| `plugin::PluginDiagnostics` | Non-invasive collector + inspectors (loaded plugins, state inspector, counters). |
| `adapters` wiring | Composition-root wiring in `Bootstrap.cpp` (real plugin backend + host facades, `PluginManager` at `kPlugins`, reverse-order teardown); default `[plugins]` config block. |

## 7. Frame/startup placement (no new tick_order key)

- **Startup** (`PluginManager.DiscoverAndLoad`) runs once during `Bootstrap Initialize`, after the subsystems the host services read/write are constructed and started, and before networking accepts connections.
- **Per-frame** `OnTick` runs via the `PluginManager` at the **pre-reserved** `tick_order::kPlugins = 700` — after Persistence (500), before Networking (900). Plugins are reactors over the completed frame; authoritative effects apply through the sanctioned seams and are captured in the subsequent frame's snapshot. This is the **only** slot used; **no new `tick_order` key is introduced**; networking-last preserved; no thread created (D-PL3).

## 8. Boundaries

- **Single Engine Boundary** — the concrete host-service facade implementations (and any engine reads/writes) are added only in `EngineAdapters.cpp`, behind the engine-free facade seams (D-PL2). No engine header enters the `plugin::` core; no engine object is exposed to a plugin.
- **Single Platform Boundary** — the concrete plugin registration/discovery code is confined to `PlatformPluginStore.cpp` behind `IPluginSource` factories (D-PL1; static registration per `[AR-P1]` Option B). No OS/loader header enters the core.

## 9. Testing strategy

`Multiplayer/tests/PluginTests.cpp` accumulates per-step unit tests (config, types, registry, event binding, validation/versioning, lifecycle, loader, manager orchestration through the **fake plugin** + null/in-memory source and recording host facades) plus a `PluginIntegrationStep17` composed suite (server-lifecycle events, player/world/entity events, multiple plugins, dependency resolution, failure isolation) and Bootstrap wiring/ordering assertions (`PluginManager` subscribed once at `kPlugins = 700`; networking-last preserved). GCC engine-free build + MSVC. Every fault is a value outcome that isolates the plugin and leaves the engine running; determinism verified by twin-instance dispatch-order identity. The real module-loading backend + engine host facades are Antigravity-smoke-verified on Windows.

## 10. Evidence gates (must all pass by close)

- **E-G1-PL** — controlled interface: no internal engine object exposed to plugins; every plugin effect crosses into subsystems only through the sanctioned Sprint-002/003/005/007/008 seams (ADR-008).
- **E-G2-PL** — determinism & single-thread: deterministic discovery/dispatch order; `OnTick` at the reserved `kPlugins = 700`; Simulation-Thread-only execution; **no new `tick_order` key**; no thread created (ADR-011).
- **E-G3-PL** — fault isolation: initialization failure / invalid API use / missing dependency / contract-reported runtime error are value outcomes that disable the plugin; the engine continues; no exception escapes the contract boundary (ADR-007 / §20.2).
- **E-G4-PL** — Single Platform Boundary: the module-loading/OS code is confined to one platform TU behind engine-free seams (ADR-009); core loader-free.
- **E-G5-PL** — Preserve Before Replace: existing subsystem seams + the Sprint-013 public API reused unchanged; the reserved `kPlugins = 700` used (no new key); networking-last preserved; baseline 736/736 intact.

## 11. Definition of Done (from scope authority §12)

Plugins integrate only through the stable public interfaces; cannot bypass subsystem ownership; lifecycle and event dispatch function correctly; a failing plugin never destabilizes the engine; worker threads never execute plugins; the engine functions with or without plugins; ready for Diagnostics & Optimization. Recorded at closure (Step-18).

## 12. Step ordering (dependency chain)

`01 types → 02 config → 03 IPlugin contract (+fake/null) → 04 PluginContext + PluginRegistry → 05 event-binding registry → 06 PluginValidator (versioning + deps) → 07 IPluginSource read seam (+null/in-memory) → 08 platform plugin backend (PlatformPluginStore) → 09 host-service exposure seam → 10 plugin lifecycle state machine → 11 PluginLoader → 12 PluginHost → 13 PluginManager → 14 fault isolation → 15 diagnostics + statistics → 16 thread-safety enforcement → 17 composition-root + engine-adapter wiring + integration + Plugins.md → 18 sprint closure.`

Each step is independently buildable (clean compile, tree green after each) and independently verifiable (its own tests), strictly additive.

### Dependency graph (edges = "depends on")

```
01 ← 02,03,04,05,06,09,10
03 ← 06,11,12,13
04 ← 05,06,10,11,13
05 ← 13,17
06 ← 11,13
07 ← 08,11        08 ← 17(wiring)
09 ← 12(register),17(engine facades)
10 ← 11,13        11,12 ← 13
13 ← 14,15,16,17  14 ← 17
16 ← 17           17 ← 18
```
No cycles. Reused Sprint-002/003/005/007/008/013 seams are pre-existing inputs (not steps).

### 12.A Implementation batching strategy (proposed — freeze with the plan)

The 18 steps are frozen exactly as written once approved; this subsection defines only the **execution batching**. Batching combines steps only where it violates no dependency, crosses no architectural/ADR/verification boundary, keeps diagnostics non-invasive and hardening test-only, and preserves independent testability and failure isolation. The load-bearing gates (Step-08 platform boundary, Step-09 engine API boundary, Step-17 integration) remain isolated; Step-18 is the documentation-only closure batch.

| Batch | Steps | Isolation | Status | Nature of the batch |
|---|---|---|---|---|
| **B1** | 01 + 02 | — | ✅ Complete | Value types + `PluginConfiguration` (pure, engine-free/loader-free foundation). |
| **B2** | 03 + 04 | — | ✅ Complete | `IPlugin` contract (+ fake/null) + `PluginContext`/`PluginRegistry`. |
| **B3** | 05 + 06 | — | ✅ Complete | Event-binding registry + `PluginValidator` (versioning + deps). |
| **B4** | 07 | **Isolated** | ✅ Complete | `IPluginSource` discovery read seam (+ null/in-memory). |
| **B5** | 08 | **Isolated** | ✅ Complete | Platform plugin backend — **Single Platform Boundary / ADR-009** (only module-loading TU; Windows smoke). |
| **B6** | 09 | **Isolated** | ✅ Complete | Host-service exposure seam — **Single Engine Boundary / ADR-008** decision point. |
| **B7** | 10 + 11 | — | ✅ Complete | Plugin lifecycle + `PluginLoader` (engine-free consumers of the Step-07/09 seams). |
| **B8** | 12 + 13 + 14 | — | ✅ Complete | **Plugin Runtime Subsystem** — `PluginHost` + `PluginManager` + fault isolation, over the fake plugin + null source + recording host facades. |
| **B9** | 15 + 16 | — | ✅ Complete | Non-invasive diagnostics/statistics + thread-safety enforcement. |
| **B10** | 17 | **Isolated** | ✅ Complete | Composition-root + integration — **Integration gate** (reused ADR-008 host facades + ADR-009 module-loading backend + `kPlugins = 700` ordering; Windows smoke). Plugin developer guide `Plugins.md` deferred to the follow-up documentation pass per the project-owner scope decision. |
| **B11** | 18 | **Isolated** | ✅ Complete | Sprint closure (documentation only; no code). |

All eleven batches (Steps 01–18) are implemented, independently Antigravity-verified, and complete; final baseline **806 / 806** tests passing (GCC + MSVC).

**Batching does not alter the dependency graph.** Within any multi-step batch, steps are implemented in their documented order.

---

## Step specifications

Each step: **Objective · Scope In/Out · FR · Evidence · Tests · Commit.** All engine-free/loader-free/OS-free unless the step names an existing boundary; ADR-007 value outcomes; additive.

### Step-01 — Plugin value types & vocabulary
- **Objective.** The engine-free vocabulary for the subsystem.
- **In.** `PluginTypes.h` (header-only): `enum class PluginOutcome : std::uint8_t { Ok, LoadFailed, VersionMismatch, ApiIncompatible, DuplicatePlugin, MissingDependency, InvalidHook, InitFailed, RuntimeError, PluginDisabled, NotFound }` (+ name); `enum class PluginState` (Discovered, Validated, Loaded, Initialized, Active, Suspended, Shutdown, Unloaded, Failed); `enum class PluginEvent` (the ten §7.6 events); `struct PluginId`; `struct PluginDescriptor` (manifest: version, supported engine versions, required-API list, optional deps, generation); statistics/consistency. Types only. **Out.** Logic.
- **FR.** POD/trivially-copyable value captures; deterministic layout; timing/memory isolated to diagnostic fields. **Evidence.** E-G2-PL groundwork. **Tests.** `PluginTypesStep1`. **Commit.** `docs(plugin): freeze Sprint-014 Step-01 spec (types)`.

### Step-02 — `PluginConfiguration`
- **Objective.** Parsed `[plugins]` config.
- **In.** `PluginConfiguration.h`/`.cpp`: opaque `pluginDirectoryToken`, `maxPlugins` (≥1), `enabled`/`validateOnLoad`/`strictApiVersion` (bools), `version` (≥1); `FromConfig` (missing → defaults; invalid → InvalidArgument). **Out.** Loader/filesystem access.
- **FR.** Cross-field validation; value outcomes; no OS/loader access in the core. **Evidence.** E-G4-PL groundwork. **Tests.** `PluginConfigStep2`. **Commit.** `docs(plugin): freeze Sprint-014 Step-02 spec (config)`.

### Step-03 — `IPlugin` contract (+ fake/null)
- **Objective.** The stable engine-free plugin contract (the concrete static registration is Step-17, `[AR-P1]` Option B).
- **In.** `IPlugin.h` (engine-free): identity/version metadata (`Descriptor()`), `Initialize(host) -> PluginOutcome`, `Shutdown()`, `OnEvent(PluginEvent, args) -> PluginOutcome`; a deterministic `FakePlugin` (records init/shutdown/events; scriptable outcomes) and `NullPlugin` for tests. No engine/loader headers. **Out.** Concrete static registration binding (Step-17).
- **FR.** Value-only surface; no engine type exposed; deterministic fake. **Evidence.** E-G1-PL/E-G4-PL. **Tests.** `PluginContractStep3`. **Commit.** `docs(plugin): freeze Sprint-014 Step-03 spec (contract)`.

### Step-04 — `PluginContext` + `PluginRegistry`
- **Objective.** The engine-free per-plugin context and deterministic registry.
- **In.** `PluginContext.h` (id, descriptor, state, bound hooks) + `PluginRegistry.h`/`.cpp`: Register/Unregister/Find/Enumerate (ascending id), duplicate-id detection → value outcome. Pure. **Out.** Loading/orchestration (Steps 11-13).
- **FR.** Deterministic enumeration; duplicate rejected; no engine/loader. **Evidence.** E-G2-PL. **Tests.** `PluginRegistryStep4`. **Commit.** `docs(plugin): freeze Sprint-014 Step-04 spec (registry)`.

### Step-05 — Event-binding registry
- **Objective.** The engine-free event→hook table (plugins react, never own).
- **In.** `EventBinding.h`/`.cpp`: Bind/Unbind; deterministic ascending dispatch order; `HooksFor(PluginEvent)`; no event emission from plugins. Pure. **Out.** Dispatch through the manager (Step-13); engine-raised events (Step-17).
- **FR.** Deterministic dispatch order; plugins cannot emit/reorder events; value outcomes. **Evidence.** E-G5-PL. **Tests.** `PluginEventBindingStep5`. **Commit.** `docs(plugin): freeze Sprint-014 Step-05 spec (events)`.

### Step-06 — `PluginValidator` (versioning + dependencies)
- **Objective.** Pure validators over a candidate plugin (`10_Extensibility.md` §16).
- **In.** `PluginValidator.h`/`.cpp`: version negotiation (candidate vs host; Fully Compatible / Warnings / Migration Required / Unsupported), supported-engine-version check, required-API availability, dependency resolution (present in registry), duplicate registration → `PluginOutcome`. Pure. **Out.** Loading orchestration (Step-11).
- **FR.** Deterministic; each rejection a value outcome; no mutation. **Evidence.** E-G3-PL/E-G5-PL. **Tests.** `PluginValidatorStep6`. **Commit.** `docs(plugin): freeze Sprint-014 Step-06 spec (validation)`.

### Step-07 — `IPluginSource` discovery read seam (+ null/in-memory)
- **Objective.** The additive engine-free discovery/read boundary (mirrors `IScriptSource`).
- **In.** `IPluginSource.h` (engine-free): `Enumerate() -> std::vector<PluginDescriptor>`, `ReadManifest(PluginId) -> Expected<PluginDescriptor>`, `Exists(PluginId) -> bool`; `InMemoryPluginSource` + `NullPluginSource`. No OS access. **Out.** Real module-loading backend (Step-08).
- **FR.** Value-outcome surface; deterministic enumeration (ascending). **Evidence.** E-G4-PL. **Tests.** `PluginSourceStep7`. **Commit.** `docs(plugin): freeze Sprint-014 Step-07 spec (source seam)`.

### Step-08 — Platform plugin backend (`PlatformPluginStore`) — **isolated (ADR-009)**
- **Objective.** The plugin discovery/registration backend, confined to ONE platform TU (static in-process registration per `[AR-P1]` Option B; dynamic library loading deferred).
- **In.** `adapters/PlatformPluginStore.cpp` (the sole plugin-side OS/loader TU) implementing `IPluginSource` over the configured `pluginDirectoryToken`; engine-free factory declarations in a header; null/in-memory counterpart for tests. OS/loader headers appear ONLY here. **Out.** Any engine access; any validation logic.
- **FR.** Deterministic enumeration; value outcomes; OS/loader confinement. **Evidence.** E-G4-PL (platform build smoke is Antigravity's on Windows). **Tests.** in-memory/null tests in the engine-free build; the real TU is Antigravity-verified. **Commit.** `docs(plugin): freeze Sprint-014 Step-08 spec (platform backend)`.

### Step-09 — Host-service exposure seam — **isolated (ADR-008)**
- **Objective.** The engine-free surface through which plugins observe/effect the world (D-PL2; `[AR-P3]` Option A — gameplay facades only).
- **In.** The plugin host-service seam, reusing the Sprint-013 Public API facades (World/Player/Entity/Environment/Inventory/Logging/Config) exactly; recording/fake facades for tests. Engine implementations added at Step-17 in `EngineAdapters.cpp`. Administration/tooling/server-information/command-registration surfaces are **deferred** (`[AR-P3]` Option A) — no new seam this sprint. **Out.** Engine implementations (Step-17); binding into the plugin (Step-12); administration/tooling APIs (deferred).
- **FR.** No engine type exposed; every authoritative effect maps to a sanctioned seam; value outcomes. **Evidence.** E-G1-PL. **Tests.** `PluginHostApiStep9`. **Commit.** `docs(plugin): freeze Sprint-014 Step-09 spec (host services)`.

### Step-10 — Plugin lifecycle state machine
- **Objective.** The deterministic per-plugin lifecycle (`10_Extensibility.md` §11.3).
- **In.** `PluginLifecycle.h`/`.cpp`: transitions Discovered → Validated → Loaded → Initialized → Active → Suspended → Shutdown → Unloaded with legal-transition enforcement → value outcomes. Pure. **Out.** Orchestration (Step-13).
- **FR.** Deterministic; illegal transitions rejected as value outcomes; no engine/loader. **Evidence.** E-G2-PL. **Tests.** `PluginLifecycleStep10`. **Commit.** `docs(plugin): freeze Sprint-014 Step-10 spec (lifecycle)`.

### Step-11 — `PluginLoader`
- **Objective.** Deterministic load composition.
- **In.** `PluginLoader.h`/`.cpp`: enumerate via `IPluginSource`, read manifest, validate (Step-06), load the contract (Step-03), register (Step-04) in deterministic order → value outcomes. Engine-free. **Out.** Manager/tick (Step-13).
- **FR.** Deterministic order; failure isolates the one plugin; value outcomes. **Evidence.** E-G3-PL. **Tests.** `PluginLoaderStep11`. **Commit.** `docs(plugin): freeze Sprint-014 Step-11 spec (loader)`.

### Step-12 — `PluginHost`
- **Objective.** Expose the host-service surface to plugins.
- **In.** `PluginHost.h`/`.cpp`: holds the public API bundle; provides the host handle passed to `IPlugin::Initialize`. Engine-free (holds seams). **Out.** Per-frame tick / event dispatch (Step-13).
- **FR.** Deterministic; value outcomes; no engine/loader header. **Evidence.** E-G1-PL. **Tests.** `PluginHostStep12`. **Commit.** `docs(plugin): freeze Sprint-014 Step-12 spec (host)`.

### Step-13 — `PluginManager`
- **Objective.** Plugin orchestration + the per-frame `OnTick` phase.
- **In.** `PluginManager.h`/`.cpp`: `DiscoverAndLoad`/`Unload`/tracking/failure isolation; dispatches events via `EventBinding` through the contract; `IService` + `ITickable` whose `Tick` dispatches `OnTick`. Runs at `tick_order::kPlugins = 700` (subscribed at Step-17). Engine-free orchestration. **Out.** Bootstrap wiring/engine facades (Step-17).
- **FR.** Deterministic; single-thread; a per-plugin fault isolates the plugin (engine continues); value outcomes. **Evidence.** E-G2-PL/E-G3-PL. **Tests.** `PluginManagerStep13`. **Commit.** `docs(plugin): freeze Sprint-014 Step-13 spec (manager)`.

### Step-14 — Fault isolation
- **Objective.** Harden the boundary against misbehaving plugins (D-PL4; policy resolved, §0.B.1).
- **In.** Boundary handling for init failure, invalid API usage, missing dependency, and contract-reported runtime error — each mapped to a `PluginOutcome` that disables the offending plugin (Log → Disable → Continue) while the engine continues; no C++ exception escapes the contract seam. Additive/test-driven over the fake plugin. **Out.** Hard native-crash containment (out of scope per §0.B.1 / Scope Authority §4).
- **FR.** Every contract-reported fault is a value outcome; engine continues; deterministic. **Evidence.** E-G3-PL. **Tests.** `PluginHardeningStep14`. **Commit.** `docs(plugin): freeze Sprint-014 Step-14 spec (fault isolation)`.

### Step-15 — Diagnostics + statistics
- **Objective.** A pure, non-invasive collector + inspectors.
- **In.** `PluginDiagnostics.h`: loaded-plugins / state-inspector snapshots; statistics (loaded, active, event dispatches, API calls, errors) — all diagnostic-only, `Reset`/`Snapshot`. **Out.** Any control-flow dependence on timing.
- **FR.** Non-invasive; monotonic counters + deterministic aggregates; timing/memory never gate control flow. **Evidence.** E-G2-PL. **Tests.** `PluginDiagnosticsStep15`. **Commit.** `docs(plugin): freeze Sprint-014 Step-15 spec (diagnostics)`.

### Step-16 — Thread-safety enforcement
- **Objective.** Enforce Simulation-Thread-only execution (scope §7.9 / ADR-011).
- **In.** A deterministic guard/assertion surface asserting plugin entry points run on the Simulation Thread and are never entered from a worker; documentation that snapshots remain the only worker channel. No thread created. **Out.** Any threading model change.
- **FR.** Deterministic guard; no thread; value outcomes. **Evidence.** E-G2-PL/E-G5-PL. **Tests.** `PluginThreadSafetyStep16`. **Commit.** `docs(plugin): freeze Sprint-014 Step-16 spec (thread safety)`.

### Step-17 — Composition-root + engine-adapter wiring + integration + `Plugins.md` — **isolated**
- **Objective.** Wire the engine-free plugin subsystem to the real module-loading backend, host facades, and frame at the composition root.
- **In.** `Bootstrap.cpp`: construct `PluginConfiguration`; bind the `IPluginSource` (static-registration backend per `[AR-P1]` Option B) + host facades behind factories (fake/in-memory in tests); construct `PluginHost`/`PluginManager`; `DiscoverAndLoad` at Initialize (before networking); subscribe `PluginManager` at the reserved `tick_order::kPlugins = 700`; reverse-order teardown; default `[plugins]` config block. `EngineAdapters.cpp`: the real host-service facade implementations over the sanctioned seams (the reused Sprint-013 gameplay facades per `[AR-P3]` Option A) and the static-registration backend; null/fake counterparts in tests. Integration tests. Author `Multiplayer/docs/Plugins.md`. **Out.** Behavior change; new features; administration/tooling APIs (deferred); dynamic library loading (deferred).
- **FR.** **No new `tick_order` key** (reserved `kPlugins = 700` used); networking-last preserved; existing ordering/services unchanged; engine code confined to `EngineAdapters.cpp`; module-loading code confined to `PlatformPluginStore.cpp`; no thread; reverse-order teardown. **Evidence.** E-G1-PL…E-G5-PL composed; engine/loader/platform build smoke is Antigravity's on Windows. **Tests.** `PluginIntegrationStep17` + Bootstrap wiring/ordering assertions (`PluginManager` subscribed once at `kPlugins`). **Commit.** `docs(plugin): freeze Sprint-014 Step-17 spec (wiring + integration + Plugins.md)`.

### Step-18 — Sprint closure (no code)
- **Objective.** Confirm DoD; freeze status; declare Sprint-015 readiness.
- **In.** Cross-check §11 acceptance criteria and §12 DoD against delivered, verified artifacts; record the final baseline (`736 + Sprint-014 additions`); set `Sprint-014-Plugin-System.md` to Implemented / Verified / Closed / Frozen (status + closure section); set this plan to EXECUTED & CLOSED; update `CURRENT_STATUS.md`, `SESSION_LOG.md`, README textual progress; consistency scan. **Out.** Any code/test change (except a genuine closure defect); any Sprint-015 work.
- **FR.** DoD satisfied & recorded; status consistent; Sprint-014 declared Closed/Frozen; Sprint-015 readiness stated. **Evidence.** All E-G*-PL recorded. **Commit.** `docs(plugin): close Sprint-014`.

---

## Verification checkpoints (one per batch)

Each **batch** (§12.A) is a verification boundary: implement the batch's step(s) in their documented order → clean Release|x64 build (GCC engine-free + MSVC) with zero warnings, all step tests green, baseline preserved → Antigravity verification → git commit → git push → next batch. Every underlying step remains independently buildable and independently testable; batching only groups the verification cadence and never relaxes a step's own requirements.

Load-bearing gates are verified **in isolation** (never batched across): **Step-08 / Batch B5** (platform module-loading boundary / ADR-009), **Step-09 / Batch B6** (host-service engine boundary / ADR-008), **Step-17 / Batch B10** (integration — real backend/facades; reserved `kPlugins = 700` ordering). **Step-18 / Batch B11** is the documentation-only closure boundary.

## Testing strategy (summary)

Per-step unit tests in `PluginTests.cpp`; deterministic registry/event/dispatch ordering; validation/versioning/lifecycle/fault negatives as value outcomes; orchestration through the fake plugin + null/in-memory source + recording host facades (no engine, no loader); integration (`PluginIntegrationStep17`) over the composed manager; Bootstrap wiring/ordering assertions. The real module-loading backend + engine host facades are Antigravity-smoke-verified on Windows.

## Documentation update sequence

- Per step: README progress marker + running test count (Steps 01-17).
- Step-17: author `Multiplayer/docs/Plugins.md` (plugin interface, lifecycle, thread rules, supported events, versioning + dependencies, best practices).
- Step-18: `Sprint-014-Plugin-System.md` status + closure section; this plan → EXECUTED & CLOSED; `CURRENT_STATUS.md`; `SESSION_LOG.md`; README textual status.

## Acceptance criteria (from scope authority §11)

Plugin interface defined & stable · Plugin Manager complete · Discovery + loading operational · Lifecycle operational · Event dispatch operational · Validation + version negotiation operational · Thread safety verified · Failure isolation verified · Diagnostics operational · Tests passing · Documentation updated — each confirmed at Step-18.

## Definition of Done (from scope authority §12)

Plugins integrate only through the stable public interfaces; cannot bypass subsystem ownership; lifecycle + event dispatch function correctly; a failing plugin never destabilizes the engine; worker threads never execute plugins; the engine functions with or without plugins; ready for Diagnostics & Optimization — all recorded at closure.

## Sprint closure checklist (Step-18)

1. All 18 steps implemented, each Antigravity-verified, tree buildable after each.
2. Final baseline recorded (`736 + Sprint-014 additions`), GCC + MSVC, zero warnings, no regressions.
3. Evidence gates E-G1-PL…E-G5-PL recorded PASSED.
4. Boundaries confirmed: engine code only in `EngineAdapters.cpp`; module-loading only in `PlatformPluginStore.cpp`; **no new `tick_order` key (reserved `kPlugins = 700` used)**; networking-last preserved; no thread; wire protocol untouched.
5. Preserve Before Replace confirmed: Sprint-002/003/005/007/008 seams + the Sprint-013 public API reused unchanged; no internal engine object exposed to plugins.
6. ADR-007…ADR-011 preserved; no ADR reinterpreted.
7. `Sprint-014-Plugin-System.md` → Implemented / Verified / Closed / Frozen; this plan → EXECUTED & CLOSED; status docs + README synchronized.
8. Sprint-015 (Diagnostics & Optimization) readiness stated.

## Compatibility & non-regression checklist (every step)
- Sprint-013 **736/736** baseline preserved; new tests only add.
- Engine headers only in `EngineAdapters.cpp`; OS/loader headers only in `PlatformPluginStore.cpp`; no wire change; no thread; no new `tick_order` key (reserved `kPlugins = 700`).
- Existing subsystem seams + the Sprint-013 public API reused unchanged; plugins own no authoritative state or events.
- Reads via sanctioned read seams; authoritative effects only through sanctioned write seams; Simulation-Thread-only execution; ADR-007…ADR-011 preserved.

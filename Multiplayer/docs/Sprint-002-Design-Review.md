# Sprint-002 — Design Review (Revision 2)

Status: awaiting final approval. No implementation code exists.

Revision 2 changes:
- WorldContext contains only world-intrinsic state; **Sprint-002 §7.4 has
  been corrected in the specification itself** (no deviation record — the
  Sprint document is the implementation plan and now matches the
  Architecture, per the project's documentation hierarchy).
- Frame integration re-evaluated; **final recommendation: observer
  registration via `Device.seqFrame`** (analysis in §7).

## 1. Objectives

Introduce the World subsystem (namespace `stalkermp::world`) as the
authoritative gateway to the Zone: module + manager + deterministic
lifecycle, immutable per-tick WorldContext, simulation clock and world
time, environment read-access, entity and spatial query interfaces,
configuration, service registration, diagnostics, tests. Out of scope:
Bubble Manager (004), entity registry ownership (003), ALife (005),
networking, persistence, player management.

## 2. New classes

| Class | Kind | Responsibility |
|---|---|---|
| `core::FrameDispatcher` | infrastructure (core) | Module-internal frame fan-out: ordered tick subscribers; the single consumer of the engine frame bridge. Future sprints (networking, replication, persistence, diagnostics) subscribe here — never in engine code |
| `WorldModule` | `IModule` impl | Attaches to the declared "World" module slot |
| `WorldManager` | `IService` impl | Lifecycle state machine + world update pipeline; subscribes to FrameDispatcher |
| `WorldConfiguration` | value | `[world]` config: tick rate, simulation speed, day length, debug flags |
| `SimulationClock` | owned value | Deterministic simulation time; advances only via `Update(dt)`, speed-scaled |
| `WorldTime` | value | Time-of-day derivation (simulation time + day length) |
| `EnvironmentState` | value | Weather/lighting/wind/emission snapshot from adapters |
| `WorldContext` | immutable value | Per-tick snapshot (§10) |
| `WorldStatistics` | value | Tick duration, query counters, entity counts |
| `EngineFrameBridge` | adapter | `pureFrame` implementation registered in `Device.seqFrame`; reads `Device.fTimeDelta`, forwards to FrameDispatcher |
| `EngineEntitySource` | adapter | Wraps existing engine entity lookup behind `IEntitySource` |
| `EngineEnvironmentSource` | adapter | Reads engine weather/time state |

Adapter files are the only translation units including engine headers;
they compile with engine include paths (per-file vcxproj settings) and are
excluded from the test target. Test doubles: `FakeEntitySource`,
`FakeEnvironmentSource`, `ManualFrameDriver` (drives FrameDispatcher in
tests without any engine).

## 3. Interfaces

| Interface | Surface |
|---|---|
| `IWorld` | `State()`, `Context()` — primary world service |
| `IWorldQueries` | `FindEntity(name)`, `GetEntity(id)`, `GetEntities(filter)`, `EntityExists(id)`, `FindByID(id)` |
| `ISpatialQueries` | Radius / bounding-box / sphere queries, position lookup, nearest entity |
| `IEnvironmentService` | Read-only weather, time, lighting, wind, emission state |
| `IClockService` | Read-only tick index, delta, simulation time, time-of-day |
| `IEntitySource` | Seam for queries: engine adapter now; Entity Registry (Sprint-003) later, unchanged API |

All construction and wiring happens in the Bootstrap composition root via
constructor injection; `ServiceRegistry::Find` remains composition-root-only.

## 4. Ownership

```
Bootstrap (composition root — extended, not redesigned)
  └── Runtime
        ├── FrameDispatcher            (new core infrastructure, owned by Runtime)
        ├── ServiceRegistry ──owns──> WorldManager (IService)
        │                               ├── owns SimulationClock, WorldTime
        │                               ├── owns EnvironmentState, WorldStatistics
        │                               └── publishes WorldContext (immutable)
        ├── ModuleRegistry  ──owns──> WorldModule ("World" slot)
        └── EngineFrameBridge          (adapter, owned by Runtime;
                                        registered in Device.seqFrame)

Device.seqFrame ──observes(REG_PRIORITY_LOW)──> EngineFrameBridge
EngineFrameBridge ──forwards dt──> FrameDispatcher ──ordered──> WorldManager
WorldManager ──refs (injected, non-owning)──> IEntitySource, IEnvironmentSource
Adapters ──read-only──> engine state (never owned, never mutated)
```

World owns existence-coordination only (04_World §8). No player state
exists anywhere in the subsystem.

## 5. Lifetime

Runtime construction (Sprint-001 pipeline, extended): adapters →
FrameDispatcher → WorldManager (injected) → registered → `InitializeAll()`.
Engine registration is the **last** initialization step: the bridge is
added to `Device.seqFrame` only after the world is ready to tick.
Shutdown is strictly reverse: **first** `Device.seqFrame.Remove(bridge)`
(no engine callback can arrive afterwards), then services shut down in
reverse order, then Runtime destruction. Lifecycle state machine:
`Created → Initialized → Running ⇄ Paused → Shutdown`; illegal transitions
rejected via `Expected` and asserted in debug.

## 6. Update order (per engine frame)

```
CRenderDevice::FrameMove
  └── Device.seqFrame.Process
        ├── engine/gameplay handlers        (REG_PRIORITY_NORMAL / HIGH)
        └── EngineFrameBridge.OnFrame       (REG_PRIORITY_LOW — runs last)
              └── FrameDispatcher.Dispatch(Device.fTimeDelta)
                    └── WorldManager.Tick(dt)
                          1. SimulationClock.Advance(dt × speed)  (0 while Paused)
                          2. WorldTime → time-of-day
                          3. EnvironmentState refresh (adapters, read-only)
                          4. Statistics update
                          5. Publish new immutable WorldContext
```

`CRegistrator` sorts handlers by descending priority, so LOW placement
after all gameplay is an engine-enforced guarantee, not a code-position
accident. The published context therefore always describes a completed
frame (02_Engine §12.2). FrameDispatcher subscribers carry explicit order
keys, so future systems get deterministic intra-module ordering
(world → registry → bubble → … → snapshot) defined in one place.

## 7. Frame integration — final recommendation

**Recommendation: Observer registration via `Device.seqFrame` (adopted).**

Verified mechanics: `pure.h` defines `pureFrame` (virtual `OnFrame()`),
`CRenderDevice::seqFrame` is a `CRegistrator<pureFrame>` processed inside
`FrameMove`; `Add(obj, priority)` / `Remove(obj)`; descending-priority
ordering places `REG_PRIORITY_LOW` after all engine gameplay handlers.

Comparison against the direct hook (one call added in `FrameMove`):

| Criterion | Observer (seqFrame) | Direct hook |
|---|---|---|
| Maintainability | Engine frame code never edited again; entry point is data (a registration) | One-time edit, but it is a foreign line in engine source that every engine update/merge must preserve |
| Scalability | All future systems subscribe to FrameDispatcher; zero engine growth | Same internal dispatch is possible, but the entry remains a hardcoded engine edit |
| Coupling | Module→engine, confined to the adapter layer (declared acceptable; matches the existing EngineEntitySource pattern) | Engine→module (engine source names `stalkermp`); inverse of the layering in 02_Engine §16 |
| Testing | FrameDispatcher + WorldManager fully testable via ManualFrameDriver; bridge excluded from test target (pattern already required for the other adapters — no *new* cost) | Module stays engine-header-free, marginally simpler test build |
| Future sprint impact | None on engine frame code, ever | None *if* dispatcher-based, but the precedent invites per-sprint engine edits |
| Engine modifications over project lifetime | Frame loop: **zero**. Total engine footprint stays at the Sprint-001 three files | Frame loop: one edit now; risk of accretion later |
| Spec alignment | Tier 3 of the 02_Engine §9 integration ladder (observer/event hooks) | Tier 5 (localized modification — "last resort") |

My earlier direct-hook recommendation rested on avoiding engine headers in
the module and test-build simplicity. Both concerns are void: the adapter
layer already accepts engine headers by design (EngineEntitySource), and
adapter exclusion from the test target was already required. With those
removed, the observer approach wins on every remaining criterion and sits
higher on the architecture's own integration-preference ladder. One
residual caveat, documented: ordering among *equal-priority* seqFrame
registrants is registration-order dependent — irrelevant while STALKER-MP
registers exactly one LOW-priority handler, and internal ordering is owned
deterministically by FrameDispatcher.

Engine-source diff for Sprint-002: **zero lines** (the Sprint-001
Initialize/Shutdown hooks in `x_ray.cpp` remain the module's only engine
edits).

## 8. Testing strategy

GoogleTest additions (~22): FrameDispatcher (subscriber ordering by key,
dt forwarding, unsubscribe safety); lifecycle state machine incl. illegal
transitions; clock determinism (fixed dt sequence ⇒ identical simulation
time; pause freezes; speed scaling); WorldContext snapshot semantics
(immutability, tick monotonicity); query correctness via FakeEntitySource;
spatial math edge cases (empty world, boundary inclusion, nearest tie);
configuration parsing incl. defaults/invalid values; composition-root
wiring. Engine-bound bridge/adapters are covered by MSVC runtime
verification (log line proves per-frame ticking).

## 9. Risks

Bridge registration lifetime is the top risk (a dangling `pureFrame`
pointer after shutdown would crash the engine) — mitigated by the strict
register-last/remove-first rule in §5 and a debug assertion that the
bridge is unregistered before destruction. Engine-header compilation
context for adapter TUs (per-file include dirs + defines) — mitigated by
compiling adapters inside xrMP with dedicated vcxproj settings and keeping
them out of the test target; verified on the first MSVC build. Equal-
priority ordering caveat (documented, currently moot). Scope creep toward
Sprint-003 (IEntitySource exposes only what the engine already provides).
Performance: tick work is O(subscribers) + O(1) world update; statistics
measure tick duration from day one.

## 10. Final WorldContext definition

```cpp
// Immutable snapshot of deterministic, world-intrinsic simulation state.
// Published once per world tick by WorldManager. Contains NO player,
// networking, or session state (see Sprint-002 §7.4 note; player influence
// reaches world machinery only through the Bubble Manager input,
// Sprint-004 / RFC-0002).
struct WorldContext
{
    std::uint64_t  tick;              // Monotonic world tick index.
    double         deltaSeconds;      // Wall-clock delta for this tick.
    double         simulationSeconds; // Accumulated simulation time (speed-scaled).
    WorldTimeOfDay timeOfDay;         // Derived in-world time of day.
    WeatherId      weather;           // Active weather profile identifier.
    bool           emissionActive;    // Global emission state.
};
```

## 11. FrameDispatcher Ordering Contract

**This section is the authoritative frame-integration reference for every
future sprint.**

### Responsibilities

FrameDispatcher owns **execution ordering** of STALKER-MP per-frame work.
The engine owns **scheduling**: when a frame happens, how long it takes,
and when the module's single `pureFrame` observer is invoked.
FrameDispatcher never creates threads, never controls frame timing, never
sleeps, and never defers work — it only dispatches callbacks synchronously
inside the engine frame in which they were triggered, on the engine's main
thread. Worker-thread execution (RFC-0006) is a separate mechanism and is
never routed through FrameDispatcher.

### Subscriber ordering policy

Every subscriber registers with an explicit ordering key from the central
`tick_order` table (FrameDispatcher.h). Lower keys run earlier. Duplicate
keys are rejected at registration (`AlreadyExists`) — ordering is total,
deterministic, and centrally owned:

| Order key | System | Sprint |
|---|---|---|
| 100 | World | 002 |
| 200 | Entity Registry | 003 |
| 300 | Bubble Manager | 004 |
| 400 | Replication | 008 |
| 500 | Persistence | 011 |
| 600 | Diagnostics | 015 |
| 700 | Plugins | 014 |

Gaps are reserved for insertion; new slots are allocated only in this
table, in this document, together with the sprint that introduces them.

### Rules (binding on all future sprints)

- Ordering is deterministic and centrally owned by the `tick_order` table.
- No subsystem may assume or depend on *registration* order — only on its
  assigned order key.
- No subsystem may bypass FrameDispatcher: exactly one engine frame
  observer exists (`EngineFrameBridge`); registering additional
  `Device.seqFrame` handlers or adding engine frame-loop calls is an
  architecture violation.
- Subscribers must not subscribe/unsubscribe from inside `Dispatch`
  (asserted).
- A subscriber that needs sub-ordering owns it internally; FrameDispatcher
  keys order subsystems, not functions.

### How future systems register

At the Bootstrap composition root, immediately after service registration:

```cpp
// Sprint-00N composition root extension:
frameDispatcher.Subscribe(*subsystem, core::tick_order::kSubsystem, "Name");
```

Registration happens only in Bootstrap; shutdown unsubscribes in reverse
before service teardown.

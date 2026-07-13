# World Subsystem (Sprint-002)

Namespace `stalkermp::world`. Design authority:
`docs/Sprint-002-Design-Review.md` (incl. §11 FrameDispatcher Ordering
Contract) and `Documentation/Architecture/04_World.md`.

## Ownership

The World owns existence-coordination only: lifecycle, deterministic
simulation clock, the published `WorldContext`, environment snapshot, and
the query surface. It owns no entities (Sprint-003), no player state
(player influence arrives via the Bubble Manager input, Sprint-004), no
engine state (adapters observe read-only).

## Public interfaces

| Interface | Implemented by | Purpose |
|---|---|---|
| `IWorld` | `WorldManager` | Lifecycle state + latest immutable `WorldContext` |
| `IClockService` | `WorldManager` | Tick index, simulation seconds, time of day |
| `IWorldQueries` | `WorldQueryService` | Entity lookup by id/name/predicate |
| `ISpatialQueries` | `WorldQueryService` | Radius/box/nearest/position queries |
| `IEnvironmentService` | `EnvironmentService` | Weather, time of day, emission state |
| `IEntitySource` | engine adapter now; Entity Registry in Sprint-003 | The seam queries operate over |
| `IEnvironmentSource` | engine adapter | Raw weather/emission sampling |

Services are registered at the Bootstrap composition root with constructor
injection; `Find<T>()` remains composition-root-only.

## Lifecycle

`Created → Initialized (ServiceRegistry) → Running (Start) ⇄ Paused →
ShutDown`. Illegal transitions return `InvalidArgument`; `Start` requires
`Initialized`, `Resume` requires `Paused`. While paused, ticks continue
(fresh contexts) but simulation time is frozen.

## Frame integration

One engine observer exists: the frame bridge registered in
`Device.seqFrame` at `REG_PRIORITY_LOW` (after all gameplay handlers),
created as the LAST initialization step and destroyed as the FIRST
shutdown step. It feeds `core::FrameDispatcher`, which dispatches
subscribers in `tick_order` (World = 100). No subsystem may register its
own engine hook — see the binding contract in the design review §11.

## Adapter layer

`src/adapters/EngineAdapters.cpp` is the only translation unit including
X-Ray headers (compiled with engine include paths, xrMP build only). The
test build links `tests/support/NullEngineAdapters.cpp` — identical factory
symbols, no engine. Composition-root code is byte-identical in both builds.

## Extension points

Sprint-003 replaces the entity source behind `IEntitySource` (query API
unchanged). Sprint-004 subscribes the Bubble Manager at `tick_order`
key 200/300 per the ordering table. Lighting/wind exposure on
`IEnvironmentService` is deferred until a consumer exists; emission state
is reported `false` by the engine source until Lua integration
(Sprint-013) supplies it — both recorded limitations, not stubs.

## Configuration

`server.cfg` `[world]`: `simulation_speed` (>0), `day_length_minutes`
(>0), `debug_logging` (bool). Missing keys keep defaults; invalid values
fail initialization.

## Diagnostics

`WorldManager::Statistics()` (ticks, last/max tick ms — logged at
shutdown), `WorldQueryService::Statistics()` (entity/spatial query
counters — logged at shutdown), per-tick verbose logging via
`debug_logging`.

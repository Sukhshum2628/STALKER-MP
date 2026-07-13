# Changelog

All notable changes to STALKER-MP are documented here, one section per
sprint. The documentation under `Documentation/` remains the authoritative
specification.

## Sprint-002 — World Abstraction Layer (implementation complete; pending MSVC verification)

### Added

- FrameDispatcher (core infrastructure): single engine frame observer
  fanned out to subsystems in a deterministic, centrally owned order
  (`tick_order` table; authoritative contract in
  `Multiplayer/docs/Sprint-002-Design-Review.md` §11).
- World subsystem (`stalkermp::world`): WorldManager (lifecycle state
  machine, deterministic SimulationClock, immutable per-tick WorldContext),
  WorldModule (first implemented module), WorldConfiguration
  (`[world]` section in server.cfg), world time-of-day derivation.
- Query services: IWorldQueries + ISpatialQueries over the IEntitySource
  seam (engine adapter now, Entity Registry in Sprint-003), with query
  counters.
- EnvironmentService: read-only weather/time/emission from the world
  context; engine weather sampled per tick.
- Engine adapter layer: `EngineAdapters.cpp` (sole engine-header TU;
  frame bridge via `Device.seqFrame` at REG_PRIORITY_LOW, entity and
  environment sources) with null test-build counterparts.
- Diagnostics: tick duration statistics, query counters, debug logging.
- 31 new tests (77 total across 21 suites), green in debug and release.

### Notes

- Zero engine-source lines changed in Sprint-002 (observer-based frame
  integration; engine footprint remains the three Sprint-001 files).
- WorldContext contains only deterministic world-intrinsic state; the
  Sprint-002 specification §7.4 was corrected to match the Architecture.
- Known limitations: emission state reported false until Lua integration
  (Sprint-013); lighting/wind exposure deferred until a consumer exists;
  spatial queries are linear scans (optimize when measured).

## Sprint-001 — Core Project Skeleton

### Added

- Bootstrap system — ordered initialization pipeline (logging →
  configuration → engine compatibility → service registry → module
  registration) with strict reverse-order shutdown and full failure
  isolation from the engine.
- Logging framework — severities (Verbose–Error), subsystem categories,
  timestamps, console and file sinks, thread-safe writes.
- Configuration system — INI-style loader with typed accessors,
  load-or-create defaults (`server.cfg`, `client.cfg`, `development.cfg`),
  and schema versioning (`[meta] config_version`).
- Assertion framework — `SMP_ASSERT` / `SMP_VERIFY` / `SMP_FATAL` with
  pluggable failure handler; captures expression, message, file, function,
  line.
- Error handling — `ErrorCode` / `Error` / `Expected<T>` result types for
  all fallible operations.
- Version system — project/compatibility versions, build timestamp, git
  revision stamping, engine compatibility verification.
- Service Registry — composition-root utility: ownership, dependency
  validation, deterministic init/shutdown ordering.
- Module Registry — declarations for the seven planned modules (World,
  Player, Replication, Persistence, Networking, Plugins, Diagnostics).
- Base interfaces — IInitializable, ITickable, ISerializable (provisional),
  ISubsystem, IService, IManager, IModule.
- Utility library — FNV-1a hashing, UUIDv4, string utilities and `{}`
  formatting, timing/timestamps, filesystem helpers, math helpers.
- GoogleTest integration — vendored 1.11.0, 46 unit tests across 12 suites.

### Engine Integration

- xrEngine bootstrap hook after `InitEngine()` with staged startup logging
  and a guarded `$app_data_root$` lookup; failure disables multiplayer and
  the game continues as single-player.
- Shutdown hook before `destroyEngine()`.
- MSVC solution integration: `xrMP` static library registered in
  `engine-vs2022.sln` with full configuration mapping; three engine files
  touched in total.

### Documentation

- Coding standards (C++17 profile, composition-root rule).
- Contributing guide (workflow, branch strategy, definition of done).
- Architecture deviation record (`Multiplayer/docs/Decisions.md`).
- Sprint decisions, verification checklist, module README.

### Notes

- C++17 instead of C++20 — approved deviation D-002 (engine toolchain).
- MSVC solution instead of CMake — approved deviation D-001.
- GoogleTest vendored at 1.11.0 (D-005); custom interim framework removed.
- Legacy `engine.sln` is not updated (D-004); use `engine-vs2022.sln`.
- Log rotation deferred to Sprint-015.
- Sprint-001 is FROZEN; only genuine build/runtime bugs may be fixed.

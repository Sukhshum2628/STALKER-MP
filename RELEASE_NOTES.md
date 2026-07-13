# STALKER-MP — Sprint-001 Release Notes (Milestone M1)

Sprint-001 delivers the production foundation for the STALKER-MP
persistent-multiplayer framework. It contains **no gameplay or networking
functionality by design** — it is the skeleton every later sprint builds
on, integrated into the Anomaly engine without altering engine behavior.

## Major systems

The `Multiplayer/` module (`stalkermp` namespace, C++17, static library
`xrMP`) provides: an ordered, failure-isolated bootstrap pipeline; a
thread-safe categorized logger with console/file sinks; a versioned
INI-style configuration system that generates documented defaults on first
run; an assertion framework with pluggable failure handling; `Expected<T>`
based error handling; a version/compatibility system; a service registry
(composition-root pattern — ownership, dependency validation, deterministic
ordering); a module registry declaring the seven planned modules; the seven
base interfaces; and a small utility library (hashing, UUID, strings,
timing, filesystem, math).

## Engine integration

Three engine files were touched, all localized: `engine-vs2022.sln`
(project registration), `xrEngine.vcxproj` (link + include path), and
`x_ray.cpp` (+53 lines: initialization after `InitEngine()` with staged
startup logging and a guarded `$app_data_root$` lookup; shutdown before
`destroyEngine()`). Every failure path disables the framework, logs the
reason, and leaves the game running as unmodified single-player Anomaly.
Runtime data lives under the engine's app-data root in `stalkermp/config`
and `stalkermp/logs`.

## Testing status

46 GoogleTest tests across 12 suites (vendored GoogleTest 1.11.0), covering
bootstrap lifecycle, configuration parsing and version policy, registries,
logging, error types, version checks, and utilities. Verified green in both
debug and optimized/NDEBUG builds with warnings-as-errors on GCC;
Windows/MSVC verification runs per
`Multiplayer/docs/Sprint-001-Verification.md`.

## Requirements

Visual Studio 2022 (v143), x64, X-Ray Monolith v1.5.x (Anomaly) source at
`Engine/xray-monolith`. Build via `engine-vs2022.sln`; tests via
`Multiplayer/xrMP.sln`.

## Known issues and limitations

No multiplayer features yet; legacy `engine.sln` will not link xrEngine
(use the vs2022 solution); log rotation deferred to Sprint-015;
`ISerializable` is provisional until Sprint-008/011. Full deviation record:
`Multiplayer/docs/Decisions.md`.

## Future work

Sprint-002 (World Abstraction Layer) introduces the first gameplay-facing
subsystem: the World module responsible for entity ownership, world
queries, and the foundation for ALife and multiplayer integration. It
begins only after the M1 verification sign-off and an approved
implementation strategy.

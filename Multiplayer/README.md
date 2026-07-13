# STALKER-MP Module (`Multiplayer/`)

The multiplayer framework for the X-Ray Engine (S.T.A.L.K.E.R. Anomaly, `Engine/xray-monolith`).
Specification: `Documentation/` (Architecture, RFCs, Sprints). Current state: **Sprint-002 — World Abstraction Layer** (implementation complete, pending MSVC verification; Sprint-001 frozen as milestone M1).

## Layout

| Path | Purpose |
|------|---------|
| `include/stalkermp/` | Public headers. `StalkerMP.h` is the only header the engine includes. |
| `include/stalkermp/core/` | Bootstrap-level systems: logging, assertions, errors, config, registries, frame dispatcher, version, base interfaces. |
| `include/stalkermp/world/` | World subsystem (Sprint-002): lifecycle, clock, context, queries, environment (see `docs/World.md`). |
| `include/stalkermp/adapters/` | Engine adapter factories (engine impl in `src/adapters/`, null impl in `tests/support/`). |
| `include/stalkermp/common/` | Utility library: hashing, UUID, string, time, filesystem, math. |
| `src/` | Implementation (mirrors `include/`). |
| `tests/` | GoogleTest unit tests (framework vendored in `third_party/googletest`). |
| `third_party/` | Vendored dependencies: GoogleTest 1.11.0 (see `third_party/README.md`). |
| `docs/` | Module documentation: decisions, coding standards, contributing. |
| `scripts/` | Build helper scripts (git revision stamping). |
| `xrMP.vcxproj` | Static library, part of the engine solution. |
| `xrMP.sln` | Standalone module solution (xrMP + xrMP_Tests) for development and testing. |

## Namespaces

Root namespace `stalkermp`. Active: `core`, `common`. Reserved for later sprints
(per Sprint-001 §7.2): `world`, `network`, `player`, `alife`, `replication`,
`persistence`, `threading`, `plugins`.

## Building

**With the engine:** open `Engine/xray-monolith/src/engine-vs2022.sln` and build as usual.
`xrMP` builds as a static library and links into `xrEngine`. The legacy `engine.sln`
(VS2015-era) was intentionally not modified; use the vs2022 solution.

**Tests:** open `Multiplayer/xrMP.sln`, build `xrMP_Tests` (Debug or Release, x64), run the
resulting console executable (`Multiplayer/_build/tests/<Config>/xrMP_Tests.exe`). All tests
must pass before merging.

## Runtime behavior (Sprint-001)

On engine start (`x_ray.cpp`, after `InitEngine()`), `stalkermp::Initialize` runs:
logging → configuration → engine compatibility check → service registry → module registration.
Config and logs live under the engine's `$app_data_root$\stalkermp\` (`config\`, `logs\`).
Default `server.cfg`, `client.cfg`, `development.cfg` are generated on first run.
A failed initialization is logged and the engine continues as single-player Anomaly.

## Rules

The documentation under `Documentation/` is the specification and has higher authority
than code. Approved deviations are recorded in `docs/Decisions.md`. See
`docs/CodingStandards.md` and `docs/Contributing.md` before making changes.

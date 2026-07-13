# STALKER-MP — Version and Milestone Status

| Field | Value |
|---|---|
| Current milestone | **M1 — Core Project Skeleton** |
| Project version | 0.1.0 (compatibility version 1) |
| Current sprint | Sprint-001 — CLOSED (frozen) · Sprint-002 — in planning |
| Repository status | Sprint-001 feature complete, revision-1 applied, pending final Windows/MSVC verification sign-off (`Multiplayer/docs/Sprint-001-Verification.md`) |
| Supported engine | X-Ray Monolith v1.5.x (S.T.A.L.K.E.R. Anomaly, `Engine/xray-monolith`), MSVC v143, x64 |

## Known limitations

- No multiplayer functionality yet — Sprint-001 delivers infrastructure
  only (bootstrap, logging, configuration, registries, interfaces, tests).
- Host machine only; no world, networking, replication, or persistence
  systems exist.
- Legacy `engine.sln` (VS2015-era) does not build the xrMP module; use
  `engine-vs2022.sln`.
- Log files grow without rotation (deferred to Sprint-015).
- `ISerializable` is provisional until Sprint-008/011.

## Approved deviations (full record: `Multiplayer/docs/Decisions.md`)

- D-001 — MSVC solution integration instead of CMake.
- D-002 — C++17 instead of C++20 until an engine toolchain migration.
- D-004 — legacy `engine.sln` intentionally not updated.
- D-005 — GoogleTest 1.11.0 vendored from the Debian/Ubuntu source package.
- D-006 — `ISerializable` signature provisional.

## Roadmap

| Sprint | Deliverable |
|---|---|
| 002 | World Abstraction Layer |
| 003 | Entity Registry Refactor |
| 004 | Bubble Manager |
| 005 | Online/Offline Transitions |
| 006 | Host Networking |
| 007 | Player Connections |
| 008–009 | Replication (snapshots, deltas) |
| 010 | Prediction & Interpolation |
| 011–012 | Persistence, Save/Load |
| 013 | Lua Integration |
| 014 | Plugin System |
| 015 | Optimization & Testing |

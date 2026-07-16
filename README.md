# STALKER-MP

**A host-authoritative, deterministic multiplayer framework for the X-Ray (S.T.A.L.K.E.R. Anomaly / Monolith) engine.**

![Architecture Overview](docs/images/architecture-overview.svg)

> _Diagrams are provided as SVG in `docs/images/` (rendered above) and as inline Mermaid diagrams below (rendered directly by GitHub)._

---

## Overview

STALKER-MP is a modular multiplayer framework layered **on top of** the X-Ray Monolith engine used by S.T.A.L.K.E.R. Anomaly. It adds host-authoritative networking, a persistent world, player lifecycle management, and an immutable snapshot pipeline — **without forking or rewriting the engine**. The framework compiles into a single static library (`xrMP.lib`) that links into the engine executable, and it is built sprint-by-sprint under a strict architecture-first workflow with independent verification at every step.

The project treats the engine as an upstream dependency and interacts with it through a single, tightly controlled boundary. Everything else — world abstraction, entity registry, activation ("bubbles"), ALife online/offline transitions, host networking, player lifecycle, and snapshots — is engine-free, deterministic, and unit-tested on both GCC and MSVC.

---

## Vision & Goals

- **Persistent, shared Zone.** A single authoritative world that multiple players inhabit, where offline/online simulation is driven by the host, not the client.
- **Host authority, always.** Clients request; the host decides. No client ever owns persistent world state.
- **Deterministic and replay-safe.** Identical inputs produce identical state, enabling reliable networking, persistence, and debugging.
- **Preserve before replace.** Reuse vanilla engine systems (ALife, spawning, navigation) rather than reimplementing them.
- **Incremental & verifiable.** Every subsystem is designed, frozen, implemented one step at a time, and independently verified before the next begins.

---

## Architecture Overview

STALKER-MP is a layered set of subsystems fanned out from a single engine frame callback through a deterministic dispatcher. Only one translation unit touches engine headers (the **One Engine Boundary**); only one touches OS sockets (the **One Platform Boundary**).

```mermaid
flowchart TD
    ENG[X-Ray Engine frame] --> BR[EngineFrameBridge - single seqFrame hook]
    BR --> FD[FrameDispatcher - deterministic ordering]
    FD --> W[World 100]
    FD --> ER[EntityRegistry 200]
    FD --> PL[PlayerLifecycle 250]
    FD --> BUB[Bubble 300]
    FD --> TR[ALifeTransition 350]
    FD --> SN[Snapshot 400]
    FD --> NET[Networking 900 - last]
    ADP[EngineAdapters.cpp - ONLY engine-touching TU] -.marshals engine-free values.-> ER
    ADP -.player spawn / entity capture.-> PL
    ADP -.player spawn / entity capture.-> SN
    SOCK[PlatformSockets.cpp - ONLY OS-socket TU] -.UDP.-> NET
```

**Deterministic per-frame pipeline** (ascending `tick_order` keys; networking is always last and owns no simulation):

```mermaid
flowchart LR
    W[World<br/>100] --> ER[EntityRegistry<br/>200] --> PL[PlayerLifecycle<br/>250] --> BUB[Bubble<br/>300] --> TR[Transition<br/>350] --> SN[Snapshot<br/>400] --> NET[Networking<br/>900]
```

See the SVG diagrams in `docs/images/` and the Mermaid pipeline above; per-subsystem documentation lives in `Multiplayer/docs/`.

---

## Core Principles

These invariants are enforced across every sprint and verified independently:

- **Preserve Before Replace** — reuse vanilla engine systems; never fork ALife or the spawn machinery.
- **Host Authority** — the host owns every state transition; clients issue requests only.
- **One World / One ALife Simulation** — a single authoritative simulation; no duplicate state.
- **Deterministic Simulation & Replay Determinism** — identical inputs → identical state and outputs.
- **One Engine Boundary** — engine headers appear in exactly one translation unit (`EngineAdapters.cpp`).
- **One Platform Boundary** — OS socket headers appear in exactly one translation unit (`PlatformSockets.cpp`).
- **Clients never own persistent world state.**
- **Incremental, frozen architecture** — architecture is frozen before implementation; each step is independently verified.

---

## Frozen ADR Summary

Architecture Decision Records are the durable, versioned source of truth (see `Documentation/Architecture/ADR/`). The currently binding decisions:

| ADR | Title | Essence |
|---|---|---|
| **ADR-007** | Engine ABI Contract (Exception & STL Policy) | Engine-linked TUs are exception-free, RTTI-free, iostream-free; fallible operations return `core::Expected<T>` / value outcomes; `/W4 /WX`, C4530-clean. |
| **ADR-008** | Engine State Mutation Boundary | Engine simulation state is mutated only through sanctioned, id-based cooperative paths behind a single gateway; observation is read-only. |
| **ADR-009** | One Platform Boundary | All OS socket/platform types are confined to one transport translation unit behind an engine-free seam. |
| **ADR-010** | Wire Protocol Contract | Fixed, field-by-field little-endian framing; control vs. data id ranges; additive, never-renumbered message ids. |
| **ADR-011** | Single-Threaded / Deterministic Networking | Networking is single-threaded and tick-derived; future worker threads consume only immutable snapshots, never live state. |

All framework code conforms to ADR-007 through ADR-011; no ADR has been superseded.

---

## Sprint Roadmap & Progress

![Sprint Roadmap](docs/images/sprint-roadmap.svg)

| Sprint | Subsystem | Status |
|---|---|---|
| **001** | Core (bootstrap, config, logging, service registry) | ✅ Verified (Closed) |
| **002** | World (FrameDispatcher, WorldManager, queries, environment) | ✅ Verified (Closed) |
| **003** | Entity Registry (identity/metadata store, engine feed) | ✅ Verified (Closed) |
| **004** | Bubble Manager (activation regions, hysteresis) | ✅ Verified (Closed) |
| **005** | ALife Transition Layer (online/offline switching) | ✅ Verified (Closed) |
| **006** | Host Networking Infrastructure (transport, session, wire) | ✅ Verified (Closed) |
| **007** | Player Lifecycle (persistent players, join/reconnect) | ✅ Verified (Closed) |
| **008** | Snapshot System (immutable snapshots) | ✅ Verified (Closed) |
| 009 | Replication Pipeline | ⏳ Planned (next) |
| 010 | Client Prediction / Interpolation | ⏳ Planned |
| 011–012 | Persistence (save format, reconnect persistence) | ⏳ Planned |
| 013–014 | Extensibility (Lua, plugin loader) | ⏳ Planned |
| 015 | Diagnostics / Optimization | ⏳ Planned |

**Sprint-008 (Snapshot System) — Verified (Closed):** the immutable, deterministic per-tick capture of world state that decouples asynchronous consumers (replication, persistence, replay) from live simulation. All 14 steps were implemented one at a time and independently verified. **Next focus — Sprint-009 (Replication Pipeline)**, which consumes these snapshots.

---

## Current Implementation Status

- **Sprints 001–008:** implemented, independently verified, and closed.
- **Sprint-008 (Snapshot System) — closed:** all 14 steps delivered and verified — value types + configuration; the immutable `SimulationSnapshot` + `ISnapshotView`; the exception-free fixed-capacity `SnapshotPool`; the additive engine-free `world::IEntitySnapshotSource` capture seam + null; the deterministic value-only `SnapshotBuilder`; the single-producer / multi-consumer `SnapshotQueue`; the per-tick `SnapshotManager` (`IService` + `ITickable`) at `tick_order::kReplication = 400`; the engine-boundary `adapters::EngineEntitySnapshotSource` (the sole new engine TU) with its engine-free marshaling helper; the Bootstrap composition-root wiring with reverse-order teardown; the read-only `SnapshotDiagnostics` inspector; the validation-hardening negative surface; and composed-stack integration with the subsystem documentation in `Multiplayer/docs/Snapshots.md`. Evidence gates E-G1-S / E-G2-S / E-G3-S passed, E-G4-S confirmed.
- **Engine boundary:** intact — engine headers confined to `Multiplayer/src/adapters/EngineAdapters.cpp`.
- **Platform boundary:** intact — OS socket headers confined to `Multiplayer/src/adapters/PlatformSockets.cpp`.

## Test Status

- **377 / 377 build tests passing** (Release x64) on **GCC** and **MSVC.** **Game testing has not started yet**.
- **0 errors, 0 warnings, no regressions.**
- Engine-free subsystems are fully unit-tested with mock/loopback/null substrates (no engine, no OS, no threads required for the test build).
- The single engine-touching and OS-touching translation units are verified on Windows.

---

## Build Requirements

- **Windows** with **Visual Studio 2022** (MSVC v143 toolset), C++17.
- **GCC** (for the engine-free test build / CI parity), C++17.
- The X-Ray **Monolith** engine sources (S.T.A.L.K.E.R. Anomaly engine).

> **Engine dependency (not vendored).** The upstream X-Ray engine is large and is **not** committed to this repository. Obtain the Anomaly / X-Ray Monolith engine separately and place it under `Engine/` (git-ignored). The framework builds as `Multiplayer/xrMP.sln` and links `xrMP.lib` into the engine executable.

**Building the framework + tests:**

```
# Open the framework solution in Visual Studio 2022:
Multiplayer/xrMP.sln          # xrMP (static lib) + xrMP_Tests (GoogleTest runner)

# Test build is engine-free and OS-free — no engine checkout required to run the suite.
```

---

## Repository Structure

```
STALKER-MP/
├── Multiplayer/                 # The STALKER-MP framework (this project)
│   ├── include/stalkermp/       # Public headers (core, world, net, player, snapshot, adapters)
│   ├── src/                     # Implementation
│   │   └── adapters/            # EngineAdapters.cpp (One Engine Boundary),
│   │                            # PlatformSockets.cpp (One Platform Boundary)
│   ├── tests/                   # GoogleTest suite (engine-free / OS-free)
│   ├── docs/                    # Per-subsystem & per-sprint architecture docs
│   └── xrMP.sln                 # Framework + test solution
├── Documentation/
│   ├── Architecture/ADR/        # Architecture Decision Records (ADR-007…011)
│   ├── SPRINTS/                 # Frozen sprint architectures, plans, step specs
│   └── AI/                      # Status, session log, decisions, roadmap
├── docs/images/                 # README diagrams (placeholders + exports)
├── Engine/                      # Upstream X-Ray engine (external; git-ignored)
├── README.md
├── .gitignore
└── VERSION.md / CHANGELOG.md / RELEASE_NOTES.md
```

---

## Contribution Guidelines

STALKER-MP follows an **architecture-first, frozen-then-implement** workflow. Contributions must respect it:

1. **Architecture before code.** New subsystems begin with a design/architecture document that is reviewed and **frozen** before any implementation.
2. **Implementation Plan → Step Specifications → Implementation.** Work proceeds in small, independently verifiable steps; each step is specified, implemented, and verified before the next.
3. **Respect the boundaries.** Never add engine headers outside `EngineAdapters.cpp` or OS headers outside `PlatformSockets.cpp`.
4. **Conform to the ADRs.** No exceptions/RTTI/iostream in engine-linked code (ADR-007); use `core::Expected<T>` and value outcomes.
5. **Determinism is non-negotiable.** No wall-clock in control flow; tick-derived logic; add replay/determinism tests for stateful code.
6. **Tests required.** Every step ships unit tests that pass on GCC and MSVC with zero new warnings.
7. **Do not modify frozen documents** (ADRs, frozen sprint architectures/plans/specs) except via a new, explicitly superseding decision.

Please open an issue to discuss substantial changes before submitting a pull request.

---

## License

_License to be determined._ A license file (`LICENSE`) will be added before public release. Until then, all rights are reserved by the project authors. Note that the upstream X-Ray engine is governed by its own separate license and is **not** included in this repository.

---

## Credits

- **STALKER-MP framework** — architecture, implementation, and verification by the STALKER-MP project.
- **X-Ray Engine / S.T.A.L.K.E.R.** — GSC Game World (engine) and the OpenXRay / S.T.A.L.K.E.R. Anomaly community (Monolith engine).
- **Testing** — GoogleTest.

STALKER-MP is a community project and is not affiliated with or endorsed by GSC Game World.

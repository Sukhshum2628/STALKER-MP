# Sprint-012: Save/Load
# Sprint-012: Save/Load System

---

# Sprint Information

| Field | Value |
|--------|-------|
| Sprint | 012 |
| Title | Save/Load System |
| Status | **Implemented / Verified / Closed / Frozen** (2026-07-20) |
| Priority | Critical |
| Estimated Duration | 3 Weeks |
| Prerequisites | Sprint-001 through Sprint-011 |
| Next Sprint | Sprint-013 – Lua Integration |

---

# 1. Sprint Overview

Sprint-012 implements the complete Save/Load System for STALKER-MP.

Building upon the Persistence Framework introduced in Sprint-011, this sprint serializes the authoritative world into durable storage and reconstructs it deterministically during server startup.

The save system restores the complete simulation including World state, ALife, Entity Registry, Smart Terrains, Players, Story progression, Environment, and Scheduler state.

Networking does not participate in world restoration.

Clients reconnect after recovery is complete.

---

# 2. Objectives

## Primary Objectives

- Implement World serialization
- Implement World deserialization
- Restore Entity Registry
- Restore ALife
- Restore Player persistence
- Restore Environment
- Restore Scheduler state
- Implement version migration

## Secondary Objectives

- Save diagnostics
- Recovery diagnostics
- Integrity verification
- Performance instrumentation

---

# 3. Scope

Included

- Save pipeline
- Load pipeline
- World reconstruction
- Entity restoration
- Player restoration
- ALife restoration
- Save validation
- Version migration

---

# 4. Out of Scope

Not Included

- Cloud saves
- Distributed persistence
- Replay system
- Backup synchronization
- Mod migration
- Cross-version compatibility beyond supported migration paths

---

# 5. Architecture References

- 04_World.md
- 05_ALife.md
- 08_Persistence.md

---

# 6. RFC References

- RFC-0003 — Immutable Snapshot Architecture
- RFC-0005 — Persistence Architecture

---

# 7. Technical Tasks

---

## 7.1 Save Manager

Implement

CreateSave()

OverwriteSave()

DeleteSave()

EnumerateSaves()

ValidateSave()

Responsibilities

- Save coordination
- Metadata management
- Validation

---

## 7.2 Save Writer

Serialize

World

↓

Environment

↓

Entity Registry

↓

Players

↓

ALife

↓

Scheduler

↓

Story

↓

Metadata

Maintain deterministic serialization order.

---

## 7.3 Save Reader

Read

Metadata

Validate Version

Allocate World

Restore Registry

Restore Entities

Restore ALife

Restore Scheduler

Finalize World

---

## 7.4 World Restoration

Restore

Simulation Tick

World Time

Weather

Environment

Global State

Zone Configuration

World restoration completes before networking starts.

---

## 7.5 Entity Restoration

Restore

Entity IDs

Runtime State

Transform

Inventory

Ownership

Relationships

Restore deterministic entity ordering.

---

## 7.6 Player Restoration

Restore

Player Profiles

Inventory

Position

Statistics

Quest Progress

Faction

Connection State (offline)

Players reconnect later.

---

## 7.7 ALife Restoration

Restore

Smart Terrains

Task Manager

Simulation State

Offline Objects

NPC Schedules

Faction Planner

Story Links

Behavior resumes after restoration.

---

## 7.8 Scheduler Restoration

Restore

Simulation Tick

Pending Updates

Scheduling Queues

Deferred Tasks

Execution Order

Scheduler resumes deterministically.

---

## 7.9 Version Migration

Support

Version Detection

Migration Pipeline

Compatibility Validation

Unsupported Version Detection

Migration Logging

---

## 7.10 Integrity Validation

Verify

Checksums

Entity Counts

Player Counts

Registry Integrity

Version Compatibility

Missing References

Duplicate IDs

---

## 7.11 Recovery Pipeline

Startup

↓

Load Save

↓

Validate

↓

Restore World

↓

Restore ALife

↓

Restore Scheduler

↓

Initialize Multiplayer

↓

Accept Connections

Simulation always starts before networking.

---

## 7.12 Diagnostics

Provide

Save Inspector

Load Inspector

Recovery Timeline

Migration Report

Integrity Report

Validation Report

---

## 7.13 Statistics

Track

Save Duration

Load Duration

Entities Restored

Players Restored

Migration Time

Validation Failures

Recovery Success Rate

---

## 7.14 Performance

Measure

Serialization Time

Deserialization Time

Recovery Time

Memory Usage

Validation Time

Migration Cost

---

## 7.15 Error Handling

Handle

Corrupted Save

Version Failure

Missing Data

Checksum Failure

Interrupted Load

Partial Recovery

Unsupported Save

---

## 7.16 Unit Tests

Verify

Serialization

Deserialization

World Restoration

Player Restoration

ALife Restoration

Scheduler Restoration

Migration

Validation

Corrupted Save Recovery

---

## 7.17 Integration Tests

Verify

Server restart

Autosave recovery

Player reconnect

Entity restoration

Large world loading

Long-running simulations

Multiple save generations

---

## 7.18 Documentation

Document

Save pipeline

Load pipeline

Recovery process

Migration

Validation

Version policy

---

# 8. Deliverables

✓ Save Manager

✓ Save Writer

✓ Save Reader

✓ World Reconstruction

✓ Entity Restoration

✓ Player Restoration

✓ ALife Restoration

✓ Scheduler Restoration

✓ Version Migration

✓ Validation Framework

✓ Diagnostics

✓ Tests

✓ Documentation

---

# 9. Risks

Potential Risks

- Save corruption
- Long load times
- Migration failure
- Missing references
- Duplicate entities
- Invalid world state

Mitigation

- Atomic save replacement
- Validation
- Checksum verification
- Rollback support
- Migration testing
- Registry consistency checks

---

# 10. Validation Strategy

Saving

✓ World saves successfully.

Loading

✓ World restores correctly.

Recovery

✓ Simulation resumes deterministically.

Players

✓ Player persistence restored.

ALife

✓ AI resumes correctly.

Performance

✓ Loading remains within acceptable limits.

Stress Test

✓ Large persistent worlds restore successfully.

---

# 11. Acceptance Criteria

✅ Save pipeline complete.

✅ Load pipeline complete.

✅ World reconstruction verified.

✅ ALife restoration verified.

✅ Player restoration verified.

✅ Version migration functional.

✅ Validation framework operational.

✅ Tests passing.

✅ Documentation updated.

---

# 12. Definition of Done

Sprint-012 is complete when

- Complete authoritative world can be saved.
- Complete authoritative world can be restored.
- ALife resumes without inconsistencies.
- Players reconnect to restored characters.
- Version migration is operational.
- Recovery is deterministic.
- Simulation starts before networking.

---

# 13. Next Sprint

Sprint-013 – Lua Integration

With persistence complete, the multiplayer framework is functionally complete.

---

# 14. Sprint Closure (2026-07-20)

**Sprint-012 (Save/Load System) is declared Implemented / Verified / Closed / Frozen.**

All 18 steps were implemented under the mandatory workflow (implement → Antigravity verification → git commit → GitHub push → next batch) — grouped into ten verification batches (B1–B10) with the four load-bearing architectural gates (Step-09, Step-10, Step-13, Step-17) verified in isolation — and each step was independently verified by Antigravity, with the tree left buildable after every step.

## 14.1 Final verified baseline
- **675 / 675 build tests passing** — Release x64 on **MSVC** and the engine-free **GCC** test build; **0 errors, 0 warnings, no regressions.** (Sprint-011 baseline 599 + the 76-test Sprint-012 save/load suite.)
- MSVC Release clean under `EngineAbi.props`. Engine restore adapters and the real filesystem backend are Antigravity-smoke-verified on Windows. Game testing has not started yet.

## 14.2 Steps 01–18 (all implemented, verified, documented)
01 value types + restoration records (`SaveLoadTypes.h`) · 02 `SaveLoadConfiguration` · 03 serialization format + byte primitives (`SaveFormat.h` / `SaveByteCursor.h`) · 04 `SaveWriter` · 05 `SaveReader` · 06 `SaveIntegrityValidator` · 07 version detection + `SaveMigrator` · 08 `ISaveSource` read seam (+ null/in-memory) · 09 platform filesystem backend (`PlatformSaveStore`) · 10 restore-sink seams (`IRestoreSinks`) · 11 World/Environment restorers · 12 Entity/Player restorers · 13 ALife/Scheduler restorers · 14 `SaveManager` + `RecoveryPipeline` · 15 `SaveLoadDiagnostics` · 16 error-handling + recovery hardening · 17 composition-root + engine-adapter wiring + integration + `Multiplayer/docs/SaveLoad.md` · 18 sprint closure.

## 14.3 Acceptance criteria (§11) — satisfied
1. Save pipeline complete (`SaveManager` + `SaveWriter` → frozen `IPersistenceStore` write seam). ✅
2. Load pipeline complete (`SaveReader` + `RecoveryPipeline`: Load → Validate → Migrate → Restore). ✅
3. World reconstruction verified (World/Environment restorers through the restore-sink boundary). ✅
4. ALife restoration verified (ALife restorer via the Sprint-005 `IAlifeSwitchGateway` seam). ✅
5. Player restoration verified (players restored offline; reconnect after recovery). ✅
6. Version migration functional (`VersionManager` compatibility + `SaveMigrator` step registry). ✅
7. Validation framework operational (`SaveIntegrityValidator` reusing Sprint-011 `ValidationFramework`). ✅
8. Tests passing (675/675, GCC + MSVC). ✅
9. Documentation updated (`Multiplayer/docs/SaveLoad.md`; status docs synchronized). ✅

## 14.4 Definition of Done (§12) — satisfied
- Complete authoritative world can be saved (deterministic, byte-identical serialization; wall-clock excluded). ✅
- Complete authoritative world can be restored (host-side restoration completes before networking). ✅
- ALife resumes without inconsistencies (restoration through the sanctioned Sprint-005 gateway; ALife ownership preserved). ✅
- Players reconnect to restored characters (offline restoration; networking comes up after recovery). ✅
- Version migration is operational (compatible-but-older saves migrated to the current build). ✅
- Recovery is deterministic (twin-instance restart yields identical results; value-outcome error handling). ✅
- Simulation starts before networking (recovery is a one-shot `Bootstrap Initialize` phase before the frame bridge; **no new `tick_order` key**). ✅

## 14.5 Evidence gates — satisfied
- **E-G1-SL** (deterministic, byte-identical round-trip serialization): **PASSED**.
- **E-G2-SL** (integrity + version validation with value-outcome error handling): **PASSED**.
- **E-G3-SL** (host-authoritative restoration completing before networking): **PASSED**.
- **E-G4-SL** (reuse of the frozen Sprint-011 infrastructure without redesign): **PASSED**.
- **E-G5-SL** (recovery isolation — a corrupted/partial load never corrupts a running or freshly-initialized world): **PASSED**.

## 14.6 Boundaries & invariants (verified)
- Engine writes confined to `EngineAdapters.cpp`; filesystem I/O confined to `PlatformSaveStore.cpp`; authoritative restoration only through the Step-10 restore-sink seams (ADR-008 / One Engine Boundary / One Platform Boundary). ✅
- **No new `tick_order` key** and no new frame subscriber (recovery is a startup phase); networking-last preserved; no thread created (ADR-011); wire protocol untouched (ADR-010). ✅
- Preserve Before Replace: Sprint-011 `IPersistenceStore` write seam, `PersistenceManager`, `VersionManager`, `ValidationFramework`, `PersistenceSnapshot`, and diagnostics reused unchanged; the X-Ray `CSaveGame` remains the owner of base object-state restoration (D-SL4). ✅
- ADR-007…ADR-011 preserved; no ADR reinterpreted. ✅

## 14.7 Sprint-013 readiness
The Save/Load System serializes the authoritative world deterministically, reconstructs it host-side before networking, validates and migrates across versions, and recovers in a failure-isolated manner — all behind the frozen Sprint-011 seams and the sanctioned engine boundaries. No new authoritative `tick_order` value is assigned or depended upon. **The project is ready for Sprint-013 (Lua Integration).**

Sprint-013 introduces a controlled Lua integration layer that exposes documented engine APIs to gameplay scripts while preserving subsystem ownership, host authority, and deterministic simulation. Lua becomes the primary scripting interface for extending gameplay without modifying the engine core.
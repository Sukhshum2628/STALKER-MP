# Sprint-012 — Save/Load System · Architecture & Implementation Plan

| Field | Value |
|---|---|
| Sprint | 012 — Save/Load System |
| Status | **Plan — FROZEN candidate** (review → freeze → approve before Step-01) |
| Scope authority | `Documentation/SPRINTS/Sprint-012/Sprint-012-Save-Load.md` (approved) |
| Baseline | Sprint-011 CLOSED & FROZEN — **599 / 599** tests passing (GCC + MSVC) |
| Governing ADRs | ADR-007, ADR-008, ADR-009, ADR-010, ADR-011 (all preserved; none modified) |
| Architecture refs | `04_World.md`, `05_ALife.md`, `08_Persistence.md`; RFC-0003, RFC-0005 |
| Reuses (unchanged) | Sprint-011 `IPersistenceStore`, `PersistenceManager`, `VersionManager`, `SaveMetadata`/`SaveMetadataBuilder`, `ValidationFramework`, `PersistenceSnapshot`, `PersistenceDiagnostics`; Sprint-008 `ISnapshotView`; Sprint-003 Entity Registry; Sprint-005 `IAlifeSwitchGateway`; Sprint-007 player/spawn seams |
| Steps | 18 (Step-01 … Step-18), strict additive dependency chain |

---

## 0. Governance & preserved invariants (apply to every step)

This plan is **purely additive** and modifies no prior sprint's frozen documents, code, or ADRs. Every step preserves, without exception:

- **Preserve Before Replace** — Sprint-012 REUSES the Sprint-011 persistence infrastructure unchanged (write seam `IPersistenceStore`, `PersistenceManager`, `VersionManager`, `SaveMetadata`, `ValidationFramework`, `PersistenceSnapshot`, diagnostics) and the Sprint-008 immutable `ISnapshotView`. It adds serialization, the load/restore pipeline, migration, and the real backends **additively**; it reimplements none of the world, ALife, snapshot, replication, or persistence logic.
- **Host Authority** — save and restore are host-side. Restoration reconstructs the single authoritative world at startup, BEFORE networking accepts connections. Clients never participate in restoration and reconnect only after recovery is complete.
- **One World / One ALife Simulation** — restoration reconstructs the single authoritative world and the single ALife simulation; it creates no second world/simulation.
- **Deterministic Simulation & Replay Determinism** — serialization order is fixed and deterministic; deserialization + restoration are deterministic; a save→load→save round-trip is byte-identical for identical state; checksums verify integrity; wall-clock appears only in explicitly-diagnostic fields.
- **Clients Never Own Persistent World State** — only the host serializes/restores authoritative state; no persistent world state is owned by or restored from clients.
- **Existing Engine Ownership / Existing ALife Ownership** — restoration writes authoritative engine/ALife state ONLY through sanctioned engine seams (see D-SL1); the engine-free core owns no engine state and includes no engine headers.
- **Existing Serialization / Existing Save Ownership** — STALKER-MP save/load owns only the multiplayer authoritative-world persistence; it does not hook, override, replace, or duplicate the vanilla X-Ray single-player save system (see D-SL4).
- **Singular Engine Boundary** — engine headers remain confined to `EngineAdapters.cpp`; restoration's engine writes and any engine reads are added there behind engine-free seams.
- **Singular Platform Boundary** — all filesystem/OS access is confined to ONE platform translation unit (`PlatformSaveStore.cpp`) behind engine-free factories (mirroring `PlatformSockets.cpp`); the core is OS/file-free.
- **ADR-007** — no exceptions, no RTTI, no iostream in the engine-free core; all fallible operations return `core::Expected<T>` / `SaveLoadOutcome` value outcomes; bounded memory.
- **ADR-008** — authoritative restoration is applied only through the sanctioned engine seams (D-SL1); the engine-free core never mutates authoritative simulation directly.
- **ADR-009** — the platform boundary is preserved: filesystem code lives only in the platform TU behind engine-free seams (D-SL2).
- **ADR-010** — the wire protocol is untouched; save/load is host-local and sends nothing.
- **ADR-011** — Sprint-012 creates **no thread**. Saving continues on the existing synchronous persistence path; loading/recovery is a synchronous, deterministic one-time startup phase. **No new `tick_order` key** is introduced (see §7).

**Compatibility.** Every step is additive and leaves the Sprint-011 **599/599** baseline intact; new tests only add to the count. No prior public API changes.

---

## 0.A Binding architectural decisions (frozen here — documentation only)

These four decisions resolve the boundary questions raised in the Sprint-012 documentation review. They are **documentation decisions**; no implementation is performed in this plan.

- **D-SL1 — Authoritative restoration crosses the engine boundary only through sanctioned seams (ADR-008 / Singular Engine / ALife Ownership).** The engine-free core parses a save into deterministic **restoration records** and applies them through engine-free **restore-sink** interfaces (`IWorldRestoreSink`, `IEntityRestoreSink`, `IPlayerRestoreSink`, `IAlifeRestoreSink`, `ISchedulerRestoreSink`, `IEnvironmentRestoreSink`). The real implementations live ONLY in `EngineAdapters.cpp` and write authoritative state through existing sanctioned engine APIs (Sprint-005 `IAlifeSwitchGateway` for ALife online/offline, Sprint-003 Entity Registry, Sprint-007 spawn gateway, engine world/environment access). The core includes no engine headers; test builds use null/in-memory sinks. Restoration runs host-side at startup, before networking.
- **D-SL2 — Filesystem access is confined behind the platform boundary (ADR-009).** The frozen write seam `IPersistenceStore` (Begin/Write/Commit/Abort) is REUSED UNCHANGED; an additive engine-free **read seam** `ISaveSource` (enumerate / read-bytes / exists) is introduced for loading. Their real filesystem backend (atomic save replacement, enumerate, delete, read) is confined to ONE platform TU, `PlatformSaveStore.cpp`, behind engine-free factories — the only OS/file code in the module, mirroring `PlatformSockets.cpp`. The engine-free `SaveWriter`/`SaveReader` own the deterministic byte format; the platform backend performs the actual disk I/O. Test/default builds use in-memory/null backends.
- **D-SL3 — Sprint-011 infrastructure is reused without redesign (Preserve Before Replace).** The save WRITE path continues through the frozen `PersistenceManager` + `IPersistenceStore`; Sprint-012 only supplies the deterministic byte format (`SaveWriter`) that the platform backend persists, and swaps the concrete store backend at the composition root (the designed Sprint-011 extension point, not a redesign). `VersionManager`, `SaveMetadata`/`SaveMetadataBuilder`, `ValidationFramework`, `PersistenceSnapshot`, and `PersistenceDiagnostics` are reused as-is. The new `saveload::` namespace adds serialization, load, restore, and migration only.
- **D-SL4 — Save/load ownership integrates with, and never replaces, existing X-Ray save ownership.** STALKER-MP save/load persists and restores ONLY the multiplayer authoritative world (World, Environment, Entity Registry, Players, ALife, Scheduler, Story) as its own save artifacts, restored through sanctioned engine seams. It does not hook, override, wrap, or duplicate the vanilla `CSaveGame`/engine save path, and modifies no engine save code.

---

## 1. Sprint objective

Implement the complete **Save/Load System** on top of the Sprint-011 Persistence Framework: serialize the authoritative world into durable storage, and reconstruct it deterministically during host startup — restoring World, Environment, Entity Registry, Players, ALife, Story, and Scheduler state — with version migration and integrity validation. Simulation is fully restored before networking starts; clients reconnect after recovery.

## 2. Scope

**In scope.** A new engine-free `saveload::` namespace: value types & configuration; the deterministic serialization format + byte primitives; the `SaveWriter` and `SaveReader`; integrity validation; version detection + migration; the additive `ISaveSource` load-side read seam; the platform filesystem backend (write store + read source) confined to one platform TU; the engine-free restoration record model + restore-sink seams; World/Environment, Entity/Player, and ALife/Scheduler restorers; the `SaveManager` (save coordination) and `RecoveryPipeline` (load + restore orchestration); diagnostics + statistics + performance instrumentation; error handling / recovery hardening; composition-root + engine-adapter wiring (save path + startup recovery, before networking); integration tests; and the subsystem doc `Multiplayer/docs/SaveLoad.md`.

## 3. Out-of-scope items

Cloud saves, distributed persistence, replay system, backup synchronization, mod migration, cross-version compatibility beyond supported migration paths (§4 of the scope authority); any modification of the vanilla X-Ray save system; any modification of the frozen persistence write seam or the wire protocol; any new authoritative `tick_order` key; any thread creation.

## 4. Design goals

Deterministic, byte-identical round-trip serialization; deterministic, host-authoritative restoration completing before networking; integrity + version validation with value-outcome error handling; reuse of the frozen Sprint-011 infrastructure without redesign; engine-free, platform-free, unit-testable core with engine writes confined to `EngineAdapters.cpp` and filesystem I/O confined to one platform TU; recovery isolation (a corrupted/partial load never corrupts a running or freshly-initialized world); zero impact on the Sprint-011 baseline.

## 5. Architectural overview

Save/Load is a **host-side** subsystem with two flows:

```
SAVE (per the frozen persistence path, at kPersistence = 500):
  PersistenceManager → PersistenceWorker.Persist → IPersistenceStore(backend)
                                                   └─ SaveWriter serializes the snapshot → bytes → atomic disk write

LOAD / RECOVERY (one-time host startup, BEFORE the frame bridge + before networking):
  Bootstrap Initialize → RecoveryPipeline:
     ISaveSource.read → SaveReader → SaveIntegrityValidator → SaveMigrator (if needed)
        → restoration records → { World/Environment, Entity/Player, ALife/Scheduler } restorers
        → restore-sink seams (engine adapters) apply authoritative state
     → (Bootstrap continues) Initialize Multiplayer → frame bridge → Accept Connections
```

Saving reuses the frozen `PersistenceManager`/`IPersistenceStore` at `kPersistence = 500`. Loading is NOT a per-frame tick — it runs once during `Bootstrap Initialize`, before the engine frame bridge is registered and before networking accepts connections, guaranteeing "simulation starts before networking" with no new `tick_order` key.

## 6. Component responsibilities

| Component | Responsibility |
| --- | --- |
| `saveload::SaveLoadTypes` | Value vocabulary: `SaveLoadOutcome`, restore phase/state enums, `SaveDescriptor`, restoration records, `RecoveryStatistics`, `RecoveryConsistency`. Reuses Sprint-011 `SaveMetadata`/`VersionSet`. |
| `saveload::SaveLoadConfiguration` (+ `FromConfig`) | `[saveload]` config: save directory token (opaque), max generations, load-on-startup, migration/integrity policy, version. |
| `saveload::SaveFormat` + byte primitives | Deterministic little-endian record framing (format header, versioned sections). Engine-free; no wire coupling (ADR-010 untouched). |
| `saveload::SaveWriter` | Deterministic serialization of a snapshot view into bytes in the fixed order (World → Environment → Registry → Players → ALife → Scheduler → Story → Metadata) with a content checksum. No I/O. |
| `saveload::SaveReader` | Deterministic parse of bytes into restoration records; validates header/version/checksum. No I/O. |
| `saveload::SaveIntegrityValidator` | Checksums, entity/player counts, registry integrity, version compatibility (reuses `VersionManager`/`ValidationFramework`), missing references, duplicate ids → value outcomes. |
| `saveload::SaveMigrator` | Version detection, migration pipeline, compatibility validation, unsupported-version detection, migration logging. |
| `saveload::ISaveSource` (+ null/in-memory) | Additive engine-free LOAD-side read seam (enumerate / read / exists). |
| `adapters::PlatformSaveStore` (platform TU) | Real filesystem backend: `IPersistenceStore` (write, atomic replacement) + `ISaveSource` (read) + enumerate/delete. The only OS/file code (ADR-009). Null in tests. |
| `saveload::` restore-sink seams | `IWorldRestoreSink` / `IEntityRestoreSink` / `IPlayerRestoreSink` / `IAlifeRestoreSink` / `ISchedulerRestoreSink` / `IEnvironmentRestoreSink` — engine-free write boundary; engine impls in `EngineAdapters.cpp` (D-SL1). |
| `saveload::WorldRestorer` / `EntityRestorer` / `PlayerRestorer` / `AlifeRestorer` / `SchedulerRestorer` | Deterministic-ordered application of restoration records through the sinks. |
| `saveload::SaveManager` | Save coordination: `CreateSave`/`OverwriteSave`/`DeleteSave`/`EnumerateSaves`/`ValidateSave`; reuses the frozen persistence write path. |
| `saveload::RecoveryPipeline` | Startup orchestration: Load → Validate → Migrate → Restore (World → ALife → Scheduler) → hand back to Bootstrap. |
| `saveload::SaveLoadDiagnostics` | Non-invasive collector + inspectors (save/load, recovery timeline, migration/integrity/validation reports; statistics; performance timing — diagnostic-only). |
| `adapters` wiring | Save-path store swap + startup recovery in `Bootstrap.cpp`; engine restore adapters in `EngineAdapters.cpp`; default `[saveload]` config block. |

## 7. Frame/startup placement (no new tick_order key)

- **Save** reuses the frozen `PersistenceManager` at the reserved `tick_order::kPersistence = 500`; the only change is swapping the concrete `IPersistenceStore` backend (in-memory → filesystem) at the composition root. No ordering change.
- **Load/recovery** runs ONCE during `Bootstrap Initialize`, **before** `CreateEngineFrameBridge` (frame registration) and **before** networking accepts connections. It is not a `FrameDispatcher` subscriber and introduces **no new `tick_order` key**. This enforces "simulation always starts before networking" and preserves the networking-last invariant.

## 8. Boundaries

- **Singular Engine Boundary** — restoration's authoritative writes (and any engine reads needed to restore) are added only in `EngineAdapters.cpp`, behind the engine-free restore-sink seams (D-SL1). No engine header enters the `saveload::` core.
- **Singular Platform Boundary** — all filesystem I/O (atomic write, read, enumerate, delete) is confined to `PlatformSaveStore.cpp` behind engine-free factories (D-SL2), mirroring `PlatformSockets.cpp`. No OS/file header enters the core.

## 9. Testing strategy

`Multiplayer/tests/SaveLoadTests.cpp` accumulates per-step unit tests (deterministic serialization/round-trip, validation, migration, restoration through fake sinks, in-memory backends) plus a `SaveLoadIntegrationStep17` composed suite (server-restart recovery, autosave recovery, player reconnect, entity restoration, large-world, long-running, multiple generations) and Bootstrap recovery assertions. GCC engine-free build + MSVC. Every rejection is a value outcome leaving state unchanged; determinism verified by twin-instance round-trip; recovery isolation verified under corrupted/partial saves.

## 10. Evidence gates (must all pass by close)

- **E-G1-SL** — deterministic serialization + round-trip identity (save→load→save byte-identical; restoration deterministic and order-fixed).
- **E-G2-SL** — integrity / version / migration validation as value outcomes; corrupted, unsupported, and mismatched saves are rejected safely (no partial authoritative mutation).
- **E-G3-SL** — authoritative restoration only through sanctioned engine seams (ADR-008); no engine header in the core; simulation restored before networking; Host Authority / One World / One ALife preserved.
- **E-G4-SL** — Singular Platform Boundary: all filesystem/OS code confined to the platform save TU behind engine-free seams (ADR-009); core OS/file-free.
- **E-G5-SL** — Preserve Before Replace + recovery isolation: Sprint-011 infrastructure reused unchanged; a failed/partial load never corrupts world state; no new `tick_order` key; networking-last preserved; no thread (ADR-011).

## 11. Definition of Done (from scope authority §12)

Complete authoritative world can be saved and restored; ALife resumes without inconsistencies; players reconnect to restored characters; version migration is operational; recovery is deterministic; simulation starts before networking. Recorded at closure (Step-18).

## 12. Step ordering (dependency chain)

`01 types → 02 config → 03 serialization format + byte primitives → 04 SaveWriter → 05 SaveReader → 06 integrity validation → 07 version detection + migration → 08 ISaveSource read seam (+ null/in-memory) → 09 platform filesystem backend (PlatformSaveStore) → 10 restoration records + restore-sink seams → 11 World/Environment restoration → 12 Entity/Player restoration → 13 ALife/Scheduler restoration → 14 SaveManager + RecoveryPipeline → 15 diagnostics + statistics + performance → 16 error handling + recovery hardening → 17 composition-root + engine-adapter wiring + integration + SaveLoad.md → 18 sprint closure.`

Each step is independently buildable (clean compile, tree green after each) and independently verifiable (its own tests), strictly additive.

### Dependency graph (edges = "depends on")

```
01 ← 02,03,06,07,10,15
03 ← 04,05
04 ← 05(round-trip),09(backend persists writer output),14
05 ← 06,07,11,12,13,14
06 ← 14,16      07 ← 14,16
08 ← 09,14      09 ← 17(wiring)
10 ← 11,12,13   11,12,13 ← 14
14 ← 15,16,17   15 ← 17
16 ← 17         17 ← 18
```
No cycles. Reused Sprint-011/008/005/007/003 components are pre-existing inputs (not steps).

### 12.A Implementation batching strategy (approved)

The 18 implementation steps above are **unchanged** — their scope, requirements, dependencies, and ordering are frozen exactly as written. This subsection defines only the **execution batching**: which consecutive steps are implemented and Antigravity-verified together as a single cycle. Batching combines steps only where doing so violates no dependency, crosses no architectural/ADR/verification boundary, keeps diagnostics non-invasive and hardening test-only, and preserves independent testability and failure isolation. The four architectural gates (Step-09, Step-10, Step-13, Step-17) remain isolated, and Step-18 is the documentation-only closure batch. Step-01 was implemented as its own initial cycle before this batching strategy was adopted.

| Batch | Steps | Isolation | Nature of the batch |
|---|---|---|---|
| **B1** | 02 + 03 | — | Config + serialization format/byte primitives (pure, independent, engine-free foundation). |
| **B2** | 04 + 05 | — | `SaveWriter` + `SaveReader` (tightly-coupled round-trip; deterministic). |
| **B3** | 06 + 07 + 08 | — | Integrity validation + version/migration + `ISaveSource` read seam (three mutually-independent pure components; reuse `VersionManager`/`ValidationFramework` unchanged). |
| **B4** | 09 | **Isolated** | Platform filesystem backend — **Singular Platform Boundary / ADR-009** (only OS/file TU; Windows smoke). |
| **B5** | 10 | **Isolated** | Restore-sink seams — **Singular Engine Boundary / ADR-008** write-boundary decision point. |
| **B6** | 11 + 12 | — | World/Environment + Entity/Player restorers (sibling engine-free consumers of the frozen Step-10 seams). |
| **B7** | 13 | **Isolated** | ALife/Scheduler restoration — **One ALife Simulation / Sprint-005 sanctioned gateway** sensitivity. |
| **B8** | 14 + 15 + 16 | — | **Recovery Subsystem** (single cohesive unit): `SaveManager` + `RecoveryPipeline`, its non-invasive diagnostics, and its additive test-only hardening/negative surface. |
| **B9** | 17 | **Isolated** | Composition-root + engine adapters + integration + `SaveLoad.md` — **Integration gate** (ADR-008 real engine writes + ADR-009 store swap + startup-recovery-before-networking ordering; Windows smoke). |
| **B10** | 18 | **Isolated** | Sprint closure (documentation only; no code). |

**Recovery Subsystem batch (B8) — rationale.** Steps 14, 15, and 16 form one cohesive implementation unit: Step-16's negative/hardening surface exists precisely to validate Step-14's recovery-isolation guarantee (E-G5-SL), and Step-15 is a pure, non-invasive collector. The batch adds **no** engine interaction and **no** platform interaction (it uses the frozen restore-sink seams as fake/in-memory sinks and the in-memory store/source — the real engine adapters and filesystem backend are Step-09/Step-17). Recovery orchestration remains independently testable (its own `SaveManager`/`RecoveryPipeline` unit groups), diagnostics remain non-invasive, hardening remains additive and test-only, and the three separable test groups keep failure localization and Antigravity verification unambiguous.

**Batching does not alter the dependency graph.** Cross-batch dependencies are the same edges listed above; batching only groups the verification cadence. Within any multi-step batch, the steps are implemented in their documented order.

---

## Step specifications

Each step: **Objective · Scope In/Out · FR · Evidence · Files · Tests · Commit.** All engine-free/OS-free unless the step names an existing boundary; ADR-007 value outcomes; additive.

### Step-01 — Save/Load value types & vocabulary
- **Objective.** The engine-free vocabulary for the subsystem.
- **In.** `SaveLoadTypes.h` (header-only): `enum class SaveLoadOutcome : std::uint8_t { Ok, CorruptedSave, VersionUnsupported, VersionMismatch, IntegrityFailure, MissingData, ChecksumFailure, InterruptedLoad, PartialRecovery, StorageUnavailable, NothingToLoad }` (+ name); `enum class RestorePhase` (World, Environment, Registry, Players, ALife, Scheduler, Story, Complete); `enum class LoadState`; `struct SaveDescriptor { std::uint64_t saveId; SaveMetadata metadata; std::uint32_t generation; }` (reuses Sprint-011 `SaveMetadata`); restoration record structs (value-only world/environment/entity/player/alife/scheduler records); `RecoveryStatistics`, `RecoveryConsistency`. Types only. **Out.** Logic.
- **FR.** POD/trivially-copyable value captures; deterministic layout; wall-clock isolated to diagnostic fields. **Evidence.** E-G1-SL groundwork. **Tests.** `SaveLoadTypesStep1`. **Commit.** `docs(saveload): freeze Sprint-012 Step-01 spec (types)`.

### Step-02 — `SaveLoadConfiguration`
- **Objective.** Parsed `[saveload]` config.
- **In.** `SaveLoadConfiguration.h`/`.cpp`: opaque `saveDirectoryToken` (resolved only by the platform backend — no path logic in the core), `maxGenerations` (≥1), `loadOnStartup` (bool), `strictIntegrity` (bool), `migrationEnabled` (bool), `version` (≥1); `FromConfig` (missing → defaults; invalid → InvalidArgument). **Out.** Filesystem access.
- **FR.** Cross-field validation; value outcomes; no OS/path resolution in the core. **Evidence.** E-G4-SL groundwork. **Tests.** `SaveLoadConfigStep2`. **Commit.** `docs(saveload): freeze Sprint-012 Step-02 spec (config)`.

### Step-03 — Serialization format + byte primitives
- **Objective.** Deterministic little-endian record framing.
- **In.** `SaveFormat.h` + engine-free byte writer/reader primitives (reuse an existing engine-free little-endian byte utility if suitable — no wire-protocol coupling, ADR-010 untouched): format magic, format version, section framing (id + length + payload), and a trailing content checksum. **Out.** World/domain serialization (Steps 04/05).
- **FR.** Deterministic byte layout; bounds-checked reads → value outcomes; no exceptions. **Evidence.** E-G1-SL. **Tests.** `SaveFormatStep3`: write/read round-trip, truncation/overrun rejected, checksum mismatch detected. **Commit.** `docs(saveload): freeze Sprint-012 Step-03 spec (format)`.

### Step-04 — `SaveWriter`
- **Objective.** Deterministic serialization of the authoritative snapshot into bytes.
- **In.** `SaveWriter.h`/`.cpp`: `Serialize(const snapshot::ISnapshotView&, const SaveMetadata&) -> std::vector<std::byte>` (or into a caller buffer) in the fixed order World → Environment → Entity Registry → Players → ALife → Scheduler → Story → Metadata, stamping the content checksum. Reads the immutable snapshot read-only; performs no I/O. **Out.** File writing (Step-09); restoration.
- **FR.** Deterministic (identical state → byte-identical output); fixed section order; no wall-clock in the serialized identity. **Evidence.** E-G1-SL. **Tests.** `SaveWriterStep4`: determinism, section presence/order, checksum stability. **Commit.** `docs(saveload): freeze Sprint-012 Step-04 spec (writer)`.

### Step-05 — `SaveReader`
- **Objective.** Deterministic parse of bytes into restoration records.
- **In.** `SaveReader.h`/`.cpp`: `Parse(bytes) -> Expected<LoadedSave>` yielding the engine-free restoration record set + `SaveMetadata`, validating magic/format-version/section framing/checksum. No I/O; no engine access. **Out.** Integrity/version policy (Step-06/07); applying records (Steps 11-13).
- **FR.** Round-trips Step-04 exactly; malformed/truncated/checksum-mismatch → `CorruptedSave`/`ChecksumFailure` value outcomes leaving nothing applied. **Evidence.** E-G1-SL/E-G2-SL. **Tests.** `SaveReaderStep5`: writer↔reader round-trip identity; corrupted inputs rejected. **Commit.** `docs(saveload): freeze Sprint-012 Step-05 spec (reader)`.

### Step-06 — Integrity validation
- **Objective.** Pure integrity validators over a loaded save.
- **In.** `SaveIntegrityValidator.h`/`.cpp`: checksum, entity/player counts vs records, registry integrity (unique ids, no duplicates), missing references, and version compatibility (REUSING Sprint-011 `VersionManager`/`ValidationFramework`) → `SaveLoadOutcome`. Pure. **Out.** Migration.
- **FR.** Deterministic; each rejection is a value outcome; no mutation. **Evidence.** E-G2-SL. **Tests.** `IntegrityStep6`: pass + each negative (checksum, duplicate id, missing ref, count mismatch, version). **Commit.** `docs(saveload): freeze Sprint-012 Step-06 spec (integrity)`.

### Step-07 — Version detection + migration
- **Objective.** Deterministic version handling and a migration pipeline.
- **In.** `SaveMigrator.h`/`.cpp`: version detection from `SaveMetadata`/`VersionSet` (reuse `VersionManager`); an ordered migration pipeline (chain of pure record→record migrators for supported paths); compatibility validation; unsupported-version detection (`VersionUnsupported`); migration logging (diagnostic). Migration EXECUTION is pure record transformation (no engine, no I/O). **Out.** Cross-version paths beyond supported (§4 out-of-scope).
- **FR.** Deterministic; Equal → no-op, MigrationRequired → apply supported chain, Incompatible/unsupported → value outcome; identical input → identical migrated records. **Evidence.** E-G2-SL. **Tests.** `MigrationStep7`: no-op, supported migration, unsupported rejected, determinism. **Commit.** `docs(saveload): freeze Sprint-012 Step-07 spec (migration)`.

### Step-08 — `ISaveSource` load-side read seam (+ null/in-memory)
- **Objective.** The additive engine-free READ boundary for loading (the write seam `IPersistenceStore` is reused unchanged).
- **In.** `ISaveSource.h` (engine-free): `Enumerate() -> std::vector<SaveDescriptor>`, `Read(saveId) -> Expected<std::vector<std::byte>>`, `Exists(saveId) -> bool`; `InMemorySaveSource` (+ paired with the Sprint-011 `InMemoryPersistenceStore` for tests) and `NullSaveSource`. No OS access. **Out.** Real filesystem backend (Step-09).
- **FR.** Value-outcome surface; deterministic enumeration order (ascending saveId). **Evidence.** E-G4-SL. **Tests.** `SaveSourceStep8`: in-memory read/enumerate/exists; missing → value outcome. **Commit.** `docs(saveload): freeze Sprint-012 Step-08 spec (read seam)`.

### Step-09 — Platform filesystem backend (`PlatformSaveStore`)
- **Objective.** The real durable backend, confined to ONE platform TU (ADR-009).
- **In.** `adapters/PlatformSaveStore.cpp` (the sole OS/file TU) implementing `IPersistenceStore` (write; **atomic save replacement** via temp-file + rename; composes `SaveWriter` to produce bytes) and `ISaveSource` (read/enumerate/exists/delete) over the configured `saveDirectoryToken`; engine-free factory declarations in a header. Null/in-memory counterparts for the test build. OS/file headers appear ONLY here. **Out.** Any engine access; any format logic (that is Steps 03-05).
- **FR.** Atomic write (a partial/interrupted write never replaces the previous save); enumerate deterministic; value outcomes; OS confinement (One Platform Boundary). **Evidence.** E-G4-SL (platform build smoke is Antigravity's on Windows). **Tests.** null-path/in-memory tests in the engine-free build; the real filesystem TU is Antigravity-verified. **Commit.** `docs(saveload): freeze Sprint-012 Step-09 spec (platform backend)`.

### Step-10 — Restoration records + restore-sink seams
- **Objective.** The engine-free write boundary through which restoration applies authoritative state (D-SL1 / ADR-008).
- **In.** Restore-sink interfaces: `IWorldRestoreSink`, `IEnvironmentRestoreSink`, `IEntityRestoreSink`, `IPlayerRestoreSink`, `IAlifeRestoreSink`, `ISchedulerRestoreSink` (each `Apply(record) -> SaveLoadOutcome`, engine-free, value-only); null/in-memory (recording) sinks for tests. The engine implementations are added at Step-17 in `EngineAdapters.cpp` (via the sanctioned Sprint-003/005/007 engine seams). **Out.** Engine implementations (Step-17); orchestration (Steps 11-14).
- **FR.** Value-only records; sinks never expose engine types; authoritative writes only through these seams. **Evidence.** E-G3-SL. **Tests.** `RestoreSinksStep10`: fake sinks record applied values; no mutation of the loaded records. **Commit.** `docs(saveload): freeze Sprint-012 Step-10 spec (restore seams)`.

### Step-11 — World + Environment restoration
- **Objective.** Deterministic-ordered application of World/Environment records.
- **In.** `WorldRestorer.h`/`.cpp`: apply simulation tick, world time, weather, environment, global state, zone configuration through `IWorldRestoreSink`/`IEnvironmentRestoreSink` in fixed order; report per-phase outcome. **Out.** Entities/ALife/scheduler.
- **FR.** Deterministic; value outcomes; "world restoration completes before networking" (enforced by the startup phase, Step-17). **Evidence.** E-G1-SL/E-G3-SL. **Tests.** `WorldRestoreStep11`: fields applied in order; failure short-circuits. **Commit.** `docs(saveload): freeze Sprint-012 Step-11 spec (world restore)`.

### Step-12 — Entity + Player restoration
- **Objective.** Deterministic restore of entities and players.
- **In.** `EntityRestorer.h`/`.cpp` (ids, runtime state, transform, inventory, ownership, relationships — ascending EntityId, deterministic) and `PlayerRestorer.h`/`.cpp` (profiles, inventory, position, statistics, quest progress, faction, offline connection state) through the sinks; reuse Sprint-003 Entity Registry + Sprint-007 player identity via their seams. Players restored offline; reconnect later. **Out.** ALife/scheduler.
- **FR.** Deterministic ordering; unique ids; value outcomes; players restored offline. **Evidence.** E-G1-SL/E-G3-SL. **Tests.** `EntityPlayerRestoreStep12`: ordered entity restore, duplicate-id rejection, player fields, offline state. **Commit.** `docs(saveload): freeze Sprint-012 Step-12 spec (entity/player restore)`.

### Step-13 — ALife + Scheduler restoration
- **Objective.** Deterministic restore of ALife and scheduler, resuming behavior.
- **In.** `AlifeRestorer.h`/`.cpp` (Smart Terrains, Task Manager, simulation state, offline objects, NPC schedules, faction planner, story links — through `IAlifeRestoreSink`, which the engine adapter drives via the Sprint-005 sanctioned ALife gateway) and `SchedulerRestorer.h`/`.cpp` (simulation tick, pending updates, scheduling queues, deferred tasks, execution order). Behavior resumes after restoration; scheduler resumes deterministically. **Out.** Engine ALife internals (owned by the engine; applied only via the seam).
- **FR.** Deterministic; ALife/scheduler ownership preserved (writes only via sanctioned seams); value outcomes. **Evidence.** E-G3-SL. **Tests.** `AlifeSchedulerRestoreStep13`: ordered application through fake sinks; deterministic resume; failure isolation. **Commit.** `docs(saveload): freeze Sprint-012 Step-13 spec (alife/scheduler restore)`.

### Step-14 — `SaveManager` + `RecoveryPipeline`
- **Objective.** Save coordination and the load→restore startup orchestration.
- **In.** `SaveManager.h`/`.cpp`: `CreateSave`/`OverwriteSave`/`DeleteSave`/`EnumerateSaves`/`ValidateSave`, reusing the frozen persistence write path + `ISaveSource`. `RecoveryPipeline.h`/`.cpp`: `Recover()` running Load (`ISaveSource`+`SaveReader`) → Validate (Step-06) → Migrate (Step-07) → Restore World → Restore ALife → Restore Scheduler (Steps 11-13) → return a `RecoveryReport`; simulation restored before control returns to Bootstrap. Engine-free orchestration. **Out.** Bootstrap wiring/engine adapters (Step-17).
- **FR.** Deterministic; recovery isolation (a failed/partial load returns a value outcome and leaves no partially-mutated authoritative world — abort/rollback); reuses Sprint-011 without redesign. **Evidence.** E-G2-SL/E-G5-SL. **Tests.** `SaveManagerStep14` + `RecoveryPipelineStep14`: enumerate/validate/delete; full recover happy-path via fakes; corrupted-save recovery isolation; determinism. **Commit.** `docs(saveload): freeze Sprint-012 Step-14 spec (managers)`.

### Step-15 — Diagnostics + statistics + performance
- **Objective.** A pure, non-invasive collector + inspectors.
- **In.** `SaveLoadDiagnostics.h` (header-only): `Reset`, `Snapshot -> RecoveryStatistics`, record save/load durations, entities/players restored, migration time, validation failures, recovery success rate; inspectors (save/load, recovery timeline, migration/integrity/validation reports). Timing explicitly diagnostic-only. **Out.** Pipeline reference/mutation.
- **FR.** Non-invasive; deterministic aggregates; monotonic counters; immutable `Snapshot`; `Reset` restores initial. **Evidence.** none. **Tests.** `SaveLoadDiagnosticsStep15`: record/snapshot, reset, monotonic, immutable copy, deterministic averages. **Commit.** `docs(saveload): freeze Sprint-012 Step-15 spec (diagnostics)`.

### Step-16 — Error handling + recovery hardening (negative surface)
- **Objective.** Prove the failure negatives as value outcomes.
- **In.** Extend `SaveLoadTests.cpp`: corrupted save, version failure, missing data, checksum failure, interrupted load, partial recovery, unsupported save — each a value outcome leaving no partial authoritative mutation (retain a safe state); a large/long-running determinism stress. Minimal `.cpp` hardening only on a proven gap. **Out.** New behavior/interfaces.
- **FR.** Every rejection is a value outcome; recovery isolation; determinism under stress; bounded memory. **Evidence.** E-G1-SL…E-G5-SL negatives. **Tests.** `SaveLoadHardeningStep16`. **Commit.** `docs(saveload): freeze Sprint-012 Step-16 spec (hardening)`.

### Step-17 — Composition-root + engine-adapter wiring + integration + `SaveLoad.md`
- **Objective.** Wire the save path and the startup recovery; add the engine restore adapters; document.
- **In.** `Bootstrap.cpp`: construct `SaveLoadConfiguration`; swap the persistence store backend to `PlatformSaveStore` (write) + provide the paired `ISaveSource` (real in the engine build, in-memory/null in tests) behind factories; run `RecoveryPipeline::Recover()` during `Initialize` — AFTER service construction and world Start groundwork but **BEFORE** `CreateEngineFrameBridge` and before networking accepts connections; reverse-order teardown; default `[saveload]` config block. `EngineAdapters.cpp`: the real restore-sink implementations (World/Environment/Entity/Player/ALife/Scheduler) writing authoritative state through the Sprint-003/005/007 sanctioned engine seams (One Engine Boundary); null counterparts in tests. Integration tests (server restart, autosave recovery, player reconnect, entity restoration, large world, long-running, multiple generations). Author `Multiplayer/docs/SaveLoad.md`. **Out.** Behavior change; new features.
- **FR.** **No new `tick_order` key** (recovery is a startup phase, not a subscriber); simulation restored before networking; networking-last preserved; existing ordering/services unchanged; engine code confined to `EngineAdapters.cpp`; filesystem code confined to `PlatformSaveStore.cpp`; deterministic; reverse-order teardown. **Evidence.** E-G1-SL…E-G5-SL composed; engine/platform build smoke is Antigravity's on Windows. **Tests.** `SaveLoadIntegrationStep17` + Bootstrap recovery/ordering assertions (recovery runs before the frame bridge; no new subscriber; save backend swapped). **Commit.** `docs(saveload): freeze Sprint-012 Step-17 spec (wiring + integration + SaveLoad.md)`.

### Step-18 — Sprint closure (no code)
- **Objective.** Confirm DoD; freeze status; declare Sprint-013 readiness.
- **In.** Cross-check §11 acceptance criteria and §12 DoD against delivered, verified artifacts; record the final baseline (`599 + Sprint-012 additions`); set `Sprint-012-Save-Load.md` to Implemented / Verified / Closed / Frozen (status + closure section); set this plan to EXECUTED & CLOSED; update `CURRENT_STATUS.md`, `SESSION_LOG.md`, README textual progress; consistency scan. **Out.** Any code/test change (except a genuine closure defect); any Sprint-013 work.
- **FR.** DoD satisfied & recorded; status consistent; Sprint-012 declared Closed/Frozen; Sprint-013 readiness stated. **Evidence.** All E-G*-SL recorded. **Commit.** `docs(saveload): close Sprint-012`.

---

## Verification checkpoints (one per batch)

Each **batch** (§12.A) is a verification boundary: implement the batch's step(s) in their documented order → clean Release|x64 build (GCC engine-free + MSVC) with zero warnings, all step tests green, baseline preserved → Antigravity verification → git commit → git push → next batch. Every underlying step remains independently buildable and independently testable; batching only groups the verification cadence and never relaxes a step's own requirements.

Load-bearing architectural gates are verified **in isolation** (never batched across): **Step-09 / Batch B4** (platform filesystem boundary / ADR-009), **Step-10 / Batch B5** (restore-sink seams / ADR-008 write boundary), **Step-13 / Batch B7** (ALife/scheduler restore via the sanctioned seam), **Step-17 / Batch B9** (startup recovery ordering — before networking; store-backend swap). **Step-18 / Batch B10** is the documentation-only closure boundary. Steps 14–16 are verified together as the single cohesive Recovery Subsystem boundary (**Batch B8**). Foundational/pure steps are batched only where §12.A permits — consecutive, mutually independent, and not crossing one of the isolated gates.

## Testing strategy (summary)

Per-step unit tests in `SaveLoadTests.cpp`; deterministic round-trip identity (writer↔reader; save→load→save); validation/migration negatives as value outcomes; restoration through fake/in-memory sinks (no engine); integration (`SaveLoadIntegrationStep17`) over the composed manager + in-memory backends; Bootstrap recovery/ordering assertions. Engine restore adapters and the real filesystem backend are Antigravity-smoke-verified on Windows.

## Documentation update sequence

- Per step: README progress marker + running test count (Steps 01-17).
- Step-17: author `Multiplayer/docs/SaveLoad.md` (save pipeline, load pipeline, recovery process, migration, validation, version policy, boundary decisions).
- Step-18: `Sprint-012-Save-Load.md` status + closure section; this plan → EXECUTED & CLOSED; `CURRENT_STATUS.md`; `SESSION_LOG.md`; README textual status.

## Acceptance criteria (from scope authority §11)

Save pipeline complete · Load pipeline complete · World reconstruction verified · ALife restoration verified · Player restoration verified · Version migration functional · Validation framework operational · Tests passing · Documentation updated — each confirmed at Step-18.

## Definition of Done (from scope authority §12)

Complete authoritative world can be saved and restored; ALife resumes without inconsistencies; players reconnect to restored characters; version migration operational; recovery deterministic; simulation starts before networking — all recorded at closure.

## Sprint closure checklist (Step-18)

1. All 18 steps implemented, each Antigravity-verified, tree buildable after each.
2. Final baseline recorded (`599 + Sprint-012 additions`), GCC + MSVC, zero warnings, no regressions.
3. Evidence gates E-G1-SL…E-G5-SL recorded PASSED.
4. Boundaries confirmed: engine code only in `EngineAdapters.cpp`; filesystem code only in `PlatformSaveStore.cpp`; **no new `tick_order` key**; recovery before networking; no thread; wire protocol untouched.
5. Preserve Before Replace confirmed: Sprint-011 write seam / managers / validators reused unchanged.
6. ADR-007…ADR-011 preserved; no ADR reinterpreted.
7. `Sprint-012-Save-Load.md` → Implemented / Verified / Closed / Frozen; this plan → EXECUTED & CLOSED; status docs + README synchronized.
8. Sprint-013 (Lua Integration) readiness stated.

## Compatibility & non-regression checklist (every step)
- Sprint-011 **599/599** baseline preserved; new tests only add.
- Engine headers only in `EngineAdapters.cpp`; OS/file headers only in `PlatformSaveStore.cpp`; no wire change; no thread; no new `tick_order` key.
- Frozen `IPersistenceStore` write seam reused unchanged; Sprint-011 managers/validators/version manager reused without redesign.
- Read-only snapshot consumption for saving; authoritative restoration only through sanctioned engine seams; recovery before networking; ADR-007…ADR-011 preserved.

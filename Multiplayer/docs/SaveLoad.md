# Save/Load System (Sprint-012)

Host-authoritative, deterministic save and recovery for STALKER-MP, built on the
frozen Sprint-011 Persistence Framework. The subsystem is engine-free and
platform-free at its core: engine writes are confined to `EngineAdapters.cpp` and
filesystem I/O to `PlatformSaveStore.cpp`. Every failure is a value outcome
(`SaveLoadOutcome`); nothing throws, no thread is created, and no new `tick_order`
key is introduced.

## Boundary decisions

- **D-SL1 — restoration crosses the engine boundary only through sanctioned seams.**
  The engine-free core parses a save into value-only **restoration records** and
  applies them through the six engine-free **restore-sink** interfaces
  (`IWorldRestoreSink`, `IEnvironmentRestoreSink`, `IEntityRestoreSink`,
  `IPlayerRestoreSink`, `IAlifeRestoreSink`, `ISchedulerRestoreSink`). The real
  implementations live only in `EngineAdapters.cpp` and write authoritative state
  through the sanctioned Sprint-003 Entity Registry, Sprint-005 `IAlifeSwitchGateway`,
  and Sprint-007 player seams. Test builds use null/recording sinks. (ADR-008.)
- **D-SL2 — one platform boundary.** The write store (`IPersistenceStore`, the frozen
  Sprint-011 seam, reused unchanged) and the additive read source (`ISaveSource`) are
  paired in one `ISaveBackend`. The real filesystem backend is confined to
  `PlatformSaveStore.cpp` (atomic temp-write + rename); tests use the in-memory
  backend. (ADR-009.)
- **D-SL3 — reuse Sprint-011 unchanged.** Saving reuses `PersistenceManager` at
  `kPersistence = 500`; version compatibility reuses `VersionManager` /
  `ValidationFramework`.
- **D-SL4 — the X-Ray `CSaveGame` remains the owner of base object-state
  restoration.** The MP restore sinks apply the multiplayer-authoritative layer on
  top; they never replace engine save ownership.

## Save pipeline

Saving is a per-frame concern owned by the frozen `PersistenceManager` at the
reserved `kPersistence = 500` tick (strictly after Snapshot 400 / Replication 450,
before Networking 900). `SaveManager` coordinates explicit saves:

`CreateSave` / `OverwriteSave` → `SaveWriter::Serialize(view, metadata)` →
`ISaveBackend::Store()` (`Begin`/`Write`/`Commit`). `DeleteSave` removes a save;
`EnumerateSaves` lists descriptors (ascending); `ValidateSave` reads and re-checks a
save without restoring it.

`SaveWriter` emits a deterministic, little-endian byte stream: a file header (magic
`0x53504D53`, format version `1`), then fixed-order sections
(World → Environment → Registry → Players → ALife → Scheduler → Metadata), then an
FNV-1a checksum trailer. Wall-clock and other diagnostic values are **excluded** from
the serialized identity, so a given world state serializes byte-identically every
time.

## Load / recovery pipeline

Loading is **not** a per-frame tick. It runs **once** during `Bootstrap Initialize`,
after world Start groundwork but **before** `CreateEngineFrameBridge` and before
networking accepts connections — guaranteeing "simulation starts before networking"
with no new `tick_order` key and no new subscriber.

`RecoveryPipeline::Recover(saveId, sinks)` runs, in order:

1. **Load** — `ISaveSource::Read`; absent → `NothingToLoad`.
2. **Parse** — `SaveReader::Parse`; bad magic/framing → `CorruptedSave`, format
   mismatch → `VersionMismatch`, bad trailer → `ChecksumFailure`.
3. **Validate** — `SaveIntegrityValidator` (counts, registry, references, version)
   via the reused `VersionManager`/`ValidationFramework`; failure → `IntegrityFailure`
   / `VersionMismatch`.
4. **Migrate** — `SaveMigrator` to the current build; no path → `VersionUnsupported`.
5. **Restore** — through the restore-sink boundary in fixed order
   World → Environment → Entities → Players → ALife → Scheduler.

Players are restored **offline**; they reconnect through the networking layer, which
comes up only after recovery completes.

## Recovery isolation

Any failure in steps 1–4 returns its specific value outcome with **nothing applied** —
the freshly-started world is left intact. A failure mid-restore (step 5) returns
`PartialRecovery` with the `reachedPhase` recorded. In `Bootstrap`, a non-`Ok`
recovery is logged and non-fatal: the server continues with the freshly-started
world. A corrupted or partial save can therefore never corrupt a running or
freshly-initialized world.

## Migration & version policy

The on-disk **save format version** (`kSaveFormatVersion`) gates parsing. Semantic
compatibility is decided by `VersionManager`: a differing `compatibility` version is
`Incompatible` (rejected); matching `compatibility` with an older
persistence/world/schema version is `MigrationRequired` and handed to `SaveMigrator`,
whose registered step functions upgrade the loaded save to the current build before
restoration. The wire protocol is untouched (ADR-010); migration is additive.

## Diagnostics

`SaveLoadDiagnostics` is a pure, non-invasive collector of monotonic counters and
deterministic aggregates (recovery success rate, average save/load durations).
Timing and wall-clock are diagnostic only and never participate in replay identity or
control flow.

## Composition & teardown

`Bootstrap` binds the build-selected backend via
`adapters::CreateEngineSaveBackend` (real filesystem in the engine build, in-memory in
tests) and the restore-sink bundle via `adapters::CreateEngineRestoreSinks` (real
engine sinks / null sinks). Both are Runtime-owned; the backend's `Store()` (the
frozen `IPersistenceStore` seam) replaces the Sprint-011 in-memory store behind the
same seam, leaving the `PersistenceManager` and its tick unchanged. Teardown is
reverse-order: the frame bridge is removed first; the save backend and the one-shot
restore-sink bundle outlive `ShutdownAll` and are destroyed with the Runtime (the
restore-sink bundle is not a subscriber — nothing to unsubscribe).

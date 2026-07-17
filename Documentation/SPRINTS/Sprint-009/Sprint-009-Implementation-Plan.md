# Sprint-009 — Replication Pipeline · Complete Implementation Plan (all step specifications)

| Field | Value |
|---|---|
| Sprint | 009 — Replication Pipeline |
| Status | **FROZEN — single implementation authority for Sprint-009** (approved 2026-07-16; supersedes the standalone Step-01 spec) |
| Scope authority | `Documentation/SPRINTS/Sprint-009/Sprint-009-Delta-Replication.md` (FROZEN) |
| Baseline | Sprint-008 CLOSED & FROZEN — **377 / 377** tests passing (GCC + MSVC) |
| Governing ADRs | ADR-007, ADR-008, ADR-009, ADR-010, ADR-011 (all preserved; none modified) |
| Steps | 16 (Step-01 … Step-16), strict additive dependency chain |

---

## 0. Governance & preserved invariants (apply to every step)

This plan is **purely additive**. No step redesigns or modifies the Sprint-009 architecture or any earlier sprint's frozen documents, code, or ADRs. Every step preserves, without exception:

- **Preserve Before Replace** — replication reuses the Sprint-006 transport/session/message-registry, the Sprint-004 Bubble membership, the Sprint-007 read-only player surface, and the Sprint-008 immutable snapshot; it reimplements none of them.
- **Host Authority** — replication only transports authoritative state to clients; clients receive, never own. Authority flags are read-only captured values; inbound client messages (acks) never mutate simulation.
- **One World / One ALife Simulation** — no second world/simulation; replication consumes a read-only projection.
- **Deterministic Simulation & Replay Determinism** — every value type and pipeline stage has a fixed, deterministic layout/ordering; wall-clock appears only in explicitly diagnostic fields excluded from replay identity; identical snapshot + client baseline yields identical replication content and byte layout.
- **One Engine Boundary** — Sprint-009 introduces **no** engine-touching code and **no** new engine translation unit; every input is already behind an engine-free seam. Engine headers remain confined to `EngineAdapters.cpp`.
- **One Platform Boundary** — Sprint-009 introduces **no** OS/socket code; assembled payloads are handed to the existing Sprint-006 transport. OS headers remain confined to `PlatformSockets.cpp`.
- **ADR-007** — no exceptions, no RTTI, no iostream; all fallible operations are value outcomes (`core::Expected<T>` / `ReplicationOutcome`).
- **ADR-008** — replication performs read-only observation of snapshots and subsystem surfaces; it mutates no engine or simulation state.
- **ADR-009** — the platform boundary is untouched; replication is transport-agnostic behind the Sprint-006 seam.
- **ADR-010** — the wire protocol is extended **only additively**: new DATA-range message ids (`kMsgReplicationUpdate`, `kMsgReplicationAck`) are appended and never renumbered; fixed little-endian framing; control/data id ranges respected.
- **ADR-011** — Sprint-009 creates **no** thread. The replication worker is *designed* for a future worker thread behind the single-producer/multi-consumer snapshot seam, but is exercised synchronously at a deterministic tick (mirroring Sprint-008's queue). One new `tick_order` key (`kReplicationPipeline = 450`) is allocated in the sanctioned central table, strictly after Snapshot (400) and before Persistence (500)/Networking (900).

**Compatibility.** Every step is additive and leaves the Sprint-008 **377/377** baseline intact; new tests only add to the count. No prior public API changes.

---

## 1. Subsystem shape (shared by all steps)

- **Namespace:** `stalkermp::replication` (abbreviated `replication::`).
- **Headers:** `Multiplayer/include/stalkermp/replication/`.
- **Sources:** `Multiplayer/src/replication/`.
- **Tests:** `Multiplayer/tests/ReplicationTests.cpp` (accumulating) and additions to `Multiplayer/tests/BootstrapTests.cpp` at the wiring step.
- **Subsystem doc:** `Multiplayer/docs/Replication.md` (authored at Step-15).
- **New tick slot:** `core::tick_order::kReplicationPipeline = 450` (allocated at Step-11; asserted `kReplication (400) < kReplicationPipeline (450) < kPersistence (500) < kNetworking (900)`).
- **New additive message ids (ADR-010):** `net::kMsgReplicationUpdate = 0x0200` (host→client), `net::kMsgReplicationAck = 0x0201` (client→host), introduced at Step-09/Step-10; never renumbered.
- **No new engine TU; no new OS code.**

### 1.1 Step → architecture / evidence-gate mapping

| Step | Delivers | Architecture ref (Sprint-009 doc) | Evidence gate |
|---|---|---|---|
| 01 | Replication value types & vocabulary | §7.6/§7.7 (value shapes) | E-G1-R groundwork |
| 02 | `ReplicationConfiguration` (+ `FromConfig`) | §7.1 | — |
| 03 | Immutable `ReplicationUpdate` + `IReplicationView` | §7.2/§7.11 | E-G1-R (immutability) |
| 04 | `ReplicationClientRegistry` (per-client baselines) | §7.1/§7.5 | — |
| 05 | Interest management seam + Bubble policy | §7.4 | E-G3-R |
| 06 | Delta generation | §7.5 | E-G2-R groundwork |
| 07 | Reliability & priority classification | §7.8/§7.9 | E-G4-R |
| 08 | Replication queues | §7.10 | — |
| 09 | Packet assembly (additive wire ids) | §7.11 (ADR-010) | — |
| 10 | Replication Worker (synchronous consumer) | §7.2/§7.3 | E-G1-R/E-G5-R |
| 11 | `ReplicationManager` (`IService`+`ITickable`) at 450 | §7.1 | E-G5-R |
| 12 | Bootstrap wiring at `kReplicationPipeline = 450` | §7.1 | — |
| 13 | `ReplicationDiagnostics` | §7.12 | — |
| 14 | Validation hardening (negative surface) | §7.14 | E-G* negatives |
| 15 | Integration + documentation | §7.15/§7.16 | E-G2-R/E-G3-R/E-G4-R composed |
| 16 | Sprint closure | §12 | all gates recorded |

### 1.2 Sprint-009 evidence gates (closed by their owning steps)

- **E-G1-R** — replication consumes immutable snapshots only; no live object or raw pointer is captured (value-only).
- **E-G2-R** — deterministic build/delta: identical snapshot + baseline → identical replication content and monotonic `ReplicationId`.
- **E-G3-R** — interest correctness: only relevant entities are replicated per client.
- **E-G4-R** — reliability/priority classification correct; priority ordering total and deterministic.
- **E-G5-R** — worker independence: replication never mutates simulation/engine; networking stays a consumer.

### 1.3 Dependency chain (explicit; sequential, no revisiting)

`01 types → 02 config → 03 update+view → 04 client registry → 05 interest → 06 delta → 07 classify → 08 queues → 09 packet assembly → 10 worker → 11 manager → 12 wiring → 13 diagnostics → 14 hardening → 15 integration+docs → 16 closure.`

Each step compiles and is tested against only already-delivered steps. No step needs a later step to compile or verify.

---

# Step-01 — Replication value types & vocabulary

**Objective.** Establish the engine-free/OS-free foundational vocabulary (identity types, enums, POD value structs) all later steps build on. Types only — no logic, no service, no tick, no thread, no packets.

**Scope — In.** New header `include/stalkermp/replication/ReplicationTypes.h`: `ClientId`, `ReplicationId` (u64, `0 = none`, `IsNone`/`==`/`!=`); `enum class : std::uint8_t` — `ReplicationReliability{Unreliable=0,Reliable}`, `ReplicationPriority{Low=0,Medium,High}`, `ReplicationChannel{Unreliable=0,Reliable,Control}`, `ReplicationState{Pending=0,Built,Queued,Sent,Acknowledged,Dropped}`, `ReplicationOutcome{Ok=0,ClientUnknown,SnapshotUnavailable,Overflow,NotRelevant}` — each with a total `Name()` helper; POD structs `EntityReplicationState`, `PlayerReplicationState`, `ReplicationMetadata`, `ReplicationStatistics`, `ReplicationConsistency` (+ `IsHealthy()`). New TU `tests/ReplicationTests.cpp`.
**Scope — Out.** Configuration (Step-02); any behavior; any tick/service/thread/packet/message-id.

**Functional Requirements.** FR-1 header compiles standalone, depending only on `<cstdint>`, `world/WorldTypes.h`, `player/PlayerTypes.h`, `snapshot/SnapshotTypes.h`. FR-2 identity types are u64 with `0 = none`, `constexpr` equality. FR-3 every enum is `: std::uint8_t` with reserved neutral `0` and total `Name()` (incl. `"Unknown"`). FR-4 `EntityReplicationState`/`PlayerReplicationState` are value-only (no pointer/handle). FR-5 `ReplicationMetadata` records provenance (`ReplicationId`, `ClientId`, `sourceSnapshotId`, `sourceSnapshotTick`, counts, reliability, state, diagnostic `timestampWallClock`). FR-6 `ReplicationStatistics` = monotonic counters, zero defaults. FR-7 `ReplicationConsistency` bool invariants default `true`; `IsHealthy()` = conjunction. FR-8 all structs trivially copyable, deterministic layout, zero/`none` defaults. FR-9 no `.cpp` needed (all `constexpr`/`inline`).

**Non-Functional Requirements.** ADR-007 (exception/RTTI/iostream-free); deterministic layout; engine/OS-free; additive (377/377 preserved); enums fixed underlying type + reserved 0 (forward-compatible).

**Public Interfaces.** All in namespace `stalkermp::replication`, header-only:

```cpp
// Identity (0 = none); constexpr operator== / operator!= for each
struct ClientId       { std::uint64_t value = 0; [[nodiscard]] constexpr bool IsNone() const noexcept; };
struct ReplicationId  { std::uint64_t value = 0; [[nodiscard]] constexpr bool IsNone() const noexcept; };

// Enumerations (fixed std::uint8_t; 0 = neutral/safe) + total Name() helpers
enum class ReplicationReliability : std::uint8_t { Unreliable = 0, Reliable };
enum class ReplicationPriority    : std::uint8_t { Low = 0, Medium, High };
enum class ReplicationChannel     : std::uint8_t { Unreliable = 0, Reliable, Control };
enum class ReplicationState       : std::uint8_t { Pending = 0, Built, Queued, Sent, Acknowledged, Dropped };
enum class ReplicationOutcome     : std::uint8_t { Ok = 0, ClientUnknown, SnapshotUnavailable, Overflow, NotRelevant };

[[nodiscard]] constexpr const char* ReplicationReliabilityName(ReplicationReliability) noexcept;
[[nodiscard]] constexpr const char* ReplicationPriorityName(ReplicationPriority) noexcept;
[[nodiscard]] constexpr const char* ReplicationChannelName(ReplicationChannel) noexcept;
[[nodiscard]] constexpr const char* ReplicationStateName(ReplicationState) noexcept;
[[nodiscard]] constexpr const char* ReplicationOutcomeName(ReplicationOutcome) noexcept;

// Value captures (values only — never a live object or pointer)
struct EntityReplicationState {
    world::EntityId id{};                 // 0 = none
    world::Vec3 position{};
    world::Vec3 velocity{};
    std::uint32_t stateFlags = 0;         // opaque
    std::uint32_t version = 0;            // per-entity replication version
    ReplicationPriority priority = ReplicationPriority::Low;
    ReplicationReliability reliability = ReplicationReliability::Unreliable;
};

struct PlayerReplicationState {
    player::PlayerId id{};                // 0 = none
    world::EntityId entity{};             // 0 = none
    world::Vec3 position{};
    player::PlayerConnectionState connectionState = player::PlayerConnectionState::Connected;
    std::uint32_t authorityFlags = 0;     // opaque, host-authoritative (read-only)
    std::uint32_t version = 0;
};

struct ReplicationMetadata {
    ReplicationId id{};
    ClientId client{};
    std::uint64_t sourceSnapshotId = 0;   // provenance: which SimulationSnapshot
    std::uint64_t sourceSnapshotTick = 0;
    std::uint32_t entityCount = 0;
    std::uint32_t playerCount = 0;
    std::uint32_t byteCount = 0;          // reserved (0 until packet assembly)
    ReplicationReliability reliability = ReplicationReliability::Unreliable;
    ReplicationState state = ReplicationState::Pending;
    std::uint64_t timestampWallClock = 0; // DIAGNOSTIC ONLY — not replay identity
};

struct ReplicationStatistics {
    std::uint64_t updatesBuilt = 0;
    std::uint64_t updatesSent = 0;
    std::uint64_t updatesDropped = 0;
    std::uint64_t bytesSent = 0;
    std::uint32_t entitiesReplicated = 0;
    std::uint32_t playersReplicated = 0;
    std::uint32_t activeClients = 0;
    std::uint64_t lastBuildDurationTicks = 0;
};

struct ReplicationConsistency {
    bool idMonotonic = true;
    bool consumesImmutableSnapshotsOnly = true;
    bool noLiveObjectCaptured = true;
    bool interestRelevantOnly = true;
    bool reliabilityClassified = true;
    bool prioritiesOrdered = true;
    bool noSimulationMutation = true;
    [[nodiscard]] bool IsHealthy() const noexcept; // conjunction of all flags
};
```

No other symbols are introduced (no class with behavior, no service, no tick, no thread).

**Integration Points.** Reuses `world::EntityId`/`world::Vec3`, `player::PlayerId`/`player::PlayerConnectionState`; references `snapshot::SnapshotMetadata` identity as integers only. No wiring, no message id, no tick key.

**Validation Requirements.** `static_assert` on enum underlying type and on trivial-copyability of every value struct; unit checks for `none`/equality, `Name()` totality (+`"Unknown"`), default states, and `ReplicationConsistency` health toggling.

**Evidence Gates.** E-G1-R groundwork (value-only vocabulary, no pointers, POD). No gate fully closed.

**Acceptance Criteria.** Header present, engine-free, compiles standalone; all types/enums/helpers exactly as specified; value structs trivially copyable with documented defaults; `ReplicationTests.cpp` passes on GCC + MSVC; full suite green, zero new warnings; `377 + N` tests; no prior API change; no tick key/service/message-id/thread/engine/OS added.

**Files Created/Modified.** Create `include/stalkermp/replication/ReplicationTypes.h`, `tests/ReplicationTests.cpp`. Modify `xrMP.vcxproj` (ClInclude), `tests/xrMP_Tests.vcxproj` (ClCompile). Registration only.

**Test Requirements.** New `ReplicationTests.cpp` groups: enum layout/`Name()`; identity `none`/equality; POD static-asserts; struct defaults; consistency health. Engine/OS/thread-free; `-Wall -Wextra -Werror` clean.

**Documentation Updates.** Local `CURRENT_STATUS.md`/`SESSION_LOG.md`: begin Sprint-009 entry (Step-01 next). No README change at spec time; no graphics; no architecture/ADR edits.

**Recommended Git Commit Message (spec freeze only).**
```
docs(replication): freeze Sprint-009 Step-01 spec (replication value types & vocabulary)
```

---

# Step-02 — `ReplicationConfiguration` (+ `FromConfig`)

**Objective.** Define the engine-free replication configuration value type and its deterministic `FromConfig` parser, consuming only Step-01 types.

**Scope — In.** `include/stalkermp/replication/ReplicationConfiguration.h` (+ `src/replication/ReplicationConfiguration.cpp`): fields `maxClients`, `maxEntitiesPerUpdate`, `maxPlayersPerUpdate`, `reliableQueueDepth`, `unreliableQueueDepth`, `retryLimit`, `bandwidthBudgetBytesPerTick`, `interestRadiusMeters`, `updateBudgetTicks`, `version`; static `Expected<ReplicationConfiguration> FromConfig(const core::ConfigStore&)` reading the `[replication]` section with per-field defaults and cross-field validation.
**Scope — Out.** Any use of the config (later steps); any behavior beyond parse/validate.

**Functional Requirements.** FR-1 documented defaults when `[replication]` absent. FR-2 each field parses a valid supplied value. FR-3 invalid/negative values rejected as `Expected` errors. FR-4 cross-field invariants: `maxClients >= 1`, `reliableQueueDepth >= 1`, `unreliableQueueDepth >= 1`, `retryLimit >= 1`, `version >= 1`, `maxEntitiesPerUpdate >= 1`, `maxPlayersPerUpdate >= 1`. FR-5 `FromConfig` never throws, never mutates the store, performs no I/O beyond the store read.

**Non-Functional Requirements.** ADR-007; deterministic; engine/OS-free; additive. Uses `common::Format` for error messages (iostream-free).

**Public Interfaces.** `struct ReplicationConfiguration { std::uint32_t maxClients; std::uint32_t maxEntitiesPerUpdate; std::uint32_t maxPlayersPerUpdate; std::uint32_t reliableQueueDepth; std::uint32_t unreliableQueueDepth; std::uint32_t retryLimit; std::uint32_t bandwidthBudgetBytesPerTick; std::uint32_t interestRadiusMeters; std::uint32_t updateBudgetTicks; std::uint32_t version; static core::Expected<ReplicationConfiguration> FromConfig(const core::ConfigStore&); };` with documented default values.

**Integration Points.** Consumes `core::ConfigStore` (Sprint-001) and `core::Expected` only. No wiring. The default `[replication]` block is added to the server-config template at the Bootstrap-wiring step (Step-12), not here.

**Validation Requirements.** Unit tests: defaults when section absent; each override parsed; invalid per-field rejected; each cross-field invariant rejected; equal-boundary accepted.

**Evidence Gates.** None (configuration groundwork).

**Acceptance Criteria.** Header+cpp present, engine-free; `FromConfig` matches the FR matrix; tests pass GCC + MSVC; full suite green (`prev + N`), zero new warnings; no prior API change.

**Files Created/Modified.** Create `ReplicationConfiguration.h`/`.cpp`; add tests to `ReplicationTests.cpp`; register in both vcxprojs.

**Test Requirements.** `ReplicationConfigStep2` group: defaults, overrides, invalid-per-field, cross-field, boundary-equal.

**Documentation Updates.** Local status/log only. No README/graphics/ADR change.

**Recommended Git Commit Message.**
```
docs(replication): freeze Sprint-009 Step-02 spec (ReplicationConfiguration + FromConfig)
```

---

# Step-03 — Immutable `ReplicationUpdate` + `IReplicationView`

**Objective.** Define the per-client immutable replication payload aggregate and its const-only consumer view — the "ReplicationSnapshot" projection reserved in Sprint-008 §22. Build-then-freeze lifecycle; values only.

**Scope — In.** `include/stalkermp/replication/IReplicationView.h` (const interface: `Metadata()`, `Entities()` ascending `EntityId`, `Players()` ascending `PlayerId`) and `include/stalkermp/replication/ReplicationUpdate.h` (+ `.cpp`): `final : IReplicationView` with `BeginBuild(ReplicationId, ClientId, sourceSnapshotId, sourceSnapshotTick) noexcept`; `AddEntity(EntityReplicationState)` / `AddPlayer(PlayerReplicationState)` → `Expected<void>` (require Building; ascending-unique; id 0 rejected); `SetReliability(ReplicationReliability)`; `Finalize()` → `Expected<void>` (Building→Finalized, stamps counts). Post-finalize mutators reject.
**Scope — Out.** Building from a real snapshot (Step-10 worker); interest/delta/classification.

**Functional Requirements.** FR-1 `BeginBuild` resets to `Building` and stamps metadata. FR-2 immutable after `Finalize()` — every mutator rejects (E-G1-R). FR-3 values only; ascending-unique entity/player ids; id 0 rejected. FR-4 counts stamped at finalize; state transitions `Building→Finalized`. FR-5 const-only `IReplicationView` is the sole consumer surface. FR-6 deterministic: identical build sequence → identical content and metadata (wall-clock excluded).

**Non-Functional Requirements.** ADR-007; deterministic; engine/OS-free; additive.

**Public Interfaces.** `class IReplicationView { virtual const ReplicationMetadata& Metadata() const=0; virtual const std::vector<EntityReplicationState>& Entities() const=0; virtual const std::vector<PlayerReplicationState>& Players() const=0; virtual ~IReplicationView()=default; };` and the `ReplicationUpdate` build API above (`State()`, `IsFinalized()` observers included).

**Integration Points.** Consumes Step-01 value types. References `snapshot::ISnapshotView` only conceptually (the worker at Step-10 supplies captured values); Step-03 does not hold a snapshot.

**Validation Requirements.** Unit tests: build→finalize content/counts/state; ascending-unique/zero rejection; immutability after finalize (all mutators + double-finalize rejected); deterministic replay of an identical build.

**Evidence Gates.** **E-G1-R (immutability)** established for the replication payload.

**Acceptance Criteria.** Interface + aggregate present; immutability enforced; ascending-unique enforced; deterministic; tests pass GCC + MSVC; suite green (`prev + N`); no prior API change.

**Files Created/Modified.** Create `IReplicationView.h`, `ReplicationUpdate.h`/`.cpp`; tests to `ReplicationTests.cpp`; register in both vcxprojs.

**Test Requirements.** `ReplicationUpdateStep3`: build/finalize, ascending-unique, immutable-after-finalize, deterministic-build.

**Documentation Updates.** Local status/log. No README/graphics/ADR change.

**Recommended Git Commit Message.**
```
docs(replication): freeze Sprint-009 Step-03 spec (immutable ReplicationUpdate + IReplicationView)
```

---

# Step-04 — `ReplicationClientRegistry` (per-client baselines)

**Objective.** Define the engine-free per-client replication state store that holds each client's identity, connection, focus position, and last-acknowledged baseline — the state delta generation (Step-06) reads and updates.

**Scope — In.** `include/stalkermp/replication/ReplicationClientRegistry.h` (+ `.cpp`): `ClientRecord { ClientId id; net::ConnectionId connection; world::PlayerPosition focus; std::uint64_t lastAckedReplicationId; std::uint64_t lastAckedSnapshotTick; bool active; }`; sorted-vector store keyed ascending by `ClientId` (+ connection accelerator); `Register`/`Unregister`/`FindById`/`FindByConnection`; `UpdateFocus`; `RecordAck`; `ActiveClients()` (ascending); `ValidateConsistency()`. Deterministic; non-reused ascending `ClientId`.
**Scope — Out.** Baseline entity-version tracking used by delta is defined here as an owned per-client container but populated by Step-06; no interest/delta logic.

**Functional Requirements.** FR-1 register/unregister transactional; ascending non-reused `ClientId`. FR-2 lookups by id and connection (read-only). FR-3 `RecordAck` updates last-acked baseline monotonically (older acks ignored). FR-4 `ActiveClients()` deterministic ascending. FR-5 capacity bounded by `maxClients`; over-capacity register → value outcome. FR-6 `ValidateConsistency` checks ordering/uniqueness/mapping.

**Non-Functional Requirements.** ADR-007; deterministic; engine/OS-free (reuses `net::ConnectionId` value type, no transport); additive.

**Public Interfaces.** The `ClientRecord` struct and the registry methods above, all returning value outcomes / const views; no mutation of any other subsystem.

**Integration Points.** Reuses `net::ConnectionId` (Sprint-006 value type) and `world::PlayerPosition` (Sprint-004). Consumes `ReplicationConfiguration.maxClients`. No transport, no session mutation.

**Validation Requirements.** Unit tests: register/find/unregister; capacity outcome; ascending order; monotonic ack; consistency healthy/negative.

**Evidence Gates.** None directly (supports E-G2-R/E-G3-R).

**Acceptance Criteria.** Registry present, engine-free, deterministic; bounded; tests pass GCC + MSVC; suite green; no prior API change.

**Files Created/Modified.** Create `ReplicationClientRegistry.h`/`.cpp`; tests to `ReplicationTests.cpp`; register in both vcxprojs.

**Test Requirements.** `ReplicationClientRegistryStep4`: lifecycle, capacity, ordering, ack monotonicity, consistency.

**Documentation Updates.** Local status/log. No README/graphics/ADR change.

**Recommended Git Commit Message.**
```
docs(replication): freeze Sprint-009 Step-04 spec (ReplicationClientRegistry + per-client baselines)
```

---

# Step-05 — Interest management seam + Bubble policy

**Objective.** Define the engine-free interest seam that selects the entities relevant to each client, and its concrete Bubble-based policy — reusing Sprint-004 activation and Sprint-007 player focus (Preserve Before Replace).

**Scope — In.** `include/stalkermp/replication/IInterestPolicy.h` (`SelectRelevant(const ClientRecord&, const snapshot::ISnapshotView&, std::vector<world::EntityId>& out) const`, append-only, ascending, values only) and `include/stalkermp/replication/BubbleInterestPolicy.h` (+ `.cpp`) consuming a read-only Bubble-membership seam + distance vs `interestRadiusMeters`. A test policy (`AllInterestPolicy`) may live in test support.
**Scope — Out.** Delta/classification/packets; any engine access (Bubble membership is read through the existing engine-free seam).

**Functional Requirements.** FR-1 `SelectRelevant` appends ascending, unique `EntityId`s; never clears pre-existing output. FR-2 selection deterministic for a given client + snapshot. FR-3 relevance = Bubble membership and/or within `interestRadiusMeters` of the client focus (frozen rule). FR-4 values only; no live object, no mutation. FR-5 policy is engine-free; the Bubble surface is consumed read-only.

**Non-Functional Requirements.** ADR-007/008 (read-only); deterministic; engine/OS-free; additive.

**Public Interfaces.** `class IInterestPolicy { virtual void SelectRelevant(const ClientRecord&, const snapshot::ISnapshotView&, std::vector<world::EntityId>&) const=0; virtual ~IInterestPolicy()=default; };` + `BubbleInterestPolicy` ctor taking the read-only Bubble seam + `interestRadiusMeters`.

**Integration Points.** Consumes `snapshot::ISnapshotView` (Sprint-008) and the Sprint-004 `BubbleManager` read-only membership surface; `world::PlayerPosition` focus from Step-04. No engine TU.

**Validation Requirements.** Unit tests: relevance ascending/unique; deterministic selection; radius cutoff; append-only; membership-driven inclusion/exclusion using a fake snapshot + fake bubble surface.

**Evidence Gates.** **E-G3-R (interest correctness)**.

**Acceptance Criteria.** Seam + policy present, engine-free, deterministic; only relevant entities selected; tests pass GCC + MSVC; suite green; no prior API change.

**Files Created/Modified.** Create `IInterestPolicy.h`, `BubbleInterestPolicy.h`/`.cpp`; test support policy; tests to `ReplicationTests.cpp`; register in both vcxprojs.

**Test Requirements.** `InterestStep5`: ascending/unique, determinism, radius, membership, append-only.

**Documentation Updates.** Local status/log. No README/graphics/ADR change.

**Recommended Git Commit Message.**
```
docs(replication): freeze Sprint-009 Step-05 spec (interest seam + BubbleInterestPolicy)
```

---

# Step-06 — Delta generation

**Objective.** Define the deterministic, value-only delta encoder that turns a client's previous baseline and the current relevant entity/player states into a minimal change set (added / changed / removed), bumping per-entity versions.

**Scope — In.** `include/stalkermp/replication/DeltaEncoder.h` (+ `.cpp`): `struct ReplicationChangeSet { std::vector<EntityReplicationState> added; std::vector<EntityReplicationState> changed; std::vector<world::EntityId> removed; std::vector<PlayerReplicationState> playersChanged; }` (all ascending); `EncodeEntityDelta(previousBaseline, currentRelevant) -> ReplicationChangeSet`; per-client baseline container update. Deterministic; excludes unchanged entities.
**Scope — Out.** Classification/queues/packets; snapshot acquisition.

**Functional Requirements.** FR-1 unchanged entities are omitted (bandwidth minimization). FR-2 added = in current, not in baseline; removed = in baseline, not in current; changed = in both with differing tracked fields. FR-3 all outputs ascending by id; deterministic for identical inputs. FR-4 version bumped on change; values only. FR-5 baseline update is pure (returns the new baseline; no external mutation of simulation).

**Non-Functional Requirements.** ADR-007; deterministic (E-G2-R); engine/OS-free; bounded by `maxEntitiesPerUpdate` (Overflow value outcome); additive.

**Public Interfaces.** `ReplicationChangeSet` and `EncodeEntityDelta(...)` plus a `PlayerReplicationState` delta helper; all value-in/value-out.

**Integration Points.** Consumes Step-01 states + Step-04 baseline container. No engine, no transport.

**Validation Requirements.** Unit tests: added/changed/removed correctness; unchanged omitted; ascending outputs; determinism (two runs identical); version bump; overflow outcome.

**Evidence Gates.** **E-G2-R groundwork** (deterministic delta).

**Acceptance Criteria.** Encoder present, deterministic, minimal; bounded; tests pass GCC + MSVC; suite green; no prior API change.

**Files Created/Modified.** Create `DeltaEncoder.h`/`.cpp`; tests to `ReplicationTests.cpp`; register in both vcxprojs.

**Test Requirements.** `DeltaStep6`: change classification, omission, ordering, determinism, version, overflow.

**Documentation Updates.** Local status/log. No README/graphics/ADR change.

**Recommended Git Commit Message.**
```
docs(replication): freeze Sprint-009 Step-06 spec (deterministic delta generation)
```

---

# Step-07 — Reliability & priority classification

**Objective.** Define the pure, total classifier that assigns exactly one Reliability, Priority, and Channel to every `ReplicationChangeKind`, per the **complete, authoritative classification table (§7.A below)**. The Sprint-009 design tables §7.8 (priority) and §7.9 (reliability) do not define a full cross-product; §7.A completes them here so the mapping is total and unambiguous. This table is FROZEN — the classifier encodes it verbatim and invents no defaults.

**Scope — In.** `include/stalkermp/replication/ReplicationClassifier.h` (pure `constexpr`, header-only — a total constexpr mapping belongs in the header; no `.cpp` is emitted): the `ReplicationChangeKind` enum with a total `Name()`, and the pure `constexpr` functions `ClassifyReliability(kind) -> ReplicationReliability`, `ClassifyPriority(kind) -> ReplicationPriority`, `ClassifyChannel(kind) -> ReplicationChannel`, each returning exactly the §7.A value.
**Scope — Out.** Queue behavior/packets; any state.

**§7.A Frozen classification table (authoritative; every kind assigned exactly once).**

`ReplicationChannel` is derived: `Reliable` reliability → `ReplicationChannel::Reliable`; `Unreliable` reliability → `ReplicationChannel::Unreliable` (the `Control` channel is reserved for future control messages and is assigned to no data kind). Priority underlying order is total: `High (2) > Medium (1) > Low (0)`.

| ReplicationChangeKind | Reliability | Priority | Channel | Source |
|---|---|---|---|---|
| `None` (0, sentinel) | Unreliable | Low | Unreliable | Neutral sentinel (no data) |
| `Player` | Unreliable | High | Unreliable | Priority §7.8 (Players=High); reliability frozen here (continuous player state → Unreliable) |
| `Combat` | Reliable | High | Reliable | Priority §7.8 (Combat=High); reliability frozen here (must arrive → Reliable) |
| `Damage` | Reliable | High | Reliable | Priority §7.8 (Damage=High); reliability frozen here (must arrive → Reliable) |
| `EntitySpawn` | Reliable | High | Reliable | §7.8 High + §7.9 Reliable |
| `EntityRemove` | Reliable | High | Reliable | §7.8 High (Entity Destruction) + §7.9 Reliable (Entity Removal) |
| `NearbyNpc` | Unreliable | Medium | Unreliable | Priority §7.8 (Nearby NPCs=Medium); reliability frozen here (frequent state → Unreliable) |
| `Inventory` | Reliable | Medium | Reliable | §7.8 Medium + §7.9 Reliable |
| `Animation` | Unreliable | Medium | Unreliable | §7.8 Medium + §7.9 Unreliable |
| `AmbientObject` | Unreliable | Low | Unreliable | Priority §7.8 (Ambient=Low); reliability frozen here (cosmetic → Unreliable) |
| `WeatherUpdate` | Unreliable | Low | Unreliable | Priority §7.8 (Weather=Low); reliability frozen here (cosmetic → Unreliable) |
| `DistantEntity` | Unreliable | Low | Unreliable | Priority §7.8 (Distant=Low); reliability frozen here (frequent state → Unreliable) |
| `Position` | Unreliable | Medium | Unreliable | Reliability §7.9 (Unreliable); priority frozen here (movement → Medium) |
| `Rotation` | Unreliable | Medium | Unreliable | Reliability §7.9 (Unreliable); priority frozen here (movement → Medium) |
| `Velocity` | Unreliable | Medium | Unreliable | Reliability §7.9 (Unreliable); priority frozen here (movement → Medium) |
| `Camera` | Unreliable | Low | Unreliable | Reliability §7.9 (Unreliable); priority frozen here (least critical → Low) |
| `QuestUpdate` | Reliable | Medium | Reliable | Reliability §7.9 (Reliable); priority frozen here (gameplay, non-urgent → Medium) |
| `PlayerJoin` | Reliable | High | Reliable | Reliability §7.9 (Reliable); priority frozen here (player lifecycle → High) |
| `PlayerLeave` | Reliable | High | Reliable | Reliability §7.9 (Reliable); priority frozen here (player lifecycle → High) |

Rows sourced purely from §7.8/§7.9 are the original frozen design; rows marked "frozen here" complete the missing dimension and are now equally authoritative. `ReplicationChangeKind` enumerators are appended, never renumbered; adding a future kind requires adding its §7.A row (no silent default).

**Functional Requirements.** FR-1 the reliable set is exactly {EntitySpawn, EntityRemove, Inventory, QuestUpdate, PlayerJoin, PlayerLeave, Combat, Damage}; the unreliable set is exactly {None, Player, NearbyNpc, Animation, AmbientObject, WeatherUpdate, DistantEntity, Position, Rotation, Velocity, Camera} (per §7.A). FR-2 priority bands per §7.A: High = {Player, Combat, Damage, EntitySpawn, EntityRemove, PlayerJoin, PlayerLeave}; Medium = {NearbyNpc, Inventory, Animation, Position, Rotation, Velocity, QuestUpdate}; Low = {None, AmbientObject, WeatherUpdate, DistantEntity, Camera}. FR-3 total mapping — every kind classified by the §7.A table; deterministic; pure (no state); no invented default. FR-4 priority ordering total (`High > Medium > Low`). FR-5 channel derives from reliability exactly as stated above.

**Non-Functional Requirements.** ADR-007; deterministic (E-G4-R); engine/OS-free; additive.

**Public Interfaces.** The `ReplicationChangeKind` enum (`: std::uint8_t`, `Name()`), and the three `constexpr` classify functions.

**Integration Points.** Consumes Step-01 enums only. No engine/transport/state.

**Validation Requirements.** Unit tests: reliability/priority/channel for each kind; totality; determinism; priority ordering.

**Evidence Gates.** **E-G4-R (classification correct)**.

**Acceptance Criteria.** Classifier present, pure, total, deterministic; tests pass GCC + MSVC; suite green; no prior API change.

**Files Created/Modified.** Create `ReplicationClassifier.h` (header-only, pure `constexpr`; no `.cpp`); tests to `ReplicationTests.cpp`; register the header in `xrMP.vcxproj`.

**Test Requirements.** `ClassifierStep7`: per-kind reliability/priority/channel matches §7.A for **every** kind; the full §7.A table (all 19 kinds) is asserted; totality (`Name()` total, no fallthrough), determinism (`constexpr` `static_assert`), priority ordering.

**Documentation Updates.** Local status/log. No README/graphics/ADR change.

**Recommended Git Commit Message.**
```
docs(replication): freeze Sprint-009 Step-07 spec (reliability & priority classification)
```

---

# Step-08 — Replication queues

**Objective.** Define the exception-free, fixed-capacity queue set (reliable / unreliable / priority / retry / outgoing) with deterministic priority ordering and bounded memory.

**Scope — In.** `include/stalkermp/replication/ReplicationQueues.h` (+ `.cpp`): a `QueuedRecord` (metadata + reliability + priority + retry count); `Enqueue(QueuedRecord) -> ReplicationOutcome` (Overflow when full); `DequeueNext()` returning the highest-priority, then FIFO-within-band, then reliable-before-unreliable record; retry-queue handling (`RequeueForRetry`, bounded by `retryLimit`); `Depth()`/`Full()`; pre-reserved storage (no steady-state allocation).
**Scope — Out.** Packet assembly/transport; snapshot/worker.

**Functional Requirements.** FR-1 deterministic dequeue order: priority band (High→Low), then FIFO within band, reliable ahead of unreliable at equal priority. FR-2 capacity from config; Overflow value outcome; no throw. FR-3 retry requeue bounded by `retryLimit`; exceeding → Dropped (counter). FR-4 pre-reserved; no steady-state heap allocation. FR-5 all operations value outcomes.

**Non-Functional Requirements.** ADR-007 (exception-free bounded memory); deterministic; engine/OS-free; additive.

**Public Interfaces.** `QueuedRecord`, the queue class with `Reserve`, `Enqueue`, `DequeueNext`, `RequeueForRetry`, `Depth`, `Full`, `Dropped`.

**Integration Points.** Consumes Step-01 types + Step-07 classification + config depths. No transport.

**Validation Requirements.** Unit tests: priority/FIFO/reliable ordering; overflow; retry bound + drop; bounded memory; deterministic sequence.

**Evidence Gates.** None directly (supports E-G4-R ordering + E-G5-R independence).

**Acceptance Criteria.** Queues present, exception-free, bounded, deterministic; tests pass GCC + MSVC; suite green; no prior API change.

**Files Created/Modified.** Create `ReplicationQueues.h`/`.cpp`; tests to `ReplicationTests.cpp`; register in both vcxprojs.

**Test Requirements.** `QueuesStep8`: ordering, overflow, retry/drop, bounded, determinism.

**Documentation Updates.** Local status/log. No README/graphics/ADR change.

**Recommended Git Commit Message.**
```
docs(replication): freeze Sprint-009 Step-08 spec (exception-free replication queues)
```

---

# Step-09 — Packet assembly (additive wire ids)

**Objective.** Define the engine-free packet builder that serializes ordered replication records into a fixed little-endian wire payload using the Sprint-006 `ByteWriter`, under new **additive** DATA-range message ids — ADR-010 respected, no renumbering.

**Scope — In.** `include/stalkermp/replication/ReplicationPacketBuilder.h` (+ `.cpp`) and `include/stalkermp/net/ReplicationMessageIds.h`: `kMsgReplicationUpdate = 0x0200` (host→client), `kMsgReplicationAck = 0x0201` (client→host). Builder: header → entity updates → player updates → metadata → checksum, fixed field-by-field little-endian framing; `Build(const IReplicationView&, net::ByteWriter&) -> ReplicationOutcome`; a reader/parse helper for the ack. Deterministic byte layout.
**Scope — Out.** Sending (transport is Sprint-006); worker/manager; message-handler registration (done at Step-10/12).

**Functional Requirements.** FR-1 fixed little-endian framing (ADR-010); DATA-range ids; additive, never renumbered. FR-2 deterministic byte layout for identical input (byte-for-byte). FR-3 no OS/socket header; writes into `net::ByteWriter` only. FR-4 bounded by `maxEntitiesPerUpdate`/`maxPlayersPerUpdate` and the byte budget; Overflow value outcome. FR-5 checksum computed deterministically; no engine access.

**Non-Functional Requirements.** ADR-007/ADR-009/ADR-010; deterministic; engine/OS-free (byte buffer only); additive.

**Public Interfaces.** `kMsgReplicationUpdate`/`kMsgReplicationAck`; `ReplicationPacketBuilder::Build(...)`; `ParseReplicationAck(net::ByteReader&) -> Expected<ReplicationAck>`.

**Integration Points.** Reuses `net::ByteWriter`/`ByteReader` and the additive message-id discipline (Sprint-006, ADR-010). No transport send here; no `MessageRegistry` registration yet.

**Validation Requirements.** Unit tests: deterministic byte layout (identical input → identical bytes); id ranges; round-trip of the ack; overflow/budget outcome; endian correctness.

**Evidence Gates.** None directly (supports E-G2-R determinism at the wire level).

**Acceptance Criteria.** Builder + ids present, engine/OS-free, deterministic bytes; additive ids; tests pass GCC + MSVC; suite green; no prior API change; ADR-010 preserved (no renumbering).

**Files Created/Modified.** Create `ReplicationPacketBuilder.h`/`.cpp`, `net/ReplicationMessageIds.h`; tests to `ReplicationTests.cpp`; register in both vcxprojs.

**Test Requirements.** `PacketStep9`: deterministic layout, id ranges, ack round-trip, overflow, endianness.

**Documentation Updates.** Local status/log. No README/graphics/ADR change (ADR-010 unchanged — ids are additive).

**Recommended Git Commit Message.**
```
docs(replication): freeze Sprint-009 Step-09 spec (packet assembly + additive wire ids)
```

---

# Step-10 — Replication Worker (synchronous consumer pipeline)

**Objective.** Define the worker that consumes an immutable snapshot and drives the full pipeline (interest → build update → delta → classify → enqueue → assemble → hand to transport) for every active client, **without creating a thread** and without mutating simulation.

**Scope — In.** `include/stalkermp/replication/ReplicationWorker.h` (+ `.cpp`): `ProcessSnapshot(const snapshot::ISnapshotView&)` running the per-client pipeline; `StartWorker()`/`StopWorker()`/`FlushQueues()` defined as synchronous lifecycle hooks (no thread spawned — designed for a future worker thread behind the snapshot seam, ADR-011); consumes snapshots via the Sprint-008 `SnapshotQueue` `Acquire`/`Release`; hands assembled payloads to the Sprint-006 session/host send seam. Read-only w.r.t. snapshot and simulation.
**Scope — Out.** Service/tick integration (Step-11); Bootstrap wiring (Step-12); real OS threading (future sprint).

**Functional Requirements.** FR-1 for each active client: interest-select → build `ReplicationUpdate` (values only) → delta vs baseline → classify → enqueue → assemble packet → send via the Sprint-006 seam. FR-2 consumes only immutable snapshots (`Acquire`/`Release`); never mutates a snapshot or simulation (E-G1-R/E-G5-R). FR-3 no thread created; `StartWorker`/`StopWorker`/`FlushQueues` are synchronous and deterministic. FR-4 respects per-tick byte budget and per-update caps (Overflow → drop lowest priority, counter). FR-5 deterministic given identical snapshot + client baselines. FR-6 handles all pipeline value outcomes without throwing.

**Non-Functional Requirements.** ADR-007/008/011; deterministic; engine/OS-free (transport via existing seam); additive.

**Public Interfaces.** `class ReplicationWorker { ReplicationWorker(config, clientRegistry, interestPolicy, deltaEncoder-or-inline, queues, packetBuilder, sendSeam); ReplicationOutcome ProcessSnapshot(const snapshot::ISnapshotView&); void StartWorker(); void StopWorker(); void FlushQueues(); ReplicationStatistics Statistics() const; };` (exact collaborator injection finalized in this spec; all by reference, non-owning).

**Integration Points.** Consumes `snapshot::ISnapshotView` + `snapshot::SnapshotQueue` (Sprint-008), `ReplicationClientRegistry` (04), `IInterestPolicy` (05), `DeltaEncoder` (06), `ReplicationClassifier` (07), `ReplicationQueues` (08), `ReplicationPacketBuilder` (09), and the Sprint-006 session/host send seam. No engine TU; no thread.

**Validation Requirements.** Unit tests via the composed engine-free stack + a loopback/mock send seam: pipeline runs; snapshot unmutated (const); only relevant entities sent; delta minimization; classification honored; deterministic output across two runs; budget/overflow handled.

**Evidence Gates.** **E-G1-R** (immutable consumption) and **E-G5-R** (no simulation mutation) confirmed.

**Acceptance Criteria.** Worker present; consumes immutable snapshots; no thread; no mutation; deterministic; tests pass GCC + MSVC; suite green; no prior API change.

**Files Created/Modified.** Create `ReplicationWorker.h`/`.cpp`; test support (mock send seam); tests to `ReplicationTests.cpp`; register in both vcxprojs.

**Test Requirements.** `WorkerStep10`: pipeline end-to-end, immutability, interest, delta, classification, determinism, budget.

**Documentation Updates.** Local status/log. No README/graphics/ADR change.

**Recommended Git Commit Message.**
```
docs(replication): freeze Sprint-009 Step-10 spec (synchronous ReplicationWorker consumer)
```

---

# Step-11 — `ReplicationManager` (`IService` + `ITickable`) at `kReplicationPipeline = 450`

**Objective.** Define the umbrella service that owns the worker/queues/registry/classifier/config and runs the replication pass once per tick at a newly reserved slot, strictly after Snapshot (400).

**Scope — In.** Allocate `core::tick_order::kReplicationPipeline = 450` in the central table. `include/stalkermp/replication/ReplicationManager.h` (+ `.cpp`): `final : core::IService, core::ITickable`; owns config/registry/queues/classifier/worker (and references the `SnapshotQueue` producer surface + the Sprint-006 send seam); `Initialize()` reserves queues + registers the inbound ack handler; `Tick()` acquires the latest published snapshot and runs `worker.ProcessSnapshot` exactly once, then flushes; `Dependencies() = {World, EntityRegistry, BubbleManager, TransitionManager, PlayerManager, Networking, SnapshotManager}` (ordering-only); read-only `Statistics()`/`ValidateConsistency()`/`Queues()`; `static constexpr TickOrder() = kReplicationPipeline`.
**Scope — Out.** Bootstrap wiring (Step-12); diagnostics (Step-13); threads.

**Functional Requirements.** FR-1 ticks once per frame at 450, strictly after Snapshot (400) and before Persistence (500)/Networking (900). FR-2 acquires the latest immutable snapshot from the Sprint-008 queue (read-only) and releases it after processing. FR-3 exactly one replication pass per tick; all value outcomes handled (a missing/absent snapshot skips the tick). FR-4 registers the additive inbound ack handler on `Initialize` (updates client baselines); never mutates simulation. FR-5 deterministic; single-threaded; no thread created. FR-6 placement `static_assert(kReplication < kReplicationPipeline < kPersistence)`.

**Non-Functional Requirements.** ADR-007/008/010/011; deterministic; engine/OS-free; additive. `ReplicationManager.h` includes `core/interfaces/IService.h` + `ITickable.h` (minimal), not the RTTI `ServiceRegistry.h`.

**Public Interfaces.** The `IService`/`ITickable` overrides (`Name()="Replication"`, `Dependencies()`, `Initialize()`, `Shutdown()`, `Tick()`), read-only accessors, and `TickOrder()`.

**Integration Points.** Consumes the `SnapshotManager`'s queue (Sprint-008), the Sprint-006 `Session`/host + `MessageRegistry` (for the ack id), and all Steps 02–10. Allocates the new tick key. No engine TU.

**Validation Requirements.** Unit tests: successful tick pass; exactly-one-pass-per-tick; dependency list + name; tick-before-init/absent-snapshot safe skip; deterministic repeated ticks; placement static_assert.

**Evidence Gates.** **E-G5-R** (worker independence via the service boundary).

**Acceptance Criteria.** Manager present; ticks at 450; consumes immutable snapshots; no thread/mutation; deterministic; tests pass GCC + MSVC; suite green; no prior API change; new tick key asserted.

**Files Created/Modified.** Modify `include/stalkermp/core/FrameDispatcher.h` (add `kReplicationPipeline = 450` to the `tick_order` table — the sanctioned central allocation point). Create `ReplicationManager.h`/`.cpp`; tests to `ReplicationTests.cpp`; register in both vcxprojs.

**Test Requirements.** `ReplicationManagerStep11`: tick lifecycle, one-pass-per-tick, dependency ordering, safe-skip, determinism, placement.

**Documentation Updates.** Local status/log. No README/graphics/ADR change. (The `tick_order` addition is a code change in the sanctioned table, not an architecture-doc edit.)

**Recommended Git Commit Message.**
```
docs(replication): freeze Sprint-009 Step-11 spec (ReplicationManager at kReplicationPipeline=450)
```

---

# Step-12 — Bootstrap wiring at `kReplicationPipeline = 450`

**Objective.** Wire the `ReplicationManager` into the composition root: register it after Networking and the SnapshotManager, subscribe it to the `FrameDispatcher` at 450, with reverse-order teardown and proper unsubscription.

**Scope — In.** `src/core/Bootstrap.cpp`: construct `ReplicationConfiguration::FromConfig`; own the client registry/policy/worker collaborators as needed; register `ReplicationManager` after `PlayerManagerService`, `NetworkingService`, and `SnapshotManager`; subscribe at `kReplicationPipeline = 450`; reverse-order unsubscribe before `ShutdownAll`; add the default `[replication]` block to the server-config template. `tests/BootstrapTests.cpp`: subscriber count 7→8; placement `static_assert(kAlifeTransition < kReplication < kReplicationPipeline < kPersistence < kNetworking)`; a wiring test analogous to `SnapshotManagerWiredAtReplicationSlot`.
**Scope — Out.** Diagnostics; behavior changes; threads.

**Functional Requirements.** FR-1 registration order places Replication after all its dependencies (init-order = registration-order). FR-2 subscribed once at 450; frame order …→ Snapshot (400) → **Replication (450)** → … → Networking (900). FR-2 reverse-order teardown unsubscribes before `ShutdownAll`. FR-3 existing tick orders and services unchanged. FR-4 default `[replication]` config block added (all keys consumed by delivered code). FR-5 no new engine seam; no thread.

**Non-Functional Requirements.** ADR-007/011; deterministic; engine-free composition (Bootstrap includes only engine-free headers); additive.

**Public Interfaces.** None new (composition only). Bootstrap gains an owned `ReplicationManager*` cache + collaborators.

**Integration Points.** `ServiceRegistry`, `FrameDispatcher` (450), `SessionService`/`MessageRegistryService` (send + ack id), `SnapshotManager` (snapshot source). Reverse-order teardown consistent with the Sprint-008 pattern.

**Validation Requirements.** `BootstrapTests`: subscriber count = 8; registration/initialization; tick subscription at 450; reverse-order shutdown; unsubscription (re-init shows no stale subscriber); placement asserts.

**Evidence Gates.** None directly (integration confirmed at Step-15).

**Acceptance Criteria.** Wiring present; subscriber count 8; placement asserted; reverse-order teardown; tests pass GCC + MSVC (Bootstrap link/execution on Windows per prior sprints); suite green; no prior behavior change.

**Files Created/Modified.** Modify `src/core/Bootstrap.cpp`, `tests/BootstrapTests.cpp`, and both vcxprojs (add the manager + collaborator TUs).

**Test Requirements.** `Bootstrap.ReplicationManagerWiredAtPipelineSlot`; updated subscriber-count assertions.

**Documentation Updates.** Local status/log. README textual progress updated when implemented/verified (not at spec time). No graphics/ADR change.

**Recommended Git Commit Message.**
```
docs(replication): freeze Sprint-009 Step-12 spec (Bootstrap wiring at kReplicationPipeline=450)
```

---

# Step-13 — `ReplicationDiagnostics`

**Objective.** Define the read-only, replay-stable diagnostics inspector over the `ReplicationManager`.

**Scope — In.** `include/stalkermp/replication/ReplicationDiagnostics.h` (+ `.cpp`): read-only `Statistics()`, `DescribeState()`, `DumpUpdate(ReplicationId)`, `PacketView()`, `BandwidthReport()`, `PriorityStatistics()`, `DroppedPacketCounter()`, `ClientStatistics()`. iostream-free (`common::Format`). Bandwidth/latency/worker timing fields explicitly diagnostic.
**Scope — Out.** Any mutation; new telemetry/logging beyond this interface.

**Functional Requirements.** FR-1 all accessors read-only; derive from the manager's const surface + queues + client registry. FR-2 `DumpUpdate` returns the current/last update for an id or `available=false`. FR-3 bandwidth/priority/dropped/client reports derive from `ReplicationStatistics` + queue state. FR-4 deterministic except explicitly-diagnostic timing/wall-clock. FR-5 iostream-free.

**Non-Functional Requirements.** ADR-007; read-only; engine/OS-free; additive.

**Public Interfaces.** The seven+ accessors above and their small read-only value structs (`BandwidthReport`, `PriorityStatistics`, `ClientReport`, `PacketView`, `UpdateDump`).

**Integration Points.** Consumes `ReplicationManager` const surface only (Statistics/Queues/ClientRegistry view). No mutation.

**Validation Requirements.** Unit tests: statistics correctness; bandwidth/priority/dropped/client reporting; update dump correctness (current vs unknown id); read-only guarantees (manager/queue state unchanged across diagnostic calls); pre-first-pass edge state.

**Evidence Gates.** None (observability).

**Acceptance Criteria.** Diagnostics present, read-only, iostream-free; tests pass GCC + MSVC; suite green; no prior API change.

**Files Created/Modified.** Create `ReplicationDiagnostics.h`/`.cpp`; tests to `ReplicationTests.cpp`; register in both vcxprojs.

**Test Requirements.** `ReplicationDiagnosticsStep13`: statistics, bandwidth, priority, dropped, client, dump, read-only.

**Documentation Updates.** Local status/log. No README/graphics/ADR change.

**Recommended Git Commit Message.**
```
docs(replication): freeze Sprint-009 Step-13 spec (read-only ReplicationDiagnostics)
```

---

# Step-14 — Validation hardening (negative surface)

**Objective.** Prove the integrity/immutability/independence negative surface across config/registry/interest/delta/classifier/queues/packet/worker/manager. Test-and-harden; tighten a `.cpp` only if a genuine gap is found (no interface change).

**Scope — In.** Extend `tests/ReplicationTests.cpp`: null/invalid inputs, capacity/overflow, ordering invariants, immutability after finalize, delta minimization edge cases, classification totality, queue retry/drop, packet budget/endianness, worker safe-skip + no-mutation, config invariants, and a deterministic large-client/high-entity churn stress with replay identity. Minimal `replication::*` `.cpp` hardening only on a proven gap.
**Scope — Out.** New behavior/interfaces; new subsystems.

**Functional Requirements.** FR-1 every rejection is a value outcome leaving state unchanged. FR-2 no live object ever captured; snapshots never mutated. FR-3 determinism preserved under stress (identical inputs → identical replication + bytes). FR-4 bounded memory under churn (queue/pool never exceed configured caps).

**Non-Functional Requirements.** ADR-007…011 negatives; deterministic; engine/OS-free; additive; no interface change for valid inputs.

**Public Interfaces.** None new.

**Integration Points.** Exercises all Steps 01–11 through their public/value surfaces + a mock send seam.

**Validation Requirements.** The full negative surface enumerated above, each asserting an unchanged-state value outcome; a stress/replay determinism test.

**Evidence Gates.** E-G1-R/E-G2-R/E-G3-R/E-G4-R/E-G5-R negatives exercised.

**Acceptance Criteria.** Negative surface green; no interface/behavior change for valid inputs; tests pass GCC + MSVC; suite green; no regressions.

**Files Created/Modified.** Modify `tests/ReplicationTests.cpp`; minimal `replication::*` `.cpp` only if a gap is found.

**Test Requirements.** `ReplicationHardeningStep14`: invalid input, lifecycle, ordering, capacity, queue invariants, packet, worker independence, configuration, regression/stress.

**Documentation Updates.** Local status/log. No README/graphics/ADR change.

**Recommended Git Commit Message.**
```
docs(replication): freeze Sprint-009 Step-14 spec (validation hardening negative surface)
```

---

# Step-15 — Integration + documentation

**Objective.** Prove the composed pipeline end-to-end behind the seams and author the subsystem doc. No behavior change.

**Scope — In.** Integration tests in `tests/ReplicationTests.cpp` (and/or `BootstrapTests.cpp`): snapshot → replication → loopback/mock transport; multiple clients synchronize; interest filtering (only relevant entities per client); delta minimizes bandwidth (unchanged entities skipped); priority ordering correct; reliability channels honored; full-pass replay identity across two runs. Author `Multiplayer/docs/Replication.md` (prose: lifecycle, delta, interest, reliability model, packet priorities, worker/consumer interaction, boundaries, ADR-007…011, worker-thread readiness, extension points).
**Scope — Out.** New functionality; Sprint-010 work; graphics.

**Functional Requirements.** FR-1 composed stack: World/Registry/Player captured (via snapshot) → replicated correctly to N clients through the loopback seam. FR-2 interest filtering verified per client. FR-3 delta skips unchanged; priority ordering verified; reliability verified. FR-4 replay identity of the full pass. FR-5 no behavior change vs unit steps.

**Non-Functional Requirements.** Deterministic; engine/OS-free (mock transport); additive.

**Public Interfaces.** None new.

**Integration Points.** The full composed replication stack + the Sprint-006 loopback transport + the Sprint-008 snapshot pipeline.

**Validation Requirements.** The integration tests above; a consistency sweep; doc matches delivered code.

**Evidence Gates.** **E-G2-R/E-G3-R/E-G4-R composed** confirmations.

**Acceptance Criteria.** Integration green; `Replication.md` written and accurate; suite green; no behavior change.

**Files Created/Modified.** Create `Multiplayer/docs/Replication.md`; modify `tests/ReplicationTests.cpp`/`tests/BootstrapTests.cpp`.

**Test Requirements.** `ReplicationIntegrationStep15`: multi-client sync, interest filtering, delta minimization, priority ordering, reliability, replay identity.

**Documentation Updates.** Create `Replication.md`; update local `CURRENT_STATUS.md`/`SESSION_LOG.md`; README textual progress updated when implemented/verified. No graphics/ADR change.

**Recommended Git Commit Message.**
```
docs(replication): freeze Sprint-009 Step-15 spec (integration tests + Replication.md)
```

---

# Step-16 — Sprint closure (no code)

**Objective.** Confirm Sprint-009 is implemented/verified/closeable; synchronize status docs; record the final baseline; declare Sprint-010 readiness.

**Scope — In.** Cross-check the Definition of Done (§12) and completion criteria against delivered, Antigravity-verified artifacts; record final test counts, build status, boundary/determinism/gate outcomes; update `Sprint-009-Delta-Replication.md` status to Closed/Frozen (status field + a closure section), `Documentation/AI/CURRENT_STATUS.md`, `Documentation/AI/SESSION_LOG.md`; public README textual progress → Sprint-009 Verified (Closed); a consistency scan.
**Scope — Out.** Any code (except a genuine closure defect); any Sprint-010 work; graphics.

**Functional Requirements.** FR-1 all completion criteria satisfied and recorded. FR-2 status docs consistent and set to Closed/Verified/Frozen. FR-3 final baseline recorded (`377 + Sprint-009 additions`). FR-4 Sprint-010 readiness stated.

**Non-Functional Requirements.** Documentation only; record only verified facts; no graphics; no architecture/ADR edits.

**Public Interfaces.** None.

**Integration Points.** Status docs only.

**Validation Requirements.** Consistency scan: every claim traces to a verified artifact; test count matches the verified suite; boundaries/gates recorded.

**Evidence Gates.** All Sprint-009 gates (E-G1-R…E-G5-R) recorded as passed/confirmed.

**Acceptance Criteria.** DoD satisfied and recorded; status consistent; Sprint-009 declared Implemented / Verified / Closed / Frozen; Sprint-010 readiness stated; suite green.

**Files Created/Modified.** Modify the three status docs (status/closure only) + README textual progress.

**Test Requirements.** None new (baseline re-confirmed).

**Documentation Updates.** As above. No graphics/ADR/architecture change.

**Recommended Git Commit Message.**
```
docs(replication): Sprint-009 Step-16 sprint closure (Closed/Frozen)
```

---

## 2. Completion criteria (Sprint-009)

Sprint-009 is **complete** when:
1. All 16 steps implemented, each independently Antigravity-verified, tree buildable after each.
2. `ReplicationManager` runs as a single `ITickable` at the new `tick_order::kReplicationPipeline = 450`, strictly after Snapshot (400) and before Persistence (500)/Networking (900), integrated via Bootstrap with reverse-order teardown.
3. Replication consumes immutable snapshots exclusively (E-G1-R); clients receive only relevant updates (E-G3-R); delta minimizes bandwidth (E-G2-R); priority/reliability correct (E-G4-R); worker never mutates simulation and networking stays a consumer (E-G5-R).
4. One Engine Boundary **and** One Platform Boundary hold — **no new engine TU and no OS code added**; the wire protocol is extended only additively (`kMsgReplicationUpdate`/`kMsgReplicationAck`), never renumbered (ADR-010).
5. ADR-007…ADR-011 all conformed to; no thread created (ADR-011); the one new `tick_order` key allocated in the sanctioned table.
6. Full suite green on GCC + MSVC, MSVC Release clean under `EngineAbi.props`, zero new warnings, no regressions; the Sprint-008 377/377 baseline preserved and extended.
7. No non-goal implemented (no client prediction/interpolation/lag-compensation/save-system); replication owns no entities and executes no gameplay.
8. Subsystem doc `Multiplayer/docs/Replication.md` written; all status docs synchronized to Closed/Verified.

## 3. Sprint closure checklist (Sprint-009)

- [ ] Steps 1–16 implemented and each independently Antigravity-verified.
- [ ] Project buildable after every step (GCC + MSVC), zero new warnings.
- [ ] `ReplicationManager` ticks once at `kReplicationPipeline = 450`; Bootstrap wiring + reverse-order teardown verified; subscriber count updated (8); placement asserted (`kReplication < kReplicationPipeline < kPersistence`).
- [ ] One Engine Boundary intact (no new engine TU); One Platform Boundary intact (no OS code).
- [ ] ADR-010 additive only (`kMsgReplicationUpdate`/`kMsgReplicationAck` appended, never renumbered); ADR-011 (no thread created).
- [ ] E-G1-R…E-G5-R passed/confirmed; replication consumes immutable snapshots only; no live object captured; no simulation mutation.
- [ ] Final test counts + build status recorded; `Replication.md` written; the three status docs updated to Closed/Verified.
- [ ] Sprint-009 Definition of Done (§12) fully satisfied; architecture unchanged (frozen).

---

## 4. One-pass freeze note

This document specifies **all 16 Sprint-009 steps in one pass** with explicit inter-step dependencies (§1.3), so the full plan can be reviewed, frozen, and approved before any implementation begins. Each step is independently implementable and verifiable against only the steps before it; none requires revisiting an earlier specification. Implementation proceeds under the mandatory workflow (implement → Antigravity verification → git commit → GitHub push → next step), one step at a time, exactly as Sprint-008 was executed.

**Recommended git commit message (freeze of this complete plan):**
```
docs(replication): freeze complete Sprint-009 implementation plan (Steps 01–16 specifications)

Full additive 16-step plan for the Replication Pipeline: value types, config,
immutable ReplicationUpdate, client registry, interest, delta, classification,
queues, packet assembly (additive wire ids), synchronous worker, manager at
new tick_order::kReplicationPipeline=450, Bootstrap wiring, diagnostics,
validation hardening, integration+docs, closure. No implementation, no thread,
no engine TU, no OS code; ADR-007..ADR-011 preserved; all standing invariants
intact. Compatible with the Sprint-008 baseline (377/377).
```

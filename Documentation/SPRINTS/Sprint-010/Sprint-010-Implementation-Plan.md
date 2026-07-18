# Sprint-010 — Client Prediction & Interpolation · Architecture & Implementation Plan

| Field | Value |
|---|---|
| Sprint | 010 — Client Prediction & Interpolation |
| Status | **Plan — FROZEN candidate** (review → freeze → approve before Step-01) |
| Scope authority | `Documentation/SPRINTS/Sprint-010/Sprint-010-Prediction-Interpolation.md` (FROZEN) |
| Baseline | Sprint-009 CLOSED & FROZEN — **442 / 442** tests passing (GCC + MSVC) |
| Governing ADRs | ADR-007, ADR-008, ADR-009, ADR-010, ADR-011 (all preserved; none modified) |
| Steps | 17 (Step-01 … Step-17), strict additive dependency chain |

---

## 0. Governance & preserved invariants (apply to every step)

This plan is **purely additive** and modifies no prior sprint's frozen documents, code, or ADRs. Every step preserves, without exception:

- **Preserve Before Replace** — prediction/interpolation reuse the Sprint-009 replication value types (`EntityReplicationState`, `PlayerReplicationState`) as their authoritative input and the Sprint-007 player identity; they reimplement none of the simulation, networking, or replication.
- **Host Authority** — the authoritative host simulation is the single source of truth. Prediction is **client-only** and advisory; on the authoritative host it is the identity (disabled). Reconciliation always snaps the client toward the authoritative state — **host authority always wins**.
- **One World / One ALife Simulation** — no second world or simulation. Prediction produces a **client-side presentation overlay** (visual transforms), never authoritative state.
- **Deterministic Simulation & Replay Determinism** — the prediction integrator and reconciliation are pure, fixed-timestep, tick-derived; identical inputs + identical authoritative frames yield identical corrected state; wall-clock appears only in explicitly-diagnostic fields.
- **One Engine Boundary** — the prediction/interpolation core is engine-free. The **only** engine-touching code (local-input read and presentation-transform apply) is confined to `EngineAdapters.cpp` behind engine-free seams; the test build links null counterparts.
- **One Platform Boundary** — Sprint-010 introduces **no** OS/socket code; it consumes already-decoded replication values.
- **ADR-007** — no exceptions, no RTTI, no iostream; all fallible operations are `core::Expected<T>` / value outcomes; bounded memory (pre-reserved ring buffers).
- **ADR-008** — prediction/interpolation apply only **client-side presentation transforms** (visual position/rotation of the local and remote representations); they never mutate authoritative ALife/simulation state. On the host, the presentation apply is the identity.
- **ADR-009** — the platform boundary is untouched.
- **ADR-010** — the wire protocol is **not** required to change for the engine-free core (it consumes decoded values). If the transport step needs a client→host sequenced-input message, one additive DATA-range id (`kMsgClientInput = 0x0300`) is reserved and appended, never renumbered; the authoritative input-acknowledgement sequence is delivered to the client as a decoded value (no frozen Sprint-009 format is modified).
- **ADR-011** — Sprint-010 creates **no** thread. The prediction/interpolation managers advance synchronously once per client presentation frame. **No new authoritative `tick_order` key is introduced** — client presentation is not part of the authoritative frame dispatch (see §8), preserving the networking-last invariant and the frozen `tick_order` table.

**Compatibility.** Every step is additive and leaves the Sprint-009 **442/442** baseline intact; new tests only add to the count. No prior public API changes.

---

## 1. Sprint objective

Deliver the **client-side presentation layer** that makes multiplayer feel responsive without weakening host authority: predict the **local** player's own movement from buffered input so it responds instantly; interpolate **remote** entities smoothly between authoritative updates; and, when the host's authoritative state disagrees with a local prediction, perform **deterministic reconciliation** (snap to authoritative and replay pending inputs). Prediction exists entirely on the client and never alters the authoritative simulation.

## 2. Scope

**In scope.** Engine-free client presentation subsystem in a new `prediction::` namespace: value types & configuration; a sequenced **input buffer**; a per-tick predicted **state buffer**; a pure deterministic **prediction integrator**; the **PredictionManager**; a **snapshot buffer** of authoritative frames; a pure deterministic **interpolation** step; the **InterpolationManager**; **reconciliation** + error correction (configurable thresholds); prediction **validation** (negative surface as value outcomes); **diagnostics** (non-invasive collector); the engine-free client seams (local-input source, authoritative-state ingestion, presentation sink) + a synchronous **client-presentation driver**; the single engine adapter (input read + presentation apply, confined to `EngineAdapters.cpp`) + null test counterparts; composition-root wiring; the subsystem doc `Multiplayer/docs/Prediction.md`.

## 3. Out-of-scope items

Lag compensation, server rewind, hit validation, anti-cheat, the save/replay systems (§4 of the scope authority); any modification of the authoritative simulation, ALife, networking wire format (beyond the single reserved additive input id), or any frozen sprint/ADR; any thread creation or new authoritative `tick_order` key; extrapolation beyond the configured interpolation delay; and predicting authoritative gameplay outcomes (damage, spawns, ALife) — prediction covers only local movement/rotation/camera/stance/basic interactions.

## 4. Design goals

Responsive local control; smooth remote motion; **host authority always wins**; deterministic, replay-safe correction; bounded memory with value-outcome error handling; engine-free, unit-testable core with the engine interaction confined to one TU; zero impact on the authoritative pipeline and the Sprint-009 baseline.

## 5. Architectural overview

Sprint-010 is a **consumer** of replication output and the client's own input, producing a presentation overlay. It is **not** part of the authoritative frame dispatch. Once per client presentation frame the composition root advances a `ClientPresentationDriver` (after `FrameDispatcher::Dispatch`, within the single engine frame):

```
local input (ILocalInputSource) ─┐
                                  ├─> PredictionManager ── predicted local state ──┐
authoritative frames             │        ^ reconcile (host wins)                 ├─> IPresentationSink
(IAuthoritativeStateSource,      │        └── replay pending inputs               │   (client-visual transforms only)
 decoded EntityReplicationState/ │                                                │
 PlayerReplicationState)         └─> InterpolationManager ── smoothed remote ─────┘
                                          (snapshot buffer + interpolation delay)
```

The authoritative simulation, the Sprint-008 snapshot, the Sprint-009 replication, and the networking tick are all unchanged. On the listen-server host, prediction/interpolation are the identity (the host renders authoritative state directly).

## 6. Component responsibilities

| Component | Responsibility |
|---|---|
| `prediction::PredictionTypes` | Engine-free vocabulary: `InputCommand` (sequence, movement, rotation, actions, tick), `PredictedState`, `InterpolatedState`, `SnapshotFrame`, `PredictionStatistics`, `PredictionConsistency`, enums, `PredictionOutcome`. |
| `prediction::PredictionConfiguration` (+ `FromConfig`) | Input/state buffer depths, interpolation delay (ticks), correction thresholds (position/rotation/velocity), max prediction ticks, version. |
| `prediction::InputBuffer` | Fixed-capacity, sequence-ordered ring of `InputCommand`; ascending sequence; bounded; `Overflow` value outcome. |
| `prediction::StateBuffer` | Fixed-capacity ring of `PredictedState` keyed by tick; supports reconciliation lookup; bounded. |
| `prediction::PredictionStep` | Pure, deterministic fixed-timestep integrator: `(PredictedState, InputCommand) -> PredictedState`. Movement/rotation/camera/stance only. |
| `prediction::PredictionManager` | Owns the buffers + integrator; `RecordInput`, `PredictLocal(tick)`, `Reconcile(authoritative, ackedInputSequence)` (replay pending inputs); read-only surface. |
| `prediction::SnapshotBuffer` | Buffers authoritative `SnapshotFrame`s (previous/current/next) + interpolation factor/delay. |
| `prediction::InterpolationStep` | Pure deterministic lerp/slerp between two authoritative states by factor. |
| `prediction::InterpolationManager` | Owns the snapshot buffer; `Interpolate(renderTick)` produces smoothed remote states; no extrapolation beyond the delay. |
| `prediction::PredictionDiagnostics` | Non-invasive monotonic collector (errors, corrections avg/max, input/interp delay, correction frequency) → immutable `Snapshot`; `Reset`. |
| `prediction::ILocalInputSource` / `IAuthoritativeStateSource` / `IPresentationSink` | Engine-free client seams (bound to the engine in one TU; faked in tests). |
| `prediction::ClientPresentationDriver` | Synchronous per-frame `Advance(clientTick, dt)` orchestrating input → predict → reconcile → interpolate → present. No thread; not a `FrameDispatcher` subscriber. |
| `adapters::EngineLocalInputSource` / `EnginePresentationSink` (`EngineAdapters.cpp`) | The one engine-touching binding: read local input; apply client-visual presentation transforms. Null counterparts in tests. |

## 7. Public interfaces (representative; finalized per step)

```cpp
namespace stalkermp::prediction {
  struct InputCommand { std::uint64_t sequence; std::uint64_t tick; world::Vec3 move; float yaw; std::uint32_t actionBits; };
  struct PredictedState { world::EntityId id; std::uint64_t tick; world::Vec3 position; world::Vec3 velocity; float yaw; std::uint32_t stateFlags; };
  struct SnapshotFrame  { std::uint64_t tick; std::vector<replication::EntityReplicationState> entities; std::vector<replication::PlayerReplicationState> players; };
  struct InterpolatedState { world::EntityId id; world::Vec3 position; float yaw; };

  enum class PredictionOutcome : std::uint8_t { Ok=0, ClientOnly, BufferOverflow, SequenceMismatch, NoAuthoritativeFrame, CorrectionRejected };

  class ILocalInputSource      { public: virtual bool PollInput(InputCommand& out) const = 0; virtual ~ILocalInputSource()=default; };
  class IAuthoritativeStateSource { public: virtual bool NextFrame(SnapshotFrame& out) const = 0; virtual ~IAuthoritativeStateSource()=default; };
  class IPresentationSink      { public: virtual void ApplyLocal(const PredictedState&) = 0; virtual void ApplyRemote(const InterpolatedState&) = 0; virtual ~IPresentationSink()=default; };
}
```

## 8. Tick-order integration

**No new authoritative `tick_order` key is introduced.** Prediction and interpolation are **client presentation**, not authoritative simulation, and must run **after** the networking tick has delivered authoritative updates — which would conflict with the frozen *networking-last* invariant (`kNetworking = 900` is the highest key). Therefore the `ClientPresentationDriver` is **not** a `FrameDispatcher` subscriber; the composition root advances it via a dedicated client-presentation phase invoked **after** `FrameDispatcher::Dispatch(...)` within the single existing engine frame (the same engine frame bridge; no new engine frame registration). The authoritative `FrameDispatcher` table, its ordering, and the networking-last invariant are unchanged. On the listen-server host, the driver runs in identity mode.

## 9. Packet / message integration

The engine-free prediction/interpolation core is **wire-agnostic**: it consumes already-decoded authoritative `SnapshotFrame`s and an `ackedInputSequence` value through `IAuthoritativeStateSource`. No frozen Sprint-009 wire format is modified. **If** the transport step requires the client to send sequenced input to the host, one additive DATA-range id is reserved: `net::kMsgClientInput = 0x0300` (client→host; additive, never renumbered, block 0x0300–0x030F); the host communicates the last-processed input sequence back to the client as a decoded value delivered through the authoritative-state seam. Defining/using this id is isolated to the seam/transport step and does not alter the Sprint-009 replication frame.

## 10. Determinism considerations

The `PredictionStep` integrator is pure and fixed-timestep (tick-derived; no wall-clock). Reconciliation is deterministic: on an authoritative frame it discards inputs at or before `ackedInputSequence`, snaps the local state to authoritative, and **replays the remaining buffered inputs in ascending sequence** through the same integrator — identical inputs + identical authoritative frame yield identical corrected state (replay determinism). Interpolation uses a tick-based delay and a factor derived from ticks (no wall-clock in the interpolated value). Wall-clock/timing appears only in `PredictionDiagnostics` fields explicitly excluded from replay identity.

## 11. Host / client authority boundaries

Prediction is advisory and **client-only**: it produces presentation transforms, never authoritative state (ADR-008). The authoritative host state always wins — reconciliation corrects the client toward it. On the authoritative host, `ClientPresentationDriver` runs in identity mode (no prediction/interpolation; the host renders authoritative state). The client still sends its input to the host through the existing player-input path; the host owns every authoritative transition (Host Authority). Prediction covers only local movement/rotation/camera/stance/basic interactions — never authoritative gameplay outcomes (damage, spawns, ALife).

## 12. Dependencies on previous sprints

Sprint-002 `world::Vec3`/`EntityId`; Sprint-007 `player::PlayerId`; Sprint-009 `replication::EntityReplicationState`/`PlayerReplicationState` (the decoded authoritative input shape) and the replication pipeline it is fed by; Sprint-001 `core::Expected`/`ConfigStore`/`common::Format`; the single engine frame bridge (Sprint-002) for the client-presentation phase. No frozen component is modified.

## 13. Risk analysis

- **Prediction divergence / rubber-banding** — mitigated by input history + deterministic replay + configurable correction thresholds (small errors are smoothed, large ones snapped).
- **Visible jitter** — mitigated by a configurable interpolation delay and no extrapolation beyond it.
- **Buffer overflow** — bounded pre-reserved ring buffers; `Overflow`/`SequenceMismatch` are value outcomes (never throws); oldest input pruned by `ackedInputSequence`.
- **Authority leakage (ADR-008 risk)** — the core is engine-free and value-only; the only engine writes are client-visual presentation transforms in one TU; on the host the apply is identity. Documented and asserted.
- **Determinism risk** — pure fixed-timestep integrator + ascending-sequence replay; no wall-clock in identity; guarded by determinism/replay tests.
- **Tick-order/invariant risk** — no new `tick_order` key; presentation runs outside `FrameDispatcher` after Dispatch, preserving networking-last (§8).
- **Wire risk (ADR-010)** — core is wire-agnostic; at most one additive, never-renumbered id is reserved and isolated to the seam/transport step.

## 14. Implementation roadmap

Strict additive dependency chain (each step compiles/tests against only earlier steps):

`01 types → 02 config → 03 input buffer → 04 state buffer → 05 prediction step → 06 PredictionManager → 07 snapshot buffer → 08 interpolation step → 09 InterpolationManager → 10 client seams (input/authoritative/presentation) → 11 reconciliation + error correction → 12 prediction validation → 13 diagnostics → 14 client-presentation driver → 15 engine adapter (input + presentation) → 16 composition-root wiring + integration → 17 documentation + sprint closure.`

### 14.1 Step → evidence-gate mapping

| Step | Delivers | Evidence gate |
|---|---|---|
| 01 | Value types & vocabulary | E-G1-P groundwork (value-only, client-only) |
| 02 | `PredictionConfiguration` | — |
| 03 | `InputBuffer` | E-G5-P (bounded) |
| 04 | `StateBuffer` | E-G5-P (bounded) |
| 05 | Deterministic prediction step | E-G2-P groundwork |
| 06 | `PredictionManager` | E-G2-P |
| 07 | `SnapshotBuffer` | — |
| 08 | Deterministic interpolation step | E-G4-P groundwork |
| 09 | `InterpolationManager` | E-G4-P |
| 10 | Client seams | E-G1-P |
| 11 | Reconciliation + error correction | E-G2-P / E-G3-P |
| 12 | Prediction validation | E-G5-P negatives |
| 13 | Diagnostics | — |
| 14 | Client-presentation driver | E-G1-P / E-G3-P composed |
| 15 | Engine adapter (input + presentation) | E-G1-P engine capture (Antigravity, Windows) |
| 16 | Composition-root wiring + integration | E-G2-P/E-G3-P/E-G4-P composed |
| 17 | Closure | all gates recorded |

### 14.2 Sprint-010 evidence gates

- **E-G1-P** — prediction never mutates authoritative state; the core is value-only and client-only; on the host it is identity.
- **E-G2-P** — deterministic prediction & reconciliation: identical inputs + authoritative frame → identical corrected state (replay determinism).
- **E-G3-P** — host authority always wins: reconciliation snaps the client toward authoritative within configured thresholds.
- **E-G4-P** — interpolation correctness: remote states interpolate between authoritative frames, bounded by the delay, no extrapolation.
- **E-G5-P** — bounded memory & robust validation: buffers bounded; overflow/sequence-mismatch/invalid-order are value outcomes leaving state unchanged.

---

## 15. Step-by-step implementation plan

**Shared shape.** Namespace `stalkermp::prediction`; headers `Multiplayer/include/stalkermp/prediction/`; sources `Multiplayer/src/prediction/`; tests `Multiplayer/tests/PredictionTests.cpp` (accumulating); subsystem doc `Multiplayer/docs/Prediction.md` (Step-16). Each step is engine-free/OS-free unless it is the engine-adapter step (Step-15), leaves the tree buildable, compiles clean on GCC + MSVC under ADR-007 flags, adds only its own tests, then stops for independent verification.

---

### Step-01 — Prediction value types & vocabulary
- **Objective.** The engine-free foundational vocabulary all later steps build on. Types only.
- **Scope — In.** `PredictionTypes.h`: `InputCommand`, `PredictedState`, `InterpolatedState`, `SnapshotFrame`, `PredictionStatistics`, `PredictionConsistency` (+ `IsHealthy`), enums `PredictionOutcome` and any state/kind enum (`: std::uint8_t`, total `Name()`), identity reuse (`world::EntityId`, `player::PlayerId`). **Out.** Any logic/config/buffer.
- **Functional Requirements.** FR-1 header-only, depends only on `<cstdint>`, `world/EntityView.h`, `player/PlayerTypes.h`, and `replication/ReplicationTypes.h` (for the authoritative value shapes in `SnapshotFrame`). FR-2 POD, trivially copyable, deterministic layout, `0 = none`; wall-clock (if any) diagnostic-only. FR-3 total `Name()` helpers incl. `"Unknown"`. FR-4 values only — no pointer/handle.
- **Non-Functional.** ADR-007; deterministic; engine/OS-free; additive.
- **Public Interfaces.** As in §7 (finalized here).
- **Integration Points.** Reuses Sprint-002/007/009 value types. No wiring.
- **Validation.** `static_assert` enum underlying type + trivially-copyable; unit checks for defaults, `Name()` totality, `IsHealthy`.
- **Evidence Gates.** E-G1-P groundwork.
- **Acceptance.** Header present, engine-free, standalone-compiling; tests pass GCC + MSVC; suite `442 + N`; no prior API change.
- **Files.** Create `PredictionTypes.h`, `tests/PredictionTests.cpp`; register in `xrMP.vcxproj` + test vcxproj.
- **Tests.** `PredictionTypesStep1`: enum layout/Name, POD defaults, consistency health.
- **Doc Updates.** Local status/log. **Commit:** `docs(prediction): freeze Sprint-010 Step-01 spec (prediction value types)`.

### Step-02 — `PredictionConfiguration` (+ `FromConfig`)
- **Objective.** Engine-free configuration value + deterministic parser.
- **Scope — In.** `PredictionConfiguration.h`/`.cpp`: `inputBufferDepth`, `stateBufferDepth`, `interpolationDelayTicks`, `maxPredictionTicks`, `positionCorrectionThreshold`, `rotationCorrectionThreshold`, `velocityCorrectionThreshold`, `version`; `FromConfig(ConfigStore)` reading `[prediction]`, defaults when absent, cross-field minimums (`inputBufferDepth>=1`, `stateBufferDepth>=1`, `maxPredictionTicks>=1`, `version>=1`). Thresholds are non-negative integers (fixed-point millimetres/milliradians — no wall-clock). **Out.** Any use of config.
- **FR.** Defaults; per-field parse; invalid rejected as `Expected` error; cross-field minimums enforced; no I/O beyond the store; no throw.
- **NFR.** ADR-007; deterministic; engine/OS-free; `common::Format` messages.
- **Public Interfaces.** The struct + `static Expected<PredictionConfiguration> FromConfig(const core::ConfigStore&)`.
- **Integration.** `core::ConfigStore`/`Expected` only. Default `[prediction]` block added to the config template at the wiring step.
- **Validation.** Defaults; overrides; each invalid field; boundary minimums.
- **Evidence Gates.** None. **Acceptance.** As Step-01 pattern. **Files.** `PredictionConfiguration.h`/`.cpp` + tests; register both vcxprojs. **Tests.** `PredictionConfigStep2`. **Commit:** `docs(prediction): freeze Sprint-010 Step-02 spec (PredictionConfiguration)`.

### Step-03 — `InputBuffer`
- **Objective.** A fixed-capacity, sequence-ordered ring of `InputCommand` for prediction + replay.
- **Scope — In.** `InputBuffer.h`/`.cpp`: `Push(InputCommand) -> PredictionOutcome` (ascending sequence required; `SequenceMismatch` on regress; `Overflow` when full unless pruning), `PruneUpTo(sequence)` (discard acknowledged inputs), `Pending(fromSequence) -> span/vector view` (ascending, for replay), `Clear`, `Empty`, `Size`, `Capacity`. Pre-reserved; no steady-state allocation. **Out.** Prediction logic.
- **FR.** Ascending, unique sequences; regress rejected; capacity bounded (config); prune advances the tail; pending returns ascending; no allocation during ops.
- **NFR.** ADR-007 (exception-free bounded memory); deterministic; engine/OS-free.
- **Evidence Gates.** E-G5-P. **Files.** `InputBuffer.h`/`.cpp` + tests. **Tests.** `InputBufferStep3`: push/order/overflow/prune/pending/clear. **Commit:** `docs(prediction): freeze Sprint-010 Step-03 spec (InputBuffer)`.

### Step-04 — `StateBuffer`
- **Objective.** A fixed-capacity ring of `PredictedState` keyed by tick, for reconciliation lookup.
- **Scope — In.** `StateBuffer.h`/`.cpp`: `Record(PredictedState)` (ascending tick), `At(tick) -> const PredictedState*` (nullptr if absent/evicted), `Latest()`, `Clear`, `Size`, `Capacity`. Pre-reserved ring; oldest evicted on overflow (bounded, deterministic). **Out.** Prediction/reconciliation logic.
- **FR.** Ascending tick; bounded ring with deterministic eviction; lookup by tick; no allocation during ops.
- **Evidence Gates.** E-G5-P. **Files.** `StateBuffer.h`/`.cpp` + tests. **Tests.** `StateBufferStep4`: record/lookup/eviction/latest. **Commit:** `docs(prediction): freeze Sprint-010 Step-04 spec (StateBuffer)`.

### Step-05 — Deterministic prediction step (integrator)
- **Objective.** The pure, fixed-timestep integrator: apply one input to one state.
- **Scope — In.** `PredictionStep.h` (header-only pure `constexpr`/inline; no `.cpp`): `PredictedState Integrate(const PredictedState& prior, const InputCommand& input, const PredictionConfiguration&)` — deterministic movement/rotation/camera/stance; no authoritative outcomes; no wall-clock. **Out.** Buffers/manager.
- **FR.** Pure value-in/value-out; deterministic (identical inputs → identical output); fixed timestep (tick-derived); movement/rotation only.
- **NFR.** ADR-007; deterministic; engine/OS-free; header-only.
- **Evidence Gates.** E-G2-P groundwork. **Files.** `PredictionStep.h` + tests. **Tests.** `PredictionStepStep5`: determinism, movement/rotation integration, no-input identity. **Commit:** `docs(prediction): freeze Sprint-010 Step-05 spec (prediction integrator)`.

### Step-06 — `PredictionManager`
- **Objective.** Own the input+state buffers + integrator; predict the local player forward.
- **Scope — In.** `PredictionManager.h`/`.cpp`: `RecordInput(InputCommand) -> PredictionOutcome`; `PredictLocal(std::uint64_t toTick) -> const PredictedState&` (integrate buffered inputs from the last confirmed state to `toTick`, capped by `maxPredictionTicks`, recording into the state buffer); read-only `Current()`, `Statistics()` groundwork. Reconciliation is added at Step-11. **Out.** Reconciliation, interpolation, seams.
- **FR.** One deterministic forward integration per `PredictLocal`; capped by `maxPredictionTicks` (`BufferOverflow`/cap as value outcome); records predicted states; deterministic.
- **Evidence Gates.** E-G2-P. **Files.** `PredictionManager.h`/`.cpp` + tests. **Tests.** `PredictionManagerStep6`: record+predict, cap, determinism. **Commit:** `docs(prediction): freeze Sprint-010 Step-06 spec (PredictionManager)`.

### Step-07 — `SnapshotBuffer`
- **Objective.** Buffer authoritative frames for interpolation (previous/current/next + factor/delay).
- **Scope — In.** `SnapshotBuffer.h`/`.cpp`: `Push(SnapshotFrame)` (ascending tick), `Pair(renderTick) -> {from, to, factor}` (the two frames bracketing `renderTick - interpolationDelayTicks`, and the tick-derived interpolation factor in [0,1]), `Clear`, `Size`, `Capacity`. Bounded ring; no extrapolation (clamp). **Out.** Interpolation math/manager.
- **FR.** Ascending tick; bounded; delay applied; factor derived from ticks in [0,1]; no extrapolation beyond the newest frame (clamp to it); deterministic.
- **Evidence Gates.** E-G4-P groundwork. **Files.** `SnapshotBuffer.h`/`.cpp` + tests. **Tests.** `SnapshotBufferStep7`: push/pair/factor/clamp/bounded. **Commit:** `docs(prediction): freeze Sprint-010 Step-07 spec (SnapshotBuffer)`.

### Step-08 — Deterministic interpolation step
- **Objective.** The pure lerp/slerp between two authoritative states by factor.
- **Scope — In.** `InterpolationStep.h` (header-only pure): `InterpolatedState Interpolate(const replication::EntityReplicationState& from, const replication::EntityReplicationState& to, float factor)` (and a player overload) — position lerp, yaw angular interpolation; factor clamped [0,1]. **Out.** Manager/buffer.
- **FR.** Pure; deterministic; factor clamped; `factor=0 -> from`, `factor=1 -> to`; no extrapolation.
- **Evidence Gates.** E-G4-P groundwork. **Files.** `InterpolationStep.h` + tests. **Tests.** `InterpolationStepStep8`: endpoints, midpoint, clamp, determinism. **Commit:** `docs(prediction): freeze Sprint-010 Step-08 spec (interpolation step)`.

### Step-09 — `InterpolationManager`
- **Objective.** Own the snapshot buffer; produce smoothed remote states for a render tick.
- **Scope — In.** `InterpolationManager.h`/`.cpp`: `PushFrame(SnapshotFrame)`; `Interpolate(std::uint64_t renderTick, std::vector<InterpolatedState>& out)` (ascending EntityId; append-only; uses `SnapshotBuffer::Pair` + `InterpolationStep`); read-only surface. **Out.** Prediction/reconciliation.
- **FR.** Ascending, unique, append-only output; no extrapolation beyond delay; deterministic; bounded.
- **Evidence Gates.** E-G4-P. **Files.** `InterpolationManager.h`/`.cpp` + tests. **Tests.** `InterpolationManagerStep9`: smoothing, ordering, determinism, no-extrapolation. **Commit:** `docs(prediction): freeze Sprint-010 Step-09 spec (InterpolationManager)`.

### Step-10 — Client seams (input / authoritative / presentation)
- **Objective.** The engine-free client seams the driver consumes/produces through.
- **Scope — In.** `ILocalInputSource.h`, `IAuthoritativeStateSource.h`, `IPresentationSink.h` (as §7), plus test-support fakes. The authoritative source yields decoded `SnapshotFrame`s (built from Sprint-009 `EntityReplicationState`/`PlayerReplicationState`) + the `ackedInputSequence`. **Out.** The engine binding (Step-15); the driver (Step-14).
- **FR.** Interfaces are engine-free, value-only; no mutation; the authoritative source is read-only; the presentation sink applies client-visual transforms only.
- **Evidence Gates.** E-G1-P. **Files.** the three headers + test fakes; register headers. **Tests.** `ClientSeamsStep10`: fakes drive/observe values; append-only; no mutation. **Commit:** `docs(prediction): freeze Sprint-010 Step-10 spec (client seams)`.

### Step-11 — Reconciliation + error correction
- **Objective.** Deterministic host-wins correction: reconcile the local prediction against an authoritative frame and replay pending inputs.
- **Scope — In.** Extend `PredictionManager`: `Reconcile(const replication::PlayerReplicationState& authoritative, std::uint64_t ackedInputSequence, std::uint64_t toTick) -> PredictionOutcome` — prune inputs `<= ackedInputSequence`; if the predicted state at the authoritative tick differs beyond the configured thresholds, snap to authoritative and **replay** the remaining pending inputs (ascending) through `Integrate`; otherwise smooth within threshold. Deterministic; host authority always wins. **Out.** Interpolation; diagnostics.
- **FR.** Prune by acked sequence; threshold compare (position/rotation/velocity); snap+replay is deterministic and identical for identical inputs; small errors smoothed, large snapped; `CorrectionRejected`/`SequenceMismatch` as value outcomes; never mutates authoritative input.
- **Evidence Gates.** **E-G2-P** (deterministic reconciliation) and **E-G3-P** (host authority wins). **Files.** modify `PredictionManager.h`/`.cpp`; tests. **Tests.** `ReconciliationStep11`: prune, snap+replay determinism, threshold smoothing vs snap, host-wins, sequence mismatch. **Commit:** `docs(prediction): freeze Sprint-010 Step-11 spec (reconciliation)`.

### Step-12 — Prediction validation (negative surface)
- **Objective.** Prove the integrity/bounded/host-authority negatives as value outcomes (test-and-harden; tighten a `.cpp` only on a proven gap; no interface change).
- **Scope — In.** Extend `PredictionTests.cpp`: input-buffer overflow + sequence regress; state-buffer eviction correctness; prediction cap; interpolation no-extrapolation + clamp; reconciliation snap threshold + replay determinism under stress; config invariants; a large-entity + churn deterministic stress with replay identity. Minimal `prediction::*` `.cpp` hardening only if a gap surfaces. **Out.** New behavior/interfaces.
- **FR.** Every rejection is a value outcome leaving state unchanged; determinism preserved under stress; bounded memory under churn; host authority preserved.
- **Evidence Gates.** E-G1-P…E-G5-P negatives. **Files.** modify `PredictionTests.cpp`; minimal `.cpp` only on a gap. **Tests.** `PredictionHardeningStep12`. **Commit:** `docs(prediction): freeze Sprint-010 Step-12 spec (validation hardening)`.

### Step-13 — `PredictionDiagnostics`
- **Objective.** A pure, non-invasive collector of prediction/interpolation metrics.
- **Scope — In.** `PredictionDiagnostics.h` (header-only): `Reset()`, `Snapshot() -> PredictionStatistics` (immutable value), `RecordPrediction(...)`, `RecordCorrection(errorMagnitude)`, `RecordInterpolation(...)`, `RecordOverflow()`. Monotonic counters + avg/max correction (deterministic aggregates); timing fields explicitly diagnostic. **Out.** Any pipeline reference/mutation/output.
- **FR.** Non-invasive; deterministic; monotonic counters; immutable `Snapshot`; `Reset` restores initial.
- **Evidence Gates.** None. **Files.** `PredictionDiagnostics.h` + tests. **Tests.** `PredictionDiagnosticsStep13`: record/snapshot, reset, monotonic, immutable copy. **Commit:** `docs(prediction): freeze Sprint-010 Step-13 spec (diagnostics)`.

### Step-14 — `ClientPresentationDriver`
- **Objective.** The synchronous per-frame orchestration (no thread, no `FrameDispatcher` subscription).
- **Scope — In.** `ClientPresentationDriver.h`/`.cpp`: owns/references the prediction + interpolation managers, the three seams, config, and diagnostics; `Advance(std::uint64_t clientTick, double dt) -> PredictionOutcome` runs: poll input → `RecordInput` → drain authoritative frames (`PushFrame` + `Reconcile` with `ackedInputSequence`) → `PredictLocal` → `Interpolate` → apply via the presentation sink; `SetIdentityMode(bool)` (host = identity). **Out.** Engine binding (Step-15); wiring (Step-16).
- **FR.** One synchronous pass per `Advance`; deterministic; no thread; identity mode on the host (renders authoritative directly, no prediction/interpolation); all value outcomes handled.
- **Evidence Gates.** **E-G1-P/E-G3-P composed**. **Files.** `ClientPresentationDriver.h`/`.cpp`; test-support seams; tests. **Tests.** `ClientDriverStep14`: end-to-end with fakes (input→predict→reconcile→interpolate→present), host identity mode, determinism. **Commit:** `docs(prediction): freeze Sprint-010 Step-14 spec (client presentation driver)`.

### Step-15 — Engine adapter (local input + presentation apply)
- **Objective.** The one engine-touching binding (`EngineAdapters.cpp`): read local input; apply client-visual presentation transforms. Null counterparts in tests.
- **Scope — In.** In `EngineAdapters.cpp`: `EngineLocalInputSource` (reads the local actor input) and `EnginePresentationSink` (applies the predicted local + interpolated remote **presentation** transforms to the client-visual representation — never authoritative ALife/simulation state) + factories; `tests/support/Null*.cpp` counterparts. **Out.** Any authoritative-state mutation; any new engine seam beyond these.
- **FR.** Engine headers only in `EngineAdapters.cpp` (One Engine Boundary); presentation apply writes client-visual transforms only (ADR-008 preserved — not authoritative state); on the host, identity; no retained engine pointers on the value path; values only cross the seams.
- **Evidence Gates.** E-G1-P engine capture (Antigravity, Windows). **Files.** modify `EngineAdapters.cpp`; create the seam factory headers + null test TUs; `xrMP.vcxproj`. **Tests.** null-path tests (engine build smoke is Antigravity's on Windows). **Commit:** `docs(prediction): freeze Sprint-010 Step-15 spec (engine adapter)`.

### Step-16 — Composition-root wiring + integration
- **Objective.** Wire the client-presentation phase into Bootstrap (after `Dispatch`, not a `FrameDispatcher` subscriber); integration tests + `Prediction.md`.
- **Scope — In.** `Bootstrap.cpp`: construct config, managers, seams (engine or null), diagnostics, and the `ClientPresentationDriver`; invoke `Advance` in the client-presentation phase after `FrameDispatcher::Dispatch` within the frame bridge; identity mode selected for the host; reverse-order teardown; default `[prediction]` config block. Integration tests (composed stack with fakes): prediction + replication frames, host disagreement → reconciliation, remote smoothing, simulated latency/packet-delay/loss recovery, determinism/replay. Author `Multiplayer/docs/Prediction.md`. **Out.** Behavior change; new features.
- **FR.** No new `tick_order` key; networking-last preserved; identity on host; reverse-order teardown; existing services/ordering unchanged; deterministic.
- **Evidence Gates.** **E-G2-P/E-G3-P/E-G4-P composed**. **Files.** modify `Bootstrap.cpp`, `tests/BootstrapTests.cpp`; create `Multiplayer/docs/Prediction.md`; vcxprojs. **Tests.** `PredictionIntegrationStep16` + Bootstrap driver-present assertions. **Commit:** `docs(prediction): freeze Sprint-010 Step-16 spec (wiring + integration + Prediction.md)`.

### Step-17 — Sprint closure (no code)
- **Objective.** Confirm the DoD; consolidate evidence; freeze the status docs; declare readiness for Sprint-011.
- **Scope — In.** Cross-check completion criteria (§17) and DoD (§12 of the scope authority) against delivered, verified artifacts; record the final baseline (`442 + Sprint-010 additions`); set `Sprint-010-Prediction-Interpolation.md` to Closed/Frozen (status + closure section); update `CURRENT_STATUS.md`, `SESSION_LOG.md`, README textual progress; a consistency scan. **Out.** Any code/test change (except a genuine closure defect); any Sprint-011 work.
- **FR.** DoD satisfied & recorded; status consistent; Sprint-010 declared Implemented / Verified / Closed / Frozen; Sprint-011 readiness stated.
- **Evidence Gates.** All E-G*-P recorded. **Files.** the status docs + README. **Tests.** none new (baseline re-confirmed). **Commit:** `docs(prediction): close Sprint-010`.

---

## 16. Verification gates (Evidence Gates)

Sprint-010 is verifiable when: **E-G1-P** (no authoritative mutation; client-only; host identity), **E-G2-P** (deterministic prediction & reconciliation), **E-G3-P** (host authority always wins), **E-G4-P** (interpolation correctness, no extrapolation), and **E-G5-P** (bounded memory & robust validation) are all satisfied — each closed by its owning step (§14.1) and confirmed composed at Steps 14/16. The engine capture path (Step-15) is smoke-verified by Antigravity on Windows; the engine-free core is fully unit-tested on GCC + MSVC.

## 17. Acceptance criteria

1. All 17 steps implemented, each independently verified, tree buildable after each.
2. Local prediction is responsive; remote motion is smooth; reconciliation is deterministic; **host authority always wins**.
3. Prediction never modifies authoritative simulation/ALife state (ADR-008); on the host the presentation apply is identity.
4. One Engine Boundary **and** One Platform Boundary hold — the only engine-touching code is in `EngineAdapters.cpp` (input read + presentation apply); no OS code; the test build links null counterparts.
5. **No new authoritative `tick_order` key**; networking-last and the frozen dispatcher table are unchanged; client presentation runs outside `FrameDispatcher` after `Dispatch`.
6. ADR-010 unchanged for the core; at most one additive, never-renumbered id (`kMsgClientInput = 0x0300`) reserved and isolated to the seam/transport step.
7. Full suite green on GCC + MSVC, MSVC Release clean under `EngineAbi.props`, zero new warnings, no regressions; the Sprint-009 442/442 baseline preserved and extended.
8. No non-goal implemented (no lag compensation/server rewind/hit validation/anti-cheat/save/replay); prediction predicts only local movement/rotation/camera/stance/basic interactions.
9. Subsystem doc `Multiplayer/docs/Prediction.md` written; all status docs synchronized to Closed/Verified.

## 18. Sprint completion checklist

- [ ] Steps 1–17 implemented and each independently verified; tree buildable after each (GCC + MSVC), zero new warnings.
- [ ] Local prediction responsive; remote interpolation smooth; reconciliation deterministic; **host authority always wins** (E-G3-P).
- [ ] Prediction never mutates authoritative state (E-G1-P); host identity mode verified; ADR-008 preserved.
- [ ] Deterministic prediction + reconciliation replay (E-G2-P); interpolation bounded, no extrapolation (E-G4-P); buffers bounded, negatives are value outcomes (E-G5-P).
- [ ] One Engine Boundary intact (engine code only in `EngineAdapters.cpp`: local input + presentation apply); One Platform Boundary intact (no OS code).
- [ ] No new `tick_order` key; networking-last preserved; client presentation runs as a post-`Dispatch` phase, not a dispatcher subscriber.
- [ ] ADR-007/008/009/010/011 preserved; at most one additive never-renumbered `kMsgClientInput = 0x0300`, isolated to the seam/transport step; no frozen sprint/ADR modified.
- [ ] Final test counts + build status recorded; `Prediction.md` written; the three status docs updated to Closed/Verified.
- [ ] Sprint-010 Definition of Done satisfied; architecture unchanged (frozen).

---

## 19. One-pass freeze note

This document specifies **all 17 Sprint-010 steps in one pass** with explicit inter-step dependencies (§14), so the full plan can be reviewed, frozen, and approved before any implementation begins. Each step is independently implementable and verifiable against only the steps before it. Implementation proceeds under the mandatory workflow (implement → Antigravity verification → git commit → GitHub push → next step), one step at a time, exactly as Sprints 008–009 were executed.

**Recommended git commit message (freeze of this plan):**
```
docs(prediction): freeze complete Sprint-010 architecture & implementation plan (Steps 01–17)

Client-side prediction & interpolation: value types, config, input/state buffers,
deterministic prediction integrator, PredictionManager, snapshot buffer,
interpolation, InterpolationManager, client seams, deterministic reconciliation,
validation hardening, diagnostics, client-presentation driver, engine adapter
(input + presentation apply, One Engine Boundary), wiring + integration, closure.
Client-only presentation overlay; host authority always wins; no authoritative
mutation; no thread; no new tick_order key (networking-last preserved);
ADR-007..ADR-011 preserved. Compatible with the Sprint-009 baseline (442/442).
```

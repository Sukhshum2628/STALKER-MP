# Client Prediction & Interpolation (Sprint-010)

STALKER-MP is host-authoritative and deterministic. Client Prediction &
Interpolation is a **client-side presentation layer** that makes the
host-authoritative world feel responsive: it predicts the local player forward
from local input, interpolates remote entities smoothly between authoritative
frames, and deterministically reconciles the local prediction against the host —
**host authority always wins**. It consumes replication output and **never mutates
authoritative simulation state** (ADR-008). It runs synchronously on the engine
main thread, creates no thread, and adds no new `tick_order` key.

## Where it runs in the frame

The subsystem is **not** a `FrameDispatcher` subscriber. The single engine frame
bridge calls, once per frame and in order:

1. `FrameDispatcher::Dispatch(dt)` — the authoritative simulation pipeline
   (World → EntityRegistry → PlayerLifecycle → Bubble → ALifeTransition →
   Snapshot → Replication → … → Networking; networking-last preserved).
2. `detail::AdvanceClientPresentation(dt)` — the **client-presentation phase**,
   strictly after `Dispatch`.

Running after `Dispatch` (rather than as a subscriber) keeps the networking-last
invariant intact and introduces no new ordering key. The real-time `dt` is **not**
used in control flow — the path is tick-driven, preserving Replay Determinism.

## Components (engine-free core)

| Component | Role |
| --- | --- |
| `PredictionTypes.h` | Value vocabulary: `InputCommand`, `PredictedState`, `InterpolatedState`, `SnapshotFrame`, `PredictionStatistics`, outcomes. |
| `PredictionConfiguration` | `[prediction]` config (buffer depths, interpolation delay, prediction cap, correction thresholds). |
| `InputBuffer` / `StateBuffer` | Bounded, ascending rings for buffered inputs and predicted states (deterministic eviction). |
| `PredictionStep::Integrate` | Pure, fixed-timestep movement/rotation integrator (one input → one state). |
| `PredictionManager` | Owns the buffers + integrator; `RecordInput`, `PredictLocal`, and `Reconcile` (host-wins correction + replay). |
| `SnapshotBuffer` | Bounded ring of authoritative frames; selects the two frames bracketing a delayed render tick (no extrapolation). |
| `InterpolationStep` | Pure position lerp + shortest-arc yaw interpolation (clamped factor). |
| `InterpolationManager` | Owns the snapshot buffer; produces ascending, unique, append-only remote states for a render tick. |
| `PredictionDiagnostics` | Pure, non-invasive metric collector (monotonic counters + correction aggregates; timing is diagnostic-only). |
| `ClientPresentationDriver` | Synchronous per-frame orchestration of the above through the three seams. |

## Seams (the one engine boundary)

Three engine-free interfaces isolate the engine:

- `ILocalInputSource` — reads the local actor's input for the tick.
- `IAuthoritativeStateSource` — yields decoded authoritative `SnapshotFrame`s
  (built from the Sprint-009 replication value shapes, carrying
  `ackedInputSequence`). Read-only.
- `IPresentationSink` — applies the predicted local + interpolated remote
  **client-visual** transforms (render only; never authoritative state).

The engine implementations live only in `EngineAdapters.cpp` (One Engine
Boundary); the test build links inert null counterparts. Only values cross the
seams — no engine pointer is retained on the value path.

## One `Advance` pass (client mode)

1. **Poll input** → `PredictionManager::RecordInput`.
2. **Drain authoritative frames** → `InterpolationManager::PushFrame` (buffer for
   interpolation) and `PredictionManager::Reconcile` the local player.
3. **Predict** the local player forward → `IPresentationSink::ApplyLocal`.
4. **Interpolate** remote entities → `IPresentationSink::ApplyRemote`.

## Reconciliation (host authority wins)

`Reconcile(authoritative, ackedInputSequence, toTick)`:

1. Reject a regressed acknowledgement (`SequenceMismatch`) or a conflicting local
   entity (`CorrectionRejected`) — both without mutating state.
2. Prune acknowledged inputs, **snap the confirmed baseline to the authoritative
   position** (host wins), and deterministically **replay** the remaining pending
   inputs through `Integrate`.
3. Classify the applied correction against the configured thresholds:
   `None` / `Smoothed` (small) / `Snapped` (large). The authoritative baseline is
   used either way; the classification informs how the driver presents the
   correction. Velocity and yaw (which the host player state does not carry) are
   carried forward for visual continuity.

## Interpolation (no extrapolation)

Remote entities are rendered `interpolation_delay_ticks` behind the newest
authoritative frame (a jitter buffer). The snapshot buffer selects the two frames
bracketing `renderTick − delay` and a tick-derived factor in `[0,1]`; a target
before the oldest or beyond the newest **clamps** to that boundary frame — it
never extrapolates. Output is ascending, unique by `EntityId`, and append-only.

## Host identity mode

On the host, `SetIdentityMode(true)` bypasses prediction/interpolation and renders
authoritative frames directly. The composition root selects identity mode because
the host owns the authoritative simulation. A client build sets identity mode off
and supplies a receive-decoded authoritative source behind the same seam (the
client receive/decode pipeline is future work; today the host source yields no
frames and the phase is inert on the host).

## Determinism & governance

- **Deterministic / Replay Determinism** — tick-driven; identical inputs + frames
  produce identical output; wall-clock/`dt` never enter control flow.
- **Host Authority / ADR-008** — reconciliation snaps toward authoritative;
  presentation writes client-visual transforms only; authoritative state is never
  mutated.
- **One Engine Boundary / One Platform Boundary** — engine headers confined to
  `EngineAdapters.cpp`; the core is engine-free and platform-free.
- **ADR-007** — no exceptions, no RTTI, no iostream in the prediction core; all
  fallible operations return `PredictionOutcome` value outcomes.
- **No new `tick_order` key; networking-last preserved** — the phase runs after
  `Dispatch`, not as a subscriber.

## Configuration (`[prediction]`, client.cfg)

| Key | Default | Meaning |
| --- | --- | --- |
| `input_buffer_depth` | 128 | Buffered local inputs retained for prediction/replay (≥ 1). |
| `state_buffer_depth` | 128 | Predicted states retained for reconciliation (≥ 1). |
| `interpolation_delay_ticks` | 2 | Remote render delay behind the newest frame (may be 0). |
| `max_prediction_ticks` | 8 | Cap on ticks predicted ahead of the confirmed state (≥ 1). |
| `position_correction_threshold_mm` | 250 | Position error (mm) above which a correction snaps. |
| `rotation_correction_threshold_mrad` | 100 | Yaw error (mrad) above which a correction snaps. |
| `velocity_correction_threshold_mm` | 500 | Velocity error (mm) above which a correction snaps. |
| `version` | 1 | Format version stamped into diagnostics (≥ 1). |

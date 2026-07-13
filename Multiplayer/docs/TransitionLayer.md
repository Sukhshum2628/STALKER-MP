# ALife Transition Layer (Sprint-005) — Subsystem Documentation

- **Status:** Implemented & Verified (Closed) — 2026-07-10.
- **Architecture:** `Sprint-005-Architecture.md` (FROZEN 2026-07-09). This document records the delivered subsystem; it introduces no new decisions.
- **Governing ADRs:** ADR-007 (Engine ABI Contract), ADR-008 (Engine State Mutation Boundary — accepted & implemented).

---

## 1. Purpose

The ALife Transition Layer consumes the Bubble Manager's per-tick activation/deactivation decisions (Sprint-004) and drives the engine's **vanilla** ALife online/offline switching so each decided entity moves between offline (strategic) and online (full) simulation — without duplicating simulation, without editing ALife, and preserving one authoritative host-driven world.

It is the first STALKER-MP subsystem that **writes** engine simulation state (Sprints 1–4 only observed it), which is why it is governed by ADR-008.

## 2. Ownership boundaries

| Data | Owner |
|---|---|
| Activation decision (which entities should be online) | Bubble Manager (Sprint-004) |
| Entity identity / registration / liveness | Entity Registry (Sprint-003) |
| Per-entity transition **intent** + in-flight bookkeeping | **TransitionManager (Sprint-005)** |
| Actual object online/offline status | **Engine ALife (vanilla, unchanged)** |
| Engine access to perform the switch | `EngineAdapters.cpp` gateway only |

The engine owns the real online/offline status; the Transition Layer owns only *intent* and drives the vanilla switch (Preserve Before Replace, Host Authority).

## 3. Components

- `world::TransitionTypes.h` — value types: `TransitionState` (Offline / PendingOnline / Online / PendingOffline), `TransitionKind`, `TransitionOutcome`, `TransitionCommand`, `TransitionRecord`, `TransitionResult`, `TransitionStatistics`. Engine-free.
- `world::TransitionIntentStore` — canonical intent store: sorted `std::vector` keyed ascending by `EntityId` + `std::unordered_map` accelerator (BC-005), with `ValidateConsistency`.
- `world::IAlifeSwitchGateway` — engine-free seam: `Apply(commands) -> outcomes` (parallel to input); `IsOnline(EntityId) -> optional<bool>`.
- `world::TransitionManager` — engine-free core: owns intent, runs the per-tick pipeline, exposes read-only surface + diagnostics.
- `world::TransitionManagerService` — `IService` + `ITickable`; owns the manager; ticked at `tick_order::kAlifeTransition = 350`.
- `adapters::EngineAlifeSwitchGateway` — the single engine-touching implementation (in `EngineAdapters.cpp`); `adapters::NullAlifeSwitchGateway` — inert test-build counterpart; `adapters::CreateEngineAlifeSwitchGateway()` — factory.

## 4. Per-tick pipeline (`TransitionManager::Update`)

Runs once per frame at `kAlifeTransition = 350`, strictly after the Bubble (300):

1. **Reconcile (Step 6):** confirm prior-tick `Pending*` intents against engine truth via `IsOnline()` — `PendingOnline → Online` on `true`, `PendingOffline → Offline` on `false`; `nullopt`/mismatch leave the entity pending. Ascending order. No Apply.
2. **Ingest + dedup (Step 4):** read Bubble `Activations()`/`Deactivations()`, drop entities not in the registry, emit a command only on a genuine edge, stage one ordered batch (ascending `EntityId`), and mark `Pending*` intent.
3. **Apply (Step 5):** apply the batch through the gateway in one phase; record one `TransitionOutcome` per command; build the read-only `TransitionResult` (only `Applied` switches are deltas).

Idempotent (dedup against intent), replay-safe (identical inputs → empty result), and deterministic (ascending `EntityId` throughout; single-step per tick).

## 5. Engine switching mechanism — Cooperative Flag Override (frozen, E-G1)

The gateway drives vanilla ALife **only** through the id-based override flags — it sets the exact predicates the engine's own `try_switch_*` path evaluates and lets the engine's ALife update perform the switch. It never calls forced `switch_*`, never edits ALife.

- **Activate (pin online):** `CALifeUpdateManager::set_switch_online(id, true)` + `set_switch_offline(id, false)`.
- **Deactivate (pin offline):** `set_switch_offline(id, true)` + `set_switch_online(id, false)`.

`EntityId` (== `ALife::_OBJECT_ID`, the `u16` `object->ID()`) is resolved to the ALife object **on demand** via `objects().object(id, /*no_assert=*/true)`; **no** engine pointer or id→object cache is retained (E-G2). Missing object / no ALife → benign `EntityMissing` / `std::nullopt`. This cooperates with, rather than races, the vanilla distance-based switching (flip-flop measured at zero).

## 6. Composition-root integration

`TransitionManagerService` is created in `Bootstrap.cpp` after the Bubble service, owned by the `ServiceRegistry`, with `Dependencies() = {World, EntityRegistry, BubbleManager}`. It is injected with the `BubbleManager`, the `EntityRegistry`, and the gateway from `CreateEngineAlifeSwitchGateway()` (Runtime-owned; null in tests), and subscribed to the single existing `FrameDispatcher` at `tick_order::kAlifeTransition = 350`. Teardown unsubscribes the service before the Bubble and before `ShutdownAll` (reverse-order). Deterministic frame order: World (100) → EntityRegistry (200) → Bubble (300) → **ALifeTransition (350)** → Replication (400, future).

## 7. Diagnostics

`Statistics()` (intent tallies + this tick's applied/skipped/failed), `DescribeState()`, `DumpPending()`, and `ValidateConsistency()` (intent-store ordering, state legality, staged-batch ordering, staged↔intent consistency, records↔batch parallelism, result consistency). All read-only, deterministic, iostream-free (`common::Format`).

## 8. Boundary & invariants

Only `EngineAdapters.cpp` includes engine headers (One Engine Boundary); `TransitionManager`, the service, the interface, value types, and the store are engine-free and covered by the null-gateway test build. ADR-007 (no exceptions, `Expected<T>`/value outcomes, C4530-clean) and ADR-008 (mutation only via the sanctioned entry point through the single gateway, on-demand resolution, no presumed synchrony) hold throughout.

## 9. Future-sprint consumption

`TransitionResult` (per-tick online/offline delta by `EntityId`, read-only, ascending) is the activation delta Sprint-008 replication will consume. Intent state is transient and reconstructable; engine ALife online/offline is persisted by the engine's own save system (no new persistence format). The single gateway is the one seam for future host-authority work.

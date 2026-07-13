# Sprint-007 · Step 11 — Service Wiring (`kPlayerLifecycle` + Bootstrap) — Implementation Specification

- **Status:** Step Specification (pre-implementation). Derives exclusively from the frozen Architecture (§8/§12, AD-2/AD-3), Implementation Plan (Step 11), Clarifications §7.2/§7.3, the Player Request Message-ID Allocation, and the MessageRegistry Registration Verification Rules. No architecture/ADR/scope/order change.
- **Governance:** ADR-007; ADR-009/010 (only additive `MessageRegistry` ids — no wire/framing change); ADR-011. One Engine Boundary / One Platform Boundary preserved.
- **Nature:** Integrate the subsystem as one `ITickable` at the additive `tick_order::kPlayerLifecycle = 250`, with correct init/teardown, the Bubble feed swap, and additive message-id registration. Touches the frozen `Bootstrap.cpp`/`FrameDispatcher.h` **once**.

---

## 1. Step objective

Deliver `player::PlayerManagerService` (`IService` + `ITickable`); add `tick_order::kPlayerLifecycle = 250`; register the service in `Bootstrap.cpp` after the Sprint-006 services; subscribe it to the `FrameDispatcher` at 250 and to `net::Session` as an `ISessionObserver`; bind `NetworkedPlayerPositionSource` into the Bubble Manager in place of the Sprint-004 `LocalPlayerPositionSource`; register the additive player-request `MessageRegistry` ids (`0x0100`/`0x0101`) per the frozen allocation and verification rules.

## 2. Scope

In scope: create `include/stalkermp/player/PlayerManagerService.h` + `src/player/PlayerManagerService.cpp`; add `kPlayerLifecycle = 250` to `FrameDispatcher.h`; modify `Bootstrap.cpp` (registration, subscription, teardown, feed swap, message registration, factory selection engine-vs-null); create `include/stalkermp/player/PlayerMessageIds.h` (the single-source-of-truth constants for `0x0100`/`0x0101`); update `tests/BootstrapTests.cpp`. Engine gateway comes from `CreatePlayerSpawnGateway()` (engine build) / null (tests).

## 3. Explicit out-of-scope items

Diagnostics population (Step 12); validation hardening (Step 13); integration/docs (Step 14). No new wire format/header/endianness (ADR-010) — only additive ids; no online/offline switching (Sprint-005); no message **payload/handler behavior** beyond registering the ids and binding their host-side request handlers to the manager (handler *semantics* are minimal request routing — no replication/snapshot); no reserved id (`0x0102–0x010F`) registered.

## 4. Dependencies

Steps 01–10. Sprint-002 `core::FrameDispatcher`/`IService`/`ITickable`; Sprint-003 Entity Registry; Sprint-004 Bubble Manager + `IPlayerPositionSource`; Sprint-005 Transition service; Sprint-006 `net::Session`, `net::MessageRegistry`, `net::ISessionObserver`. Clarifications §7.2/§7.3; the frozen allocation + registration verification rules.

## 5. Files to create

- `include/stalkermp/player/PlayerManagerService.h` + `src/player/PlayerManagerService.cpp`.
- `include/stalkermp/player/PlayerMessageIds.h` — `kMsgPlayerJoinRequest{0x0100}`, `kMsgPlayerRespawnRequest{0x0101}` (single-source-of-truth; Sprint-006 `ProtocolConstants.h` untouched).

## 6. Files to modify

- `include/stalkermp/core/FrameDispatcher.h` — add `inline constexpr std::uint32_t kPlayerLifecycle = 250;` in `tick_order` (between `kEntityRegistry = 200` and `kBubble = 300`).
- `src/core/Bootstrap.cpp` — construct/register `PlayerManagerService` after Sprint-006 services; subscribe at 250 + as `ISessionObserver`; swap the Bubble's position source to `NetworkedPlayerPositionSource`; register `0x0100`/`0x0101`; reverse-order teardown; select engine-vs-null gateway.
- `tests/BootstrapTests.cpp` — subscriber count + ordering asserts (§7.3).
- Both vcxprojs — add the service `.cpp` + headers.

## 7. Public interfaces / types

- `class PlayerManagerService final : public core::IService, public core::ITickable` — `Name()`, `Dependencies() = {World, EntityRegistry, BubbleManager, AlifeTransition, Networking}` (ordering-only const edges), `Initialize()`, `Shutdown()`, `Tick(...)`; owns the `PlayerManager`; exposes read-only accessors (`Manager()`, `LastTick()`).
- `PlayerMessageIds.h` constants (data-range `MessageId`s).

## 8. Internal components

The service owns a `PlayerManager` (Step 7), the `NetworkedPlayerPositionSource` (Step 8), the `PlayerSessionObserver` + `PlayerDeltaQueue` (Step 6), and holds the injected gateway (`unique_ptr`, engine or null) via the Runtime. `Tick` advances a monotonic tick and calls `PlayerManager::ApplyPendingDeltas(tick)`.

## 9. Responsibilities

Compose and run the subsystem: on `Initialize`, subscribe to `Session` (observer) and register message ids; bind the Bubble to the networked position source; on each `Tick` at 250, drain+apply queued deltas so a joined player is visible to the Bubble (300) the same frame; on `Shutdown`, reverse-order unsubscribe (FrameDispatcher + Session) and despawn owned entities before Sprint-006 teardown. Register exactly `0x0100`/`0x0101` (once each), treating a duplicate-registration outcome as a hard `Initialize` failure (registration verification rules R2/R6.3).

## 10. Data structures

Reuses all prior `player::*` types; introduces the two `MessageId` constants. No new value type beyond `PlayerMessageIds.h`.

## 11. Invariants

Existing `tick_order` table preserved — only `kPlayerLifecycle = 250` added, strictly between 200 and 300; frame order EntityRegistry(200) → **Player(250)** → Bubble(300) → Transition(350) → Networking(900); strict reverse-order teardown; Bubble Manager code unchanged (only the injected source instance differs); networking untouched except the two additive ids; observer remains enqueue-only (AD-3); the registered id set equals `{0x0100, 0x0101}` exactly (no reserved id registered) — registration verification rules R1–R6.

## 12. Ownership rules

The `ServiceRegistry` owns `PlayerManagerService`; the service owns the `PlayerManager` (hence the registry), the position source, and the observer/queue; the Runtime owns the gateway `unique_ptr`. Session/Bubble/Entity Registry/Transition remain owned by their sprints (const-ref edges). No ownership change to any prior subsystem.

## 13. Naming conventions

`PlayerManagerService`; `kPlayerLifecycle`; `PlayerMessageIds.h` with `kMsgPlayer*` constants; `#pragma once`. Matches the Sprint-006 `NetworkingService`/`tick_order` precedent.

## 14. Step-specific constraints

Touches `Bootstrap.cpp`/`FrameDispatcher.h` exactly once; only additive ids (no ADR-010 change); no switching; no diagnostics logic. The Bubble feed swap replaces the injected `IPlayerPositionSource` instance only — no Bubble code edit. Engine gateway selected in the engine build; null in tests. Registration follows the frozen verification rules.

## 15. Determinism constraints

The 250 slot makes a joined player visible to activation the same frame; the 900→250 ingress latency is deterministic (AD-2/AD-3); message-id resolution is order-independent (registration verification R5); replay unchanged regardless of registration order.

## 16. Engine-boundary constraints

Bootstrap composes the engine gateway (engine build) or null (tests) via the factory; **no** engine header is added to `Bootstrap.cpp` (it includes only the factory header, as with Sprint-006's transport). One Engine Boundary preserved.

## 17. Platform-boundary constraints

OS-free at the wiring layer; no socket/platform header in the service or Bootstrap for this step. One Platform Boundary preserved.

## 18. Acceptance criteria

Service ticks once at 250; frame order and subscriber count correct; reverse-order teardown leaves no orphan (record/mapping/subscription/entity); Bubble consumes the networked feed; exactly `0x0100`/`0x0101` registered once each, duplicate registration fails `Initialize`, no reserved id registered (verification rules R1–R6); engine/OS-free at the wiring layer; 238/238 + new wiring tests green; zero new warnings.

## 19. Test requirements

`BootstrapTests` (per Clarifications §7.3): (1) dependency-graph validation (no cycle; service after its five deps); (2) initialization order (service after deps; Session subscription + feed swap in `Initialize`); (3) shutdown order (reverse; unsubscribe + despawn before Sprint-006 teardown); (4) FrameDispatcher subscription order + updated subscriber count + `static_assert(kEntityRegistry < kPlayerLifecycle && kPlayerLifecycle < kBubble)`; (5) Session-observer registration ordering (registered in `Initialize`, deregistered before `Session` teardown; enqueue-only). Plus message-registration verification (R6.1–R6.6): required ids present once, reserved absent, duplicate rejected, missing fails, incorrect binding fails, order-independence. End-to-end lifecycle through the composed null-gateway stack.

## 20. Completion checklist

- [ ] `PlayerManagerService.h`/`.cpp` (`IService`+`ITickable`, `Dependencies`, `Initialize`/`Shutdown`/`Tick`).
- [ ] `kPlayerLifecycle = 250` added to `FrameDispatcher.h`.
- [ ] `PlayerMessageIds.h` (`0x0100`/`0x0101`); `ProtocolConstants.h` untouched.
- [ ] `Bootstrap.cpp`: register/subscribe/feed-swap/register-ids/reverse-teardown/gateway-select.
- [ ] `BootstrapTests` updated (five §7.3 checks + registration verification R1–R6).
- [ ] Only additive ids (ADR-010 intact); Bubble code unchanged; no switching.
- [ ] Engine/OS-free wiring; 238/238 + new tests green; zero new warnings.

## 21. Stop condition

Stop when the subsystem is wired, ticks at 250, tears down cleanly, and all wiring/registration verifications pass. Do not begin Step 12.

## 22. Self-review

- **Architecture compliance.** Matches §8/§12, AD-2 (250 slot), AD-3 (enqueue-only observer); additive ids per §16 + the frozen allocation. Compliant.
- **ADR compliance.** ADR-007 clean; ADR-009/010 intact (additive ids only, no wire change); ADR-011 single-threaded. One Engine Boundary preserved (factory, no engine header in Bootstrap).
- **Dependency correctness.** Steps 01–10 + Sprint-002/003/004/005/006 seams + Clarifications/allocation/verification rules. Correct.
- **Scope isolation.** Wiring + additive ids only; no diagnostics/validation/switching. Isolated.
- **Determinism.** 250 slot, deterministic ingress latency, order-independent registration. Sound.
- **Engine/Platform boundary.** Factory-composed gateway; no engine/OS header at wiring. Preserved.
- **Ownership.** Service owns manager/source/observer; Runtime owns gateway; prior subsystems unchanged. Correct.
- *Ambiguity resolved:* message **handlers** registered here perform only minimal host-side request routing into the manager (`RequestJoin`/`RequestRespawn`), not any replication/snapshot behavior — keeping Step 11 within scope and honoring Host Authority. Consistent with §16 and the non-goals. No other issue.

## 23. Step-11 Specification Freeze

Sprint-007 Step-11 (Service Wiring) Specification is **FROZEN** (2026-07-11). Derived exclusively from the frozen architecture (§8/§12, AD-2/AD-3), plan (Step 11), Clarifications §7.2/§7.3, the message-id allocation, and the registration verification rules; no architecture/ADR/order/scope change. Self-review passed; ambiguity resolved within the step. Existing tick order, One Engine/Platform Boundary, ADR-009/010/011, determinism, ADR-007, ownership preserved. Ready for implementation.

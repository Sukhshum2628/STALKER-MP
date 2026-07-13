# Sprint-007 · Step 01 — Player Value Types — Implementation Specification

- **Status:** Step Specification (pre-implementation). Derives exclusively from the frozen `Multiplayer/docs/Sprint-007-Architecture.md` (§9 component value shapes, §11 ownership, §13 determinism, §19 error model) and the frozen `Documentation/SPRINTS/Sprint-007-Implementation-Plan.md` (Step 1). Introduces no architectural decision, no ADR change, no scope change, no order change.
- **Governance:** ADR-007 (engine-free, no exceptions, no RTTI, no iostream), ADR-009/010/011 preserved (untouched by this step). One Engine Boundary and One Platform Boundary preserved.
- **Nature:** The engine-free / OS-free vocabulary every later Sprint-007 step consumes. **Value types, enumerations, POD-style structures, and `const char*` name helpers only.** No logic.

---

## 1. Step objective

Establish the frozen engine-free vocabulary for the `player::` subsystem: the identity, lifecycle-state, connection-state, disposition, per-stage outcome enumerations, and the POD record/view/statistics structures named in architecture §9, together with `const char*` `Name()` helpers (mirroring the Sprint-006 `*Name()` precedent). This is the type foundation on which Steps 2–15 build; it contains no behavior.

## 2. Scope

In scope: create `include/stalkermp/player/PlayerTypes.h` containing — the `PlayerId` reuse alias (identity reused from `world::PlayerId`), the enumerations `PlayerLifecycleState`, `PlayerConnectionState`, `DisconnectDisposition`, `JoinOutcome`, `SpawnOutcome`, `ReconnectOutcome`; the POD structures `PlayerRecord`, `PlayerMappingView`, `PlayerStatistics`; and a `const char*` name helper per enumeration. Wire the header into both build projects and create the (initially minimal) `tests/PlayerTests.cpp`. Everything is engine-free, OS-free, header-only value definitions.

## 3. Explicit out-of-scope items

Not in this step (deferred to their planned steps, per the Implementation Plan): registry storage/allocation (Step 3), lookup APIs (Step 4), configuration parsing (Step 2), lifecycle transition logic (Step 5), session observer / delta queue (Step 6), `PlayerManager` (Step 7), position source (Step 8), spawn-gateway interface/null/adapter (Steps 9–10), service wiring / `tick_order` / Bootstrap / message registration (Step 11), diagnostics (Step 12), validation hardening (Step 13), integration/docs (Step 14). Also excluded here: any function with behavior beyond `constexpr` comparison / `Name()`; any networking, engine, or platform contact; any `MessageId` constant (those live in the frozen `PlayerMessageIds.h`, introduced no earlier than Step 7 per the plan, not here); any serialization routine.

## 4. Dependencies

Sprint-001 core is not required at this step beyond what the reused ids pull in. Reused frozen value types (referenced, never redefined): `world::PlayerId` and `world::PlayerPosition` (`include/stalkermp/world/BubbleTypes.h`, Sprint-004), `world::EntityId` (`include/stalkermp/world/EntityView.h`, Sprint-003), `net::ConnectionId` and `net::DisconnectReason` (`include/stalkermp/net/NetTypes.h`, Sprint-006). No other subsystem is consumed. No later Sprint-007 step is depended upon.

## 5. Files to create

- `include/stalkermp/player/PlayerTypes.h` — all Step-01 value types, enumerations, structures, and `Name()` helpers, in `namespace stalkermp::player`.
- `tests/PlayerTests.cpp` — new test translation unit for the `player::` subsystem (Step-01 populates it with trivial construction / `Name()` round-trip checks only; later steps extend it).

## 6. Files to modify

- `Multiplayer/xrMP.vcxproj` — add `include\stalkermp\player\PlayerTypes.h` to `ClInclude`.
- `Multiplayer/tests/xrMP_Tests.vcxproj` — add `PlayerTests.cpp` to `ClCompile` and ensure `include\stalkermp\player\PlayerTypes.h` is visible (via the existing `..\include` include path).

No source (`.cpp`) file in `src/` is created or modified (this step is header + test only). `Bootstrap.cpp`, `FrameDispatcher.h`, `EngineAdapters.cpp`, `ProtocolConstants.h`, and all Sprint-001–006 headers are **untouched**.

## 7. Public types

All in `namespace stalkermp::player`, all engine-free and OS-free:

- `using PlayerId = world::PlayerId;` — identity is **reused, not redefined** (architecture §9: "`PlayerId` (reuses `world::PlayerId`)"). Value `0` remains "none" per the frozen `world::PlayerId` contract. An identity value only — never a connection/session/network handle.
- `struct PlayerRecord` — the canonical player record (fields in §10).
- `struct PlayerMappingView` — a read-only projection of the id↔id mapping (fields in §10).
- `struct PlayerStatistics` — read-only tallies (fields in §10).
- The enumerations of §9 and their `Name()` helpers (§13).

## 8. Internal types

None. Step-01 defines no `player::detail::*` or file-private type; every type is a public value type in `namespace stalkermp::player`. (Accelerators, allocators, and any internal storage helpers belong to Step 3+, not here.)

## 9. Enumerations

All enumerations are `enum class` with an explicit `std::uint8_t` underlying type (fixed layout, deterministic; mirrors Sprint-006 `NetTypes.h`). Enumerator order is fixed and additive; the first enumerator represents the natural initial/neutral value.

- `enum class PlayerLifecycleState : std::uint8_t { Joining, Active, Dead, AwaitingRespawn, Suspended, Removed };` — architecture §9.
- `enum class PlayerConnectionState : std::uint8_t { Connected, Suspended, Reclaimed };` — architecture §9.
- `enum class DisconnectDisposition : std::uint8_t { … };` — the disposition of a disconnect (e.g. retain-vs-remove intent). Enumerators fixed per architecture §9/§12 (retain on ordinary disconnect → Suspended; explicit removal → Removed). Exact enumerator set is fixed in this spec as `{ Retain, Remove };` (Retain = suspend and keep the record/entity; Remove = release — architecture §12/§17).
- `enum class JoinOutcome : std::uint8_t { Accepted, RejectedCapacity, RejectedDuplicate, RejectedInvalid, RejectedSpawnFailed };` — the per-stage join result vocabulary (architecture §18/§19; values chosen to name the failure surface §18 enumerates; carries no logic here).
- `enum class SpawnOutcome : std::uint8_t { Spawned, EntityMissing, EngineUnavailable, RejectedInvalid };` — the spawn/despawn result vocabulary (architecture §18; the gateway that produces these is Steps 9–10, but the vocabulary is defined here).
- `enum class ReconnectOutcome : std::uint8_t { Reclaimed, TokenUnknown, AlreadyActive, RejectedInvalid };` — the reconnect result vocabulary (architecture §18, reconnect-without-duplication via the Sprint-006 token).

> The enumerator sets above are the frozen Step-01 vocabulary. They name the outcome surface the architecture already describes; they add no new behavior and no new failure mode beyond §18/§19. If a later step needs an additional enumerator, it is added **additively** at the end (never renumbered), consistent with the project's additive-enumerator discipline.

## 10. Structures

POD-style aggregates (no user-declared constructors beyond default member initializers; no virtuals; no ownership of heap resources). Fields hold value ids and enumerations only.

- `struct PlayerRecord`
  - `PlayerId id{};` — the owning identity (0 = none).
  - `net::ConnectionId connection{};` — current bound connection (Sprint-006 value; 0 = none when suspended).
  - `std::uint64_t sessionMember = 0;` — session-member key (the Sprint-006 session membership token/id, value-only).
  - `world::EntityId entity{};` — the persistent world entity (0 = none until spawned).
  - `PlayerLifecycleState lifecycle = PlayerLifecycleState::Joining;`
  - `PlayerConnectionState connectionState = PlayerConnectionState::Connected;`
  - `world::PlayerPosition lastPosition{};` — last host-owned position (positions-only; reused Sprint-004 value).
  - `std::uint64_t joinTick = 0;` — tick of join (tick-derived timing; no wall-clock).
  - `std::uint64_t respawnEligibleTick = 0;` — scheduled respawn tick (0 = not scheduled).
  - `std::uint64_t reconnectToken = 0;` — the Sprint-006 deterministic reclaim token (value-only).
  - *(Reserved-for-future-persistence fields are intentionally NOT added here; the record is deliberately reconstructable — architecture §17 — but Step-01 defines only the fields the frozen §9 record requires. No durable-serialization field is introduced.)*
- `struct PlayerMappingView` — read-only projection: `PlayerId id{}; net::ConnectionId connection{}; std::uint64_t sessionMember = 0; world::EntityId entity{};`. Used by the Step-4 `Find*` lookups; defined here as a value type only.
- `struct PlayerStatistics` — read-only tallies (all `std::uint32_t`/`std::uint64_t`, default 0): `connected`, `suspended`, `dead`, `respawns`, `deaths`, `reconnects`, `joinTick`, `averageSessionDurationTicks`. Tick-derived; populated by Step-12 diagnostics, not here.

Each value struct provides `constexpr bool operator==` / `operator!=` where equality is meaningful (identity/view structs), mirroring the Sprint-003/004/006 value-type precedent. `PlayerStatistics` and `PlayerRecord` need not define comparison operators (aggregate snapshots).

## 11. Type invariants

- All identity fields use the frozen "0 = none" convention (`PlayerId`, `ConnectionId`, `EntityId`).
- Enumerations have a fixed `std::uint8_t` layout and a fixed, additive enumerator order; the first enumerator is the natural initial value (`Joining` / `Connected`).
- Structures are POD-style aggregates: default member initializers only, no invariants requiring a constructor, no heap ownership, no pointers, no references, no virtuals — so they are trivially copyable and deterministic to construct.
- No field carries engine or platform state; player identity never doubles as a connection/session/network handle (architecture §11, invariant B11 spirit).
- `Name()` helpers are total: every enumerator maps to a stable, non-null `const char*`; an out-of-range value maps to a fixed `"?"`/`"Unknown"` sentinel (never undefined behavior).

## 12. Ownership rules

Step-01 introduces **types only**; it creates no owner and no instance lifetime. Per architecture §11: these types will later be owned by `player::PlayerRegistry` (records/views) and produced by `player::PlayerDiagnostics` (statistics); nothing in this step owns, allocates, or stores them. Reused types (`world::PlayerId/PlayerPosition/EntityId`, `net::ConnectionId/DisconnectReason`) remain owned by their originating subsystems and are referenced by value only — never redefined, never re-versioned.

## 13. Naming conventions

- Namespace `stalkermp::player`; header path `include/stalkermp/player/PlayerTypes.h`; include guard via `#pragma once` (project convention).
- Enumerations are `enum class` PascalCase with `std::uint8_t` base; enumerators PascalCase.
- Name helpers are free `constexpr` functions returning `const char*`, one per enumeration, following the Sprint-006 `NetTypes.h` precedent exactly: `PlayerLifecycleStateName`, `PlayerConnectionStateName`, `DisconnectDispositionName`, `JoinOutcomeName`, `SpawnOutcomeName`, `ReconnectOutcomeName`. (`const char*` literals only — no `std::string`, no iostream, ADR-007.)
- Structures PascalCase; fields camelCase with default member initializers.

## 14. Serialization constraints (if applicable)

Not applicable — and explicitly excluded. Step-01 defines **no** serialization: no `ByteWriter`/`ByteReader` use, no wire encoding, no `MessageId`, no packet layout. ADR-010 is untouched. The record is designed to be *reconstructable* for a future persistence sprint (architecture §17), but no durable-serialization field or routine is introduced here.

## 15. Determinism constraints

Fixed `std::uint8_t` enum layout and fixed enumerator order make the vocabulary deterministic across GCC and MSVC. All time-bearing fields (`joinTick`, `respawnEligibleTick`, `reconnectToken`, `PlayerPosition.lastUpdateTick`) are tick-derived integers — no wall-clock, no floating time. POD aggregates construct deterministically. `Name()` helpers are pure and order-independent. Nothing in this step introduces hashing, ordering, or address-dependent behavior (those belong to Step 3+).

## 16. Engine-boundary constraints

Engine-free. `PlayerTypes.h` includes no engine header, names no engine type, and holds no engine pointer. It depends only on engine-free value headers (`BubbleTypes.h`, `EntityView.h`, `NetTypes.h`). One Engine Boundary is preserved; the single engine-touching TU (`EnginePlayerSpawnGateway`) is Step 10, not here.

## 17. Platform-boundary constraints

OS-free. No `<winsock2.h>`/`<ws2tcpip.h>`, no socket/`sockaddr`/`SOCKET` type, no platform header. One Platform Boundary (ADR-009, Sprint-006) is untouched — this step adds nothing to any platform TU and the test build links no OS code for it.

## 18. Acceptance criteria

- `PlayerTypes.h` defines exactly the §7–§10 types with the §11 invariants and §13 names, and compiles engine-free / OS-free under the project's `-std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -Werror` (GCC) and `/std:c++17 /W4 /WX` (MSVC) settings, C4530-clean.
- `PlayerId` is a **reuse alias** of `world::PlayerId` (not a new struct); reused Sprint-003/004/006 types are referenced, not redefined.
- Every enumeration has an `std::uint8_t` base and a total `Name()` helper; no iostream, no `std::string`, no exceptions.
- The header introduces no logic, no owner, no serialization, no `MessageId`, and no networking/engine/platform contact.
- Both vcxprojs build with the new header and `PlayerTests.cpp`; the full existing suite still passes (238/238) with no regression and zero new warnings.

## 19. Test requirements

`tests/PlayerTests.cpp` (Step-01 portion only): trivial value-construction of each struct (defaults hold the "none"/initial values); `operator==`/`!=` on the identity/view structs; and a `Name()` totality check per enumeration (every enumerator returns a stable non-null string, and an out-of-range cast returns the sentinel). No lifecycle, registry, session, or gateway behavior is tested here (none exists yet). Tests are engine-free/OS-free and run in the existing `xrMP_Tests` executable on GCC + MSVC.

## 20. Completion checklist

- [ ] `include/stalkermp/player/PlayerTypes.h` created with the §7–§10 types, §9 enumerations (`std::uint8_t` base), §10 POD structures, and §13 `Name()` helpers.
- [ ] `PlayerId` defined as `using PlayerId = world::PlayerId;` (reuse, not redefine); all reused types referenced only.
- [ ] No logic, owner, serialization, `MessageId`, or networking/engine/platform contact in the header.
- [ ] `xrMP.vcxproj` (ClInclude) and `tests/xrMP_Tests.vcxproj` (ClCompile `PlayerTests.cpp`) updated.
- [ ] `PlayerTests.cpp` created with construction / `operator==` / `Name()`-totality checks only.
- [ ] Engine-free and OS-free confirmed (no engine/OS headers, no engine/platform types).
- [ ] ADR-007 clean (no exceptions/RTTI/iostream); deterministic fixed-layout enums.
- [ ] Full suite green on GCC + MSVC (238/238 baseline preserved), zero new warnings.

## 21. Stop condition

Stop when the value-type header and its minimal tests exist, compile engine-free/OS-free on both toolchains, and the full suite is green with no regression. Do **not** add configuration, registry, lifecycle, session, manager, position source, gateway, service/wiring, diagnostics, message registration, or any `.cpp` logic — those are Steps 2+. Do not begin Step 2.

---

## Specification Review (self-review)

- **Architecture compliance.** Every type traces to architecture §9 (component value shapes); ownership deferral matches §11; tick-derived fields match §13; the outcome vocabulary matches the failure surface of §18/§19. No type is invented beyond §9. Compliant.
- **ADR compliance.** ADR-007: engine-free, no exceptions/RTTI/iostream, `const char*` names, fixed-layout enums — conformant. ADR-009/010/011: untouched (no platform code, no wire/serialization, no threading) — conformant. No ADR modified.
- **Dependency correctness.** Depends only on already-frozen engine-free value headers (`BubbleTypes.h`, `EntityView.h`, `NetTypes.h`); reuses `world::PlayerId` rather than redefining it (matches the Implementation Plan Step 1 and architecture §9). No later step is depended upon. Correct.
- **Scope isolation.** Only value types/enums/POD structs/`Name()` helpers; every prohibited item (registry, config, lifecycle, services, managers, networking, engine adapters, Bootstrap, message registration, tick ordering, diagnostics, implementation code) is explicitly out of scope (§3) and absent from the deliverables. Isolated.
- **Determinism.** Fixed `std::uint8_t` enums, tick-derived integer time fields, pure `Name()`, POD construction — deterministic across toolchains; no hashing/ordering introduced. Sound.
- **Engine-boundary preservation.** No engine header/type/pointer; One Engine Boundary intact. Preserved.
- **Platform-boundary preservation.** No OS/socket header/type; One Platform Boundary intact. Preserved.
- **Ownership correctness.** Introduces no owner or instance; reused types stay owned by their subsystems; future ownership (Registry/Diagnostics) is correctly deferred. Correct.

**Issue found and resolved during review:** the architecture names the enumerations and structures (§9) but does not enumerate every enumerator or field. To remove ambiguity for implementation, this spec **fixes** the enumerator sets (§9) and record fields (§10) as the frozen Step-01 vocabulary, strictly within the failure/ownership surface the architecture already describes (§11/§17/§18/§19), and marks them additive-only for later steps. This adds no behavior and no new failure mode — it only makes the frozen §9 vocabulary concrete. No architecture or ADR change results. No other issue found.

---

## Step-01 Specification Freeze

The Sprint-007 Step-01 (Player Value Types) Specification is **FROZEN** as of 2026-07-11. It derives exclusively from the frozen Sprint-007 Architecture (§9/§11/§13/§17/§18/§19) and Implementation Plan (Step 1); it changes no architecture, no ADR, no implementation order, and no scope. The self-review passed on all eight dimensions and the single ambiguity (concrete enumerator/field sets) was resolved within the step by fixing the frozen §9 vocabulary additively. It preserves One Engine Boundary, One Platform Boundary, determinism, ADR-007, and all existing ownership.

This specification is ready for implementation. No implementation, verification, or Step-02 content is produced here — the specification phase for Step 01 ends at this freeze.

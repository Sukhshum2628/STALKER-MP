# Sprint-007 · Step 02 — Configuration — Implementation Specification

- **Status:** Step Specification (pre-implementation). Derives exclusively from the frozen `Multiplayer/docs/Sprint-007-Architecture.md` (§9 `PlayerConfiguration`, §6 NFR-5/NFR-6, §13 determinism, §19 error model) and the frozen `Documentation/SPRINTS/Sprint-007-Implementation-Plan.md` (Step 2). Introduces no architectural decision, no ADR change, no scope change, no order change.
- **Governance:** ADR-007 (engine-free, no exceptions, no RTTI, no iostream — `common::Format`). One Engine Boundary and One Platform Boundary preserved; ADR-009/010/011 untouched.
- **Nature:** Parse and validate the `[player]` configuration into a single engine-free `PlayerConfiguration` value. **Config value + `FromConfig` only.** No consumer wires it yet.

---

## 1. Step objective

Deliver `player::PlayerConfiguration` and its `FromConfig(const core::ConfigStore&) -> core::Expected<PlayerConfiguration>`, parsing the `[player]` section into a validated value with all durations stored as **tick counts** (never ms/wall-clock in control flow), following the Sprint-006 `TransportConfig`/`NetworkingConfig` `FromConfig` precedent exactly. This is the configuration every later Sprint-007 step consumes; it contains parsing/validation only, no lifecycle behavior.

## 2. Scope

In scope: create `include/stalkermp/player/PlayerConfiguration.h` (the `PlayerConfiguration` struct with default member initializers + the static `FromConfig` declaration) and `src/player/PlayerConfiguration.cpp` (the `FromConfig` definition: read `[player]`, apply defaults for missing keys, return `InvalidArgument` with a `common::Format` message for invalid values, enforce cross-field rules). Wire both into the two build projects. Extend `tests/PlayerTests.cpp` with default/override/invalid cases.

## 3. Explicit out-of-scope items

Not in this step (deferred per the Implementation Plan): registry storage/allocation (Step 3), lookups (Step 4), lifecycle logic (Step 5), session/queue (Step 6), `PlayerManager` (Step 7), position source (Step 8), gateway interface/null/adapter (Steps 9–10), service wiring / `tick_order` / Bootstrap / message registration (Step 11), diagnostics (Step 12), validation hardening (Step 13), integration/docs (Step 14). Also excluded: any consumer of the config (nothing reads it yet); any tick-to-ms *runtime* conversion (config stores ticks directly); any networking/engine/platform contact; any new value type beyond `PlayerConfiguration` (Step-01 owns the vocabulary).

## 4. Dependencies

Step-01 (`player::PlayerTypes.h`) for the namespace and any referenced enum/id (none strictly required by the config fields, but the header may include it for cohesion). Sprint-001 core: `core::ConfigStore` (`stalkermp/core/Config.h`), `core::Expected` (`stalkermp/core/Expected.h`), `core::MakeError`/`ErrorCode::InvalidArgument`, and `common::Format` (iostream-free formatting). No later Sprint-007 step is depended upon.

## 5. Files to create

- `include/stalkermp/player/PlayerConfiguration.h` — the `PlayerConfiguration` struct + `FromConfig` declaration, in `namespace stalkermp::player`.
- `src/player/PlayerConfiguration.cpp` — the `FromConfig` definition (parsing, defaults, validation, cross-field rules).

## 6. Files to modify

- `Multiplayer/xrMP.vcxproj` — add `src\player\PlayerConfiguration.cpp` (ClCompile) and `include\stalkermp\player\PlayerConfiguration.h` (ClInclude).
- `Multiplayer/tests/xrMP_Tests.vcxproj` — add `..\src\player\PlayerConfiguration.cpp` (ClCompile) so tests link it; extend `PlayerTests.cpp` (already in the project from Step-01).

No other file is modified. `Bootstrap.cpp`, `FrameDispatcher.h`, `EngineAdapters.cpp`, and all Sprint-001–006 headers/sources are untouched.

## 7. Public interfaces / types

In `namespace stalkermp::player`:

- `struct PlayerConfiguration` — a POD-style value with default member initializers (fields in §10) and one static method:
  - `[[nodiscard]] static core::Expected<PlayerConfiguration> FromConfig(const core::ConfigStore& config);`
- No other public type. No free functions beyond `FromConfig`.

## 8. Internal components

None beyond the `FromConfig` body. `FromConfig` may use file-local `static` parse helpers inside `PlayerConfiguration.cpp` (anonymous namespace) for reading/validating a single key, mirroring the Sprint-006 config `.cpp` style; these helpers are not exposed in the header.

## 9. Responsibilities

`FromConfig` reads the `[player]` section from the injected `ConfigStore`: for each key, use the parsed value if present and valid, the documented default if absent, and return `InvalidArgument` (with a precise `common::Format` message naming the key and the violated rule) if present-but-invalid. It converts any human-facing duration expressed in the config to a **tick count** at parse time (the config schema for Sprint-007 expresses durations directly in ticks to avoid a runtime ms→tick conversion; if a value is supplied out of range it is rejected). It enforces the cross-field rules of §11. It performs no side effects and touches no other subsystem.

## 10. Data structures

`struct PlayerConfiguration` (all fields with default member initializers; POD-style, trivially copyable):

- `std::uint32_t maxPlayers = 32;` — capacity of the player registry (Step 3 consumes this).
- `std::uint32_t respawnDelayTicks = 300;` — respawn delay in ticks (tick-derived; Step 5 respawn scheduling consumes this).
- `std::uint32_t reconnectRetentionTicks = 3600;` — how long a Suspended record is retained for reclaim, in ticks (Step 7 reconnect window).
- `std::uint32_t spawnPolicyId = 0;` — identifier selecting the deterministic spawn-location policy (Step 8/Step 10 consume this; `0` = default policy).

> These are exactly the four configuration inputs the frozen architecture §9 names for `PlayerConfiguration` ("capacity, respawn delay (ticks), reconnect-retention window (ticks), spawn-selection policy id"). No field is added beyond §9. Field names/types are fixed here as the frozen Step-02 shape.

## 11. Invariants

- Missing key → documented default (above); present-but-invalid → `InvalidArgument` with a `common::Format` message; no partial/side-effecting parse.
- **Cross-field / range rules (fixed here):** `maxPlayers >= 1` (a session must allow at least one player); `respawnDelayTicks` and `reconnectRetentionTicks` are non-negative by type and accepted across the full `std::uint32_t` range (0 is legal: immediate respawn / no retention); `spawnPolicyId` must be a known policy id — for Step-02 only `0` (default) is defined, so any non-zero value is **accepted and stored** but its policy resolution is a later-step concern (Step 02 does not reject unknown ids, to avoid coupling to Step 8/10; it validates type/range only). *(If the frozen spawn-policy set is later enumerated, that is an additive amendment, not a Step-02 change.)*
- All durations are tick counts; no wall-clock, no floating-point time.
- `FromConfig` is pure: same `ConfigStore` → same result, no observable side effect.

## 12. Ownership rules

Step-02 introduces a value type and a parse function only; it creates no owner and no long-lived instance. The composed `PlayerConfiguration` will later be owned by the `PlayerManagerService` (constructed in Step 11) and read by the manager; nothing in this step stores or owns it. `ConfigStore` remains owned by Sprint-001 and is consumed by const reference.

## 13. Naming conventions

Namespace `stalkermp::player`; header `include/stalkermp/player/PlayerConfiguration.h` with `#pragma once`; struct PascalCase; fields camelCase with default member initializers and explicit `Ticks` suffix on tick-valued durations (matching the project's tick-derived-timing convention); the static factory named `FromConfig` returning `core::Expected<PlayerConfiguration>` (exact Sprint-006 precedent). Error messages via `common::Format` (no iostream, no `std::string` streaming).

## 14. Constraints specific to the step

Config value + `FromConfig` only. No consumer, no service, no registry, no lifecycle, no engine/platform/networking contact, no new Step-01 vocabulary type. The `.cpp` may include `stalkermp/common/StringUtil.h` (for `common::Format`) and `stalkermp/core/Config.h`/`Expected.h` only. No `MessageId`, no serialization.

## 15. Determinism constraints

Parsing is deterministic and toolchain-independent: fixed defaults, fixed cross-field rules, integer tick durations, no locale/float dependence, no wall-clock. `FromConfig` yields identical results on GCC and MSVC for identical input. No hashing or ordering is introduced.

## 16. Engine-boundary constraints

Engine-free. Neither the header nor the `.cpp` includes any engine header, names any engine type, or holds any engine pointer. One Engine Boundary preserved.

## 17. Platform-boundary constraints

OS-free. No socket/platform header or type. One Platform Boundary (ADR-009) untouched; the test build links no OS code for this step.

## 18. Acceptance criteria

- `PlayerConfiguration` defines exactly the four §10 fields with §11 defaults/rules; `FromConfig` matches the Sprint-006 `FromConfig` contract (missing→default, invalid→`InvalidArgument` with a `common::Format` message, cross-field rules enforced).
- All durations are stored as ticks; no ms/wall-clock/float in the value or the parse.
- Engine-free and OS-free; ADR-007-clean (no exceptions/RTTI/iostream); compiles under GCC (`-std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -Werror`) and MSVC (`/std:c++17 /W4 /WX`), C4530-clean.
- Both vcxprojs updated; the full existing suite still passes (238/238) with no regression and zero new warnings.

## 19. Test requirements

Extend `tests/PlayerTests.cpp`: (a) **defaults** — a `ConfigStore` with no `[player]` section yields all documented defaults; (b) **override** — each field parses a valid supplied value; (c) **invalid** — an out-of-range/invalid value per field (e.g. `maxPlayers = 0`) yields `InvalidArgument` and no partial value; (d) **cross-field** — the `maxPlayers >= 1` rule is exercised. Tests are engine-free/OS-free and run in `xrMP_Tests` on GCC + MSVC. No lifecycle/registry behavior is tested (none exists yet).

## 20. Completion checklist

- [ ] `include/stalkermp/player/PlayerConfiguration.h` created (four §10 fields + `FromConfig` decl).
- [ ] `src/player/PlayerConfiguration.cpp` created (defaults, per-key validation, cross-field rules, `common::Format` messages).
- [ ] Durations stored as ticks; no wall-clock/float.
- [ ] `xrMP.vcxproj` and `tests/xrMP_Tests.vcxproj` updated (ClCompile/ClInclude).
- [ ] `PlayerTests.cpp` extended with default/override/invalid/cross-field cases.
- [ ] Engine-free and OS-free confirmed; ADR-007 clean; deterministic.
- [ ] Full suite green on GCC + MSVC (238/238 baseline preserved), zero new warnings.

## 21. Stop condition

Stop when `PlayerConfiguration` + `FromConfig` exist, parse/validate per §11, compile engine-free/OS-free on both toolchains, and the suite is green with no regression. Do **not** add any consumer, registry, lifecycle, service, or wiring — those are Steps 3+. Do not begin Step 3.

---

## 22. Self-review

- **Architecture compliance.** The four fields and their meanings are exactly architecture §9's `PlayerConfiguration` inputs; tick-only durations match §6 NFR-5 and §13; error handling matches §19. No field or behavior beyond §9. Compliant.
- **ADR compliance.** ADR-007: engine-free, no exceptions/RTTI/iostream, `Expected<T>`, `common::Format` — conformant. ADR-009/010/011 untouched. No ADR modified.
- **Dependency correctness.** Depends only on Step-01 and already-frozen Sprint-001 core (`ConfigStore`, `Expected`, `common::Format`); reuses the Sprint-006 `FromConfig` pattern; no later step depended upon. Correct.
- **Scope isolation.** Config value + `FromConfig` only; every prohibited item (registry, lifecycle, services, networking, engine/platform, message ids) is out of scope (§3) and absent from deliverables. Isolated.
- **Determinism.** Fixed defaults/rules, integer tick durations, no locale/float/wall-clock — deterministic across toolchains. Sound.
- **Engine-boundary preservation.** No engine header/type/pointer. Preserved.
- **Platform-boundary preservation.** No OS/socket header/type. Preserved.
- **Ownership correctness.** No owner/instance introduced; future ownership (`PlayerManagerService`, Step 11) correctly deferred; `ConfigStore` consumed by const ref. Correct.

**Issue found and resolved during review:** the architecture names `spawnPolicyId` but does not enumerate the valid policy set. To avoid coupling Step-02 to Step 8/10, this spec validates `spawnPolicyId` for **type/range only** and accepts unknown non-zero ids (storing them), deferring policy resolution to the consuming step; `0` is the defined default. This adds no behavior and no architecture change — it only fixes the Step-02 validation boundary. No other issue found.

---

## 23. Step-02 Specification Freeze

The Sprint-007 Step-02 (Configuration) Specification is **FROZEN** as of 2026-07-11. It derives exclusively from the frozen Sprint-007 Architecture (§9/§6/§13/§19) and Implementation Plan (Step 2); it changes no architecture, no ADR, no implementation order, and no scope. The self-review passed on all eight dimensions and the single ambiguity (`spawnPolicyId` validation boundary) was resolved within the step. It preserves One Engine Boundary, One Platform Boundary, determinism, ADR-007, and all existing ownership.

This specification is ready for implementation. No implementation, verification, or subsequent-step content is produced here. **Per the Documentation Rules, I am stopping after Step-02 and awaiting approval before documenting Step-03.**

# Sprint-007 · Step 03 — Registry Storage — Implementation Specification

- **Status:** Step Specification (pre-implementation). Derives exclusively from the frozen `Multiplayer/docs/Sprint-007-Architecture.md` (§9 `PlayerRegistry`, §11 ownership, §13 determinism) and the frozen `Documentation/SPRINTS/Sprint-007-Implementation-Plan.md` (Step 3). No architecture/ADR/scope/order change.
- **Governance:** ADR-007 (engine-free, no exceptions/RTTI/iostream, `Expected<T>`/value outcomes). One Engine Boundary and One Platform Boundary preserved.
- **Nature:** The canonical `PlayerRecord` store with `PlayerId` allocation and insert/retire — **storage only** (no secondary lookups, no lifecycle logic).

---

## 1. Step objective

Deliver `player::PlayerRegistry` storage: a capacity-bounded, sorted-by-`PlayerId` record store with strictly-ascending, non-reused `PlayerId` allocation and transactional insert/retire, following the Sprint-006 `ConnectionTable` sorted-vector + BC-005 precedent. Lookups (Step 4), lifecycle transitions (Step 5), and all consumers are excluded.

## 2. Scope

In scope: create `include/stalkermp/player/PlayerRegistry.h` and `src/player/PlayerRegistry.cpp` with: `PlayerId Allocate()`, `core::Expected<void> Insert(PlayerRecord)`, `Retire(PlayerId)`, `size()`/`capacity()`, and the ascending sorted `std::vector<PlayerRecord>` primary store keyed by `PlayerId`. Wire into both projects; extend `PlayerTests.cpp`.

## 3. Explicit out-of-scope items

Secondary lookups + accelerators + `ValidateConsistency` (Step 4); lifecycle transitions (Step 5); session/queue (Step 6); manager (Step 7); position source (Step 8); gateway (Steps 9–10); wiring/diagnostics/validation/integration (Steps 11–14). No engine/platform/networking contact; no `MessageId`; no lifecycle field mutation beyond storing the record as given.

## 4. Dependencies

Steps 01 (`PlayerTypes.h`: `PlayerRecord`, `PlayerId`) and 02 (`PlayerConfiguration.maxPlayers` for capacity). Sprint-001 core (`Expected`, `MakeError`, `ErrorCode::InvalidArgument`, `common::Format`). No later step depended upon.

## 5. Files to create

- `include/stalkermp/player/PlayerRegistry.h` — the storage class (namespace `stalkermp::player`).
- `src/player/PlayerRegistry.cpp` — allocation + insert/retire + ordering maintenance.

## 6. Files to modify

- `Multiplayer/xrMP.vcxproj` — add `src\player\PlayerRegistry.cpp` (ClCompile) + header (ClInclude).
- `Multiplayer/tests/xrMP_Tests.vcxproj` — add `..\src\player\PlayerRegistry.cpp` (ClCompile); extend `PlayerTests.cpp`.

No other file modified.

## 7. Public interfaces / types

In `namespace stalkermp::player`, class `PlayerRegistry`:

- `explicit PlayerRegistry(std::size_t capacity);`
- `[[nodiscard]] PlayerId Allocate();` — mint the next ascending, non-reused `PlayerId`.
- `[[nodiscard]] core::Expected<void> Insert(PlayerRecord record);` — insert keeping the vector sorted-unique by `PlayerId`; over-capacity or duplicate id → `InvalidArgument` (call site maps to a capacity/duplicate outcome — Sprint-006 convention).
- `void Retire(PlayerId id);` — remove the record if present (no error if absent).
- `[[nodiscard]] std::size_t size() const noexcept;` / `[[nodiscard]] std::size_t capacity() const noexcept;`
- `[[nodiscard]] const std::vector<PlayerRecord>& Records() const noexcept;` — ascending (read-only; the Step-4 lookups and Step-12 diagnostics consume this).

## 8. Internal components

The sorted `std::vector<PlayerRecord> m_records` (ascending by `PlayerId`); a monotonic `std::uint32_t m_nextId` counter (starts at 1; `0` reserved "none"); `m_capacity`. No hash accelerators yet (Step 4). File-local binary-search helper for the sorted insert/find-by-primary-key.

## 9. Responsibilities

Allocate ascending non-reused ids; maintain the ascending sorted-unique invariant on insert; enforce capacity; retire by id; expose read-only ascending access. No lifecycle interpretation, no secondary indexing, no validation report (Step 4).

## 10. Data structures

Reuses `PlayerRecord` (Step-01). Internal: `std::vector<PlayerRecord>` (ascending), `std::uint32_t m_nextId`, `std::size_t m_capacity`. No new value type.

## 11. Invariants

`PlayerId` strictly ascending and never reused (even after retire); `m_records` sorted-unique by `PlayerId`; `size() <= capacity()`; over-capacity insert rejected as a value outcome (no partial insert); `Allocate()` never returns `0`.

## 12. Ownership rules

`PlayerRegistry` owns the record store (architecture §11: mapping+lifecycle owned here). It does not own the entity (Entity Registry), session (Sprint-006), or ALife status (engine). Records are held by value. Nothing external owns the registry yet (the manager owns it from Step 7).

## 13. Naming conventions

Namespace `stalkermp::player`; class `PlayerRegistry`; methods PascalCase; members `m_` prefix; `#pragma once`. Value outcomes via `core::Expected`; messages via `common::Format`.

## 14. Step-specific constraints

Storage only. No `Find*`, no accelerators, no `ValidateConsistency`, no lifecycle. Engine-free/OS-free; no `MessageId`; no networking.

## 15. Determinism constraints

Ascending id allocation and deterministic sorted insertion order make storage replay-stable; identical operation sequences → identical vector state. No wall-clock, no address-ordering.

## 16. Engine-boundary constraints

Engine-free: no engine header/type/pointer. One Engine Boundary preserved.

## 17. Platform-boundary constraints

OS-free: no socket/platform header/type. One Platform Boundary preserved.

## 18. Acceptance criteria

Allocation ascending/non-reused; sorted-unique maintained; capacity enforced (reject with outcome, no partial state); retire removes without error when absent; read-only ascending access exposed; engine/OS-free; ADR-007-clean; GCC + MSVC green; 238/238 preserved, zero new warnings.

## 19. Test requirements

Allocation monotonicity + non-reuse across retire; insert keeps ordering; duplicate-id insert rejected; capacity-exceeded rejected with no state change; retire-absent is a no-op; `Records()` ascending. Engine/OS-free in `xrMP_Tests`.

## 20. Completion checklist

- [ ] `PlayerRegistry.h`/`.cpp` created (Allocate/Insert/Retire/size/capacity/Records).
- [ ] Ascending non-reused ids; sorted-unique store; capacity enforced.
- [ ] vcxprojs updated; `PlayerTests.cpp` extended.
- [ ] Engine/OS-free; ADR-007 clean; deterministic.
- [ ] 238/238 green on GCC + MSVC; zero new warnings.

## 21. Stop condition

Stop when records store/retire by `PlayerId` with the §11 invariants and the suite is green. Do not add lookups/lifecycle/consumers. Do not begin Step 4.

## 22. Self-review

- **Architecture compliance.** Matches §9 `PlayerRegistry` (sorted-vector + accelerator, BC-005) — this step delivers the sorted-vector half; accelerators are Step 4 per the plan. Ownership per §11. Compliant.
- **ADR compliance.** ADR-007 clean; ADR-009/010/011 untouched.
- **Dependency correctness.** Only Steps 01–02 + Sprint-001 core. Correct.
- **Scope isolation.** Storage only; lookups/lifecycle excluded (§3). Isolated.
- **Determinism.** Ascending ids, deterministic order. Sound.
- **Engine/Platform boundary.** No engine/OS contact. Preserved.
- **Ownership.** Registry owns records; no external owner yet. Correct.
- *Ambiguity resolved:* capacity/duplicate rejection reuses `InvalidArgument` (no dedicated `ErrorCode`), the established Sprint-006 convention — no new error type. No other issue.

## 23. Step-03 Specification Freeze

Sprint-007 Step-03 (Registry Storage) Specification is **FROZEN** (2026-07-11). Derived exclusively from the frozen architecture (§9/§11/§13) and plan (Step 3); no architecture/ADR/order/scope change. Self-review passed; ambiguity resolved within the step. One Engine/Platform Boundary, determinism, ADR-007, and existing ownership preserved. Ready for implementation.

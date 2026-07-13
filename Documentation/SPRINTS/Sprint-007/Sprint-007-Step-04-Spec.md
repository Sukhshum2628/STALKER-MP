# Sprint-007 · Step 04 — Lookup APIs — Implementation Specification

- **Status:** Step Specification (pre-implementation). Derives exclusively from the frozen Architecture (§9/§10 `Find*`, §20 consistency) and Implementation Plan (Step 4). No architecture/ADR/scope/order change.
- **Governance:** ADR-007. One Engine Boundary / One Platform Boundary preserved.
- **Nature:** The four secondary lookups over the registry + hash accelerators + storage-level `ValidateConsistency`. Read-only; no lifecycle.

---

## 1. Step objective

Add the four read-only secondary lookups (`FindByPlayerId`, `FindBySession`, `FindByEntity`, `FindByConnection`) with hash accelerators keyed to the Step-3 ascending vector (BC-005), plus a storage-level `ValidateConsistency` proving accelerator↔vector agreement and the mapping bijection.

## 2. Scope

In scope: extend `PlayerRegistry.h`/`.cpp` with the four `Find*` (returning `optional` const views/pointers), the accelerator maps, accelerator maintenance on insert/retire, and `ValidateConsistency`. Extend `PlayerTests.cpp`.

## 3. Explicit out-of-scope items

Lifecycle transitions (Step 5); session/queue (Step 6); manager (Step 7); position source (Step 8); gateway (Steps 9–10); wiring/diagnostics/validation-hardening/integration (Steps 11–14). No mutation API beyond maintaining accelerators; no engine/platform/networking; no `MessageId`.

## 4. Dependencies

Step 03 (storage) and Steps 01–02. Reused id types: `net::ConnectionId`, `world::EntityId`, session-member key (`std::uint64_t`). Sprint-001 core.

## 5. Files to create

None. (Extends Step-3 files.)

## 6. Files to modify

- `include/stalkermp/player/PlayerRegistry.h`, `src/player/PlayerRegistry.cpp` — add `Find*`, accelerators, `ValidateConsistency`.
- `tests/PlayerTests.cpp` — hit/miss + consistency tests. (vcxprojs already include these from Step 3.)

## 7. Public interfaces / types

- `[[nodiscard]] std::optional<PlayerMappingView> FindByPlayerId(PlayerId) const;`
- `[[nodiscard]] std::optional<PlayerMappingView> FindByConnection(net::ConnectionId) const;`
- `[[nodiscard]] std::optional<PlayerMappingView> FindBySession(std::uint64_t sessionMember) const;`
- `[[nodiscard]] std::optional<PlayerMappingView> FindByEntity(world::EntityId) const;`
- `[[nodiscard]] PlayerRegistryConsistency ValidateConsistency() const;` — a read-only report struct (fields in §10) with `IsHealthy()`, mirroring the Sprint-006 `*Consistency` precedent.

## 8. Internal components

`std::unordered_map` accelerators: `PlayerId→index`, `ConnectionId→index`, `sessionMember→index`, `EntityId→index` (index into the ascending vector). File-local `RebuildAccelerators()` and incremental update on insert/retire.

## 9. Responsibilities

Resolve each secondary key to a `PlayerMappingView` via its accelerator in O(1) amortized; keep accelerators consistent with the sorted vector on every mutation; report consistency (ordering, accelerator agreement, mapping bijection where required — a live `PlayerId` maps to at most one `ConnectionId`/`EntityId`/session member and vice versa, treating `0`/none as unmapped).

## 10. Data structures

`struct PlayerRegistryConsistency { bool sortedUnique = true; bool acceleratorsConsistent = true; bool mappingBijective = true; bool withinCapacity = true; bool noZeroId = true; [[nodiscard]] bool IsHealthy() const noexcept { return sortedUnique && acceleratorsConsistent && mappingBijective && withinCapacity && noZeroId; } };` Reuses `PlayerMappingView` (Step-01).

## 11. Invariants

Accelerators always consistent with `m_records` after any mutation; lookups never mutate; a key resolves to at most one record; `0`/none keys are never inserted into accelerators (unmapped); `ValidateConsistency` detects a deliberately corrupted accelerator or a broken bijection.

## 12. Ownership rules

Accelerators are owned by `PlayerRegistry` (internal); views are non-owning projections returned by value. No external ownership change.

## 13. Naming conventions

`Find*` PascalCase; `ValidateConsistency`/`PlayerRegistryConsistency`/`IsHealthy` mirror Sprint-006; `#pragma once`; members `m_`.

## 14. Step-specific constraints

Read-only lookups + accelerator maintenance + consistency only. No new mutation semantics (insert/retire from Step 3 are extended only to update accelerators). Engine/OS-free; no `MessageId`.

## 15. Determinism constraints

Lookups are side-effect-free and order-independent; results depend only on `PlayerId` value keys, never on hash iteration order (accelerators map to the ascending vector, which is the ordered source of truth). Replay-stable.

## 16. Engine-boundary constraints

Engine-free.

## 17. Platform-boundary constraints

OS-free.

## 18. Acceptance criteria

All four lookups resolve hit/miss correctly; accelerators stay consistent across insert/retire; `ValidateConsistency` catches an injected accelerator corruption and a broken bijection; engine/OS-free; ADR-007-clean; 238/238 green; zero new warnings.

## 19. Test requirements

Hit + miss per key; accelerator consistency after insert/retire churn; negative tests injecting a corrupted accelerator / duplicate mapping → `IsHealthy()` false; ascending `Records()` unaffected.

## 20. Completion checklist

- [ ] Four `Find*` added; accelerators maintained on insert/retire.
- [ ] `ValidateConsistency` + `PlayerRegistryConsistency` added.
- [ ] Bijection + ordering + accelerator checks covered by tests (incl. negatives).
- [ ] Engine/OS-free; ADR-007 clean; deterministic.
- [ ] 238/238 green on GCC + MSVC; zero new warnings.

## 21. Stop condition

Stop when lookups resolve and `ValidateConsistency` validates. No lifecycle. Do not begin Step 5.

## 22. Self-review

- **Architecture compliance.** Matches §9/§10 `Find*` and §20 consistency. Compliant.
- **ADR compliance.** ADR-007 clean; others untouched.
- **Dependency correctness.** Steps 01–03 + core only. Correct.
- **Scope isolation.** Read-only lookups; lifecycle excluded. Isolated.
- **Determinism.** Value-keyed, order-independent. Sound.
- **Engine/Platform boundary.** No contact. Preserved.
- **Ownership.** Accelerators internal; views non-owning. Correct.
- *Ambiguity resolved:* the "bijection where required" is defined precisely (live id ↔ at most one secondary key; none/`0` unmapped) so the consistency check is unambiguous — consistent with §20. No other issue.

## 23. Step-04 Specification Freeze

Sprint-007 Step-04 (Lookup APIs) Specification is **FROZEN** (2026-07-11). Derived exclusively from the frozen architecture (§9/§10/§20) and plan (Step 4); no architecture/ADR/order/scope change. Self-review passed; ambiguity resolved within the step. Boundaries, determinism, ADR-007, ownership preserved. Ready for implementation.

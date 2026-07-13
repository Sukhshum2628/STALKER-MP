# Sprint-003 — Entity Registry Refactor — Design Review (FROZEN)

- **Status:** Frozen. Architecturally approved via the Final Architectural Gate.
- **Date:** 2026-07-06
- **Governing ADR:** ADR-007 — Engine ABI Contract
- **Specification:** `Documentation/SPRINTS/Sprint-003-Entity-Registry-Refactor.md` (Architecturally Frozen)
- **Author role:** Principal Architect / Architecture Review Board.

This document is the frozen record of the Sprint-003 architectural review. It captures
the accepted decisions, the reasoning, the rejected alternatives, and the two
pre-implementation evidence gates. It is authoritative for Sprint-003 implementation and
subordinate only to ADR-007 and the architecture documents.

---

## 1. Summary

Sprint-003 establishes the authoritative Entity Registry as the canonical record of
**identity and registration** for every persistent entity, seated behind the existing
Sprint-002 `IEntitySource` seam. The registry owns records, not engine object lifetime;
the engine remains the mechanical owner of `CObject`/`CSE_Abstract`, observed only in
`EngineAdapters.cpp`. The design was approved conditional on ten frozen invariants (§2)
and two pre-implementation evidence gates (§4).

---

## 2. Accepted architectural decisions (frozen invariants)

| ID | Invariant | Rationale / source |
|----|-----------|--------------------|
| I1 | Registry owns identity + registration records, **not** engine object lifetime; deregistration mirrors engine destroy | `04_World.md §8`; Sprint-005 owns spawn/destroy; Preserve Before Replace |
| I2 | Engine spawn/destroy observed **only** in `EngineAdapters.cpp`, which feeds the registry; registry is engine-agnostic | One Engine Boundary; keeps sole engine-header TU; enables engine-free unit tests |
| I3 | Canonical iteration order = ascending `EntityId`; hash structures are secondary indices only | RFC-0001 determinism; reproducible replication/persistence baseline |
| I4 | Persistent `EntityId` distinct from runtime `EntityHandle{EntityId, generation}`; generation never persisted | Sprint-008 wire id, Sprint-011 persistence; `04_World.md §12.4` |
| I5 | Registration-lifecycle events only; activation/deactivation deferred | Online/offline owned by Sprint-004/005 (`05_ALife.md §12.3`) |
| I6 | Registry exposes no spatial query; `ISpatialQueries` owns spatial iteration | Sprint-002 already owns spatial; avoid duplicate responsibility |
| I7 | Statistics by entity-category metadata only; no session/connection state | WorldContext Player Count Leak lesson (AI_CONTEXT.md) |
| I8 | Single-writer: registry accessed on host simulation thread only; cross-thread via RFC-0003 snapshots | RFC-0003 / RFC-0006 |
| I9 | Events non-fallible, deterministic order, non-reentrant | ADR-007 invariant 6; determinism |
| I10 | All registry TUs conform to ADR-007 (C4530-clean); no engine containers outside the adapter | ADR-007 Engine ABI Contract |

---

## 3. Rejected / deferred alternatives

- **Registry owning engine object lifetime/destruction** — rejected: collides with engine
  ALife/object-factory ownership, violates Preserve Before Replace and One Engine
  Boundary, and steals Sprint-005 scope.
- **Hash-map primary store defining iteration order** — rejected: non-deterministic
  (rehash-dependent) order violates I3/RFC-0001.
- **`ForEachNearby` on the registry** — deferred to `ISpatialQueries` (I6).
- **Activation/deactivation events in Sprint-003** — deferred to Sprint-004/005 (I5).
- **`RestoreEntity` coupled to `ISerializable`** — rejected: `ISerializable` is provisional
  (D-006); `RestoreEntity` is format-agnostic (register a pre-assigned identity).
- **Engine `xr_hash_map` / `robin_hood` in the registry** — rejected outside
  `EngineAdapters.cpp`: violates One Engine Boundary and the shared test build.
- **Presuming `std::unordered_map` non-conforming** — rejected: conformance is empirical
  (see Gate 1); BC-004 shows STL conformance under `/EH`-off is case-by-case.

---

## 4. Pre-implementation evidence gates (implementation gates, not architectural)

Both gates select internal mechanisms encapsulated behind the frozen contract, and each
has a proven fallback within this architecture. Neither can reopen the specification.

**Gate 1 — `std::unordered_map` ABI probe.** Evidence gathering. Compile a throwaway TU
under `EngineAbi.props` instantiating the intended associative structures; if C4530-clean
use `std::`, else a conforming fallback (sorted `std::vector` + binary search, or an
in-module open-addressing map). Canonical order (I3) is unaffected either way. Result
recorded as a BC-note.

**Gate 2 — Engine feed mechanism.** Evidence gathering + implementation choice. Determine
whether the engine exposes a spawn/destroy hook; otherwise adopt tick reconciliation of
the engine object list (already proven feasible with zero engine edits by Sprint-002's
`EngineEntitySource` enumeration via `FrameDispatcher`). Either outcome keeps I2. Decision
recorded as an implementation note.

Escalation clause: only if a gate revealed that **no** solution exists within the frozen
constraints (e.g. observation impossible without an engine edit) would it escalate to a
new ADR. The proven fallbacks foreclose this.

---

## 5. Composition-root and lifetime

The registry is a `ServiceRegistry`-owned service, created in `Bootstrap.cpp` and injected
as `world::IEntitySource` into `WorldQueryService` (replacing the direct
`EngineEntitySource` injection). Initialization order: registry → `WorldQueryService` →
`EnvironmentService`; shutdown strictly reverse. The `EngineAdapters.cpp` feed starts after
the registry exists and stops before it is destroyed (no dangling feed — cf. the
`EngineFrameBridge` lifetime rule).

---

## 6. Testing strategy

The registry is engine-agnostic and unit-tested in `xrMP_Tests` using the
`NullEngineAdapters` feed pattern: registration, deregistration, duplicate-id rejection,
handle generation/validation, deterministic iteration (ascending `EntityId`), lookup
correctness, registration-lifecycle transitions, event ordering/non-reentrancy,
metadata-only statistics, and large-count stress tests.

---

## 7. Gate outcome

**Conditionally Approved → Architecturally Frozen.** The architecture is complete and
frozen under the ten invariants above. The two evidence gates are pre-implementation
steps that resolve internal mechanisms and report back (Gate 1 as a BC-note, Gate 2 as an
implementation note) before registry code is written. No further design iteration is
required.

---

## 8. Evidence-gate resolutions (implementation notes)

Both pre-implementation gates were resolved before and during registry implementation; the
outcomes select internal mechanisms only and did not reopen the architecture.

### Gate 1 — `std::unordered_map` ABI probe — RESOLVED
A standalone TU compiled under `EngineAbi.props` (exceptions disabled, `/WX`) exercised
`std::unordered_map`/`std::unordered_set` (insert/erase/reserve/rehash/lookup/iteration):
clean, no C4530/C2220. `std::` containers are conformant and approved as the registry's
secondary indices; the sorted `std::vector` remains the canonical ascending-`EntityId`
store (I3). Recorded as **BC-005** in `Documentation/Engine_Integration/Build_Compatibility.md`.

### Gate 2 — engine feed mechanism — RESOLVED
The engine exposes both required observation points with **zero engine source edits**:
- **Creation:** per-frame reconciliation of `g_pGameLevel->Objects` (already enumerated by
  Sprint-002's `EngineEntitySource`), driven via the existing `FrameDispatcher` at
  `tick_order::kEntityRegistry`.
- **Destruction:** the engine-native `relcase` notification
  (`CObjectList::relcase_register`/`relcase_unregister`), registered from
  `EngineAdapters.cpp` and bound to level lifetime (C4).

Adopted mechanism: reconciliation for creation + `relcase` for destruction, all confined to
`EngineAdapters.cpp` (One Engine Boundary), honoring C1–C4. Implemented in Steps 10–11.

### Note on the `IEntitySource` re-wiring
Design Review §5 anticipated the registry replacing the direct `EngineEntitySource`
injection into `WorldQueryService`. That re-wiring was intentionally **not** performed in
Sprint-003: `IEntitySource` returns live entity **position**, which is transient simulation
state the registry deliberately does not store (identity/metadata only, §7.10 / I1). The
registry is populated and queryable in its own right; unifying the `IEntitySource` view
(registry identity + live engine position) is deferred to a later sprint where position
snapshotting is defined. This does not affect any Sprint-003 acceptance criterion.

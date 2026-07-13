# E-G1 — Evidence Spike: Vanilla ALife Switch Entry Point & Contention

**Gate:** E-G1 (sole remaining **blocking** evidence gate before Sprint-005 Architecture Freeze)
**Type:** Engine Investigation / Evidence Spike (read-only tracing + configuration probing; **no ALife source modification**)
**Status:** Specified — not yet executed
**Owner of execution:** Antigravity (Windows / MSVC / live engine)
**Consumes:** Sprint-005 Architecture §19 (ADR-008), §21 (E-G2 resolved)
**Produces:** the frozen selection of the production switching path, behind the single `IAlifeSwitchGateway` seam

> This spike does **not** redesign Sprint-005 and does **not** author implementation. Its entire outcome resolves behind the single gateway (`adapters::EngineAlifeSwitchGateway`), and therefore **cannot reopen the architecture**. It selects (a) the engine call the gateway makes and (b) the reconciliation strategy against vanilla distance switching.

---

## 1. Purpose

Determine, with runtime evidence, **which existing vanilla engine mechanism the gateway should use to drive an entity online/offline**, and **whether Bubble-driven switching contends with the engine's vanilla distance-based switching** — including flip-flop — and whether any **engine configuration** (not source change) removes contention.

The architecture already mandates (E-G2, closed): on-demand `EntityId → CSE_ALifeDynamicObject` resolution via the engine's authoritative lookup, no retained pointers, no gateway cache, preference for the id-based API where appropriate. E-G1 decides the *call* made after that resolution.

---

## 2. Background — what the code already tells us (pre-spike facts)

These are **high-confidence facts** established by static reading of the engine; the spike is to *confirm them at runtime* and measure the behaviours that static reading cannot show.

**F1 — The vanilla driver is single-actor-keyed.**
`CALifeUpdateManager::update` schedules nearby objects and, per object, calls `CALifeSwitchManager::switch_object(I)` (via `CSwitchPredicate`, `alife_update_manager.cpp:55`). `switch_object` dispatches to `try_switch_offline` if `I->m_bOnline`, else `try_switch_online` (`alife_switch_manager.cpp:229`).

**F2 — The distance gate is measured to a single actor.**
`CSE_ALifeDynamicObject::try_switch_online()` (`alife_dynamic_object.cpp:133`) gates online on
`alife().graph().actor()->o_Position.distance_to(o_Position) > alife().online_distance()`.
`online_distance = switch_distance * (1 - switch_factor)`, `offline_distance = switch_distance * (1 + switch_factor)` (`alife_switch_manager_inline.h`). This is the hysteresis band that our own Bubble hysteresis must be reconciled against.

**F3 — Two override flags already exist and are honoured by the vanilla path.**
`try_switch_online()` early-returns online-forced when `!can_switch_offline()`, and refuses online when `!can_switch_online()` (calls `on_failed_switch_online()`). The id-based API sets exactly these flags:
`CALifeUpdateManager::set_switch_online(id, bool)` → `object->can_switch_online(value)`;
`set_switch_offline(id, bool)` → `object->can_switch_offline(value)` (`alife_update_manager.cpp:333`). Both resolve `id → object` via `objects().object(id)` — the engine's authoritative lookup (satisfies E-G2).

**F4 — Direct calls bypass the flags and the distance gate.**
`CALifeSwitchManager::switch_online/switch_offline` perform the state change immediately; `try_switch_*` apply the guard logic first.

**Working hypothesis to be tested (H0):** the **id-flag API** (`set_switch_online`/`set_switch_offline`) is the correct production path because it *overrides the predicates the vanilla loop already evaluates* rather than racing the vanilla loop's own `switch_object` calls — making Bubble-driven pinning cooperative with vanilla rather than contending. Direct `switch_online/offline` is the fallback if flag semantics prove insufficient. This spike must confirm or refute H0.

---

## 3. Candidate mechanisms under evaluation

| # | Mechanism | Nature | Primary risk to prove/disprove |
|---|-----------|--------|--------------------------------|
| C1 | `CALifeUpdateManager::set_switch_online(id,true)` / `set_switch_offline(id,false)` to **pin online**; inverse to **release** | Sets `can_switch_*` override flags; actual switch still performed by vanilla loop | Does a pinned object reliably go/stay online without the actor being near? Does releasing return it cleanly to vanilla control? |
| C2 | `CALifeSwitchManager::try_switch_online/try_switch_offline(object)` | Applies vanilla guards (incl. single-actor distance gate) | Distance gate to single actor will **reject** online for objects far from the actor but inside a remote player's bubble |
| C3 | `CALifeSwitchManager::switch_online/switch_offline(object)` | Immediate, unconditional | Next vanilla `switch_object` on that object may **reverse** it → flip-flop |
| C4 | Configuration-only: `switch_distance` / `switch_factor` (via `set_switch_distance`/`set_switch_factor`) | Widen vanilla band so bubble ⊆ vanilla-online region | May force excessive online set / perf blowup; still single-actor-centred, so remote-player bubbles remain unserved |
| C5 | Another existing mechanism discovered during tracing (e.g. group `try_switch_*`, `add_online`/`remove_online`) | TBD | Documented if found; else explicitly ruled out |

C4 alone cannot serve **remote** players (band is centred on the one actor), so it is evaluated only as an adjunct to C1/C3, never standalone.

---

## 4. Conflict & flip-flop model to validate

Contention exists if, within the same object, **two authorities issue opposing switch intents across consecutive update cycles**:

- Authority A (vanilla): `switch_object` driven by actor distance every schedule cycle.
- Authority B (STALKER-MP): gateway call driven by Bubble membership every `kAlifeTransition` tick (order 350).

**Flip-flop signature:** an object's `m_bOnline` toggles on successive cycles with no change in Bubble membership — i.e. B sets online, A (or A's distance gate) reverts to offline, B re-sets, ad infinitum. The spike must determine whether each candidate produces this signature, and whether C1's flag-override design suppresses it by construction (because A's `try_switch_*` reads B's flags rather than opposing them).

---

## 5. Engine files to investigate

Read-only tracing scope (no edits):

- `xrGame/alife_update_manager.cpp` — `update`, `CSwitchPredicate`, `set_switch_online/offline` (lines ~33–56, ~333–345).
- `xrGame/alife_update_manager.h` — id-flag API declarations (~55–60).
- `xrGame/alife_switch_manager.cpp` — `switch_object`, `try_switch_online/offline`, `switch_online/offline`, `add_online/remove_online` (~110–250).
- `xrGame/alife_switch_manager.h` / `alife_switch_manager_inline.h` — distance/factor math.
- `xrGame/alife_dynamic_object.cpp` — `CSE_ALifeDynamicObject::try_switch_online/offline`, `can_switch_online/offline`, `redundant`, `synchronize_location` (~55–200).
- `xrGame/alife_object_registry.*` — `objects().object(id)` authoritative lookup (E-G2 path).
- `xrGame/alife_graph_registry.*` — `graph().actor()` (the single-actor reference in F2).
- `xrGame/alife_online_offline_group.cpp`, `alife_group_abstract.cpp` — group override paths (C5 discovery).
- Config: the ALife section consumed in `CALifeSwitchManager` ctor (`switch_distance`, `switch_factor`) — locate the live `system.ltx`/`alife` section values used at runtime.

## 6. Functions to trace

`CALifeUpdateManager::update` → `CSwitchPredicate::operator()` → `CALifeSwitchManager::switch_object` → `try_switch_online` / `try_switch_offline` → `CSE_ALifeDynamicObject::try_switch_online/offline` → `alife().switch_online/offline`. In parallel: `set_switch_online/offline` → `can_switch_online/offline`. Lookup: `objects().object(id)`. Reference position: `graph().actor()->o_Position`.

## 7. Breakpoints / logging points

Prefer **non-intrusive logging** at these points (temporary `Msg(...)` in a *scratch* build only, or debugger breakpoints — never committed to ALife source):

- BP-1 `switch_object` entry: log `id`, `m_bOnline`, `can_switch_online()`, `can_switch_offline()`, distance-to-actor, cycle.
- BP-2 `try_switch_online` distance-gate branch: log whether the `> online_distance()` reject fired.
- BP-3 `CSE_ALifeDynamicObject::try_switch_online/offline` exit: log resulting `m_bOnline`.
- BP-4 `set_switch_online/offline`: log `id`, `value`, resolved object non-null.
- BP-5 gateway call site (STALKER-MP side, `EngineAlifeSwitchGateway`): log intended transition per `EntityId` per tick.
- Derived trace: per `EntityId`, a time series of `{cycle, source∈{vanilla,stalkermp}, intent, m_bOnline}` — the flip-flop detector.

## 8. Experiment matrix (observable runtime scenarios)

Each run: bubble of known radius around a **non-actor** position (to expose the single-actor assumption), plus a set of ALife objects straddling the bubble boundary and the vanilla band.

- S1 **Single actor, object inside bubble & inside vanilla band** — baseline agreement.
- S2 **Object inside bubble, outside vanilla band (far from actor)** — the decisive case: C2 must fail here, C1/C3 must succeed. Confirms remote-player service.
- S3 **Object inside vanilla band, outside bubble** — should vanilla keep it online? Define whether STALKER-MP suppresses via `set_switch_offline`/`can_switch_online(false)` or defers to vanilla.
- S4 **Boundary oscillation** — object parked on the bubble edge across many cycles → measure toggles/N cycles (flip-flop rate) for each candidate.
- S5 **Release** — pin an object online via C1, then drop it from the bubble → confirm it returns to vanilla control and eventually offlines by vanilla rule (no leak, no stuck-online).
- S6 **Attached/parented object** (`ID_Parent != 0xffff`) and **redundant()** object — confirm candidate honours vanilla parent/redundancy guards (must not force-online an attached child).
- S7 **Config adjunct (C4)** — repeat S2 with widened `switch_factor`; measure online-set size and whether remote bubble is still unserved (expected: yes, unserved).

## 9. Evidence to collect

For each candidate × scenario: the BP-1..BP-5 trace; per-object online/offline time series; flip-flop count over a fixed cycle window; count of objects correctly online for a remote (non-actor) bubble; any triggered `VERIFY`/`on_failed_switch_online`; frame-time delta of the switch phase; and the resulting engine config values in effect. Store as a tabulated appendix in this document.

---

## 10. Success criteria

The spike **succeeds** when all hold for at least one candidate (expected C1):

1. **Remote service:** objects inside a bubble centred away from the single actor are driven online reliably (S2 passes).
2. **No flip-flop:** flip-flop count = 0 across the S4 window with stable membership (band vs bubble reconciled).
3. **Clean release:** S5 returns objects to vanilla control with no stuck-online and no leaked overrides.
4. **Vanilla guards intact:** S6 shows parent/redundant/attached rules are never violated (Preserve Before Replace).
5. **Determinism:** identical inputs → identical online/offline sets and ordering across repeated runs.
6. **Boundary compliance:** the chosen call reaches the engine only via `objects().object(id)` on-demand resolution (E-G2) and sits entirely inside `EngineAlifeSwitchGateway` (One Engine Boundary).
7. **No ALife source modification** was required to achieve 1–6 (config-only adjustments permitted).

## 11. Failure criteria

The spike **fails** (and escalates) if any of:

- **F-a** No candidate serves remote (non-actor) bubbles without direct `switch_online/offline` **and** direct calls produce persistent flip-flop that no configuration suppresses.
- **F-b** The only way to stop contention is to alter `CALifeUpdateManager::update` / `switch_object` / distance-gate **source** (violates "never modify engine source").
- **F-c** A candidate violates vanilla parent/redundant guards or triggers `VERIFY` failures.
- **F-d** Behaviour is non-deterministic across identical runs.
- **F-e** Correct behaviour requires the gateway to retain object pointers or a cache (would reopen E-G2 — not permitted).

On any failure, record which invariant broke and route to an architecture decision (not a code patch) before freeze.

## 12. Final decision criteria — selecting the production switching path

Rank and select by this order (first candidate meeting §10 wins):

1. **C1 (id-flag override)** — preferred if it passes §10. Rationale: cooperates with, rather than races, the vanilla loop (F3); uses the id-based authoritative lookup (E-G2-aligned); no direct state mutation contention. If chosen, ADR-008 records the gateway as writing **override flags**, with the vanilla scheduler performing the actual switch.
2. **C3 + C4 (direct switch, band widened to eliminate reversal)** — fallback if C1 cannot force the desired state. Requires demonstrating flip-flop = 0 via configuration only. ADR-008 records direct mutation with a reconciliation note.
3. **C5 (discovered mechanism)** — only if it strictly dominates C1/C3 on §10.
4. **C2** — rejected for production (fails S2 by construction) except as an *optional* optimisation when the object is already within the actor band.

The winning candidate is recorded as the single call made by `EngineAlifeSwitchGateway`; the reconciliation strategy against vanilla distance switching is recorded alongside it. Both live behind the gateway seam and are the inputs ADR-008 formalises. **This closes E-G1 and clears the last blocking gate for Sprint-005 Architecture Freeze** (with ADR-008 authored as the remaining non-gate prerequisite; E-G3 remains non-blocking tuning).

---

## 13. Constraints (restated)

Read-only/scratch-build tracing only; no committed change to `xrGame` ALife source; engine access confined conceptually to the gateway; on-demand id→object resolution with no retained pointers or cache (E-G2); Expected<T> at the STALKER-MP boundary; deterministic ordering by ascending EntityId. The spike's outcome selects a call and a config, both behind one seam — it cannot alter the Sprint-005 architecture.

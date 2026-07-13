# STALKER-MP Build Model — Engine Integration vs Standalone Testing

**Status:** Design proposal (Release Architect). No project files modified.
**Date:** 2026-07-04 · **Context:** Sprint-002 MSVC verification · **Author role:** Technical Lead / Release Engineer

---

## 1. Problem statement

MSVC verification of the engine-facing translation unit surfaced a series of
build-compatibility failures (`<hash_map>`, `Device`/`permissive`,
`fast_dynamic_cast`, `INGAME_EDITOR`). The ABI investigation
(see [Build_Compatibility.md](./Build_Compatibility.md)) proved these are not
independent papercuts but symptoms of one root condition:

> The X-Ray (xray-monolith) engine ships **only a release-style ABI** — every
> engine configuration (`ReleaseR2`, `ReleaseR4`, `ProfiledDX11`, …) compiles
> with `NDEBUG`, `RuntimeLibrary = MultiThreadedDLL` (`/MD`), `_SECURE_SCL=0`,
> exceptions off, and permissive name lookup. There is **no debug engine** to
> link against.

The current `xrMP.vcxproj` **Debug** configuration (`/MDd`, `_DEBUG`,
`_ITERATOR_DEBUG_LEVEL=2`, `/EHsc`, `/permissive-`) is ABI-incompatible with that
engine on four axes (STL layout, CRT/heap, exception model, and — via
`INGAME_EDITOR` — engine vtables). Patching one macro per failure cannot produce
a valid link; the next wall (`LNK2038`) is structurally guaranteed.

This document defines how xrMP should be built so that the engine boundary is
ABI-correct by construction.

---

## 2. Two axes that were being conflated

The prior "one engine-facing TU" rule mixed two independent concerns. The design
separates them explicitly:

- **Source-inclusion boundary (unchanged):** `EngineAdapters.cpp` remains the
  *only* translation unit that `#include`s X-Ray engine headers. This is the
  architectural "One Engine Boundary" seam and does not change.
- **ABI boundary (the real issue):** Every object file linked into the engine
  binary must share one CRT, one STL layout, and one exception model. That is a
  property of the **entire engine-linked configuration**, not of a single file.
  `xrMP.lib` also contains the core/world/common objects; they do not include
  engine headers, but they still travel into the engine binary and therefore
  must be compiled with the engine ABI.

Conclusion: keep the *source* boundary at `EngineAdapters.cpp`; move the *ABI*
requirement to the whole engine-linked configuration.

---

## 3. Answers to the design questions

**Q1 — Separate configurations for standalone testing vs engine integration?**
Yes. They are already two artifacts (`xrMP_Tests.exe`, `xrMP.lib`); the design
formalizes their configuration sets so they can never share ABI-incompatible
settings by accident.

**Q2 — Tests stay Debug while engine-linked `xrMP.lib` is always release-style?**
Yes. `xrMP_Tests` never touches the engine (it compiles `NullEngineAdapters.cpp`
and links no engine library), so it is free to stay `/MDd` + `_DEBUG` +
`/permissive-` + `/W4 /WX` strict C++17 — the correctness gate for the frozen
77 tests. The engine-linked `xrMP.lib` must always mirror the engine ABI.

**Q3 — Should the whole engine-linked config mirror the engine ABI, or only
`EngineAdapters.cpp`?** The whole engine-linked configuration mirrors the engine
ABI (see §2). `EngineAdapters.cpp` remains the only engine-*header*-including TU,
but the ABI (NDEBUG, `/MD`, `_SECURE_SCL=0`, exceptions-off, permissive) applies
to every object in `xrMP.lib`.

**Q4 — Final configuration layout.** See §4.

**Q5 — Mechanism.** A shared property sheet is the primary mechanism; plus
pruning the ABI-invalid `Debug` config from the engine-linked library, plus
solution-configuration mapping. See §5.

**Q6 — Estimates.** See §7.

---

## 4. Recommended configuration layout

Two artifacts, each with a fixed, non-overlapping ABI contract.

### Artifact A — `xrMP_Tests` (Application) — correctness gate
- Purpose: run the 77 Sprint-001/002 unit tests. **Never** links the engine.
- Sources: core/world/common `.cpp` + `NullEngineAdapters.cpp` + GoogleTest.
- Config `Debug|x64`: `/MDd`, `_DEBUG`, `_ITERATOR_DEBUG_LEVEL=2`,
  `/EHsc`, `ConformanceMode=true` (`/permissive-`), `/W4 /WX`, `stdcpp17`.
- Config `Release|x64`: `/MD`, `NDEBUG`, otherwise same strict profile.
- Rationale: maximum diagnostics (checked iterators, asserts, conformance) with
  zero engine coupling. This is where correctness is proven.

### Artifact B — `xrMP.lib` (StaticLibrary) — engine integration
- Purpose: the library `xrEngine` links. Contains `EngineAdapters.cpp` (only
  engine-facing TU) + shared core/world/common sources.
- ABI contract (all configs), inherited from a shared property sheet:
  `NDEBUG`, `RuntimeLibrary = MultiThreadedDLL` (`/MD`), `_SECURE_SCL=0`
  (→ `_ITERATOR_DEBUG_LEVEL=0`), `ExceptionHandling = false` (match engine),
  `ConformanceMode=false` (`/permissive`), `_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS`,
  and the `..\Engine\xray-monolith\src\3rd party` include root.
- Configs: `Release|x64`, `Release-AVX|x64`, `ProfiledDX11|x64`, `Verified|x64`
  — each aligned to the corresponding engine configuration.
- The `Debug|x64` config is **removed from the engine-linked library** (it cannot
  ABI-link to a `/MD`-only engine). If a debug-symbol engine-linked build is ever
  needed, it is a `/MD` + `NDEBUG` + full-PDB variant (debug *info*, not debug
  *CRT*) — never `/MDd`.

Mapping to your proposed names: your `xrMP_Test` == existing `xrMP_Tests`;
your `xrMP_Engine` == existing `xrMP` (the `.lib`). Recommendation is to keep the
existing project names and formalize their configs, avoiding rename churn.

| Property | `xrMP_Tests` (A) | engine-linked `xrMP.lib` (B) | Engine |
|---|---|---|---|
| Debug/Release macro | `_DEBUG` | `NDEBUG` | `NDEBUG` |
| RuntimeLibrary | `/MDd` | `/MD` | `/MD` |
| `_ITERATOR_DEBUG_LEVEL` | 2 | 0 (`_SECURE_SCL=0`) | 0 |
| Exceptions | `/EHsc` | off (match engine) | off |
| Conformance | `/permissive-` strict | `/permissive` | `/permissive` |
| Warnings | `/W4 /WX` | `/W3 /WX-` (engine headers) | `/W3`-class |
| Links engine? | No | Yes | — |

---

## 5. Mechanism

**(a) Shared property sheet — primary mechanism.**
Create `Multiplayer/build/EngineAbi.props` defining the Artifact-B ABI contract
in one place: `NDEBUG`, `_SECURE_SCL=0`, `RuntimeLibrary`, `ExceptionHandling`,
`ConformanceMode=false`, `_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS`, and the
3rd-party include dir. Import it from `xrMP.vcxproj` for the engine-linked
configs only. This is DRY (single source of truth), avoids per-config
duplication, and — deliberately — does **not** import the engine's full
`Common.props` (which also carries optimization, renderer selection, and
module-identity macros that a consumer library must not inherit).

**(b) Prune the ABI-invalid Debug config from the library.** Remove `Debug|x64`
from `xrMP.vcxproj` (Artifact B). The per-file compatibility settings currently
on the `EngineAdapters.cpp` item (`permissive`, hash-silence, 3rd-party include)
are then **absorbed into the property sheet** and the per-file overrides removed —
they become project-wide for the engine-linked lib, which is now correct.

**(c) Solution-configuration mapping.** The engine solution
(`engine-vs2022.sln`) drives which `xrMP` config it links; ensure its
Release-family configs map `xrMP` to the matching engine-ABI config. In the
standalone `xrMP.sln`, map the solution's build so the library builds in an
engine-ABI config and `xrMP_Tests` builds Debug/Release as its own gate.

**Does this need new vcxproj files?** No. The two artifacts already exist as two
`.vcxproj`. The change is: one new `.props`, edits to `xrMP.vcxproj` configs,
and solution mapping. No new projects, no source changes.

---

## 6. Why this preserves the architecture

- **Preserve Before Replace / Minimal Engine Modification:** zero engine edits;
  the fix lives entirely in STALKER-MP build configuration.
- **One Engine Boundary:** `EngineAdapters.cpp` stays the sole engine-header TU;
  the ABI contract simply applies at the correct (library) granularity.
- **Host Authority:** unaffected — this is compile/link only.

---

## 7. Estimates

- **Implementation complexity: Low–Medium.** One property sheet, import lines,
  removal of one config, absorption of three per-file overrides, and solution
  mapping. No source changes. Bounded, mechanical.
- **Future maintenance cost: Low (net reduction).** Replaces per-macro
  accumulation with a single ABI source of truth. New engine-facing code inherits
  correct ABI automatically instead of re-triggering this investigation.
- **Risk to Sprint-001/002 freeze: Low.** No production source is modified. The
  frozen 77 tests run in `xrMP_Tests`, which is left byte-for-byte unchanged, so
  the correctness gate is fully preserved. Changes are confined to build
  configuration of the engine-linked library — within "build config, not frozen
  source."
- **Compatibility with later sprints: High (enabling).** Sprints 003–015 add many
  engine-facing adapters (Entity Registry, Bubble Manager, ALife, host
  networking, replication). A formalized engine-ABI property sheet is exactly the
  infrastructure those sprints require, and prevents recurrence of ABI drift.

---

## 8. Consequence: verification procedure changes — documents to update

Adopting this model makes the current "build xrMP in Debug and link the engine"
procedure invalid. The following documents must be updated (none modified yet):

1. **`Multiplayer/docs/Sprint-001-Verification.md`** — primary checklist. Resolve
   the internal contradiction: unit tests run via `xrMP_Tests` (Debug + Release,
   standalone); the engine-linked `xrMP.lib` is built only in a release-style
   (engine-ABI) config; step 6 ("Module Debug build") must be rewritten.
2. **`Documentation/AI/NEXT_TASK.md`** — Task 1 steps 2 and 4 ("Open `xrMP.sln`
   and build under `Debug|x64`", then run tests) must reflect the two-artifact
   procedure and the engine-ABI config for linkage.
3. **`Documentation/AI/CURRENT_STATUS.md`** — the "MSVC Verification Gate"
   description references a Debug build; update to describe the build model and
   note ABI alignment as part of the gate.
4. **`Documentation/AI/DECISIONS_LOG.md`** — record the approved architectural
   decision: standalone-test (Debug) vs engine-linked (Release-ABI) split.
5. **`Documentation/AI/AI_CONTEXT.md`** — §4 workflow ("unit tests green on
   GCC/MSVC", "verify within MSVC") and §3 boundaries should reference the ABI
   boundary and two-artifact model.
6. **`Documentation/AI/SESSION_LOG.md`** — append a chronological session entry.
7. **`Documentation/Engine_Integration/Build_Compatibility.md`** — add BC-004
   (`INGAME_EDITOR`/`NDEBUG`), and note that the per-file BC-001/002/003 fixes are
   superseded/absorbed by `EngineAbi.props`.
8. **`Multiplayer/docs/CodingStandards.md`** — clarify that the `/W4 /WX` strict
   C++17 profile governs framework sources and the test artifact; the
   engine-linked lib compiles permissive to match the engine.
9. **`Multiplayer/docs/Decisions.md`** — mirror the decision if this log is kept
   in parallel with the AI decisions log.
10. **`Documentation/AI/AI_START_HERE.md`** — add this Build Model doc to the
    onboarding reading order so future sessions inherit the model.

---

## 9. Recommendation

Adopt the two-artifact model with an `EngineAbi.props` property sheet:
`xrMP_Tests` stays the Debug/strict correctness gate; the engine-linked
`xrMP.lib` is always engine-ABI (release-style). This resolves the entire class
of ABI failures in one deliberate move, keeps every project philosophy intact,
and does not touch frozen source. Implementation should proceed only after the
verification-procedure documents in §8 are updated (or the update is scheduled),
so the gate and the build model stay consistent.

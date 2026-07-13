# ADR-007 — Engine ABI Contract (Exception and STL Usage Policy)

- **Status:** Proposed (design review). Awaiting project-owner approval.
- **Date:** 2026-07-04
- **Deciders:** Project owner (approval), Lead Systems Architect (author).
- **Relates to:** extends the D-series decision log (`Documentation/AI/DECISIONS_LOG.md`, D-001..D-006) and the build-model design (`Documentation/Engine_Integration/Build_Model.md`). If approved, record a pointer as **D-007** in the decision log.
- **Scope:** every **engine-linked** translation unit shipped through Sprint-015 (i.e. every TU compiled into `xrMP.lib`). Translation units that never link the engine — notably the standalone `xrMP_Tests` executable — are outside the contract's scope.

---

## 1. Context

Matching the X-Ray engine ABI made the engine-linked Release build reach STALKER-MP source compilation, where it failed on **C4530** ("C++ exception handler used, but unwind semantics are not enabled"), promoted to an error by `/WX`. The triggering constructs:

- `src/core/Log.cpp` — explicit `throw std::runtime_error` (line 106) in `FileLogSink`'s constructor; `std::ofstream operator<<` (line 114).
- `src/core/Bootstrap.cpp` — explicit `try`/`catch (std::exception)` (lines 145–151) and a catch-all (lines 455–475).
- `src/common/FileSystemUtil.cpp` — no explicit exceptions; C4530 arises purely from `<filesystem>`, `<sstream>`, `<fstream>`, iostream internals. This file already returns `core::Expected<T>` and uses `std::error_code`.

This is not another compiler papercut; it forces a project-wide architectural decision. Earlier drafts of this ADR expressed that decision as a **denylist of libraries** ("prohibit `std::filesystem`, iostream, …"). That framing was too broad: it named technologies where the true requirement is an **architectural property**. This revision replaces the denylist with a single governing abstraction — the **Engine ABI Contract** (§3) — and expresses every downstream rule as conformance to it.

### 1.1 Decisive evidence: the engine is not uniformly exceptions-off

`xrCore.vcxproj` and `xrEngine.vcxproj` set `ExceptionHandling=false` in every configuration, but `3rd party/lua-extensions/lua_extensions.vcxproj` and `xrEngine/_scripting.cpp` compile **with** exceptions. The shipped engine binary already links EH-enabled and EH-disabled objects together. Therefore `/EHsc` on `xrMP.lib` would not, by itself, break the link. The choice of exception model is consequently an **architectural** decision, not one forced by a hard link constraint — which is why it belongs in a contract expressed as invariants (§3), and why the reasoning must rest on ABI *soundness* rather than mere link success (§5.10, §9).

---

## 2. The two honest framings

**A — "exceptions and throwing STL are foreign to this codebase."** The project already declares `core::Expected<T>` as the error model, constructor injection as the wiring model, and "no exceptions across the engine boundary." The engine core is EH-off. Under this framing the Sprint-001 `throw`/`try`/`catch` and throwing STL are the anomaly the ABI-matched build surfaced.

**B — "exceptions never escape, so enable `/EHsc` and move on."** The Sprint-001 exceptions are caught internally and converted to `Expected<T>`; `std::filesystem`/iostream are standard; the engine tolerates EH-on modules.

Both are internally coherent. The decision is made on long-term architectural forces, formalized as the contract below.

---

## 3. Engine ABI Contract

> **Definition.** An *engine-linked translation unit* is any TU compiled into a binary that is linked with, and shares types or allocations with, the X-Ray engine. Every such TU **must be a binary-compatible peer of the engine core**. The Engine ABI Contract is the set of architectural invariants that guarantee this. It is defined in terms of *sameness with the engine*, not in terms of specific compiler switches, so that it survives compiler and toolchain upgrades: whatever realization the engine uses, an engine-linked TU must match it.

The contract has seven invariants. Each is an architectural property; the parenthetical notes name the *current* realization for orientation only and are not themselves the rule.

1. **Single runtime model.** An engine-linked TU shares one C/C++ runtime and heap with the engine, so allocations, deallocations, and runtime globals are interoperable. *(Why: memory allocated by one side is freed by the other across engine/module calls; divergent runtimes corrupt the heap. Currently: dynamically-linked release runtime.)*

2. **Uniform standard-library ABI.** The standard library must be instantiated with the same options that affect the size and layout of its types — iterator-hardening / checked-iterator / debug levels — as the engine. *(Why: these options change `sizeof` and internal layout of containers; the module manipulates engine-visible, STL-derived types such as the engine's `xr_vector`/`xr_hash_map`, so a mismatch is a silent layout divergence. Currently: secure-SCL / iterator-debug level pinned to the engine's release value.)*

3. **Consistent object layout.** No configuration may add, remove, or reorder members or virtual functions of any engine-visible type. Build-time toggles that alter layout are forbidden. *(Why: the engine and module must agree bit-for-bit on shared classes. Currently: release/`NDEBUG` build, and specifically no editor toggle that would turn engine members virtual.)*

4. **Consistent exception model.** The TU must present the same exception configuration the engine core presents, and must **not introduce compiler-generated unwind semantics** the engine core does not have. *(Why: the exception configuration influences standard-library internals and the presence of unwind funclets; a module that assumes a different exception world than the types it shares is, per the standard-library vendor, an unsupported mixture — and the risk is concentrated exactly where this module touches engine STL-derived containers. Currently: exceptions disabled, matching the engine core.)*

5. **Header-compatible compiler behavior.** The TU must use the name-lookup / conformance behavior that the engine headers require to compile. *(Why: engine headers rely on permissive, delayed name lookup — e.g. `_vector3d.h` referencing the global `Device` as a non-dependent name. A stricter conformance mode rejects headers the engine itself compiles. Currently: permissive lookup for engine-including TUs.)*

6. **No exception-based control flow.** Failure must be reachable without relying on a thrown-and-caught exception. Fallible operations return `core::Expected<T>`; fallible construction is expressed as `Expected<T>`-returning factory functions; constructors do not fail. *(Why: this is the runtime companion to invariant 4 — under the engine's exception model an uncaught throw terminates the process, and exception-driven control flow is non-deterministic, which is unacceptable in a host-authoritative simulation. `Expected<T>` is the project's sole error channel.)*

7. **No engine-header ABI divergence.** The TU must not define or undefine macros that change the meaning of engine headers. *(Why: such macros (e.g. an in-game-editor toggle) can silently alter engine class layout or semantics for this TU relative to the engine libraries. Currently: the module inherits the engine's macro environment for engine-including TUs and adds none that alter engine semantics.)*

**Enforcement.** The contract is largely *self-enforcing at compile time*: the engine-ABI build configuration (realized today by `Multiplayer/build/EngineAbi.props` plus `/WX`) turns most violations of invariants 2–5 and 7 into hard build failures — C4530 for invariant 4/6, layout/link diagnostics for the rest. Invariant 6's runtime aspect (not relying on a facility that *throws at runtime* to report errors, e.g. `std::stoi`) is the one part not caught by the compiler and is a code-review rule. The contract deliberately turns "policy" into "build configuration" wherever possible, because a compiler-enforced invariant outlasts a review rule across fifteen sprints and multiple contributors.

### Contract Authority

The Engine ABI Contract is the **normative** architectural contract for every engine-linked translation unit. It is binding, not advisory.

No sprint specification, implementation document, coding guideline, implementation plan, design review, or future ADR may weaken, narrow, or contradict any of the seven invariants while ADR-007 remains in force. A document that appears to do so is void on that point and defers to this ADR.

Any change to an invariant — including relaxing, strengthening, or reinterpreting it — requires a formal architectural review and a revision of ADR-007 that explicitly supersedes the affected text. Until such a revision is approved, the invariants stand exactly as written in §3.

### Contract Completeness

The seven invariants above collectively define the **complete** Engine ABI Contract. There are no additional, implicit, or "unwritten" ABI rules; if a requirement is not expressed as, or derivable from, one of the seven invariants, it is not part of the contract.

Future architectural decisions shall be interpreted **through** these seven invariants. New invariants may be introduced only by formally revising ADR-007; no other document may add an eighth invariant by implication or convention.

---

## 4. Conformance Rule

> **Any language feature, compiler feature, runtime component, allocator, build configuration, standard-library facility, or third-party library may be used in an engine-linked translation unit only if it conforms to the Engine ABI Contract (§3).**

There is no permanent denylist. Conformance is a property of a facility *under the current toolchain and its build options*, not of its name.

- **Facilities that do not conform under today's toolchain** (examples, not permanent prohibitions): `std::filesystem`, iostream formatting, `std::fstream`/`std::ofstream`/`std::ifstream`, and `std::ostringstream`/`std::stringstream`. Each instantiates compiler-generated unwind code under the engine's exception model, violating invariant 4 (and, where it relies on throwing to report failure, invariant 6). They are excluded *for that reason*, and only until it no longer holds.
- **Facilities that conform** are admitted automatically, with no ADR amendment. Non-throwing standard-library components (`std::string`, `std::vector`, `std::string_view`, `<charconv>`, and similar) qualify today. A third-party library qualifies if it can be built to conform — for example, **`fmt` compiled with exceptions disabled** (`FMT_EXCEPTIONS=0`), a future non-throwing logging backend, or a serialization library compiled without unwind semantics. If it conforms, it is allowed; if it stops conforming, it is not. The rule never changes; the set of conforming technologies evolves.

The standalone `xrMP_Tests` executable is outside the contract's scope and may use exceptions and exception-aware facilities (GoogleTest, `std::filesystem` in fixtures).

---

## 5. Analysis of the current triggers against the contract

**5.2 Explicit `throw` (Log.cpp:106).** Violates invariant 6. It exists only because a constructor cannot return `Expected<T>`; the factory pattern resolves it. A genuine architectural inconsistency with the declared error model, independent of any library choice.

**5.3 `try`/`catch` (Bootstrap.cpp).** Violates invariants 4 and 6. The inner block exists only to convert `FileLogSink`'s throw into `Expected`; it disappears once `FileLogSink` is a factory. The outer catch-all is a boundary net that, under the engine's exception model, is redundant (unrecoverable conditions are fatal, as in the engine).

**5.4 `std::filesystem` / 5.7 iostream / 5.8 `fstream` / `ostringstream`.** Do not satisfy invariant 4 under today's toolchain (they instantiate unwind code). `FileSystemUtil` already uses `error_code` and returns `Expected<T>`, so its *error model* is contract-conformant; only its *facility choice* is not. Replacement is by any contract-satisfying I/O primitive (the engine's `FS`/`IReader`/`IWriter`, or a thin non-throwing `FILE*`/OS wrapper returning `Expected<T>`) — chosen on ABI-conformance grounds, not because one is intrinsically preferred.

**5.10 The concentrated ABI risk (invariant 4 rationale).** Enabling `/EHsc` sets the module's exception configuration to differ from the engine core's. The engine tolerates EH-on *leaf* modules (lua-extensions), but this module manipulates engine STL-derived container internals — the precise surface where a differing exception configuration is, per the standard-library vendor, an unsupported mixture. Link success (§1.1) does not imply ABI soundness. This is the strongest reason invariant 4 pins the module to the engine's exception model rather than to `/EHsc`.

---

## 6. Strategy comparison

**A** — engine-linked library satisfies the contract with exceptions disabled; explicit exceptions removed; non-conforming facilities replaced by conforming ones.
**B** — enable `/EHsc`; keep `Expected<T>` public; forbid exceptions crossing subsystem boundaries by review policy.
**C** — hybrid: the engine-linked library follows A; the standalone `xrMP_Tests` (outside contract scope) keeps exceptions.

| Dimension | A (contract-conformant, EH-off) | B (/EHsc) | C (hybrid) |
|---|---|---|---|
| **Advantages** | Full contract conformance; compiler-enforced (invariant 4 ⇒ C4530); strongest determinism | Least churn now; keeps std::filesystem/iostream; fastest to green | Contract-conformant shipped lib **and** ergonomic tests; matches two-artifact build model |
| **Disadvantages** | Must move Sprint-001 I/O onto conforming primitives (touches frozen code) | Violates invariant 4; exception configuration diverges from engine core exactly where the module touches engine containers; review-enforced only | One extra configuration boundary |
| **Maintenance cost** | Higher once, then self-enforcing | Low now, permanent vigilance policing boundary leaks | Like A for the library; tests unconstrained |
| **ABI risk** | Lowest | Moderate, concentrated on engine STL-derived types | Lowest for shipped lib |
| **X-Ray compatibility** | Highest (matches core) | Acceptable (engine links EH modules) but non-matching | Highest for shipped lib |
| **Determinism impact** | Strongest | Weaker (latent exception paths) | Strong for shipped lib |
| **Long-term technical debt** | Low; aligns future subsystems to one contract | `/EHsc` must be re-justified at every engine upgrade; boundary rule is a standing review burden, worst at plugin/scripting edges | Low |

`/EHsc` (B) is rejected: it trades a one-time, bounded change for a permanent divergence from the engine's exception model (invariant 4) plus a permanent review obligation, and it places that divergence where the module manipulates engine container internals. The plugin loader (Sprint-014) and Lua scripting (Sprint-013) are where a review-only "exceptions must not cross the boundary" rule is least likely to survive — and where a compiler-enforced contract is most valuable.

---

## 7. Evaluation against project principles

- **Preserve Before Replace:** *does not apply* to general-purpose standard-library utilities. PBR governs engine *systems* (ALife, AI, physics, rendering, gameplay), not file/string I/O in our own module. Using `std::filesystem` would not violate PBR, and the recommendation does **not** rest on PBR. Where a conforming primitive happens to be an engine facility (`FS`/`IReader`/`IWriter`), that is an ABI-conformance convenience, not a PBR obligation.
- **Minimal Engine Modification:** all strategies keep zero engine edits.
- **One Engine Boundary:** the contract extends the single-boundary discipline to the ABI: every engine-linked TU is a binary peer of the engine, and the points where non-conforming (e.g. throwing) code could enter are made explicit and compiler-checked.
- **Host Authority / Deterministic Simulation:** invariants 4 and 6 remove non-local, non-deterministic control flow from the simulation library.
- **Dependency Injection:** unaffected; factories (invariant 6) compose with constructor injection — a `Create` returns the object the composition root injects.
- **`Expected<T>` error handling:** invariant 6 makes it the sole error channel.

---

## 8. Does Sprint-001 violate architecture, or only build configuration?

Both, in different places:

- **`std::filesystem`, `std::ofstream`, `std::ostringstream`, iostream** — a **build-configuration / facility-choice** issue only. `FileSystemUtil.cpp` already returns `Expected<T>` and checks `error_code`/stream state; its *error model* satisfies invariant 6. It conflicts only with invariant 4 under the current toolchain.
- **`throw std::runtime_error` + `try`/`catch`** — a **genuine architectural** violation (contained) of invariants 4 and 6. Using exceptions as an internal error channel contradicts the declared `Expected<T>` model even though they never escape. This should be corrected regardless of any facility decision.

Verdict: primarily a build-configuration/facility mismatch, with one real but contained architectural drift in the logging/bootstrap error path.

---

## 9. Challenge to the recommendation (strongest counterargument)

*"Formalizing a contract to keep exceptions out is over-engineering. The engine binary already links EH modules, so `/EHsc` is link-safe. `std::filesystem`/iostream are standard and battle-tested; replacing them adds bug surface. C4530 is just a flag; exceptions never escape anyway. Enable `/EHsc`, keep `Expected<T>` public, and move on."*

The strongest case for B, strong on churn and portability. It fails on two points that the contract makes precise:

1. **ABI soundness ≠ link success.** Invariant 4 is about the exception *configuration* differing from the engine core exactly where the module manipulates engine STL-derived containers — an unsupported mixture per the standard-library vendor. The engine's tolerance for leaf EH modules does not transfer to a module woven into engine container internals.
2. **Enforcement half-life.** A contract realized as build configuration is compiler-enforced and survives fifteen sprints, a plugin loader, and a scripting bridge; a review rule does not. Portability of `std::filesystem` is moot here — the engine-linked artifact is single-target, and the cross-platform test build (outside contract scope) is unaffected.

*(An earlier draft added a third leg — Preserve Before Replace — which is retracted: PBR does not apply to general-purpose STL, and `std::filesystem` is non-conforming only for failing invariant 4, not for being "a parallel stack." The recommendation rests solely on ABI soundness, compiler-enforced invariants, deterministic engine integration, and long-term maintainability.)*

The recommendation survives on those grounds.

---

## 10. Recommendation (single policy, valid through Sprint-015)

Adopt **Strategy C**: every engine-linked translation unit must satisfy the **Engine ABI Contract (§3)**; the standalone `xrMP_Tests` executable is exempt.

Concretely, this means:

1. Engine-linked TUs are compiled in the engine-ABI configuration (contract invariants 1–5, 7). `/EHsc` for engine-linked code is rejected (invariant 4).
2. Engine-linked code uses `core::Expected<T>` as its sole error channel; **fallible construction is an `Expected<T>` factory**, and constructors do not fail (invariant 6). No exceptions are used as control flow.
3. **Facility selection is governed by the Conformance Rule (§4), not by a denylist.** Any feature or library that conforms to the contract is permitted; facilities that do not (today: `std::filesystem`, iostream, `fstream`, `ostringstream`) are replaced by conforming primitives; conforming future libraries (e.g. `fmt` with `FMT_EXCEPTIONS=0`) are admitted automatically.
4. The engine-ABI build configuration (`EngineAbi.props` + `/WX`) is the enforcement mechanism for the compile-time invariants; invariant 6's runtime aspect is a review rule.

### 10.1 Consequences / follow-up (not authorized by this ADR)

- Sprint-001 `FileLogSink` becomes an `Expected<T>` factory; `FileSystemUtil` and the file sink move to a contract-conforming I/O primitive; Bootstrap's `try`/`catch` blocks are removed. Because Sprint-001 is frozen, this proceeds as an **approved genuine-defect correction**, tracked separately and re-verified against the 77 tests (which run in the exception-permitted `xrMP_Tests`, unaffected by the library's exception model).
- Documents to update on approval: `DECISIONS_LOG.md` (record D-007), `Multiplayer/docs/CodingStandards.md` (reference the contract), `Documentation/Engine_Integration/Build_Model.md` and `Build_Compatibility.md` (cross-reference), `AI_CONTEXT.md` (§2 error model).

### 10.2 Why this is the durable choice

It removes an entire class of future failures by construction, matches the engine on the ABI axes that affect the module, keeps the simulation library free of non-deterministic control flow, and leaves tests ergonomic — optimizing for networking, persistence, plugins, scripting, and engine upgrades rather than for turning one build green.

---

## 11. The contract as the long-term governing rule

The Engine ABI Contract is the long-term governing rule for every engine-linked module. Individual libraries will change, compiler implementations will evolve, and new technologies will be adopted. The contract intentionally governs **architectural properties** — runtime model, standard-library ABI, object layout, exception model, header-compatible compiler behavior, absence of compiler-generated unwind semantics, and absence of engine-header ABI divergence — rather than specific implementations, so that the project remains stable while individual technologies evolve. Compiler flags and library names in this document are illustrative realizations of the contract at the time of writing; when the toolchain changes, the realizations are re-derived and the contract stands unchanged.

---

## 12. Self-review: is any restriction broader than necessary?

A deliberate pass for over-restriction and residual technology-naming:

- **"No exceptions" as a blanket ban?** Narrowed. The governing invariants are 4 (same exception model as engine) and 6 (no exception-based control flow). "No `throw`/`try`/`catch` in engine-linked code" is stated as a *consequence* of those invariants, not an independent taboo.
- **Named-library bans?** Removed from the normative text. `std::filesystem`/iostream/`fstream`/`ostringstream` and `fmt` appear only as **time-stamped examples** of (non-)conformance under §4, explicitly non-permanent.
- **Any rule naming a library where a property would be more appropriate?** The remaining library names are all in illustrative/example positions; the normative rules reference only the contract.
- **Any unnecessary restriction found?** The exemption of `xrMP_Tests` is preserved so the test artifact is never constrained by an ABI it does not link against. No further restriction was found that the five guarantees (engine-ABI compatibility, deterministic simulation, `Expected<T>`, zero engine modification, long-term maintainability) do not require.

No changes were manufactured beyond those that convert implementation-specific wording into architectural wording.

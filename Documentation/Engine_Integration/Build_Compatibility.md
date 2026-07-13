# Engine Build Compatibility Log

This document records every compatibility discovery encountered while building
STALKER-MP against the X-Ray (Anomaly / xray-monolith) engine under MSVC.

Each entry follows the same structure so future engine upgrades and onboarding can
scan it quickly. Fixes recorded here are **build-configuration** fixes localized to
STALKER-MP's own project files. Per project philosophy (*Preserve Before Replace*,
*Minimal Engine Modification*, *One Engine Boundary*), the engine source is never
modified, and compatibility shims are confined to the single engine-facing
translation unit (`Multiplayer/src/adapters/EngineAdapters.cpp`) via its `ClCompile`
item in `Multiplayer/xrMP.vcxproj`.

Template for new entries:

```
## <ID> — <short title>
- Date / Sprint:
- Symptom:
- Root cause:
- Evidence:
- Approved fix:
- Why localized:
```

---

## BC-001 — `<hash_map>` deprecation on the engine-facing TU
- **Date / Sprint:** 2026-07-04 / Sprint-002 (MSVC verification, frozen source)
- **Symptom:** MSVC diagnostic (STL4041 / C4996) originating from
  `xrCore/xrCore.h:197`, which under `#ifndef _EDITOR` includes the deprecated MSVC
  extension header `<hash_map>`; `xrCore/_stl_extensions.h` derives `xr_hash_map`
  from `stdext::hash_map`.
- **Root cause:** `EngineAdapters.cpp` (the only STALKER-MP TU that includes X-Ray
  headers) reproduces the engine include chain but does not inherit the engine's
  compile-time opt-out for the stdext hash containers.
- **Evidence:** Engine defines `_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS` in
  `Engine/xray-monolith/src/Common.props:30`, inherited by every engine project
  (xrCore, xrEngine, ...). The STALKER-MP adapter item did not define it and does not
  import `Common.props`.
- **Approved fix:** Add `_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS` to the
  `EngineAdapters.cpp` `PreprocessorDefinitions` in `Multiplayer/xrMP.vcxproj`.
- **Why localized:** Only the adapter TU includes engine headers; the define mirrors
  the engine's own configuration and touches no other TU and no engine source.

## BC-002 — `Device: identifier not found` under `/permissive-`
- **Date / Sprint:** 2026-07-04 / Sprint-002 (MSVC verification, frozen source)
- **Symptom:** 46 errors (C3861 / C2065) from `xrCore/_vector3d.h` (711/717/723/729/736)
  and `xrCore/_matrix.h:921`: `'Device': identifier not found`, "while compiling class
  template `_vector3` / `_matrix`".
- **Root cause:** `_vector3<T>` / `_matrix<T>` have inline template members
  (`hud_to_world`, `world_to_hud`, `ema`, ...) referencing the global `Device`
  (declared `extern ENGINE_API CRenderDevice Device;` in `xrEngine/device.h:509`, a
  layer above xrCore). `Device` is a **non-dependent** unqualified name; under
  conformance mode (`/permissive-`, set by `ConformanceMode=true` in
  `xrMP.vcxproj:63`) two-phase lookup binds it at template *definition* time, where
  `Device` is not visible. This is a compilation-environment mismatch, **not** a
  missing include: even the engine's own xrEngine TUs parse `_vector3d.h` (via
  `xrCore.h`) before `device.h`, and compile because the engine builds in permissive
  mode. Adding `device.h` to the adapter would be architecturally inverted (xrCore
  depending on xrEngine) and would not address other permissive-only constructs.
- **Evidence:** No `ConformanceMode` / `/permissive` setting exists in the engine
  (`Common.props`, `xrCore.vcxproj`, `xrEngine.vcxproj` all lack it) → engine compiles
  in default permissive mode. STALKER-MP set `ConformanceMode=true` project-wide.
- **Approved fix:** Set `<ConformanceMode>false</ConformanceMode>` on the
  `EngineAdapters.cpp` item only. All Sprint-002 conformance-clean C++17 source keeps
  the project-wide `/permissive-`.
- **Why localized:** The adapter TU is the quarantine zone that adopts the engine's
  compile contract; relaxing conformance for it alone preserves strict conformance for
  the rest of the framework (One Engine Boundary).

## BC-003 — `fast_dynamic_cast/fast_dynamic_cast.hpp` not found (C1083)
- **Date / Sprint:** 2026-07-04 / Sprint-002 (MSVC verification, frozen source)
- **Symptom:** `xrCore/xrMemory_subst_msvc.h:156` (also `xr_resource.h:2`):
  `Cannot open include file: 'fast_dynamic_cast/fast_dynamic_cast.hpp'`.
- **Root cause:** Missing include directory. The header **exists in-repo** at
  `Engine/xray-monolith/src/3rd party/fast_dynamic_cast/fast_dynamic_cast.hpp`
  (not a missing dependency or submodule); the adapter TU's include paths omitted the
  `3rd party` root.
- **Evidence:** Engine resolves the angle-bracket include via
  `$(SolutionDir)..\src\3rd party` on `IncludePath` in `Common.props:19`. The adapter
  item listed `xrEngine`, `src`, `xrCore`, `sdk\include` but not `src\3rd party`.
- **Approved fix:** Add `..\Engine\xray-monolith\src\3rd party` to the
  `EngineAdapters.cpp` `AdditionalIncludeDirectories`.
- **Why localized:** Only the adapter TU includes engine headers; replicating the one
  required include root mirrors the engine's `IncludePath` without importing all of
  `Common.props`.

## BC-004 — C4530 exception-handling violation (ADR-007)
- **Date / Sprint:** 2026-07-06 / Sprint-002 (MSVC verification)
- **Symptom:** MSVC warning `C4530: C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc` treated as error under `/WX`.
- **Root cause:** The library target `xrMP` is compiled with exceptions disabled (`<ExceptionHandling>false</ExceptionHandling>` in `EngineAbi.props`), but referenced template-based STL iostreams (`std::fstream`, `std::ofstream`, `std::ostringstream`) inside `Log.cpp` and `FileSystemUtil.cpp`. Under `/EH` disabled, compiler generates warning/error when exceptions could be thrown by the STL.
- **Evidence:** Compiler error output explicitly pointing to throwing stream constructors.
- **Approved fix:** Replaced `std::fstream`, `std::ofstream`, and `std::ostringstream` with a non-throwing interface using C standard input/output (`std::FILE*`, `fopen_s`, `fwrite`, `fread`, etc.) and custom `core::Expected<T>` error propagation. Crucially retained `<filesystem>` (e.g. `std::filesystem::create_directories`) via `std::error_code` overloads which do not instantiate compiler-generated exception unwinds under MSVC.
- **Why localized:** Localized to the framework core, preserving Invariant 4 and 6 of `ADR-007` while retaining standard filesystem features verified as exception-clean under current MSVC toolchain constraints.

## BC-005 — `std::unordered_map` / `std::unordered_set` ABI probe (Sprint-003 Gate 1)
- **Date / Sprint:** 2026-07-06 / Sprint-003 (Pre-Implementation Evidence Gate 1)
- **Symptom:** N/A — this records the outcome of the ADR-007 conformance probe required
  before the Entity Registry chose its lookup containers (Sprint-003 spec §14, Gate 1).
- **Root cause / question:** Under the engine ABI (`EngineAbi.props`: exceptions disabled,
  `/WX`), do `std::unordered_map` / `std::unordered_set` instantiate compiler-generated
  exception unwinds (C4530)? Conformance under `/EH`-off is empirical (cf. BC-004), so it
  must be measured, not assumed.
- **Evidence:** A standalone translation unit compiled under `EngineAbi.props`
  (`/WX`, `/MD`, RTTI disabled, exceptions disabled) exercised `std::unordered_map` and
  `std::unordered_set` across insert / erase / reserve / rehash / lookup / iteration:
  clean compilation, 0 warnings, 0 errors, no C4530, no C2220.
- **Approved outcome:** `std::unordered_map` / `std::unordered_set` are **conformant** and
  approved as the registry's secondary lookup structures. The primary canonical store
  remains the sorted `std::vector` that defines ascending-`EntityId` iteration order (I3);
  hash containers are secondary accelerators only and never define order.
- **Why localized:** A configuration-level conformance measurement; no engine source and no
  build-config change resulted. The fallback (sorted `std::vector` + binary search, or an
  in-module open-addressing map) was not needed.


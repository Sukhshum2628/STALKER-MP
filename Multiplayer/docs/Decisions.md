# STALKER-MP — Approved Specification Deviations and Decisions

Deviations from the sprint specifications require explicit approval
(project rule: documentation has higher authority than implementation).
This file is the record of those approvals.

## D-001 — MSVC solution instead of CMake (Sprint-001 §7.3)

**Approved:** 2026-07-03 by project owner.

Sprint-001 §7.3 specifies CMake. The target engine (`Engine/xray-monolith`,
Anomaly) builds exclusively via MSVC solutions. To minimize integration risk,
STALKER-MP integrates into the existing `engine-vs2022.sln` as `xrMP.vcxproj`
and does **not** introduce a parallel CMake build. A CMake migration may be
revisited together with a future engine toolchain migration.

Consequences: warnings-as-errors (§7.3) is enforced via
`<TreatWarningAsError>` + `Level4` in the xrMP projects; the engine's own
projects keep their existing warning settings.

## D-002 — C++17 instead of C++20 (Coding Standard)

**Approved:** 2026-07-03 by project owner.

The engine compiles as `stdcpp17`. The module uses C++17 until the engine
toolchain is upgraded. Practical substitutions:

| Standard requirement | C++17 substitute |
|---|---|
| `std::expected` | `stalkermp::core::Expected<T>` (`core/Expected.h`) |
| `std::span` | avoided; explicit pointer+size or references |
| concepts | `static_assert` + type traits |

All other requirements (RAII, smart pointers, `std::optional`, `constexpr`,
`enum class`, strong typing, move semantics, const correctness, `noexcept`)
apply unchanged.

## D-003 — Self-contained test framework (SUPERSEDED by D-005)

Sprint-001 initially used a self-contained framework
(`tests/framework/SmpTest.h`) because GoogleTest could not be vendored in
the implementation environment. Revision-1 R3 replaced it with vendored
GoogleTest; the custom framework has been deleted.

## D-005 — GoogleTest 1.11.0 vendored from the Debian/Ubuntu source package

**Implemented:** Sprint-001 Revision-1 (R3).

GoogleTest is vendored at `third_party/googletest/` (`include/`, `src/`,
`LICENSE`). GitHub remained unreachable from the implementation
environment, so the source was obtained from the Ubuntu `googletest`
package (upstream release **1.11.0**, Debian revision 1.11.0-3) — identical
upstream sources, different distribution channel. 1.11.0 fully supports
C++17 and MSVC v143. When direct GitHub access is available, bumping to a
newer pinned release (e.g. 1.14.x) is a drop-in replacement of the vendored
tree. `gtest-all.cc` builds as third-party code without `/W4 /WX`.

## D-006 — ISerializable is provisional

`core/interfaces/ISerializable.h` satisfies Sprint-001 §7.8, but its
concrete byte-buffer signature is **provisional until Sprint-008
(Replication) / Sprint-011 (Persistence)** finalize the serialization
format. Those sprints may revise the signature without a deprecation
period. No implementation or caller exists in Sprint-001.

## Deferred decisions

- **Log rotation** — deliberately not implemented in Sprint-001. The file
  sink appends without size limits, which is acceptable for development but
  not for long-running persistent servers (02_Engine.md §20.5). Deferred to
  **Sprint-015 (Optimization & Testing / Diagnostics)**: size-capped
  rotation or session-stamped log files, decided there.

## D-004 — Legacy `engine.sln` not modified

Only `engine-vs2022.sln` (the solution used by `batch_build.bat`) registers
`xrMP`. Building the legacy VS2015-era `engine.sln` will fail at the
`xrEngine` link step (unresolved `xrMP.lib`) — it is considered superseded.
If it must remain buildable, register xrMP there as well.

## Known documentation gap

`Documentation/Architecture/10_Extensibility.md` is empty. RFC-0007 covers
the extension principles, so Sprints 001–013 are not blocked, but the
document must be written before Sprint-014 (Plugin System) begins.

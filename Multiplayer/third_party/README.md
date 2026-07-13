# third_party

Vendored external dependencies for the STALKER-MP module.

## googletest/ — GoogleTest 1.11.0

| | |
|---|---|
| Upstream | https://github.com/google/googletest |
| Version | **1.11.0** (obtained via the Ubuntu `googletest` 1.11.0-3 source package; identical upstream sources — see docs/Decisions.md D-005) |
| Contents | `include/`, `src/`, `LICENSE` (BSD-3-Clause) |
| Consumers | `tests/xrMP_Tests.vcxproj` (compiles `src/gtest-all.cc`) |

Local modifications: **none**. The tree must stay pristine — never patch
vendored code; upgrade by replacing the whole directory with a newer pinned
release and updating the version here and in `docs/Decisions.md`.

Build integration: test targets add `googletest/include` and `googletest/`
to the include path and compile `src/gtest-all.cc` as third-party code
(without the project's `/W4 /WX`).

# STALKER-MP — Coding Standards (C++17 profile)

Derived from the project coding standard, adjusted per approved deviation
D-002 (`docs/Decisions.md`).

## Language

C++17, MSVC v143, `/W4` with warnings as errors, ConformanceMode on.
Code must also stay portable to GCC/Clang (`-Wall -Wextra -Werror`) — the
test suite is routinely built there.

## Naming

| Entity | Convention | Example |
|---|---|---|
| Types, functions, methods | PascalCase | `ServiceRegistry`, `LoadFile` |
| Interfaces | `I` + PascalCase | `IService` |
| Member variables | `m_` + camelCase | `m_minimumLevel` |
| Constants (incl. constexpr) | `k` + PascalCase | `kCompatibleEnginePrefix` |
| Namespaces | lowercase | `stalkermp::core` |
| Macros (exceptional) | `SMP_` + UPPER_SNAKE | `SMP_ASSERT` |
| Files | PascalCase, one class/theme per file | `ServiceRegistry.h/.cpp` |

## Required practice

- RAII everywhere; no raw owning pointers. `unique_ptr` for ownership,
  raw pointers/references only for non-owning observation.
- Fallible operations return `Expected<T>` / `Expected<void>`; never silently
  ignore errors. Assertions (`SMP_ASSERT`) are for programmer errors only,
  never for validating external input (02_Engine.md §19.6).
- `enum class` only; no plain enums.
- `const` correctness and `noexcept` on non-throwing functions, especially
  anything crossing the engine boundary.
- One responsibility per class; prefer composition; inheritance only for the
  interface hierarchy in `core/interfaces/`.
- No macros except the assertion framework (file/line capture) and test
  framework (registration) — the two documented exceptions.
- No global mutable state. The two documented exceptions, each created and
  destroyed at exactly one place: the module logger pointer (`Log.cpp`) and
  the Bootstrap runtime (`Bootstrap.cpp`).
- **ServiceRegistry is a composition-root utility, not a Service Locator.**
  The registry exists for ownership, dependency validation and deterministic
  init/shutdown ordering. Production code must receive its dependencies
  through constructor injection at the Bootstrap composition root.
  `ServiceRegistry::Find<T>()` / `FindByName()` may be called only from the
  Bootstrap composition root and diagnostics code; no global accessor to the
  registry may ever be introduced. Service lookup from arbitrary code is an
  architecture violation, not a convenience.
- Every new system starts with a header comment answering: why it exists,
  responsibilities, ownership, interactions, lifetime.

## Threading

Follow 09_Threading.md and RFC-0006. Locks are permitted only in approved
limited-lock domains (logging, queues, metrics). Simulation-facing code
(from Sprint-002 on) must be lock-free and single-threaded on the
Simulation Thread; cross-thread data moves via immutable snapshots only.

## Engine boundary

Engine code includes only `stalkermp/StalkerMP.h`. Boundary functions are
`noexcept` and must never let exceptions escape into engine code (the engine
compiles with exception handling disabled). Engine types do not appear in
module headers; module types do not leak engine internals.

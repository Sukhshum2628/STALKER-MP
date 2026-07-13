# STALKER-MP — Contributing

## Source of truth

`Documentation/` (Architecture, RFC, SPRINTS) is the specification.
If implementation and documentation conflict: stop, describe the conflict,
propose alternatives, and wait for approval. Approved deviations are
recorded in `docs/Decisions.md`.

## Workflow (mandatory, per project charter)

Before writing code for a task:

1. Read the current Sprint document.
2. Read every Architecture document and RFC it references.
3. Read the existing source the change interacts with.
4. Summarize understanding and present an implementation plan
   (design, new/modified classes, data flow, risks, order).
5. Implement only after the plan is accepted.

Sprint discipline: implement only the current sprint. No early
implementation of future sprints, no placeholders, no TODO/FIXME in
production code.

## Branch strategy

- `main` — always builds, all tests pass, matches completed sprints.
- `sprint/NNN-short-name` — integration branch per sprint.
- `feature/NNN-topic` — task branches, merged into the sprint branch via
  review.

Commits are small and single-purpose; unrelated changes never share a
commit. Reference the sprint/section in the message, e.g.
`Sprint-001 §7.9: service registry dependency validation`.

## Definition of done (every change)

- Builds warning-free in the engine solution (xrMP projects use /W4 /WX).
- `xrMP_Tests` passes (Debug and Release).
- New public behavior has tests.
- Header documentation (why/responsibilities/ownership) present.
- Documentation updated when behavior or structure changed.
